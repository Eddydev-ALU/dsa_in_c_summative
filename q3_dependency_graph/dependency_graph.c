/*
 * Q3 - Server Dependency Network Analyzer
 * Directed graph stored as an adjacency matrix: adj[a][b] = 1 means
 * server a depends on (talks to) server b. Because the matrix IS the graph,
 * it is always up to date after every add/remove.
 */
#define _POSIX_C_SOURCE 200809L  /* for strcasecmp */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#define MAX_SERVERS 20
#define NAME_LEN    40

typedef struct {
    char name[MAX_SERVERS][NAME_LEN];
    int adj[MAX_SERVERS][MAX_SERVERS];
    int n;                                   /* number of servers */
} Graph;

/* ---------- helpers ---------- */

static int read_line(const char *prompt, char *buf, size_t size) {
    printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) return 0;
    if (!strchr(buf, '\n')) { int c; while ((c = getchar()) != '\n' && c != EOF); }
    buf[strcspn(buf, "\r\n")] = '\0';
    char *s = buf;                           /* trim spaces */
    while (isspace((unsigned char)*s)) s++;
    memmove(buf, s, strlen(s) + 1);
    size_t len = strlen(buf);
    while (len && isspace((unsigned char)buf[len - 1])) buf[--len] = '\0';
    return 1;
}

static int is_number(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

/* Finds a server by list number (1..n) or by name (case-insensitive). */
static int find_server(const Graph *g, const char *key) {
    if (is_number(key)) {
        int i = atoi(key) - 1;
        return (i >= 0 && i < g->n) ? i : -1;
    }
    for (int i = 0; i < g->n; i++)
        if (strcasecmp(g->name[i], key) == 0) return i;
    return -1;
}

/* Prompts for a server; prints a message and returns -1 if not found. */
static int ask_server(const Graph *g, const char *prompt) {
    char buf[NAME_LEN + 8];
    if (g->n == 0) { puts("  The graph is empty."); return -1; }
    if (!read_line(prompt, buf, sizeof buf)) return -1;
    int i = find_server(g, buf);
    if (i < 0) printf("  Server '%s' does not exist.\n", buf);
    return i;
}

/* ---------- graph operations ---------- */

static int add_server(Graph *g, const char *name) {
    if (!*name)                     { puts("  Name cannot be empty."); return 0; }
    if (strlen(name) >= NAME_LEN)   { printf("  Name too long (max %d chars).\n", NAME_LEN - 1); return 0; }
    if (is_number(name))            { puts("  Name cannot be only digits."); return 0; }
    if (find_server(g, name) >= 0)  { printf("  Server '%s' already exists.\n", name); return 0; }
    if (g->n == MAX_SERVERS)        { printf("  Graph is full (max %d servers).\n", MAX_SERVERS); return 0; }
    strcpy(g->name[g->n], name);
    for (int i = 0; i <= g->n; i++) g->adj[g->n][i] = g->adj[i][g->n] = 0;
    g->n++;
    return 1;
}

/* Removes vertex v: deleting its row and column removes every connected edge. */
static void remove_server(Graph *g, int v) {
    for (int i = v; i < g->n - 1; i++) strcpy(g->name[i], g->name[i + 1]);
    for (int i = 0; i < g->n; i++)                       /* shift columns left */
        for (int j = v; j < g->n - 1; j++) g->adj[i][j] = g->adj[i][j + 1];
    for (int i = v; i < g->n - 1; i++)                   /* shift rows up */
        for (int j = 0; j < g->n - 1; j++) g->adj[i][j] = g->adj[i + 1][j];
    g->n--;
}

static int add_dependency(Graph *g, int from, int to) {
    if (from == to)       { puts("  A server cannot depend on itself."); return 0; }
    if (g->adj[from][to]) { puts("  That dependency already exists."); return 0; }
    g->adj[from][to] = 1;
    return 1;
}

static void query(const Graph *g, int v) {
    int found = 0;
    printf("\n  Server: %s\n  Direct dependencies:\n", g->name[v]);
    for (int j = 0; j < g->n; j++)                       /* row v = outgoing */
        if (g->adj[v][j]) { printf("    - %s\n", g->name[j]); found = 1; }
    if (!found) puts("    (none)");

    found = 0;
    printf("  Services depending on %s:\n", g->name[v]);
    for (int i = 0; i < g->n; i++)                       /* column v = incoming */
        if (g->adj[i][v]) { printf("    - %s\n", g->name[i]); found = 1; }
    if (!found) puts("    (none)");
}

static void list_servers(const Graph *g) {
    if (!g->n) { puts("  The graph is empty."); return; }
    for (int i = 0; i < g->n; i++) printf("  %2d. %s\n", i + 1, g->name[i]);
}

static void show_matrix(const Graph *g) {
    if (!g->n) { puts("  The graph is empty."); return; }
    puts("  Adjacency matrix (row depends on column):");
    printf("      ");
    for (int j = 0; j < g->n; j++) printf("%3d", j + 1);
    putchar('\n');
    for (int i = 0; i < g->n; i++) {
        printf("  %2d |", i + 1);
        for (int j = 0; j < g->n; j++) printf("%3d", g->adj[i][j]);
        printf("   %s\n", g->name[i]);
    }
}

static void add_edge_by_name(Graph *g, const char *a, const char *b) {
    g->adj[find_server(g, a)][find_server(g, b)] = 1;
}

static void load_initial(Graph *g) {
    const char *servers[] = {"Web Server", "API Server", "Authentication Server",
                             "Database Server", "Cache Server", "Backup Server"};
    g->n = 0;
    for (int i = 0; i < 6; i++) add_server(g, servers[i]);
    add_edge_by_name(g, "Web Server", "API Server");
    add_edge_by_name(g, "Web Server", "Authentication Server");
    add_edge_by_name(g, "API Server", "Database Server");
    add_edge_by_name(g, "API Server", "Cache Server");
    add_edge_by_name(g, "Authentication Server", "Database Server");
    add_edge_by_name(g, "Cache Server", "Database Server");
    add_edge_by_name(g, "Database Server", "Backup Server");
}

static void help(void) {
    puts("\nCommands: a=add server  r=remove server  c=create dependency\n"
         "          d=delete dependency  q=query server  m=matrix  l=list  x=exit\n"
         "  (servers can be entered by name or by list number)");
}

int main(void) {
    static Graph g;                          /* zero-initialised */
    char line[64], name[NAME_LEN + 8];
    int a, b;

    load_initial(&g);
    puts("=== Server Dependency Network Analyzer ===");
    list_servers(&g);
    help();

    while (read_line("\n> ", line, sizeof line)) {
        char cmd = (line[0] && !line[1]) ? line[0] : '?';
        switch (cmd) {
        case 'a':
            if (read_line("  New server name: ", name, sizeof name) && add_server(&g, name))
                printf("  Server '%s' added.\n", name);
            break;
        case 'r':
            if ((a = ask_server(&g, "  Server to remove: ")) >= 0) {
                printf("  Server '%s' and all its dependencies removed.\n", g.name[a]);
                remove_server(&g, a);
            }
            break;
        case 'c':
            if ((a = ask_server(&g, "  Server (depends on...): ")) < 0) break;
            if ((b = ask_server(&g, "  ...this server: ")) < 0) break;
            if (add_dependency(&g, a, b))
                printf("  Dependency %s -> %s created.\n", g.name[a], g.name[b]);
            break;
        case 'd':
            if ((a = ask_server(&g, "  From server: ")) < 0) break;
            if ((b = ask_server(&g, "  To server: ")) < 0) break;
            if (!g.adj[a][b]) puts("  That dependency does not exist.");
            else { g.adj[a][b] = 0; printf("  Dependency %s -> %s removed.\n", g.name[a], g.name[b]); }
            break;
        case 'q':
            if ((a = ask_server(&g, "  Server to query: ")) >= 0) query(&g, a);
            break;
        case 'm': show_matrix(&g); break;
        case 'l': list_servers(&g); break;
        case 'h': help(); break;
        case 'x': puts("  Goodbye."); return 0;
        default:  puts("  Unknown command. Type h for help.");
        }
    }
    return 0;
}
