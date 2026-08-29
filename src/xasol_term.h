#ifndef XASOL_TERM_H
#define XASOL_TERM_H

#include <SDL3/SDL.h>
#include <sys/types.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  Una terminal de verdad adentro del panel — bash, no un panel de salida.
 *
 *  Corre `bash` en un pseudo-terminal, interpreta lo que escribe y lo pinta.
 *  Con eso se puede hacer cualquier cosa que se hace en una terminal: `ls`,
 *  `git`, `paed`, historial, Ctrl+C, autocompletado.
 *
 *  ── Que es un pseudo-terminal ────────────────────────────────────────────
 *
 *  Un programa como bash no escribe "texto": escribe texto MEZCLADO con
 *  ordenes — "poné el cursor en 1,1", "de acá en adelante en verde", "borrá
 *  hasta el final de la línea". Esas ordenes son las SECUENCIAS ANSI, y sin
 *  alguien que las entienda, bash no anda: cree que le habla a una terminal.
 *
 *  El PTY es el par de puntas que hace de terminal falsa. Bash escribe de un
 *  lado; de este lado se lee, se interpreta y se dibuja. Y las teclas van al
 *  reves por el mismo caño.
 *
 *  ── Por que no alcanzaba con pipes ───────────────────────────────────────
 *
 *  Con pipes, bash detecta que no hay terminal y se apaga: sin prompt, sin
 *  colores, sin edicion de linea. El PTY es lo que lo convence de que si la
 *  hay.
 *
 *  ── Lo que se aprendio de editorBim ──────────────────────────────────────
 *
 *  editorBim tambien usaba un PTY, y su parser entendia siete codigos: mover
 *  el cursor y borrar. No entendia 'm', que es el del COLOR, asi que todo
 *  salia gris — y eso fue lo que lo condeno. Aca el color es parte del
 *  modelo desde el principio: cada celda guarda su caracter Y su color.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* El tamaño del buffer de pantalla. Lo que entre de verdad depende del alto
   del panel; esto es el techo. */
#define XT_MAX_ROWS 80
#define XT_MAX_COLS 240

/* Una celda: que caracter hay y de que color. El color se guarda como INDICE
   (0-15 de la paleta ANSI, o -1 para el color normal) y no como RGB, porque
   es lo que manda la terminal y porque asi entra en un byte. */
typedef struct {
    char          ch;
    signed char   fg;      /* -1 = el de por defecto */
    signed char   bg;      /* -1 = el de por defecto */
    unsigned char negrita;
} XTCelda;

typedef struct {
    XTCelda pantalla[XT_MAX_ROWS][XT_MAX_COLS];
    int     rows, cols;

    int  cur_row, cur_col;
    int  cur_visible;

    /* El color con el que se esta escribiendo ahora. Lo cambia cada SGR. */
    signed char   fg, bg;
    unsigned char negrita;

    /* ── El proceso ── */
    pid_t pid;
    int   fd;              /* la punta de aca del PTY; NO bloqueante */
    int   vivo;

    /* ── El parser ──
       Una secuencia llega de a bytes y puede venir partida entre dos
       lecturas, asi que el estado tiene que sobrevivir entre llamadas. */
    int  estado;           /* 0 texto · 1 vio ESC · 2 en CSI · 3 en OSC */
    char buf[64];
    int  buf_len;
    int  interrogacion;    /* CSI ? — los modos privados, como el cursor */
} XasolTerm;

/* Arranca bash en `cwd`. Devuelve 1 si pudo. */
int  xterm_abrir(XasolTerm *t, const char *cwd, int rows, int cols);

/* Trae lo que haya escrito, sin esperar. Se llama en cada frame. */
void xterm_leer(XasolTerm *t);

/* Manda una tecla especial (flechas, Enter, Tab, Ctrl+C…). */
void xterm_tecla(XasolTerm *t, SDL_Keycode k, SDL_Keymod mod);

/* Manda texto ya resuelto por SDL (acentos, ñ, cualquier teclado). */
void xterm_texto(XasolTerm *t, const char *txt);

/* Escribe una linea y la ejecuta. Es lo que usa F10. */
void xterm_comando(XasolTerm *t, const char *cmd);

/* Le avisa el tamaño nuevo. Sin esto bash sigue creyendo que la ventana mide
   lo de antes y parte las lineas donde no va. */
void xterm_redimensionar(XasolTerm *t, int rows, int cols);

/* El color de un indice ANSI. -1 devuelve el de por defecto. */
SDL_Color xterm_color(int idx, int negrita);

void xterm_cerrar(XasolTerm *t);

#endif /* XASOL_TERM_H */
