#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>
    
#include "prism.h"

// The ACTUAL allocation of the matrix
StructuralEntry *structural_matrix[HASH_SIZE] = {0};

int is_id_punctuation(uint32_t id) {
    if (id == 0 || id >= nodes_count) return 0;
    
    // Check the letter_idx we assigned in our mapping
    uint8_t l_idx = trie_pool[id].letter_idx;
    
    // . (36), , (37), ! (38), ? (39)
    return (l_idx >= 36 && l_idx <= 39);
}

int is_id_sentence_ender(uint32_t id) {
    if (id == 0 || id >= nodes_count) return 0;
    
    uint8_t l_idx = trie_pool[id].letter_idx;
    
    // . (36), ! (38), ? (39)
    return (l_idx == 36 || l_idx == 38 || l_idx == 39);
}

// Simple hash for integer IDs
uint32_t hash_id(uint32_t id) {
    return id & (HASH_SIZE - 1);
}

void record_transition(uint32_t src_id, uint32_t dest_id) {
    if (src_id == 0 || dest_id == 0) return;

    uint32_t slot = src_id % STRUCTURAL_BUCKETS;
    StructuralEntry *entry = structural_matrix[slot];

    // 1. Find the entry for this source word
    while (entry != NULL) {
        if (entry->source_id == src_id) break;
        entry = entry->next;
    }

    // 2. If it doesn't exist, create it
    if (!entry) {
        entry = malloc(sizeof(StructuralEntry));
        entry->source_id = src_id;
        entry->total_occurrences = 0;
        entry->transitions = NULL;
        // Prepend to the collision chain
        entry->next = structural_matrix[slot];
        structural_matrix[slot] = entry;
    }

    entry->total_occurrences++;

    // 3. Update the Transition (Target Word)
    TransitionNode *t_node = entry->transitions;
    while (t_node != NULL) {
        if (t_node->target_id == dest_id) {
            t_node->frequency++;
            return;
        }
        t_node = t_node->next;
    }

    // 4. New transition pair discovered
    TransitionNode *new_t = malloc(sizeof(TransitionNode));
    new_t->target_id = dest_id;
    new_t->frequency = 1;
    new_t->next = entry->transitions;
    entry->transitions = new_t;
}


uint32_t* load_all_history(size_t *total_tokens) {
    FILE *f = fopen("packed_edges.bin", "rb");
    if (!f) {
        *total_tokens = 0;
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    rewind(f);

    *total_tokens = file_size / sizeof(uint32_t);

    if (*total_tokens == 0) {
        fclose(f);
        return NULL;
    }

    //printf("tokens: %zu\n", *total_tokens);

    uint32_t *buffer = malloc(file_size);
    if (!buffer) {
        fclose(f);
        printf("Memory allocation failed!\n");
        return NULL;
    }

    //printf("buffer address: %p\n", (void *)buffer);

    size_t read_count = fread(buffer, sizeof(uint32_t), *total_tokens, f);

    //printf("Requested: %zu | Read: %zu\n", *total_tokens, read_count);

    if (read_count != *total_tokens) {
        if (feof(f)) printf("EOF reached early\n");
        if (ferror(f)) printf("Read error\n");
    }

    for (size_t i = 0; i < read_count && i < 10; i++) {
        //printf("[%zu] = %u\n", i, buffer[i]);
    }

    fclose(f);
    return buffer;
}

void analyze_structure(void) {
    // 1. Safety First: Wipe old patterns to prevent leaks
    clear_structural_matrix();

    size_t total_tokens = 0;
    uint32_t *history = load_all_history(&total_tokens); 
    
    if (!history) {
        printf(YELLOW "PRISM: No history found. Structural Plane is empty." RESET "\n");
        return;
    }
    
    if (total_tokens < 2) {
        free(history);
        return;
    }

    printf(BLUE "PRISM: Weaving structural patterns from %zu tokens..." RESET "\n", total_tokens);

    // 2. The Weaving Loop
    for (size_t i = 0; i < total_tokens - 1; i++) {
        uint32_t src_id = history[i];
        uint32_t dest_id = history[i + 1];

        // CRITICAL: Feed the IDs into the structural matrix
        record_transition(src_id, dest_id);

        // Optional: Progress bar for large history files
        if (i % 5000 == 0 && i > 0) {
            printf(CYAN "  ...weaving token %zu/%zu\n" RESET, i, total_tokens);
        }
    }
        
    printf(GREEN "PRISM: Structural Plane successfully hydrated." RESET "\n");
    
    // 3. Clean up the temporary buffer used for loading
    free(history); 
}


void train_structural_plane(uint32_t *ids, size_t count) {
    if (count < 2) return;
    
    for (size_t i = 0; i < count - 1; i++) {
        record_transition(ids[i], ids[i+1]);
    }
    
    printf(GREEN "PRISM trained Successfully" RESET);
}


uint32_t predict_next_id(uint32_t current_id, uint32_t subject_id) {
    uint32_t slot = hash_id(current_id);
    StructuralEntry *entry = structural_matrix[slot];

    if (!entry || entry->source_id != current_id || !entry->transitions) return 0;

    uint32_t best_id = 0;
    float max_score = -1.0f;

    uint16_t prev_mask = trie_pool[current_id].pos_mask;
    TransitionNode *curr = entry->transitions;

    while (curr) {
        uint32_t candidate_id = curr->target_id;
        uint16_t curr_mask = trie_pool[candidate_id].pos_mask;

        // 1. Structural Score (Frequency normalized against total)
        float s_score = (float)curr->frequency / entry->total_occurrences;

        // 2. Grammar Score (How likely is this PoS transition?)
        float g_score = 0.0f;
        int active_tags = 0;
        for (int i = 0; i < 16; i++) {
            if (prev_mask & (1 << i)) {
                for (int j = 0; j < 16; j++) {
                    if (curr_mask & (1 << j)) {
                        // Look up the transition strength in your 16x16 matrix
                        g_score += grammar_matrix[i][j];
                        active_tags++;
                    }
                }
            }
        }
        if (active_tags > 0) g_score /= active_tags; // Simple normalization

        // 3. Contextual Relevance (The "Hallucination Killer")
        float c_score = get_contextual_relevance(candidate_id, subject_id);

        // Final Weighted Consensus
        float total_score = (s_score * WEIGHT_STRUCTURAL) + 
                            (g_score * WEIGHT_GRAMMAR) + 
                            (c_score * WEIGHT_CONTEXT);

        if (total_score > max_score) {
            max_score = total_score;
            best_id = candidate_id;
        }
        curr = curr->next;
    }

    return best_id;
}



void generate_multi_sentence(const char *seed, int target_sentences) {
    uint32_t seed_id = insert_word(seed); 
    if (seed_id == 0) {
        printf(RED "PRISM: Seed word not found." RESET "\n");
        return;
    }

    uint32_t current_id = seed_id;
    uint32_t subject_id = seed_id; // The context anchor
    int sentences_count = 0;
    int max_words = 150; 
    int word_count = 0;

    printf(BOLD "\nPRISM> " RESET "%s ", seed);

    while (sentences_count < target_sentences && word_count < max_words) {
        // Now passing both current word and the subject anchor
        uint32_t next_id = predict_next_id(current_id, subject_id);
        
        if (next_id == 0) {
            printf(" [End of Path]");
            break;
        }

        char *word = get_word_by_id(next_id);
        if (word) {
            // Check if the word is a stop token (., !, ?)
            uint8_t l_idx = trie_pool[next_id].letter_idx;
            
            if (l_idx >= 36 && l_idx <= 39) {
                printf("%s", word); // No space before punctuation
                sentences_count++;
            } else {
                printf(" %s", word); // Space before normal words
            }
            
            free(word);
        }

        current_id = next_id;
        word_count++;
    }
    printf("\n");
}


void replay_with_structure(uint32_t *history_ids, size_t count) {
    int capitalize_next = 1;

    for (size_t i = 0; i < count; i++) {
        char *word = get_word_by_id(history_ids[i]);
        if (!word) continue;

        // 1. Structural Check: Did we miss a comma?
        // If the frequency of (Current -> Comma) is > 70% of total occurrences
        // we can safely assume a structural break belongs here.
        
        // 2. Formatting based on ID type
        if (capitalize_next && isalpha(word[0])) {
            word[0] = toupper(word[0]);
            capitalize_next = 0;
        }

        printf("%s", word);

        // 3. Smart Spacing
        uint32_t next_id = (i + 1 < count) ? history_ids[i+1] : 0;
        if (!is_id_punctuation(next_id)) {
            printf(" ");
        }

        // 4. Sentence Logic
        if (is_id_sentence_ender(history_ids[i])) {
            printf("\n");
            capitalize_next = 1;
        }

        free(word);
    }
}

void inspect_word_context(const char *target_word) {
    uint32_t id = search_word(target_word); 
    if (id == 0) {
        printf(RED "Word '%s' not found in Lexical Matrix." RESET "\n", target_word);
        return;
    }

    // 1. Calculate Hash Slot
    uint32_t slot = id % STRUCTURAL_BUCKETS; 
    StructuralEntry *entry = structural_matrix[slot];

    // 2. Traverse the chain to find the specific Word ID
    while (entry != NULL) {
        if (entry->source_id == id) break;
        entry = entry->next;
    }

    if (!entry || !entry->transitions) {
        printf(YELLOW "PRISM: No structural patterns for '%s' (ID: %u) yet." RESET "\n", target_word, id);
        return;
    }

    printf(BOLD "\n--- Structural Context: [%s] ---" RESET "\n", target_word);
    printf("Total Occurrences: %u\n", entry->total_occurrences);
    printf("Likely Next Tokens:\n");

    TransitionNode *curr = entry->transitions;
while (curr) {
    // 1. Reconstruct the string from the Trie ID
    char *decoded = get_word_by_id(curr->target_id);
    
    // 2. Calculate probability based on the total occurrences for this source
    float probability = 0.0f;
    if (entry->total_occurrences > 0) {
        probability = ((float)curr->frequency / entry->total_occurrences) * 100;
    }

    // 3. Print the formatted output using the decoded word
    // We handle the case where get_word_by_id might return NULL
    const char *display_word = (decoded) ? decoded : "???";

    printf("  -> " CYAN "[ID:%-4u] %-14s" RESET " | Count: %-3u | Prob: " GREEN "%5.1f%%" RESET "\n", 
           curr->target_id, 
           display_word, 
           curr->frequency, 
           probability);

    // 4. CRITICAL: Free the string AFTER printing, then move to next node
    if (decoded) {
        free(decoded);
    }
    
    curr = curr->next;
}

    printf("-------------------------------\n");
}


void clear_structural_matrix(void) {
    for (int i = 0; i < HASH_SIZE; i++) {
        if (structural_matrix[i] != NULL) {
            TransitionNode *curr = structural_matrix[i]->transitions;
            while (curr != NULL) {
                TransitionNode *temp = curr;
                curr = curr->next;
                free(temp); // Free each link in the chain
            }
            free(structural_matrix[i]); // Free the entry itself
            structural_matrix[i] = NULL;
        }
    }
}
