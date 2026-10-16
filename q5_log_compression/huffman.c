/*
 * Q5 - Embedded Firmware Log Compression Utility (Huffman coding)
 *
 * .huff file format (one bit stream after a 8-byte header):
 *   "HUF1"            4-byte magic
 *   original size     4 bytes, little-endian
 *   tree              pre-order: 1 + 8 bits for a leaf (its byte), 0 for an internal node
 *   data              Huffman codes of every byte, last byte padded with 0s
 * Storing the tree shape (~10 bits per symbol) is much smaller than a
 * 256-entry frequency table, which matters for small log files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_FILE    "device_log.txt"
#define HUFF_FILE     "device_log.huff"
#define RESTORED_FILE "device_log_restored.txt"

typedef struct Node {
    unsigned char ch;
    unsigned long freq;
    struct Node *left, *right;
} Node;

/* ---------- tree nodes ---------- */

static Node *new_node(unsigned char ch, unsigned long freq, Node *l, Node *r) {
    Node *n = malloc(sizeof *n);
    if (!n) { fputs("Out of memory\n", stderr); exit(1); }
    n->ch = ch; n->freq = freq; n->left = l; n->right = r;
    return n;
}

static int is_leaf(const Node *n) { return !n->left && !n->right; }

static void free_tree(Node *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

/* ---------- min-heap (priority queue by frequency) ---------- */

typedef struct { Node *a[256]; int size; } Heap;

static void heap_push(Heap *h, Node *n) {
    int i = h->size++;
    while (i > 0 && h->a[(i - 1) / 2]->freq > n->freq) {   /* sift up */
        h->a[i] = h->a[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    h->a[i] = n;
}

static Node *heap_pop(Heap *h) {
    Node *top = h->a[0], *last = h->a[--h->size];
    int i = 0;
    for (;;) {                                               /* sift down */
        int c = 2 * i + 1;
        if (c >= h->size) break;
        if (c + 1 < h->size && h->a[c + 1]->freq < h->a[c]->freq) c++;
        if (last->freq <= h->a[c]->freq) break;
        h->a[i] = h->a[c];
        i = c;
    }
    h->a[i] = last;
    return top;
}

/* Builds the Huffman tree: repeatedly merge the two least frequent nodes. */
static Node *build_tree(const unsigned long freq[256]) {
    Heap h = {.size = 0};
    for (int c = 0; c < 256; c++)
        if (freq[c]) heap_push(&h, new_node((unsigned char)c, freq[c], NULL, NULL));
    if (h.size == 0) return NULL;
    while (h.size > 1) {
        Node *a = heap_pop(&h), *b = heap_pop(&h);
        heap_push(&h, new_node(0, a->freq + b->freq, a, b));
    }
    return heap_pop(&h);
}

/* Left edge = '0', right edge = '1'. A one-symbol tree gets code "0". */
static void make_codes(const Node *n, char codes[256][256], char *buf, int depth) {
    if (is_leaf(n)) {
        buf[depth] = '\0';
        strcpy(codes[n->ch], depth ? buf : "0");
        return;
    }
    buf[depth] = '0'; make_codes(n->left,  codes, buf, depth + 1);
    buf[depth] = '1'; make_codes(n->right, codes, buf, depth + 1);
}

/* ---------- bit I/O ---------- */

typedef struct { FILE *f; int buf, nbits; } BitIO;

static void put_bit(BitIO *b, int bit) {
    b->buf = (b->buf << 1) | bit;
    if (++b->nbits == 8) { fputc(b->buf, b->f); b->buf = b->nbits = 0; }
}

static void put_byte(BitIO *b, unsigned char c) {
    for (int i = 7; i >= 0; i--) put_bit(b, (c >> i) & 1);
}

static void flush_bits(BitIO *b) { while (b->nbits) put_bit(b, 0); }

/* Returns 0/1, or -1 at end of file. */
static int get_bit(BitIO *b) {
    if (b->nbits == 0) {
        if ((b->buf = fgetc(b->f)) == EOF) return -1;
        b->nbits = 8;
    }
    return (b->buf >> --b->nbits) & 1;
}

static void write_tree(BitIO *b, const Node *n) {
    if (is_leaf(n)) { put_bit(b, 1); put_byte(b, n->ch); return; }
    put_bit(b, 0);
    write_tree(b, n->left);
    write_tree(b, n->right);
}

/* Rebuilds the tree from its pre-order bits. *budget limits nodes (corrupt-file guard). */
static Node *read_tree(BitIO *b, int *budget, int *ok) {
    if (--*budget < 0) { *ok = 0; return NULL; }
    int bit = get_bit(b);
    if (bit < 0) { *ok = 0; return NULL; }
    if (bit == 1) {
        int c = 0;
        for (int i = 0; i < 8; i++) {
            int x = get_bit(b);
            if (x < 0) { *ok = 0; return NULL; }
            c = (c << 1) | x;
        }
        return new_node((unsigned char)c, 0, NULL, NULL);
    }
    Node *l = read_tree(b, budget, ok);
    Node *r = *ok ? read_tree(b, budget, ok) : NULL;
    if (!*ok) { free_tree(l); free_tree(r); return NULL; }
    return new_node(0, 0, l, r);
}

/* ---------- file helpers ---------- */

/* Reads a whole file into memory; returns NULL if it cannot be opened. */
static unsigned char *read_file(const char *path, long *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    unsigned char *data = malloc(*size ? (size_t)*size : 1);
    if (!data || fread(data, 1, (size_t)*size, f) != (size_t)*size) { free(data); fclose(f); return NULL; }
    fclose(f);
    return data;
}

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fclose(f);
    return s;
}

static void show_char(unsigned char c) {
    if      (c == '\n') printf("'\\n'  ");
    else if (c == '\r') printf("'\\r'  ");
    else if (c == '\t') printf("'\\t'  ");
    else if (c == ' ')  printf("' '   ");
    else if (c >= 33 && c < 127) printf("'%c'   ", c);
    else printf("0x%02X  ", c);
}

/* ---------- operations ---------- */

static void compress(void) {
    long size;
    unsigned char *data = read_file(INPUT_FILE, &size);
    if (!data) { printf("  Error: cannot read '%s'.\n", INPUT_FILE); return; }
    if (size > 0xFFFFFFFFL) { puts("  Error: file too large."); free(data); return; }

    unsigned long freq[256] = {0};
    for (long i = 0; i < size; i++) freq[data[i]]++;

    Node *root = build_tree(freq);
    static char codes[256][256];
    char buf[256];
    memset(codes, 0, sizeof codes);
    if (root) make_codes(root, codes, buf, 0);

    puts("  Char  Frequency  Code");
    for (int c = 0; c < 256; c++)
        if (freq[c]) { printf("  "); show_char((unsigned char)c); printf("%9lu  %s\n", freq[c], codes[c]); }

    FILE *out = fopen(HUFF_FILE, "wb");
    if (!out) { printf("  Error: cannot create '%s'.\n", HUFF_FILE); free(data); free_tree(root); return; }
    fwrite("HUF1", 1, 4, out);
    for (int i = 0; i < 4; i++) fputc((int)((unsigned long)size >> (8 * i)) & 0xFF, out);

    BitIO b = {out, 0, 0};
    if (root) {
        write_tree(&b, root);
        if (!is_leaf(root))                          /* one-symbol file needs no data bits */
            for (long i = 0; i < size; i++)
                for (const char *p = codes[data[i]]; *p; p++) put_bit(&b, *p == '1');
    }
    flush_bits(&b);
    fclose(out);
    free(data);
    free_tree(root);

    long csize = file_size(HUFF_FILE);
    printf("\n  Original size:     %ld bytes\n  Compressed size:   %ld bytes\n", size, csize);
    if (size > 0)
        printf("  Compression ratio: %.2f%% (space saved: %.2f%%)\n",
               100.0 * csize / size, 100.0 - 100.0 * csize / size);
    printf("  Saved to %s\n", HUFF_FILE);
}

static int verify(void) {
    long sa, sb;
    unsigned char *a = read_file(INPUT_FILE, &sa);
    unsigned char *b = read_file(RESTORED_FILE, &sb);
    int same = a && b && sa == sb && memcmp(a, b, (size_t)sa) == 0;
    if (!a)      printf("  Error: cannot read '%s'.\n", INPUT_FILE);
    else if (!b) printf("  Error: cannot read '%s' (decompress first).\n", RESTORED_FILE);
    else if (same) puts("  Verification: SUCCESS\n  The restored file is identical to the original.");
    else           puts("  Verification: FAILED\n  The restored file differs from the original.");
    free(a);
    free(b);
    return same;
}

static void decompress(void) {
    FILE *in = fopen(HUFF_FILE, "rb");
    if (!in) { printf("  Error: '%s' not found (compress first).\n", HUFF_FILE); return; }
    unsigned char hdr[8];
    if (fread(hdr, 1, 8, in) != 8 || memcmp(hdr, "HUF1", 4) != 0) {
        puts("  Error: not a valid .huff file."); fclose(in); return;
    }
    unsigned long size = 0;
    for (int i = 0; i < 4; i++) size |= (unsigned long)hdr[4 + i] << (8 * i);

    BitIO b = {in, 0, 0};
    int ok = 1, budget = 511;                       /* a 256-leaf tree has 511 nodes */
    Node *root = size ? read_tree(&b, &budget, &ok) : NULL;
    FILE *out = ok ? fopen(RESTORED_FILE, "wb") : NULL;
    if (!out) {
        puts(ok ? "  Error: cannot create output file." : "  Error: corrupt tree data.");
        free_tree(root); fclose(in); return;
    }

    for (unsigned long i = 0; i < size && ok; i++) {
        const Node *n = root;
        while (!is_leaf(n)) {                       /* walk down until a leaf */
            int bit = get_bit(&b);
            if (bit < 0) { ok = 0; break; }
            n = bit ? n->right : n->left;
        }
        if (ok) fputc(n->ch, out);
    }
    fclose(out);
    fclose(in);
    free_tree(root);
    if (!ok) { puts("  Error: compressed data ended early (corrupt file)."); return; }
    printf("  Decompressed %lu bytes to %s\n", size, RESTORED_FILE);
    verify();                                       /* automatic verification */
}

int main(void) {
    char line[32];
    for (;;) {
        puts("\n=== Firmware Log Compression (Huffman) ===\n"
             "  1. Compress log\n  2. Decompress log\n  3. Verify restored log\n  4. Exit");
        printf("Choice: ");
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';
        if      (!strcmp(line, "1")) compress();
        else if (!strcmp(line, "2")) decompress();
        else if (!strcmp(line, "3")) verify();
        else if (!strcmp(line, "4")) break;
        else puts("  Invalid choice. Enter 1-4.");
    }
    puts("  Goodbye.");
    return 0;
}
