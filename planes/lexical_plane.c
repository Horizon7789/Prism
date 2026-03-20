#include <stdlib.h>
#include <string.h>
    
#include "prism.h"

POSMap pos_map[] = {
    {POS_NOUN,  "Noun"},
    {POS_VERB,  "Verb"},
    {POS_ADJ,   "Adjective"},
    {POS_ADV,   "Adverb"},

    {POS_DET,   "Determiner"},
    {POS_PRON,  "Pronoun"},
    {POS_PREP,  "Preposition"},
    {POS_CONJ,  "Conjunction"},

    {POS_AUX,   "Auxiliary Verb"},
    {POS_MODAL, "Modal Verb"},

    {POS_PART,  "Particle"},
    {POS_INTJ,  "Interjection"},

    {POS_NUM,   "Number"},
    {POS_STOP,  "Punctuation"},

    {POS_WH,    "WH-word"}
};

const int POS_MAP_COUNT = sizeof(pos_map) / sizeof(POSMap);

StructuralEntry* get_structural_entry(uint32_t source_id) {
    if (source_id >= nodes_count) return NULL;

    uint32_t slot = source_id % STRUCTURAL_BUCKETS;
    StructuralEntry *entry = structural_matrix[slot];

    // Traverse the hash bucket to find the specific ID
    while (entry != NULL) {
        if (entry->source_id == source_id) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL; // Not found
}


FiniteEntry finite_list[] = {
    
{"the", POS_DET}, {"a", POS_DET}, {"an", POS_DET},
{"this", POS_DET}, {"that", POS_DET},
{"these", POS_DET}, {"those", POS_DET},
{"my", POS_DET}, {"your", POS_DET}, {"his", POS_DET},
{"her", POS_DET}, {"its", POS_DET}, {"our", POS_DET}, {"their", POS_DET},
{"some", POS_DET}, {"any", POS_DET}, {"each", POS_DET},
{"every", POS_DET}, {"no", POS_DET}, {"many", POS_DET}, {"few", POS_DET},


{"i", POS_PRON}, {"you", POS_PRON}, {"he", POS_PRON},
{"she", POS_PRON}, {"it", POS_PRON}, {"we", POS_PRON}, {"they", POS_PRON},

{"me", POS_PRON}, {"him", POS_PRON}, {"her", POS_PRON},
{"us", POS_PRON}, {"them", POS_PRON},

{"mine", POS_PRON}, {"yours", POS_PRON}, {"his", POS_PRON},
{"hers", POS_PRON}, {"ours", POS_PRON}, {"theirs", POS_PRON},

{"myself", POS_PRON}, {"yourself", POS_PRON}, {"himself", POS_PRON},
{"herself", POS_PRON}, {"itself", POS_PRON}, {"ourselves", POS_PRON}, {"themselves", POS_PRON},


{"am", POS_AUX}, {"is", POS_AUX}, {"are", POS_AUX},
{"was", POS_AUX}, {"were", POS_AUX}, {"be", POS_AUX},
{"being", POS_AUX}, {"been", POS_AUX},

{"have", POS_AUX}, {"has", POS_AUX}, {"had", POS_AUX},

{"do", POS_AUX}, {"does", POS_AUX}, {"did", POS_AUX},

{"can", POS_MODAL}, {"could", POS_MODAL},
{"shall", POS_MODAL}, {"should", POS_MODAL},
{"will", POS_MODAL}, {"would", POS_MODAL},
{"may", POS_MODAL}, {"might", POS_MODAL},
{"must", POS_MODAL},


{"in", POS_PREP}, {"on", POS_PREP}, {"at", POS_PREP},
{"by", POS_PREP}, {"for", POS_PREP}, {"with", POS_PREP},
{"about", POS_PREP}, {"against", POS_PREP},
{"between", POS_PREP}, {"into", POS_PREP},
{"through", POS_PREP}, {"during", POS_PREP},
{"before", POS_PREP}, {"after", POS_PREP},
{"above", POS_PREP}, {"below", POS_PREP},
{"to", POS_PREP | POS_PART},   // dual role
{"from", POS_PREP}, {"up", POS_PREP}, {"down", POS_PREP},
{"over", POS_PREP}, {"under", POS_PREP},{ "of",   POS_PREP | POS_LOCKED },
{ "for",  POS_PREP | POS_LOCKED },
{ "with", POS_PREP | POS_LOCKED },
{ "on",   POS_PREP | POS_LOCKED },


{"and", POS_CONJ}, {"or", POS_CONJ}, {"but", POS_CONJ},
{"because", POS_CONJ}, {"so", POS_CONJ}, {"although", POS_CONJ},
{"if", POS_CONJ}, {"while", POS_CONJ}, {"since", POS_CONJ},
{"unless", POS_CONJ}, {"though", POS_CONJ},


{"not", POS_PART},
{"to", POS_PART},   // infinitive marker
{"up", POS_PART}, {"off", POS_PART}, {"out", POS_PART},


{"who", POS_WH}, {"whom", POS_WH}, {"whose", POS_WH},
{"what", POS_WH}, {"which", POS_WH},
{"when", POS_WH}, {"where", POS_WH},
{"why", POS_WH}, {"how", POS_WH},


{"oh", POS_INTJ}, {"wow", POS_INTJ}, {"hey", POS_INTJ},
{"ah", POS_INTJ}, {"oops", POS_INTJ},


{".", POS_STOP}, {",", POS_STOP},
{"!", POS_STOP}, {"?", POS_STOP},
{";", POS_STOP}, {":", POS_STOP},

{"zero", POS_NUM}, {"one", POS_NUM}, {"two", POS_NUM}, {"three", POS_NUM},
{"four", POS_NUM}, {"five", POS_NUM}, {"six", POS_NUM}, {"seven", POS_NUM},
{"eight", POS_NUM}, {"nine", POS_NUM}, {"ten", POS_NUM},
{"eleven", POS_NUM}, {"twelve", POS_NUM}, {"thirteen", POS_NUM},
{"fourteen", POS_NUM}, {"fifteen", POS_NUM}, {"sixteen", POS_NUM},
{"seventeen", POS_NUM}, {"eighteen", POS_NUM}, {"nineteen", POS_NUM},
{"twenty", POS_NUM}, {"thirty", POS_NUM}, {"forty", POS_NUM},
{"fifty", POS_NUM}, {"sixty", POS_NUM}, {"seventy", POS_NUM},
{"eighty", POS_NUM}, {"ninety", POS_NUM},

{"hundred", POS_NUM}, {"thousand", POS_NUM}, {"million", POS_NUM},
{"billion", POS_NUM}, {"trillion", POS_NUM},

{"first", POS_NUM}, {"second", POS_NUM}, {"third", POS_NUM},
{"fourth", POS_NUM}, {"fifth", POS_NUM}, {"sixth", POS_NUM},
{"seventh", POS_NUM}, {"eighth", POS_NUM}, {"ninth", POS_NUM},
{"tenth", POS_NUM}, {"twentieth", POS_NUM}, {"hundredth", POS_NUM},

{"all", POS_NUM}, {"none", POS_NUM}, {"some", POS_NUM},
{"many", POS_NUM}, {"few", POS_NUM}, {"several", POS_NUM},
{"most", POS_NUM}, {"more", POS_NUM}, {"less", POS_NUM},

{"half", POS_NUM}, {"quarter", POS_NUM},

{"0", POS_NUM}, {"1", POS_NUM}, {"2", POS_NUM}, {"3", POS_NUM},
{"4", POS_NUM}, {"5", POS_NUM}, {"6", POS_NUM}, {"7", POS_NUM},
{"8", POS_NUM}, {"9", POS_NUM}

};




void seed_golden_lexicon(void) {
    size_t finite_count = sizeof(finite_list) / sizeof(FiniteEntry);
    
    for (size_t i = 0; i < finite_count; i++) {
        // 1. Insert the word (or find existing ID)
        uint32_t id = insert_word(finite_list[i].word);
        
        if (id < nodes_count) {
            // 2. Apply the Mask AND the Locked bit
            // The POS_LOCKED bit (0x8000) tells train_prism_planes: "DO NOT CHANGE THIS"
            trie_pool[id].pos_mask = finite_list[i].mask | POS_LOCKED;
            
            // 3. Mark as high frequency/semantic weight so it doesn't get pruned
            trie_pool[id].total_frequency = 255; 
        }
    }
    
    printf(GREEN "PRISM: Golden Lexicon seeded with %zu locked tokens." RESET "\n", finite_count);
}


// Define the hash table and pools
CoEntry *co_occurrence_table[CO_HASH_SIZE] = {0};
CoEntry entry_pool[MAX_CO_ENTRIES] = {0};
uint32_t entries_used = 0;



const int FINITE_COUNT = sizeof(finite_list) / sizeof(FiniteEntry);

GrammarTemplate logic_templates[] = {
    {{POS_DET, POS_NOUN, POS_AUX, POS_DET}, 0.9}, // "The asteroid is a..."
    {{POS_NOUN, POS_AUX, POS_ADJ, POS_STOP}, 0.8}, // "Space is boundless."
    {{POS_PRON, POS_VERB, POS_PREP, POS_NOUN}, 0.7} // "It orbits around stars."
};


// Symmetric Hashing: (a,b) should hash same as (b,a) for relationship logic
uint32_t hash_pair(uint32_t a, uint32_t b) {
    uint32_t min = (a < b) ? a : b;
    uint32_t max = (a < b) ? b : a;
    return (min * 31337 ^ max) % CO_HASH_SIZE;
}

void increment_co_link(uint32_t a, uint32_t b) {
    if (a == b) return; // Ignore self-links

    uint32_t h = hash_pair(a, b);
    CoEntry *curr = co_occurrence_table[h];

    // Search for existing link
    while (curr) {
        if ((curr->word_a == a && curr->word_b == b) || 
            (curr->word_a == b && curr->word_b == a)) {
            curr->count++;
            return;
        }
        curr = curr->next;
    }

    // Create new link if arena has space
    if (entries_used < MAX_CO_ENTRIES) {
        CoEntry *new_node = &entry_pool[entries_used++];
        new_node->word_a = a;
        new_node->word_b = b;
        new_node->count = 1;
        
        // Push to front of bucket (classic hash-map insert)
        new_node->next = co_occurrence_table[h];
        co_occurrence_table[h] = new_node;
    }
}


void hydrate_prism_grammar() {
    for (int i = 0; i < FINITE_COUNT; i++) {
        // Find existing word or insert new one
        uint32_t id = insert_word(finite_list[i].word);
        
        //printf("word: %s \n", finite_list[i].word);
       
        // Safety: Ensure we don't overwrite, we APPEND bits
        // All words are Nouns by default, we add the specific finite tags
        trie_pool[id].pos_mask |= finite_list[i].mask;
        
       //printf("mask: 0x%X\n", trie_pool[id].pos_mask);
    }
}



void infer_pos_with_context(uint32_t *ids, size_t idx, size_t start, size_t end) {
    uint32_t curr_id = ids[idx];
    uint16_t *curr_mask = &trie_pool[curr_id].pos_mask;

    if (*curr_mask & POS_LOCKED) return;

    // --- SUFFIX HEURISTIC (Adverb Detection) ---
    char *word = get_word_by_id(curr_id);
    if (word) {
        size_t len = strlen(word);
        if (len > 2 && strcmp(&word[len-2], "ly") == 0) {
            *curr_mask |= POS_ADV;
            *curr_mask &= ~POS_NOUN; // Strip Noun; "mostly" is not a person/place/thing
        }
        free(word); 
    }

    // --- LOOK BACK (History) ---
    if (idx > start) {
        uint16_t prev_mask = trie_pool[ids[idx-1]].pos_mask;
        
        if (prev_mask & POS_DET) {
            // [Determiner] -> [Unknown] (Usually Noun or Adj)
            *curr_mask |= (POS_NOUN | POS_ADJ);
        }
        if (prev_mask & POS_PRON) {
            // [Pronoun] -> [Unknown] (Likely Verb)
            *curr_mask |= POS_VERB;
        }
    }

    // --- LOOK AHEAD (Expectation) ---
    if (idx < end) {
        uint16_t next_mask = trie_pool[ids[idx+1]].pos_mask;
        
        // [Unknown] -> [is/are]
        if (next_mask & POS_AUX) {
            *curr_mask |= POS_NOUN;
            *curr_mask &= ~POS_VERB; 
        }

        // [Unknown] -> [Noun]
        if (next_mask & POS_NOUN) {
            *curr_mask |= POS_ADJ;
        }

        // [Unknown] -> [Preposition]
        if (next_mask & POS_PREP) {
            // "orbits within", "runs toward"
            *curr_mask |= POS_VERB;
        }
    }
}




void apply_structural_pressure(uint32_t *window, int pos) {
    uint32_t current_id = window[pos];
    uint16_t *target = &trie_pool[current_id].pos_mask;

    // RULE 0: Do not apply pressure to the Golden Lexicon
    if (*target & POS_LOCKED) return;

    uint16_t prev_mask = trie_pool[window[pos-1]].pos_mask;
    uint16_t next_mask = trie_pool[window[pos+1]].pos_mask;

    // RULE 1: The "Subject Anchor" 
    // [Article] -> [Target] -> [Verb/Aux] 
    // Example: "The [asteroid] is..."
    if ((prev_mask & POS_DET) && (next_mask & (POS_AUX | POS_VERB))) {
        *target |= POS_NOUN;
        *target &= ~POS_ADJ; // If it's the subject of a verb, it's not an adjective here
    }
    
    // RULE 2: The "Verb Proximity" Strip
    // If followed by a Verb, it is 90% likely a Noun, not an Adj
    if (next_mask & (POS_VERB | POS_AUX)) {
        *target &= ~POS_ADJ; 
    }

    // RULE 3: The "Attributive Noun" / Descriptor
    // [Verb] -> [Target] -> [Noun]
    // Example: "...is [rubble] piles"
    if ((prev_mask & (POS_VERB | POS_AUX)) && (next_mask & POS_NOUN)) {
        *target |= POS_ADJ; 
    }
    
    // RULE 4: Noun Stacking (The Growth Rule)
    // [Noun] -> [Target] -> [Noun]
    // If surrounded by nouns, the target is likely a middle-descriptor (ADJ)
    if ((prev_mask & POS_NOUN) && (next_mask & POS_NOUN)) {
        *target |= POS_ADJ;
    }
}



void force_re_lock_lexicon() {
    size_t finite_count = sizeof(finite_list) / sizeof(FiniteEntry);
    
    for (size_t i = 0; i < finite_count; i++) {
        uint32_t id = search_word(finite_list[i].word);
        
        if (id > 0 && id < nodes_count) {
            // Apply the intended Role + the Hard Lock bit
            trie_pool[id].pos_mask = finite_list[i].mask | POS_LOCKED;

        }
    }
}


void reset_pos_planes() {
    for (uint32_t i = 0; i < nodes_count; i++) {
        // Only clear words that ARE NOT locked
        if (!(trie_pool[i].pos_mask & POS_LOCKED)) {
            trie_pool[i].pos_mask = 0; // Clear the board
        }
    }
    // Now re-run the lock to ensure the Golden Lexicon is fresh
    force_re_lock_lexicon();
}

void prune_structural_noise() {
    int pruned_count = 0;

    for (int i = 0; i < STRUCTURAL_BUCKETS; i++) {
        StructuralEntry *entry = structural_matrix[i];
        while (entry) {
            TransitionNode *prev = NULL;
            TransitionNode *curr = entry->transitions;

            while (curr) {
                // If the connection only happened once, it's likely noise
                if (curr->frequency <= 1) {
                    TransitionNode *temp = curr;
                    if (prev == NULL) {
                        entry->transitions = curr->next;
                        curr = entry->transitions;
                    } else {
                        prev->next = curr->next;
                        curr = curr->next;
                    }
                    free(temp);
                    pruned_count++;
                } else {
                    prev = curr;
                    curr = curr->next;
                }
            }
            entry = entry->next;
        }
    }
    printf(YELLOW "PRISM: Pruned %d noisy structural links." RESET "\n", pruned_count);
}


void debug_show_phrases(void) {
    int np_count = 0;
    int adjp_count = 0;
    const int MIN_STRENGTH = 2; // Only show patterns seen 2+ times

    printf(BOLD "\n--- PRISM: Structural Growth Map (Strength > 1) ---" RESET "\n");

    for (uint32_t i = 0; i < nodes_count; i++) {
        if (!(trie_pool[i].pos_mask & POS_DET)) continue;

        char *det_str = get_word_by_id(i);
        StructuralEntry *entry = get_structural_entry(i);
        if (!entry) { if (det_str) free(det_str); continue; }

        TransitionNode *t = entry->transitions;
        while (t) {
            // Only proceed if the DET -> WORD link is strong
            if (t->frequency < MIN_STRENGTH) { t = t->next; continue; }

            uint32_t next_id = t->target_id;
            uint16_t next_mask = trie_pool[next_id].pos_mask;
            char *next_str = get_word_by_id(next_id);
            
            if (next_mask & POS_ADJ) {
                StructuralEntry *adj_entry = get_structural_entry(next_id);
                if (adj_entry) {
                    TransitionNode *n = adj_entry->transitions;
                    while (n) {
                        uint16_t noun_mask = trie_pool[n->target_id].pos_mask;

                        // Filter: NOUN must exist, not be an echo, and be strong
                        if (n->target_id != next_id && 
                            n->frequency >= MIN_STRENGTH &&
                            (noun_mask & POS_NOUN) && 
                            !(noun_mask & (POS_STOP | POS_AUX | POS_DET))) {
                            
                            char *noun_str = get_word_by_id(n->target_id);
                            printf(CYAN "  [ADJ-P] " RESET "%-5s %-15s %-15s (Str: %d)\n", 
                                   det_str, next_str, noun_str, n->frequency);
                            adjp_count++;
                            if (noun_str) free(noun_str);
                        }
                        n = n->next;
                    }
                }
            } else if (next_mask & POS_NOUN) {
                printf(GREEN "  [NP]    " RESET "%-5s %-15s (Str: %d)\n", det_str, next_str, t->frequency);
                np_count++;
            }

            if (next_str) free(next_str);
            t = t->next;
        }
        if (det_str) free(det_str);
    }
}


uint32_t grammar_matrix[16][16] = {0};

void record_grammar_path(uint16_t prev_mask, uint16_t curr_mask) {
    for (int i = 0; i < 16; i++) {
        if (prev_mask & (1 << i)) { // If prev word had this tag
            for (int j = 0; j < 16; j++) {
                if (curr_mask & (1 << j)) { // And curr word has this tag
                    grammar_matrix[i][j]++;
                }
            }
        }
    }
}


float get_contextual_relevance(uint32_t candidate_id, uint32_t subject_id) {
    if (candidate_id == subject_id) return 1.0f;

    uint32_t h = hash_pair(candidate_id, subject_id);
    CoEntry *curr = co_occurrence_table[h];

    while (curr) {
        if ((curr->word_a == candidate_id && curr->word_b == subject_id) ||
            (curr->word_a == subject_id && curr->word_b == candidate_id)) {
            
            // Normalize: Ratio of co-occurrence to total appearances
            // (Assuming you track total_mentions per word in the Lexical Plane)
            return (float)curr->count / (float)trie_pool[subject_id].total_frequency;
        }
        curr = curr->next;
    }

    return 0.001f; // Baseline for words never seen together
}

void update_co_occurrence(uint32_t *window, int current_idx) {
    uint32_t current_id = window[current_idx];
    
    // Look back up to 64 tokens
    int start = (current_idx - 64 > 0) ? current_idx - 64 : 0;
    
    for (int i = start; i < current_idx; i++) {
        uint32_t past_id = window[i];
        
        // We only care about linking to the "Subject" (Nouns/Adjs)
        if (trie_pool[past_id].pos_mask & (POS_NOUN | POS_ADJ)) {
            increment_co_link(current_id, past_id);
        }
    }
}


void train_prism_planes(uint32_t *ids, size_t count, int silent) {
    if (!ids || count < 2) return;

    for (size_t i = 0; i < count; i++) {
        uint32_t current_id = ids[i];

        // 0. SAFETY: Bounds check for current node
        if (current_id >= nodes_count) continue; 

        // 1. CONTEXTUAL INFERENCE (Window of 6-8 words)
        // We look ahead and behind to set POS bits accurately
        size_t window_start = (i > 3) ? i - 3 : 0;
        size_t window_end = (i + 3 < count) ? i + 3 : count - 1;
        
        for (size_t j = i; j <= window_end; j++) {
    if (trie_pool[ids[j]].pos_mask & POS_STOP) {
        window_end = j; 
        break;
    }
}
      
        // This function now handles the "Copula Check" and "POS_LOCKED" guards
        infer_pos_with_context(ids, i, window_start, window_end);

        // 2. STRUCTURAL WEAVING (Short-term Transitions)
        if (i > 0) {
            uint32_t prev_id = ids[i-1];
            if (prev_id < nodes_count) {
                // Record how grammar bits flow (e.g., Noun -> Verb)
                record_grammar_path(trie_pool[prev_id].pos_mask, trie_pool[current_id].pos_mask);
                // Record the raw word-to-word transition
                record_transition(prev_id, current_id);
            }
        }

        // 3. STRUCTURAL PRESSURE (Contextual Refinement)
        if (i > 0 && i < count - 1) {
            if (ids[i-1] < nodes_count && ids[i+1] < nodes_count) {
                apply_structural_pressure(ids, (int)i);
            }
        }

        // 4. REASONING PLANE (Long-term Symmetric Co-occurrence)
        // Only link semantically "heavy" words (Nouns, Verbs, Adjectives)
        uint16_t current_mask = trie_pool[current_id].pos_mask;
        if (current_mask & (POS_NOUN | POS_VERB | POS_ADJ)) {
            // Check previous 64 tokens for meaningful associations
            size_t co_start = (i > 64) ? i - 64 : 0;
            for (size_t j = co_start; j < i; j++) {
                uint32_t past_id = ids[j];
                
                if (past_id < nodes_count) {
                    uint16_t past_mask = trie_pool[past_id].pos_mask;
                    if (past_mask & (POS_NOUN | POS_VERB | POS_ADJ)) {
                        // Strengthen the logical bond between these two concepts
                        increment_co_link(past_id, current_id);
                    }
                }
            }
        }
    }
    
    if (!silent) {
        printf(GREEN "PRISM: All Planes (Lexical, Structural, Reasoning) Hydrated." RESET "\n");
    }
}



void print_pos_roles(uint16_t mask) {
    int found = 0;
    for (int i = 0; i < POS_MAP_COUNT; i++) {
        if (mask & pos_map[i].flag) {
            printf(CYAN " [x] Role: %s" RESET "\n", pos_map[i].name);
            found = 1;
        }
    }
    if (!found) printf(YELLOW " [!] No specific roles assigned." RESET "\n");
}

void debug_word_tags(const char *test_word) {
    // IMPORTANT: Use search_word, not insert_word, for auditing.
    // We don't want to add typos to the Trie during an audit.
    uint32_t id = search_word(test_word); 

    if (id == 0) {
        printf(RED "PRISM Audit: Word '%s' not found in Lexical Plane." RESET "\n", test_word);
        return;
    }

    uint16_t mask = trie_pool[id].pos_mask;

    printf(BOLD "\n--- Audit Report: [%s] ---" RESET "\n", test_word);
    printf("Trie ID: %u | Bitmask: 0x%04X\n", id, mask);
    
    print_pos_roles(mask);
    printf("---------------------------\n");
}


void print_structural_patterns(const char *target_word) {
    uint32_t id = search_word(target_word);
    if (id == 0) {
        printf(RED "Word not found in Lexical Trie." RESET "\n");
        return;
    }

    uint32_t slot = id % STRUCTURAL_BUCKETS;
    StructuralEntry *entry = structural_matrix[slot];

    // Find the specific entry in the hash chain
    while (entry && entry->source_id != id) {
        entry = entry->next;
    }

    if (!entry || !entry->transitions) {
        printf(YELLOW "No structural transitions recorded for '%s'." RESET "\n", target_word);
        return;
    }

    printf(BOLD "\n--- [PRISM Structural Chain: %s] ---" RESET "\n", target_word);
    
    TransitionNode *t = entry->transitions;
    while (t) {
        char *word = get_word_by_id(t->target_id);
        uint16_t mask = trie_pool[t->target_id].pos_mask;

        // Determine the Role String
        char role[32] = "";
        if (mask & POS_DET)  strcat(role, "DET ");
        if (mask & POS_ADJ)  strcat(role, "ADJ ");
        if (mask & POS_NOUN) strcat(role, "NOUN ");
        if (mask & POS_VERB) strcat(role, "VERB ");

        printf("  + [%-12s] -> " CYAN "%-12s" RESET " | Count: %-2u | Role: %s\n", 
               target_word, word ? word : "???", t->frequency, role);

        if (word) free(word);
        t = t->next;
    }
    printf("------------------------------------------\n");
}

