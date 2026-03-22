#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "prism.h"

#define MAX_MEMORY_MB 500 
    
// Global Pool State

CompactNode *trie_pool = NULL;
uint32_t pool_size = 1000000000;
uint32_t nodes_count = 1;

uint32_t free_slots[MAX_FREE_SLOTS];
uint32_t free_ptr = 0; // Points to the next available recycled index


/* ==========================
   Utility & Setup
   ========================== */
void print_banner(void) {
    printf(BOLD GREEN
        "╔════════════════════════════════════════════════════════════════════════╗\n"
        "║                                                                        ║\n"
        "║  ██████╗ ██████╗ ██╗███████╗███╗   ███╗                                ║\n"
        "║  ██╔══██╗██╔══██╗██║██╔════╝████╗ ████║                                ║\n"
        "║  ██████╔╝██████╔╝██║███████╗██╔████╔██║                                ║\n"
        "║  ██╔═══╝ ██╔══██╗██║╚════██║██║╚██╔╝██║                                ║\n"
        "║  ██║     ██║  ██║██║███████║██║ ╚═╝ ██║                                ║\n"
        "║  ╚═╝     ╚═╝  ╚═╝╚═╝╚══════╝╚═╝     ╚═╝                                ║\n"
        "║                                                                        ║\n"
        "║  PRISM — Precise Reasoning & Intelligent Symbolic Matrix               ║\n"
        "║  Deterministic · Symbolic · Local · Lexical & Structural AI            ║\n"
        "║                                                                        ║\n"
        "║  CORE COMMANDS                                                         ║\n"
        "║  ────────────────────────────────────────────────────────────────────  ║\n"
        "║  speak <word> [n]   → Generate n sentences from seed word              ║\n"
        "║  tags <word>        → Show POS tags and lexical classification         ║\n"
        "║  context <word>     → Inspect structural/context relationships         ║\n"
        "║  replay [word]      → Replay history (global or specific word)         ║\n"
        "║  trie words         → Print all stored vocabulary in Trie              ║\n"
        "║  stats              → Show memory and system statistics                ║\n"
        "║                                                                        ║\n"
        "║  SYSTEM COMMANDS                                                       ║\n"
        "║  ────────────────────────────────────────────────────────────────────  ║\n"
        "║  seed vocab         → Load initial vocabulary into PRISM               ║\n"
        "║  free memory        → Clear structural matrix                          ║\n"
        "║  clear / cls        → Clear terminal screen                            ║\n"
        "║                                                                        ║\n"
        "║  MODES                                                                 ║\n"
        "║  ────────────────────────────────────────────────────────────────────  ║\n"
        "║  mode reasoning     → Default reasoning engine                         ║\n"
        "║  mode shell         → Execute system commands                          ║\n"
        "║  mode audit         → Debugging & inspection tools                     ║\n"
        "║  mode train         → Silent learning mode                             ║\n"
        "║                                                                        ║\n"
        "║  EXIT                                                                 ║\n"
        "║  ────────────────────────────────────────────────────────────────────  ║\n"
        "║  exit / quit        → Shutdown PRISM (with save option)                ║\n"
        "║                                                                        ║\n"
        "║  Version: %1f.0  |  Offline Reasoning  |  Deterministic AI Core         ║\n"
        "╚════════════════════════════════════════════════════════════════════════╝\n"
        RESET, PRISM_VERSION);
}


void init_trie(void) {
    if (trie_pool) return;
    
    trie_pool = calloc(pool_size, sizeof(CompactNode));
    if (!trie_pool) { perror("Initial pool allocation"); exit(1); }
    
    // Initialize 26 root nodes (a-z) at indices 1-26
    nodes_count = 1 + ALPHABET_SIZE; 
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        trie_pool[i + 1].letter_idx = i;
    }
}


/**
 * Search for a word in the trie.
 * Returns:
 *   - The node ID of the full word if it exists (is_word == 1)
 *   - Otherwise, the node ID of the longest matching prefix
 *   - Returns 0 if no prefix matches at all
 */


uint32_t search_word(const char *word) {
    if (!word || !word[0] || nodes_count == 0) return 0;

    uint32_t curr_idx = 0;       // root
    uint32_t last_match = 0;     // longest valid word found

    for (int i = 0; word[i] != '\0'; i++) {
        uint8_t target_letter = char_to_idx(word[i]); // ✅ FIXED

        uint32_t child_idx = trie_pool[curr_idx].first_child;
        uint32_t found = 0;

        while (child_idx != 0) {
            if (trie_pool[child_idx].letter_idx == target_letter) {
                found = child_idx;
                break;
            }
            child_idx = trie_pool[child_idx].next_sibling;
        }

        if (!found) {
            return last_match; // longest prefix
        }

        curr_idx = found;

        if (trie_pool[curr_idx].is_word) {
            last_match = curr_idx;
        }
    }

    return last_match;
}

void build_word_from_id(uint32_t id, char *buffer, size_t max_len) {
    int pos = 0;
    uint32_t curr = id;

    while (curr != 0 && pos < (int)(max_len - 1)) {
        buffer[pos++] = idx_to_char(trie_pool[curr].letter_idx);
        curr = trie_pool[curr].parent;
    }

    // reverse
    for (int i = 0; i < pos / 2; i++) {
        char tmp = buffer[i];
        buffer[i] = buffer[pos - 1 - i];
        buffer[pos - 1 - i] = tmp;
    }

    buffer[pos] = '\0';
}

void prune_nodes() {
    uint32_t reclaimed = 0;
    
    // Start from the last occupied node and work backwards
    for (uint32_t i = nodes_count - 1; i > 0; i--) {
        // Condition: No children AND not a marked word
        if (trie_pool[i].first_child == 0 && trie_pool[i].is_word == 0) {
            uint32_t p = trie_pool[i].parent;
            
            // 1. Unlink from Parent's Sibling Chain
            uint32_t prev = 0;
            uint32_t curr = trie_pool[p].first_child;
            while (curr != 0) {
                if (curr == i) {
                    if (prev == 0) trie_pool[p].first_child = trie_pool[i].next_sibling;
                    else trie_pool[prev].next_sibling = trie_pool[i].next_sibling;
                    break;
                }
                prev = curr;
                curr = trie_pool[curr].next_sibling;
            }

            // 2. THE FIX: Push to Recycle Bin (The "Memory Push")
            if (free_ptr < MAX_FREE_SLOTS) {
                free_slots[free_ptr++] = i; 
            }

            // 3. Wipe data so it doesn't ghost
            memset(&trie_pool[i], 0, sizeof(CompactNode));
            reclaimed++;
        }
    }

    // 4. Physical Push: If we cleared the very end of the pool, 
    // we can actually shrink the nodes_count.
    while (nodes_count > 0 && trie_pool[nodes_count - 1].letter_idx == 0) {
        nodes_count--;
        
        // If we shrunk the count, we should remove those indices 
        // from the recycle bin since they are now "beyond the wall"
        for (uint32_t j = 0; j < free_ptr; j++) {
            if (free_slots[j] >= nodes_count) {
                free_slots[j] = free_slots[--free_ptr];
            }
        }
    }

    printf(GREEN "PRISM: Memory Pushed. Reclaimed: %u | Recycle Bin: %u\n" RESET, reclaimed, free_ptr);
}



void log_expansion(uint32_t new_count) {
    FILE *log = fopen("prism_growth.log", "a");
    if (log) {
        time_t now = time(NULL);
        float mb_size = (float)(new_count * sizeof(CompactNode)) / (1024 * 1024);
        fprintf(log, "[%ld] Expansion: New Pool Size = %u nodes (%.2f MB)\n", 
                now, new_count, mb_size);
        fclose(log);
    }
}


uint32_t create_node(uint8_t letter, uint32_t parent) {
    size_t current_mem = pool_size * sizeof(CompactNode);
    size_t max_mem = (size_t)MAX_MEMORY_MB * 1024 * 1024;
    uint32_t new_idx;

    // --- 1. Memory Boundary & Management ---
    if (nodes_count >= pool_size) {
        if (free_ptr > 0) {
            new_idx = free_slots[--free_ptr]; // Reuse pruned slots
        } 
        else if (current_mem >= max_mem) {
            printf(RED "\n[!] HARD LIMIT REACHED (%d MB). FORCING PRUNE...\n" RESET, MAX_MEMORY_MB);
            prune_nodes(); 
            if (nodes_count >= pool_size && free_ptr == 0) {
                printf(RED "FATAL: Memory Exhausted. Shutdown.\n" RESET);
                save_trie("lexical_trie.bin");
                exit(1);
            }
            // If prune created a free slot, use it; otherwise, use the new count
            new_idx = (free_ptr > 0) ? free_slots[--free_ptr] : nodes_count++;
        } else {
            // Interactive Expansion Logic
            printf(YELLOW "\n[!] PRISM MEMORY FULL: %u nodes. [e]xpand, [p]rune, [s]ave: " RESET, nodes_count);
            char choice;
            if (scanf(" %c", &choice) != 1) choice = 's';

            if (choice == 'e') {
                uint32_t old_size = pool_size;
                pool_size *= 2;
                trie_pool = realloc(trie_pool, pool_size * sizeof(CompactNode));
                if (!trie_pool) exit(1);
                memset(trie_pool + old_size, 0, (pool_size - old_size) * sizeof(CompactNode));
                new_idx = nodes_count++;
            } else if (choice == 'p') {
                prune_nodes();
                if (free_ptr == 0) return 0; 
                new_idx = free_slots[--free_ptr];
            } else {
                save_trie("lexical_trie.bin");
                exit(0);
            }
        }
    } else {
        new_idx = nodes_count++;
    }

    // --- 2. Node Initialization & LCRS Linking ---
    trie_pool[new_idx].letter_idx = letter;
    trie_pool[new_idx].parent = parent;
    trie_pool[new_idx].is_word = 0;
    trie_pool[new_idx].first_child = 0;
    trie_pool[new_idx].pos_mask = 0;
    trie_pool[new_idx].total_frequency = 1;

    // Linking: New node becomes the Head of the sibling list (Prepend)
    trie_pool[new_idx].next_sibling = trie_pool[parent].first_child;
    trie_pool[parent].first_child = new_idx;
    
    return new_idx;
}


uint32_t insert_word(const char *word) {
    if (!word || !word[0]) return 0;

    uint32_t current = 0; // root

    for (int i = 0; word[i]; i++) {
        uint8_t l_idx = char_to_idx(word[i]);
        uint32_t found = 0;

        uint32_t child = trie_pool[current].first_child;
        while (child) {
            if (trie_pool[child].letter_idx == l_idx) {
                found = child;
                break;
            }
            child = trie_pool[child].next_sibling;
        }

        if (!found) {
            found = create_node(l_idx, current);
            if (found == 0) return 0; // safety
        } else {
            if (trie_pool[found].total_frequency < 255)
                trie_pool[found].total_frequency++;
        }

        current = found;
    }

    trie_pool[current].is_word = 1;
    return current;
}

int find_word(const char *word) {
    if (!word || !word[0]) return 0;

    int first_idx = tolower(word[0]) - 'a';
    if (first_idx < 0 || first_idx >= ALPHABET_SIZE) return 0;

    uint32_t current = first_idx + 1;

    for (int i = 1; word[i]; i++) {
        uint8_t target = tolower(word[i]) - 'a';
        uint32_t child = trie_pool[current].first_child;
        
        int found = 0;
        while (child != 0) {
            if (trie_pool[child].letter_idx == target) {
                current = child;
                found = 1;
                break;
            }
            child = trie_pool[child].next_sibling;
        }
        if (!found) return 0;
    }
    return trie_pool[current].is_word;
}

char* decode_word(const char *word) {
    return find_word(word) ? strdup(word) : NULL;
}

int load_trie(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return 0; 

    // 1. Read Version First (The "Guard")
    float version = 0;
    if (fread(&version, sizeof(float), 1, f) != 1 || version != PRISM_VERSION) {
        fprintf(stderr, RED "Error: Incompatible Trie version (Found %.1f, Expected %.1f)\n" RESET, 
                version, PRISM_VERSION);
        fclose(f);
        return 0;
    }

    // 2. Read Node Count
    uint32_t incoming_count = 0;
    if (fread(&incoming_count, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    // 3. Setup Pool
    nodes_count = incoming_count;
    pool_size = nodes_count + 1000;

    if (trie_pool) free(trie_pool);
    trie_pool = calloc(pool_size, sizeof(CompactNode));
    
    if (!trie_pool) {
        perror("Failed to allocate pool during load");
        exit(1);
    }

    // 4. Read the actual pool data
    fread(trie_pool, sizeof(CompactNode), nodes_count, f);
    fclose(f);
    
    printf(GREEN "Trie loaded (%u nodes) from %s\n" RESET, nodes_count, filename);
    return 1; 
}


/* ==========================
   Persistence (IO)
   ========================== */
void save_trie(const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) { perror("Save Trie"); return; }

    // Header: Node count and Version for compatibility
    float version = PRISM_VERSION;
    fwrite(&version, sizeof(float), 1, f);
    fwrite(&nodes_count, sizeof(uint32_t), 1, f);
    
    // Save the entire active pool including the new parent indices
    fwrite(trie_pool, sizeof(CompactNode), nodes_count, f);

    fclose(f);
    
    printf(GREEN "Trie saved (%u nodes, Ver %.1f) to %s\n" RESET, nodes_count, version, filename);
}


/* ==========================
   Cleanup & Interface
   ========================== */
void free_trie(void) {
    if (trie_pool) {
        free(trie_pool);
        trie_pool = NULL;
    }
    nodes_count = 1;
}

void prism_shutdown() {
    printf(BLUE "PRISM: Finalizing symbolic matrix...\n" RESET);
    save_trie("lexical_trie.bin");
    free_trie();
    printf(GREEN "Backup complete. Systems offline.\n" RESET);
}

#include <sys/stat.h>

// Helper to get file size in bytes
long get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) return st.st_size;
    return 0;
}


void print_stats(void) {
    // Memory calculations
    size_t bytes_used = nodes_count * sizeof(CompactNode);
    size_t bytes_allocated = pool_size * sizeof(CompactNode);

    // File sizes
    long trie_file_size = get_file_size("lexical_trie.bin");
    long history_file_size = get_file_size("packed_edges.bin");

    // 1. Calculate Unique Words in Trie
    uint32_t unique_words = 0;
    for (uint32_t i = 1; i < nodes_count; i++) {
        if (trie_pool[i].is_word) unique_words++;
    }

    // 2. Calculate Total Tokens in History
    // Since we store raw uint32_t IDs, tokens = total bytes / 4
    uint32_t total_tokens = (history_file_size > 0) ? (uint32_t)(history_file_size / sizeof(uint32_t)) : 0;
    
    uint32_t active_nodes = nodes_count - free_ptr; // Real density
    printf("--- PRISM Matrix Statistics ---\n");
    printf(BLUE "[Memory Pool]\n" RESET);
    printf("  Bytes Used/Allocated: %zu / %zu\n", bytes_used, bytes_allocated);
    printf("  Nodes (Active/Total): %u / %u\n", active_nodes, nodes_count);
    printf("  Recycle Bin:          %u slots available\n", free_ptr);
    printf("  Memory Usage:         %.2f MB / %.2f MB\n", 
            (float)(nodes_count * sizeof(CompactNode)) / (1024*1024),
            (float)(pool_size * sizeof(CompactNode)) / (1024*1024));


    // Section 2: The Lexical Brain
    printf("\n" BLUE "[Knowledge Base: lexical_trie.bin]" RESET "\n");
    printf("  Unique Words:   " CYAN "%u" RESET "\n", unique_words);
    printf("  Trie Nodes:     " CYAN "%u" RESET "\n", nodes_count);
    printf("  Disk Size:      " GREEN "%.2f KB" RESET "\n", (double)trie_file_size / 1024);

    // Section 3: The Sequential Memory
    printf("\n" BLUE "[History Store: packed_edges.bin]" RESET "\n");
    printf("  Total Tokens:   " CYAN "%u" RESET "\n", total_tokens);
    printf("  Format:         " WHITE "Raw uint32_t IDs" RESET "\n");
    
    if (history_file_size < 1024 * 1024) {
        printf("  Disk Size:      " GREEN "%.2f KB" RESET "\n", (double)history_file_size / 1024);
    } else {
        printf("  Disk Size:      " GREEN "%.2f MB" RESET "\n", (double)history_file_size / (1024 * 1024));
    }

    printf("-------------------------------\n");
}

void print_all_words_recursive(uint32_t node_idx, char *buffer, int depth) {
    if (node_idx == 0 || node_idx >= nodes_count) return;

    CompactNode *node = &trie_pool[node_idx];

    buffer[depth] = idx_to_char(node->letter_idx);

    if (node->is_word) {
        buffer[depth + 1] = '\0';
        printf("%s | ", buffer);
    }

    uint32_t child = node->first_child;
    while (child != 0) {
        print_all_words_recursive(child, buffer, depth + 1);
        child = trie_pool[child].next_sibling;
    }
}

void print_all_words(void) {
    char buffer[256];
    printf("\n--- Knowledge Base Vocabulary ---\n");

    uint32_t child = trie_pool[0].first_child; // root children
    while (child != 0) {
        print_all_words_recursive(child, buffer, 0);
        child = trie_pool[child].next_sibling;
    }

    printf("\n---------------------------------\n");
}

void print_trie_ids_recursive(uint32_t node_idx, char *buffer, int depth) {
    if (node_idx == 0 || node_idx >= nodes_count) return;

    CompactNode *node = &trie_pool[node_idx];
    
    buffer[depth] = idx_to_char(node->letter_idx);
    
    if (node->is_word) {
        buffer[depth + 1] = '\0';
        printf("[%u] %s | ", node_idx, buffer); // print node ID + word
    }

    uint32_t child = node->first_child;
    while (child != 0) {
        print_trie_ids_recursive(child, buffer, depth + 1);
        child = trie_pool[child].next_sibling;
    }
}

void print_trie_ids(void) {
    char buffer[256];
    printf("\n--- Trie Words with IDs ---\n");

    uint32_t child = trie_pool[0].first_child; // root children
    while (child != 0) {
        print_trie_ids_recursive(child, buffer, 0);
        child = trie_pool[child].next_sibling;
    }

    printf("\n---------------------------------\n");
}

void append_history(uint32_t *node_ids, size_t count) {
    FILE *f = fopen("packed_edges.bin", "ab"); // Append binary
    if (f) {
        fwrite(node_ids, sizeof(uint32_t), count, f);
        fclose(f);
    }

}



