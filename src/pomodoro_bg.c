#include "pomodoro_bg.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Grilla VT publica — screenPomodoro la dibuja
char pom_grid[POM_ROWS][POM_COLS];
int  pom_row = 0, pom_col = 0;

#ifndef _WIN32
// ── Solo Linux/macOS: forkpty, señales, descriptores ──────────────────────
#include <pty.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

static pid_t g_pid = -1;
static int   g_fd  = -1;

static void
vt_putchar(char c)
{
    if (c == '\r') { pom_col = 0; return; }
    if (c == '\n') {
        pom_row++;
        pom_col = 0;
        if (pom_row >= POM_ROWS) pom_row = POM_ROWS - 1;
        return;
    }
    if (c == '\b') { if (pom_col > 0) pom_col--; return; }
    if (pom_row < POM_ROWS && pom_col < POM_COLS)
        pom_grid[pom_row][pom_col++] = c;
}

static void
vt_process(const char *buf, int n)
{
    int i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)buf[i];
        if (c == '\033') {
            i++;
            if (i >= n) break;
            if (buf[i] == '[') {
                i++;
                char params[64] = ""; int pi = 0;
                while (i < n && pi < 63 &&
                       (buf[i] == ';' || buf[i] == '?' ||
                        (buf[i] >= '0' && buf[i] <= '9')))
                    params[pi++] = buf[i++];
                params[pi] = '\0';
                if (i >= n) break;
                char cmd = buf[i++];
                switch (cmd) {
                    case 'H': case 'f': {
                        int r=0, co=0; sscanf(params, "%d;%d", &r, &co);
                        pom_row = r>0?r-1:0; pom_col = co>0?co-1:0;
                        if (pom_row>=POM_ROWS) pom_row=POM_ROWS-1;
                        if (pom_col>=POM_COLS) pom_col=POM_COLS-1;
                        break;
                    }
                    case 'J': memset(pom_grid, ' ', sizeof(pom_grid)); break;
                    case 'K':
                        if (pom_row < POM_ROWS)
                            memset(&pom_grid[pom_row][pom_col], ' ', POM_COLS - pom_col);
                        break;
                    case 'A': { int d=params[0]?atoi(params):1; pom_row-=d; if(pom_row<0)pom_row=0; break; }
                    case 'B': { int d=params[0]?atoi(params):1; pom_row+=d; if(pom_row>=POM_ROWS)pom_row=POM_ROWS-1; break; }
                    case 'C': { int d=params[0]?atoi(params):1; pom_col+=d; if(pom_col>=POM_COLS)pom_col=POM_COLS-1; break; }
                    case 'D': { int d=params[0]?atoi(params):1; pom_col-=d; if(pom_col<0)pom_col=0; break; }
                    default: break;
                }
            } else if (buf[i] == ']') {
                i++;
                while (i < n) {
                    if ((unsigned char)buf[i] == 0x07) { i++; break; }
                    if (buf[i] == '\033' && i+1 < n && buf[i+1] == '\\') { i+=2; break; }
                    i++;
                }
            } else { i++; }
        } else { vt_putchar((char)c); i++; }
    }
}

void pom_init(void) {
    if (g_pid > 0) return;
    memset(pom_grid, ' ', sizeof(pom_grid));
    pom_row = 0; pom_col = 0;
    struct winsize ws = {POM_ROWS, POM_COLS, 0, 0};
    g_pid = forkpty(&g_fd, NULL, NULL, &ws);
    if (g_pid == 0) {
        setenv("TERM", "xterm", 1);
        execl("/bin/bash", "bash", "scripts/editorBim/pomodoro.sh", NULL);
        _exit(1);
    }
    if (g_pid < 0) { g_pid = -1; g_fd = -1; return; }
    fcntl(g_fd, F_SETFL, O_NONBLOCK);
}

void pom_cleanup(void) {
    if (g_pid > 0) { kill(g_pid, SIGTERM); waitpid(g_pid, NULL, 0); }
    if (g_fd  >= 0) close(g_fd);
    g_pid = -1; g_fd = -1;
}

void pom_send(const char *s, int n) {
    if (g_fd >= 0 && g_pid > 0) write(g_fd, s, n);
}

void pom_tick(void) {
    if (g_fd < 0) return;
    char buf[4096];
    int n = read(g_fd, buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; vt_process(buf, n); }
    if (waitpid(g_pid, NULL, WNOHANG) == g_pid) {
        close(g_fd); g_pid = -1; g_fd = -1;
    }
}

#else
// ── Windows: stubs vacios ─────────────────────────────────────────────────
void pom_init(void)    { /* no disponible en Windows */ }
void pom_cleanup(void) { }
void pom_send(const char *s, int n) { (void)s; (void)n; }
void pom_tick(void)    { }
#endif
