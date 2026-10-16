/*
 * Q4 - Emergency Medical Supply Routing
 * Undirected weighted graph (adjacency matrix, 0 = no road) and
 * Dijkstra's algorithm with a prev[] array to rebuild the actual route.
 */
#define _POSIX_C_SOURCE 200809L  /* for strcasecmp */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <strings.h>

#define MAX_LOC   15
#define NAME_LEN  40
#define INF       INT_MAX
#define MAX_KM    100000

typedef struct {
    char name[MAX_LOC][NAME_LEN];
    int  w[MAX_LOC][MAX_LOC];      /* road distance in km, 0 = no road */
    int  n;
} Graph;

/* last computed route (for "Display route distance") */
static int last_path[MAX_LOC], last_len = 0, last_dist = -1;

/* ---------- helpers ---------- */

static int read_line(const char *prompt, char *buf, size_t size) {
    printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) return 0;
    if (!strchr(buf, '\n')) { int c; while ((c = getchar()) != '\n' && c != EOF); }
    buf[strcspn(buf, "\r\n")] = '\0';
    char *s = buf;
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

/* Location by list number (1..n) or name (case-insensitive); -1 if missing. */
static int find_loc(const Graph *g, const char *key) {
    if (is_number(key)) {
        int i = atoi(key) - 1;
        return (i >= 0 && i < g->n) ? i : -1;
    }
    for (int i = 0; i < g->n; i++)
        if (strcasecmp(g->name[i], key) == 0) return i;
    return -1;
}

static int ask_loc(const Graph *g, const char *prompt) {
    char buf[NAME_LEN + 8];
    if (!read_line(prompt, buf, sizeof buf)) return -1;
    int i = find_loc(g, buf);
    if (i < 0) printf("  Location '%s' does not exist.\n", buf);
    return i;
}

/* Validates a distance: positive whole number. Returns -1 if invalid. */
static int parse_distance(const char *s) {
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0') { puts("  Invalid distance: enter a whole number."); return -1; }
    if (v <= 0)                   { puts("  Invalid distance: must be greater than 0."); return -1; }
    if (v > MAX_KM)               { printf("  Invalid distance: max %d km.\n", MAX_KM); return -1; }
    return (int)v;
}

/* ---------- graph ---------- */

static int add_location(Graph *g, const char *name) {
    if (!*name || is_number(name)) { puts("  Invalid name."); return 0; }
    if (strlen(name) >= NAME_LEN)  { puts("  Name too long."); return 0; }
    if (find_loc(g, name) >= 0)    { puts("  Location already exists."); return 0; }
    if (g->n == MAX_LOC)           { puts("  Network is full."); return 0; }
    strcpy(g->name[g->n], name);
    for (int i = 0; i <= g->n; i++) g->w[g->n][i] = g->w[i][g->n] = 0;
    g->n++;
    return 1;
}

static void add_road(Graph *g, const char *a, const char *b, int km) {
    int i = find_loc(g, a), j = find_loc(g, b);
    g->w[i][j] = g->w[j][i] = km;            /* undirected */
}

static void load_network(Graph *g) {
    const char *locs[] = {"Central Warehouse", "Distribution Center", "District Hospital",
                          "Health Center A", "Health Center B", "Health Center C"};
    g->n = 0;
    for (int i = 0; i < 6; i++) add_location(g, locs[i]);
    add_road(g, "Central Warehouse",   "Distribution Center", 8);
    add_road(g, "Central Warehouse",   "District Hospital",  14);
    add_road(g, "Distribution Center", "Health Center A",     5);
    add_road(g, "Distribution Center", "Health Center B",    11);
    add_road(g, "Health Center A",     "District Hospital",   7);
    add_road(g, "Health Center B",     "Health Center C",     3);
    add_road(g, "Health Center C",     "District Hospital",   6);
    add_road(g, "Health Center B",     "District Hospital",   9);
    add_road(g, "Health Center A",     "Health Center C",     4);
}

/* Dijkstra O(V^2): fills dist[] and prev[] from src. */
static void dijkstra(const Graph *g, int src, int dist[], int prev[]) {
    int done[MAX_LOC] = {0};
    for (int i = 0; i < g->n; i++) { dist[i] = INF; prev[i] = -1; }
    dist[src] = 0;

    for (int k = 0; k < g->n; k++) {
        int u = -1;                                  /* closest unvisited vertex */
        for (int i = 0; i < g->n; i++)
            if (!done[i] && dist[i] != INF && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0) break;                            /* rest is unreachable */
        done[u] = 1;
        for (int v = 0; v < g->n; v++)               /* relax edges of u */
            if (g->w[u][v] && !done[v] && dist[u] + g->w[u][v] < dist[v]) {
                dist[v] = dist[u] + g->w[u][v];
                prev[v] = u;
            }
    }
}

static void print_route(const Graph *g) {
    printf("  Shortest Route:\n  ");
    for (int i = 0; i < last_len; i++)
        printf("%s%s", g->name[last_path[i]], i < last_len - 1 ? " -> " : "\n");
    printf("  Legs: ");
    for (int i = 1; i < last_len; i++)
        printf("%d%s", g->w[last_path[i - 1]][last_path[i]], i < last_len - 1 ? " + " : "");
    printf(" km\n  Total Distance: %d km\n", last_dist);
}

static void find_route(const Graph *g) {
    int src = ask_loc(g, "  Start location: ");
    if (src < 0) return;
    int dst = ask_loc(g, "  Destination: ");
    if (dst < 0) return;
    if (src == dst) { puts("  Start and destination are the same - no travel needed (0 km)."); return; }

    int dist[MAX_LOC], prev[MAX_LOC];
    dijkstra(g, src, dist, prev);
    if (dist[dst] == INF) {
        printf("  No route exists from %s to %s.\n", g->name[src], g->name[dst]);
        return;
    }
    int rev[MAX_LOC], n = 0;                         /* walk prev[] back to src */
    for (int v = dst; v != -1; v = prev[v]) rev[n++] = v;
    for (int i = 0; i < n; i++) last_path[i] = rev[n - 1 - i];
    last_len = n;
    last_dist = dist[dst];
    print_route(g);
}

static void display(const Graph *g) {
    puts("  Locations:");
    for (int i = 0; i < g->n; i++) printf("  %2d. %s\n", i + 1, g->name[i]);
    puts("  Roads:");
    for (int i = 0; i < g->n; i++)
        for (int j = i + 1; j < g->n; j++)
            if (g->w[i][j]) printf("      %s --%d-- %s\n", g->name[i], g->w[i][j], g->name[j]);
}

static void menu_add_road(Graph *g) {
    char buf[32];
    int a = ask_loc(g, "  From location: ");
    if (a < 0) return;
    int b = ask_loc(g, "  To location: ");
    if (b < 0) return;
    if (a == b) { puts("  A road must connect two different locations."); return; }
    if (!read_line("  Distance (km): ", buf, sizeof buf)) return;
    int km = parse_distance(buf);
    if (km < 0) return;
    g->w[a][b] = g->w[b][a] = km;
    last_dist = -1;                                  /* old route may be outdated */
    printf("  Road %s --%d-- %s saved.\n", g->name[a], km, g->name[b]);
}

int main(void) {
    static Graph g;
    char line[64], name[NAME_LEN + 8];
    load_network(&g);

    for (;;) {
        puts("\n=== Emergency Medical Supply Routing ===\n"
             "  1. Display locations\n  2. Find shortest route\n  3. Display route distance\n"
             "  4. Add road\n  5. Add location\n  0. Exit");
        if (!read_line("Choice: ", line, sizeof line)) break;
        if      (!strcmp(line, "1")) display(&g);
        else if (!strcmp(line, "2")) find_route(&g);
        else if (!strcmp(line, "3")) {
            if (last_dist < 0) puts("  No route calculated yet - use option 2 first.");
            else print_route(&g);
        }
        else if (!strcmp(line, "4")) menu_add_road(&g);
        else if (!strcmp(line, "5")) {
            if (read_line("  New location name: ", name, sizeof name) && add_location(&g, name))
                printf("  Location '%s' added (no roads yet).\n", name);
        }
        else if (!strcmp(line, "0")) break;
        else puts("  Invalid choice.");
    }
    puts("  Goodbye.");
    return 0;
}
