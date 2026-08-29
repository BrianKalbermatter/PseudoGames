/* ═══════════════════════════════════════════════════════════════════════════
 *  xasol_sintaxis.c — de donde salen los colores
 *
 *  xasol NO sabe pseudocodigo. No tiene una lista de palabras clave, ni un
 *  lexer, ni una gramatica. Le pregunta al interprete:
 *
 *      $ paed --tokens programa.paed
 *      1   1   6   estructura   ACCION
 *      1   8   1   acciones     x
 *      3   3   1   variables    a
 *      3   6   6   tipos        ENTERO
 *
 *  linea, columna, largo, ROL, texto — separados por tabs. Con eso alcanza
 *  para pintar sin volver a leer el archivo.
 *
 *  Por que asi y no un lexer propio: con dos gramaticas, un dia el editor
 *  pinta de verde una palabra que el interprete rechaza. El que edita ve el
 *  color, confia, y el error aparece al correr. Es el bug que mato a la
 *  version anterior del resaltador de PAED, y esta anotado en su parser.c.
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "xasol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── La paleta ──────────────────────────────────────────────────────────────
 *
 *  Son los colores de HELIX, no los de la terminal. PAED tiene dos paletas y
 *  no son la misma:
 *
 *    - `paed --colores` pinta para la terminal, con los nombres de
 *      data/sintaxis.json ("dorado", "aqua") traducidos a xterm-256.
 *    - Helix pinta con los scopes de helix/queries/highlights.scm, y su
 *      reparto esta en helix/tema.toml.ejemplo.
 *
 *  xasol sigue la de Helix, porque es contra la que uno tiene el ojo hecho
 *  al escribir PAED. El reparto, tal como lo declara ese archivo:
 *
 *      ACCION / PROCESO / AMBIENTE ... verde
 *      SI / SINO / ENTONCES .......... oro
 *      MIENTRAS / PARA / REPETIR ..... azul
 *      Y / O / NO / MOD / DIV ........ naranja
 *      lo que definis vos ............ magenta
 *
 *  El ORO es de los CONDICIONALES, no de la estructura. Es el error que
 *  tenia esta tabla antes, y se veia enseguida: `ACCION` y `SI` salian del
 *  mismo color, que es justo lo que el reparto de Helix separa.
 *
 *  El ROJO queda libre a proposito, y esto vale igual adentro del editor:
 *  es el color del error — lo que subraya paed-lsp y lo que sale en la
 *  consola. Si el codigo tambien tuviera rojo, competirian.
 */
typedef struct { const char *rol; SDL_Color color; } RolColor;

/* Los cinco de tema.toml.ejemplo, en RGB. */
#define XA_HX_VERDE   {  0, 255, 102, 255}   /* #00ff66 */
#define XA_HX_ORO     {255, 215,   0, 255}   /* #ffd700 */
#define XA_HX_AZUL    { 51, 119, 255, 255}   /* #3377ff */
#define XA_HX_NARANJA {255, 136,   0, 255}   /* #ff8800 */
#define XA_HX_MAGENTA {255,   0, 255, 255}   /* #ff00ff */

static const RolColor PALETA[] = {
    /* Los cinco que tema.toml.ejemplo nombra explicitamente. */
    { "estructura",     XA_HX_VERDE   },  /* keyword.paed                     */
    { "condicionales",  XA_HX_ORO     },  /* keyword.control.conditional.paed */
    { "bucles",         XA_HX_AZUL    },  /* keyword.control.repeat.paed      */
    { "operadores",     XA_HX_NARANJA },  /* keyword.operator.paed            */

    /* function.paed — "lo que definis vos": el nombre de la ACCION, las
       subacciones, las funciones y los procedimientos propios. */
    { "acciones",       XA_HX_MAGENTA },
    { "funciones",      XA_HX_MAGENTA },
    { "procedimientos", XA_HX_MAGENTA },

    /* Los que highlights.scm delega al tema base de Helix con un scope
       estandar. Se eligen para que no choquen con los cinco de arriba y
       para dejar el rojo libre. */
    { "tipos",          {102, 204, 255, 255} },  /* @type.builtin            */
    { "tipos_usuario",  {102, 204, 255, 255} },  /* @type.builtin            */
    { "entrada",        {255, 221,  85, 255} },  /* @function.builtin        */
    { "salida",         {255, 221,  85, 255} },  /* @function.builtin        */
    { "funciones_builtin", {255, 221, 85, 255} },/* @function.builtin        */
    { "secuencias",     {255, 221,  85, 255} },  /* @function.builtin        */
    { "archivos",       {255, 221,  85, 255} },  /* @function.builtin        */
    { "booleanos",      {197, 134, 192, 255} },  /* @constant.builtin.boolean*/
    { "numeros",        {197, 134, 192, 255} },  /* @constant.numeric        */
    { "strings",        {206, 145, 120, 255} },  /* @constant.character      */
    { "strings_simples",{206, 145, 120, 255} },  /* @constant.character      */
    { "comentario",     {106, 115, 125, 255} },  /* comentario, apagado      */
    { "variables",      {212, 212, 212, 255} },  /* @variable                */
};

/* El texto normal: lo que no es ningun rol conocido, y tambien la puntuacion,
   que a proposito no lleva color. Un ';' de otro color no dice nada nuevo y
   ensucia la linea. */
static const SDL_Color COL_PLANO = {170, 170, 170, 255};

SDL_Color
xasol_color_de_rol(const char *rol)
{
    if (!rol || !rol[0]) return COL_PLANO;

    for (size_t i = 0; i < sizeof(PALETA) / sizeof(PALETA[0]); i++)
        if (strcmp(PALETA[i].rol, rol) == 0) return PALETA[i].color;

    /* Un rol nuevo en sintaxis.json que todavia no esta aca. No es un error:
       se ve plano y se sigue. Que agregar una categoria al lenguaje no pueda
       romper el editor es medio el punto de que la definicion viva alla. */
    return COL_PLANO;
}

/* Arma el indice por linea. Se hace de una pasada porque los tokens estan
   ORDENADOS: todos los de la linea 3 juntos y antes que los de la 4. Eso vale
   tanto cuando llegan de paed (que los emite asi) como despues de correrlos,
   porque correr no cambia el orden relativo. */
static void
reindexar(XasolResaltado *r)
{
    memset(r->idx_desde,   0, sizeof(r->idx_desde));
    memset(r->idx_cuantos, 0, sizeof(r->idx_cuantos));

    for (int i = 0; i < r->count; i++) {
        int l = r->tokens[i].linea;
        if (l < 1 || l > XASOL_MAX_LINEAS) continue;
        if (r->idx_cuantos[l] == 0) r->idx_desde[l] = i;
        r->idx_cuantos[l]++;
    }
}

/* ── Correr paed y leer lo que dice ─────────────────────────────────────── */

int
xasol_resaltar(const char *path, XasolResaltado *out)
{
    if (!out) return 0;
    out->count  = 0;
    out->valido = 0;
    if (!path || !path[0]) return 0;

    /* 2>/dev/null: si el programa tiene un error de sintaxis, paed lo escribe
       por stderr. Aca no interesa — el editor no es el que reporta errores,
       y mezclarlo con los tokens ensuciaria el parseo de abajo. */
    char cmd[700];
    snprintf(cmd, sizeof(cmd), "paed --tokens '%s' 2>/dev/null", path);

    FILE *p = popen(cmd, "r");
    if (!p) return 0;

    char linea[512];
    while (fgets(linea, sizeof(linea), p) && out->count < XASOL_MAX_TOKENS) {
        int  l = 0, c = 0, largo = 0;
        char rol[24] = {0};

        /* "%23[^\t]" lee el rol hasta el tab sin pasarse del campo. El texto
           del token viene despues y no se lee: el editor ya tiene el texto,
           lo saca de su propio buffer. De paed solo necesita DONDE y QUE ES. */
        if (sscanf(linea, "%d\t%d\t%d\t%23[^\t]", &l, &c, &largo, rol) != 4)
            continue;
        if (l <= 0 || c <= 0 || largo <= 0) continue;

        XasolToken *t = &out->tokens[out->count++];
        t->linea = l;
        t->col   = c;
        t->largo = largo;
        snprintf(t->rol, sizeof(t->rol), "%s", rol);
    }

    pclose(p);

    reindexar(out);

    /* Cero tokens es "no se pudo": o no hay paed instalado, o el archivo esta
       vacio. En los dos casos el editor sigue, en gris. */
    out->valido = out->count > 0;
    return out->valido;
}

const XasolToken *
xasol_token_en(const XasolResaltado *r, int linea, int col)
{
    if (!r || !r->valido) return NULL;
    if (linea < 1 || linea > XASOL_MAX_LINEAS) return NULL;

    /* Se miran SOLO los tokens de esa linea, que son un punado, en vez de
       los del archivo entero. Ver el comentario de idx_desde en xasol.h. */
    int desde  = r->idx_desde[linea];
    int cuenta = r->idx_cuantos[linea];

    for (int i = desde; i < desde + cuenta && i < r->count; i++) {
        const XasolToken *t = &r->tokens[i];
        if (col <  t->col)            continue;
        if (col >= t->col + t->largo) continue;
        return t;
    }
    return NULL;
}

/* ── Correr el resaltado con el texto ───────────────────────────────────────
 *
 * El por que esta en xasol.h. Aca solo se mueven numeros.
 */

void
xasol_correr_lineas(XasolResaltado *r, int desde_linea, int delta)
{
    if (!r || !r->valido || delta == 0) return;

    for (int i = 0; i < r->count; i++)
        if (r->tokens[i].linea >= desde_linea)
            r->tokens[i].linea += delta;

    /* El indice apunta por numero de linea, asi que hay que rearmarlo. Son
       unos miles de enteros y pasa solo al insertar o borrar una linea
       entera, no en cada tecla. */
    reindexar(r);
}

void
xasol_correr_columnas(XasolResaltado *r, int linea, int desde_col, int delta)
{
    if (!r || !r->valido || delta == 0) return;
    if (linea < 1 || linea > XASOL_MAX_LINEAS) return;

    /* Solo los de esa linea: el indice ya sabe cuales son. */
    int desde  = r->idx_desde[linea];
    int cuenta = r->idx_cuantos[linea];

    for (int i = desde; i < desde + cuenta && i < r->count; i++) {
        XasolToken *t = &r->tokens[i];

        if (t->col >= desde_col) {
            /* Empieza a la derecha de la edicion: se mueve entero. */
            t->col += delta;
            if (t->col < 1) t->col = 1;
        } else if (t->col + t->largo > desde_col) {
            /* La edicion cayo ADENTRO: la palabra crecio o se achico, no se
               movio. Es el caso de tipear en el medio de un nombre. */
            t->largo += delta;
            if (t->largo < 1) t->largo = 1;
        }
        /* El que termina antes de la edicion no se toca. */
    }

    /* No hace falta reindexar: los tokens siguen en la misma linea. */
}
