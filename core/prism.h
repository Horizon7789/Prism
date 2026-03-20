/* PRISM.H */

#ifndef PRISM_H
#define PRISM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
    
#include "prism_grammar.h"

#define PRISM_VERSION 2.0
#define STRUCTURAL_BUCKETS 16384
#define HASH_SIZE 16384 
#define POS_LOCKED 0x8000

/* COLORS */

#define RESET "\x1b[0m"

#define BOLD  "\x1b[1m"

#define RED   "\x1b[31m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE  "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN  "\x1b[36m"
#define WHITE "\x1b[37m"

/* SIZE */

#define MAX_QUERY_INPUT        512
#define MAX_LETTERS 26       // a-z
#define EDGE_BITS 10         // 5 bits per letter (from + to)

#define PRISM_DATA_DIR          "prism_data"
#define PRISM_CACHE_DIR         "prism_cache"
#define PRISM_INDEX_DIR         "prism_index"

extern int PRISM_SILENT;

#define ALPHABET_SIZE 26

#define MAX_FREE_SLOTS 10000
extern uint32_t free_slots[MAX_FREE_SLOTS];
extern uint32_t free_ptr; // Points to the next available recycled index

typedef struct {
    uint32_t first_child;  
    uint32_t next_sibling; 
    uint32_t parent;       // <--- NEW: Allows reconstruction from ID
    uint8_t letter_idx;    
    uint8_t is_word;  
    uint16_t pos_mask;  
    uint8_t total_frequency; 
} CompactNode;


typedef struct {
    uint8_t start_letter; // Explicitly store 'a', 'b', etc.
    uint32_t num_edges;
    uint16_t *edges;
} WordEdges;


//typedef struct TrieNode TrieNode;

typedef struct EdgeNode {
    int to;                    // letter index 0-25
    struct TrieNode *child;    // pointer to next node
    struct EdgeNode *next;     // next edge in the linked list
} EdgeNode;

typedef struct {
    uint32_t node_index; // The ID of the word in the Lexical Trie
    uint8_t flags;       // Special markers (e.g., 1 for "Start of Sentence")
} HistoryEntry;

typedef struct TrieNode {
    int is_word;               // mark end of word
    EdgeNode *edges;           // linked list of edges
    
} TrieNode;

typedef struct {
    uint32_t from_id;
    uint32_t to_id;
    uint32_t frequency;
} NodeEdge;

typedef struct {
    uint32_t target_id;   // The ID of the following word
    uint32_t count;       // How many times this transition occurred
} Transition;

typedef struct {
    uint32_t source_id;   // The ID of the current word
    size_t edge_count;    // How many unique words follow this one
    Transition *targets;  // Dynamic array of following words
} StructuralNode;

typedef struct {
    uint32_t children[26];
    uint32_t word_id;
    uint8_t pos_tag; // 1: Noun, 2: Verb, 3: Adj, 4: Punctuation...
} LexicalNode;

typedef struct {
    uint32_t word_id;
    uint8_t pos_mask; // Bitwise OR of all valid tags
} LexicalMetadata;



typedef struct TransitionNode {
    uint32_t target_id;            // The ID of the word that follows
    uint32_t frequency;            // Count of how often this pair occurs
    struct TransitionNode *next;   // Next transition in the list
} TransitionNode;

typedef struct {
    uint32_t source_id;            // The ID of the current word
    TransitionNode *transitions;   // Head of the list of following words
    uint32_t total_occurrences;    // Sum of all transition frequencies
    struct StructuralEntry *next; // <--- Added: For "Chaining" collisions
} StructuralEntry;


typedef struct {
    const char *keywords[5];    // List of triggers (e.g., "hi", "hello", "hey")
    const char *responses[3];   // List of possible answers
} GreetingMap;

extern TrieNode *root[ALPHABET_SIZE];
extern StructuralEntry* get_structural_entry(uint32_t source_id);

/* Function Definition*/

static size_t web_write_cb(void *ptr, size_t sz, size_t n, void *ud);
void handle_query(const char *input);

void print_banner(void);

/* =========================
   Helper Functions
   ========================= */

// Convert a letter 'a'-'z' to index 0-25
static inline int letter_to_index(char c);

// Convert index 0-25 back to a letter 'a'-'z'
static inline char index_to_letter(int idx);

// Set an edge in the bit-packed array
static void set_edge(uint32_t *arr, size_t edge_idx, int from, int to);

// Get an edge from the bit-packed array
static void get_edge(uint32_t *arr, size_t edge_idx, int *from, int *to);


/* =========================
   1. Encode word into edges
   ========================= */

// Encode a word into a WordEdges struct, avoiding duplicate edges
WordEdges encode_word_to_edges(const char *word);


/* =========================
   2. Decode edges back to word
   ========================= */

// Reconstruct the word from WordEdges, requires first letter
char* decode_edges_to_word(const WordEdges *we, char start_letter);


/* =========================
   3. Split sentence into words
   ========================= */

// Split a sentence into lowercase words, returns array of strings and sets word_count
char** split_sentence(const char *sentence, size_t *word_count);


/* =========================
   4. Free WordEdges memory
   ========================= */

// Free the edges stored in WordEdges
void free_word_edges(WordEdges *we);

/* --- Trie Root --- */
/*extern TrieNode *root[ALPHABET_SIZE]; // pointers to root nodes*/

/* ==========================
   Trie Management
   ========================== */
void init_trie(void);

TrieNode* insert_word_node(TrieNode *node, const char *word);
uint32_t insert_word(const char *word); 


/* ==========================
   Disk Persistence
   ========================== */
void save_trie_node(FILE *f, TrieNode *node);
void save_trie(const char *filename);
TrieNode* load_trie_node(FILE *f);
int load_trie(const char *filename);

/* ==========================
   Decode / Lookup
   ========================== */
int decode_word_node(TrieNode *node, char *buffer, int depth, char **result);
char* decode_word(const char *word);

/* ==========================
   Word Edge File I/O
   ========================== */
void save_edges_to_file(const char *filename, WordEdges *edges_array, size_t count);
WordEdges* load_edges_from_file(const char *filename, size_t *out_count);
// Add to your storage section in prism.h
uint32_t* load_all_history(size_t *total_tokens);

char** decode_words_from_file(const char *filename, size_t *out_count);

int check_greetings(const char *input);

int   train_from_string(const char *text);
int   train_from_file  (const char *filepath);
void  deep_crawl       (const char *seed_topic, int seconds);

// Global Pool Access
extern CompactNode *trie_pool;
extern uint32_t nodes_count;
extern uint32_t pool_size;

int find_word(const char *word);
char* decode_word(const char *word);

void prism_shutdown(void);
void print_stats(void);

long get_file_size(const char *filename);
void print_all_words_recursive(uint32_t node_idx, char *buffer, int depth);
void print_all_words(void);

// Returns true if the ID represents a structural marker (., !, ?, etc.)
int is_id_punctuation(uint32_t id);

// Returns true specifically for sentence-ending IDs
int is_id_sentence_ender(uint32_t id);


char* reconstruct_word_from_edges(WordEdges *we, char start_letter);
void replay_history(int limit);
void replay_specific(const char *target_word);


// Climbs the Trie from a leaf ID to the root to reconstruct the string
char* get_word_by_id(uint32_t node_idx);

// Helper to convert indices back to characters (updated for punctuation)
char idx_to_char(uint8_t idx);

// Helper to convert characters to safe indices (0-63 range)
uint8_t char_to_idx(char c);

void append_history(uint32_t *node_ids, size_t count);

extern StructuralEntry *structural_matrix[STRUCTURAL_BUCKETS];

// Core Hashing function for Node IDs
uint32_t hash_id(uint32_t id);

// Records a single Word A -> Word B transition in the map
void record_transition(uint32_t src, uint32_t target);

// Scans an array of IDs (from a fetch) to teach the Structural Plane
void train_structural_plane(uint32_t *ids, size_t count);

// Returns the most statistically likely ID to follow the given current_id
uint32_t predict_next_id(uint32_t current_id, uint32_t subject_id);


// Performs a replay that can "auto-correct" based on structural probability
void replay_with_structure(uint32_t *history_ids, size_t count);

// Frees the structural matrix from memory (important for Termux sessions)
void clear_structural_plane(void);

void inspect_word_context(const char *target_word);

void analyze_structure(void);
void seed_prism_vocabulary(void);
void prune_nodes(void);

void clear_structural_matrix(void);
void generate_multi_sentence(const char *seed, int target_sentences);
uint32_t search_word(const char *word);
void print_structural_patterns(const char *target_word);
void debug_show_phrases(void);
void force_re_lock_lexicon(void);
void reset_pos_planes(void);
void prune_structural_noise(void);

/* Convert index 0-25 back to letter 'a'-'z' */
static inline int letter_to_index(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    return -1; // invalid character
}

// Convert index 0-25 back to letter
static inline char index_to_letter(int idx) {
    if (idx >= 0 && idx < MAX_LETTERS) return 'a' + idx;
    return '?';
}

void prism_shutdown();





extern FiniteEntry finite_list[];

#endif