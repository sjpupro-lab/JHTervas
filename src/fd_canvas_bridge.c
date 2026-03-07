/*
 * fd_canvas_bridge.c — Phase-10: FD ↔ CanvasFS Bridge
 *
 * Connects the file descriptor abstraction to CanvasFS slot I/O.
 * Key concepts:
 *   1. path_resolve → FsKey (gate_id + slot)
 *   2. FsKey maps to a CanvasFS slot (TINY/SMALL/LARGE)
 *   3. fd->cursor tracks position within slot payload
 *   4. fs_read/fs_write operate on the canvas tile data
 *
 * This file provides the _bridge_ functions called from fd.c
 * when a FileDesc has type == FD_FILE and a valid FsKey.
 */
#include "../include/canvasos_fd.h"
#include "../include/canvasos_path.h"
#include "../include/canvasfs.h"
#include <string.h>
#include <stdio.h>

/* ── CanvasFS context (set during system boot) ───────── */
static CanvasFS *g_bridge_fs = NULL;

void fd_bridge_init(CanvasFS *fs) {
    g_bridge_fs = fs;
}

/* ── Bind an FsKey to a FileDesc ─────────────────────── */
int fd_file_bind(FileDesc *fd, FsKey key, uint8_t flags) {
    if (!fd) return -1;
    memset(fd, 0, sizeof(*fd));
    fd->key    = key;
    fd->flags  = flags;
    fd->type   = FD_FILE;
    fd->active = true;
    fd->cursor = 0;
    return 0;
}

/* ── Read from CanvasFS slot via bridge ──────────────── */
int fd_file_read_slot(void *ctx_v, FileDesc *fd, uint8_t *buf, uint16_t len) {
    (void)ctx_v;
    if (!fd || !buf || len == 0) return -1;
    if (!g_bridge_fs) return 0; /* No FS mounted — return EOF */

    /* Read full slot payload into temp buffer */
    uint8_t payload[1024];
    size_t actual = 0;
    FsResult rc = fs_read(g_bridge_fs, fd->key, payload, sizeof(payload), &actual);
    if (rc != FS_OK || actual == 0) return 0; /* EOF or error */

    /* Apply cursor offset */
    if (fd->cursor >= (uint16_t)actual) return 0; /* past EOF */

    uint16_t avail = (uint16_t)(actual - fd->cursor);
    uint16_t to_read = len < avail ? len : avail;
    memcpy(buf, payload + fd->cursor, to_read);
    fd->cursor += to_read;

    return (int)to_read;
}

/* ── Write to CanvasFS slot via bridge ───────────────── */
int fd_file_write_slot(void *ctx_v, FileDesc *fd, const uint8_t *buf, uint16_t len) {
    (void)ctx_v;
    if (!fd || !buf || len == 0) return -1;
    if (!g_bridge_fs) return -1; /* No FS mounted */

    /* For O_APPEND, we need current content length first */
    if (fd->flags & O_APPEND) {
        FsSlotClass cls;
        size_t cur_len = 0;
        FsResult sr = fs_stat(g_bridge_fs, fd->key, &cls, &cur_len);
        if (sr == FS_OK) fd->cursor = (uint16_t)cur_len;
    }

    /* Read existing payload, merge with new data, write back.
     * CanvasFS uses whole-slot writes, so we need read-modify-write. */
    uint8_t payload[1024];
    memset(payload, 0, sizeof(payload));
    size_t existing = 0;
    fs_read(g_bridge_fs, fd->key, payload, sizeof(payload), &existing);

    /* Insert/overwrite at cursor position */
    uint16_t write_end = fd->cursor + len;
    if (write_end > sizeof(payload)) write_end = (uint16_t)sizeof(payload);
    uint16_t actual_write = write_end - fd->cursor;
    memcpy(payload + fd->cursor, buf, actual_write);

    /* Total length is max(existing, write_end) */
    size_t total = existing > write_end ? existing : write_end;

    /* Write back to CanvasFS */
    FsResult rc = fs_write(g_bridge_fs, fd->key, payload, total);
    if (rc != FS_OK) return -1;

    fd->cursor = write_end;
    return (int)actual_write;
}

/* ── Open file with path resolution + CanvasFS binding ── */
int fd_open_with_path(EngineContext *ctx, PathContext *pc,
                      uint32_t pid, const char *path, uint8_t flags) {
    if (!ctx || !pc || !path) return -1;

    FsKey key;
    int rc = path_resolve(ctx, pc, path, &key);

    if (rc != 0 && (flags & O_CREATE)) {
        /* File not found but O_CREATE: create in cwd */
        const char *basename = strrchr(path, '/');
        basename = basename ? basename + 1 : path;
        rc = path_mkdir(ctx, pc, basename); /* create entry */
        if (rc == 0)
            rc = path_resolve(ctx, pc, path, &key);
    }

    if (rc != 0) return -1;

    /* Allocate fd */
    int fd = fd_open(ctx, pid, path, flags);
    if (fd < 0) return -1;

    return fd;
}

/* ── Stat a path and return file size ────────────────── */
int fd_bridge_stat(const char *path, PathContext *pc,
                   EngineContext *ctx, size_t *out_len) {
    if (!path || !pc || !ctx || !out_len) return -1;
    if (!g_bridge_fs) return -1;

    FsKey key;
    if (path_resolve(ctx, pc, path, &key) != 0) return -1;

    FsSlotClass cls;
    size_t len = 0;
    FsResult rc = fs_stat(g_bridge_fs, key, &cls, &len);
    if (rc != FS_OK) return -1;

    *out_len = len;
    return 0;
}
