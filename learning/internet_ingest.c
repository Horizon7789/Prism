/* ================================================================
 * PRISM — internet_ingest.c  (full replacement)
 *
 * 1. handle_query()     — fetch + train immediately (no double training)
 * 2. train_from_string()— tokenise + train planes + append history
 * 3. train_from_file()  — read file → train_from_string
 * 4. deep_crawl()       — stay on Wikipedia for N seconds,
 *                         follow linked concepts automatically
 * ================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "prism.h"

#ifdef WEB
#include <curl/curl.h>
#include <json-c/json.h>

/* ── HTTP helper ─────────────────────────────────────────────── */

typedef struct { char *data; size_t size; } WebBuf;

static size_t web_write_cb(void *ptr, size_t sz, size_t n, void *ud) {
    WebBuf *b = (WebBuf *)ud;
    char *tmp = realloc(b->data, b->size + sz * n + 1);
    if (!tmp) return 0;
    b->data = tmp;
    memcpy(b->data + b->size, ptr, sz * n);
    b->size += sz * n;
    b->data[b->size] = '\0';
    return sz * n;
}

/* Fetch Wikipedia summary JSON for a topic.
 * Returns malloc'd extract string — caller must free(). */
static char *fetch_wikipedia(const char *subject) {
    char url[512], encoded[256] = {0};
    int j = 0;
    for (int i = 0; subject[i] && j < 254; i++)
        encoded[j++] = (subject[i] == ' ') ? '_' : subject[i];

    snprintf(url, sizeof(url),
             "https://en.wikipedia.org/api/rest_v1/page/summary/%s", encoded);

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    WebBuf buf = {malloc(1), 0};
    if (!buf.data) { curl_easy_cleanup(curl); return NULL; }
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  web_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "PRISM/2.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { free(buf.data); return NULL; }

    struct json_object *root = json_tokener_parse(buf.data);
    free(buf.data);
    if (!root) return NULL;

    /* Extract the plain-text summary */
    struct json_object *extract_obj = NULL;
    char *result = NULL;
    if (json_object_object_get_ex(root, "extract", &extract_obj)) {
        const char *s = json_object_get_string(extract_obj);
        if (s) result = strdup(s);
    }
    json_object_put(root);
    return result;
}

/* Fetch the list of titles linked from a Wikipedia page.
 * Returns a NULL-terminated array of malloc'd strings — caller frees. */
static char **fetch_linked_titles(const char *subject, size_t *out_count) {
    *out_count = 0;

    char url[512], encoded[256] = {0};
    int j = 0;
    for (int i = 0; subject[i] && j < 254; i++)
        encoded[j++] = (subject[i] == ' ') ? '_' : subject[i];

    /* Wikipedia action API — fetch up to 30 links per page */
    snprintf(url, sizeof(url),
        "https://en.wikipedia.org/w/api.php"
        "?action=query&titles=%s&prop=links&pllimit=30&format=json",
        encoded);

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    WebBuf buf = {malloc(1), 0};
    buf.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL,            url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  web_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "PRISM/2.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) { free(buf.data); return NULL; }

    struct json_object *root = json_tokener_parse(buf.data);
    free(buf.data);
    if (!root) return NULL;

    /* Navigate: .query.pages.*.links[].title */
    char **titles = NULL;
    size_t count  = 0;

    struct json_object *query = NULL, *pages = NULL;
    if (!json_object_object_get_ex(root, "query", &query)) goto done;
    if (!json_object_object_get_ex(query, "pages",  &pages)) goto done;

    json_object_object_foreach(pages, page_key, page_val) {
        (void)page_key;
        struct json_object *links = NULL;
        if (!json_object_object_get_ex(page_val, "links", &links)) continue;

        int len = json_object_array_length(links);
        titles  = realloc(titles, (count + len + 1) * sizeof(char *));

        for (int i = 0; i < len; i++) {
            struct json_object *link  = json_object_array_get_idx(links, i);
            struct json_object *title = NULL;
            if (json_object_object_get_ex(link, "title", &title)) {
                const char *t = json_object_get_string(title);
                if (t) titles[count++] = strdup(t);
            }
        }
    }

done:
    json_object_put(root);
    if (titles) titles[count] = NULL;
    *out_count = count;
    return titles;
}

#else

/* ── Stubs when compiled without -DWEB ──────────────────────── */
static char *fetch_wikipedia(const char *subject) {
    char *s = malloc(128);
    snprintf(s, 128, "(%s: compile with -DWEB -lcurl -ljson-c for live fetch)",
             subject);
    return s;
}
static char **fetch_linked_titles(const char *subject, size_t *out_count) {
    (void)subject; *out_count = 0; return NULL;
}

#endif /* WEB */


/* ================================================================
   TRAIN FROM STRING
   The single shared training path used by everything below.
   Tokenises, inserts into trie, runs train_prism_planes,
   appends IDs to packed_edges.bin.
   Returns number of tokens trained.
   ================================================================ */

int train_from_string(const char *text) {
    if (!text || !text[0]) return 0;

    size_t  wc    = 0;
    char  **words = split_sentence(text, &wc);
    if (!words || wc == 0) return 0;

    uint32_t *ids = malloc(wc * sizeof(uint32_t));
    if (!ids) {
        for (size_t i = 0; i < wc; i++) free(words[i]);
        free(words);
        return -1;
    }

    for (size_t i = 0; i < wc; i++) {
        ids[i] = insert_word(words[i]);
        free(words[i]);
    }
    free(words);

    /* Silent mode — don't flood console during bulk crawl */
    train_prism_planes(ids, wc, 1);

    /* Persist to history so transitions survive restarts */
    FILE *f = fopen("packed_edges.bin", "ab");
    if (f) { fwrite(ids, sizeof(uint32_t), wc, f); fclose(f); }

    free(ids);
    return (int)wc;
}


/* ================================================================
   TRAIN FROM FILE
   Reads entire file, delegates to train_from_string.
   ================================================================ */

int train_from_file(const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        printf(RED "PRISM: Cannot open: %s" RESET "\n", filepath);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return 0; }

    char *text = malloc(sz + 1);
    if (!text) { fclose(f); return -1; }

    size_t r  = fread(text, 1, sz, f);
    text[r]   = '\0';
    fclose(f);

    int tokens = train_from_string(text);
    free(text);

    printf(GREEN "PRISM: Trained %d tokens from %s" RESET "\n", tokens, filepath);
    return tokens;
}


/* ================================================================
   HANDLE QUERY  (wired training)
   Every search now immediately trains from its own result.
   No double-training — one call to train_prism_planes via
   train_from_string, no extra record_transition loop.
   ================================================================ */

void handle_query(const char *input) {
    /* Reset stale POS assignments before a fresh fetch */
    reset_pos_planes();

    char *text = fetch_wikipedia(input);
    if (!text) {
        printf(RED "PRISM: No result for: %s" RESET "\n", input);
        return;
    }

    if (!PRISM_SILENT)
        printf("\n--- Wikipedia Summary ---\n%s\n\n", text);

    /* Train immediately from the fetched text */
    int tokens = train_from_string(text);
    free(text);

    if (!PRISM_SILENT)
        printf(GREEN "PRISM: Trained %d tokens from '%s'." RESET "\n",
               tokens, input);

    /* Persist the trie after every live query */
    save_trie("lexical_trie.bin");
}


/* ================================================================
   DEEP CRAWL
   Stays on Wikipedia for `seconds` (default 300 = 5 minutes).
   Starts at `seed_topic`, fetches its summary, trains from it,
   then fetches all linked article titles and queues them.
   Continues until time runs out or the queue empties.

   Usage (add to main loop):
     if (strncmp(input, "crawl ", 6) == 0)
         deep_crawl(input + 6, 300);

     if (strncmp(input, "crawl ", 6) == 0) {
         char topic[128]; int secs = 300;
         sscanf(input + 6, "%127[^\n] %d", topic, &secs);
         deep_crawl(topic, secs);
     }
   ================================================================ */

/* Simple queue — circular buffer of topic strings */
#define CRAWL_QUEUE_MAX 512

typedef struct {
    char  **items;
    int     head, tail, size;
} CrawlQueue;

static void queue_init(CrawlQueue *q) {
    q->items = calloc(CRAWL_QUEUE_MAX, sizeof(char *));
    q->head  = q->tail = q->size = 0;
}
static int queue_push(CrawlQueue *q, const char *s) {
    if (q->size >= CRAWL_QUEUE_MAX) return 0;
    q->items[q->tail] = strdup(s);
    q->tail = (q->tail + 1) % CRAWL_QUEUE_MAX;
    q->size++;
    return 1;
}
static char *queue_pop(CrawlQueue *q) {
    if (q->size == 0) return NULL;
    char *s = q->items[q->head];
    q->items[q->head] = NULL;
    q->head = (q->head + 1) % CRAWL_QUEUE_MAX;
    q->size--;
    return s; /* caller must free */
}
static void queue_free(CrawlQueue *q) {
    for (int i = 0; i < CRAWL_QUEUE_MAX; i++) free(q->items[i]);
    free(q->items);
}

/* Visited set — simple hash to avoid re-fetching same article */
#define VISITED_SIZE 1024
typedef struct VisitNode { char *topic; struct VisitNode *next; } VisitNode;
static VisitNode *visited[VISITED_SIZE] = {0};

static uint32_t topic_hash(const char *s) {
    uint32_t h = 5381;
    while (*s) h = ((h << 5) + h) + (unsigned char)*s++;
    return h % VISITED_SIZE;
}
static int was_visited(const char *s) {
    uint32_t h = topic_hash(s);
    for (VisitNode *n = visited[h]; n; n = n->next)
        if (strcasecmp(n->topic, s) == 0) return 1;
    return 0;
}
static void mark_visited(const char *s) {
    uint32_t h   = topic_hash(s);
    VisitNode *n = malloc(sizeof(VisitNode));
    n->topic     = strdup(s);
    n->next      = visited[h];
    visited[h]   = n;
}
static void clear_visited(void) {
    for (int i = 0; i < VISITED_SIZE; i++) {
        VisitNode *n = visited[i];
        while (n) { VisitNode *t = n; n = n->next; free(t->topic); free(t); }
        visited[i] = NULL;
    }
}

void deep_crawl(const char *seed_topic, int seconds) {
    if (!seed_topic || !seed_topic[0]) return;
    if (seconds <= 0) seconds = 300;

    time_t start    = time(NULL);
    time_t deadline = start + seconds;

    int  pages_done  = 0;
    int  total_tokens = 0;

    CrawlQueue q;
    queue_init(&q);
    clear_visited();

    queue_push(&q, seed_topic);

    printf(BOLD CYAN
           "\nPRISM: Deep crawl starting — topic: '%s' — %d seconds\n"
           RESET, seed_topic, seconds);

    while (q.size > 0 && time(NULL) < deadline) {
        char *topic = queue_pop(&q);
        if (!topic) break;

        if (was_visited(topic)) { free(topic); continue; }
        mark_visited(topic);

        time_t remaining = deadline - time(NULL);
        printf(CYAN "  [%3d pages | %3ld s left] Fetching: %s\n" RESET,
               pages_done, remaining, topic);

        /* 1. Fetch and train the article text */
        char *text = fetch_wikipedia(topic);
        if (text) {
            int tok = train_from_string(text);
            total_tokens += tok;
            free(text);
        }

        /* 2. Fetch linked articles and queue them */
        if (time(NULL) < deadline) {
            size_t  link_count = 0;
            char  **links      = fetch_linked_titles(topic, &link_count);

            if (links) {
                for (size_t i = 0; i < link_count; i++) {
                    if (links[i] && !was_visited(links[i]))
                        queue_push(&q, links[i]);
                    free(links[i]);
                }
                free(links);
            }
        }

        pages_done++;
        free(topic);
    }

    /* Clean up */
    queue_free(&q);
    clear_visited();

    /* Persist everything */
    save_trie("lexical_trie.bin");

    printf(BOLD GREEN
           "\nPRISM: Crawl complete.\n"
           "  Pages trained : %d\n"
           "  Total tokens  : %d\n"
           "  Time taken    : %ld seconds\n"
           RESET,
           pages_done, total_tokens, time(NULL) - start);
}
