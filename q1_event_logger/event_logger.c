/*
 * Q1 - Embedded Device Event Logger
 * Keeps the 25 most recent events in a dynamically allocated doubly linked list.
 * head = oldest, tail = newest, current = event the operator is viewing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_EVENTS   25
#define SESSION_FILE "event_session.txt"

static const char *EVENT_TYPES[] = {"TEMPERATURE_WARNING", "LOW_BATTERY",
                                    "SENSOR_FAILURE", "COMMUNICATION_ERROR"};
static const char *SEVERITIES[]  = {"INFO", "WARNING", "CRITICAL"};

typedef struct Event {
    int id;
    int type;              /* index into EVENT_TYPES */
    char sensor[24];       /* sensor / device identifier */
    time_t timestamp;
    int severity;          /* index into SEVERITIES */
    struct Event *prev, *next;
} Event;

typedef struct {
    Event *head, *tail, *current;
    int count, next_id;
} Logger;

/* ---------- list operations ---------- */

static void logger_init(Logger *log) {
    log->head = log->tail = log->current = NULL;
    log->count = 0;
    log->next_id = 1;
}

/* Unlink and free the oldest event (head). */
static void remove_oldest(Logger *log) {
    Event *old = log->head;
    if (!old) return;
    log->head = old->next;
    if (log->head) log->head->prev = NULL;
    else           log->tail = NULL;              /* list became empty */
    if (log->current == old) log->current = log->head;
    printf("  [capacity] Oldest event #%d removed and freed.\n", old->id);
    free(old);
    log->count--;
}

/* Append a new event at the tail; evict the oldest first if full. */
static Event *add_event(Logger *log, int type, const char *sensor, int severity) {
    Event *e = malloc(sizeof *e);
    if (!e) { puts("  Error: out of memory."); return NULL; }

    if (log->count == MAX_EVENTS) remove_oldest(log);

    e->id = log->next_id++;
    e->type = type;
    snprintf(e->sensor, sizeof e->sensor, "%s", sensor);
    e->timestamp = time(NULL);
    e->severity = severity;
    e->next = NULL;
    e->prev = log->tail;

    if (log->tail) log->tail->next = e;
    else           log->head = e;                 /* first event */
    log->tail = e;
    if (!log->current) log->current = e;          /* current starts at first event */
    log->count++;
    return e;
}

static void clear_all(Logger *log) {
    Event *e = log->head;
    while (e) {
        Event *next = e->next;
        free(e);
        e = next;
    }
    log->head = log->tail = log->current = NULL;   /* no dangling pointers */
    log->count = 0;
}

/* ---------- display ---------- */

static void print_event(const Event *e) {
    char ts[20];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&e->timestamp));
    printf("  #%-3d %s | %-19s | %-10s | %s\n",
           e->id, ts, EVENT_TYPES[e->type], e->sensor, SEVERITIES[e->severity]);
}

static void list_all(const Logger *log) {
    if (!log->head) { puts("  Log is empty."); return; }
    printf("  %d/%d events (oldest -> newest), '>' = current:\n", log->count, MAX_EVENTS);
    for (const Event *e = log->head; e; e = e->next) {
        putchar(e == log->current ? '>' : ' ');
        print_event(e);
    }
}

static void save_session(const Logger *log) {
    FILE *f = fopen(SESSION_FILE, "w");
    if (!f) { puts("  Warning: could not save session."); return; }
    for (const Event *e = log->head; e; e = e->next) {
        char ts[20];
        strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&e->timestamp));
        fprintf(f, "%d,%s,%s,%s,%s\n", e->id, ts, EVENT_TYPES[e->type],
                e->sensor, SEVERITIES[e->severity]);
    }
    fclose(f);
    printf("  Session saved to %s (%d events).\n", SESSION_FILE, log->count);
}

/* ---------- input helpers ---------- */

/* Reads one line; returns 0 on EOF. Strips the newline. */
static int read_line(const char *prompt, char *buf, size_t size) {
    if (prompt) printf("%s", prompt);
    if (!fgets(buf, (int)size, stdin)) return 0;
    if (!strchr(buf, '\n')) { int c; while ((c = getchar()) != '\n' && c != EOF); }
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* Reads an integer in [min,max]; re-prompts on invalid input. Returns 0 on EOF. */
static int read_int(const char *prompt, int min, int max, int *out) {
    char buf[32], *end;
    while (read_line(prompt, buf, sizeof buf)) {
        long v = strtol(buf, &end, 10);
        if (end != buf && *end == '\0' && v >= min && v <= max) { *out = (int)v; return 1; }
        printf("  Invalid input: enter a number from %d to %d.\n", min, max);
    }
    return 0;
}

static void manual_add(Logger *log) {
    int type, sev;
    char sensor[24];
    puts("  Type: 1) Temperature warning 2) Low battery 3) Sensor failure 4) Communication error");
    if (!read_int("  Choose type: ", 1, 4, &type)) return;
    for (;;) {
        if (!read_line("  Sensor/device ID: ", sensor, sizeof sensor)) return;
        if (sensor[0] != '\0') break;
        puts("  Sensor ID cannot be empty.");
    }
    if (!read_int("  Severity 1) INFO 2) WARNING 3) CRITICAL: ", 1, 3, &sev)) return;
    Event *e = add_event(log, type - 1, sensor, sev - 1);
    if (e) { puts("  Event added:"); print_event(e); }
}

/* Simulates the device generating n random events (handy to test the 25 limit). */
static void simulate(Logger *log) {
    int n;
    if (!read_int("  How many events to generate (1-100)? ", 1, 100, &n)) return;
    for (int i = 0; i < n; i++) {
        char sensor[24];
        snprintf(sensor, sizeof sensor, "SENSOR_%02d", rand() % 12 + 1);
        add_event(log, rand() % 4, sensor, rand() % 3);
    }
    printf("  %d events generated. Log now holds %d/%d.\n", n, log->count, MAX_EVENTS);
}

static void help(void) {
    puts("\nCommands: n=next(newer)  p=previous(older)  a=add  c=current  l=list all\n"
         "          s=simulate events  x=clear all  q=save & quit");
}

int main(void) {
    Logger log;
    char line[64];
    logger_init(&log);
    srand((unsigned)time(NULL));
    puts("=== Embedded Device Event Logger (max 25 events) ===");
    help();

    while (read_line("\n> ", line, sizeof line)) {
        char cmd = line[0];
        if (line[0] == '\0' || line[1] != '\0') cmd = '?';   /* exactly one character */
        switch (cmd) {
        case 'n':
            if (!log.current)            puts("  Log is empty.");
            else if (!log.current->next) puts("  Already at the newest event.");
            else { log.current = log.current->next; print_event(log.current); }
            break;
        case 'p':
            if (!log.current)            puts("  Log is empty.");
            else if (!log.current->prev) puts("  Already at the oldest event.");
            else { log.current = log.current->prev; print_event(log.current); }
            break;
        case 'a': manual_add(&log); break;
        case 'c':
            if (log.current) print_event(log.current); else puts("  Log is empty.");
            break;
        case 'l': list_all(&log); break;
        case 's': simulate(&log); break;
        case 'x': clear_all(&log); puts("  All events cleared and memory released."); break;
        case 'h': help(); break;
        case 'q': goto quit;
        default:  puts("  Unknown command. Type h for help.");
        }
    }
quit:
    save_session(&log);
    clear_all(&log);
    puts("  Memory released. Goodbye.");
    return 0;
}
