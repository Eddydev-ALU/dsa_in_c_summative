/*
 * Q2 - Network Device Command Registry
 * Approved commands are loaded from a file into a Binary Search Tree
 * (lexicographic order). Exact matches are approved; near-misses get a
 * "Did you mean" suggestion (Levenshtein distance <= 2); unknown attempts
 * are written to audit.log.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define MAX_COMMANDS  50
#define MAX_LEN       64
#define MAX_SUGGEST   2          /* max edit distance for a suggestion */
#define DEFAULT_FILE  "commands.txt"
#define AUDIT_FILE    "audit.log"
#define SESSION_FILE  "session.log"

typedef struct Node {
    char *cmd;                   /* dynamically allocated string */
    struct Node *left, *right;
} Node;

typedef struct {
    Node *root;
    int count;
} Registry;

/* session statistics, saved on quit */
static int stat_checked, stat_approved, stat_unknown;

/* ---------- BST ---------- */

/* Returns 1 inserted, 0 duplicate, -1 out of memory. */
static int bst_insert(Registry *r, const char *cmd) {
    Node **link = &r->root;
    while (*link) {
        int c = strcmp(cmd, (*link)->cmd);
        if (c == 0) return 0;
        link = c < 0 ? &(*link)->left : &(*link)->right;
    }
    Node *n = malloc(sizeof *n);
    char *copy = malloc(strlen(cmd) + 1);
    if (!n || !copy) { free(n); free(copy); return -1; }
    strcpy(copy, cmd);
    n->cmd = copy;
    n->left = n->right = NULL;
    *link = n;
    r->count++;
    return 1;
}

/* Iterative search; *depth receives the level where it was found (root = 0). */
static Node *bst_search(Node *n, const char *cmd, int *depth) {
    *depth = 0;
    while (n) {
        int c = strcmp(cmd, n->cmd);
        if (c == 0) return n;
        n = c < 0 ? n->left : n->right;
        (*depth)++;
    }
    return NULL;
}

static void bst_inorder(const Node *n, int *i) {
    if (!n) return;
    bst_inorder(n->left, i);
    printf("  %2d. %s\n", ++*i, n->cmd);
    bst_inorder(n->right, i);
}

static int bst_height(const Node *n) {
    if (!n) return 0;
    int l = bst_height(n->left), r = bst_height(n->right);
    return 1 + (l > r ? l : r);
}

/* Prints the tree sideways: right subtree on top, root on the left. */
static void bst_print_shape(const Node *n, int level) {
    if (!n) return;
    bst_print_shape(n->right, level + 1);
    printf("  %*s%s\n", level * 6, "", n->cmd);
    bst_print_shape(n->left, level + 1);
}

static void bst_free(Node *n) {             /* post-order: children first */
    if (!n) return;
    bst_free(n->left);
    bst_free(n->right);
    free(n->cmd);
    free(n);
}

static void registry_clear(Registry *r) {
    bst_free(r->root);
    r->root = NULL;
    r->count = 0;
}

/* ---------- similarity (Levenshtein edit distance) ---------- */

static int edit_distance(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    int prev[MAX_LEN + 1], cur[MAX_LEN + 1];
    for (int j = 0; j <= lb; j++) prev[j] = j;
    for (int i = 1; i <= la; i++) {
        cur[0] = i;
        for (int j = 1; j <= lb; j++) {
            int cost = a[i - 1] != b[j - 1];
            int best = prev[j - 1] + cost;                  /* substitute */
            if (prev[j] + 1 < best) best = prev[j] + 1;     /* delete */
            if (cur[j - 1] + 1 < best) best = cur[j - 1] + 1; /* insert */
            cur[j] = best;
        }
        memcpy(prev, cur, sizeof(int) * (lb + 1));
    }
    return prev[lb];
}

/* Visits every node and keeps the closest command. */
static void find_closest(const Node *n, const char *input, const char **best, int *best_d) {
    if (!n) return;
    int d = edit_distance(input, n->cmd);
    if (d < *best_d) { *best_d = d; *best = n->cmd; }
    find_closest(n->left, input, best, best_d);
    find_closest(n->right, input, best, best_d);
}

/* ---------- helpers ---------- */

/* Trims spaces and converts to upper case in place. */
static void normalize(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
    for (; *s; s++) *s = (char)toupper((unsigned char)*s);
}

/* Valid commands contain only A-Z, 0-9 and '_'. */
static int is_valid_command(const char *s) {
    if (!*s || strlen(s) >= MAX_LEN) return 0;
    for (; *s; s++)
        if (!isupper((unsigned char)*s) && !isdigit((unsigned char)*s) && *s != '_') return 0;
    return 1;
}

static int read_line(const char *prompt, char *buf, size_t size) {
    printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) return 0;
    if (!strchr(buf, '\n')) { int c; while ((c = getchar()) != '\n' && c != EOF); }
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static void timestamp(char *out, size_t size) {
    time_t now = time(NULL);
    strftime(out, size, "%Y-%m-%d %H:%M:%S", localtime(&now));
}

/* ---------- features ---------- */

static void load_commands(Registry *r, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { printf("  Error: cannot open '%s'. Registry is empty.\n", path); return; }
    char line[256];
    int dup = 0, bad = 0, skipped = 0;
    while (fgets(line, sizeof line, f)) {
        normalize(line);
        if (line[0] == '\0') continue;                       /* blank line */
        if (!is_valid_command(line)) { bad++; continue; }
        if (r->count >= MAX_COMMANDS) { skipped++; continue; }
        int res = bst_insert(r, line);
        if (res == 0) dup++;
        else if (res < 0) { puts("  Error: out of memory."); break; }
    }
    fclose(f);
    printf("  Loaded %d command(s) from '%s'.\n", r->count, path);
    if (dup)     printf("  Skipped %d duplicate(s).\n", dup);
    if (bad)     printf("  Skipped %d invalid line(s).\n", bad);
    if (skipped) printf("  Limit of %d reached: %d command(s) not loaded.\n", MAX_COMMANDS, skipped);
    if (r->count == 0) puts("  Warning: command file is empty - nothing to verify against.");
}

static void audit_unknown(const char *cmd) {
    FILE *f = fopen(AUDIT_FILE, "a");
    if (!f) { puts("  Warning: could not write audit.log."); return; }
    char ts[20];
    timestamp(ts, sizeof ts);
    fprintf(f, "%s | UNKNOWN | %s\n", ts, cmd);
    fclose(f);
}

static void verify(const Registry *r) {
    char input[256];
    if (!read_line("  Enter command: ", input, sizeof input)) return;
    normalize(input);
    if (!is_valid_command(input)) {
        puts("  Invalid input: use letters, digits and '_' only (max 63 chars).");
        return;
    }
    stat_checked++;
    int depth;
    if (bst_search(r->root, input, &depth)) {
        stat_approved++;
        printf("  COMMAND APPROVED (found at tree level %d)\n", depth);
        return;
    }
    stat_unknown++;
    const char *best = NULL;
    int best_d = MAX_SUGGEST + 1;
    find_closest(r->root, input, &best, &best_d);
    if (best && best_d < (int)strlen(input))
        printf("  UNKNOWN COMMAND\n  Did you mean: %s?\n", best);
    else
        puts(r->root ? "  UNKNOWN COMMAND" : "  UNKNOWN COMMAND (registry is empty)");
    audit_unknown(input);
}

static void save_session(const Registry *r) {
    FILE *f = fopen(SESSION_FILE, "a");
    if (!f) { puts("  Warning: could not save session."); return; }
    char ts[20];
    timestamp(ts, sizeof ts);
    fprintf(f, "%s | commands=%d | checked=%d | approved=%d | unknown=%d\n",
            ts, r->count, stat_checked, stat_approved, stat_unknown);
    fclose(f);
    printf("  Session summary saved to %s.\n", SESSION_FILE);
}

static void help(void) {
    puts("\nCommands: v=verify  p=print (A-Z)  s=stats  t=show tree\n"
         "          l=reload file  x=clear registry  q=save & quit");
}

int main(int argc, char *argv[]) {
    const char *path = argc > 1 ? argv[1] : DEFAULT_FILE;
    Registry reg = {NULL, 0};
    char line[64];

    puts("=== Network Device Command Registry (BST) ===");
    load_commands(&reg, path);
    help();

    while (read_line("\n> ", line, sizeof line)) {
        char cmd = (line[0] && !line[1]) ? line[0] : '?';
        int i = 0;
        switch (cmd) {
        case 'v': verify(&reg); break;
        case 'p':
            if (!reg.root) puts("  Registry is empty.");
            else bst_inorder(reg.root, &i);
            break;
        case 's':
            printf("  Stored commands: %d/%d   Tree height: %d\n",
                   reg.count, MAX_COMMANDS, bst_height(reg.root));
            break;
        case 't':
            if (!reg.root) puts("  Registry is empty.");
            else { puts("  (rotated 90 deg: root on the left, right child above)"); bst_print_shape(reg.root, 0); }
            break;
        case 'l': registry_clear(&reg); load_commands(&reg, path); break;
        case 'x': registry_clear(&reg); puts("  Registry cleared, all nodes freed."); break;
        case 'h': help(); break;
        case 'q': goto quit;
        default:  puts("  Unknown option. Type h for help.");
        }
    }
quit:
    save_session(&reg);
    registry_clear(&reg);
    puts("  Memory released. Goodbye.");
    return 0;
}
