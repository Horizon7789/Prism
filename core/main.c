#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "prism.h"

int PRISM_SILENT = 0;

TrieNode *root[ALPHABET_SIZE] = {0};

#define GRAMMAR_SIZE    16

// Weights
#define W_STRUCTURAL 0.30f
#define W_GRAMMAR    0.40f
#define W_CONTEXT    0.30f

typedef enum {
    MODE_REASONING,
    MODE_SHELL,
    MODE_AUDIT,
    MODE_TRAIN,
    MODE_DEBUG   // ← add this
} PrismMode;


/* --- 5. THE STATE-MACHINE MAIN --- */
void clean_input(char *str) {
    int len = strlen(str);
    while (len > 0 && isspace(str[len-1])) str[--len] = '\0';
}


int main(void) {
    /* ===== 1. CORE INITIALIZATION ===== */
    init_trie();
    seed_golden_lexicon();
    force_re_lock_lexicon();
    // Load the Lexical Plane (Vocabulary & PoS Tags)
    if (load_trie("lexical_trie.bin")) {
        printf(GREEN "PRISM: Lexical Plane loaded from disk.\n" RESET);
    }

    // Hydrate Hardcoded Grammar Rules (Static Truth)
    hydrate_prism_grammar();

    /* ===== 2. DATA HYDRATION ===== */
    size_t total_tokens = 0;
    uint32_t *history = load_all_history(&total_tokens); 
    
    if (history && total_tokens > 1) {
        // Hydrate the Structural, Grammar, and Reasoning Planes
        // We use '1' for silent mode to keep the Termux boot clean
        train_prism_planes(history, total_tokens, 0);
        
        // Build the 1-to-1 transition matrix for prediction
        analyze_structure();
        
        free(history); // Clean up temp buffer after planes are hydrated
    } else {
        printf(YELLOW "PRISM: No history found. Starting with empty planes.\n" RESET);
    }

    /* ===== 3. UI LIFT-OFF ===== */
    print_banner();
    

    char input[MAX_QUERY_INPUT];
    PrismMode current_mode = MODE_REASONING;

    while (1) {

        /* ===== PROMPT ===== */
        switch (current_mode) {
            case MODE_SHELL:     printf(CYAN  "PRISM:SHELL> " RESET); break;
            case MODE_AUDIT:     printf(MAGENTA "PRISM:AUDIT> " RESET); break;
            case MODE_TRAIN:     printf(RED   "PRISM:TRAIN> " RESET); break;
            default:             printf(GREEN "PRISM> " RESET); break;
            case MODE_DEBUG:
    printf(YELLOW "PRISM:DEBUG> " RESET);
    break;
        }

        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        clean_input(input);

        if (strlen(input) == 0) continue;

        /* ===== GLOBAL EXIT ===== */
        if (!strcmp(input, "exit") || !strcmp(input, "quit")) {
            if (current_mode != MODE_REASONING) {
                current_mode = MODE_REASONING;
                printf(YELLOW "Returning to Reasoning Mode\n" RESET);
                continue;
            }

            printf(YELLOW "Terminate session and save changes? (y/n): " RESET);
            char confirm[8];

            if (fgets(confirm, sizeof(confirm), stdin) &&
                tolower(confirm[0]) == 'y') {
                prism_shutdown();
                break;
            }

            printf(BLUE "Shutdown aborted. Matrix active.\n" RESET);
            continue;
        }

        /* ===== GLOBAL COMMANDS (WORK IN ALL MODES) ===== */

        if (!strcmp(input, "clear") || !strcmp(input, "cls")) {
            system("clear");
            print_banner();
            continue;
        }

        if (!strcmp(input, "mode shell"))      { current_mode = MODE_SHELL; continue; }
        if (!strcmp(input, "mode audit"))      { current_mode = MODE_AUDIT; continue; }
        if (!strcmp(input, "mode train"))      { current_mode = MODE_TRAIN; continue; }
        if (!strcmp(input, "mode reasoning"))  { current_mode = MODE_REASONING; continue; }
        if (!strcmp(input, "mode debug")) {
    current_mode = MODE_DEBUG;
    continue;
}


        /* ===== MODE-SPECIFIC EXECUTION ===== */

        switch (current_mode) {

            case MODE_DEBUG:
            PRISM_SILENT = 0;

      if (!strcmp(input, "trie words")) {
            printf("Words stored in Trie:\n");
            print_all_words();
            continue;
        }

        else if (!strcmp(input, "stats")) {
            print_stats();
            continue;
        }

        else if (!strcmp(input, "replay")) {
            replay_history(100);
            continue;
        }

        else if (strncmp(input, "replay ", 7) == 0) {
            replay_specific(input + 7);
            continue;
        }

        else if (strncmp(input, "context ", 8) == 0) {
            inspect_word_context(input + 8);
            continue;
        }

        else if (!strcmp(input, "seed vocab")) {
            seed_prism_vocabulary();
            continue;
        }
        
        else if (strncmp(input, "structure ", 10) == 0) {
            print_structural_patterns(input + 10);
            continue;
        }

        else if (!strcmp(input, "phrases")) {
            debug_show_phrases();
            continue;
        }

        else if (!strcmp(input, "free memory")) {
            clear_structural_matrix();
            continue;
        }
        
        else if (strncmp(input, "tags ", 5) == 0) {  
    // Skip the first 5 characters ("tags ") to get the word  
    const char *word_to_test = input + 5;  
      
    // Clean up trailing newline if input comes from fgets  
    char *newline = strchr(word_to_test, '\n');  
    if (newline) *newline = '\0';  
  
    debug_word_tags(word_to_test);  
    continue;  
}  

else if (strncmp(input, "speak ", 6) == 0) {  
    char seed[64];  
    int count = 1; // Default to 1 sentence  
  
    // sscanf parses the string: "speak [word] [number]"  
    int parsed = sscanf(input + 6, "%s %d", seed, &count);  
  
    if (parsed >= 1) {  
        // Ensure count doesn't exceed a safe limit for Termux  
        if (count > 5) count = 5;   
        generate_multi_sentence(seed, count);  
    } else {  
        printf(RED "Usage: speak [seed_word] [num_sentences]" RESET "\n");  
    }  
    continue;  
}  
  
  
        
        else {
        printf("Debug Commands:\n");
        printf("  stats\n");
        printf("  trie words\n");
        printf("  tags [word]\n");
        printf("  memory\n");
        }
    
        break;
        
        /* ===== SHELL MODE (FIXED) ===== */
            case MODE_SHELL:
            PRISM_SILENT = 0;
                system(input);   // ← no infinite loop anymore
                break;

            /* ===== AUDIT MODE ===== */
            case MODE_AUDIT:
            PRISM_SILENT = 0;
                if (strncmp(input, "tags ", 5) == 0) {
                    debug_word_tags(input + 5);
                } else if (!strcmp(input, "stats")) {
                    print_stats();
                } else {
                    printf("Audit Commands:\n");
                    printf("  tags [word]\n  stats\n");
                }
                break;

            /* ===== TRAIN MODE ===== */
            case MODE_TRAIN:
                PRISM_SILENT = 1;
                
                handle_query(input);  // silent learning mode
                break;

            /* ===== REASONING MODE ===== */
            case MODE_REASONING:
            PRISM_SILENT = 0;

                if (strncmp(input, "speak ", 6) == 0) {
                    char seed[64];
                    int count = 1;

                    if (sscanf(input + 6, "%63s %d", seed, &count) >= 1) {
                        if (count > 5) count = 5;
                        generate_multi_sentence(seed, count);
                    }
                    break;
                }

                if (strncmp(input, "tags ", 5) == 0) {
                    debug_word_tags(input + 5);
                    break;
                }

                if (check_greetings(input)) break;

                handle_query(input);
                break;
        }
    }

    return 0;
}
