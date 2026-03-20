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
    MODE_DEBUG
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

    if (load_trie("lexical_trie.bin")) {
        printf(GREEN "PRISM: Lexical Plane loaded from disk.\n" RESET);
    }

    hydrate_prism_grammar();

    /* ===== 2. DATA HYDRATION ===== */
    size_t total_tokens = 0;
    uint32_t *history = load_all_history(&total_tokens); 
    
    if (history && total_tokens > 1) {
        train_prism_planes(history, total_tokens, 0);
        analyze_structure();
        free(history);
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
            case MODE_DEBUG:     printf(YELLOW "PRISM:DEBUG> " RESET); break;
            default:             printf(GREEN "PRISM> " RESET); break;
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

        /* ===== GLOBAL COMMANDS ===== */
        if (!strcmp(input, "clear") || !strcmp(input, "cls")) {
            system("clear");
            print_banner();
            continue;
        }

        if (!strcmp(input, "mode shell"))      { current_mode = MODE_SHELL; continue; }
        if (!strcmp(input, "mode audit"))      { current_mode = MODE_AUDIT; continue; }
        if (!strcmp(input, "mode train"))      { current_mode = MODE_TRAIN; continue; }
        if (!strcmp(input, "mode reasoning"))  { current_mode = MODE_REASONING; continue; }
        if (!strcmp(input, "mode debug"))      { current_mode = MODE_DEBUG; continue; }

        /* ===== MODE-SPECIFIC EXECUTION ===== */
        switch (current_mode) {
            case MODE_DEBUG:
                PRISM_SILENT = 0;

                if (!strcmp(input, "trie words")) {
                    printf("Words stored in Trie:\n");
                    print_all_words();
                    continue;
                } else if (!strcmp(input, "stats")) {
                    print_stats();
                    continue;
                } else if (!strcmp(input, "replay")) {
                    replay_history(100);
                    continue;
                } else if (strncmp(input, "replay ", 7) == 0) {
                    replay_specific(input + 7);
                    continue;
                } else if (strncmp(input, "context ", 8) == 0) {
                    inspect_word_context(input + 8);
                    continue;
                } else if (!strcmp(input, "seed vocab")) {
                    seed_prism_vocabulary();
                    continue;
                } else if (strncmp(input, "structure ", 10) == 0) {
                    print_structural_patterns(input + 10);
                    continue;
                } else if (strncmp(input, "crawl ", 6) == 0) {
                    char topic[128] = {0};
                    int secs = 300;
                    int parsed = sscanf(input + 6, "%127[^\n] %d", topic, &secs);
                    if (parsed < 1) {
                        printf(RED "Usage: crawl [topic] [seconds]\n" RESET);
                        continue;
                    }
                    deep_crawl(topic, secs);
                    break;
                } else if (strncmp(input, "train file ", 11) == 0) {
                    train_from_file(input + 11);
                    save_trie("lexical_trie.bin");
                    break;
                } else if (strncmp(input, "train text ", 11) == 0) {
                    train_from_string(input + 11);
                    break;
                } else if (!strcmp(input, "phrases")) {
                    debug_show_phrases();
                    continue;
                } else if (!strcmp(input, "free memory")) {
                    clear_structural_matrix();
                    continue;
                } else if (strncmp(input, "tags ", 5) == 0) {
                    char word[128] = {0};
                    snprintf(word, sizeof(word), "%s", input + 5);
                    debug_word_tags(word);
                    continue;
                } else if (strncmp(input, "speak ", 6) == 0) {
                    char seed[64] = {0};
                    int count = 1;
                    sscanf(input + 6, "%63s %d", seed, &count);
                    if (count < 1) count = 1;
                    if (count > 5) count = 5;
                    generate_multi_sentence(seed, count);
                    continue;
                } else {
                    printf("Debug Commands:\n");
                    printf("  stats\n  trie words\n  tags [word]\n  memory\n");
                }
                break;

            case MODE_SHELL:
                PRISM_SILENT = 0;
                system(input);
                break;

            case MODE_AUDIT:
                PRISM_SILENT = 0;
                if (strncmp(input, "tags ", 5) == 0) {
                    char word[128] = {0};
                    snprintf(word, sizeof(word), "%s", input + 5);
                    debug_word_tags(word);
                } else if (!strcmp(input, "stats")) {
                    print_stats();
                } else {
                    printf("Audit Commands:\n  tags [word]\n  stats\n");
                }
                break;

            case MODE_TRAIN:
                PRISM_SILENT = 1;
                handle_query(input);
                break;

            case MODE_REASONING:
                PRISM_SILENT = 0;
                if (strncmp(input, "speak ", 6) == 0) {
                    char seed[64] = {0};
                    int count = 1;
                    sscanf(input + 6, "%63s %d", seed, &count);
                    if (count < 1) count = 1;
                    if (count > 5) count = 5;
                    generate_multi_sentence(seed, count);
                    break;
                }
                if (strncmp(input, "tags ", 5) == 0) {
                    char word[128] = {0};
                    snprintf(word, sizeof(word), "%s", input + 5);
                    debug_word_tags(word);
                    break;
                }
                if (check_greetings(input)) break;
                handle_query(input);
                break;
        }
    }

    return 0;
}