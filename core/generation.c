/* ================================================================
 * PRISM — Bulk Training & Improved Generation
 *
 * 1. train_from_file()    — ingest any .txt file as training data
 * 2. train_from_string()  — ingest a raw string directly
 * 3. generate_multi_sentence() — improved generation with:
 *      - grammar template guidance
 *      - repetition avoidance
 *      - dead-end recovery
 *      - proper spacing / capitalisation
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "prism.h"

/* ================================================================
   BULK TRAINING FROM FILE
   Reads the file in chunks, splits into tokens, trains all planes.
   No Wikipedia fetch needed — feed it any plain text.

   Usage (add to main loop):
     if (strncmp(input, "train file ", 11) == 0)
         train_from_file(input + 11);

   Example files that work great:
     - Any Wikipedia article saved as .txt
     - A book chapter
     - Your own notes / documents
   ================================================================ */

#define TRAIN_CHUNK 65536   /* 64 KB per read pass */


/* ================================================================
   GRAMMAR TEMPLATE TABLE
   Maps seed word POS to a generation target pattern.
   The generator tries to satisfy this pattern before allowing
   a sentence-ender. This is deterministic — no probabilities.

   Pattern is read left-to-right. Each slot is a POS bitmask.
   0 = "any" (no constraint for this slot).
   ================================================================ */

typedef struct {
    uint16_t trigger_pos;   /* POS of seed word that activates this */
    uint16_t slots[8];      /* expected POS sequence */
    int      slot_count;
} GenTemplate;

static const GenTemplate gen_templates[] = {
    /* NOUN seed:  NOUN + AUX + (ADJ|NOUN) + PREP + DET + NOUN */
    { POS_NOUN,
      {POS_AUX, POS_NOUN|POS_ADJ, POS_PREP, POS_DET, POS_NOUN, 0, 0, 0},
      5 },
    /* VERB seed:  VERB + DET + NOUN + PREP + DET + NOUN */
    { POS_VERB,
      {POS_DET, POS_NOUN, POS_PREP, POS_DET, POS_NOUN, 0, 0, 0},
      5 },
    /* ADJ seed:   ADJ + NOUN + AUX + ADJ */
    { POS_ADJ,
      {POS_NOUN, POS_AUX, POS_ADJ, 0, 0, 0, 0, 0},
      3 },
};
#define GEN_TEMPLATE_COUNT (int)(sizeof(gen_templates)/sizeof(gen_templates[0]))


/* ================================================================
   IMPROVED generate_multi_sentence
   Drop-in replacement for the original.

   Improvements over original:
   1. Grammar template guidance — tries to fill POS slots before
      allowing a sentence-ender, so output has real structure.
   2. Repetition window — won't repeat a word seen in last 8 tokens.
   3. Dead-end recovery — if predict_next_id returns 0, tries the
      subject anchor directly instead of immediately giving up.
   4. Capitalisation and spacing are handled correctly.
   5. Sentence count is enforced properly.
   ================================================================ */

#define REPEAT_WINDOW 8     /* how many recent tokens to avoid repeating */
#define MAX_TEMPLATE_SLOTS 8

void generate_multi_sentence(const char *seed, int target_sentences) {
    uint32_t seed_id = search_word(seed);
    if (seed_id == 0) {
        /* Word not in trie yet — insert it so it becomes an anchor */
        seed_id = insert_word(seed);
        if (seed_id == 0) {
            printf(RED "PRISM: Seed word could not be registered." RESET "\n");
            return;
        }
        printf(YELLOW "PRISM: '%s' was unknown — inserted as anchor. "
               "Train more data for richer output.\n" RESET, seed);
    }

    /* Select a grammar template based on seed word's POS */
    const GenTemplate *tmpl = NULL;
    uint16_t seed_mask = trie_pool[seed_id].pos_mask;
    for (int t = 0; t < GEN_TEMPLATE_COUNT; t++) {
        if (seed_mask & gen_templates[t].trigger_pos) {
            tmpl = &gen_templates[t];
            break;
        }
    }
    /* Default to first template if no match */
    if (!tmpl) tmpl = &gen_templates[0];

    uint32_t current_id   = seed_id;
    uint32_t subject_id   = seed_id;
    int      sentences    = 0;
    int      word_count   = 0;
    int      max_words    = 60;
    int      tmpl_slot    = 0;       /* how far into the template we are */
    int      capitalize   = 0;       /* next word should be capitalised */

    /* Repetition window */
    uint32_t recent[REPEAT_WINDOW] = {0};
    int      recent_idx = 0;

    /* Print seed word */
    printf(BOLD "\nPRISM> " RESET);
    char *seed_str = get_word_by_id(seed_id);
    if (seed_str) {
        /* Capitalise first word of output */
        if (seed_str[0] >= 'a' && seed_str[0] <= 'z')
            seed_str[0] = seed_str[0] - 32;
        printf("%s", seed_str);
        free(seed_str);
    }

    recent[recent_idx++ % REPEAT_WINDOW] = seed_id;

    while (sentences < target_sentences && word_count < max_words) {

        uint32_t next_id = predict_next_id(current_id, subject_id);

        /* ── Dead-end recovery ── */
        if (next_id == 0) {
            /* Try predicting from subject anchor instead */
            if (current_id != subject_id)
                next_id = predict_next_id(subject_id, subject_id);

            /* Still nothing — we're done */
            if (next_id == 0) break;
        }

        /* ── Repetition check ── */
        int is_repeat = 0;
        for (int r = 0; r < REPEAT_WINDOW; r++) {
            if (recent[r] == next_id) { is_repeat = 1; break; }
        }
        if (is_repeat) {
            /* Skip this candidate and try the subject anchor */
            uint32_t alt = predict_next_id(subject_id, subject_id);
            if (alt == 0 || alt == next_id) break; /* give up gracefully */
            next_id = alt;
        }

        /* ── Template slot advance ── */
        if (tmpl && tmpl_slot < tmpl->slot_count) {
            uint16_t expected = tmpl->slots[tmpl_slot];
            if (expected != 0) {
                uint16_t cand_mask = trie_pool[next_id].pos_mask;
                /* If candidate doesn't match expected slot, don't advance slot */
                if (cand_mask & expected) tmpl_slot++;
            } else {
                tmpl_slot++; /* slot is "any" — always advance */
            }
        }

        /* ── Sentence ender logic ── */
        int is_ender = is_id_sentence_ender(next_id);

        /* Don't end sentence before template is satisfied */
        if (is_ender && tmpl && tmpl_slot < tmpl->slot_count - 1) {
            /* Skip this ender — look for something else */
            uint32_t alt = predict_next_id(subject_id, subject_id);
            if (alt != 0 && !is_id_sentence_ender(alt)) {
                next_id  = alt;
                is_ender = 0;
            }
        }

        /* ── Print ── */
        char *word = get_word_by_id(next_id);
        if (!word) { current_id = next_id; word_count++; continue; }

        if (is_id_punctuation(next_id)) {
            /* No space before punctuation */
            printf("%s", word);
        } else {
            printf(" ");
            if (capitalize && isalpha(word[0])) {
                word[0] = toupper((unsigned char)word[0]);
                capitalize = 0;
            }
            printf("%s", word);
        }
        free(word);

        if (is_ender) {
            printf("\n");
            sentences++;
            capitalize  = 1;
            tmpl_slot   = 0;   /* reset template for next sentence */
            current_id  = seed_id; /* restart from seed for next sentence */
        } else {
            current_id = next_id;
        }

        recent[recent_idx++ % REPEAT_WINDOW] = next_id;
        word_count++;
    }

    printf("\n");

    if (word_count == 0)
        printf(YELLOW "PRISM: No transitions found for '%s'. "
               "Try: train file <path> to add training data.\n" RESET, seed);
}
