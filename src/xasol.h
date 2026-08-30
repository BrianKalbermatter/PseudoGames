#ifndef XASOL_H
#define XASOL_H

#include "shell.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  xasol — el editorXL de xasol
 *
 *  Un editor modal de pseudocodigo PAED, dibujado directo con SDL. Reemplaza
 *  a editorBim, que era otra cosa: un emulador de terminal que lanzaba un
 *  editor escrito en bash adentro de un pseudo-terminal.
 *
 *  ── Por que se reemplazo ──────────────────────────────────────────────────
 *
 *  editorBim tenia TRES capas entre la tecla y el pixel:
 *
 *      tecla SDL ──> PTY ──> bash (bim.sh) ──> secuencias ANSI ──> parser ──> pixel
 *
 *  Y el parser ANSI solo entendia mover el cursor y borrar: no entendia 'm',
 *  que es el codigo de color. Asi que el resaltado de sintaxis era imposible
 *  de raiz — bash podia emitir los colores, pero se descartaban en el camino.
 *
 *  xasol tiene una:
 *
 *      tecla SDL ──> buffer ──> pixel
 *
 *  ── Lo que se conservo de bim ─────────────────────────────────────────────
 *
 *  La forma, que estaba bien pensada y es la de vi:
 *
 *    - El buffer es un ARRAY DE LINEAS. Abrir un archivo es partirlo por '\n';
 *      guardarlo es pegarlas de vuelta.
 *    - Dos MODOS. En NORMAL las teclas son comandos; en INSERT son texto. Es
 *      lo que permite manejar el editor sin combinaciones imposibles.
 *    - El cursor es un par (fila, columna) que indexa ese array, y despues de
 *      cada movimiento se ACOTA a lo que existe: no puede quedar en una linea
 *      que no hay ni en una columna pasada el fin del texto.
 *    - La barra de estado abajo, con el modo a la izquierda.
 *
 *  ── Lo que se agrego ──────────────────────────────────────────────────────
 *
 *    - GUARDAR. bim cargaba el archivo y nunca lo escribia: editabas y perdias
 *      todo al salir. Era el bug mas caro que tenia.
 *    - Resaltado de sintaxis PAED, que es el motivo de todo esto.
 *    - Undo.
 *    - Scroll: bim dibujaba desde la linea 0 siempre, asi que un archivo mas
 *      largo que la pantalla no se podia ver entero.
 *    - Movimientos de palabra, de linea y de archivo (w, b, 0, $, gg, G).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Cuanto entra. Un ejercicio de la catedra son 100 o 200 lineas, asi que 600
   sobra; el limite existe porque el buffer y las fotos de undo son memoria
   fija y hay que saber cuanta se pide. */
#define XASOL_MAX_LINEAS 600
#define XASOL_MAX_COL    240

/* Cuantos pasos atras se pueden deshacer. Cada uno es una FOTO del buffer
   entero (600 x 240 = 144 KB), que es la forma mas simple de hacer undo y la
   unica que se entiende de una: antes de tocar nada, saco una foto. */
#define XASOL_UNDO_NIVELES 8

/* ── La consola ───────────────────────────────────────────────────────────
 *
 * F10 guarda el archivo, se lo pasa a `paed`, y trae la salida a una barra
 * abajo del editor — como la terminal de VSCode: se abre pegada al piso, se
 * agarra del borde de arriba y se estira.
 *
 * Que GUARDE antes de correr no es un atajo: `paed` lee del disco, asi que
 * correr sin guardar ejecutaria la version anterior del programa. Ver un
 * error que ya arreglaste, o no ver el que acabas de escribir, es peor que
 * no tener el boton.
 */
#define XASOL_CONSOLA_LINEAS  400   /* cuanta salida se guarda              */
#define XASOL_CONSOLA_COL     240
#define XASOL_CONSOLA_MIN_H    60   /* mas baja que esto no se puede dejar  */
#define XASOL_CONSOLA_DEF_H   180   /* con la que abre la primera vez       */

/* Cuanto se le deja correr a un programa antes de cortarlo. Sin esto, un
   MIENTRAS que no termina cuelga PseudoGames entero: la consola espera a que
   el proceso cierre, y el proceso no cierra nunca. */
#define XASOL_TIMEOUT_SEG 5

/* ── La interfaz de panel que pide shell.h ─────────────────────────────── */
void xasol_init        (ShellCtx *ctx, Tab *tab);
void xasol_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e);
void xasol_draw        (ShellCtx *ctx, Tab *tab, SDL_FRect area);
void xasol_cleanup     (ShellCtx *ctx, Tab *tab);

/* ═══════════════════════════════════════════════════════════════════════════
 *  El resaltado — xasol_sintaxis.c
 *
 *  No hay una gramatica de PAED escrita aca adentro, y es a proposito: se le
 *  pregunta al interprete. `paed --tokens` devuelve una linea por token con
 *  linea, columna, largo y ROL, y xasol solo elige un color para cada rol.
 *
 *  El motivo es el mismo que hace que las organizaciones de archivo vivan en
 *  sintaxis.json y no en el C: con dos gramaticas, un dia el editor pinta una
 *  palabra que el interprete rechaza, o al reves. Con una, no puede pasar.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Un token ya ubicado en el buffer. linea y col arrancan en 1, como los
   reporta paed y como los cuenta un humano. */
typedef struct {
    int  linea;
    int  col;
    int  largo;
    char rol[24];
} XasolToken;

#define XASOL_MAX_TOKENS 4000

typedef struct {
    XasolToken tokens[XASOL_MAX_TOKENS];
    int        count;
    /* 0 cuando no se pudo pintar: no hay `paed` instalado, o el archivo tiene
       un error de sintaxis tan temprano que no salio ningun token. El editor
       sigue andando en gris, que es mejor que no abrir. */
    int        valido;

    /* Donde arranca cada linea dentro de tokens[], y cuantos tiene. Es un
       INDICE, y existe por velocidad: el dibujo pregunta "que token cubre
       esta columna" una vez por caracter de la pantalla, y sin indice cada
       pregunta recorre los cuatro mil tokens del archivo. Con 40 lineas de
       80 columnas eso son millones de comparaciones por frame, y se ve:
       es la mitad del parpadeo al escribir.

       Se llena una sola vez, cuando llega la salida de paed, aprovechando
       que viene ordenada por linea. La linea 1 esta en el indice 1: se
       desperdicia el 0 para no restar uno en cada consulta. */
    int        idx_desde[XASOL_MAX_LINEAS + 2];
    int        idx_cuantos[XASOL_MAX_LINEAS + 2];
} XasolResaltado;

/* Corre `paed --tokens` sobre `path` y llena `out`. Devuelve 1 si pinto algo.
   No imprime ni falla ruidosamente: sin paed, el editor es un editor. */
int  xasol_resaltar(const char *path, XasolResaltado *out);

/* El color de un rol de PAED. Los roles y sus colores salen de la misma
   paleta que usa `paed --colores` en la terminal, traducidos de xterm-256 a
   RGB, para que el mismo codigo se vea igual en los dos lados. */
SDL_Color xasol_color_de_rol(const char *rol);

/* El color de lo que TODAVIA no tiene token: los caracteres que acabas de
   tipear y que paed no vio. No es un color aparte a proposito — el porque
   esta arriba de su definicion, en xasol_sintaxis.c. */
SDL_Color xasol_color_sin_token(void);

/* El token que cubre esa posicion, o NULL. Es lo que consulta el dibujante
   caracter por caracter para saber de que color va. */
const XasolToken *xasol_token_en(const XasolResaltado *r, int linea, int col);

/* ── Correr el resaltado con el texto ─────────────────────────────────────
 *
 * El resaltado guarda POSICIONES: "de la linea 11, columna 5, seis
 * caracteres". Cuando editas, el texto se mueve y esas posiciones dejan de
 * apuntar a donde apuntaban — pero los colores nuevos recien llegan cuando
 * vuelve a correr paed, 150 ms despues.
 *
 * En ese hueco, sin corregir nada, pasa esto: apretas Enter y todo lo de
 * abajo se corre una linea, asi que durante ese rato cada linea se pinta con
 * los colores de la de arriba. Se ve como si el archivo entero cambiara de
 * color solo.
 *
 * Estas dos funciones mueven las posiciones junto con el texto. No adivinan
 * colores nuevos: las lineas de abajo no cambiaron su contenido, solo su
 * numero, asi que sus colores siguen siendo los correctos en cuanto se les
 * corrige el numero. Es lo mismo que hace cualquier editor mientras espera
 * a que el lexer termine.
 */

/* Suma `delta` a la linea de todos los tokens de `desde_linea` para abajo.
   +1 cuando se inserta una linea, -1 cuando se borra. */
void xasol_correr_lineas(XasolResaltado *r, int desde_linea, int delta);

/* Suma `delta` a la columna de los tokens de `linea` que estan en
   `desde_col` o mas a la derecha. Si la edicion cae ADENTRO de un token, a
   ese se le estira o encoge el largo en vez de moverlo. */
void xasol_correr_columnas(XasolResaltado *r, int linea, int desde_col, int delta);

/* Le da a los `n` caracteres recien insertados en `col` el color de la
   palabra de al lado: la de la izquierda si `hacia_izq`, la de la derecha si
   no. Se llama DESPUES de xasol_correr_columnas, que ya movio todo de lugar.
 *
 * Correr no alcanza: correr mueve los tokens que ya existian, pero la letra
 * nueva no la cubre ninguno, y hasta que paed vuelva a mirar el archivo se
 * pinta del color de "todavia no se". Tipear al final de una palabra —o sea,
 * tipear— no la parte en dos: la hace mas larga. Eso es lo que esto anota.
 *
 * Devuelve 1 si habia una palabra a la que pegarse. */
int xasol_absorber_insercion(XasolResaltado *r, int linea, int col, int n,
                             int hacia_izq);

#endif /* XASOL_H */
