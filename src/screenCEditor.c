#define _XOPEN_SOURCE 600
#include "screenCEditor.h"
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pty.h>

/* ── Dimensiones del buffer de terminal ── */
#define CE_ROWS      36    /* buffer del editor (vim) */
#define CE_OUT_ROWS 200    /* buffer del output (clang + run) */
#define CE_COLS     120
#define CE_ROWBUF   256

/* ── Tabs internos del panel ── */
#define CE_MAX_TABS  20
#define CE_MAX_FILES 64
#define CE_LIST_VIS  12

/* ── Modos del panel ── */
#define MODE_PICKER  0   /* file picker inicial            */
#define MODE_EDITOR  1   /* vim corriendo en PTY           */
#define MODE_OUTPUT  2   /* salida de clang / ejecucion    */

/* ── Resultados de hit-test en la tab bar interna ── */
#define CE_HIT_NONE -1
#define CE_HIT_PLUS -2

/* ════════════════════════════════════════════════════════════════════════════
 *  ESTRUCTURAS
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char   filepath[512];
    char   name[64];

    int    master_fd;
    pid_t  child_pid;

    char   screen[CE_ROWS][CE_ROWBUF];
    int    cur_row;
    int    cur_col;
    int    ansi_state;
    char   ansi_buf[64];
    int    ansi_len;

    int    is_main;     /* 1 = este archivo es el punto de entrada */
    int    vivo;        /* 1 = vim sigue corriendo                 */
} CTab;

typedef struct {
    int mode;

    /* Tabs internos */
    CTab tabs[CE_MAX_TABS];
    int  n_tabs;
    int  tab_activo;

    /* PTY de output (clang + ejecucion) */
    int   out_fd;
    pid_t out_pid;
    char  out_screen[CE_OUT_ROWS][CE_ROWBUF];
    int   out_row;
    int   out_col;
    int   out_ansi_state;
    char  out_ansi_buf[64];
    int   out_ansi_len;
    int   out_scroll;    /* primera fila visible en el output */

    /* File picker */
    char archivos[CE_MAX_FILES][64];
    int  n_archivos;
    int  sel;
    int  scroll;

    /* Crear nuevo archivo */
    int  creando_nuevo;
    char nuevo_buf[64];
    int  nuevo_len;
    int  skip_next_text;

    /* Confirmar borrado */
    int confirm_borrar;

    char project_root[512];
} CEditorState;

/* ════════════════════════════════════════════════════════════════════════════
 *  HELPERS DE DIBUJO
 * ════════════════════════════════════════════════════════════════════════════ */

static void
ce_text(SDL_Renderer *r, TTF_Font *f,
        const char *txt, int x, int y, SDL_Color c)
{
    if (!txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(f, txt, c);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(r, t, NULL, &dst);
    SDL_FreeSurface(s);
    SDL_DestroyTexture(t);
}

static void
ce_fill(SDL_Renderer *r, SDL_Rect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

static void
ce_hline(SDL_Renderer *r, int x, int y, int w, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(r, x, y, x + w - 1, y);
}

/* ── Paleta UI ── */
#define CE_BG       (SDL_Color){ 12,  12,  12, 255}
#define CE_SEP      (SDL_Color){ 36,  36,  36, 255}
#define CE_TEXT     (SDL_Color){130, 130, 130, 255}
#define CE_BRIGHT   (SDL_Color){212, 212, 212, 255}
#define CE_ACCENT   (SDL_Color){  0, 200,  80, 255}
#define CE_WARN     (SDL_Color){200,  80,  80, 255}
#define CE_MAIN_COL (SDL_Color){ 80, 160, 255, 255}
#define CE_TAB_ACT  (SDL_Color){ 22,  22,  22, 255}

/* ── Paleta Visual Studio Community ── */
#define CHL_DEFAULT (SDL_Color){212, 212, 212, 255}  /* #D4D4D4  texto normal    */
#define CHL_KEYWORD (SDL_Color){ 86, 156, 214, 255}  /* #569CD6  keywords/types  */
#define CHL_STRING  (SDL_Color){206, 145, 120, 255}  /* #CE9178  strings/chars   */
#define CHL_COMMENT (SDL_Color){106, 153,  85, 255}  /* #6A9955  comentarios     */
#define CHL_NUMBER  (SDL_Color){181, 206, 168, 255}  /* #B5CEA8  numeros         */
#define CHL_PREPROC (SDL_Color){197, 134, 192, 255}  /* #C586C0  #include/#define*/
#define CHL_FUNC    (SDL_Color){220, 220, 170, 255}  /* #DCDCAA  funciones       */
#define CHL_TYPE    (SDL_Color){ 78, 201, 176, 255}  /* #4EC9B0  tipos usuario   */

/* ════════════════════════════════════════════════════════════════════════════
 *  SYNTAX HIGHLIGHTER — Visual Studio Community palette
 * ════════════════════════════════════════════════════════════════════════════ */

static int
hl_is_word_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static int
hl_is_keyword(const char *s, int len)
{
    static const char *kw[] = {
        /* control */
        "if","else","for","while","do","switch","case","default","break",
        "continue","return","goto",
        /* tipos primitivos */
        "int","char","float","double","long","short","unsigned","signed",
        "void","const","volatile","static","extern","register","inline",
        "auto","restrict",
        /* tipos compuestos */
        "struct","union","enum","typedef",
        /* C11 */
        "sizeof","_Bool","_Complex","_Alignas","_Alignof","_Atomic",
        "_Noreturn","_Static_assert","_Thread_local","_Generic",
        /* tipos stdlib comunes */
        "size_t","ssize_t","ptrdiff_t","intptr_t","uintptr_t",
        "uint8_t","uint16_t","uint32_t","uint64_t",
        "int8_t","int16_t","int32_t","int64_t",
        "bool","true","false","NULL","FILE","pid_t",
        NULL
    };
    for (int i = 0; kw[i]; i++)
        if ((int)strlen(kw[i]) == len && strncmp(kw[i], s, len) == 0)
            return 1;
    return 0;
}

/*
 * ce_draw_row_hl — renderiza una fila con syntax highlighting.
 * Devuelve el x final (util para posicionar el cursor).
 */
static void
ce_draw_row_hl(SDL_Renderer *r, TTF_Font *f, const char *row, int x, int y)
{
    if (!row || !row[0]) return;

    int   len   = (int)strlen(row);
    char  span[CE_ROWBUF];
    int   slen  = 0;
    int   sx    = x;

    /* Estado del parser */
    int in_string   = 0;
    int in_char_lit = 0;
    int in_comment  = 0;  /* // hasta fin de linea */

    /* Detectar linea de preprocesador */
    int i = 0;
    while (i < len && row[i] == ' ') i++;
    int is_preproc = (i < len && row[i] == '#');
    i = 0;

    SDL_Color cur_color = CHL_DEFAULT;

    /* Vuelca el span acumulado y lo renderiza */
#define FLUSH_SPAN() do { \
    if (slen > 0) { \
        span[slen] = '\0'; \
        SDL_Surface *ss = TTF_RenderUTF8_Blended(f, span, cur_color); \
        if (ss) { \
            SDL_Texture *tt = SDL_CreateTextureFromSurface(r, ss); \
            SDL_Rect dr = {sx, y, ss->w, ss->h}; \
            SDL_RenderCopy(r, tt, NULL, &dr); \
            sx += ss->w; \
            SDL_FreeSurface(ss); SDL_DestroyTexture(tt); \
        } \
        slen = 0; \
    } \
} while(0)

    while (i < len) {
        SDL_Color new_color;

        /* ── comentario de linea ── */
        if (in_comment) {
            new_color = CHL_COMMENT;

        /* ── dentro de string ── */
        } else if (in_string) {
            new_color = CHL_STRING;
            if (row[i] == '"' && (i == 0 || row[i-1] != '\\')) in_string = 0;

        /* ── dentro de char literal ── */
        } else if (in_char_lit) {
            new_color = CHL_STRING;
            if (row[i] == '\'' && (i == 0 || row[i-1] != '\\')) in_char_lit = 0;

        /* ── inicio de comentario ── */
        } else if (row[i] == '/' && i + 1 < len && row[i+1] == '/') {
            in_comment = 1;
            new_color  = CHL_COMMENT;

        /* ── inicio de string ── */
        } else if (row[i] == '"') {
            in_string = 1;
            new_color = CHL_STRING;

        /* ── inicio de char literal ── */
        } else if (row[i] == '\'') {
            in_char_lit = 1;
            new_color   = CHL_STRING;

        /* ── preprocesador ── */
        } else if (is_preproc) {
            new_color = CHL_PREPROC;

        /* ── numero ── */
        } else if ((row[i] >= '0' && row[i] <= '9') &&
                   (i == 0 || !hl_is_word_char(row[i-1]))) {
            new_color = CHL_NUMBER;
            /* consumir el numero entero en el span */
            if (new_color.r != cur_color.r || new_color.g != cur_color.g ||
                new_color.b != cur_color.b) {
                FLUSH_SPAN();
                cur_color = new_color;
            }
            while (i < len && (hl_is_word_char(row[i]) || row[i] == '.'))
                span[slen++] = row[i++];
            continue;

        /* ── identificador (keyword, funcion o normal) ── */
        } else if (hl_is_word_char(row[i]) && (i == 0 || !hl_is_word_char(row[i-1]))) {
            /* medir largo de la palabra */
            int wstart = i;
            while (i < len && hl_is_word_char(row[i])) i++;
            int wlen = i - wstart;

            /* elegir color */
            if (hl_is_keyword(row + wstart, wlen)) {
                new_color = CHL_KEYWORD;
            } else {
                /* funcion si va seguida de ( */
                int j = i;
                while (j < len && row[j] == ' ') j++;
                new_color = (j < len && row[j] == '(') ? CHL_FUNC : CHL_DEFAULT;
            }

            if (new_color.r != cur_color.r || new_color.g != cur_color.g ||
                new_color.b != cur_color.b) {
                FLUSH_SPAN();
                cur_color = new_color;
            }
            for (int k = wstart; k < i; k++) span[slen++] = row[k];
            continue;

        } else {
            new_color = CHL_DEFAULT;
        }

        /* si cambia el color, flush y actualizar */
        if (new_color.r != cur_color.r || new_color.g != cur_color.g ||
            new_color.b != cur_color.b) {
            FLUSH_SPAN();
            cur_color = new_color;
        }
        span[slen++] = row[i++];
    }

    FLUSH_SPAN();
#undef FLUSH_SPAN
}

/* ════════════════════════════════════════════════════════════════════════════
 *  ANSI PARSER  (independiente, uno por PTY)
 * ════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char (*screen)[CE_ROWBUF];
    int  *cur_row;
    int  *cur_col;
    int  *state;
    char *buf;
    int  *len;
} AnsiCtx;

static void
ansi_exec(AnsiCtx *a, char cmd)
{
    int p[4] = {0};
    int np   = 0;
    char tmp[64];
    strncpy(tmp, a->buf, 63);
    char *tok = strtok(tmp, ";");
    while (tok && np < 4) { p[np++] = atoi(tok); tok = strtok(NULL, ";"); }

    int row = *a->cur_row;
    int col = *a->cur_col;

    switch (cmd) {
    case 'J':
        if (p[0] == 0) {
            /* ESC[0J — borrar desde cursor hasta fin de pantalla */
            if (row < CE_ROWS) {
                int len = (int)strlen(a->screen[row]);
                if (col < len) memset(a->screen[row] + col, 0, len - col);
                for (int i = row + 1; i < CE_ROWS; i++) memset(a->screen[i], 0, CE_ROWBUF);
            }
        } else if (p[0] == 1) {
            /* ESC[1J — borrar desde inicio hasta cursor */
            for (int i = 0; i < row; i++) memset(a->screen[i], 0, CE_ROWBUF);
            if (row < CE_ROWS) memset(a->screen[row], ' ', col < CE_ROWBUF ? col : CE_ROWBUF - 1);
        } else {
            /* ESC[2J / ESC[3J — borrar pantalla completa */
            for (int i = 0; i < CE_ROWS; i++) memset(a->screen[i], 0, CE_ROWBUF);
            *a->cur_row = *a->cur_col = 0;
        }
        break;
    case 'H': case 'f':
        *a->cur_row = p[0] > 0 ? p[0] - 1 : 0;
        *a->cur_col = p[1] > 0 ? p[1] - 1 : 0;
        if (*a->cur_row >= CE_ROWS) *a->cur_row = CE_ROWS - 1;
        if (*a->cur_col >= CE_COLS) *a->cur_col = CE_COLS - 1;
        break;
    case 'A': *a->cur_row -= p[0] ? p[0] : 1; if (*a->cur_row < 0)         *a->cur_row = 0;           break;
    case 'B': *a->cur_row += p[0] ? p[0] : 1; if (*a->cur_row >= CE_ROWS)  *a->cur_row = CE_ROWS - 1; break;
    case 'C': *a->cur_col += p[0] ? p[0] : 1; if (*a->cur_col >= CE_COLS)  *a->cur_col = CE_COLS - 1; break;
    case 'D': *a->cur_col -= p[0] ? p[0] : 1; if (*a->cur_col < 0)         *a->cur_col = 0;           break;
    case 'G':
        /* ESC[nG — mover cursor a columna n en la fila actual */
        *a->cur_col = p[0] > 0 ? p[0] - 1 : 0;
        if (*a->cur_col >= CE_COLS) *a->cur_col = CE_COLS - 1;
        break;
    case 'K':
        if (row >= CE_ROWS) break;
        if (p[0] == 0) {
            /* ESC[0K — borrar desde cursor hasta fin de linea */
            int len = (int)strlen(a->screen[row]);
            if (col < len) memset(a->screen[row] + col, 0, len - col);
        } else if (p[0] == 1) {
            /* ESC[1K — borrar desde inicio hasta cursor */
            int n = col < CE_ROWBUF - 1 ? col : CE_ROWBUF - 2;
            memset(a->screen[row], ' ', n);
        } else {
            /* ESC[2K — borrar linea entera */
            memset(a->screen[row], 0, CE_ROWBUF);
        }
        break;
    case 'P': {
        /* ESC[nP — borrar n chars desde cursor (shift izquierda) */
        if (row >= CE_ROWS) break;
        int n   = p[0] ? p[0] : 1;
        int len = (int)strlen(a->screen[row]);
        if (col < len) {
            int src = col + n;
            if (src > len) src = len;
            memmove(a->screen[row] + col, a->screen[row] + src, len - src);
            memset(a->screen[row] + len - (src - col), 0, src - col);
        }
        break;
    }
    case '@': {
        /* ESC[n@ — insertar n blancos desde cursor (shift derecha) */
        if (row >= CE_ROWS) break;
        int n   = p[0] ? p[0] : 1;
        int len = (int)strlen(a->screen[row]);
        int end = len + n < CE_ROWBUF - 1 ? len + n : CE_ROWBUF - 2;
        memmove(a->screen[row] + col + n, a->screen[row] + col, end - col - n);
        memset(a->screen[row] + col, ' ', n);
        a->screen[row][end] = '\0';
        break;
    }
    case 'L': {
        /* ESC[nL — insertar n lineas en cursor (scroll down) */
        int n = p[0] ? p[0] : 1;
        for (int i = CE_ROWS - 1; i >= row + n; i--)
            memcpy(a->screen[i], a->screen[i - n], CE_ROWBUF);
        for (int i = row; i < row + n && i < CE_ROWS; i++)
            memset(a->screen[i], 0, CE_ROWBUF);
        break;
    }
    case 'M': {
        /* ESC[nM — borrar n lineas en cursor (scroll up) */
        int n = p[0] ? p[0] : 1;
        for (int i = row; i + n < CE_ROWS; i++)
            memcpy(a->screen[i], a->screen[i + n], CE_ROWBUF);
        for (int i = CE_ROWS - n; i < CE_ROWS; i++)
            memset(a->screen[i], 0, CE_ROWBUF);
        break;
    }
    /* todo lo demas (SGR colores, modos de terminal, etc.) se ignora */
    }
}

static void
ansi_feed(AnsiCtx *a, unsigned char c)
{
    switch (*a->state) {
    case 0:
        if      (c == 0x1b) { *a->state = 1; }
        else if (c == '\r') { *a->cur_col = 0; }
        else if (c == '\n') { if (++*a->cur_row >= CE_ROWS) *a->cur_row = CE_ROWS - 1; }
        else if (c == 0x08) {
            /* backspace: retroceder cursor */
            if (*a->cur_col > 0) (*a->cur_col)--;
        }
        else if (c == 0x07) { /* BEL: ignorar */ }
        else if ((c >= 0x20 && c < 0x7f) || c >= 0x80) {
            if (*a->cur_row >= CE_ROWS) break;
            char *row = a->screen[*a->cur_row];
            int   len = (int)strlen(row);
            if (*a->cur_col < len) {
                row[*a->cur_col] = (char)c;
            } else {
                while (len < *a->cur_col && len < CE_ROWBUF - 2) row[len++] = ' ';
                if (len < CE_ROWBUF - 1) { row[len] = (char)c; row[len + 1] = '\0'; }
            }
            if (++*a->cur_col >= CE_COLS) *a->cur_col = CE_COLS - 1;
        }
        break;
    case 1:
        if (c == '[') {
            *a->state = 2;
            *a->len   = 0;
            memset(a->buf, 0, 64);
        } else if (c == ']') {
            *a->state = 3;   /* OSC — consumir hasta BEL o ST */
        } else {
            *a->state = 0;
        }
        break;
    case 2:
        if      (c >= 0x30 && c <= 0x3f) { if (*a->len < 63) a->buf[(*a->len)++] = (char)c; }
        else if (c >= 0x20 && c <= 0x2f) { /* intermedio: ignorar */ }
        else if (c >= 0x40 && c <= 0x7e) { a->buf[*a->len] = '\0'; ansi_exec(a, (char)c); *a->state = 0; }
        else                              { *a->state = 0; }
        break;
    case 3:
        /* Consumir secuencia OSC (titulo de ventana, etc.) hasta BEL o ESC\ */
        if (c == 0x07 || c == 0x9c) *a->state = 0;
        else if (c == 0x1b)         *a->state = 4;
        break;
    case 4:
        /* ESC dentro de OSC — si es '\' termina OSC, si no reiniciar */
        *a->state = (c == '\\') ? 0 : 3;
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  FILE PICKER — helpers
 * ════════════════════════════════════════════════════════════════════════════ */

static int
cmp_str(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static void
scan_c_files(CEditorState *s)
{
    s->n_archivos = 0;
    s->sel        = -1;
    s->scroll     = 0;

    char path[600];
    snprintf(path, sizeof(path), "%s/saves/c", s->project_root);

    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) && s->n_archivos < CE_MAX_FILES) {
        int len = (int)strlen(ent->d_name);
        if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0)
            strncpy(s->archivos[s->n_archivos++], ent->d_name, 63);
    }
    closedir(d);

    qsort(s->archivos, s->n_archivos, sizeof(s->archivos[0]), cmp_str);
    if (s->n_archivos > 0) s->sel = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  LANZAR PTY — vim sobre un .c
 * ════════════════════════════════════════════════════════════════════════════ */

static void
lanzar_vim(CEditorState *s, int idx)
{
    CTab *t = &s->tabs[idx];

    struct winsize ws = {0};
    ws.ws_row = CE_ROWS;
    ws.ws_col = CE_COLS;

    pid_t pid = forkpty(&t->master_fd, NULL, NULL, &ws);
    if (pid < 0) { t->master_fd = -1; return; }

    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("LANG", "en_US.UTF-8",    1);
        execlp("vim", "vim",
               "-c", "set noswapfile insertmode",
               t->filepath, (char *)NULL);
        _exit(1);
    }

    t->child_pid  = pid;
    t->vivo       = 1;
    t->cur_row    = t->cur_col   = 0;
    t->ansi_state = t->ansi_len  = 0;
    for (int i = 0; i < CE_ROWS; i++) memset(t->screen[i], 0, CE_ROWBUF);
    fcntl(t->master_fd, F_SETFL, O_NONBLOCK);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  ABRIR ARCHIVO — agrega un tab interno o activa uno existente
 * ════════════════════════════════════════════════════════════════════════════ */

static void
abrir_archivo(CEditorState *s, const char *filename)
{
    char filepath[600];
    snprintf(filepath, sizeof(filepath), "%s/saves/c/%s",
             s->project_root, filename);

    /* si ya esta abierto, solo activarlo */
    for (int i = 0; i < s->n_tabs; i++) {
        if (strcmp(s->tabs[i].filepath, filepath) == 0) {
            s->tab_activo = i;
            s->mode       = MODE_EDITOR;
            return;
        }
    }

    if (s->n_tabs >= CE_MAX_TABS) return;

    CTab *t = &s->tabs[s->n_tabs];
    memset(t, 0, sizeof(*t));
    strncpy(t->filepath, filepath, 511);
    strncpy(t->name,     filename,  63);
    t->master_fd = -1;
    t->child_pid = -1;

    /* Si es el primer tab Y se llama main.c, marcarlo como main automaticamente */
    if (s->n_tabs == 0 && strcmp(filename, "main.c") == 0)
        t->is_main = 1;

    lanzar_vim(s, s->n_tabs);

    s->tab_activo = s->n_tabs;
    s->n_tabs++;
    s->mode = MODE_EDITOR;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  CERRAR TAB INTERNO
 * ════════════════════════════════════════════════════════════════════════════ */

static void
cerrar_tab_interno(CEditorState *s, int idx)
{
    if (idx < 0 || idx >= s->n_tabs) return;
    CTab *t = &s->tabs[idx];

    if (t->child_pid > 0) { kill(t->child_pid, SIGTERM); waitpid(t->child_pid, NULL, 0); }
    if (t->master_fd >= 0) close(t->master_fd);

    for (int i = idx; i < s->n_tabs - 1; i++)
        s->tabs[i] = s->tabs[i + 1];
    s->n_tabs--;

    if (s->tab_activo >= s->n_tabs)
        s->tab_activo = s->n_tabs - 1;
    if (s->tab_activo < 0)
        s->tab_activo = 0;

    if (s->n_tabs == 0) {
        s->mode = MODE_PICKER;
        scan_c_files(s);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  COMPILAR Y EJECUTAR  (clang → binario → run)
 * ════════════════════════════════════════════════════════════════════════════ */

static void
lanzar_output(CEditorState *s)
{
    /* Buscar el main */
    const char *main_path = NULL;
    for (int i = 0; i < s->n_tabs; i++) {
        if (s->tabs[i].is_main) { main_path = s->tabs[i].filepath; break; }
    }
    /* Si ningun tab tiene is_main, usar el activo */
    if (!main_path && s->n_tabs > 0)
        main_path = s->tabs[s->tab_activo].filepath;
    if (!main_path) return;

    /* Limpiar output anterior */
    for (int i = 0; i < CE_OUT_ROWS; i++) memset(s->out_screen[i], 0, CE_ROWBUF);
    s->out_row = s->out_col = 0;
    s->out_ansi_state = s->out_ansi_len = 0;
    s->out_scroll = 0;

    if (s->out_pid > 0) { kill(s->out_pid, SIGTERM); waitpid(s->out_pid, NULL, 0); }
    if (s->out_fd >= 0) close(s->out_fd);
    s->out_fd  = -1;
    s->out_pid = -1;

    /* Script inline: compila con diagnosticos completos y si ok ejecuta */
    char script[1024];
    snprintf(script, sizeof(script),
        "cd '%s/saves/c' && "
        "clang -Wall -Wextra -Wpedantic "
        "-fdiagnostics-color=never "
        "-fdiagnostics-show-option "
        "-fno-elide-type "
        "-o /tmp/ce_out '%s' 2>&1 "
        "&& echo '' "
        "&& echo '>>> Compilacion exitosa. Ejecutando...' "
        "&& echo '' "
        "&& /tmp/ce_out "
        "; echo '' "
        "&& echo \">>> Fin (codigo: $?)\"",
        s->project_root, main_path);

    struct winsize ws = {CE_OUT_ROWS, CE_COLS, 0, 0};
    pid_t pid = forkpty(&s->out_fd, NULL, NULL, &ws);
    if (pid < 0) { s->out_fd = -1; return; }
    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        execlp("/bin/bash", "bash", "-c", script, (char *)NULL);
        _exit(1);
    }

    s->out_pid = pid;
    fcntl(s->out_fd, F_SETFL, O_NONBLOCK);
    s->mode = MODE_OUTPUT;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  INTERFAZ DEL PANEL — init
 * ════════════════════════════════════════════════════════════════════════════ */

void
ceditor_init(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    CEditorState *s = calloc(1, sizeof(*s));
    s->mode      = MODE_PICKER;
    s->out_fd    = -1;
    s->out_pid   = -1;
    s->tab_activo = 0;

    if (!getcwd(s->project_root, sizeof(s->project_root)))
        strcpy(s->project_root, ".");

    /* Asegurarse que saves/c/ existe */
    char dir[600];
    snprintf(dir, sizeof(dir), "%s/saves/c", s->project_root);
    mkdir(dir, 0755);

    scan_c_files(s);
    tab->state = s;
}

/* forward declaration — definida en la seccion de dibujo mas abajo */
static int ce_tabbar_hit(CEditorState *s, TTF_Font *f, SDL_Rect area,
                         int mx, int my, int *es_close);

/* ════════════════════════════════════════════════════════════════════════════
 *  INTERFAZ DEL PANEL — handle_event
 * ════════════════════════════════════════════════════════════════════════════ */

void
ceditor_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    CEditorState *s = (CEditorState *)tab->state;
    if (!s) return;

    /* ══ OUTPUT: scroll, ESC vuelve, F5 relanza ══ */
    if (s->mode == MODE_OUTPUT) {
        if (e->type == SDL_KEYDOWN) {
            switch (e->key.keysym.sym) {
            case SDLK_ESCAPE:
                s->mode = (s->n_tabs > 0) ? MODE_EDITOR : MODE_PICKER;
                return;
            case SDLK_F5: lanzar_output(s); return;
            case SDLK_UP:
                if (s->out_scroll > 0) s->out_scroll--;
                return;
            case SDLK_DOWN:
                if (s->out_scroll < s->out_row) s->out_scroll++;
                return;
            case SDLK_PAGEUP:
                s->out_scroll -= 10;
                if (s->out_scroll < 0) s->out_scroll = 0;
                return;
            case SDLK_PAGEDOWN:
                s->out_scroll += 10;
                if (s->out_scroll > s->out_row) s->out_scroll = s->out_row;
                return;
            default: break;
            }
            /* F5 relanza output */
            if (e->key.keysym.sym == SDLK_F5) lanzar_output(s);
            /* input al programa corriendo */
            if (s->out_fd >= 0) {
                const char *seq = NULL; char ch = 0;
                switch (e->key.keysym.sym) {
                case SDLK_RETURN: ch = '\r'; break;
                case SDLK_BACKSPACE: ch = '\x7f'; break;
                default: break;
                }
                if (ch)  write(s->out_fd, &ch, 1);
                if (seq) write(s->out_fd, seq, strlen(seq));
            }
        }
        if (e->type == SDL_TEXTINPUT && s->out_fd >= 0)
            write(s->out_fd, e->text.text, strlen(e->text.text));
        return;
    }

    /* ══ EDITOR: pasar input al tab activo (vim) ══ */
    if (s->mode == MODE_EDITOR && s->n_tabs > 0) {
        CTab *t = &s->tabs[s->tab_activo];

        /* ── Mouse: clicks en la tab bar interna ── */
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            SDL_Rect area = {
                ctx->sidebar_w, TAB_BAR_H,
                ctx->ancho - ctx->sidebar_w, ctx->alto - TAB_BAR_H
            };
            int es_close = 0;
            int hit = ce_tabbar_hit(s, ctx->fuente, area,
                                    e->button.x, e->button.y, &es_close);
            if (hit == CE_HIT_PLUS) {
                /* + → volver al picker para abrir/crear otro archivo */
                s->mode = MODE_PICKER;
                scan_c_files(s);
                return;
            }
            if (hit >= 0 && hit < s->n_tabs) {
                if (es_close) {
                    /* × → mandar :wq a ese vim (se cerrara en el draw) */
                    CTab *ct = &s->tabs[hit];
                    if (ct->master_fd >= 0) write(ct->master_fd, "\x0f:wq\r", 5);
                } else {
                    s->tab_activo = hit;
                }
                return;
            }
        }

        /* ── Teclado: atajos del panel ── */
        if (e->type == SDL_KEYDOWN) {
            SDL_Keymod mod = SDL_GetModState();

            if (e->key.keysym.sym == SDLK_F5) {
                lanzar_output(s);
                return;
            }
            if ((mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_w) {
                if (t->master_fd >= 0) write(t->master_fd, "\x0f:wq\r", 5);
                return;
            }
            if ((mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_s) {
                if (t->master_fd >= 0) write(t->master_fd, "\x0f:wa\r", 5);
                return;
            }
            if ((mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_n) {
                s->mode           = MODE_PICKER;
                s->creando_nuevo  = 1;
                s->skip_next_text = 1;
                memset(s->nuevo_buf, 0, sizeof(s->nuevo_buf));
                s->nuevo_len = 0;
                SDL_StartTextInput();
                return;
            }
            if ((mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_m) {
                for (int i = 0; i < s->n_tabs; i++) s->tabs[i].is_main = 0;
                t->is_main = 1;
                return;
            }
            if ((mod & KMOD_CTRL) && e->key.keysym.sym == SDLK_TAB) {
                if (mod & KMOD_SHIFT)
                    s->tab_activo = (s->tab_activo - 1 + s->n_tabs) % s->n_tabs;
                else
                    s->tab_activo = (s->tab_activo + 1) % s->n_tabs;
                return;
            }
        }

        if (!t->vivo || t->master_fd < 0) return;

        if (e->type == SDL_TEXTINPUT)
            write(t->master_fd, e->text.text, strlen(e->text.text));
        else if (e->type == SDL_KEYDOWN) {
            const char *seq = NULL; char ch = 0;
            switch (e->key.keysym.sym) {
            case SDLK_RETURN:    case SDLK_KP_ENTER: ch  = '\r';     break;
            case SDLK_BACKSPACE:                      ch  = '\x7f';   break;
            case SDLK_ESCAPE:                         return;         /* no salir de insert mode */
            case SDLK_UP:                             seq = "\x1b[A"; break;
            case SDLK_DOWN:                           seq = "\x1b[B"; break;
            case SDLK_RIGHT:                          seq = "\x1b[C"; break;
            case SDLK_LEFT:                           seq = "\x1b[D"; break;
            case SDLK_HOME:                           seq = "\x1b[H"; break;
            case SDLK_END:                            seq = "\x1b[F"; break;
            case SDLK_TAB:                            ch  = '\t';     break;
            default: break;
            }
            if (ch)  write(t->master_fd, &ch, 1);
            if (seq) write(t->master_fd, seq, strlen(seq));
        }
        return;
    }

    /* ══ PICKER ══ */
    if (s->creando_nuevo) {
        if (e->type == SDL_TEXTINPUT) {
            if (s->skip_next_text) { s->skip_next_text = 0; return; }
            int len = (int)strlen(e->text.text);
            if (s->nuevo_len + len < 60) {
                memcpy(s->nuevo_buf + s->nuevo_len, e->text.text, len);
                s->nuevo_len               += len;
                s->nuevo_buf[s->nuevo_len]  = '\0';
            }
        } else if (e->type == SDL_KEYDOWN) {
            SDL_Keycode k = e->key.keysym.sym;
            if (k == SDLK_BACKSPACE && s->nuevo_len > 0)
                s->nuevo_buf[--s->nuevo_len] = '\0';
            else if (k == SDLK_ESCAPE) {
                s->creando_nuevo = 0;
                memset(s->nuevo_buf, 0, sizeof(s->nuevo_buf));
                s->nuevo_len = 0;
            } else if (k == SDLK_RETURN && s->nuevo_len > 0) {
                char filename[80], filepath[600];
                snprintf(filename, sizeof(filename), "%s.c", s->nuevo_buf);
                snprintf(filepath, sizeof(filepath), "%s/saves/c/%s",
                         s->project_root, filename);
                FILE *f = fopen(filepath, "a"); if (f) fclose(f);
                s->creando_nuevo = 0;
                memset(s->nuevo_buf, 0, sizeof(s->nuevo_buf));
                s->nuevo_len = 0;
                abrir_archivo(s, filename);
            }
        }
        return;
    }

    if (e->type != SDL_KEYDOWN) return;
    SDL_Keycode k = e->key.keysym.sym;

    if (s->confirm_borrar) {
        if (k == SDLK_RETURN && s->sel >= 0) {
            char fp[600];
            snprintf(fp, sizeof(fp), "%s/saves/c/%s", s->project_root, s->archivos[s->sel]);
            remove(fp);
            scan_c_files(s);
        }
        s->confirm_borrar = 0;
        return;
    }

    if (s->n_archivos == 0) {
        if (k == SDLK_n) { s->creando_nuevo = 1; s->skip_next_text = 1; SDL_StartTextInput(); }
        return;
    }

    switch (k) {
    case SDLK_UP:
        if (s->sel > 0) { s->sel--; if (s->sel < s->scroll) s->scroll = s->sel; }
        break;
    case SDLK_DOWN:
        if (s->sel < s->n_archivos - 1) {
            s->sel++;
            if (s->sel >= s->scroll + CE_LIST_VIS) s->scroll = s->sel - CE_LIST_VIS + 1;
        }
        break;
    case SDLK_RETURN:
        if (s->sel >= 0) abrir_archivo(s, s->archivos[s->sel]);
        break;
    case SDLK_d:
        if (s->sel >= 0) s->confirm_borrar = 1;
        break;
    case SDLK_n:
        s->creando_nuevo  = 1;
        s->skip_next_text = 1;
        SDL_StartTextInput();
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  DIBUJAR TAB BAR INTERNO
 * ════════════════════════════════════════════════════════════════════════════ */

#define CE_TABBAR_H 24

/*
 * ce_tab_width — ancho de un tab dado su nombre.
 * Mismo calculo usado en draw y en el hit-test.
 */
static int
ce_tab_width(TTF_Font *f, const char *name)
{
    int tw;
    TTF_SizeUTF8(f, name, &tw, NULL);
    return 8 + tw + 6 + 12 + 8;   /* pad_x + text + gap + close_w + pad_x */
}

/*
 * ce_tabbar_hit — detecta que elemento se clicó en la tab bar interna.
 *
 * Retorna:
 *   >= 0        → indice del tab clicado
 *   CE_HIT_PLUS → click en el botón +
 *   CE_HIT_NONE → click fuera de la tab bar
 *
 * *es_close = 1 si se clicó la × del tab.
 */
#define CE_HIT_NONE -1
#define CE_HIT_PLUS -2

static int
ce_tabbar_hit(CEditorState *s, TTF_Font *f, SDL_Rect area,
              int mx, int my, int *es_close)
{
    *es_close = 0;
    if (my < area.y || my >= area.y + CE_TABBAR_H) return CE_HIT_NONE;

    int tx    = area.x + 2;
    int pad_x = 8;

    for (int i = 0; i < s->n_tabs; i++) {
        int tw;
        TTF_SizeUTF8(f, s->tabs[i].name, &tw, NULL);
        int tab_w = ce_tab_width(f, s->tabs[i].name);

        if (mx >= tx && mx < tx + tab_w) {
            int cx = tx + pad_x + tw + 6;
            if (mx >= cx && mx < cx + 12) *es_close = 1;
            return i;
        }
        tx += tab_w;
    }

    /* Boton + */
    if (mx >= tx && mx < tx + 24) return CE_HIT_PLUS;

    return CE_HIT_NONE;
}

static void
draw_tab_bar_interna(CEditorState *s, SDL_Renderer *r, TTF_Font *f, SDL_Rect area)
{
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    SDL_Rect bar = {area.x, area.y, area.w, CE_TABBAR_H};
    ce_fill(r, bar, (SDL_Color){10, 10, 10, 255});
    ce_hline(r, area.x, area.y + CE_TABBAR_H - 1, area.w, CE_SEP);

    int tx    = area.x + 2;
    int pad_x = 8;

    for (int i = 0; i < s->n_tabs; i++) {
        int tw, th;
        TTF_SizeUTF8(f, s->tabs[i].name, &tw, &th);

        int close_w = 12;
        int tab_w   = pad_x + tw + 6 + close_w + pad_x;
        int is_act  = (i == s->tab_activo);
        int hover   = (mx >= tx && mx < tx + tab_w &&
                       my >= area.y && my < area.y + CE_TABBAR_H);

        if (is_act || hover)
            ce_fill(r, (SDL_Rect){tx, area.y, tab_w, CE_TABBAR_H}, CE_TAB_ACT);

        /* linea inferior: neon si activo, sep si hover */
        SDL_SetRenderDrawColor(r,
            is_act  ? CE_ACCENT.r : (hover ? 60 : CE_SEP.r),
            is_act  ? CE_ACCENT.g : (hover ? 60 : CE_SEP.g),
            is_act  ? CE_ACCENT.b : (hover ? 60 : CE_SEP.b), 255);
        SDL_RenderDrawLine(r, tx, area.y + CE_TABBAR_H - 1,
                              tx + tab_w - 2, area.y + CE_TABBAR_H - 1);

        /* separador derecho */
        SDL_SetRenderDrawColor(r, CE_SEP.r, CE_SEP.g, CE_SEP.b, 255);
        SDL_RenderDrawLine(r, tx + tab_w - 1, area.y + 3,
                              tx + tab_w - 1, area.y + CE_TABBAR_H - 4);

        /* nombre — azul si es main */
        SDL_Color tc = s->tabs[i].is_main ? CE_MAIN_COL
                     : ((is_act || hover) ? CE_BRIGHT : CE_TEXT);
        ce_text(r, f, s->tabs[i].name, tx + pad_x, area.y + (CE_TABBAR_H - th) / 2, tc);

        /* × para cerrar — siempre visible en hover/activo */
        int cx       = tx + pad_x + tw + 6;
        int cy       = area.y + (CE_TABBAR_H - th) / 2;
        int hover_x  = (mx >= cx && mx < cx + 12 &&
                        my >= area.y && my < area.y + CE_TABBAR_H);
        SDL_Color xc = hover_x ? CE_WARN : (is_act || hover ? CE_TEXT : (SDL_Color){0,0,0,0});
        ce_text(r, f, "x", cx, cy, xc);

        tx += tab_w;
    }

    /* Boton + */
    if (s->n_tabs < CE_MAX_TABS) {
        int hover_plus = (mx >= tx && mx < tx + 24 &&
                          my >= area.y && my < area.y + CE_TABBAR_H);
        SDL_Color pc = hover_plus ? CE_ACCENT : CE_TEXT;
        int th; TTF_SizeUTF8(f, "+", NULL, &th);
        ce_text(r, f, "+", tx + 4, area.y + (CE_TABBAR_H - th) / 2, pc);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  INTERFAZ DEL PANEL — draw
 * ════════════════════════════════════════════════════════════════════════════ */

void
ceditor_draw(ShellCtx *ctx, Tab *tab, SDL_Rect area)
{
    CEditorState *s = (CEditorState *)tab->state;
    if (!s) return;

    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    SDL_RenderSetClipRect(r, &area);
    ce_fill(r, area, CE_BG);

    int fh;
    TTF_SizeUTF8(f, "A", NULL, &fh);

    /* ══ OUTPUT ══ */
    if (s->mode == MODE_OUTPUT) {
        /* Leer del PTY de output */
        if (s->out_fd >= 0) {
            unsigned char buf[4096]; ssize_t n;
            AnsiCtx a = {
                s->out_screen, &s->out_row, &s->out_col,
                &s->out_ansi_state, s->out_ansi_buf, &s->out_ansi_len
            };
            while ((n = read(s->out_fd, buf, sizeof(buf))) > 0)
                for (ssize_t i = 0; i < n; i++) ansi_feed(&a, buf[i]);

            /* Verificar si termino */
            if (s->out_pid > 0) {
                int st;
                if (waitpid(s->out_pid, &st, WNOHANG) > 0) {
                    s->out_pid = -1;
                    close(s->out_fd); s->out_fd = -1;
                }
            }
        }

        /* Header */
        ce_text(r, f, "[ OUTPUT — clang + run ]",
            area.x + 8, area.y + 6, CE_ACCENT);

        /* Indicador de scroll */
        if (s->out_row > 0) {
            char sc[32];
            snprintf(sc, sizeof(sc), "linea %d/%d", s->out_scroll + 1, s->out_row + 1);
            int sw; TTF_SizeUTF8(f, sc, &sw, NULL);
            ce_text(r, f, sc, area.x + area.w - sw - 10, area.y + 6, CE_TEXT);
        }

        ce_hline(r, area.x, area.y + fh + 10, area.w, CE_SEP);

        /* Buffer scrolleable */
        SDL_Rect out_content = {area.x, area.y + fh + 12,
                                area.w, area.h - fh * 2 - 24};
        SDL_RenderSetClipRect(r, &out_content);

        int visible_rows = out_content.h / fh;
        int y = out_content.y;
        for (int row = s->out_scroll; row < CE_OUT_ROWS && row < s->out_scroll + visible_rows; row++) {
            if (!s->out_screen[row][0]) { y += fh; continue; }

            /* colorear lineas de error/warning/ok */
            SDL_Color lc;
            if      (strstr(s->out_screen[row], "error:"))   lc = CE_WARN;
            else if (strstr(s->out_screen[row], "warning:")) lc = (SDL_Color){220,180, 60,255};
            else if (strstr(s->out_screen[row], "note:"))    lc = (SDL_Color){ 86,156,214,255};
            else if (strstr(s->out_screen[row], ">>>"))      lc = CE_ACCENT;
            else                                              lc = CE_BRIGHT;

            ce_text(r, f, s->out_screen[row], out_content.x + 8, y, lc);
            y += fh;
        }
        SDL_RenderSetClipRect(r, &area);

        /* Footer */
        ce_text(r, f, "[ESC] volver   [F5] recompilar   [↑↓] scroll",
            area.x + 8, area.y + area.h - fh - 6, CE_TEXT);

        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ══ EDITOR ══ */
    if (s->mode == MODE_EDITOR && s->n_tabs > 0) {

        /* Tab bar interna */
        draw_tab_bar_interna(s, r, f, area);

        SDL_Rect content = {
            area.x,
            area.y + CE_TABBAR_H,
            area.w,
            area.h - CE_TABBAR_H - fh - 10  /* espacio para footer */
        };

        CTab *t = &s->tabs[s->tab_activo];

        /* Verificar si vim termino */
        if (t->child_pid > 0) {
            int st;
            if (waitpid(t->child_pid, &st, WNOHANG) > 0) {
                t->child_pid = -1;
                t->vivo      = 0;
                close(t->master_fd); t->master_fd = -1;
                cerrar_tab_interno(s, s->tab_activo);
                SDL_RenderSetClipRect(r, NULL);
                return;
            }
        }

        /* Leer del PTY de vim */
        if (t->master_fd >= 0) {
            unsigned char buf[4096]; ssize_t n;
            AnsiCtx a = {
                t->screen, &t->cur_row, &t->cur_col,
                &t->ansi_state, t->ansi_buf, &t->ansi_len
            };
            while ((n = read(t->master_fd, buf, sizeof(buf))) > 0)
                for (ssize_t i = 0; i < n; i++) ansi_feed(&a, buf[i]);
        }

        /* Renderizar screen buffer con syntax highlighting */
        SDL_RenderSetClipRect(r, &content);
        for (int row = 0; row < CE_ROWS; row++) {
            int y = content.y + row * fh;
            if (y + fh > content.y + content.h) break;
            if (t->screen[row][0])
                ce_draw_row_hl(r, f, t->screen[row], content.x + 4, y);
        }

        /* Cursor parpadeante — raya debajo del caracter actual */
        if (t->vivo && (SDL_GetTicks() % 900) < 450) {
            int crow = t->cur_row;
            int ccol = t->cur_col;
            if (crow < CE_ROWS) {
                int cy = content.y + crow * fh;
                if (cy + fh <= content.y + content.h) {
                    /* medir ancho del texto hasta la columna del cursor */
                    char tmp[CE_ROWBUF];
                    int  clen = (int)strlen(t->screen[crow]);
                    int  take = ccol < clen ? ccol : clen;
                    strncpy(tmp, t->screen[crow], take);
                    tmp[take] = '\0';
                    int px = 0;
                    TTF_SizeUTF8(f, tmp, &px, NULL);
                    /* ancho de un caracter para la raya */
                    int cw;
                    TTF_SizeUTF8(f, "M", &cw, NULL);
                    int cur_x = content.x + 4 + px;
                    /* raya debajo: 2px de alto */
                    SDL_SetRenderDrawColor(r, 220, 220, 220, 220);
                    SDL_Rect cr = {cur_x, cy + fh - 2, cw, 2};
                    SDL_RenderFillRect(r, &cr);
                }
            }
        }

        SDL_RenderSetClipRect(r, &area);

        /* Footer */
        char footer[128];
        snprintf(footer, sizeof(footer),
            "[F5] compilar   [Ctrl+Tab] cambiar tab   [Ctrl+M] marcar main   [Ctrl+W] cerrar tab");
        ce_text(r, f, footer, area.x + 6, area.y + area.h - fh - 4, CE_TEXT);

        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ══ PICKER ══ */
    int pad = 24;
    int cy  = area.y + pad;

    SDL_Color col_dim    = { 80,  80,  80, 255};
    SDL_Color col_normal = {140, 140, 140, 255};
    SDL_Color col_bright = {220, 220, 220, 255};
    SDL_Color col_accent = {  0, 200,  80, 255};
    SDL_Color col_warn   = {200,  80,  80, 255};

    ce_text(r, f, "Archivos .c", area.x + pad, cy, col_dim);
    cy += fh + 4;
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    SDL_RenderDrawLine(r, area.x + pad, cy, area.x + area.w - pad, cy);
    cy += 8;

    if (s->creando_nuevo) {
        ce_text(r, f, "Nombre del nuevo archivo:", area.x + pad, cy, col_dim);
        cy += fh + 6;
        int bw = 300, bh = fh + 10;
        SDL_Rect box = {area.x + pad, cy, bw, bh};
        ce_fill(r, box, (SDL_Color){30, 30, 30, 255});
        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
        SDL_RenderDrawRect(r, &box);
        char display[80];
        snprintf(display, sizeof(display), "%s_.c", s->nuevo_buf);
        ce_text(r, f, display, area.x + pad + 8, cy + 5, col_bright);
        cy += bh + 10;
        ce_text(r, f, "[Enter] crear   [Esc] cancelar", area.x + pad, cy, col_accent);
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    if (s->n_archivos == 0) {
        ce_text(r, f, "No hay archivos .c todavia.", area.x + pad, cy, col_dim);
        cy += fh + 12;
        ce_text(r, f, "[N] crear nuevo", area.x + pad, cy, col_accent);
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    if (s->confirm_borrar && s->sel >= 0) {
        char msg[120];
        snprintf(msg, sizeof(msg), "Borrar %s ?", s->archivos[s->sel]);
        ce_text(r, f, msg, area.x + pad, cy, col_warn);
        cy += fh + 10;
        ce_text(r, f, "[Enter] confirmar   [cualquier otra] cancelar",
                area.x + pad, cy, col_dim);
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    int item_h = fh + 6;
    int end    = s->scroll + CE_LIST_VIS;
    if (end > s->n_archivos) end = s->n_archivos;

    for (int i = s->scroll; i < end; i++) {
        SDL_Rect row_rect = {area.x + pad - 4, cy - 2,
                             area.w - pad * 2 + 8, item_h};
        if (i == s->sel) {
            ce_fill(r, row_rect, (SDL_Color){28, 28, 28, 255});
            SDL_SetRenderDrawColor(r, 0, 200, 80, 255);
            SDL_RenderDrawLine(r, area.x + pad - 4, cy - 2,
                               area.x + pad - 4, cy - 2 + item_h - 1);
            ce_text(r, f, s->archivos[i], area.x + pad + 8, cy, col_bright);
        } else {
            ce_text(r, f, s->archivos[i], area.x + pad + 8, cy, col_normal);
        }
        cy += item_h;
    }

    if (s->n_archivos > CE_LIST_VIS) {
        char sc[16];
        snprintf(sc, sizeof(sc), "%d/%d", s->sel + 1, s->n_archivos);
        int sw; TTF_SizeUTF8(f, sc, &sw, NULL);
        ce_text(r, f, sc, area.x + area.w - pad - sw, area.y + pad, col_dim);
    }

    int fy = area.y + area.h - fh - 10;
    ce_text(r, f,
        "[↑↓] navegar   [Enter] abrir   [D] borrar   [N] nuevo",
        area.x + pad, fy, col_dim);

    SDL_RenderSetClipRect(r, NULL);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  INTERFAZ DEL PANEL — cleanup
 * ════════════════════════════════════════════════════════════════════════════ */

void
ceditor_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    CEditorState *s = (CEditorState *)tab->state;
    if (!s) return;

    for (int i = 0; i < s->n_tabs; i++) {
        CTab *t = &s->tabs[i];
        if (t->child_pid > 0) { kill(t->child_pid, SIGTERM); waitpid(t->child_pid, NULL, 0); }
        if (t->master_fd >= 0) close(t->master_fd);
    }
    if (s->out_pid > 0) { kill(s->out_pid, SIGTERM); waitpid(s->out_pid, NULL, 0); }
    if (s->out_fd >= 0) close(s->out_fd);

    free(s);
    tab->state = NULL;
}
