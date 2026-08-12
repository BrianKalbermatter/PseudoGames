#define _XOPEN_SOURCE 600
#include "editorBim.h"
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <pty.h>

/* ── Dimensiones del buffer de terminal ── */
#define BIM_ROWS   24
#define BIM_COLS   80
#define BIM_ROWBUF 256
#define MAX_FILES  64
#define LIST_VIS   10   /* filas visibles en el picker */

typedef struct {
    /* ── File picker ── */
    int  esperando_archivo;   /* 0 = PTY activo */
    char archivos[MAX_FILES][64];
    int  n_archivos;
    int  sel;                 /* item seleccionado (-1 si lista vacia) */
    int  scroll;              /* primer item visible */

    /* Confirmar borrado */
    int  confirm_borrar;

    /* Crear nuevo archivo */
    int  creando_nuevo;
    char nuevo_buf[64];
    int  nuevo_len;
    int  skip_next_text;   /* ignora el SDL_TEXTINPUT que sigue al [N] */

    /* ── PTY ── */
    int   master_fd;
    pid_t child_pid;

    /* ── Screen buffer del terminal ── */
    char  screen[BIM_ROWS][BIM_ROWBUF];
    int   cur_row;
    int   cur_col;

    char  nombre_arch[256];
    char  project_root[512];  /* cwd al momento del init */

    /* ── Parser ANSI ── */
    int   ansi_state;
    char  ansi_buf[64];
    int   ansi_len;
} EditorBimState;

/* ════════════════════════════════════════════════════════════════════════════
 *  HELPERS
 * ════════════════════════════════════════════════════════════════════════════ */

static void
bim_text(SDL_Renderer *r, TTF_Font *f,
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
bim_fill(SDL_Renderer *r, SDL_Rect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

/* Comparador para qsort — ordena alfabeticamente */
static int
cmp_str(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

/* Escanea saves/ buscando archivos .paed y llena s->archivos */
static void
scan_archivos(EditorBimState *s)
{
    s->n_archivos = 0;
    s->sel        = -1;
    s->scroll     = 0;

    DIR *d = opendir("saves");
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) && s->n_archivos < MAX_FILES) {
        int len = (int)strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".paed") == 0)
            strncpy(s->archivos[s->n_archivos++], ent->d_name, 63);
    }
    closedir(d);

    qsort(s->archivos, s->n_archivos, sizeof(s->archivos[0]), cmp_str);

    if (s->n_archivos > 0) s->sel = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  ANSI PARSER
 * ════════════════════════════════════════════════════════════════════════════ */

static void
ansi_execute(EditorBimState *s, char cmd)
{
    int p[4] = {0};
    int np = 0;
    char tmp[64];
    strncpy(tmp, s->ansi_buf, sizeof(tmp) - 1);
    char *tok = strtok(tmp, ";");
    while (tok && np < 4) { p[np++] = atoi(tok); tok = strtok(NULL, ";"); }

    switch (cmd) {
    case 'J':
        for (int i = 0; i < BIM_ROWS; i++) memset(s->screen[i], 0, BIM_ROWBUF);
        s->cur_row = s->cur_col = 0;
        break;
    case 'H': case 'f':
        s->cur_row = (p[0] > 0 ? p[0] - 1 : 0);
        s->cur_col = (p[1] > 0 ? p[1] - 1 : 0);
        if (s->cur_row >= BIM_ROWS) s->cur_row = BIM_ROWS - 1;
        if (s->cur_col >= BIM_COLS) s->cur_col = BIM_COLS - 1;
        break;
    case 'A': s->cur_row -= (p[0] ? p[0] : 1); if (s->cur_row < 0)         s->cur_row = 0;          break;
    case 'B': s->cur_row += (p[0] ? p[0] : 1); if (s->cur_row >= BIM_ROWS) s->cur_row = BIM_ROWS-1; break;
    case 'C': s->cur_col += (p[0] ? p[0] : 1); if (s->cur_col >= BIM_COLS) s->cur_col = BIM_COLS-1; break;
    case 'D': s->cur_col -= (p[0] ? p[0] : 1); if (s->cur_col < 0)         s->cur_col = 0;          break;
    case 'K':
        if (p[0] == 0 && s->cur_row < BIM_ROWS) {
            int len = (int)strlen(s->screen[s->cur_row]);
            if (s->cur_col < len)
                memset(s->screen[s->cur_row] + s->cur_col, ' ', len - s->cur_col);
        }
        break;
    }
}

static void
process_byte(EditorBimState *s, unsigned char c)
{
    switch (s->ansi_state) {
    case 0:
        if      (c == 0x1b) { s->ansi_state = 1; }
        else if (c == '\r') { s->cur_col = 0; }
        else if (c == '\n') { if (++s->cur_row >= BIM_ROWS) s->cur_row = BIM_ROWS - 1; }
        else if ((c >= 0x20 && c < 0x7f) || c >= 0x80) {
            if (s->cur_row >= BIM_ROWS) break;
            char *row = s->screen[s->cur_row];
            int   len = (int)strlen(row);
            if (s->cur_col < len) {
                row[s->cur_col] = (char)c;
            } else {
                while (len < s->cur_col && len < BIM_ROWBUF - 2) row[len++] = ' ';
                if (len < BIM_ROWBUF - 1) { row[len] = (char)c; row[len+1] = '\0'; }
            }
            s->cur_col++;
        }
        break;
    case 1:
        s->ansi_state = (c == '[') ? 2 : 0;
        if (s->ansi_state == 2) { s->ansi_len = 0; memset(s->ansi_buf, 0, sizeof(s->ansi_buf)); }
        break;
    case 2:
        if      (c >= 0x30 && c <= 0x3f) { if (s->ansi_len < 63) s->ansi_buf[s->ansi_len++] = (char)c; }
        else if (c >= 0x20 && c <= 0x2f) { /* intermedio: ignorar */ }
        else if (c >= 0x40 && c <= 0x7e) { s->ansi_buf[s->ansi_len] = '\0'; ansi_execute(s, (char)c); s->ansi_state = 0; }
        else                              { s->ansi_state = 0; }
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  LANZAR PTY
 *  filepath = ruta absoluta al .paed
 *  display  = nombre corto para el tab
 * ════════════════════════════════════════════════════════════════════════════ */

static void
lanzar_pty(EditorBimState *s, Tab *tab,
           const char *filepath, const char *display)
{
    struct winsize ws = {0};
    ws.ws_row = BIM_ROWS;
    ws.ws_col = BIM_COLS;

    pid_t pid = forkpty(&s->master_fd, NULL, NULL, &ws);
    if (pid < 0) { s->master_fd = -1; return; }

    if (pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("LANG", "en_US.UTF-8",    1);
        if (chdir("scripts/editorBim") == 0)
            execl("/bin/bash", "bash", "bim.sh", filepath, (char *)NULL);
        _exit(1);
    }

    s->child_pid = pid;
    fcntl(s->master_fd, F_SETFL, O_NONBLOCK);
    strncpy(s->nombre_arch, display, 255);
    s->esperando_archivo = 0;

    for (int i = 0; i < BIM_ROWS; i++) memset(s->screen[i], 0, BIM_ROWBUF);
    s->cur_row = s->cur_col = 0;
    s->ansi_state = 0;

    snprintf(tab->nombre, sizeof(tab->nombre), "bim: %s", display);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  INTERFAZ DEL PANEL
 * ════════════════════════════════════════════════════════════════════════════ */

void
editor_bim_init(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    EditorBimState *s = calloc(1, sizeof(*s));
    s->master_fd         = -1;
    s->child_pid         = -1;
    s->esperando_archivo = 1;

    /* Guardar el cwd para construir rutas absolutas */
    if (!getcwd(s->project_root, sizeof(s->project_root)))
        strcpy(s->project_root, ".");

    scan_archivos(s);
    tab->state = s;
}

/* ── handle_event ── */

void
editor_bim_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    (void)ctx;
    EditorBimState *s = (EditorBimState *)tab->state;
    if (!s) return;

    /* ════════ PTY ACTIVO ════════ */
    if (!s->esperando_archivo) {
        if (s->master_fd < 0) return;
        if (e->type == SDL_TEXTINPUT) {
            (void)write(s->master_fd, e->text.text, strlen(e->text.text));
        } else if (e->type == SDL_KEYDOWN) {
            SDL_Keycode  k   = e->key.keysym.sym;
            const char  *seq = NULL;
            char         ch  = 0;
            switch (k) {
            case SDLK_RETURN:   case SDLK_KP_ENTER: ch  = '\r';     break;
            case SDLK_BACKSPACE:                     ch  = '\x7f';   break;
            case SDLK_ESCAPE:                        ch  = '\x1b';   break;
            case SDLK_UP:                            seq = "\x1b[A"; break;
            case SDLK_DOWN:                          seq = "\x1b[B"; break;
            case SDLK_RIGHT:                         seq = "\x1b[C"; break;
            case SDLK_LEFT:                          seq = "\x1b[D"; break;
            case SDLK_HOME:                          seq = "\x1b[H"; break;
            case SDLK_END:                           seq = "\x1b[F"; break;
            default: break;
            }
            if (ch)  (void)write(s->master_fd, &ch,  1);
            if (seq) (void)write(s->master_fd, seq, strlen(seq));
        }
        return;
    }

    /* ════════ CREAR NUEVO ARCHIVO ════════ */
    if (s->creando_nuevo) {
        if (e->type == SDL_TEXTINPUT) {
            if (s->skip_next_text) { s->skip_next_text = 0; return; }
            int len = (int)strlen(e->text.text);
            if (s->nuevo_len + len < 60) {
                memcpy(s->nuevo_buf + s->nuevo_len, e->text.text, len);
                s->nuevo_len              += len;
                s->nuevo_buf[s->nuevo_len] = '\0';
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
                /* Crear el archivo vacio en saves/ */
                char filename[80], filepath[600];
                snprintf(filename, sizeof(filename), "%s.paed", s->nuevo_buf);
                snprintf(filepath, sizeof(filepath), "%s/saves/%s",
                         s->project_root, filename);
                FILE *f = fopen(filepath, "a");
                if (f) fclose(f);

                s->creando_nuevo = 0;
                memset(s->nuevo_buf, 0, sizeof(s->nuevo_buf));
                s->nuevo_len = 0;

                lanzar_pty(s, tab, filepath, filename);
            }
        }
        return;
    }

    /* ════════ FILE PICKER ════════ */
    if (e->type != SDL_KEYDOWN) return;
    SDL_Keycode k = e->key.keysym.sym;

    /* Confirmar borrado pendiente */
    if (s->confirm_borrar) {
        if (k == SDLK_RETURN && s->sel >= 0 && s->sel < s->n_archivos) {
            char filepath[600];
            snprintf(filepath, sizeof(filepath), "%s/saves/%s",
                     s->project_root, s->archivos[s->sel]);
            remove(filepath);
            scan_archivos(s);
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
        if (s->sel > 0) {
            s->sel--;
            if (s->sel < s->scroll) s->scroll = s->sel;
        }
        break;
    case SDLK_DOWN:
        if (s->sel < s->n_archivos - 1) {
            s->sel++;
            if (s->sel >= s->scroll + LIST_VIS) s->scroll = s->sel - LIST_VIS + 1;
        }
        break;
    case SDLK_RETURN:
        if (s->sel >= 0) {
            char filepath[600];
            snprintf(filepath, sizeof(filepath), "%s/saves/%s",
                     s->project_root, s->archivos[s->sel]);
            lanzar_pty(s, tab, filepath, s->archivos[s->sel]);
        }
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

/* ── draw ── */

void
editor_bim_draw(ShellCtx *ctx, Tab *tab, SDL_Rect area)
{
    EditorBimState *s = (EditorBimState *)tab->state;
    if (!s) return;

    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    SDL_RenderSetClipRect(r, &area);
    bim_fill(r, area, (SDL_Color){18, 18, 18, 255});

    /* ════════ PTY ACTIVO ════════ */
    if (!s->esperando_archivo) {
        /* Verificar si el hijo termino */
        if (s->child_pid > 0) {
            int st;
            if (waitpid(s->child_pid, &st, WNOHANG) > 0) {
                s->child_pid         = -1;
                close(s->master_fd);
                s->master_fd         = -1;
                s->esperando_archivo = 1;
                snprintf(tab->nombre, sizeof(tab->nombre), "Editor Libre");
                scan_archivos(s);
                SDL_RenderSetClipRect(r, NULL);
                return;
            }
        }
        /* Leer del PTY */
        if (s->master_fd >= 0) {
            unsigned char buf[4096];
            ssize_t n;
            while ((n = read(s->master_fd, buf, sizeof(buf))) > 0)
                for (ssize_t i = 0; i < n; i++) process_byte(s, buf[i]);
        }
        /* Renderizar el screen buffer */
        int fh;
        TTF_SizeUTF8(f, "A", NULL, &fh);
        SDL_Color fg = {210, 210, 210, 255};
        for (int row = 0; row < BIM_ROWS; row++) {
            int y = area.y + row * fh;
            if (y + fh > area.y + area.h) break;
            if (s->screen[row][0])
                bim_text(r, f, s->screen[row], area.x + 4, y, fg);
        }
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ════════ FILE PICKER ════════ */
    int fh;
    TTF_SizeUTF8(f, "A", NULL, &fh);
    int pad  = 24;
    int cy   = area.y + pad;

    SDL_Color col_dim    = { 80,  80,  80, 255};
    SDL_Color col_normal = {140, 140, 140, 255};
    SDL_Color col_bright = {220, 220, 220, 255};
    SDL_Color col_accent = {  0, 200,  80, 255};
    SDL_Color col_sel_bg = { 28,  28,  28, 255};
    SDL_Color col_warn   = {200,  80,  80, 255};

    /* Header */
    bim_text(r, f, "Archivos .paed", area.x + pad, cy, col_dim);
    cy += fh + 4;

    /* Separador */
    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    SDL_RenderDrawLine(r, area.x + pad, cy, area.x + area.w - pad, cy);
    cy += 8;

    /* ── Crear nuevo ── */
    if (s->creando_nuevo) {
        bim_text(r, f, "Nombre del nuevo archivo:", area.x + pad, cy, col_dim);
        cy += fh + 6;

        int bw = 300, bh = fh + 10;
        SDL_Rect box = {area.x + pad, cy, bw, bh};
        bim_fill(r, box, (SDL_Color){30, 30, 30, 255});
        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
        SDL_RenderDrawRect(r, &box);

        char display[80];
        snprintf(display, sizeof(display), "%s_", s->nuevo_buf);
        bim_text(r, f, display, area.x + pad + 8, cy + 5, col_bright);
        cy += bh + 6;

        char hint[80];
        snprintf(hint, sizeof(hint), "se guarda como %s.paed", s->nuevo_buf[0] ? s->nuevo_buf : "nombre");
        bim_text(r, f, hint, area.x + pad, cy, col_dim);
        cy += fh + 10;
        bim_text(r, f, "[Enter] crear   [Esc] cancelar", area.x + pad, cy, col_accent);

        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ── Lista vacia ── */
    if (s->n_archivos == 0) {
        bim_text(r, f, "No hay archivos .paed todavia.", area.x + pad, cy, col_dim);
        cy += fh + 12;
        bim_text(r, f, "[N] crear nuevo", area.x + pad, cy, col_accent);
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ── Confirmar borrado ── */
    if (s->confirm_borrar && s->sel >= 0) {
        char msg[120];
        snprintf(msg, sizeof(msg), "Borrar  %s ?", s->archivos[s->sel]);
        bim_text(r, f, msg, area.x + pad, cy, col_warn);
        cy += fh + 10;
        bim_text(r, f, "[Enter] confirmar   [cualquier otra] cancelar",
                 area.x + pad, cy, col_dim);
        SDL_RenderSetClipRect(r, NULL);
        return;
    }

    /* ── Lista de archivos ── */
    int item_h = fh + 6;
    int end    = s->scroll + LIST_VIS;
    if (end > s->n_archivos) end = s->n_archivos;

    for (int i = s->scroll; i < end; i++) {
        SDL_Rect row_rect = {area.x + pad - 4, cy - 2,
                             area.w - pad * 2 + 8, item_h};
        if (i == s->sel) {
            bim_fill(r, row_rect, col_sel_bg);
            /* borde verde izquierdo */
            SDL_SetRenderDrawColor(r, 0, 200, 80, 255);
            SDL_RenderDrawLine(r, area.x + pad - 4, cy - 2,
                               area.x + pad - 4, cy - 2 + item_h - 1);
            bim_text(r, f, s->archivos[i], area.x + pad + 8, cy, col_bright);
        } else {
            bim_text(r, f, s->archivos[i], area.x + pad + 8, cy, col_normal);
        }
        cy += item_h;
    }

    /* Scroll indicator */
    if (s->n_archivos > LIST_VIS) {
        char sc[16];
        snprintf(sc, sizeof(sc), "%d/%d", s->sel + 1, s->n_archivos);
        int sw; TTF_SizeUTF8(f, sc, &sw, NULL);
        bim_text(r, f, sc, area.x + area.w - pad - sw,
                 area.y + pad, col_dim);
    }

    /* Footer con hints */
    int fy = area.y + area.h - fh - 10;
    bim_text(r, f,
        "[↑↓] navegar   [Enter] abrir   [D] borrar   [N] nuevo",
        area.x + pad, fy, col_dim);

    SDL_RenderSetClipRect(r, NULL);
}

void
editor_bim_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    EditorBimState *s = (EditorBimState *)tab->state;
    if (!s) return;
    if (s->child_pid > 0) { kill(s->child_pid, SIGTERM); waitpid(s->child_pid, NULL, 0); }
    if (s->master_fd >= 0) close(s->master_fd);
    free(s);
    tab->state = NULL;
}
