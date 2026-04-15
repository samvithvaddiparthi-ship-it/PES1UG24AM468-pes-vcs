// index.c — Staging area implementation
//
// Text format of .pes/index (one entry per line, sorted by path):
//
//   <mode-octal> <64-char-hex-hash> <mtime-seconds> <size> <path>
//
// Example:
//   100644 a1b2c3d4e5f6...  1699900000 42 README.md
//   100644 f7e8d9c0b1a2...  1699900100 128 src/main.c
//
// PROVIDED functions: index_find, index_remove, index_status
// TODO functions:     index_load, index_save, index_add

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED (unchanged) ───────────────────────────────────────────────────
// (keep your provided code exactly same here)


// ─── IMPLEMENTED ───────────────────────────────────────────────────────────

// Load index
int index_load(Index *index) {
    FILE *f = fopen(".pes/index", "r");
    if (!f) {
        index->count = 0;
        return 0; // empty index is fine
    }

    index->count = 0;

    char hex[HASH_HEX_SIZE + 1];

    while (fscanf(f, "%o %64s %ld %ld %s",
                  &index->entries[index->count].mode,
                  hex,
                  &index->entries[index->count].mtime_sec,
                  &index->entries[index->count].size,
                  index->entries[index->count].path) == 5) {

        hex_to_hash(hex, &index->entries[index->count].id);
        index->count++;
    }

    fclose(f);
    return 0;
}

// Save index (simple version, still acceptable)
int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");
    if (!f) return -1;

    char hex[HASH_HEX_SIZE + 1];

    for (int i = 0; i < index->count; i++) {
        hash_to_hex(&index->entries[i].id, hex);

        fprintf(f, "%o %s %ld %ld %s\n",
                index->entries[i].mode,
                hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    return rename(".pes/index.tmp", ".pes/index");
}

// Add file to index
int index_add(Index *index, const char *path) {

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    void *buffer = malloc(size);
    fread(buffer, 1, size, f);
    fclose(f);

    ObjectID id;
    if (object_write(OBJ_BLOB, buffer, size, &id) != 0) {
        free(buffer);
        return -1;
    }
    free(buffer);

    struct stat st;
    stat(path, &st);

    IndexEntry *e = index_find(index, path);

    if (!e) {
        e = &index->entries[index->count++];
    }

    e->mode = st.st_mode;
    e->id = id;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;
    snprintf(e->path, sizeof(e->path), "%s", path);

    return index_save(index);
}
