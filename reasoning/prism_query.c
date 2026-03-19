#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>
    
#include "prism.h"

#ifdef WEB

#include <curl/curl.h>
    
typedef struct { char *data; size_t size; } WebBuf;

static size_t web_write_cb(void *ptr, size_t sz, size_t n, void *ud) {
    WebBuf *b = (WebBuf*)ud;
    size_t chunk_size = sz * n;

    // Safely resize buffer
    char *tmp = realloc(b->data, b->size + chunk_size + 1);
    if (!tmp) {
        // Allocation failed, stop the transfer
        fprintf(stderr, "[ERROR] Memory allocation failed in web_write_cb\n");
        return 0; // Returning 0 tells libcurl to abort
    }
    b->data = tmp;

    // Copy the new data into the buffer
    memcpy(b->data + b->size, ptr, chunk_size);

    // Update the buffer size
    b->size += chunk_size;

    // Null-terminate to make it a valid C string
    b->data[b->size] = '\0';

    // Tell libcurl we processed all bytes
    return chunk_size;
}

#include <json-c/json.h>

static char* fetch_wikipedia(const char *subject) {
    // Encode spaces to underscores
    char url[512], encoded[256] = {0};
    int j = 0;
    for (int i = 0; subject[i] && j < 254; i++)
        encoded[j++] = (subject[i] == ' ') ? '_' : subject[i];

    snprintf(url, sizeof(url),
             "https://en.wikipedia.org/api/rest_v1/page/summary/%s", encoded);

    // Initialize CURL
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    WebBuf buf = { malloc(1), 0 };
    if (!buf.data) { curl_easy_cleanup(curl); return NULL; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, web_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "BLAF/0.6");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) { free(buf.data); return NULL; }

    // Parse JSON using json-c
    struct json_object *json_root = json_tokener_parse(buf.data); // renamed
    free(buf.data);
    if (!json_root) return NULL;

    struct json_object *extract_obj = NULL;
    if (!json_object_object_get_ex(json_root, "extract", &extract_obj)) {
        json_object_put(json_root); // free JSON root
        return NULL;
    }

    const char *extract_str = json_object_get_string(extract_obj);
    char *result = NULL;
    if (extract_str) result = strdup(extract_str);  // Allocate memory for caller
    json_object_put(json_root); // free JSON root

    return result; // Caller must free()
}

#else

static char* fetch_wikipedia(const char *subject) {
    printf("[WEB] STUB — compile with -DBLAF_USE_CURL -lcurl for live search.\n");
    char *stub = malloc(256);
    snprintf(stub, 256, "(%s: enable -DBLAF_USE_CURL for live results.)", subject);
    return stub;
}
#endif

/* =========================================
   Handle a Wikipedia query and store words
   in the deterministic trie
   ========================================= */

void handle_query(const char *input) {
    // 1. Fetch Wikipedia summary
    void reset_pos_planes();
    char *web_facts = fetch_wikipedia(input);
    
     if (!web_facts) {
        printf(RED "Error: Failed to retrieve information for: %s" RESET "\n", input);
        return;
    }
    
    printf("\n--- Wikipedia Summary ---\n%s\n\n", web_facts);

    // 2. Split into words
    size_t wc = 0;
    char **words = split_sentence(web_facts, &wc);
    free(web_facts); 

    if (!words || wc == 0) return;

    // 3. Allocate and FILL the id_buffer
    uint32_t *id_buffer = malloc(wc * sizeof(uint32_t));
    if (!id_buffer) { /* cleanup words */ return; }

    for (size_t i = 0; i < wc; i++) {
        // insert_word MUST increment a global nodes_count internally
        id_buffer[i] = insert_word(words[i]); 
        free(words[i]); 
    }
    free(words);

    // 4. THE MULTI-PLANE TRAINING
    // If you don't call this, the co-occurrence links and PoS masks stay at 0
    train_prism_planes(id_buffer, wc, 0); 
    prune_structural_noise();

    // 5. Persistence & INTERNAL SYNC (The Missing Step)
    save_trie("lexical_trie.bin");
    
    FILE *f = fopen("packed_edges.bin", "ab"); 
    if (f) {
        fwrite(id_buffer, sizeof(uint32_t), wc, f);
        fclose(f);
    }
    
    // CRITICAL: You must rebuild the structural matrix 
    // to include the newly added words/transitions
    // Instead of re-reading the whole file, just train the new edges
    for (size_t i = 0; i < wc - 1; i++) {
        record_transition(id_buffer[i], id_buffer[i+1]);
    }

    analyze_structure(); 
    
    free(id_buffer);
    printf(GREEN "Success: PRISM processed %zu tokens." RESET "\n", wc);
}




void seed_prism_vocabulary(void) {
    const char *vocab[] = {
        // 1. EVM Stack & Arithmetic
        "push", "pop", "dup", "swap", "add", "sub", "mul", "div", "sdiv", "mod", 
        "smod", "addmod", "mulmod", "exp", "signextend", "lt", "gt", "slt", "sgt", "eq",

        // 2. Logic & Bitwise
        "iszero", "and", "or", "xor", "not", "byte", "shl", "shr", "sar", "keccak256", 
        "sha3", "address", "balance", "origin", "caller", "callvalue", "calldataload", "calldatasize", "calldatacopy", "codesize",

        // 3. Memory & Storage Ops
        "codecopy", "gasprice", "extcodesize", "extcodecopy", "returndatasize", "returndatacopy", "extcodehash", "blockhash", "coinbase", "timestamp", 
        "number", "difficulty", "gaslimit", "chainid", "selfbalance", "basefee", "mload", "mstore", "mstore8", "sload",

        // 4. State & Execution Flow
        "sstore", "jump", "jumpi", "pc", "msize", "gas", "jumpdest", "tload", "tstore", "create", 
        "call", "callcode", "return", "delegatecall", "create2", "staticcall", "revert", "invalid", "selfdestruct", "stop",

        // 5. Higher-Level Syntax & Security
        "immutable", "constant", "virtual", "override", "indexed", "anonymous", "unchecked", "receive", "fallback", "constructor", 
        "salt", "offset", "length", "selector", "sig", "data", "stack", "heap", "pointer", "proxy",
    
        // 1. Logic & Conditionals
        "if", "then", "else", "when", "while", "for", "because", "not", "and", "or", 
        "true", "false", "equals", "matches", "exists", "becomes", "triggers", "causes", "prevents", "requires",
        
        // 2. Blockchain & Security
        "contract", "address", "balance", "sender", "owner", "caller", "value", "amount", "gas", "limit", 
        "transaction", "transfer", "mint", "burn", "deposit", "withdraw", "approve", "allowance", "mapping", "struct", 
        "array", "event", "emit", "revert", "require", "assert", "modifier", "internal", "public", "private", 
        "external", "view", "pure", "payable", "storage", "memory", "calldata", "library", "interface", "implementation",
        
        // 3. Vulnerability & Risk
        "reentrancy", "overflow", "underflow", "inflation", "rounding", "manipulation", "exploit", "attack", "vulnerability", "flaw", 
        "bug", "risk", "threat", "bypass", "unauthorized", "permanent", "failure", "collision", "delegatecall", "selfdestruct",
        
        // 4. Reasoning & Actions
        "calculate", "analyze", "verify", "check", "compare", "deduce", "simulate", "predict", "find", "scan", 
        "report", "audit", "explain", "summarize", "increase", "decrease", "update", "remove", "change", "result"
    };

    int total = sizeof(vocab) / sizeof(vocab[0]);
    printf("PRISM: Seeding %d core tokens...\n", total);

    for (int i = 0; i < total; i++) {
        // Replace 'learn_word' with your actual insertion function name
        handle_query(vocab[i]); 
    }

    printf("Success: PRISM core vocabulary initialized.\n");
}

