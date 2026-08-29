/* ═══════════════════════════════════════════════════════════════════════════
 *  xasol_term.c — el emulador de terminal
 *
 *  El por que esta en xasol_term.h. Aca esta el como, en cuatro partes:
 *
 *      1. LA PANTALLA   el buffer de celdas y sus operaciones
 *      2. EL PARSER     de bytes a ordenes
 *      3. EL PROCESO    arrancar bash, leerlo, escribirle
 *      4. EL TECLADO    de teclas SDL a lo que espera una terminal
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "xasol_term.h"

#include <errno.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  1. LA PANTALLA
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
celda_limpiar(XTCelda *c)
{
    c->ch = ' ';
    c->fg = -1;
    c->bg = -1;
    c->negrita = 0;
}

static void
fila_limpiar(XasolTerm *t, int fila, int desde, int hasta)
{
    if (fila < 0 || fila >= t->rows) return;
    if (desde < 0) desde = 0;
    if (hasta >= t->cols) hasta = t->cols - 1;
    for (int c = desde; c <= hasta; c++) celda_limpiar(&t->pantalla[fila][c]);
}

static void
pantalla_limpiar(XasolTerm *t)
{
    for (int f = 0; f < t->rows; f++) fila_limpiar(t, f, 0, t->cols - 1);
}

/* Sube todo una linea y deja la ultima vacia. Es lo que pasa cuando el cursor
   se va del piso: la terminal no crece, se corre. */
static void
scroll(XasolTerm *t)
{
    for (int f = 0; f < t->rows - 1; f++)
        memcpy(t->pantalla[f], t->pantalla[f + 1], sizeof(t->pantalla[0]));
    fila_limpiar(t, t->rows - 1, 0, t->cols - 1);
}

static void
poner(XasolTerm *t, char ch)
{
    if (t->cur_col >= t->cols) {      /* la linea se lleno: sigue abajo */
        t->cur_col = 0;
        t->cur_row++;
    }
    while (t->cur_row >= t->rows) { scroll(t); t->cur_row--; }

    XTCelda *c = &t->pantalla[t->cur_row][t->cur_col];
    c->ch      = ch;
    c->fg      = t->fg;
    c->bg      = t->bg;
    c->negrita = t->negrita;
    t->cur_col++;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  2. EL PARSER
 *
 *  Una secuencia se escribe `ESC [ parametros letra` — por ejemplo `ESC[1;31m`
 *  es "negrita y rojo". La LETRA dice que hacer y los numeros de antes son sus
 *  argumentos.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parte "1;31" en {1, 31}. Devuelve cuantos saco. */
static int
params(const char *s, int *out, int max)
{
    int n = 0, val = 0, hay = 0;
    for (; *s && n < max; s++) {
        if (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); hay = 1; }
        else if (*s == ';')         { out[n++] = hay ? val : 0; val = 0; hay = 0; }
    }
    if (n < max && (hay || n == 0)) out[n++] = val;
    return n;
}

/* SGR — el color y los atributos. Es el que a editorBim le faltaba. */
static void
sgr(XasolTerm *t, int *p, int n)
{
    for (int i = 0; i < n; i++) {
        int v = p[i];
        if (v == 0)                     { t->fg = -1; t->bg = -1; t->negrita = 0; }
        else if (v == 1)                { t->negrita = 1; }
        else if (v == 22)               { t->negrita = 0; }
        else if (v >= 30 && v <= 37)    { t->fg = (signed char)(v - 30); }
        else if (v == 39)               { t->fg = -1; }
        else if (v >= 40 && v <= 47)    { t->bg = (signed char)(v - 40); }
        else if (v == 49)               { t->bg = -1; }
        /* Los brillantes: 90-97 son los mismos ocho con negrita. */
        else if (v >= 90 && v <= 97)    { t->fg = (signed char)(v - 90); t->negrita = 1; }
        else if (v >= 100 && v <= 107)  { t->bg = (signed char)(v - 100); }
        /* 38;5;N y 48;5;N son la paleta de 256. Se toman los primeros 16 y el
           resto se deja en el color normal: PAED y bash usan los basicos, y
           una tabla de 256 no cambiaria nada de lo que se ve aca. */
        else if ((v == 38 || v == 48) && i + 2 < n && p[i + 1] == 5) {
            int idx = p[i + 2];
            if (idx < 16) { if (v == 38) t->fg = (signed char)(idx % 8);
                            else         t->bg = (signed char)(idx % 8);
                            if (idx >= 8 && v == 38) t->negrita = 1; }
            i += 2;
        }
    }
}

static void
csi(XasolTerm *t, char letra)
{
    int p[8] = {0};
    int n = params(t->buf, p, 8);
    int a = p[0];

    switch (letra) {
    case 'm': sgr(t, p, n); break;

    case 'H': case 'f':                       /* cursor a fila;columna */
        t->cur_row = (n > 0 && p[0] > 0) ? p[0] - 1 : 0;
        t->cur_col = (n > 1 && p[1] > 0) ? p[1] - 1 : 0;
        break;

    case 'A': t->cur_row -= a ? a : 1; break;
    case 'B': t->cur_row += a ? a : 1; break;
    case 'C': t->cur_col += a ? a : 1; break;
    case 'D': t->cur_col -= a ? a : 1; break;
    case 'G': t->cur_col  = a ? a - 1 : 0; break;   /* a una columna */
    case 'd': t->cur_row  = a ? a - 1 : 0; break;   /* a una fila    */

    case 'J':                                  /* borrar pantalla */
        if      (a == 0) { fila_limpiar(t, t->cur_row, t->cur_col, t->cols - 1);
                           for (int f = t->cur_row + 1; f < t->rows; f++)
                               fila_limpiar(t, f, 0, t->cols - 1); }
        else if (a == 1) { for (int f = 0; f < t->cur_row; f++)
                               fila_limpiar(t, f, 0, t->cols - 1);
                           fila_limpiar(t, t->cur_row, 0, t->cur_col); }
        else             { pantalla_limpiar(t); t->cur_row = t->cur_col = 0; }
        break;

    case 'K':                                  /* borrar linea */
        if      (a == 0) fila_limpiar(t, t->cur_row, t->cur_col, t->cols - 1);
        else if (a == 1) fila_limpiar(t, t->cur_row, 0, t->cur_col);
        else             fila_limpiar(t, t->cur_row, 0, t->cols - 1);
        break;

    case 'P': {                                /* borrar N caracteres */
        int cuantos = a ? a : 1;
        XTCelda *fila = t->pantalla[t->cur_row];
        for (int c = t->cur_col; c < t->cols; c++)
            if (c + cuantos < t->cols) fila[c] = fila[c + cuantos];
            else                       celda_limpiar(&fila[c]);
        break;
    }

    case '@': {                                /* insertar N espacios */
        int cuantos = a ? a : 1;
        XTCelda *fila = t->pantalla[t->cur_row];
        for (int c = t->cols - 1; c >= t->cur_col + cuantos; c--)
            fila[c] = fila[c - cuantos];
        fila_limpiar(t, t->cur_row, t->cur_col, t->cur_col + cuantos - 1);
        break;
    }

    case 'h': case 'l':                        /* modos: solo el del cursor */
        if (t->interrogacion && a == 25) t->cur_visible = (letra == 'h');
        break;

    default: break;   /* lo que no se entiende se ignora, no se dibuja */
    }

    if (t->cur_row < 0) t->cur_row = 0;
    if (t->cur_col < 0) t->cur_col = 0;
    if (t->cur_row >= t->rows) t->cur_row = t->rows - 1;
    if (t->cur_col >  t->cols) t->cur_col = t->cols;
}

static void
byte(XasolTerm *t, unsigned char b)
{
    switch (t->estado) {

    case 0:                                    /* texto normal */
        if (b == 0x1b) { t->estado = 1; t->buf_len = 0; t->interrogacion = 0; }
        else if (b == '\r') t->cur_col = 0;
        else if (b == '\n') {
            t->cur_row++;
            while (t->cur_row >= t->rows) { scroll(t); t->cur_row--; }
        }
        else if (b == '\b') { if (t->cur_col > 0) t->cur_col--; }
        else if (b == '\t') { do { poner(t, ' '); } while (t->cur_col % 8); }
        else if (b == 0x07) { /* la campanita: no suena nada */ }
        else if (b >= 0x20)  poner(t, (char)b);
        break;

    case 1:                                    /* vino un ESC */
        if      (b == '[') { t->estado = 2; t->buf_len = 0; }
        else if (b == ']') { t->estado = 3; }        /* OSC: titulos */
        else if (b == '(' || b == ')') { t->estado = 4; }  /* juego de chars */
        else               { t->estado = 0; }
        break;

    case 2:                                    /* adentro de un CSI */
        if (b == '?') { t->interrogacion = 1; }
        else if (b >= 0x30 && b <= 0x3f) {
            if (t->buf_len < (int)sizeof(t->buf) - 1) t->buf[t->buf_len++] = (char)b;
        }
        else if (b >= 0x40 && b <= 0x7e) {
            t->buf[t->buf_len] = '\0';
            csi(t, (char)b);
            t->estado = 0;
        }
        break;

    case 3:                                    /* OSC: hasta BEL o ESC \ */
        if (b == 0x07)      t->estado = 0;
        else if (b == 0x1b) t->estado = 5;
        break;

    case 4: t->estado = 0; break;              /* el byte del juego de chars */

    case 5: t->estado = 0; break;              /* el '\\' que cierra el OSC  */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  3. EL PROCESO
 * ═══════════════════════════════════════════════════════════════════════════ */

int
xterm_abrir(XasolTerm *t, const char *cwd, int rows, int cols)
{
    memset(t, 0, sizeof(*t));
    t->rows = rows > XT_MAX_ROWS ? XT_MAX_ROWS : (rows < 4 ? 4 : rows);
    t->cols = cols > XT_MAX_COLS ? XT_MAX_COLS : (cols < 20 ? 20 : cols);
    t->fg = t->bg = -1;
    t->cur_visible = 1;
    pantalla_limpiar(t);

    struct winsize ws = {0};
    ws.ws_row = (unsigned short)t->rows;
    ws.ws_col = (unsigned short)t->cols;

    pid_t pid = forkpty(&t->fd, NULL, NULL, &ws);
    if (pid < 0) { t->vivo = 0; return 0; }

    if (pid == 0) {
        if (cwd && cwd[0]) { if (chdir(cwd) != 0) { /* seguir donde se pueda */ } }

        /* TERM le dice a bash que puede usar: sin esto se comporta como una
           terminal tonta y no manda colores. 'xterm-256color' es lo que
           entiende este parser. */
        setenv("TERM", "xterm-256color", 1);
        setenv("LANG", "en_US.UTF-8", 1);

        /* -i: interactivo. Es lo que hace que haya prompt e historial. */
        execlp("bash", "bash", "-i", (char *)NULL);
        _exit(127);
    }

    t->pid  = pid;
    t->vivo = 1;
    fcntl(t->fd, F_SETFL, O_NONBLOCK);   /* leer sin quedarse esperando */
    return 1;
}

void
xterm_leer(XasolTerm *t)
{
    if (!t->vivo) return;

    unsigned char buf[4096];
    ssize_t n;
    /* Un tope por frame: si un comando escupe un archivo entero, se reparte
       en varios frames en vez de congelar uno. */
    int vueltas = 0;
    while ((n = read(t->fd, buf, sizeof(buf))) > 0 && vueltas++ < 16)
        for (ssize_t i = 0; i < n; i++) byte(t, buf[i]);

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        /* El otro lado cerro: bash salio. */
        int st;
        waitpid(t->pid, &st, WNOHANG);
        close(t->fd);
        t->fd   = -1;
        t->vivo = 0;
        return;
    }

    int st;
    if (waitpid(t->pid, &st, WNOHANG) > 0) { close(t->fd); t->fd = -1; t->vivo = 0; }
}

static void
mandar(XasolTerm *t, const char *s, size_t n)
{
    if (t->vivo && t->fd >= 0) { if (write(t->fd, s, n) < 0) { /* se perdio */ } }
}

void
xterm_texto(XasolTerm *t, const char *txt)
{
    if (txt && *txt) mandar(t, txt, strlen(txt));
}

void
xterm_comando(XasolTerm *t, const char *cmd)
{
    if (!cmd || !*cmd) return;
    mandar(t, cmd, strlen(cmd));
    mandar(t, "\r", 1);
}

void
xterm_redimensionar(XasolTerm *t, int rows, int cols)
{
    if (rows > XT_MAX_ROWS) rows = XT_MAX_ROWS;
    if (cols > XT_MAX_COLS) cols = XT_MAX_COLS;
    if (rows < 4)  rows = 4;
    if (cols < 20) cols = 20;
    if (rows == t->rows && cols == t->cols) return;

    t->rows = rows;
    t->cols = cols;
    if (t->cur_row >= rows) t->cur_row = rows - 1;
    if (t->cur_col >= cols) t->cur_col = cols - 1;

    /* Avisarle al programa. Sin esto bash sigue partiendo las lineas donde
       creia que terminaba la pantalla, y el texto queda cortado en el aire. */
    if (t->vivo && t->fd >= 0) {
        struct winsize ws = {0};
        ws.ws_row = (unsigned short)rows;
        ws.ws_col = (unsigned short)cols;
        ioctl(t->fd, TIOCSWINSZ, &ws);
    }
}

void
xterm_cerrar(XasolTerm *t)
{
    if (!t->vivo) return;
    kill(t->pid, SIGHUP);        /* SIGHUP: "se cerro la terminal", que es lo
                                    que de verdad paso. bash lo entiende y se
                                    va ordenado, guardando su historial. */
    waitpid(t->pid, NULL, 0);
    if (t->fd >= 0) close(t->fd);
    t->fd   = -1;
    t->vivo = 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  4. EL TECLADO
 *
 *  Una terminal no recibe "teclas": recibe BYTES. Las flechas son secuencias
 *  de tres, Ctrl+C es el byte 3, y Enter es un retorno de carro.
 * ═══════════════════════════════════════════════════════════════════════════ */

void
xterm_tecla(XasolTerm *t, SDL_Keycode k, SDL_Keymod mod)
{
    if (!t->vivo) return;

    /* Ctrl+letra manda el byte 1..26. Es como se corta un programa (Ctrl+C
       = 3), como se cierra la entrada (Ctrl+D = 4) y como funciona media
       terminal. */
    if (mod & SDL_KMOD_CTRL) {
        if (k >= SDLK_A && k <= SDLK_Z) {
            char b = (char)(k - SDLK_A + 1);
            mandar(t, &b, 1);
            return;
        }
    }

    const char *seq = NULL;
    char b = 0;

    switch (k) {
    case SDLK_RETURN: case SDLK_KP_ENTER: b = '\r';    break;
    case SDLK_BACKSPACE:                  b = 0x7f;    break;   /* DEL */
    case SDLK_TAB:                        b = '\t';    break;   /* autocompletar */
    case SDLK_ESCAPE:                     b = 0x1b;    break;
    case SDLK_UP:     seq = "\x1b[A"; break;                    /* historial */
    case SDLK_DOWN:   seq = "\x1b[B"; break;
    case SDLK_RIGHT:  seq = "\x1b[C"; break;
    case SDLK_LEFT:   seq = "\x1b[D"; break;
    case SDLK_HOME:   seq = "\x1b[H"; break;
    case SDLK_END:    seq = "\x1b[F"; break;
    case SDLK_DELETE: seq = "\x1b[3~"; break;
    case SDLK_PAGEUP:   seq = "\x1b[5~"; break;
    case SDLK_PAGEDOWN: seq = "\x1b[6~"; break;
    default: break;
    }

    if (b)   mandar(t, &b, 1);
    if (seq) mandar(t, seq, strlen(seq));
}

/* ── Los colores ────────────────────────────────────────────────────────────
 *
 * Los 8 de siempre, en su version normal y en negrita. Son los que usa bash
 * para el prompt, `ls` para los directorios y `git` para el estado.
 */
SDL_Color
xterm_color(int idx, int negrita)
{
    static const SDL_Color NORMAL[8] = {
        { 40,  40,  40, 255},   /* 0 negro   */
        {205,  49,  49, 255},   /* 1 rojo    */
        { 13, 188, 121, 255},   /* 2 verde   */
        {229, 229,  16, 255},   /* 3 amarillo*/
        { 36, 114, 200, 255},   /* 4 azul    */
        {188,  63, 188, 255},   /* 5 magenta */
        { 17, 168, 205, 255},   /* 6 cian    */
        {229, 229, 229, 255},   /* 7 blanco  */
    };
    static const SDL_Color BRILLANTE[8] = {
        {102, 102, 102, 255},
        {241,  76,  76, 255},
        { 35, 209, 139, 255},
        {245, 245,  67, 255},
        { 59, 142, 234, 255},
        {214, 112, 214, 255},
        { 41, 184, 219, 255},
        {255, 255, 255, 255},
    };

    if (idx < 0 || idx > 7) return negrita ? BRILLANTE[7] : (SDL_Color){200, 200, 200, 255};
    return negrita ? BRILLANTE[idx] : NORMAL[idx];
}
