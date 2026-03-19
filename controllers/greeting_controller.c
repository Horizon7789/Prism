#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
    
#include "prism.h"

// The "Growth" Table: Add new rows here to expand PRISM's personality
GreetingMap interaction_table[] = {
    {
        .keywords = {"hi", "hello", "hey", "greetings", NULL},
        .responses = {
            "PRISM: Hello. Systems are stable.",
            "PRISM: Greetings. How shall we expand the matrix today?",
            "PRISM: Connection established. I am ready."
        }
    },
    {
        .keywords = {"how are you", "status", "health", NULL},
        .responses = {
            "PRISM: All modules nominal. Entropy is low.",
            "PRISM: Processing efficiency at 98%. I am functioning well.",
            "PRISM: My symbolic matrix is fully synchronized."
        }
    },
    {
        .keywords = {"bye", "exit", "shutdown", NULL},
        .responses = {
            "PRISM: Terminating session. Goodbye.",
            "PRISM: Matrix suspended. See you soon.",
            "PRISM: Powering down reasoning modules."
        }
    }
};


#define NUM_INTERACTIONS (sizeof(interaction_table) / sizeof(GreetingMap))

int check_greetings(const char *input) {
    static int seeded = 0;
    if (!seeded) { srand(time(NULL)); seeded = 1; }

    for (size_t i = 0; i < NUM_INTERACTIONS; i++) {
        for (int j = 0; interaction_table[i].keywords[j] != NULL; j++) {
            
            // Check if the input contains the keyword (e.g., "hello there" matches "hello")
            if (strcasestr(input, interaction_table[i].keywords[j])) {
                
                // Pick a random response from the available 3
                int res_idx = rand() % 3;
                printf(BLUE BOLD "%s" RESET "\n", interaction_table[i].responses[res_idx]);
                return 1; // Handled
            }
        }
    }
    return 0; // Not a greeting
}
