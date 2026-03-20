#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "prism.h"

/*
 * predict_next_id — Deterministic Symbolic Selection
 *
 * Replaces the probabilistic weighted float scoring with a
 * three-stage deterministic pipeline:
 *
 *   Stage 1 — GRAMMAR GATE (hard filter)
 *             Discard any candidate whose POS transition is
 *             forbidden by the grammar_matrix.
 *             A zero in grammar_matrix[prev_pos][cand_pos]
 *             means "this transition is structurally illegal."
 *
 *   Stage 2 — FREQUENCY SELECT (deterministic pick)
 *             Among the surviving legal candidates, pick the
 *             one with the highest raw transition count.
 *             No probabilities. No floats. Highest count wins.
 *
 *   Stage 3 — CO-OCCURRENCE TIE-BREAK
 *             If two candidates share the same frequency,
 *             prefer the one with the strongest co-occurrence
 *             link to the subject anchor (context relevance).
 *             Still integer comparison — no floats.
 *
 * Drop-in replacement for the old weighted version.
 * Same signature, same return type.
 */

uint32_t predict_next_id(uint32_t current_id, uint32_t subject_id) {
    uint32_t slot  = hash_id(current_id);
    StructuralEntry *entry = structural_matrix[slot];

    /* Find the entry for this word */
    while (entry != NULL) {
        if (entry->source_id == current_id) break;
        entry = entry->next;
    }

    if (!entry || !entry->transitions) return 0;

    uint16_t prev_mask = trie_pool[current_id].pos_mask;

    uint32_t best_id    = 0;
    uint32_t best_freq  = 0;
    uint32_t best_cooc  = 0;

    TransitionNode *curr = entry->transitions;

    while (curr) {
        uint32_t candidate_id = curr->target_id;

        /* ── Stage 1: Grammar Gate ──────────────────────────────
         * Check every active POS bit of the previous word against
         * every active POS bit of the candidate.
         * If the grammar_matrix allows at least one valid crossing,
         * the candidate passes. If all crossings are zero, reject.
         * Locked words (POS_LOCKED) always pass — they are grammar
         * anchors and should never be filtered out.             */

        uint16_t cand_mask = trie_pool[candidate_id].pos_mask;

        if (!(cand_mask & POS_LOCKED)) {
            int allowed = 0;
            for (int i = 0; i < 16 && !allowed; i++) {
                if (!(prev_mask & (1 << i))) continue;
                for (int j = 0; j < 16 && !allowed; j++) {
                    if (!(cand_mask & (1 << j))) continue;
                    if (grammar_matrix[i][j] > 0) allowed = 1;
                }
            }
            if (!allowed) {
                curr = curr->next;
                continue; /* Grammar gate: rejected */
            }
        }

        /* ── Stage 2: Frequency Select ──────────────────────────
         * Highest raw transition count wins.
         * Ties broken by Stage 3.                               */

        if (curr->frequency > best_freq) {
            best_id   = candidate_id;
            best_freq = curr->frequency;

            /* Reset co-occurrence baseline for new leader */
            best_cooc = 0;
            if (subject_id != 0) {
                uint32_t h = hash_pair(candidate_id, subject_id);
                CoEntry *ce = co_occurrence_table[h];
                while (ce) {
                    if ((ce->word_a == candidate_id && ce->word_b == subject_id) ||
                        (ce->word_a == subject_id  && ce->word_b == candidate_id)) {
                        best_cooc = ce->count;
                        break;
                    }
                    ce = ce->next;
                }
            }

        } else if (curr->frequency == best_freq && subject_id != 0) {

            /* ── Stage 3: Co-occurrence Tie-Break ───────────────
             * Both candidates appeared equally often after the
             * current word. Prefer the one more strongly linked
             * to the subject anchor in the co-occurrence table.
             * All integer arithmetic — no floats.              */

            uint32_t cand_cooc = 0;
            uint32_t h = hash_pair(candidate_id, subject_id);
            CoEntry *ce = co_occurrence_table[h];
            while (ce) {
                if ((ce->word_a == candidate_id && ce->word_b == subject_id) ||
                    (ce->word_a == subject_id  && ce->word_b == candidate_id)) {
                    cand_cooc = ce->count;
                    break;
                }
                ce = ce->next;
            }

            if (cand_cooc > best_cooc) {
                best_id   = candidate_id;
                best_cooc = cand_cooc;
                /* best_freq stays the same — it was a tie */
            }
        }

        curr = curr->next;
    }

    return best_id;
}
