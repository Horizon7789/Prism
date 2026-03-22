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

SubjectMemory subject_memory = {0};   // Global definition

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

SubjectMemory mem = {0};

// Picks an auxiliary verb connected to current word or recent subjects
uint32_t pick_auxiliary_verb(uint32_t current_id, uint32_t *recent_subjects, int subj_count) {
    if (!current_id) return 0;

    // Check structural transitions from current word first
    StructuralEntry *entry = structural_matrix[hash_id(current_id)];
    while (entry && entry->source_id != current_id) entry = entry->next;
    if (!entry || !entry->transitions) return 0;

    uint32_t best_id = 0;
    uint32_t best_freq = 0;
    TransitionNode *curr = entry->transitions;

    while (curr) {
        uint32_t candidate_id = curr->target_id;
        uint16_t mask = trie_pool[candidate_id].pos_mask;

        // Only allow auxiliaries
        if (mask & POS_AUX) {
            if (curr->frequency > best_freq) {
                best_id = candidate_id;
                best_freq = curr->frequency;
            }
        }
        curr = curr->next;
    }

    // If no aux found in direct transitions, check causal links with subjects
    if (!best_id && recent_subjects && subj_count > 0) {
        for (int i = 0; i < subj_count; i++) {
            uint32_t subj_id = recent_subjects[i];
            uint32_t h_c = hash_pair(current_id, subj_id);
            CoEntry *ce = causal_table[h_c];
            while (ce) {
                if ((ce->word_a != current_id && ce->word_b != current_id) &&
                    (trie_pool[ce->word_a].pos_mask & POS_AUX)) {
                    best_id = ce->word_a;
                    break;
                }
                if ((ce->word_a != current_id && ce->word_b != current_id) &&
                    (trie_pool[ce->word_b].pos_mask & POS_AUX)) {
                    best_id = ce->word_b;
                    break;
                }
                ce = ce->next;
            }
            if (best_id) break;
        }
    }

    return best_id;
}

void generate_multi_sentence(const char *seed, int target_sentences) {
    if (!seed || strlen(seed) == 0) return;

    uint32_t seed_id = search_word(seed);
    if (seed_id == 0) seed_id = insert_word(seed);

    uint32_t current_id = seed_id;
    int sentences = 0;
    int word_count = 0;

    uint32_t recent[REPEAT_WINDOW] = {0};
    int recent_idx = 0;

    uint32_t recent_subjects[SUBJECT_MEMORY] = {seed_id};
    int subj_idx = 1;

    uint32_t context_window[CONTEXT_WINDOW] = {0};
    int ctx_size = 0;

    printf(BOLD "\nPRISM> " RESET);

    char word_str[64];
    build_word_from_id(seed_id, word_str, sizeof(word_str));
    word_str[0] = toupper((unsigned char)word_str[0]);
    printf("%s", word_str);

    recent[recent_idx % REPEAT_WINDOW] = seed_id;
    recent_idx++;
    context_window[ctx_size % CONTEXT_WINDOW] = seed_id;
    ctx_size++;
    word_count++;

    while (sentences < target_sentences && word_count < MAX_WORDS_PER_SENTENCE) {

        uint32_t next_id = predict_next_id(
            current_id,
            recent_subjects,
            subj_idx,
            context_window,
            ctx_size
        );

        if (next_id == 0) {
            next_id = pick_auxiliary_verb(current_id, recent_subjects, subj_idx);
        }
        if (next_id == 0) {
            next_id = get_most_frequent_successor(current_id);
        }
        if (next_id == 0) break;

        // Avoid repeats
        int is_repeat = 0;
        for (int r = 0; r < REPEAT_WINDOW; r++) {
            if (recent[r] == next_id) {
                is_repeat = 1;
                break;
            }
        }
        if (is_repeat) {
            current_id = recent[(recent_idx - 1 + REPEAT_WINDOW) % REPEAT_WINDOW];
            continue;
        }

        // Update subjects (simple version - no subject_memory struct)
        if (trie_pool[next_id].pos_mask & POS_NOUN) {
            recent_subjects[subj_idx % SUBJECT_MEMORY] = next_id;
            subj_idx++;
        }

        // Print
        build_word_from_id(next_id, word_str, sizeof(word_str));
        if (!is_id_punctuation(next_id)) printf(" ");
        printf("%s", word_str);

        // Update windows
        recent[recent_idx % REPEAT_WINDOW] = next_id;
        recent_idx++;
        context_window[ctx_size % CONTEXT_WINDOW] = next_id;
        ctx_size++;

        current_id = next_id;
        word_count++;

        if (is_id_sentence_ender(next_id)) {
            printf(".\n");
            sentences++;
            word_count = 0;
            current_id = seed_id;
            memset(recent, 0, sizeof(recent));
            recent_idx = 0;
            memset(context_window, 0, sizeof(context_window));
            ctx_size = 0;
            subj_idx = 1;
            recent_subjects[0] = seed_id;
        }
    }

    printf("\n");
}

#include <time.h>

uint32_t pick_random_anchor(void) {
    static int seeded = 0;
    if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }

    // Count candidates
    int count = 0;
    for (uint32_t i = 1; i < nodes_count; i++) {
        if (trie_pool[i].is_word && has_outgoing_edges(i)) count++;
    }

    if (count == 0) return 0;

    int target = rand() % count;
    for (uint32_t i = 1; i < nodes_count; i++) {
        if (trie_pool[i].is_word && has_outgoing_edges(i)) {
            if (target-- == 0) return i;
        }
    }

    return 0;
}

int has_outgoing_edges(uint32_t word_id) {
    if (word_id == 0 || word_id >= nodes_count) return 0;

    uint32_t slot = hash_id(word_id);
    StructuralEntry *entry = structural_matrix[slot];

    // Find the entry for this word
    while (entry) {
        if (entry->source_id == word_id) break;
        entry = entry->next;
    }

    if (!entry || !entry->transitions) return 0;

    // Check if there is at least one candidate that passes grammar
    TransitionNode *curr = entry->transitions;
    uint16_t prev_mask = trie_pool[word_id].pos_mask;

    while (curr) {
        uint32_t cand_id = curr->target_id;
        uint16_t cand_mask = trie_pool[cand_id].pos_mask;

        // Locked words always count as outgoing
        if (cand_mask & POS_LOCKED) return 1;

        // Check grammar matrix for any valid POS crossing
        int allowed = 0;
        for (int i = 0; i < 16 && !allowed; i++) {
            if (!(prev_mask & (1 << i))) continue;
            for (int j = 0; j < 16 && !allowed; j++) {
                if (!(cand_mask & (1 << j))) continue;
                if (grammar_matrix[i][j] > 0) allowed = 1;
            }
        }

        if (allowed) return 1;
        curr = curr->next;
    }

    return 0; // No valid outgoing edges
}

void auto_train_seed(uint32_t seed_id, const char *seed) {
    // Simple template: "<seed> is ..."
    char buf[128];
    snprintf(buf, sizeof(buf), "%s is interesting.", seed);

    // Split into words
    char words[16][64];
    int count = 0;
    char *p = strtok(buf, " ");
    while (p && count < 16) {
        strncpy(words[count++], p, 63);
        words[count-1][63] = '\0';
        p = strtok(NULL, " ");
    }

    // Insert each word into Trie & connect as transitions
    uint32_t prev_id = seed_id;
    for (int i = 1; i < count; i++) {
        uint32_t id = search_word(words[i]);
        if (!id) id = insert_word(words[i]);
        if (id) record_transition(prev_id, id); // add edge in structural plane
        prev_id = id;
    }
}




#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <json-c/json.h>

// --- Buffer to capture HTTP response ---
typedef struct {
    char *data;
    size_t size;
} HttpBuffer;

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    HttpBuffer *buf = (HttpBuffer*)userdata;

    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;

    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

// --- Map online POS string to PRISM mask ---
static uint16_t map_online_pos(const char *pos_str) {
    if (!pos_str) return 0;

    if (strcasecmp(pos_str, "noun") == 0) return POS_NOUN;
    if (strcasecmp(pos_str, "verb") == 0) return POS_VERB;
    if (strcasecmp(pos_str, "adjective") == 0) return POS_ADJ;
    if (strcasecmp(pos_str, "adverb") == 0) return POS_ADV;
    if (strcasecmp(pos_str, "pronoun") == 0) return POS_PRON;
    if (strcasecmp(pos_str, "determiner") == 0) return POS_DET;
    if (strcasecmp(pos_str, "preposition") == 0) return POS_PREP;
    if (strcasecmp(pos_str, "auxiliary") == 0) return POS_AUX;

    return POS_UNKNOWN;
}

// --- Fetch POS online for a single word ---
uint16_t fetch_online_pos(const char *word) {
    if (!word || !word[0]) return POS_UNKNOWN;

    CURL *curl = curl_easy_init();
    if (!curl) return POS_UNKNOWN;

    char url[256];
    snprintf(url, sizeof(url), "https://api.dictionaryapi.dev/api/v2/entries/en/%s", word);

    HttpBuffer buf = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !buf.data) {
        free(buf.data);
        return POS_UNKNOWN;
    }

    uint16_t mask = POS_UNKNOWN;

    // Parse JSON to extract partOfSpeech
    struct json_object *root, *entry, *meanings, *meaning, *pos_str;
    root = json_tokener_parse(buf.data);
    free(buf.data);

    if (!root) return POS_UNKNOWN;
    if (json_object_get_type(root) == json_type_array) {
        entry = json_object_array_get_idx(root, 0);
        if (entry && json_object_object_get_ex(entry, "meanings", &meanings)) {
            if (json_object_get_type(meanings) == json_type_array) {
                meaning = json_object_array_get_idx(meanings, 0);
                if (meaning && json_object_object_get_ex(meaning, "partOfSpeech", &pos_str)) {
                    const char *pos_text = json_object_get_string(pos_str);
                    mask = map_online_pos(pos_text);
                }
            }
        }
    }
    json_object_put(root);

    return mask;
}

// Thread function to assign POS
void* pos_worker(void* arg) {
    PosJob* job = (PosJob*)arg;
    if (!job) return NULL;

    uint32_t id = job->node_id;
    char* word  = job->word;

    // 1. Check existing POS
    if (trie_pool[id].pos_mask & POS_LOCKED) {
        free(job);
        return NULL;
    }

    // 2. Hybrid approach
    // a) Check local heuristics / known suffixes
    size_t len = strlen(word);
    if (len > 2 && strcmp(&word[len-2], "ly") == 0) {
        trie_pool[id].pos_mask |= POS_ADV;
    } else {
        // b) Online lookup (simulate here)
        // Replace with actual HTTP request or DB query
        uint16_t online_pos = fetch_online_pos(word); // returns POS_* mask
        trie_pool[id].pos_mask |= online_pos;
    }

    // c) Lock the POS so it won’t be overwritten
    trie_pool[id].pos_mask |= POS_LOCKED;

    free(job);
    return NULL;
}


void assign_hybrid_pos(uint32_t node_id) {
    char word[128];
    build_word_from_id(node_id, word, sizeof(word));

    uint16_t pos = POS_UNKNOWN;

    // Local heuristics
    size_t len = strlen(word);
    if (len > 2 && strcmp(&word[len-2], "ly") == 0) pos |= POS_ADV;
    else if (len > 3 && strcmp(&word[len-3], "ing") == 0) pos |= POS_VERB;
    else if (len > 1 && isupper((unsigned char)word[0])) pos |= POS_NOUN;
    else pos |= POS_NOUN;

    // Online lookup if still uncertain
    if (pos == POS_UNKNOWN || pos == POS_NOUN) {
        uint16_t online = fetch_online_pos(word);
        if (online != POS_UNKNOWN) pos = online;
    }

    trie_pool[node_id].pos_mask |= pos;
}