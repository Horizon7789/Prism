#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
    
#include "prism.h"

/* =========================
   Helper Functions
   ========================= */
   
// Helper to identify punctuation
int is_prism_punct(char c) {
    return (c == '.' || c == ',' || c == '!' || c == '?' || c == '-');
}

// Set an edge in a uint32_t array
static void set_edge(uint32_t *arr, size_t edge_idx, int from, int to) {
    size_t bit_pos = edge_idx * EDGE_BITS;
    uint32_t val = ((from & 0x1F) << 5) | (to & 0x1F);

    for (int i = 0; i < 10; i++) {
        if ((val >> i) & 1) {
            size_t curr_bit = bit_pos + i;
            arr[curr_bit / 32] |= (1U << (curr_bit % 32));
        }
    }
}


// Get an edge from a uint32_t array
static void get_edge(uint32_t *arr, size_t edge_idx, int *from, int *to) {
    size_t bit_pos = edge_idx * EDGE_BITS;
    uint32_t val = 0;

    for (int i = 0; i < 10; i++) {
        size_t curr_bit = bit_pos + i;
        if (arr[curr_bit / 32] & (1U << (curr_bit % 32))) {
            val |= (1U << i);
        }
    }
    *from = (val >> 5) & 0x1F;
    *to   = val & 0x1F;
}

/* =========================
   Decode edges back to word
   ========================= */
char* decode_edges_to_word(const WordEdges *we, char start_letter) {
    if (!we || we->num_edges == 0) {
        char *single = malloc(2);
        single[0] = start_letter;
        single[1] = '\0';
        return single;
    }

    char *word = malloc(we->num_edges + 2); // edges + start + null
    word[0] = start_letter;
    int from = letter_to_index(start_letter);

    for (size_t i = 0; i < we->num_edges; i++) {
        int f, t;
        get_edge(we->edges, i, &f, &t);
        if (f != from) {
            // something went wrong, mark as unknown
            word[i+1] = '?';
            from = t; // continue decoding
        } else {
            word[i+1] = index_to_letter(t);
            from = t;
        }
    }
    word[we->num_edges + 1] = '\0';
    return word;
}

/* =========================
   3. Split sentence into words
   ========================= */

char** split_sentence(const char *sentence, size_t *word_count) {
    *word_count = 0;
    size_t capacity = 16; // Start slightly larger for punctuation
    char **words = malloc(capacity * sizeof(char*));

    const char *p = sentence;
    while (*p) {
        // 1. Skip whitespace and characters we don't care about at all
        while (*p && isspace(*p)) p++;
        if (!*p) break;

        // 2. Check if the current character is a punctuation "word"
        if (*p == ',' || *p == '.' || *p == '!' || *p == '?' || *p == '-') {
            char *punct = malloc(2);
            punct[0] = *p;
            punct[1] = '\0';
            
            if (*word_count >= capacity) {
                capacity *= 2;
                words = realloc(words, capacity * sizeof(char*));
            }
            words[(*word_count)++] = punct;
            p++;
            continue; // Move to next character
        }

        // 3. Otherwise, check for alphanumeric words (letters + numbers)
        if (isalnum(*p)) {
            const char *start = p;
            while (*p && isalnum(*p)) p++;
            
            size_t len = p - start;
            char *word = malloc(len + 1);
            for (size_t i = 0; i < len; i++) {
                word[i] = tolower(start[i]);
            }
            word[len] = '\0';

            if (*word_count >= capacity) {
                capacity *= 2;
                words = realloc(words, capacity * sizeof(char*));
            }
            words[(*word_count)++] = word;
        } else {
            // Ignore any other weird symbols (brackets, quotes, etc.) 
            // unless you want to add them to the punct list above
            p++;
        }
    }
    return words;
}


/* =========================
   4. Free word edges
   ========================= */

void free_word_edges(WordEdges *we) {
    if (!we) return;
    free(we->edges);
    we->edges = NULL;
    we->num_edges = 0;
}

/**
 * Save an array of WordEdges to file.
 * Each word stores:
 *   1. Number of edges (uint32_t)
 *   2. Bit-packed edges array length (uint32_t)
 *   3. Edges array (uint32_t[])
 */

WordEdges* load_edges_from_file(const char *filename, size_t *out_count) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        *out_count = 0;
        return NULL;
    }

    uint32_t total_words = 0;
    if (fread(&total_words, sizeof(uint32_t), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    WordEdges *edges_array = malloc(total_words * sizeof(WordEdges));
    if (!edges_array) { fclose(f); return NULL; }

    for (uint32_t i = 0; i < total_words; i++) {
        // 1. Read start letter
        fread(&edges_array[i].start_letter, sizeof(uint8_t), 1, f);

        // 2. Read edge count
        uint32_t num_edges = 0;
        fread(&num_edges, sizeof(uint32_t), 1, f);
        edges_array[i].num_edges = num_edges;

        // 3. Read edge data
        if (num_edges > 0) {
            edges_array[i].edges = malloc(num_edges * sizeof(uint16_t));
            fread(edges_array[i].edges, sizeof(uint16_t), num_edges, f);
        } else {
            edges_array[i].edges = NULL;
        }
    }

    fclose(f);
    *out_count = (size_t)total_words;
    return edges_array;
}


// Maps a character to a safe 0-63 index for the Trie/Edge Store
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "prism.h"

/* =========================
   1. Character Mapping (The Bridge)
   ========================= */

uint8_t char_to_idx(char c) {
    c = tolower(c);
    if (c >= 'a' && c <= 'z') return c - 'a';          // 0-25
    if (c >= '0' && c <= '9') return c - '0' + 26;     // 26-35
    if (c == '.') return 36;
    if (c == ',') return 37;
    if (c == '!') return 38;
    if (c == '?') return 39;
    if (c == '-') return 40;
    return 0; 
}


char idx_to_char(uint8_t idx) {
    if (idx <= 25) return 'a' + idx;
    if (idx >= 26 && idx <= 35) return '0' + (idx - 26);
    switch (idx) {
        case 36: return '.';
        case 37: return ',';
        case 38: return '!';
        case 39: return '?';
        case 40: return '-';
        default: return ' ';
    }
}

/* =========================
   2. Encoding (Writing to Memory)
   ========================= */

WordEdges encode_word_to_edges(const char *word) {
    WordEdges we;
    we.start_letter = tolower(word[0]); 
    
    size_t len = strlen(word);
    we.num_edges = (len > 1) ? (uint32_t)(len - 1) : 0;

    if (we.num_edges > 0) {
        we.edges = malloc(we.num_edges * sizeof(uint16_t));
        for (size_t i = 0; i < we.num_edges; i++) {
            // USE THE MAPPING TABLE
            we.edges[i] = (uint16_t)char_to_idx(word[i+1]); 
        }
    } else {
        we.edges = NULL;
    }
    return we;
}

/* =========================
   3. Reconstruction (Reading for Replay)
   ========================= */

char* reconstruct_word_from_edges(WordEdges *we, char start_letter) {
    if (!we) return NULL;

    // Length is num_edges + start_char + null terminator
    char *word = malloc(we->num_edges + 2);
    word[0] = start_letter;

    for (uint32_t i = 0; i < we->num_edges; i++) {
        // USE THE MAPPING TABLE
        word[i + 1] = idx_to_char((uint8_t)we->edges[i]);
    }
    word[we->num_edges + 1] = '\0';
    return word;
}

/* =========================
   4. Persistence (Disk I/O)
   ========================= */

void save_edges_to_file(const char *filename, WordEdges *edges_array, size_t word_count) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;

    uint32_t wc = (uint32_t)word_count;
    fwrite(&wc, sizeof(uint32_t), 1, f);

    for (size_t i = 0; i < word_count; i++) {
        fwrite(&edges_array[i].start_letter, sizeof(uint8_t), 1, f);
        fwrite(&edges_array[i].num_edges, sizeof(uint32_t), 1, f);
        if (edges_array[i].num_edges > 0) {
            fwrite(edges_array[i].edges, sizeof(uint16_t), edges_array[i].num_edges, f);
        }
    }
    fclose(f);
}

void replay_history(int limit) {
    FILE *f = fopen("packed_edges.bin", "rb");
    if (!f) {
        printf(RED "No history found." RESET "\n");
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    size_t total_tokens = file_size / sizeof(uint32_t);

    if (total_tokens == 0) {
        fclose(f);
        return;
    }

    size_t to_read = (total_tokens > (size_t)limit) ? (size_t)limit : total_tokens;
    fseek(f, -(to_read * sizeof(uint32_t)), SEEK_END);

    uint32_t *buffer = malloc(to_read * sizeof(uint32_t));
    if (!buffer) { fclose(f); return; }
    fread(buffer, sizeof(uint32_t), to_read, f);
    fclose(f);

    printf(BOLD CYAN "\n--- PRISM Replay (Last %zu tokens) ---" RESET "\n", to_read);
    
    int capitalize_next = 1;
    for (size_t i = 0; i < to_read; i++) {
        char *word = get_word_by_id(buffer[i]);
        if (!word) continue;

        // 1. Capitalization
        if (capitalize_next && isalpha(word[0])) {
            word[0] = toupper(word[0]);
            capitalize_next = 0;
        }

        // 2. Print the word/punctuation
        printf("%s", word);

        // 3. Smart Spacing & Sentence Handling
        if (is_id_sentence_ender(buffer[i])) {
            printf("\n");
            capitalize_next = 1;
        } else {
            int should_space = 1;
            
            // PEEK: If the next token is punctuation, don't print a space now
            if (i + 1 < to_read) {
                if (is_id_punctuation(buffer[i+1])) {
                    should_space = 0;
                }
            }
            
            // Don't space after hyphens (for "c-type")
            if (word[0] == '-') should_space = 0;

            if (should_space) printf(" ");
        }

        free(word);
    }
    printf(BOLD CYAN "\n---------------------------------------" RESET "\n\n");
    free(buffer);
}


void replay_specific(const char *target_word) {
    // 1. Find the Node ID for the target word first
    // We use find_word (or a similar lookup) to get the ID
    uint32_t target_id = 0;
    
    // We need a helper or use insert_word (which returns existing ID if found)
    target_id = insert_word(target_word); 
    if (target_id == 0) {
        printf(RED "Word not found in Lexical Matrix." RESET "\n");
        return;
    }

    // 2. Load the ID-based history
    FILE *f = fopen("packed_edges.bin", "rb");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    size_t count = file_size / sizeof(uint32_t);
    rewind(f);

    uint32_t *history = malloc(file_size);
    if (!history) { fclose(f); return; }
    fread(history, sizeof(uint32_t), count, f);
    fclose(f);

    int found_context = 0;
    int capitalize_next = 1;

    for (size_t i = 0; i < count; i++) {
        // Integer comparison is MUCH faster than strcmp
        if (!found_context && history[i] == target_id) {
            found_context = 1;
        }

        if (found_context) {
            char *decoded = get_word_by_id(history[i]);
            if (!decoded) continue;

            // Apply capitalization
            if (capitalize_next && isalpha(decoded[0])) {
                decoded[0] = toupper(decoded[0]);
                capitalize_next = 0;
            }

            printf("%s", decoded);

            // Spacing Logic
            int should_space = 1;
            if (decoded[0] == '-') should_space = 0;
            
            // Peek ahead for punctuation to avoid "word . "
            if (i + 1 < count) {
                char *next_word = get_word_by_id(history[i+1]);
                if (next_word) {
                    if (next_word[0] == '.' || next_word[0] == ',' || 
                        next_word[0] == '!' || next_word[0] == '?') {
                        should_space = 0;
                    }
                    free(next_word);
                }
            }

            if (should_space) printf(" ");

            // Sentence Ends
            if (decoded[0] == '.' || decoded[0] == '!' || decoded[0] == '?') {
                printf("\n");
                capitalize_next = 1;
                // Optional: Stop after one paragraph
                // found_context = 0; 
            }
            free(decoded);
        }
    }
    free(history);
}
