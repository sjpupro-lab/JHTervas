/*
 * fd.c — Phase-10: File Descriptor
 *
 * 프로세스별 fd 테이블. CanvasFS 슬롯 또는 Pipe에 매핑.
 * fd 0/1/2 = stdin/stdout/stderr (예약, 기본 터미널)
 */
#include "../include/canvasos_fd.h"
#include "../include/canvasos_proc.h"
#include "../include/canvasos_pipe.h"
#include <string.h>
#include <stdio.h>

/* 전역 fd 테이블 (프로세스별) */
static FileDesc g_fds[PROC8_MAX][FD_MAX_PER_PROC];

/* stdout 버퍼 (fd_write stdout용) */
static uint8_t g_stdout_buf[4096];
static uint16_t g_stdout_len = 0;

void fd_table_init(void) {
    memset(g_fds, 0, sizeof(g_fds));
    g_stdout_len = 0;
}

static FileDesc *fd_get(uint32_t pid, int fd) {
    if (pid >= PROC8_MAX || fd < 0 || fd >= FD_MAX_PER_PROC) return NULL;
    FileDesc *f = &g_fds[pid][fd];
    return f->active ? f : NULL;
}

static int fd_find_free(uint32_t pid) {
    if (pid >= PROC8_MAX) return -1;
    for (int i = 3; i < FD_MAX_PER_PROC; i++) /* 0,1,2 예약 */
        if (!g_fds[pid][i].active) return i;
    return -1;
}

int fd_open(void *ctx_v, uint32_t pid, const char *path, uint8_t flags) {
    (void)ctx_v; (void)path; /* path resolution은 Phase-10 path.c에서 */
    if (pid >= PROC8_MAX) return -1;
    int fd = fd_find_free(pid);
    if (fd < 0) return -1;

    FileDesc *f = &g_fds[pid][fd];
    memset(f, 0, sizeof(*f));
    f->flags  = flags;
    f->type   = FD_FILE;
    f->active = true;
    /* FsKey는 path_resolve 후 설정 — 여기선 슬롯 0 기본값 */
    return fd;
}

int fd_read(void *ctx_v, uint32_t pid, int fd, uint8_t *buf, uint16_t len) {
    (void)ctx_v;
    if (!buf || len == 0) return -1;

    /* stdin 특수 처리 */
    if (fd == FD_STDIN) {
        /* 터미널 입력 — fgets로 읽기 */
        if (!fgets((char *)buf, len, stdin)) return 0;
        return (int)strlen((char *)buf);
    }

    FileDesc *f = fd_get(pid, fd);
    if (!f) return -1;
    if (!(f->flags & O_READ)) return -1;

    if (f->type == FD_PIPE) {
        /* pipe_read는 PipeTable 필요 — 여기선 stub */
        return 0;
    }

    /* FD_FILE: Use CanvasFS bridge if available */
    extern int fd_file_read_slot(void*, FileDesc*, uint8_t*, uint16_t);
    return fd_file_read_slot(ctx_v, f, buf, len);
}

int fd_write(void *ctx_v, uint32_t pid, int fd, const uint8_t *data, uint16_t len) {
    (void)ctx_v;
    if (!data || len == 0) return -1;

    /* stdout 특수 처리 */
    if (fd == FD_STDOUT || fd == FD_STDERR) {
        for (uint16_t i = 0; i < len; i++) {
            putchar(data[i]);
            if (g_stdout_len < sizeof(g_stdout_buf))
                g_stdout_buf[g_stdout_len++] = data[i];
        }
        fflush(stdout);
        return len;
    }

    FileDesc *f = fd_get(pid, fd);
    if (!f) return -1;
    if (!(f->flags & O_WRITE)) return -1;

    if (f->type == FD_PIPE) {
        return 0; /* pipe_write 연동 */
    }

    /* FD_FILE: Use CanvasFS bridge if available */
    extern int fd_file_write_slot(void*, FileDesc*, const uint8_t*, uint16_t);
    return fd_file_write_slot(ctx_v, f, data, len);
}

int fd_close(void *ctx_v, uint32_t pid, int fd) {
    (void)ctx_v;
    if (fd < 3) return -1; /* stdin/stdout/stderr는 닫을 수 없음 */
    FileDesc *f = fd_get(pid, fd);
    if (!f) return -1;
    memset(f, 0, sizeof(*f));
    return 0;
}

int fd_seek(void *ctx_v, uint32_t pid, int fd, uint16_t offset) {
    (void)ctx_v;
    FileDesc *f = fd_get(pid, fd);
    if (!f) return -1;
    f->cursor = offset;
    return 0;
}

int fd_dup(void *ctx_v, uint32_t pid, int old_fd, int new_fd) {
    (void)ctx_v;
    if (pid >= PROC8_MAX) return -1;
    if (old_fd < 0 || old_fd >= FD_MAX_PER_PROC) return -1;
    if (new_fd < 0 || new_fd >= FD_MAX_PER_PROC) return -1;
    g_fds[pid][new_fd] = g_fds[pid][old_fd];
    return 0;
}

/* stdout 버퍼 접근 (테스트용) */
uint16_t fd_stdout_get(uint8_t *buf, uint16_t max) {
    uint16_t n = g_stdout_len < max ? g_stdout_len : max;
    if (buf && n > 0) memcpy(buf, g_stdout_buf, n);
    return n;
}

void fd_stdout_clear(void) { g_stdout_len = 0; }
