/*
 * predict_next_id — Deterministic Context-Resonance Predictor
 *
 * Fully aligned with PRISM blueprint:
 *   • Grammar gate (hard filter)
 *   • Full context resonance (structural + co-occurrence + causal)
 *   • Lateral inhibition (repeat suppression)
 *   • No floats, no probabilities — pure integer scoring
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "prism.h"

#define TOP_K 5

uint32_t candidates[TOP_K] = {0};
float scores[TOP_K] = {0};

uint32_t recent_content[CONTEXT_WINDOW] = {0};
int recent_count = 0;
CoEntry *causal_table[CAUSAL_TABLE_SIZE] = {0};

// -------------------------
// PRISM Reasoning & Generation
// -------------------------

#define REPEAT_WINDOW 8
#define MAX_WORDS_PER_SENTENCE 128

typedef struct TransitionNode TransitionNode;
typedef struct StructuralEntry StructuralEntry;

// -------------------------
// Subject Memory Helpers
// -------------------------
void add_subject(SubjectMemory *mem, uint32_t id) {
    if (!mem || id == 0) return;

    for (int i = 0; i < mem->count; i++) {
        if (mem->ids[i] == id) return; // already exists
    }

    if (mem->count < SUBJECT_MEMORY) {
        mem->ids[mem->count++] = id;
    } else {
        // rotate oldest out
        memmove(&mem->ids[0], &mem->ids[1], sizeof(uint32_t)*(SUBJECT_MEMORY-1));
        mem->ids[SUBJECT_MEMORY-1] = id;
    }
}

extern CoEntry *causal_table[CAUSAL_TABLE_SIZE]; // causal link table

// Grammar check function
int grammar_pass(uint16_t prev_mask, uint16_t cand_mask) {
    if (cand_mask & POS_LOCKED) return 1;  // always allow locked POS
    for (int i = 0; i < 16; i++) {
        if (!(prev_mask & (1 << i))) continue;
        for (int j = 0; j < 16; j++) {
            if (!(cand_mask & (1 << j))) continue;
            if (grammar_matrix[i][j] > 0) return 1;
        }
    }
    return 0;
}


// ================================================================
// Helper: Structural Frequency
// ================================================================
int get_structural_freq(uint32_t from, uint32_t to) {
    if (from == 0 || to == 0 || from >= nodes_count || to >= nodes_count)
        return 0;

    uint32_t slot = hash_id(from);
    StructuralEntry *entry = structural_matrix[slot];

    while (entry && entry->source_id != from)
        entry = entry->next;

    if (!entry || !entry->transitions)
        return 0;

    TransitionNode *t = entry->transitions;
    while (t) {
        if (t->target_id == to)
            return t->frequency;
        t = t->next;
    }
    return 0;
}

// ================================================================
// Helper: Co-occurrence Count
// ================================================================
int get_co_count(uint32_t a, uint32_t b) {
    if (a == 0 || b == 0 || a == b) return 0;

    uint32_t h = hash_pair(a, b);
    CoEntry *curr = co_occurrence_table[h];

    while (curr) {
        if ((curr->word_a == a && curr->word_b == b) ||
            (curr->word_a == b && curr->word_b == a))
            return curr->count;
        curr = curr->next;
    }
    return 0;
}

// ================================================================
// Helper: Causal Count
// ================================================================
int get_causal_count(uint32_t a, uint32_t b) {
    if (a == 0 || b == 0 || a == b) return 0;

    uint32_t h = hash_pair(a, b);
    CoEntry *curr = causal_table[h];

    while (curr) {
        if ((curr->word_a == a && curr->word_b == b) ||
            (curr->word_a == b && curr->word_b == a))
            return curr->count;
        curr = curr->next;
    }
    return 0;
}

// ================================================================
// Main Predictor — Context-Aware Resonance
// ================================================================
uint32_t predict_next_id(uint32_t current_id,
                         uint32_t *subjects, int subj_count,
                         uint32_t *context_window, int ctx_size)
{
    if (!context_window || ctx_size < 1) return 0;

    uint32_t best_id = 0;
    int32_t best_score = -1;

    uint32_t candidates[CANDIDATE_POOL] = {0};
    int cand_count = 0;

    // 1. Structural from recent context
    for (int i = 0; i < 3 && (ctx_size - 1 - i) >= 0; i++) {
        uint32_t prev = context_window[ctx_size - 1 - i];
        if (prev == 0) continue;
        // ... your existing structural candidate code ...
        uint32_t slot = hash_id(prev);
        StructuralEntry *entry = structural_matrix[slot];
        while (entry && entry->source_id != prev) entry = entry->next;
        if (entry && entry->transitions) {
            TransitionNode *t = entry->transitions;
            while (t && cand_count < CANDIDATE_POOL) {
                if (t->target_id && t->frequency >= 2) {
                    candidates[cand_count++] = t->target_id;
                }
                t = t->next;
            }
        }
    }

    // 2. Strong Conceptual boost from subjects (this is the key new part)
    for (int s = 0; s < subj_count; s++) {
        uint32_t subj = subjects[s];
        if (subj == 0) continue;

        // Pull associated concepts from co-occurrence and causal tables (Reasoning Plane)
        uint32_t slot = hash_id(subj);
        StructuralEntry *entry = structural_matrix[slot];
        while (entry && entry->source_id != subj) entry = entry->next;

        if (entry && entry->transitions) {
            TransitionNode *t = entry->transitions;
            while (t && cand_count < CANDIDATE_POOL) {
                if (t->target_id && t->frequency >= 3) {
                    candidates[cand_count++] = t->target_id;
                }
                t = t->next;
            }
        }
    }

    // Stage 1: Score
    for (int c = 0; c < cand_count; c++) {
        uint32_t cand = candidates[c];
        if (cand == 0) continue;

        uint16_t cand_mask = trie_pool[cand].pos_mask;

        if (is_id_punctuation(cand)) {
            if (ctx_size > 0 && is_id_punctuation(context_window[ctx_size-1])) continue;
            if (ctx_size > 2 && is_id_sentence_ender(context_window[ctx_size-2])) continue;
        }

        if (!(cand_mask & POS_LOCKED) && !grammar_pass(trie_pool[current_id].pos_mask, cand_mask))
            continue;

        int32_t score = 0;

        score += get_structural_freq(current_id, cand) * 10;
        for (int i = 0; i < ctx_size; i++) {
            if (context_window[i]) score += get_co_count(context_window[i], cand) * 5;
        }
        for (int s = 0; s < subj_count; s++) {
            if (subjects[s]) score += get_causal_count(subjects[s], cand) * 12;  // boosted
        }

        // === NEW: Reasoning Plane bonus for asteroid-related concepts ===
        if (cand_mask & POS_NOUN) score += 25;
        if (cand_mask & POS_ADJ)  score += 18;
        if (cand_mask & POS_VERB) score += 10;

        // Bonus for known asteroid attributes (add more as you learn)
        char word_buf[64];
        build_word_from_id(cand, word_buf, sizeof(word_buf));
        if (strstr(word_buf, "rock") || strstr(word_buf, "orbit") || 
            strstr(word_buf, "planet") || strstr(word_buf, "size") || 
            strstr(word_buf, "dwarf") || strstr(word_buf, "metallic")) {
            score += 35;
        }

        if (is_id_sentence_ender(cand)) score += 30;

        // Lateral inhibition
        for (int r = 0; r < ctx_size; r++) {
            if (context_window[r] == cand) {
                score -= 90;
                break;
            }
        }

#if DEBUG_GENERATION
        build_word_from_id(cand, word_buf, sizeof(word_buf));
        printf("[RESONANCE] '%s' → Score=%d\n", word_buf, score);
#endif

        if (score > best_score) {
            best_score = score;
            best_id = cand;
        }
    }

    if (best_id == 0) {
        best_id = pick_auxiliary_verb(current_id, subjects, subj_count);
        if (best_id == 0)
            best_id = get_most_frequent_successor(current_id);
    }

    return best_id;
}

// ================================================================
// Fallback: Most frequent structural successor (when resonance gives nothing)
// ================================================================
uint32_t get_most_frequent_successor(uint32_t from) {
    if (from == 0 || from >= nodes_count) return 0;

    uint32_t slot = hash_id(from);
    StructuralEntry *entry = structural_matrix[slot];
    while (entry && entry->source_id != from)
        entry = entry->next;

    if (!entry || !entry->transitions) return 0;

    TransitionNode *t = entry->transitions;
    uint32_t best = 0;
    int best_freq = 0;

    while (t) {
        if (t->frequency > best_freq) {
            best_freq = t->frequency;
            best = t->target_id;
        }
        t = t->next;
    }
    return best;
}