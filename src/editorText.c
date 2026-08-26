#include "editorText.h"
#include "screenVerificador.h"
#include "progreso.h"
#include "audio.h"
#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Dibuja texto en (x, y) exacto, sin offsets extra */
static void
dibujadoTextoSimple(SDL_Renderer *renderer, TTF_Font *fuente,
                    const char *texto, int x, int y, SDL_Color color)
{
    SDL_Surface *s = TTF_RenderText_Blended(fuente, texto, 0, color);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_FRect pos   = { x, y, s->w, s->h };
    SDL_RenderTexture(renderer, t, NULL, &pos);
    SDL_DestroySurface(s);
    SDL_DestroyTexture(t);
}

/* ── Paleta gruvbox dark ──────────────────────────────────────────── */
#define BG_R   40,  40,  40  /* #282828 fondo del editor              */
#define GT_R   29,  32,  33  /* #1d2021 fondo del gutter (numeros)    */
#define LN_R  102,  92,  84  /* #665c54 color de los numeros de linea */
#define PH_R   80,  73,  69  /* #504945 color del placeholder         */
#define TX_R  235, 219, 178  /* #ebdbb2 texto normal (futuro)         */

#define GUTTER_W   50   /* ancho del gutter en pixeles  */
#define LINEAS_VIS 20   /* cuantas lineas se ven        */

/* ── drawEditorText ───────────────────────────────────────────────────
 * Dibuja el editor dentro del rect `area` sin loop propio.
 * Se puede llamar desde cualquier pantalla para embeber el editor.
 * ─────────────────────────────────────────────────────────────────── */
void
drawEditorText(SDL_Renderer *renderer, TTF_Font *fuente, SDL_FRect area)
{
    int line_h = TTF_GetFontHeight(fuente) + 4;
    int x = area.x;
    int y = area.y;
    int h = area.h;

    ui_clip(renderer, &area);

    /* Fondo del editor */
    SDL_SetRenderDrawColor(renderer, BG_R, 255);
    SDL_RenderFillRect(renderer, &area);

    /* Gutter */
    SDL_FRect gutter = { x, y, GUTTER_W, h };
    SDL_SetRenderDrawColor(renderer, GT_R, 255);
    SDL_RenderFillRect(renderer, &gutter);

    /* Separador gutter / contenido */
    SDL_SetRenderDrawColor(renderer, LN_R, 255);
    SDL_RenderLine(renderer, x + GUTTER_W, y, x + GUTTER_W, y + h);

    SDL_Color c_linea_num   = { LN_R, 255 };
    SDL_Color c_placeholder = { PH_R, 255 };
    SDL_Color c_cursor      = { TX_R, 200 };

    for (int i = 0; i < LINEAS_VIS; i++) {
        int ly = y + i * line_h + 8;
        if (ly + line_h > y + h) break;

        /* numero de linea */
        char num[8];
        snprintf(num, sizeof(num), "%d", i + 1);
        int num_w;
        TTF_GetStringSize(fuente, num, 0, &num_w, NULL);

        SDL_Surface *s_num = TTF_RenderText_Blended(fuente, num, 0, c_linea_num);
        if (s_num) {
            SDL_Texture *t_num = SDL_CreateTextureFromSurface(renderer, s_num);
            SDL_FRect pos = { x + GUTTER_W - num_w - 8, ly, s_num->w, s_num->h };
            SDL_RenderTexture(renderer, t_num, NULL, &pos);
            SDL_DestroySurface(s_num);
            SDL_DestroyTexture(t_num);
        }

        /* placeholder en linea 1 */
        if (i == 0) {
            const char *ph = "Escribe tu codigo aqui...";
            SDL_Surface *s_ph = TTF_RenderText_Blended(fuente, ph, 0, c_placeholder);
            if (s_ph) {
                SDL_Texture *t_ph = SDL_CreateTextureFromSurface(renderer, s_ph);
                SDL_FRect pos = { x + GUTTER_W + 12, ly, s_ph->w, s_ph->h };
                SDL_RenderTexture(renderer, t_ph, NULL, &pos);
                SDL_DestroySurface(s_ph);
                SDL_DestroyTexture(t_ph);
            }

            /* cursor fijo */
            SDL_SetRenderDrawColor(renderer, c_cursor.r, c_cursor.g, c_cursor.b, c_cursor.a);
            SDL_FRect cursor = { x + GUTTER_W + 12, ly, 2, line_h - 2 };
            SDL_RenderFillRect(renderer, &cursor);
        }
    }

    ui_clip(renderer, NULL);
}

/* ── Paleta syntax highlighting (gruvbox dimmed ~65%) ───────────── */
/* ── Diagnósticos (errores inline como VSCode) ───────────────────── */
#define MAX_DIAGS 64

typedef struct {
    int  linea;        /* 1-based; 0 = error global (sin línea) */
    char msg[128];
} Diagnostico;

/* ── Paleta syntax highlighting (gruvbox) ────────────────────────── *
 * Cada categoría tiene su propio color para identificar visualmente  *
 * ─────────────────────────────────────────────────────────────────── */
#define C_COND      69, 133, 136, 255  /* azul          SI FIN_SI SINO ENTONCES  */
#define C_LOOP     214,  93,  14, 255  /* naranja       MIENTRAS PARA REPETIR... */
#define C_STRUCT   215, 153,  33, 255  /* dorado        ACCION FUNCION PROCESO.. */
#define C_OUTPUT   177,  98, 134, 255  /* violeta       ESCRIBIR                 */
#define C_INPUT    104, 157, 106, 255  /* verde-teal    LEER                     */
#define C_TIPO      83, 150, 153, 255  /* azul claro    ENTERO VAR ARREGLO...    */
#define C_OPERADOR 152, 151,  26, 255  /* verde-lima    Y O NO * /               */
#define C_FUNC     250, 189,  47, 255  /* amarillo      MOD TRUNC ABSO REDOND    */
#define C_SEQ      131, 165, 152, 255  /* aqua          ARR AVZ (secuencias)     */
#define C_COMMENT   95,  85,  75, 255  /* gris          // comentarios           */
#define C_STRING   214, 105,  14, 255  /* naranja       "strings"                */
#define C_NUMBER   211, 134, 155, 255  /* rosa-lila     numeros                  */
#define C_NORMAL   235, 219, 178, 255  /* blanco-crema  variables e identifiers  */

/* ── Arrays por categoría ────────────────────────────────────────── */

/* Condicionales */
static const char *kw_cond[] = {
    "SI","FIN_SI","SINO","ENTONCES", NULL
};
/* Bucles */
static const char *kw_loop[] = {
    "MIENTRAS","FIN_MIENTRAS","PARA","FIN_PARA",
    "REPETIR","HASTA","HACER", NULL
};
/* Estructura del programa */
static const char *kw_struct[] = {
    "ACCION","ES","FIN_ACCION","PROCESO","AMBIENTE",
    "FUNCION","FIN_FUNCION","PROCEDIMIENTO","FIN_PROCEDIMIENTO",
    "RETORNAR","SEGUN","ABRIR","CERRAR","ARCHIVO","REGISTRO", NULL
};
/* Salida */
static const char *kw_output[] = { "ESCRIBIR", NULL };
/* Entrada */
static const char *kw_input[]  = { "LEER", NULL };
/* Tipos de datos */
static const char *kw_tipos[] = {
    "ENTERO","REAL","CARACTER","BOOLEANO",
    "SECUENCIA_DE_CARACTERES","SECUENCIA_DE_ENTEROS",
    "ARREGLO","CONSTANTE","VAR","DE","AN","N", NULL
};
/* Operadores lógicos */
static const char *kw_operadores[] = { "Y","O","NO", NULL };
/* Funciones built-in */
static const char *kw_funciones[]  = { "MOD","DIV","TRUNC","ABSO","REDOND", NULL };
/* Operaciones de secuencia y ventana */
static const char *kw_seq[]        = { "ARR","AVZ","VENTANA","NFDS","FDS", NULL };

/* Devuelve el color según la categoría del token */
static SDL_Color
get_token_color(const char *tok)
{
    for (int i = 0; kw_cond[i];      i++) if (strcmp(tok, kw_cond[i])      == 0) return (SDL_Color){C_COND};
    for (int i = 0; kw_loop[i];      i++) if (strcmp(tok, kw_loop[i])      == 0) return (SDL_Color){C_LOOP};
    for (int i = 0; kw_struct[i];    i++) if (strcmp(tok, kw_struct[i])    == 0) return (SDL_Color){C_STRUCT};
    for (int i = 0; kw_output[i];    i++) if (strcmp(tok, kw_output[i])    == 0) return (SDL_Color){C_OUTPUT};
    for (int i = 0; kw_input[i];     i++) if (strcmp(tok, kw_input[i])     == 0) return (SDL_Color){C_INPUT};
    for (int i = 0; kw_tipos[i];     i++) if (strcmp(tok, kw_tipos[i])     == 0) return (SDL_Color){C_TIPO};
    for (int i = 0; kw_operadores[i];i++) if (strcmp(tok, kw_operadores[i])== 0) return (SDL_Color){C_OPERADOR};
    for (int i = 0; kw_funciones[i]; i++) if (strcmp(tok, kw_funciones[i]) == 0) return (SDL_Color){C_FUNC};
    for (int i = 0; kw_seq[i];      i++) if (strcmp(tok, kw_seq[i])      == 0) return (SDL_Color){C_SEQ};
    return (SDL_Color){C_NORMAL};
}

/* Dibuja una linea de texto con syntax highlighting token por token */
static void
render_highlighted(SDL_Renderer *renderer, TTF_Font *fuente,
                   const char *text, int x, int y)
{
    int p   = 0;
    int len = (int)strlen(text);

    while (p < len) {
        unsigned char c = (unsigned char)text[p];

        /* Espacios: avanzar x sin dibujar */
        if (c == ' ') {
            int sw; TTF_GetStringSize(fuente, " ", 0, &sw, NULL);
            x += sw;
            p++;
            continue;
        }

        char     tok[512];
        int      tok_len = 0;
        SDL_Color color;

        /* Comentario: // hasta el final */
        if (c == '/' && p+1 < len && text[p+1] == '/') {
            strncpy(tok, text + p, sizeof(tok) - 1);
            tok[sizeof(tok) - 1] = '\0';
            tok_len = len - p;
            color   = (SDL_Color){C_COMMENT};
        }
        /* String: "..."  (34 = ASCII double quote) */
        else if (c == 34) {
            int e = p + 1;
            while (e < len && (unsigned char)text[e] != 34) e++;
            if (e < len) e++;   /* incluir la comilla de cierre */
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = (SDL_Color){C_STRING};
        }
        /* Numero: empieza con digito */
        else if (c >= '0' && c <= '9') {
            int e = p;
            while (e < len) {
                unsigned char ec = (unsigned char)text[e];
                if ((ec >= '0' && ec <= '9') || ec == '.') e++;
                else break;
            }
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = (SDL_Color){C_NUMBER};
        }
        /* Palabra: letras, digitos, guion_bajo */
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            int e = p;
            while (e < len) {
                unsigned char ec = (unsigned char)text[e];
                if ((ec >= 'a' && ec <= 'z') || (ec >= 'A' && ec <= 'Z') ||
                    (ec >= '0' && ec <= '9') || ec == '_') e++;
                else break;
            }
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = get_token_color(tok);
        }
        /* Resto: :=  ;  (  )  etc. */
        else {
            tok[0] = (char)c; tok[1] = '\0';
            tok_len = 1;
            color = (SDL_Color){C_NORMAL};
        }

        /* Renderizar token */
        SDL_Surface *s = TTF_RenderText_Blended(fuente, tok, 0, color);
        if (s) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_FRect dst = { x, y, s->w, s->h };
            SDL_RenderTexture(renderer, t, NULL, &dst);
            x += s->w;
            SDL_DestroySurface(s);
            SDL_DestroyTexture(t);
        }

        p += tok_len;
    }
}

/* ── render_highlighted_wrap ──────────────────────────────────────────
 * Igual que render_highlighted pero hace word-wrap al llegar a wrap_w.
 * Retorna cuántas líneas visuales usó.
 * Si cursor_col >= 0, escribe la posición pixel del cursor en *cx, *cy.
 * ─────────────────────────────────────────────────────────────────── */
static int
render_highlighted_wrap(SDL_Renderer *renderer, TTF_Font *fuente,
                        const char *text, int x_start, int y_start,
                        int wrap_w, int line_h,
                        int cursor_col, int *cx, int *cy)
{
    int p      = 0;
    int len    = (int)strlen(text);
    int x      = x_start;
    int y      = y_start;
    int vlines = 1;
    int col    = 0;   /* columna de carácter actual para tracking del cursor */
    int sw1; TTF_GetStringSize(fuente, " ", 0, &sw1, NULL);

    /* Cursor al inicio de línea vacía */
    if (cx && cursor_col == 0) { *cx = x; *cy = y; }

    while (p < len) {
        unsigned char c = (unsigned char)text[p];

        /* ── Tokenizar (mismo que render_highlighted) ─────────── */
        char      tok[512];
        int       tok_len = 0;
        SDL_Color color;
        int       is_space = 0;

        if (c == ' ') {
            tok[0] = ' '; tok[1] = '\0';
            tok_len = 1;
            color   = (SDL_Color){C_NORMAL};
            is_space = 1;
        } else if (c == '/' && p+1 < len && text[p+1] == '/') {
            strncpy(tok, text + p, sizeof(tok) - 1);
            tok[sizeof(tok) - 1] = '\0';
            tok_len = len - p;
            color   = (SDL_Color){C_COMMENT};
        } else if (c == 34) {
            int e = p + 1;
            while (e < len && (unsigned char)text[e] != 34) e++;
            if (e < len) e++;
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = (SDL_Color){C_STRING};
        } else if (c >= '0' && c <= '9') {
            int e = p;
            while (e < len) {
                unsigned char ec = (unsigned char)text[e];
                if ((ec >= '0' && ec <= '9') || ec == '.') e++; else break;
            }
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = (SDL_Color){C_NUMBER};
        } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            int e = p;
            while (e < len) {
                unsigned char ec = (unsigned char)text[e];
                if ((ec >= 'a' && ec <= 'z') || (ec >= 'A' && ec <= 'Z') ||
                    (ec >= '0' && ec <= '9') || ec == '_') e++;
                else break;
            }
            tok_len = e - p;
            strncpy(tok, text + p, tok_len);
            tok[tok_len] = '\0';
            color = get_token_color(tok);
        } else {
            tok[0] = (char)c; tok[1] = '\0';
            tok_len = 1;
            color = (SDL_Color){C_NORMAL};
        }

        /* ── Medir token ───────────────────────────────────────── */
        int tw = sw1;
        if (!is_space) {
            SDL_Surface *sm = TTF_RenderText_Blended(fuente, tok, 0, color);
            if (sm) { tw = sm->w; SDL_DestroySurface(sm); }
        }

        /* ── Wrap: si no cabe y no estamos al inicio de visual-line ─ */
        if (x + tw > x_start + wrap_w && x > x_start) {
            y += line_h;
            x  = x_start;
            vlines++;
        }

        /* ── Cursor position tracking ──────────────────────────── */
        if (cx && cursor_col >= col && cursor_col < col + tok_len) {
            /* cursor cae dentro de este token: calcular x exacto */
            int before = cursor_col - col;
            if (before > 0) {
                char tmp[512]; strncpy(tmp, tok, before); tmp[before] = '\0';
                int bw; TTF_GetStringSize(fuente, tmp, 0, &bw, NULL);
                *cx = x + bw;
            } else {
                *cx = x;
            }
            *cy = y;
        }

        /* ── Dibujar token ─────────────────────────────────────── */
        if (is_space) {
            x += sw1;
        } else {
            SDL_Surface *s = TTF_RenderText_Blended(fuente, tok, 0, color);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_RenderTexture(renderer, t, NULL, &(SDL_FRect){x, y, s->w, s->h});
                x += s->w;
                SDL_DestroySurface(s); SDL_DestroyTexture(t);
            }
        }

        col += tok_len;
        p   += tok_len;
    }

    /* Cursor al final del texto */
    if (cx && cursor_col >= col) { *cx = x; *cy = y; }

    return vlines;
}

/* ── Clipboard multi-linea ────────────────────────────────────────── */
#define CB_MAX 50
static char cb_buf[CB_MAX][512];
static int  cb_n = 0;

/* Copia lineas [r0..r1] del buffer en cb_buf y en el portapapeles del sistema */
static void
editor_copiar(char buf[][512], int r0, int r1)
{
    if (r0 > r1) { int t = r0; r0 = r1; r1 = t; }
    cb_n = 0;
    char texto[CB_MAX * 513];
    texto[0] = '\0';
    for (int i = r0; i <= r1 && cb_n < CB_MAX; i++, cb_n++) {
        strncpy(cb_buf[cb_n], buf[i], 511);
        cb_buf[cb_n][511] = '\0';
        strncat(texto, cb_buf[cb_n], sizeof(texto) - strlen(texto) - 2);
        if (i < r1) strncat(texto, "\n", sizeof(texto) - strlen(texto) - 1);
    }
    SDL_SetClipboardText(texto);
}

/* Corta lineas [r0..r1]: las copia y las elimina del buffer */
static void
editor_cortar(char buf[][512], int *n_lines, int r0, int r1,
              int *cursor_row, int *cursor_col)
{
    if (r0 > r1) { int t = r0; r0 = r1; r1 = t; }
    editor_copiar(buf, r0, r1);
    int count = r1 - r0 + 1;
    for (int i = r0; i + count < *n_lines; i++)
        memcpy(buf[i], buf[i + count], 512);
    for (int i = *n_lines - count; i < *n_lines; i++)
        buf[i][0] = '\0';
    *n_lines -= count;
    if (*n_lines < 1) { *n_lines = 1; buf[0][0] = '\0'; }
    if (*cursor_row >= *n_lines) *cursor_row = *n_lines - 1;
    *cursor_col = 0;
}

/* Pega cb_buf (o el portapapeles del sistema) despues de cursor_row */
static void
editor_pegar(char buf[][512], int *n_lines, int *cursor_row, int *cursor_col)
{
    /* Intentar leer desde portapapeles del sistema primero */
    char *sys = SDL_GetClipboardText();
    if (sys && sys[0] != '\0') {
        static char tmp[CB_MAX][512];
        int tn = 0;
        char *p = sys;
        while (*p && tn < CB_MAX) {
            char *nl = strchr(p, '\n');
            int len = nl ? (int)(nl - p) : (int)strlen(p);
            if (len > 511) len = 511;
            strncpy(tmp[tn], p, len);
            tmp[tn][len] = '\0';
            tn++;
            if (!nl) break;
            p = nl + 1;
        }
        SDL_free(sys);
        if (tn > 0) {
            cb_n = tn;
            for (int i = 0; i < tn; i++) strncpy(cb_buf[i], tmp[i], 511);
        }
    } else {
        if (sys) SDL_free(sys);
    }

    if (cb_n == 0) return;
    if (*n_lines + cb_n > MAX_LINES) return;

    int ins = *cursor_row + 1;
    /* desplazar lineas existentes hacia abajo */
    for (int i = *n_lines - 1; i >= ins; i--)
        memcpy(buf[i + cb_n], buf[i], 512);
    /* insertar las lineas del clipboard */
    for (int i = 0; i < cb_n; i++)
        strncpy(buf[ins + i], cb_buf[i], 511);
    *n_lines  += cb_n;
    *cursor_row = ins + cb_n - 1;
    *cursor_col = (int)strlen(buf[*cursor_row]);
}

/* ── helpers de archivo ───────────────────────────────────────────── */
#include <dirent.h>

/* Escanea saves/ y llena lista[] con nombres sin extension .paed */
static int
escanear_saves(char lista[][64], int max)
{
    int n = 0;
    DIR *d = opendir("saves");
    if (!d) return 0;
    struct dirent *e;
    while (n < max && (e = readdir(d))) {
        int l = (int)strlen(e->d_name);
        if (l > 5 && strcmp(e->d_name + l - 5, ".paed") == 0) {
            int nl = l - 5;
            if (nl >= (int)sizeof(lista[0])) nl = (int)sizeof(lista[0]) - 1;
            strncpy(lista[n], e->d_name, nl);
            lista[n][nl] = '\0';
            n++;
        }
    }
    closedir(d);
    /* orden alfabetico simple (bubble) */
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (strcmp(lista[i], lista[j]) > 0) {
                char tmp[64]; strncpy(tmp, lista[i], 63);
                strncpy(lista[i], lista[j], 63); strncpy(lista[j], tmp, 63);
            }
    return n;
}

/* Guarda el buffer completo en `path` */
static void
guardar_archivo(char buf[][512], int n, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < n; i++) fprintf(f, "%s\n", buf[i]);
    fclose(f);
}

/* Ejecuta paed sobre `path` y llena out[][] con el output */
static int
ejecutar_paed(const char *path, char out[][256], int max)
{
    char cmd[256];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "cd Frankly && paed.exe \"../%s\" 2>&1", path);
#else
    snprintf(cmd, sizeof(cmd), "cd Frankly && ./paed '../%s' 2>&1", path);
#endif
    FILE *fp = popen(cmd, "r");
    if (!fp) { strncpy(out[0], "error: no se pudo ejecutar paed", 255); return 1; }
    int  n = 0;
    char lb[512];
    while (n < max && fgets(lb, sizeof(lb), fp)) {
        int l = (int)strlen(lb);
        if (l > 0 && lb[l-1] == '\n') lb[l-1] = '\0';
        strncpy(out[n], lb, 255); out[n][255] = '\0'; n++;
    }
    pclose(fp);
    return n > 0 ? n : 0;
}

/* Corre ./paed sobre `path`, parsea líneas "archivo:N: error: msg"
 * y llena el array diags[]. Retorna cantidad de diagnósticos. */
static int
analizar_paed_diags(const char *path, Diagnostico *diags, int max)
{
    char cmd[256];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "cd Frankly && paed.exe \"../%s\" 2>&1", path);
#else
    snprintf(cmd, sizeof(cmd), "cd Frankly && ./paed '../%s' 2>&1", path);
#endif
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0;

    int  n = 0;
    char lb[512];
    while (n < max && fgets(lb, sizeof(lb), fp)) {
        int l = (int)strlen(lb);
        if (l > 0 && lb[l-1] == '\n') lb[l-1] = '\0';

        /* Formato 1: "algo:N: error: mensaje"  (error en línea específica) */
        /* Formato 2: "algo: error: mensaje"     (error global, sin línea)   */
        const char *p = lb;
        /* Buscar primer ':' */
        const char *c1 = strchr(p, ':');
        if (!c1) continue;

        /* ¿El siguiente char es un dígito? → tiene número de línea */
        const char *after_c1 = c1 + 1;
        if (*after_c1 >= '0' && *after_c1 <= '9') {
            int linea = 0;
            while (*after_c1 >= '0' && *after_c1 <= '9')
                linea = linea * 10 + (*after_c1++ - '0');
            /* after_c1 ahora apunta a ':' o espacio */
            const char *msg_start = after_c1;
            if (*msg_start == ':') msg_start++;   /* saltar ':' */
            while (*msg_start == ' ') msg_start++; /* saltar espacios */
            /* Quitar prefijo "error: " si existe */
            if (strncmp(msg_start, "error: ", 7) == 0) msg_start += 7;
            diags[n].linea = linea;
            strncpy(diags[n].msg, msg_start, sizeof(diags[n].msg) - 1);
            diags[n].msg[sizeof(diags[n].msg) - 1] = '\0';
            n++;
        } else {
            /* Error global: buscar " error: " en la cadena */
            const char *e = strstr(lb, " error: ");
            if (!e) { e = strstr(lb, ": error:"); }
            const char *msg_start = e ? e + 8 : lb;
            while (*msg_start == ' ') msg_start++;
            diags[n].linea = 0;
            strncpy(diags[n].msg, msg_start, sizeof(diags[n].msg) - 1);
            diags[n].msg[sizeof(diags[n].msg) - 1] = '\0';
            n++;
        }
    }
    pclose(fp);
    return n;
}

/* ── screenEditorText ─────────────────────────────────────────────────
 * Editor multi-linea completo. ESC/F10 para salir.
 * ─────────────────────────────────────────────────────────────────── */

int
screenEditorText(SDL_Renderer *renderer, TTF_Font *fuente,
                 int ancho, int alto, const char *nombre_fijo,
                 const char *cons_titulo, const char *cons_texto,
                 Nivel *nivel, int nivel_num)
{
    /* ── Buffer de lineas ─────────────────────────────────────────── */
    static EditorPanel tabs[MAX_TABS];
    static int tab_activo = 0;
    static int n_tabs     = 1;
    static char libre_ultimo[64] = "";  /* ultimo archivo guardado en modo libre */

    /* Reset de la tab activa al entrar */
    memset(&tabs[tab_activo], 0, sizeof(EditorPanel));
    tabs[tab_activo].n_lines = 1;

    /* Alias corto para la tab activa */
    EditorPanel *ep = &tabs[tab_activo];

    /* ── Layout ───────────────────────────────────────────────────── */
    int tiene_consigna = (cons_titulo && cons_titulo[0] != '\0');
    int editor_x  = tiene_consigna ? ancho / 2 : 0;
    int editor_w  = tiene_consigna ? ancho / 2 : ancho;
    int line_h    = TTF_GetFontHeight(fuente) + 4;
    int out_h     = line_h * 6 + 24;
    int edit_h    = alto - out_h;          /* y donde empieza la salida */
    int vis_lines = (edit_h - TAB_H - 8) / line_h;

    /* Boton |> RUN (panel de salida, extremo derecho) */
    int btn_run_w = 72;
    SDL_FRect btn_run = { editor_x + editor_w - btn_run_w - 6, edit_h + 3, btn_run_w, line_h };

    /* Boton menu ≡ (a la izquierda del RUN) */
    int btn_menu_w = 30;
    SDL_FRect btn_menu = { btn_run.x - btn_menu_w - 6, edit_h + 3, btn_menu_w, line_h };

    /* Dropdown del mini menu */
    int   mini_menu   = 0;
    const char *menu_items[] = { "Guardar", "Cargar" };
    int   mitem_h   = line_h + 6;
    int   mdrop_w   = 260;
    int   mdrop_h   = mitem_h * 2 + 6;
    SDL_FRect mdrop  = { btn_menu.x + btn_menu_w - mdrop_w,
                        edit_h - mdrop_h - 4,
                        mdrop_w, mdrop_h };

    /* ── Guardar ──────────────────────────────────────────────────── */
    int tiene_nombre_fijo = (nombre_fijo && nombre_fijo[0] != '\0');
    if (tiene_nombre_fijo) strncpy(ep->nombre_arch, nombre_fijo, sizeof(ep->nombre_arch)-1);
    else                   ep->nombre_arch[0] = '\0';
    ep->nombre_arch[sizeof(ep->nombre_arch)-1] = '\0';

    char prompt_buf[64]  = "";
    int  prompt_len      = 0;
    int  guardando       = 0;
    int  cargando        = 0;
    int  confirm_salir   = 0;   /* 1 = mostrando dialogo "salir sin guardar?" */

    /* ── Lista de archivos guardados (para overlay F9) ────────────── */
#define MAX_SAVES 50
    char saves_lista[MAX_SAVES][64];
    int  saves_n        = 0;
    int  saves_sel      = -1;   /* fila seleccionada con flechas */
    int  saves_offset   = 0;    /* scroll de la lista */

    /* ── Cargar archivo existente (solo si tiene nombre fijo) ─────── */
    if (tiene_nombre_fijo) {
        char path[128];
        snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
        FILE *f = fopen(path, "r");
        if (f) {
            ep->n_lines = 0;
            char linea[512];
            while (ep->n_lines < MAX_LINES && fgets(linea, sizeof(linea), f)) {
                int l = (int)strlen(linea);
                if (l > 0 && linea[l-1] == '\n') linea[l-1] = '\0';
                strncpy(ep->buf[ep->n_lines], linea, 511);
                ep->buf[ep->n_lines][511] = '\0';
                ep->n_lines++;
            }
            fclose(f);
            if (ep->n_lines == 0) ep->n_lines = 1;
        }
    }

    /* ── Salida ───────────────────────────────────────────────────── */
#define OUT_MAX 6
    char out_buf[OUT_MAX][256];
    int  n_out = 0;
    memset(out_buf, 0, sizeof(out_buf));

    SDL_Color c_num    = { LN_R, 255 };
    SDL_Color c_ph     = { PH_R, 255 };
    SDL_Color c_out    = {   0, 210,  75, 255 };
    SDL_Color c_out_hd = {   0, 155,  50, 255 };


    SDL_FRect btn_nuevo = {0, 0, 0, 0};  /* boton "Nuevo archivo" del estado vacio */

    ui_text_input(true);
    SDL_Event evento;
    int corriendo = 1;

    while (corriendo) {

        /* ── Eventos ──────────────────────────────────────────────── */
        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_EVENT_QUIT) { ui_text_input(false); return 0; }

            if (evento.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode k = evento.key.key;

                /* ── Dialogo confirmar salida ──────────────────────── */
                if (confirm_salir) {
                    if (k == SDLK_S || k == SDLK_RETURN) { corriendo = 0; }
                    if (k == SDLK_N || k == SDLK_ESCAPE)  confirm_salir = 0;
                    break;
                }

                /* ── Modo guardar nombre ───────────────────────────── */
                if (guardando) {
                    if (k == SDLK_ESCAPE) {
                        guardando = 0; prompt_len = 0; prompt_buf[0] = '\0';
                    }
                    if (k == SDLK_BACKSPACE && prompt_len > 0) {
                        prompt_buf[--prompt_len] = '\0';
                        saves_sel = -1; /* al editar texto, deseleccionar lista */
                    }
                    /* Navegar lista con flechas */
                    if (k == SDLK_UP) {
                        if (saves_sel > 0) saves_sel--;
                        else saves_sel = saves_n - 1;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        }
                    }
                    if (k == SDLK_DOWN) {
                        if (saves_sel < saves_n - 1) saves_sel++;
                        else saves_sel = 0;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        }
                    }
                    /* scroll: mantener seleccion visible (vis = 8 filas) */
                    if (saves_sel >= 0) {
                        if (saves_sel < saves_offset) saves_offset = saves_sel;
                        if (saves_sel >= saves_offset + 8) saves_offset = saves_sel - 7;
                    }
                    /* Delete: borrar archivo seleccionado */
                    if (k == SDLK_DELETE && saves_sel >= 0) {
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", saves_lista[saves_sel]);
                        remove(path);
                        saves_n   = escanear_saves(saves_lista, MAX_SAVES);
                        if (saves_sel >= saves_n) saves_sel = saves_n - 1;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        } else { prompt_buf[0] = '\0'; prompt_len = 0; }
                    }
                    if (k == SDLK_RETURN && prompt_len > 0) {
                        /* quitar .paed si el usuario lo escribio */
                        if (prompt_len > 5 && strcmp(prompt_buf + prompt_len - 5, ".paed") == 0) {
                            prompt_buf[prompt_len - 5] = '\0';
                            prompt_len -= 5;
                        }
                        strncpy(ep->nombre_arch, prompt_buf, sizeof(ep->nombre_arch)-1);
                        ep->nombre_arch[sizeof(ep->nombre_arch)-1] = '\0';
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                        guardar_archivo(ep->buf, ep->n_lines, path);
                        ep->dirty = 0;
                        guardando = 0; prompt_len = 0; prompt_buf[0] = '\0';
                        /* recordar el nombre para auto-cargar la proxima vez */
                        if (!tiene_nombre_fijo)
                            strncpy(libre_ultimo, ep->nombre_arch, sizeof(libre_ultimo)-1);
                    }
                    break;
                }

                /* ── Modo cargar archivo ───────────────────────────── */
                if (cargando) {
                    if (k == SDLK_ESCAPE) {
                        cargando = 0; prompt_len = 0; prompt_buf[0] = '\0';
                    }
                    if (k == SDLK_BACKSPACE && prompt_len > 0) {
                        prompt_buf[--prompt_len] = '\0';
                        saves_sel = -1;
                    }
                    if (k == SDLK_UP) {
                        if (saves_sel > 0) saves_sel--;
                        else saves_sel = saves_n - 1;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        }
                    }
                    if (k == SDLK_DOWN) {
                        if (saves_sel < saves_n - 1) saves_sel++;
                        else saves_sel = 0;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        }
                    }
                    if (saves_sel >= 0) {
                        if (saves_sel < saves_offset) saves_offset = saves_sel;
                        if (saves_sel >= saves_offset + 8) saves_offset = saves_sel - 7;
                    }
                    if (k == SDLK_DELETE && saves_sel >= 0) {
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", saves_lista[saves_sel]);
                        remove(path);
                        saves_n = escanear_saves(saves_lista, MAX_SAVES);
                        if (saves_sel >= saves_n) saves_sel = saves_n - 1;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        } else { prompt_buf[0] = '\0'; prompt_len = 0; }
                    }
                    if (k == SDLK_RETURN && prompt_len > 0) {
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", prompt_buf);
                        FILE *fl = fopen(path, "r");
                        if (fl) {
                            memset(ep->buf, 0, sizeof(ep->buf));
                            ep->n_lines = 0;
                            char linea[512];
                            while (ep->n_lines < MAX_LINES && fgets(linea, sizeof(linea), fl)) {
                                int l = (int)strlen(linea);
                                if (l > 0 && linea[l-1] == '\n') linea[l-1] = '\0';
                                strncpy(ep->buf[ep->n_lines], linea, 511);
                                ep->buf[ep->n_lines][511] = '\0';
                                ep->n_lines++;
                            }
                            fclose(fl);
                            if (ep->n_lines == 0) ep->n_lines = 1;
                            /* quitar .paed si el usuario lo escribio */
                            {
                                int pl = (int)strlen(prompt_buf);
                                if (pl > 5 && strcmp(prompt_buf + pl - 5, ".paed") == 0)
                                    prompt_buf[pl - 5] = '\0';
                            }
                            strncpy(ep->nombre_arch, prompt_buf, sizeof(ep->nombre_arch)-1);
                            strncpy(libre_ultimo, prompt_buf, sizeof(libre_ultimo)-1);
                            ep->cursor_row = 0; ep->cursor_col = 0; ep->offset_row = 0;
                            ep->dirty = 0;
                        }
                        cargando = 0; prompt_len = 0; prompt_buf[0] = '\0';
                    }
                    break;
                }

                /* ── Editor normal ─────────────────────────────────── */
                if (k == SDLK_F10) {
                    audio_sfx_btn();
                    if (ep->dirty) confirm_salir = 1;
                    else           corriendo = 0;
                    break;
                }
                if (k == SDLK_F8)  { audio_sfx_btn(); screenDoc(renderer, fuente, ancho, alto); break; }
                if (k == SDLK_TAB && (SDL_GetModState() & SDL_KMOD_CTRL)) {
                    /* Ctrl+Tab = siguiente tab */
                    audio_sfx_btn();
                    tab_activo = (tab_activo + 1) % n_tabs;
                    ep = &tabs[tab_activo];
                    break;
                }
                if (k == SDLK_TAB && !(SDL_GetModState() & SDL_KMOD_CTRL)) {
                    char *ln = ep->buf[ep->cursor_row];
                    int len = (int)strlen(ln);
                    if (len + 4 < 511) {
                        memmove(ln + ep->cursor_col + 4, ln + ep->cursor_col, len - ep->cursor_col + 1);
                        memset(ln + ep->cursor_col, ' ', 4);
                        ep->cursor_col += 4;
                        ep->dirty = 1;
                    }
                    break;
                }

                /* Nueva tab: Ctrl+T */
                if (k == SDLK_T && (SDL_GetModState() & SDL_KMOD_CTRL)) {
                    if (n_tabs < MAX_TABS) {
                        audio_sfx_btn();
                        tab_activo = n_tabs;
                        n_tabs++;
                        memset(&tabs[tab_activo], 0, sizeof(EditorPanel));
                        tabs[tab_activo].n_lines = 1;
                        ep = &tabs[tab_activo];
                    }
                    break;
                }
                /* Cerrar tab activa: Ctrl+W */
                if (k == SDLK_W && (SDL_GetModState() & SDL_KMOD_CTRL)) {
                    if (n_tabs > 1) {
                        audio_sfx_btn();
                        for (int i = tab_activo; i < n_tabs - 1; i++)
                            tabs[i] = tabs[i + 1];
                        n_tabs--;
                        if (tab_activo >= n_tabs) tab_activo = n_tabs - 1;
                        ep = &tabs[tab_activo];
                    }
                    break;
                }

                /* ENTER: partir linea en cursor_col */
                if (k == SDLK_RETURN && ep->n_lines < MAX_LINES) {
                    char resto[512];
                    strncpy(resto, ep->buf[ep->cursor_row] + ep->cursor_col, 511);
                    resto[511] = '\0';
                    ep->buf[ep->cursor_row][ep->cursor_col] = '\0';
                    for (int i = ep->n_lines; i > ep->cursor_row + 1; i--)
                        memcpy(ep->buf[i], ep->buf[i-1], 512);
                    strncpy(ep->buf[ep->cursor_row + 1], resto, 511);
                    ep->buf[ep->cursor_row + 1][511] = '\0';
                    ep->n_lines++;
                    ep->cursor_row++;
                    ep->cursor_col = 0;
                    ep->dirty = 1;
                }

                /* BACKSPACE */
                if (k == SDLK_BACKSPACE) {
                    if (ep->cursor_col > 0) {
                        int len = (int)strlen(ep->buf[ep->cursor_row]);
                        memmove(ep->buf[ep->cursor_row] + ep->cursor_col - 1,
                                ep->buf[ep->cursor_row] + ep->cursor_col,
                                len - ep->cursor_col + 1);
                        ep->cursor_col--;
                        ep->dirty = 1;
                    } else if (ep->cursor_row > 0) {
                        /* al inicio: merge con linea anterior */
                        int prev_len = (int)strlen(ep->buf[ep->cursor_row - 1]);
                        int cur_len  = (int)strlen(ep->buf[ep->cursor_row]);
                        if (prev_len + cur_len < 511) {
                            strcat(ep->buf[ep->cursor_row - 1], ep->buf[ep->cursor_row]);
                            for (int i = ep->cursor_row; i < ep->n_lines - 1; i++)
                                memcpy(ep->buf[i], ep->buf[i+1], 512);
                            ep->buf[ep->n_lines - 1][0] = '\0';
                            ep->n_lines--;
                            ep->cursor_row--;
                            ep->cursor_col = prev_len;
                            ep->dirty = 1;
                        }
                    }
                }

                /* Flechas */
                if (k == SDLK_LEFT) {
                    if (ep->cursor_col > 0) ep->cursor_col--;
                    else if (ep->cursor_row > 0) {
                        ep->cursor_row--;
                        ep->cursor_col = (int)strlen(ep->buf[ep->cursor_row]);
                    }
                }
                if (k == SDLK_RIGHT) {
                    int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
                    if (ep->cursor_col < cur_len) ep->cursor_col++;
                    else if (ep->cursor_row < ep->n_lines - 1) { ep->cursor_row++; ep->cursor_col = 0; }
                }
                if (k == SDLK_UP && ep->cursor_row > 0) {
                    ep->cursor_row--;
                    int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
                    if (ep->cursor_col > cur_len) ep->cursor_col = cur_len;
                }
                if (k == SDLK_DOWN && ep->cursor_row < ep->n_lines - 1) {
                    ep->cursor_row++;
                    int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
                    if (ep->cursor_col > cur_len) ep->cursor_col = cur_len;
                }
                if (k == SDLK_HOME) ep->cursor_col = 0;
                if (k == SDLK_END)  ep->cursor_col = (int)strlen(ep->buf[ep->cursor_row]);

                /* Ctrl+C: copiar linea actual */
                if (k == SDLK_C && (evento.key.mod & SDL_KMOD_CTRL))
                    editor_copiar(ep->buf, ep->cursor_row, ep->cursor_row);

                /* Ctrl+X: cortar linea actual */
                if (k == SDLK_X && (evento.key.mod & SDL_KMOD_CTRL)) {
                    editor_cortar(ep->buf, &ep->n_lines, ep->cursor_row, ep->cursor_row,
                                  &ep->cursor_row, &ep->cursor_col);
                    ep->dirty = 1;
                }

                /* Ctrl+V: pegar (una o multiples lineas) */
                if (k == SDLK_V && (evento.key.mod & SDL_KMOD_CTRL)) {
                    editor_pegar(ep->buf, &ep->n_lines, &ep->cursor_row, &ep->cursor_col);
                    ep->dirty = 1;
                }

                /* Scroll: mantener cursor visible */
                if (ep->cursor_row < ep->offset_row) ep->offset_row = ep->cursor_row;
                if (ep->cursor_row >= ep->offset_row + vis_lines)
                    ep->offset_row = ep->cursor_row - vis_lines + 1;

                /* F5: ya tiene audio_sfx_btn en su bloque abajo */
                /* F9: guardar
                 * Si ya tiene nombre (fijo o dado antes): sobreescribe directo.
                 * Si no tiene nombre todavia: abre el overlay para elegir uno. */
                if (k == SDLK_F9) {
                    if (ep->nombre_arch[0] != '\0') {
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                        guardar_archivo(ep->buf, ep->n_lines, path);
                        ep->dirty = 0;
                    } else {
                        /* sin nombre aun: abrir overlay */
                        guardando  = 1;
                        prompt_buf[0] = '\0';
                        prompt_len    = 0;
                        saves_n   = escanear_saves(saves_lista, MAX_SAVES);
                        saves_sel = -1; saves_offset = 0;
                    }
                }

                /* F5: guardar y ejecutar */
                if (k == SDLK_F5) {
                    audio_sfx_btn();
                    char path[128];
                    snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                    guardar_archivo(ep->buf, ep->n_lines, path);
                    n_out = ejecutar_paed(path, out_buf, OUT_MAX);
                    if (nivel && n_out < OUT_MAX) {
                        if (verificar_nivel(nivel, ep->nombre_arch)) {
                            marcar_completado(nivel_num);
                            strncpy(out_buf[n_out], "[OK] Nivel superado!", 255);
                        } else {
                            strncpy(out_buf[n_out], "[FAIL] Resultado incorrecto.", 255);
                        }
                        n_out++;
                    }
                }
            }

            /* Click en [X] de la lista de guardado */
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN && (guardando || cargando) &&
                evento.button.button == SDL_BUTTON_LEFT) {
                int mx = evento.button.x, my = evento.button.y;
                /* recalcular layout del overlay (igual que en render) */
                int bw2 = editor_w - 40, bx2 = editor_x + 20, by2 = 40;
                int iy2 = by2 + line_h + 22;
                int ly2 = iy2 + line_h + 10 + line_h + 4 + 4;
                int vis2 = (by2 + (alto-80) - ly2 - line_h - 20) / (line_h + 2);
                int x_btn = bx2 + bw2 - 36; /* posicion X del boton [X] */
                for (int i = 0; i < vis2 && (saves_offset + i) < saves_n; i++) {
                    int idx = saves_offset + i;
                    int fy  = ly2 + i * (line_h + 2);
                    SDL_FRect btn = { x_btn, fy - 2, 28, line_h + 4 };
                    if (mx >= btn.x && mx <= btn.x + btn.w &&
                        my >= btn.y && my <= btn.y + btn.h) {
                        char path[128];
                        snprintf(path, sizeof(path), "saves/%s.paed", saves_lista[idx]);
                        remove(path);
                        saves_n = escanear_saves(saves_lista, MAX_SAVES);
                        if (saves_sel >= saves_n) saves_sel = saves_n - 1;
                        if (saves_sel >= 0) {
                            strncpy(prompt_buf, saves_lista[saves_sel], sizeof(prompt_buf)-1);
                            prompt_len = (int)strlen(prompt_buf);
                        } else { prompt_buf[0] = '\0'; prompt_len = 0; }
                        break;
                    }
                }
            }

            /* Click en "Nuevo archivo" (estado vacio) */
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !guardando && !cargando &&
                evento.button.button == SDL_BUTTON_LEFT &&
                btn_nuevo.w > 0) {
                int mx = evento.button.x, my = evento.button.y;
                if (mx >= btn_nuevo.x && mx < btn_nuevo.x + btn_nuevo.w &&
                    my >= btn_nuevo.y && my < btn_nuevo.y + btn_nuevo.h) {
                    audio_sfx_btn();
                    guardando  = 1;
                    prompt_buf[0] = '\0'; prompt_len = 0;
                    saves_n = escanear_saves(saves_lista, MAX_SAVES);
                    saves_sel = -1; saves_offset = 0;
                }
            }

            /* Click en la barra de tabs */
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !guardando && !cargando &&
                evento.button.button == SDL_BUTTON_LEFT &&
                evento.button.y < TAB_H) {
                int mx = evento.button.x, my = evento.button.y;
                int tw = (editor_w - 30) / n_tabs;
                if (tw > 160) tw = 160;
                /* Boton + (nueva tab) */
                int plus_x = editor_x + n_tabs * tw;
                if (mx >= plus_x && mx < plus_x + 30 && n_tabs < MAX_TABS) {
                    audio_sfx_btn();
                    tab_activo = n_tabs;
                    n_tabs++;
                    memset(&tabs[tab_activo], 0, sizeof(EditorPanel));
                    tabs[tab_activo].n_lines = 1;
                    ep = &tabs[tab_activo];
                } else {
                    for (int i = 0; i < n_tabs; i++) {
                        int tx = editor_x + i * tw;
                        /* Click en × (cerrar tab) */
                        if (mx >= tx + tw - 20 && mx < tx + tw - 4 &&
                            my >= 4 && my < TAB_H - 4 && n_tabs > 1) {
                            audio_sfx_btn();
                            for (int j = i; j < n_tabs - 1; j++) tabs[j] = tabs[j + 1];
                            n_tabs--;
                            if (tab_activo >= n_tabs) tab_activo = n_tabs - 1;
                            ep = &tabs[tab_activo];
                            break;
                        }
                        /* Click en el cuerpo de la tab */
                        if (mx >= tx && mx < tx + tw) {
                            audio_sfx_btn();
                            tab_activo = i;
                            ep = &tabs[tab_activo];
                            break;
                        }
                    }
                }
                (void)my;
            }

            /* Click en boton |> ejecutar */
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !guardando && !cargando &&
                evento.button.button == SDL_BUTTON_LEFT) {
                int mx = evento.button.x, my = evento.button.y;
                if (mx >= btn_run.x && mx < btn_run.x + btn_run.w &&
                    my >= btn_run.y && my < btn_run.y + btn_run.h) {
                    audio_sfx_btn();
                    char path[128];
                    snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                    guardar_archivo(ep->buf, ep->n_lines, path);
                    n_out = ejecutar_paed(path, out_buf, OUT_MAX);
                    if (nivel && n_out < OUT_MAX) {
                        if (verificar_nivel(nivel, ep->nombre_arch)) {
                            marcar_completado(nivel_num);
                            strncpy(out_buf[n_out], "[OK] Nivel superado!", 255);
                        } else {
                            strncpy(out_buf[n_out], "[FAIL] Resultado incorrecto.", 255);
                        }
                        n_out++;
                    }
                }
                /* Boton menu ≡: toggle dropdown */
                else if (mx >= btn_menu.x && mx < btn_menu.x + btn_menu.w &&
                         my >= btn_menu.y && my < btn_menu.y + btn_menu.h) {
                    mini_menu = !mini_menu;
                }
                /* Click dentro del dropdown: ejecutar item */
                else if (mini_menu &&
                         mx >= mdrop.x && mx < mdrop.x + mdrop.w &&
                         my >= mdrop.y && my < mdrop.y + mdrop.h) {
                    int item = (my - mdrop.y - 3) / mitem_h;
                    if (item == 0) {
                        /* Guardar */
                        guardando = 1;
                        if (ep->nombre_arch[0] != '\0')
                            strncpy(prompt_buf, ep->nombre_arch, sizeof(prompt_buf)-1);
                        else
                            prompt_buf[0] = '\0';
                        prompt_len = (int)strlen(prompt_buf);
                        saves_n    = escanear_saves(saves_lista, MAX_SAVES);
                        saves_sel  = -1; saves_offset = 0;
                    } else if (item == 1) {
                        /* Cargar */
                        cargando   = 1;
                        prompt_buf[0] = '\0';
                        prompt_len    = 0;
                        saves_n    = escanear_saves(saves_lista, MAX_SAVES);
                        saves_sel  = -1; saves_offset = 0;
                    }
                    mini_menu = 0;
                }
                /* Click fuera del dropdown: cerrar */
                else if (mini_menu) {
                    mini_menu = 0;
                }
            }

            if (evento.type == SDL_EVENT_TEXT_INPUT) {
                if (guardando || cargando) {
                    int add = (int)strlen(evento.text.text);
                    if (prompt_len + add < (int)sizeof(prompt_buf) - 1) {
                        strcat(prompt_buf, evento.text.text);
                        prompt_len += add;
                    }
                } else {
                    int add     = (int)strlen(evento.text.text);
                    int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
                    if (cur_len + add < 511) {
                        memmove(ep->buf[ep->cursor_row] + ep->cursor_col + add,
                                ep->buf[ep->cursor_row] + ep->cursor_col,
                                cur_len - ep->cursor_col + 1);
                        memcpy(ep->buf[ep->cursor_row] + ep->cursor_col, evento.text.text, add);
                        ep->cursor_col += add;
                        ep->dirty = 1;
                    }
                }
            }
        }

        /* ── Render ───────────────────────────────────────────────── */
        SDL_SetRenderDrawColor(renderer, BG_R, 255);
        SDL_RenderClear(renderer);

        /* Layout con tabs */
        int p_w  = editor_w;
        int p_x  = editor_x;
        int p_tx = p_x + GUTTER_W + 12;
        int tw   = (editor_w - 30) / n_tabs;
        if (tw > 160) tw = 160;

        /* Panel consigna (izquierda) */
        if (tiene_consigna) {
            SDL_FRect panel_izq = { 0, 0, editor_x, alto };
            SDL_SetRenderDrawColor(renderer, 10, 18, 10, 255);
            SDL_RenderFillRect(renderer, &panel_izq);
            SDL_Color c_tit = { 0, 230, 80, 255 };
            SDL_Color c_txt = { 0, 190, 70, 255 };
            dibujadoTextoColor(renderer, fuente, cons_titulo, 14, 12, c_tit);
            SDL_SetRenderDrawColor(renderer, 0, 130, 50, 255);
            SDL_RenderLine(renderer, 14, 46, editor_x - 14, 46);
            dibujadoTextoMultilineaColor(renderer, fuente, cons_texto,
                                         14, 56, editor_x - 28, c_txt);
            /* separador vertical */
            SDL_SetRenderDrawColor(renderer, 0, 100, 40, 255);
            SDL_RenderLine(renderer, editor_x, 0, editor_x, alto);
        }

        int vacio = (ep->n_lines == 1 && ep->buf[0][0] == '\0' && ep->nombre_arch[0] == '\0');

        if (!vacio) {

        /* Fondo editor (debajo de los tabs) */
        SDL_FRect editor_bg = { p_x, TAB_H, p_w, edit_h - TAB_H };
        SDL_SetRenderDrawColor(renderer, BG_R, 255);
        SDL_RenderFillRect(renderer, &editor_bg);

        /* Gutter */
        SDL_FRect gutter = { p_x, TAB_H, GUTTER_W, edit_h - TAB_H };
        SDL_SetRenderDrawColor(renderer, GT_R, 255);
        SDL_RenderFillRect(renderer, &gutter);
        SDL_SetRenderDrawColor(renderer, LN_R, 255);
        SDL_RenderLine(renderer, p_x + GUTTER_W, TAB_H, p_x + GUTTER_W, edit_h);

        /* Lineas visibles */
        SDL_FRect edit_clip = { p_x, TAB_H, p_w, edit_h - TAB_H };
        ui_clip(renderer, &edit_clip);

        for (int i = 0; i < vis_lines; i++) {
            int row = ep->offset_row + i;
            if (row >= ep->n_lines) break;
            int y = TAB_H + 8 + i * line_h;

            /* Numero de linea alineado a la derecha del gutter */
            char num[12];
            snprintf(num, sizeof(num), "%d", row + 1);
            int nw; TTF_GetStringSize(fuente, num, 0, &nw, NULL);
            SDL_Surface *sn = TTF_RenderText_Blended(fuente, num, 0, c_num);
            if (sn) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, sn);
                SDL_FRect pos = { p_x + GUTTER_W - nw - 8, y, sn->w, sn->h };
                SDL_RenderTexture(renderer, t, NULL, &pos);
                SDL_DestroySurface(sn); SDL_DestroyTexture(t);
            }

            /* Guias de indentacion */
            {
                static const SDL_Color indent_col[] = {
                    { 90, 25, 25,  60 },   /* nivel 1 — rojo     */
                    { 25, 80, 25,  60 },   /* nivel 2 — verde    */
                    { 85, 75, 10,  60 },   /* nivel 3 — amarillo */
                    { 20, 50, 90,  60 },   /* nivel 4 — azul     */
                    { 70, 20, 75,  60 },   /* nivel 5 — violeta  */
                };
                int sp_w; TTF_GetStringSize(fuente, "    ", 0, &sp_w, NULL);
                int espacios = 0;
                while (ep->buf[row][espacios] == ' ') espacios++;
                int niveles = espacios / 4;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                for (int lv = 0; lv < niveles && lv < 5; lv++) {
                    SDL_Color gc = indent_col[lv];
                    SDL_SetRenderDrawColor(renderer, gc.r, gc.g, gc.b, gc.a);
                    SDL_FRect zona = { p_tx + lv * sp_w, y, sp_w, line_h - 1 };
                    SDL_RenderFillRect(renderer, &zona);
                    /* linea vertical al borde izquierdo de cada nivel */
                    SDL_SetRenderDrawColor(renderer, gc.r, gc.g, gc.b, 130);
                    SDL_RenderLine(renderer, zona.x, y, zona.x, y + line_h - 2);
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            /* Texto de la linea (o placeholder en linea 1 vacia) */
            if (ep->buf[row][0] == '\0' && ep->n_lines == 1) {
                SDL_Surface *s = TTF_RenderText_Blended(fuente,
                                     "Escribe tu codigo aqui...", 0, c_ph);
                if (s) {
                    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                    SDL_FRect pos = { p_tx, y, s->w, s->h };
                    SDL_RenderTexture(renderer, t, NULL, &pos);
                    SDL_DestroySurface(s); SDL_DestroyTexture(t);
                }
            } else if (ep->buf[row][0] != '\0') {
                render_highlighted(renderer, fuente, ep->buf[row], p_tx, y);
            }

            /* Cursor fijo en la fila activa */
            if (row == ep->cursor_row) {
                int cx = p_tx;
                if (ep->cursor_col > 0) {
                    char antes[512];
                    strncpy(antes, ep->buf[ep->cursor_row], ep->cursor_col);
                    antes[ep->cursor_col] = '\0';
                    int tw; TTF_GetStringSize(fuente, antes, 0, &tw, NULL);
                    cx = p_tx + tw;
                }
                SDL_SetRenderDrawColor(renderer, TX_R, 200);
                SDL_FRect cur = { cx, y, 2, line_h - 2 };
                SDL_RenderFillRect(renderer, &cur);
            }
        }
        ui_clip(renderer, NULL);

        } /* end if (!vacio) */

        /* ── Estado vacio (tab sin contenido ni nombre) ─────────────── */
        btn_nuevo.w = 0;   /* resetear cada frame */
        if (vacio) {
            int cx = p_x + p_w / 2;
            int cy = TAB_H + (edit_h - TAB_H) / 2 - 30;

            /* Icono de archivo (pagina con esquina doblada) */
            int iw = 40, ih = 52, fold = 12;
            int ix = cx - iw / 2, iy = cy - ih / 2;
            SDL_SetRenderDrawColor(renderer, 55, 55, 65, 255);
            /* cuerpo */
            SDL_FRect page = { ix, iy + fold, iw, ih - fold };
            SDL_RenderFillRect(renderer, &page);
            SDL_FRect page_top = { ix, iy, iw - fold, fold };
            SDL_RenderFillRect(renderer, &page_top);
            /* esquina doblada */
            SDL_SetRenderDrawColor(renderer, 35, 35, 45, 255);
            SDL_FRect corner = { ix + iw - fold, iy, fold, fold };
            SDL_RenderFillRect(renderer, &corner);
            SDL_SetRenderDrawColor(renderer, 70, 70, 85, 255);
            SDL_RenderLine(renderer, ix + iw - fold, iy, ix + iw - fold, iy + fold);
            SDL_RenderLine(renderer, ix + iw - fold, iy + fold, ix + iw, iy + fold);
            /* lineas de texto dentro del icono */
            SDL_SetRenderDrawColor(renderer, 80, 80, 95, 255);
            for (int li = 0; li < 4; li++)
                SDL_RenderLine(renderer,
                    ix + 6, iy + fold + 8 + li * 9,
                    ix + iw - 6, iy + fold + 8 + li * 9);

            /* Descripcion */
            SDL_Color c_desc = { 80, 80, 95, 255 };
            const char *desc = "No hay ningun archivo abierto";
            int dw; TTF_GetStringSize(fuente, desc, 0, &dw, NULL);
            dibujadoTextoSimple(renderer, fuente, desc, cx - dw / 2, cy + ih / 2 + 14, c_desc);

            /* Boton "Nuevo archivo" */
            int bw = 160, bh = 28;
            int bx = cx - bw / 2, by = cy + ih / 2 + 14 + line_h + 10;
            int mx_h, my_h; ui_mouse(&mx_h, &my_h);
            int bhov = (mx_h >= bx && mx_h < bx + bw && my_h >= by && my_h < by + bh);
            SDL_SetRenderDrawColor(renderer,
                bhov ? 15 : 8, bhov ? 100 : 65, bhov ? 40 : 25, 255);
            SDL_FRect br = { bx, by, bw, bh };
            SDL_RenderFillRect(renderer, &br);
            SDL_SetRenderDrawColor(renderer, 0, 170, 60, 255);
            SDL_RenderRect(renderer, &br);
            const char *blbl = "Nuevo archivo";
            int blw; TTF_GetStringSize(fuente, blbl, 0, &blw, NULL);
            SDL_Color c_blbl = { 0, 220, 80, 255 };
            dibujadoTextoSimple(renderer, fuente, blbl, bx + (bw - blw) / 2, by + 4, c_blbl);

            btn_nuevo = br;   /* guardar para el click handler */
        }

        /* ── Barra de tabs ──────────────────────────────────────────── */
        {
            int mx_t, my_t; ui_mouse(&mx_t, &my_t);

            /* Fondo de la barra */
            SDL_FRect tab_bar = { editor_x, 0, editor_w, TAB_H };
            SDL_SetRenderDrawColor(renderer, 20, 20, 26, 255);
            SDL_RenderFillRect(renderer, &tab_bar);

            for (int i = 0; i < n_tabs; i++) {
                int tx    = editor_x + i * tw;
                int activ = (i == tab_activo);

                /* Fondo de la tab */
                SDL_SetRenderDrawColor(renderer,
                    activ ? 40 : 28,
                    activ ? 40 : 28,
                    activ ? 40 : 32, 255);
                SDL_FRect tr = { tx, 0, tw, TAB_H };
                SDL_RenderFillRect(renderer, &tr);

                /* Borde superior verde en tab activa */
                if (activ) {
                    SDL_SetRenderDrawColor(renderer, 0, 200, 70, 255);
                    SDL_RenderLine(renderer, tx, 0, tx + tw - 1, 0);
                }

                /* Nombre */
                char tab_name[48];
                if (tabs[i].nombre_arch[0] != '\0')
                    snprintf(tab_name, sizeof(tab_name), "%.14s.paed", tabs[i].nombre_arch);
                else
                    snprintf(tab_name, sizeof(tab_name), "Tab %d", i + 1);
                if (tabs[i].dirty)
                    strncat(tab_name, " *", sizeof(tab_name) - strlen(tab_name) - 1);
                SDL_Color c_tn = activ
                    ? (SDL_Color){ 235, 219, 178, 255 }
                    : (SDL_Color){ 100, 100, 100, 255 };
                dibujadoTextoSimple(renderer, fuente, tab_name, tx + 8, 6, c_tn);

                /* Boton × */
                int xbx = tx + tw - 20;
                int xb_hov = (mx_t >= xbx && mx_t < xbx + 16 &&
                              my_t >= 4   && my_t < TAB_H - 4);
                SDL_Color c_xb = xb_hov
                    ? (SDL_Color){ 255, 80, 80, 255 }
                    : (SDL_Color){ 70, 70, 70, 255 };
                dibujadoTextoSimple(renderer, fuente, "x", xbx, 6, c_xb);

                /* Separador vertical entre tabs */
                SDL_SetRenderDrawColor(renderer, 45, 45, 55, 255);
                SDL_RenderLine(renderer, tx + tw - 1, 4, tx + tw - 1, TAB_H - 4);
            }

            /* Boton + (nueva tab) */
            int plus_x = editor_x + n_tabs * tw;
            int plus_hov = (mx_t >= plus_x && mx_t < plus_x + 30 &&
                            my_t >= 0 && my_t < TAB_H);
            SDL_SetRenderDrawColor(renderer,
                plus_hov ? 40 : 25, plus_hov ? 80 : 40, plus_hov ? 40 : 25, 255);
            SDL_FRect plus_r = { plus_x, 4, 26, TAB_H - 8 };
            SDL_RenderFillRect(renderer, &plus_r);
            SDL_Color c_plus = { 0, 200, 70, 255 };
            dibujadoTextoSimple(renderer, fuente, "+", plus_x + 6, 6, c_plus);

            /* Linea inferior de la barra */
            SDL_SetRenderDrawColor(renderer, 0, 130, 45, 255);
            SDL_RenderLine(renderer, editor_x, TAB_H - 1, editor_x + editor_w, TAB_H - 1);
        }

        /* ── Panel de salida ─────────────────────────────────────── */
        SDL_SetRenderDrawColor(renderer, 0, 170, 55, 255);
        SDL_RenderLine(renderer, editor_x, edit_h, editor_x + editor_w, edit_h);
        SDL_SetRenderDrawColor(renderer, 0, 70, 22, 255);
        SDL_RenderLine(renderer, editor_x, edit_h + 1, editor_x + editor_w, edit_h + 1);

        SDL_FRect out_area = { editor_x, edit_h + 2, editor_w, out_h - 2 };
        SDL_SetRenderDrawColor(renderer, 6, 12, 6, 255);
        SDL_RenderFillRect(renderer, &out_area);

        SDL_FRect hd = { editor_x, edit_h + 2, editor_w, line_h + 4 };
        SDL_SetRenderDrawColor(renderer, 0, 38, 13, 255);
        SDL_RenderFillRect(renderer, &hd);
        dibujadoTextoSimple(renderer, fuente, "SALIDA", editor_x + 4, edit_h + 4, c_out_hd);

        /* Boton menu ≡ */
        {
            int mx_h, my_h; ui_mouse(&mx_h, &my_h);
            int hov = (mx_h >= btn_menu.x && mx_h < btn_menu.x + btn_menu.w &&
                       my_h >= btn_menu.y && my_h < btn_menu.y + btn_menu.h);
            SDL_SetRenderDrawColor(renderer, hov || mini_menu ? 50 : 25,
                                             hov || mini_menu ? 80 : 45,
                                             hov || mini_menu ? 50 : 25, 255);
            SDL_RenderFillRect(renderer, &btn_menu);
            SDL_SetRenderDrawColor(renderer, 0, 160, 55, 255);
            SDL_RenderRect(renderer, &btn_menu);
            int lw2; TTF_GetStringSize(fuente, "=", 0, &lw2, NULL);
            dibujadoTextoSimple(renderer, fuente, "=",
                                btn_menu.x + (btn_menu.w - lw2) / 2,
                                btn_menu.y, (SDL_Color){180, 200, 180, 255});
        }

        /* Boton |> RUN al extremo derecho */
        {
            int mx_h, my_h; ui_mouse(&mx_h, &my_h);
            int hover = (mx_h >= btn_run.x && mx_h < btn_run.x + btn_run.w &&
                         my_h >= btn_run.y && my_h < btn_run.y + btn_run.h);
            if (hover)
                SDL_SetRenderDrawColor(renderer, 60, 220, 90, 255);
            else
                SDL_SetRenderDrawColor(renderer, 20, 160, 55, 255);
            SDL_RenderFillRect(renderer, &btn_run);
            SDL_SetRenderDrawColor(renderer, 0, 230, 80, 255);
            SDL_RenderRect(renderer, &btn_run);
            /* centrar texto "|> RUN" dentro del boton */
            int lbl_w; TTF_GetStringSize(fuente, "|> RUN", 0, &lbl_w, NULL);
            int lbl_x = btn_run.x + (btn_run.w - lbl_w) / 2;
            dibujadoTextoSimple(renderer, fuente, "|> RUN",
                                lbl_x, btn_run.y, (SDL_Color){10, 10, 10, 255});
        }

        /* Dropdown mini menu */
        if (mini_menu) {
            int mx_d, my_d; ui_mouse(&mx_d, &my_d);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 10, 18, 10, 240);
            SDL_RenderFillRect(renderer, &mdrop);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 0, 180, 60, 255);
            SDL_RenderRect(renderer, &mdrop);

            for (int i = 0; i < 3; i++) {
                int iy = mdrop.y + 3 + i * mitem_h;
                SDL_FRect item_r = { mdrop.x + 2, iy, mdrop.w - 4, mitem_h };
                int hov_i = (mx_d >= item_r.x && mx_d < item_r.x + item_r.w &&
                             my_d >= item_r.y && my_d < item_r.y + item_r.h);
                if (hov_i) {
                    SDL_SetRenderDrawColor(renderer, 20, 60, 25, 255);
                    SDL_RenderFillRect(renderer, &item_r);
                }
                /* separador entre items */
                if (i > 0) {
                    SDL_SetRenderDrawColor(renderer, 0, 70, 25, 255);
                    SDL_RenderLine(renderer, mdrop.x + 6, iy - 1,
                                                mdrop.x + mdrop.w - 6, iy - 1);
                }
                SDL_Color c_mi = hov_i ? (SDL_Color){200, 240, 200, 255}
                                       : (SDL_Color){142, 192, 124, 255};
                dibujadoTextoSimple(renderer, fuente, menu_items[i],
                                    mdrop.x + 10, iy + 2, c_mi);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 55, 18, 160);
        SDL_RenderLine(renderer, editor_x, edit_h + line_h + 8, editor_x + editor_w, edit_h + line_h + 8);

        ui_clip(renderer, &out_area);
        int oy = edit_h + line_h + 12;
        for (int i = 0; i < n_out; i++) {
            if (oy + line_h > alto) break;
            char ms[300]; snprintf(ms, sizeof(ms), "> %s", out_buf[i]);
            dibujadoTextoSimple(renderer, fuente, ms, editor_x + 10, oy, c_out);
            oy += line_h + 2;
        }
        ui_clip(renderer, NULL);

        /* ── Pantalla de guardado (overlay) ─────────────────────────── */
        if (guardando) {
            /* oscurecer fondo */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_FRect sombra2 = { 0, 0, ancho, alto };
            SDL_RenderFillRect(renderer, &sombra2);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            /* caja centrada */
            int bw = editor_w - 40;  int bh = alto - 80;
            int bx = editor_x + 20;  int by = 40;
            SDL_FRect box = { bx, by, bw, bh };
            SDL_SetRenderDrawColor(renderer, 20, 22, 20, 255);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 0, 170, 55, 255);
            SDL_RenderRect(renderer, &box);

            SDL_Color c_hdr  = {   0, 210,  75, 255 };
            SDL_Color c_lbl  = { 146, 131, 116, 255 };
            SDL_Color c_txt  = { 235, 219, 178, 255 };
            SDL_Color c_sel  = {   0,  80,  30, 255 };
            SDL_Color c_item = { 142, 192, 124, 255 };
            SDL_Color c_hint2= {  80,  73,  69, 255 };

            /* cabecera */
            dibujadoTextoSimple(renderer, fuente, "Guardar archivo",
                                bx + 14, by + 10, c_hdr);
            SDL_SetRenderDrawColor(renderer, 0, 120, 40, 255);
            SDL_RenderLine(renderer, bx + 10, by + line_h + 14,
                                          bx + bw - 10, by + line_h + 14);

            /* campo de nombre (input) */
            int iy = by + line_h + 22;
            dibujadoTextoSimple(renderer, fuente, "Nombre:", bx + 14, iy, c_lbl);
            int lw; TTF_GetStringSize(fuente, "Nombre: ", 0, &lw, NULL);

            SDL_FRect input_bg = { bx + 12, iy - 2, bw - 24, line_h + 4 };
            SDL_SetRenderDrawColor(renderer, 30, 40, 30, 255);
            SDL_RenderFillRect(renderer, &input_bg);
            SDL_SetRenderDrawColor(renderer, 0, 130, 50, 255);
            SDL_RenderRect(renderer, &input_bg);

            dibujadoTextoSimple(renderer, fuente, prompt_buf,
                                bx + 14 + lw, iy, c_txt);
            /* cursor de texto */
            {
                int tw = 0;
                if (prompt_len > 0) TTF_GetStringSize(fuente, prompt_buf, 0, &tw, NULL);
                SDL_SetRenderDrawColor(renderer, 235, 219, 178, 200);
                SDL_FRect pc = { bx + 14 + lw + tw, iy, 2, line_h - 2 };
                SDL_RenderFillRect(renderer, &pc);
            }

            /* separador lista */
            int ly = iy + line_h + 10;
            dibujadoTextoSimple(renderer, fuente, "Archivos guardados:",
                                bx + 14, ly, c_lbl);
            ly += line_h + 4;
            SDL_SetRenderDrawColor(renderer, 0, 80, 25, 255);
            SDL_RenderLine(renderer, bx + 10, ly, bx + bw - 10, ly);
            ly += 4;

            int vis_saves = (by + bh - ly - line_h - 20) / (line_h + 2);
            if (vis_saves < 1) vis_saves = 1;

            if (saves_n == 0) {
                dibujadoTextoSimple(renderer, fuente, "(no hay archivos guardados)",
                                    bx + 20, ly, c_hint2);
            }
            for (int i = 0; i < vis_saves && (saves_offset + i) < saves_n; i++) {
                int idx = saves_offset + i;
                int fy  = ly + i * (line_h + 2);
                if (idx == saves_sel) {
                    SDL_FRect sel_bg = { bx + 10, fy - 2, bw - 20, line_h + 4 };
                    SDL_SetRenderDrawColor(renderer, c_sel.r, c_sel.g, c_sel.b, 255);
                    SDL_RenderFillRect(renderer, &sel_bg);
                }
                char entry[80];
                snprintf(entry, sizeof(entry), "%s.paed", saves_lista[idx]);
                SDL_Color ce = (idx == saves_sel) ? c_txt : c_item;
                dibujadoTextoSimple(renderer, fuente, entry, bx + 20, fy, ce);

                /* boton [X] rojo a la derecha */
                SDL_FRect btn_x = { bx + bw - 36, fy - 2, 28, line_h + 4 };
                SDL_SetRenderDrawColor(renderer, 100, 15, 15, 255);
                SDL_RenderFillRect(renderer, &btn_x);
                SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
                SDL_RenderRect(renderer, &btn_x);
                SDL_Color c_x = { 251, 73, 52, 255 };
                dibujadoTextoSimple(renderer, fuente, "X",
                                    btn_x.x + 8, fy, c_x);
            }

            /* hint inferior */
            dibujadoTextoSimple(renderer, fuente,
                "[↑↓] seleccionar   [Enter] guardar   [ESC] cancelar",
                bx + 14, by + bh - line_h - 8, c_hint2);
        }

        /* ── Pantalla de carga (overlay) ──────────────────────────── */
        if (cargando) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
            SDL_FRect sombra3 = { 0, 0, ancho, alto };
            SDL_RenderFillRect(renderer, &sombra3);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            int bw = editor_w - 40;  int bh = alto - 80;
            int bx = editor_x + 20;  int by = 40;
            SDL_FRect box = { bx, by, bw, bh };
            SDL_SetRenderDrawColor(renderer, 20, 22, 20, 255);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 0, 170, 55, 255);
            SDL_RenderRect(renderer, &box);

            SDL_Color c_hdr  = {   0, 210,  75, 255 };
            SDL_Color c_lbl  = { 146, 131, 116, 255 };
            SDL_Color c_txt  = { 235, 219, 178, 255 };
            SDL_Color c_sel  = {   0,  80,  30, 255 };
            SDL_Color c_item = { 142, 192, 124, 255 };
            SDL_Color c_hint2= {  80,  73,  69, 255 };

            dibujadoTextoSimple(renderer, fuente, "Cargar archivo",
                                bx + 14, by + 10, c_hdr);
            SDL_SetRenderDrawColor(renderer, 0, 120, 40, 255);
            SDL_RenderLine(renderer, bx + 10, by + line_h + 14,
                                          bx + bw - 10, by + line_h + 14);

            /* campo de nombre */
            int iy = by + line_h + 22;
            dibujadoTextoSimple(renderer, fuente, "Nombre:", bx + 14, iy, c_lbl);
            int lw; TTF_GetStringSize(fuente, "Nombre: ", 0, &lw, NULL);

            SDL_FRect input_bg = { bx + 12, iy - 2, bw - 24, line_h + 4 };
            SDL_SetRenderDrawColor(renderer, 30, 40, 30, 255);
            SDL_RenderFillRect(renderer, &input_bg);
            SDL_SetRenderDrawColor(renderer, 0, 130, 50, 255);
            SDL_RenderRect(renderer, &input_bg);
            dibujadoTextoSimple(renderer, fuente, prompt_buf,
                                bx + 14 + lw, iy, c_txt);
            {
                int tw = 0;
                if (prompt_len > 0) TTF_GetStringSize(fuente, prompt_buf, 0, &tw, NULL);
                SDL_SetRenderDrawColor(renderer, 235, 219, 178, 200);
                SDL_FRect pc = { bx + 14 + lw + tw, iy, 2, line_h - 2 };
                SDL_RenderFillRect(renderer, &pc);
            }

            /* lista de archivos */
            int ly = iy + line_h + 10;
            dibujadoTextoSimple(renderer, fuente, "Archivos guardados:",
                                bx + 14, ly, c_lbl);
            ly += line_h + 4;
            SDL_SetRenderDrawColor(renderer, 0, 80, 25, 255);
            SDL_RenderLine(renderer, bx + 10, ly, bx + bw - 10, ly);
            ly += 4;

            int vis_saves = (by + bh - ly - line_h - 20) / (line_h + 2);
            if (vis_saves < 1) vis_saves = 1;

            if (saves_n == 0) {
                dibujadoTextoSimple(renderer, fuente, "(no hay archivos guardados)",
                                    bx + 20, ly, c_hint2);
            }
            for (int i = 0; i < vis_saves && (saves_offset + i) < saves_n; i++) {
                int idx = saves_offset + i;
                int fy  = ly + i * (line_h + 2);
                if (idx == saves_sel) {
                    SDL_FRect sel_bg = { bx + 10, fy - 2, bw - 20, line_h + 4 };
                    SDL_SetRenderDrawColor(renderer, c_sel.r, c_sel.g, c_sel.b, 255);
                    SDL_RenderFillRect(renderer, &sel_bg);
                }
                char entry[80];
                snprintf(entry, sizeof(entry), "%s.paed", saves_lista[idx]);
                SDL_Color ce = (idx == saves_sel) ? c_txt : c_item;
                dibujadoTextoSimple(renderer, fuente, entry, bx + 20, fy, ce);

                /* boton [X] */
                SDL_FRect btn_x = { bx + bw - 36, fy - 2, 28, line_h + 4 };
                SDL_SetRenderDrawColor(renderer, 100, 15, 15, 255);
                SDL_RenderFillRect(renderer, &btn_x);
                SDL_SetRenderDrawColor(renderer, 200, 30, 30, 255);
                SDL_RenderRect(renderer, &btn_x);
                SDL_Color c_x = { 251, 73, 52, 255 };
                dibujadoTextoSimple(renderer, fuente, "X", btn_x.x + 8, fy, c_x);
            }

            dibujadoTextoSimple(renderer, fuente,
                "[↑↓] seleccionar   [Enter] cargar   [ESC] cancelar",
                bx + 14, by + bh - line_h - 8, c_hint2);
        }

        /* ── Dialogo "salir sin guardar?" (overlay) ─────────────── */
        if (confirm_salir) {
            /* oscurecer fondo */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_FRect sombra = { 0, 0, ancho, alto };
            SDL_RenderFillRect(renderer, &sombra);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            /* caja centrada */
            int bw = 380, bh = 80;
            int bx = editor_x + (editor_w - bw) / 2, by = (alto - bh) / 2;
            SDL_FRect box = { bx, by, bw, bh };
            SDL_SetRenderDrawColor(renderer, 28, 24, 22, 255);
            SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, 251, 73, 52, 255);   /* rojo gruvbox */
            SDL_RenderRect(renderer, &box);

            SDL_Color c_pregunta = { 235, 219, 178, 255 };
            SDL_Color c_opciones = { 146, 131, 116, 255 };
            dibujadoTextoSimple(renderer, fuente,
                "Salir sin guardar?",
                bx + 20, by + 12, c_pregunta);
            dibujadoTextoSimple(renderer, fuente,
                "[S / Enter] Si     [N / ESC] No",
                bx + 20, by + 12 + line_h + 4, c_opciones);
        }

        presente(renderer);
        SDL_Delay(16);
    }

    ui_text_input(false);
    return 0;
}

// =============================================================
//  API DE PANEL — Editor Libre integrado en el shell de tabs
//
//  Mismo patrón que panel DOC:
//  - SDL_SetRenderViewport para render sin cambiar coordenadas
//  - Traduccion de mouse en evento (resta area.x / area.y)
//  - Estado en heap (EditorLibreState)
// =============================================================

/* Layout calculado a partir de area.w / area.h — se recalcula cada frame */
typedef struct {
    int ancho, alto;
    int line_h, out_h, edit_h, vis_lines;
    int editor_x, editor_w;
    SDL_FRect btn_run, btn_menu, mdrop;
    int btn_run_w, btn_menu_w;
    int mitem_h, mdrop_w, mdrop_h;
    int tw;   /* ancho de cada tab interna del editor */
} EditorLayout;

static EditorLayout
el_layout(TTF_Font *fuente, SDL_FRect area, int n_tabs_interno)
{
    EditorLayout L;
    L.ancho      = area.w;
    L.alto       = area.h;
    L.line_h     = TTF_GetFontHeight(fuente) + 4;
    L.out_h      = L.line_h * 6 + 24;
    L.edit_h     = L.alto - L.out_h;
    L.vis_lines  = (L.edit_h - TAB_H - 8) / L.line_h;
    if (L.vis_lines < 1) L.vis_lines = 1;
    L.editor_x   = 0;
    L.editor_w   = L.ancho;
    L.btn_run_w  = 72;
    L.btn_run    = (SDL_FRect){ L.editor_x + L.editor_w - L.btn_run_w - 6,
                               L.edit_h + 3, L.btn_run_w, L.line_h };
    L.btn_menu_w = 30;
    L.btn_menu   = (SDL_FRect){ L.btn_run.x - L.btn_menu_w - 6,
                               L.edit_h + 3, L.btn_menu_w, L.line_h };
    L.mitem_h    = L.line_h + 6;
    L.mdrop_w    = 260;
    L.mdrop_h    = L.mitem_h * 2 + 6;
    L.mdrop      = (SDL_FRect){ L.btn_menu.x + L.btn_menu_w - L.mdrop_w,
                               L.edit_h - L.mdrop_h - 4, L.mdrop_w, L.mdrop_h };
    L.tw         = (L.editor_w - 30) / (n_tabs_interno > 0 ? n_tabs_interno : 1);
    if (L.tw > 160) L.tw = 160;
    return L;
}

/* ── struct ────────────────────────────────────────────────────────────────── */
struct EditorLibreState {
    EditorPanel tabs[MAX_TABS];
    int tab_activo;
    int n_tabs;
    char libre_ultimo[64];

    int mini_menu;
    int guardando;
    int cargando;
    int confirm_salir;
    int cerrado;          /* 1 = el usuario presionó F10 → mostrar pantalla de reposo */

    char prompt_buf[64];
    int  prompt_len;

    char saves_lista[EDITOR_MAX_SAVES][64];
    int  saves_n;
    int  saves_sel;
    int  saves_offset;

    char out_buf[EDITOR_OUT_MAX][256];
    int  n_out;

    /* Diagnósticos inline (errores como VSCode) */
    Diagnostico diags[MAX_DIAGS];
    int         n_diags;
    Uint32      last_edit_tick;   /* timestamp del último tipeo        */
    int         diag_pending;     /* 1 = hay cambios sin re-analizar   */

    SDL_FRect btn_nuevo;   /* calculado en dibujar, usado en evento */
};

/* ── editor_libre_crear ────────────────────────────────────────────────────── */
EditorLibreState *
editor_libre_crear(TTF_Font *fuente)
{
    (void)fuente;
    EditorLibreState *s = calloc(1, sizeof(EditorLibreState));
    if (!s) return NULL;
    s->n_tabs     = 1;
    s->tab_activo = 0;
    s->tabs[0].n_lines = 1;
    ui_text_input(true);
    return s;
}

/* ── editor_libre_destruir ─────────────────────────────────────────────────── */
void
editor_libre_destruir(EditorLibreState *s)
{
    ui_text_input(false);
    free(s);
}

/* ── editor_libre_evento ───────────────────────────────────────────────────── */
void
editor_libre_evento(EditorLibreState *s, TTF_Font *fuente, SDL_Event *e, SDL_FRect area)
{
    if (!s) return;

    /* Traducir coordenadas de mouse al espacio local del panel */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN || e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        e->button.x -= area.x;
        e->button.y -= area.y;
    } else if (e->type == SDL_EVENT_MOUSE_MOTION) {
        e->motion.x -= area.x;
        e->motion.y -= area.y;
    }

    /* Si está cerrado, Enter lo reabre */
    if (s->cerrado) {
        if (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_RETURN) {
            s->cerrado = 0;
            ui_text_input(true);
        }
        return;
    }

    EditorLayout L  = el_layout(fuente, area, s->n_tabs);
    EditorPanel *ep = &s->tabs[s->tab_activo];
    int ancho       = L.ancho;
    int alto        = L.alto;

    if (e->type == SDL_EVENT_KEY_DOWN) {
        SDL_Keycode k = e->key.key;

        /* ── Confirmar salida ── */
        if (s->confirm_salir) {
            if (k == SDLK_S || k == SDLK_RETURN) {
                s->confirm_salir = 0;
                s->cerrado = 1;
                ui_text_input(false);
            }
            if (k == SDLK_N || k == SDLK_ESCAPE) s->confirm_salir = 0;
            return;
        }

        /* ── Modo guardar ── */
        if (s->guardando) {
            if (k == SDLK_ESCAPE) {
                s->guardando = 0; s->prompt_len = 0; s->prompt_buf[0] = '\0';
            }
            if (k == SDLK_BACKSPACE && s->prompt_len > 0) {
                s->prompt_buf[--s->prompt_len] = '\0';
                s->saves_sel = -1;
            }
            if (k == SDLK_UP) {
                if (s->saves_sel > 0) s->saves_sel--;
                else s->saves_sel = s->saves_n - 1;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                }
            }
            if (k == SDLK_DOWN) {
                if (s->saves_sel < s->saves_n - 1) s->saves_sel++;
                else s->saves_sel = 0;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                }
            }
            if (s->saves_sel >= 0) {
                if (s->saves_sel < s->saves_offset) s->saves_offset = s->saves_sel;
                if (s->saves_sel >= s->saves_offset + 8) s->saves_offset = s->saves_sel - 7;
            }
            if (k == SDLK_DELETE && s->saves_sel >= 0) {
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", s->saves_lista[s->saves_sel]);
                remove(path);
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                if (s->saves_sel >= s->saves_n) s->saves_sel = s->saves_n - 1;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                } else { s->prompt_buf[0] = '\0'; s->prompt_len = 0; }
            }
            if (k == SDLK_RETURN && s->prompt_len > 0) {
                if (s->prompt_len > 5 && strcmp(s->prompt_buf + s->prompt_len - 5, ".paed") == 0) {
                    s->prompt_buf[s->prompt_len - 5] = '\0';
                    s->prompt_len -= 5;
                }
                strncpy(ep->nombre_arch, s->prompt_buf, sizeof(ep->nombre_arch)-1);
                ep->nombre_arch[sizeof(ep->nombre_arch)-1] = '\0';
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                guardar_archivo(ep->buf, ep->n_lines, path);
                ep->dirty = 0;
                strncpy(s->libre_ultimo, ep->nombre_arch, sizeof(s->libre_ultimo)-1);
                s->guardando = 0; s->prompt_len = 0; s->prompt_buf[0] = '\0';
            }
            return;
        }

        /* ── Modo cargar ── */
        if (s->cargando) {
            if (k == SDLK_ESCAPE) {
                s->cargando = 0; s->prompt_len = 0; s->prompt_buf[0] = '\0';
            }
            if (k == SDLK_BACKSPACE && s->prompt_len > 0) {
                s->prompt_buf[--s->prompt_len] = '\0'; s->saves_sel = -1;
            }
            if (k == SDLK_UP) {
                if (s->saves_sel > 0) s->saves_sel--;
                else s->saves_sel = s->saves_n - 1;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                }
            }
            if (k == SDLK_DOWN) {
                if (s->saves_sel < s->saves_n - 1) s->saves_sel++;
                else s->saves_sel = 0;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                }
            }
            if (s->saves_sel >= 0) {
                if (s->saves_sel < s->saves_offset) s->saves_offset = s->saves_sel;
                if (s->saves_sel >= s->saves_offset + 8) s->saves_offset = s->saves_sel - 7;
            }
            if (k == SDLK_DELETE && s->saves_sel >= 0) {
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", s->saves_lista[s->saves_sel]);
                remove(path);
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                if (s->saves_sel >= s->saves_n) s->saves_sel = s->saves_n - 1;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                } else { s->prompt_buf[0] = '\0'; s->prompt_len = 0; }
            }
            if (k == SDLK_RETURN && s->prompt_len > 0) {
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", s->prompt_buf);
                FILE *fl = fopen(path, "r");
                if (fl) {
                    memset(ep->buf, 0, sizeof(ep->buf));
                    ep->n_lines = 0;
                    char linea[512];
                    while (ep->n_lines < MAX_LINES && fgets(linea, sizeof(linea), fl)) {
                        int l = (int)strlen(linea);
                        if (l > 0 && linea[l-1] == '\n') linea[l-1] = '\0';
                        strncpy(ep->buf[ep->n_lines], linea, 511);
                        ep->buf[ep->n_lines][511] = '\0';
                        ep->n_lines++;
                    }
                    fclose(fl);
                    if (ep->n_lines == 0) ep->n_lines = 1;
                    int pl = (int)strlen(s->prompt_buf);
                    if (pl > 5 && strcmp(s->prompt_buf + pl - 5, ".paed") == 0)
                        s->prompt_buf[pl - 5] = '\0';
                    strncpy(ep->nombre_arch, s->prompt_buf, sizeof(ep->nombre_arch)-1);
                    strncpy(s->libre_ultimo, s->prompt_buf, sizeof(s->libre_ultimo)-1);
                    ep->cursor_row = 0; ep->cursor_col = 0; ep->offset_row = 0;
                    ep->dirty = 0;
                    /* Analizar inmediatamente al cargar para mostrar errores existentes */
                    s->n_diags = analizar_paed_diags(path, s->diags, MAX_DIAGS);
                    s->diag_pending = 0;
                }
                s->cargando = 0; s->prompt_len = 0; s->prompt_buf[0] = '\0';
            }
            return;
        }

        /* ── Editor normal ── */
        if (k == SDLK_F10) {
            audio_sfx_btn();
            if (ep->dirty) s->confirm_salir = 1;
            else { s->cerrado = 1; ui_text_input(false); }
            return;
        }

        /* Ctrl+Tab: siguiente tab interna */
        if (k == SDLK_TAB && (SDL_GetModState() & SDL_KMOD_CTRL)) {
            audio_sfx_btn();
            s->tab_activo = (s->tab_activo + 1) % s->n_tabs;
            ep = &s->tabs[s->tab_activo];
            return;
        }
        /* Tab: insertar 4 espacios */
        if (k == SDLK_TAB) {
            char *ln = ep->buf[ep->cursor_row];
            int len = (int)strlen(ln);
            if (len + 4 < 511) {
                memmove(ln + ep->cursor_col + 4, ln + ep->cursor_col, len - ep->cursor_col + 1);
                memset(ln + ep->cursor_col, ' ', 4);
                ep->cursor_col += 4;
                ep->dirty = 1;
            }
            return;
        }
        /* Ctrl+T: nueva tab interna */
        if (k == SDLK_T && (SDL_GetModState() & SDL_KMOD_CTRL)) {
            if (s->n_tabs < MAX_TABS) {
                audio_sfx_btn();
                s->tab_activo = s->n_tabs++;
                memset(&s->tabs[s->tab_activo], 0, sizeof(EditorPanel));
                s->tabs[s->tab_activo].n_lines = 1;
                ep = &s->tabs[s->tab_activo];
            }
            return;
        }
        /* Ctrl+W: cerrar tab interna */
        if (k == SDLK_W && (SDL_GetModState() & SDL_KMOD_CTRL)) {
            if (s->n_tabs > 1) {
                audio_sfx_btn();
                for (int i = s->tab_activo; i < s->n_tabs - 1; i++)
                    s->tabs[i] = s->tabs[i + 1];
                s->n_tabs--;
                if (s->tab_activo >= s->n_tabs) s->tab_activo = s->n_tabs - 1;
                ep = &s->tabs[s->tab_activo];
            }
            return;
        }

        /* Enter */
        if (k == SDLK_RETURN && ep->n_lines < MAX_LINES) {
            char resto[512];
            strncpy(resto, ep->buf[ep->cursor_row] + ep->cursor_col, 511); resto[511] = '\0';
            ep->buf[ep->cursor_row][ep->cursor_col] = '\0';
            for (int i = ep->n_lines; i > ep->cursor_row + 1; i--)
                memcpy(ep->buf[i], ep->buf[i-1], 512);
            strncpy(ep->buf[ep->cursor_row + 1], resto, 511);
            ep->buf[ep->cursor_row + 1][511] = '\0';
            ep->n_lines++; ep->cursor_row++; ep->cursor_col = 0; ep->dirty = 1;
        }
        /* Backspace */
        if (k == SDLK_BACKSPACE) {
            if (ep->cursor_col > 0) {
                int len = (int)strlen(ep->buf[ep->cursor_row]);
                memmove(ep->buf[ep->cursor_row] + ep->cursor_col - 1,
                        ep->buf[ep->cursor_row] + ep->cursor_col,
                        len - ep->cursor_col + 1);
                ep->cursor_col--; ep->dirty = 1;
            } else if (ep->cursor_row > 0) {
                int prev_len = (int)strlen(ep->buf[ep->cursor_row - 1]);
                int cur_len  = (int)strlen(ep->buf[ep->cursor_row]);
                if (prev_len + cur_len < 511) {
                    strcat(ep->buf[ep->cursor_row - 1], ep->buf[ep->cursor_row]);
                    for (int i = ep->cursor_row; i < ep->n_lines - 1; i++)
                        memcpy(ep->buf[i], ep->buf[i+1], 512);
                    ep->buf[ep->n_lines - 1][0] = '\0';
                    ep->n_lines--; ep->cursor_row--;
                    ep->cursor_col = prev_len; ep->dirty = 1;
                }
            }
        }
        /* Flechas */
        if (k == SDLK_LEFT) {
            if (ep->cursor_col > 0) ep->cursor_col--;
            else if (ep->cursor_row > 0) { ep->cursor_row--; ep->cursor_col = (int)strlen(ep->buf[ep->cursor_row]); }
        }
        if (k == SDLK_RIGHT) {
            int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
            if (ep->cursor_col < cur_len) ep->cursor_col++;
            else if (ep->cursor_row < ep->n_lines - 1) { ep->cursor_row++; ep->cursor_col = 0; }
        }
        if (k == SDLK_UP   && ep->cursor_row > 0) { ep->cursor_row--; int cl=(int)strlen(ep->buf[ep->cursor_row]); if(ep->cursor_col>cl)ep->cursor_col=cl; }
        if (k == SDLK_DOWN && ep->cursor_row < ep->n_lines - 1) { ep->cursor_row++; int cl=(int)strlen(ep->buf[ep->cursor_row]); if(ep->cursor_col>cl)ep->cursor_col=cl; }
        if (k == SDLK_HOME) ep->cursor_col = 0;
        if (k == SDLK_END)  ep->cursor_col = (int)strlen(ep->buf[ep->cursor_row]);

        /* Ctrl+C/X/V */
        if (k == SDLK_C && (e->key.mod & SDL_KMOD_CTRL))
            editor_copiar(ep->buf, ep->cursor_row, ep->cursor_row);
        if (k == SDLK_X && (e->key.mod & SDL_KMOD_CTRL)) {
            editor_cortar(ep->buf, &ep->n_lines, ep->cursor_row, ep->cursor_row,
                          &ep->cursor_row, &ep->cursor_col);
            ep->dirty = 1;
        }
        if (k == SDLK_V && (e->key.mod & SDL_KMOD_CTRL)) {
            editor_pegar(ep->buf, &ep->n_lines, &ep->cursor_row, &ep->cursor_col);
            ep->dirty = 1;
        }

        /* Scroll: mantener cursor visible */
        if (ep->cursor_row < ep->offset_row) ep->offset_row = ep->cursor_row;
        if (ep->cursor_row >= ep->offset_row + L.vis_lines)
            ep->offset_row = ep->cursor_row - L.vis_lines + 1;

        /* F9: guardar */
        if (k == SDLK_F9) {
            if (ep->nombre_arch[0] != '\0') {
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
                guardar_archivo(ep->buf, ep->n_lines, path);
                ep->dirty = 0;
            } else {
                s->guardando = 1; s->prompt_buf[0] = '\0'; s->prompt_len = 0;
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                s->saves_sel = -1; s->saves_offset = 0;
            }
        }
        /* F5: guardar, analizar y ejecutar */
        if (k == SDLK_F5) {
            audio_sfx_btn();
            char path[128];
            snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
            guardar_archivo(ep->buf, ep->n_lines, path);
            /* Análisis inmediato al correr */
            s->n_diags = analizar_paed_diags(path, s->diags, MAX_DIAGS);
            s->diag_pending = 0;
            s->n_out = ejecutar_paed(path, s->out_buf, EDITOR_OUT_MAX);
        }
        (void)ancho;
    }

    /* Click en lista de guardado/cargado: botón [X] */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && (s->guardando || s->cargando) &&
        e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        int bw2 = L.editor_w - 40, bx2 = L.editor_x + 20, by2 = 40;
        int iy2 = by2 + L.line_h + 22;
        int ly2 = iy2 + L.line_h + 10 + L.line_h + 4 + 4;
        int vis2 = (by2 + (alto - 80) - ly2 - L.line_h - 20) / (L.line_h + 2);
        int x_btn = bx2 + bw2 - 36;
        for (int i = 0; i < vis2 && (s->saves_offset + i) < s->saves_n; i++) {
            int idx = s->saves_offset + i;
            int fy  = ly2 + i * (L.line_h + 2);
            SDL_FRect btn = { x_btn, fy - 2, 28, L.line_h + 4 };
            if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                char path[128];
                snprintf(path, sizeof(path), "saves/%s.paed", s->saves_lista[idx]);
                remove(path);
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                if (s->saves_sel >= s->saves_n) s->saves_sel = s->saves_n - 1;
                if (s->saves_sel >= 0) {
                    strncpy(s->prompt_buf, s->saves_lista[s->saves_sel], sizeof(s->prompt_buf)-1);
                    s->prompt_len = (int)strlen(s->prompt_buf);
                } else { s->prompt_buf[0] = '\0'; s->prompt_len = 0; }
                break;
            }
        }
    }

    /* Click en "Nuevo archivo" */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !s->guardando && !s->cargando &&
        e->button.button == SDL_BUTTON_LEFT && s->btn_nuevo.w > 0) {
        int mx = e->button.x, my = e->button.y;
        if (mx >= s->btn_nuevo.x && mx < s->btn_nuevo.x + s->btn_nuevo.w &&
            my >= s->btn_nuevo.y && my < s->btn_nuevo.y + s->btn_nuevo.h) {
            audio_sfx_btn();
            s->guardando = 1; s->prompt_buf[0] = '\0'; s->prompt_len = 0;
            s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
            s->saves_sel = -1; s->saves_offset = 0;
        }
    }

    /* Click en barra de tabs interna */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !s->guardando && !s->cargando &&
        e->button.button == SDL_BUTTON_LEFT && e->button.y < TAB_H) {
        int mx = e->button.x;
        int plus_x = L.editor_x + s->n_tabs * L.tw;
        if (mx >= plus_x && mx < plus_x + 30 && s->n_tabs < MAX_TABS) {
            audio_sfx_btn();
            s->tab_activo = s->n_tabs++;
            memset(&s->tabs[s->tab_activo], 0, sizeof(EditorPanel));
            s->tabs[s->tab_activo].n_lines = 1;
            ep = &s->tabs[s->tab_activo];
        } else {
            for (int i = 0; i < s->n_tabs; i++) {
                int tx = L.editor_x + i * L.tw;
                if (mx >= tx + L.tw - 20 && mx < tx + L.tw - 4 && s->n_tabs > 1) {
                    audio_sfx_btn();
                    for (int j = i; j < s->n_tabs - 1; j++) s->tabs[j] = s->tabs[j + 1];
                    s->n_tabs--;
                    if (s->tab_activo >= s->n_tabs) s->tab_activo = s->n_tabs - 1;
                    ep = &s->tabs[s->tab_activo];
                    break;
                }
                if (mx >= tx && mx < tx + L.tw) {
                    audio_sfx_btn(); s->tab_activo = i; ep = &s->tabs[s->tab_activo]; break;
                }
            }
        }
    }

    /* Click en |> RUN */
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !s->guardando && !s->cargando &&
        e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (mx >= L.btn_run.x && mx < L.btn_run.x + L.btn_run.w &&
            my >= L.btn_run.y && my < L.btn_run.y + L.btn_run.h) {
            audio_sfx_btn();
            char path[128];
            snprintf(path, sizeof(path), "saves/%s.paed", ep->nombre_arch);
            guardar_archivo(ep->buf, ep->n_lines, path);
            s->n_diags = analizar_paed_diags(path, s->diags, MAX_DIAGS);
            s->diag_pending = 0;
            s->n_out = ejecutar_paed(path, s->out_buf, EDITOR_OUT_MAX);
        } else if (mx >= L.btn_menu.x && mx < L.btn_menu.x + L.btn_menu.w &&
                   my >= L.btn_menu.y && my < L.btn_menu.y + L.btn_menu.h) {
            s->mini_menu = !s->mini_menu;
        } else if (s->mini_menu && mx >= L.mdrop.x && mx < L.mdrop.x + L.mdrop.w &&
                   my >= L.mdrop.y && my < L.mdrop.y + L.mdrop.h) {
            int item = (my - L.mdrop.y - 3) / L.mitem_h;
            if (item == 0) {
                s->guardando = 1;
                if (ep->nombre_arch[0] != '\0')
                    strncpy(s->prompt_buf, ep->nombre_arch, sizeof(s->prompt_buf)-1);
                else s->prompt_buf[0] = '\0';
                s->prompt_len = (int)strlen(s->prompt_buf);
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                s->saves_sel = -1; s->saves_offset = 0;
            } else if (item == 1) {
                s->cargando = 1; s->prompt_buf[0] = '\0'; s->prompt_len = 0;
                s->saves_n = escanear_saves(s->saves_lista, EDITOR_MAX_SAVES);
                s->saves_sel = -1; s->saves_offset = 0;
            }
            s->mini_menu = 0;
        } else if (s->mini_menu) {
            s->mini_menu = 0;
        }
    }

    /* Texto */
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (s->guardando || s->cargando) {
            int add = (int)strlen(e->text.text);
            if (s->prompt_len + add < (int)sizeof(s->prompt_buf) - 1) {
                strcat(s->prompt_buf, e->text.text);
                s->prompt_len += add;
            }
        } else {
            int add     = (int)strlen(e->text.text);
            int cur_len = (int)strlen(ep->buf[ep->cursor_row]);
            if (cur_len + add < 511) {
                memmove(ep->buf[ep->cursor_row] + ep->cursor_col + add,
                        ep->buf[ep->cursor_row] + ep->cursor_col,
                        cur_len - ep->cursor_col + 1);
                memcpy(ep->buf[ep->cursor_row] + ep->cursor_col, e->text.text, add);
                ep->cursor_col += add;
                ep->dirty = 1;
                s->diag_pending   = 1;
                s->last_edit_tick = SDL_GetTicks();
            }
        }
    }

    /* Marcar diagnóstico pendiente en cualquier tecla de edición */
    if (e->type == SDL_EVENT_KEY_DOWN && !s->guardando && !s->cargando) {
        SDL_Keycode k2 = e->key.key;
        if (k2 == SDLK_RETURN || k2 == SDLK_BACKSPACE || k2 == SDLK_DELETE) {
            s->diag_pending   = 1;
            s->last_edit_tick = SDL_GetTicks();
        }
    }
}

/* ── editor_libre_dibujar ─────────────────────────────────────────────────── */
void
editor_libre_dibujar(EditorLibreState *s, SDL_Renderer *renderer, TTF_Font *fuente, SDL_FRect area)
{
    if (!s) return;

    /* Viewport: el código de render usa (0,0) como origen; el viewport lo desplaza */
    ui_viewport(renderer, &area);
    ui_clip(renderer, NULL);

    /* Helper: SDL_GetMouseState traducido al espacio local */
    #define LOCAL_MOUSE(mx, my) do { \
        ui_mouse(&(mx), &(my)); \
        (mx) -= area.x; (my) -= area.y; \
    } while(0)

    /* Pantalla de reposo cuando el editor fue cerrado con F10 */
    if (s->cerrado) {
        int ancho = area.w, alto = area.h;
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});
        SDL_Color c = {100, 100, 100, 255};
        dibujadoTextoSimple(renderer, fuente, "Editor cerrado.  Presiona Enter para reabrir.",
                            ancho/2 - 180, alto/2 - 8, c);
        ui_viewport(renderer, NULL);
        ui_clip(renderer, NULL);
        return;
    }

    EditorLayout L  = el_layout(fuente, area, s->n_tabs);
    EditorPanel *ep = &s->tabs[s->tab_activo];
    int ancho       = L.ancho;
    int alto        = L.alto;

    /* ── Análisis automático con debounce de 800ms ───────────────── */
    if (s->diag_pending && (SDL_GetTicks() - s->last_edit_tick) > 800) {
        s->diag_pending = 0;
        /* Guardar en carpeta tmp/ separada de las saves del usuario */
        guardar_archivo(ep->buf, ep->n_lines, "tmp/.diag.paed");
        s->n_diags = analizar_paed_diags("tmp/.diag.paed", s->diags, MAX_DIAGS);
    }

    SDL_Color c_num    = { LN_R, 255 };
    SDL_Color c_ph     = { PH_R, 255 };
    SDL_Color c_out    = {   0, 210,  75, 255 };
    SDL_Color c_out_hd = {   0, 155,  50, 255 };

    /* Fondo general */
    SDL_SetRenderDrawColor(renderer, BG_R, 255);
    SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});

    int p_w  = L.editor_w;
    int p_x  = L.editor_x;
    int p_tx = p_x + GUTTER_W + 12;

    int vacio = (ep->n_lines == 1 && ep->buf[0][0] == '\0' && ep->nombre_arch[0] == '\0');

    if (!vacio) {
        /* Fondo editor */
        SDL_FRect editor_bg = { p_x, TAB_H, p_w, L.edit_h - TAB_H };
        SDL_SetRenderDrawColor(renderer, BG_R, 255);
        SDL_RenderFillRect(renderer, &editor_bg);

        /* Gutter */
        SDL_FRect gutter = { p_x, TAB_H, GUTTER_W, L.edit_h - TAB_H };
        SDL_SetRenderDrawColor(renderer, GT_R, 255);
        SDL_RenderFillRect(renderer, &gutter);
        SDL_SetRenderDrawColor(renderer, LN_R, 255);
        SDL_RenderLine(renderer, p_x + GUTTER_W, TAB_H, p_x + GUTTER_W, L.edit_h);

        /* Lineas con word-wrap */
        SDL_FRect edit_clip = { p_x, TAB_H, p_w, L.edit_h - TAB_H };
        ui_clip(renderer, &edit_clip);

        int wrap_w  = p_w - GUTTER_W - 12;   /* ancho disponible para texto  */
        int vis_y   = TAB_H + 8;             /* posición Y visual actual      */
        int row     = ep->offset_row;

        while (vis_y < L.edit_h && row < ep->n_lines) {
            int y = vis_y;

            /* ── Diagnóstico ── */
            const Diagnostico *diag_linea = NULL;
            for (int d = 0; d < s->n_diags; d++) {
                if (s->diags[d].linea == row + 1) { diag_linea = &s->diags[d]; break; }
            }

            /* Fondo rojo si hay error */
            if (diag_linea) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 180, 30, 30, 45);
                SDL_RenderFillRect(renderer, &(SDL_FRect){p_x + GUTTER_W + 1, y, p_w - GUTTER_W - 1, L.line_h - 1});
                SDL_SetRenderDrawColor(renderer, 200, 50, 50, 220);
                SDL_RenderFillRect(renderer, &(SDL_FRect){p_x + GUTTER_W - 3, y, 3, L.line_h - 1});
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            /* Número de línea (solo en la primera visual-line de cada buffer-line) */
            SDL_Color c_num_row = diag_linea ? (SDL_Color){200, 80, 80, 255} : c_num;
            char num[12]; snprintf(num, sizeof(num), "%d", row + 1);
            int nw; TTF_GetStringSize(fuente, num, 0, &nw, NULL);
            SDL_Surface *sn = TTF_RenderText_Blended(fuente, num, 0, c_num_row);
            if (sn) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, sn);
                SDL_RenderTexture(renderer, t, NULL, &(SDL_FRect){p_x + GUTTER_W - nw - 8, y, sn->w, sn->h});
                SDL_DestroySurface(sn); SDL_DestroyTexture(t);
            }

            /* Guías de indentación */
            {
                static const SDL_Color indent_col[] = {
                    {90,25,25,60},{25,80,25,60},{85,75,10,60},{20,50,90,60},{70,20,75,60}
                };
                int sp_w; TTF_GetStringSize(fuente, "    ", 0, &sp_w, NULL);
                int espacios = 0;
                while (ep->buf[row][espacios] == ' ') espacios++;
                int niveles = espacios / 4;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                for (int lv = 0; lv < niveles && lv < 5; lv++) {
                    SDL_Color gc = indent_col[lv];
                    SDL_SetRenderDrawColor(renderer, gc.r, gc.g, gc.b, gc.a);
                    SDL_FRect zona = { p_tx + lv * sp_w, y, sp_w, L.line_h - 1 };
                    SDL_RenderFillRect(renderer, &zona);
                    SDL_SetRenderDrawColor(renderer, gc.r, gc.g, gc.b, 130);
                    SDL_RenderLine(renderer, zona.x, y, zona.x, y + L.line_h - 2);
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            /* Texto con word-wrap */
            int vlines = 1;
            if (ep->buf[row][0] == '\0' && ep->n_lines == 1) {
                SDL_Surface *sph = TTF_RenderText_Blended(fuente, "Escribe tu codigo aqui...", 0, c_ph);
                if (sph) {
                    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, sph);
                    SDL_RenderTexture(renderer, t, NULL, &(SDL_FRect){p_tx, y, sph->w, sph->h});
                    SDL_DestroySurface(sph); SDL_DestroyTexture(t);
                }
            } else if (ep->buf[row][0] != '\0') {
                /* cursor tracking: solo si es la línea del cursor */
                int cx_cur = -1, cy_cur = -1;
                int cur_col = (row == ep->cursor_row) ? ep->cursor_col : -1;
                vlines = render_highlighted_wrap(renderer, fuente,
                                                 ep->buf[row], p_tx, y,
                                                 wrap_w, L.line_h,
                                                 cur_col,
                                                 (cur_col >= 0) ? &cx_cur : NULL,
                                                 (cur_col >= 0) ? &cy_cur : NULL);
                /* Dibujar cursor */
                if (cur_col >= 0) {
                    int draw_cx = (cx_cur >= 0) ? cx_cur : p_tx;
                    int draw_cy = (cy_cur >= 0) ? cy_cur : y;
                    SDL_SetRenderDrawColor(renderer, TX_R, 200);
                    SDL_RenderFillRect(renderer, &(SDL_FRect){draw_cx, draw_cy, 2, L.line_h - 2});
                }
            } else if (row == ep->cursor_row) {
                /* Línea vacía con cursor */
                SDL_SetRenderDrawColor(renderer, TX_R, 200);
                SDL_RenderFillRect(renderer, &(SDL_FRect){p_tx, y, 2, L.line_h - 2});
            }

            /* Mensaje de error inline */
            if (diag_linea) {
                int last_y = y + (vlines - 1) * L.line_h;
                SDL_Color c_err_msg = {200, 80, 80, 180};
                char label[140]; snprintf(label, sizeof(label), "  %s", diag_linea->msg);
                SDL_Surface *se = TTF_RenderText_Blended(fuente, label, 0, c_err_msg);
                if (se) {
                    SDL_Texture *te = SDL_CreateTextureFromSurface(renderer, se);
                    SDL_SetTextureAlphaMod(te, 180);
                    int msg_x = p_tx + 4;
                    SDL_RenderTexture(renderer, te, NULL, &(SDL_FRect){msg_x, last_y, se->w, se->h});
                    SDL_DestroySurface(se); SDL_DestroyTexture(te);
                }
                /* Subrayado ondulado en la última visual-line */
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 220, 60, 60, 200);
                int uy = last_y + L.line_h - 3;
                for (int ux = p_tx; ux < p_tx + wrap_w - 1; ux += 4) {
                    SDL_RenderLine(renderer, ux,   uy,   ux+2, uy+1);
                    SDL_RenderLine(renderer, ux+2, uy+1, ux+4, uy);
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            vis_y += vlines * L.line_h;
            row++;
        }
        ui_clip(renderer, NULL);
    }

    /* Estado vacio */
    s->btn_nuevo.w = 0;
    if (vacio) {
        int cx = p_x + p_w / 2;
        int cy = TAB_H + (L.edit_h - TAB_H) / 2 - 30;
        int iw = 40, ih = 52, fold = 12;
        int ix = cx - iw/2, iy = cy - ih/2;
        SDL_SetRenderDrawColor(renderer, 55, 55, 65, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){ix, iy+fold, iw, ih-fold});
        SDL_RenderFillRect(renderer, &(SDL_FRect){ix, iy, iw-fold, fold});
        SDL_SetRenderDrawColor(renderer, 35, 35, 45, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){ix+iw-fold, iy, fold, fold});
        SDL_SetRenderDrawColor(renderer, 70, 70, 85, 255);
        SDL_RenderLine(renderer, ix+iw-fold, iy, ix+iw-fold, iy+fold);
        SDL_RenderLine(renderer, ix+iw-fold, iy+fold, ix+iw, iy+fold);
        SDL_SetRenderDrawColor(renderer, 80, 80, 95, 255);
        for (int li = 0; li < 4; li++)
            SDL_RenderLine(renderer, ix+6, iy+fold+8+li*9, ix+iw-6, iy+fold+8+li*9);

        SDL_Color c_desc = {80, 80, 95, 255};
        const char *desc = "No hay ningun archivo abierto";
        int dw; TTF_GetStringSize(fuente, desc, 0, &dw, NULL);
        dibujadoTextoSimple(renderer, fuente, desc, cx - dw/2, cy + ih/2 + 14, c_desc);

        int bw = 160, bh = 28;
        int bx = cx - bw/2, by = cy + ih/2 + 14 + L.line_h + 10;
        int mx_h, my_h; LOCAL_MOUSE(mx_h, my_h);
        int bhov = (mx_h >= bx && mx_h < bx+bw && my_h >= by && my_h < by+bh);
        SDL_SetRenderDrawColor(renderer, bhov?15:8, bhov?100:65, bhov?40:25, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){bx, by, bw, bh});
        SDL_SetRenderDrawColor(renderer, 0, 170, 60, 255);
        SDL_RenderRect(renderer, &(SDL_FRect){bx, by, bw, bh});
        const char *blbl = "Nuevo archivo";
        int blw; TTF_GetStringSize(fuente, blbl, 0, &blw, NULL);
        dibujadoTextoSimple(renderer, fuente, blbl, bx+(bw-blw)/2, by+4, (SDL_Color){0,220,80,255});
        s->btn_nuevo = (SDL_FRect){bx, by, bw, bh};
    }

    /* Barra de tabs interna */
    {
        int mx_t, my_t; LOCAL_MOUSE(mx_t, my_t);
        SDL_FRect tab_bar = { L.editor_x, 0, L.editor_w, TAB_H };
        SDL_SetRenderDrawColor(renderer, 20, 20, 26, 255);
        SDL_RenderFillRect(renderer, &tab_bar);

        for (int i = 0; i < s->n_tabs; i++) {
            int tx = L.editor_x + i * L.tw;
            int activ = (i == s->tab_activo);
            SDL_SetRenderDrawColor(renderer, activ?40:28, activ?40:28, activ?40:32, 255);
            SDL_RenderFillRect(renderer, &(SDL_FRect){tx, 0, L.tw, TAB_H});
            if (activ) {
                SDL_SetRenderDrawColor(renderer, 0, 200, 70, 255);
                SDL_RenderLine(renderer, tx, 0, tx + L.tw - 1, 0);
            }
            char tab_name[48];
            if (s->tabs[i].nombre_arch[0] != '\0')
                snprintf(tab_name, sizeof(tab_name), "%.14s.paed", s->tabs[i].nombre_arch);
            else
                snprintf(tab_name, sizeof(tab_name), "Tab %d", i + 1);
            if (s->tabs[i].dirty)
                strncat(tab_name, " *", sizeof(tab_name) - strlen(tab_name) - 1);
            SDL_Color c_tn = activ ? (SDL_Color){235,219,178,255} : (SDL_Color){100,100,100,255};
            dibujadoTextoSimple(renderer, fuente, tab_name, tx + 8, 6, c_tn);
            int xbx = tx + L.tw - 20;
            int xb_hov = (mx_t >= xbx && mx_t < xbx+16 && my_t >= 4 && my_t < TAB_H-4);
            SDL_Color c_xb = xb_hov ? (SDL_Color){255,80,80,255} : (SDL_Color){70,70,70,255};
            dibujadoTextoSimple(renderer, fuente, "x", xbx, 6, c_xb);
            SDL_SetRenderDrawColor(renderer, 45, 45, 55, 255);
            SDL_RenderLine(renderer, tx+L.tw-1, 4, tx+L.tw-1, TAB_H-4);
        }
        int plus_x = L.editor_x + s->n_tabs * L.tw;
        int plus_hov = (mx_t >= plus_x && mx_t < plus_x+30 && my_t >= 0 && my_t < TAB_H);
        SDL_SetRenderDrawColor(renderer, plus_hov?40:25, plus_hov?80:40, plus_hov?40:25, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){plus_x, 4, 26, TAB_H-8});
        dibujadoTextoSimple(renderer, fuente, "+", plus_x+6, 6, (SDL_Color){0,200,70,255});
        SDL_SetRenderDrawColor(renderer, 0, 130, 45, 255);
        SDL_RenderLine(renderer, L.editor_x, TAB_H-1, L.editor_x+L.editor_w, TAB_H-1);
    }

    /* Panel de salida */
    SDL_SetRenderDrawColor(renderer, 0, 170, 55, 255);
    SDL_RenderLine(renderer, L.editor_x, L.edit_h, L.editor_x+L.editor_w, L.edit_h);
    SDL_SetRenderDrawColor(renderer, 0, 70, 22, 255);
    SDL_RenderLine(renderer, L.editor_x, L.edit_h+1, L.editor_x+L.editor_w, L.edit_h+1);
    SDL_FRect out_area2 = { L.editor_x, L.edit_h+2, L.editor_w, L.out_h-2 };
    SDL_SetRenderDrawColor(renderer, 6, 12, 6, 255);
    SDL_RenderFillRect(renderer, &out_area2);
    SDL_FRect hd = { L.editor_x, L.edit_h+2, L.editor_w, L.line_h+4 };
    SDL_SetRenderDrawColor(renderer, 0, 38, 13, 255);
    SDL_RenderFillRect(renderer, &hd);
    dibujadoTextoSimple(renderer, fuente, "SALIDA", L.editor_x+4, L.edit_h+4, c_out_hd);

    /* Boton ≡ */
    {
        int mx_h, my_h; LOCAL_MOUSE(mx_h, my_h);
        int hov = (mx_h >= L.btn_menu.x && mx_h < L.btn_menu.x+L.btn_menu.w &&
                   my_h >= L.btn_menu.y && my_h < L.btn_menu.y+L.btn_menu.h);
        SDL_SetRenderDrawColor(renderer, hov||s->mini_menu?50:25, hov||s->mini_menu?80:45, hov||s->mini_menu?50:25, 255);
        SDL_RenderFillRect(renderer, &L.btn_menu);
        SDL_SetRenderDrawColor(renderer, 0, 160, 55, 255);
        SDL_RenderRect(renderer, &L.btn_menu);
        int lw2; TTF_GetStringSize(fuente, "=", 0, &lw2, NULL);
        dibujadoTextoSimple(renderer, fuente, "=", L.btn_menu.x+(L.btn_menu.w-lw2)/2, L.btn_menu.y, (SDL_Color){180,200,180,255});
    }
    /* Boton |> RUN */
    {
        int mx_h, my_h; LOCAL_MOUSE(mx_h, my_h);
        int hover = (mx_h >= L.btn_run.x && mx_h < L.btn_run.x+L.btn_run.w &&
                     my_h >= L.btn_run.y && my_h < L.btn_run.y+L.btn_run.h);
        SDL_SetRenderDrawColor(renderer, hover?60:20, hover?220:160, hover?90:55, 255);
        SDL_RenderFillRect(renderer, &L.btn_run);
        SDL_SetRenderDrawColor(renderer, 0, 230, 80, 255);
        SDL_RenderRect(renderer, &L.btn_run);
        int lbl_w; TTF_GetStringSize(fuente, "|> RUN", 0, &lbl_w, NULL);
        dibujadoTextoSimple(renderer, fuente, "|> RUN", L.btn_run.x+(L.btn_run.w-lbl_w)/2, L.btn_run.y, (SDL_Color){10,10,10,255});
    }
    /* Dropdown */
    if (s->mini_menu) {
        int mx_d, my_d; LOCAL_MOUSE(mx_d, my_d);
        const char *menu_items[] = { "Guardar", "Cargar" };
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 10, 18, 10, 240);
        SDL_RenderFillRect(renderer, &L.mdrop);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 180, 60, 255);
        SDL_RenderRect(renderer, &L.mdrop);
        for (int i = 0; i < 2; i++) {
            int iy2 = L.mdrop.y + 3 + i * L.mitem_h;
            SDL_FRect item_r = { L.mdrop.x+2, iy2, L.mdrop.w-4, L.mitem_h };
            int hov_i = (mx_d >= item_r.x && mx_d < item_r.x+item_r.w &&
                         my_d >= item_r.y && my_d < item_r.y+item_r.h);
            if (hov_i) { SDL_SetRenderDrawColor(renderer, 20, 60, 25, 255); SDL_RenderFillRect(renderer, &item_r); }
            if (i > 0) { SDL_SetRenderDrawColor(renderer, 0, 70, 25, 255); SDL_RenderLine(renderer, L.mdrop.x+6, iy2-1, L.mdrop.x+L.mdrop.w-6, iy2-1); }
            SDL_Color c_mi = hov_i ? (SDL_Color){200,240,200,255} : (SDL_Color){142,192,124,255};
            dibujadoTextoSimple(renderer, fuente, menu_items[i], L.mdrop.x+10, iy2+2, c_mi);
        }
    }

    SDL_SetRenderDrawColor(renderer, 0, 55, 18, 160);
    SDL_RenderLine(renderer, L.editor_x, L.edit_h+L.line_h+8, L.editor_x+L.editor_w, L.edit_h+L.line_h+8);

    ui_clip(renderer, &out_area2);
    int oy = L.edit_h + L.line_h + 12;
    for (int i = 0; i < s->n_out; i++) {
        if (oy + L.line_h > alto) break;
        char ms[300]; snprintf(ms, sizeof(ms), "> %s", s->out_buf[i]);
        dibujadoTextoSimple(renderer, fuente, ms, L.editor_x+10, oy, c_out);
        oy += L.line_h + 2;
    }
    ui_clip(renderer, NULL);

    /* Overlay: guardar */
    if (s->guardando) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        int bw = L.editor_w-40, bh = alto-80, bx = L.editor_x+20, by = 40;
        SDL_SetRenderDrawColor(renderer, 20, 22, 20, 255); SDL_RenderFillRect(renderer, &(SDL_FRect){bx,by,bw,bh});
        SDL_SetRenderDrawColor(renderer, 0, 170, 55, 255); SDL_RenderRect(renderer, &(SDL_FRect){bx,by,bw,bh});
        SDL_Color c_hdr={0,210,75,255}, c_lbl={146,131,116,255}, c_txt2={235,219,178,255};
        SDL_Color c_sel2={0,80,30,255}, c_item={142,192,124,255}, c_hint2={80,73,69,255};
        dibujadoTextoSimple(renderer, fuente, "Guardar archivo", bx+14, by+10, c_hdr);
        SDL_SetRenderDrawColor(renderer, 0, 120, 40, 255);
        SDL_RenderLine(renderer, bx+10, by+L.line_h+14, bx+bw-10, by+L.line_h+14);
        int iy = by+L.line_h+22;
        dibujadoTextoSimple(renderer, fuente, "Nombre:", bx+14, iy, c_lbl);
        int lw; TTF_GetStringSize(fuente, "Nombre: ", 0, &lw, NULL);
        SDL_SetRenderDrawColor(renderer, 30, 40, 30, 255); SDL_RenderFillRect(renderer, &(SDL_FRect){bx+12, iy-2, bw-24, L.line_h+4});
        SDL_SetRenderDrawColor(renderer, 0, 130, 50, 255); SDL_RenderRect(renderer, &(SDL_FRect){bx+12, iy-2, bw-24, L.line_h+4});
        dibujadoTextoSimple(renderer, fuente, s->prompt_buf, bx+14+lw, iy, c_txt2);
        { int tw3=0; if(s->prompt_len>0)TTF_GetStringSize(fuente,s->prompt_buf, 0, &tw3, NULL); SDL_SetRenderDrawColor(renderer,235,219,178,200); SDL_RenderFillRect(renderer,&(SDL_FRect){bx+14+lw+tw3,iy,2,L.line_h-2}); }
        int ly = iy+L.line_h+10;
        dibujadoTextoSimple(renderer, fuente, "Archivos guardados:", bx+14, ly, c_lbl);
        ly += L.line_h+4;
        SDL_SetRenderDrawColor(renderer, 0, 80, 25, 255); SDL_RenderLine(renderer, bx+10, ly, bx+bw-10, ly); ly+=4;
        int vis_saves=(by+bh-ly-L.line_h-20)/(L.line_h+2); if(vis_saves<1)vis_saves=1;
        if (s->saves_n == 0) dibujadoTextoSimple(renderer, fuente, "(no hay archivos guardados)", bx+20, ly, c_hint2);
        for (int i = 0; i < vis_saves && (s->saves_offset+i) < s->saves_n; i++) {
            int idx=s->saves_offset+i, fy=ly+i*(L.line_h+2);
            if (idx==s->saves_sel) { SDL_SetRenderDrawColor(renderer,c_sel2.r,c_sel2.g,c_sel2.b,255); SDL_RenderFillRect(renderer,&(SDL_FRect){bx+10,fy-2,bw-20,L.line_h+4}); }
            char entry[80]; snprintf(entry,sizeof(entry),"%s.paed",s->saves_lista[idx]);
            dibujadoTextoSimple(renderer, fuente, entry, bx+20, fy, idx==s->saves_sel?c_txt2:c_item);
            SDL_FRect btn_x={bx+bw-36,fy-2,28,L.line_h+4};
            SDL_SetRenderDrawColor(renderer,100,15,15,255); SDL_RenderFillRect(renderer,&btn_x);
            SDL_SetRenderDrawColor(renderer,200,30,30,255); SDL_RenderRect(renderer,&btn_x);
            dibujadoTextoSimple(renderer, fuente, "X", btn_x.x+8, fy, (SDL_Color){251,73,52,255});
        }
        dibujadoTextoSimple(renderer, fuente, "[↑↓] seleccionar   [Enter] guardar   [ESC] cancelar", bx+14, by+bh-L.line_h-8, c_hint2);
    }

    /* Overlay: cargar */
    if (s->cargando) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        int bw=L.editor_w-40, bh=alto-80, bx=L.editor_x+20, by=40;
        SDL_SetRenderDrawColor(renderer,20,22,20,255); SDL_RenderFillRect(renderer,&(SDL_FRect){bx,by,bw,bh});
        SDL_SetRenderDrawColor(renderer,0,170,55,255); SDL_RenderRect(renderer,&(SDL_FRect){bx,by,bw,bh});
        SDL_Color c_hdr={0,210,75,255},c_lbl={146,131,116,255},c_txt2={235,219,178,255};
        SDL_Color c_sel2={0,80,30,255},c_item={142,192,124,255},c_hint2={80,73,69,255};
        dibujadoTextoSimple(renderer, fuente, "Cargar archivo", bx+14, by+10, c_hdr);
        SDL_SetRenderDrawColor(renderer,0,120,40,255); SDL_RenderLine(renderer, bx+10, by+L.line_h+14, bx+bw-10, by+L.line_h+14);
        int iy=by+L.line_h+22;
        dibujadoTextoSimple(renderer, fuente, "Nombre:", bx+14, iy, c_lbl);
        int lw; TTF_GetStringSize(fuente, "Nombre: ", 0, &lw, NULL);
        SDL_SetRenderDrawColor(renderer,30,40,30,255); SDL_RenderFillRect(renderer,&(SDL_FRect){bx+12,iy-2,bw-24,L.line_h+4});
        SDL_SetRenderDrawColor(renderer,0,130,50,255); SDL_RenderRect(renderer,&(SDL_FRect){bx+12,iy-2,bw-24,L.line_h+4});
        dibujadoTextoSimple(renderer, fuente, s->prompt_buf, bx+14+lw, iy, c_txt2);
        { int tw3=0; if(s->prompt_len>0)TTF_GetStringSize(fuente,s->prompt_buf, 0, &tw3, NULL); SDL_SetRenderDrawColor(renderer,235,219,178,200); SDL_RenderFillRect(renderer,&(SDL_FRect){bx+14+lw+tw3,iy,2,L.line_h-2}); }
        int ly=iy+L.line_h+10;
        dibujadoTextoSimple(renderer, fuente, "Archivos guardados:", bx+14, ly, c_lbl);
        ly+=L.line_h+4;
        SDL_SetRenderDrawColor(renderer,0,80,25,255); SDL_RenderLine(renderer, bx+10, ly, bx+bw-10, ly); ly+=4;
        int vis_saves=(by+bh-ly-L.line_h-20)/(L.line_h+2); if(vis_saves<1)vis_saves=1;
        if (s->saves_n==0) dibujadoTextoSimple(renderer, fuente, "(no hay archivos guardados)", bx+20, ly, c_hint2);
        for (int i=0; i<vis_saves&&(s->saves_offset+i)<s->saves_n; i++) {
            int idx=s->saves_offset+i, fy=ly+i*(L.line_h+2);
            if (idx==s->saves_sel) { SDL_SetRenderDrawColor(renderer,c_sel2.r,c_sel2.g,c_sel2.b,255); SDL_RenderFillRect(renderer,&(SDL_FRect){bx+10,fy-2,bw-20,L.line_h+4}); }
            char entry[80]; snprintf(entry,sizeof(entry),"%s.paed",s->saves_lista[idx]);
            dibujadoTextoSimple(renderer, fuente, entry, bx+20, fy, idx==s->saves_sel?c_txt2:c_item);
            SDL_FRect btn_x={bx+bw-36,fy-2,28,L.line_h+4};
            SDL_SetRenderDrawColor(renderer,100,15,15,255); SDL_RenderFillRect(renderer,&btn_x);
            SDL_SetRenderDrawColor(renderer,200,30,30,255); SDL_RenderRect(renderer,&btn_x);
            dibujadoTextoSimple(renderer, fuente, "X", btn_x.x+8, fy, (SDL_Color){251,73,52,255});
        }
        dibujadoTextoSimple(renderer, fuente, "[↑↓] seleccionar   [Enter] cargar   [ESC] cancelar", bx+14, by+bh-L.line_h-8, c_hint2);
    }

    /* Overlay: confirmar salida */
    if (s->confirm_salir) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        int bw=380, bh=80, bx=L.editor_x+(L.editor_w-bw)/2, by=(alto-bh)/2;
        SDL_SetRenderDrawColor(renderer,28,24,22,255); SDL_RenderFillRect(renderer,&(SDL_FRect){bx,by,bw,bh});
        SDL_SetRenderDrawColor(renderer,251,73,52,255); SDL_RenderRect(renderer,&(SDL_FRect){bx,by,bw,bh});
        dibujadoTextoSimple(renderer, fuente, "Salir sin guardar?", bx+20, by+12, (SDL_Color){235,219,178,255});
        dibujadoTextoSimple(renderer, fuente, "[S / Enter] Si     [N / ESC] No", bx+20, by+12+L.line_h+4, (SDL_Color){146,131,116,255});
    }

    ui_viewport(renderer, NULL);
    ui_clip(renderer, NULL);
    #undef LOCAL_MOUSE
}
