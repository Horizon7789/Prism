#ifndef PRISM_GRAMMAR_H
#define PRISM_GRAMMAR_H

#include <stdint.h>
#include <stddef.h>
    
extern uint32_t grammar_matrix[16][16];
    
#define WEIGHT_STRUCTURAL 0.3f  // Raw statistical frequency
#define WEIGHT_GRAMMAR    0.4f  // Syntactic correctness (from grammar_matrix)
#define WEIGHT_CONTEXT    0.3f  // Relevance to the 64-token subject

typedef struct {
    const char *word;
    uint16_t mask;   // upgraded to 16-bit (we now exceed 8 categories)
} FiniteEntry;

typedef struct {
    uint16_t flag;
    const char *name;
} POSMap;

extern POSMap pos_map[] ;

extern const int POS_MAP_COUNT;

// Forward declare your causal entry
typedef struct CoEntry {
    uint32_t word_a;
    uint32_t word_b;
    uint32_t count;
    struct CoEntry *next;
} CoEntry;

// Global causal table
#define CAUSAL_TABLE_SIZE 65536  // adjust as needed
extern CoEntry *causal_table[CAUSAL_TABLE_SIZE];

#define CO_HASH_SIZE 32768   // Power of 2 is better for masking
extern CoEntry *co_occurrence_table[CO_HASH_SIZE];
#define MAX_CO_ENTRIES 100000 // Limit to keep within 500MB

// Sparse Table and Arena

extern CoEntry entry_pool[MAX_CO_ENTRIES];
extern uint32_t entries_used ;


/* =========================
   PART-OF-SPEECH BITMASKS
   ========================= */

#define POS_NOUN      0x0001
#define POS_VERB      0x0002
#define POS_ADJ       0x0004
#define POS_ADV       0x0008

#define POS_DET       0x0010   // Determiners / Articles
#define POS_PRON      0x0020
#define POS_PREP      0x0040
#define POS_CONJ      0x0080

#define POS_AUX       0x0100   // Auxiliary verbs
#define POS_MODAL     0x0200   // can, should, etc.

#define POS_PART      0x0400   // particles (to, not)
#define POS_INTJ      0x0800   // interjections

#define POS_NUM       0x1000   // numbers
#define POS_STOP      0x2000   // punctuation

#define POS_WH        0x4000   // who, what, when, etc.
#define POS_UNKNOWN   0x000

extern const int FINITE_COUNT;

typedef struct {
    uint16_t sequence[4]; // e.g., {POS_DET, POS_ADJ, POS_NOUN, POS_VERB}
    float stability;      // How often this pattern successfully completes
} GrammarTemplate;

/* Predefined Wikipedia-style Structures */
extern GrammarTemplate logic_templates[];

// Initialization & Training
void hydrate_prism_grammar(void);
void infer_pos_with_context(uint32_t *ids, size_t idx, size_t start, size_t end);
void record_grammar_path(uint16_t prev_mask, uint16_t curr_mask);
void apply_structural_pressure(uint32_t *window, int pos);

// Reasoning & Generation
float get_contextual_relevance(uint32_t candidate_id, uint32_t subject_id);
void update_co_occurrence(uint32_t *window, int current_idx);
void increment_co_link(uint32_t a, uint32_t b);
uint32_t hash_pair(uint32_t a, uint32_t b);
//void train_prism_planes(uint32_t *ids, size_t count);
void train_prism_planes(uint32_t *ids, size_t count, int silent);
void debug_word_tags(const char *test_word);
void print_pos_roles(uint16_t mask);
void seed_golden_lexicon(void);

#endif // PRISM_GRAMMAR_H