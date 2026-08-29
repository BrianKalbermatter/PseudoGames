/* ═══════════════════════════════════════════════════════════════════════════
 *  xasol.c — el editorXL de xasol
 *
 *  Un editor modal para pseudocodigo PAED, dibujado directo con SDL.
 *  El por que de cada decision grande esta en xasol.h; aca esta el como.
 *
 *  El archivo esta en el orden en que conviene leerlo:
 *
 *      1. EL ESTADO      que se guarda entre frames
 *      2. DIBUJAR        dos helpers y nada mas
 *      3. EL BUFFER      abrir, guardar, y las cuatro ediciones basicas
 *      4. UNDO           una foto antes de cada cambio
 *      5. MOVER          el cursor, y la regla de que siempre quede valido
 *      6. LOS MODOS      NORMAL manda comandos, INSERT escribe texto
 *      7. EL PICKER      elegir que archivo abrir
 *      8. PINTAR         numeros de linea, texto con colores, cursor, barra
 *      9. EL PANEL       las cuatro funciones que pide shell.h
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "xasol.h"
#include "xasol_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

/* ═══════════════════════════════════════════════════════════════════════════
 *  1. EL ESTADO
 * ═══════════════════════════════════════════════════════════════════════════ */

#define XASOL_MAX_ARCHIVOS 64
#define XASOL_PICKER_VIS   12    /* cuantos archivos se ven a la vez         */
#define XASOL_GUTTER        5    /* ancho del margen de numeros, en columnas */

/* Cuanto se espera despues de la ultima tecla antes de volver a pedirle los
   colores a paed. Sin esto se lanzaria un proceso por cada letra tipeada;
   con esto, uno recien cuando parás de escribir. 150 ms es mas rapido de lo
   que se nota y mas lento de lo que se tarda entre dos teclas seguidas. */
#define XASOL_DEBOUNCE_MS 150

/* El divisor: la franja de arriba de la consola, de la que se agarra para
   estirarla. Se le dan unos pixeles de gracia para arriba y para abajo, o
   habria que apuntarle a una linea de 4 px. */
#define XASOL_DIVISOR_H    4
#define XASOL_DIVISOR_GRIP 4

typedef enum { XASOL_NORMAL = 0, XASOL_INSERT } XasolModo;

/* En que pantalla esta el panel. bim tenia esto como un `esperando_archivo`
   de 0 o 1; con un enum se lee que es cada estado sin adivinar. */
typedef enum {
    XASOL_PICKER = 0,   /* eligiendo que archivo abrir                       */
    XASOL_NUEVO,        /* tipeando el nombre de uno nuevo                   */
    XASOL_BORRAR,       /* confirmando un borrado                            */
    XASOL_EDITANDO,     /* el editor propiamente dicho                       */
} XasolPantalla;

/* Una foto del buffer, para deshacer. */
typedef struct {
    char lineas[XASOL_MAX_LINEAS][XASOL_MAX_COL];
    int  n_lineas;
    int  cur_fila, cur_col;
} XasolFoto;

typedef struct {
    XasolPantalla pantalla;

    /* ── El picker ── */
    char archivos[XASOL_MAX_ARCHIVOS][64];
    int  n_archivos;
    int  sel;                /* -1 si la lista esta vacia                    */
    int  scroll_picker;
    char nuevo_buf[64];      /* el nombre que se esta tipeando               */
    int  nuevo_len;
    int  saltear_texto;      /* ignora el TEXT_INPUT que dispara la propia N */

    /* ── El buffer ── */
    char lineas[XASOL_MAX_LINEAS][XASOL_MAX_COL];
    int  n_lineas;
    char path[600];          /* ruta absoluta del archivo abierto            */
    char nombre[64];         /* solo el nombre, para la barra y el tab       */
    int  modificado;         /* hay cambios sin guardar                      */

    /* ── El cursor y la vista ── */
    int  cur_fila, cur_col;
    int  scroll;             /* primera linea visible                        */
    XasolModo modo;

    /* Tecla pendiente de un comando de dos: la 'g' de 'gg', la 'd' de 'dd'.
       0 cuando no hay ninguna esperando. */
    char pendiente;

    /* ── Undo ── */
    XasolFoto undo[XASOL_UNDO_NIVELES];
    int  undo_n;             /* cuantas fotos validas hay                    */

    /* ── Resaltado ── */
    XasolResaltado resaltado;
    Uint64 ultimo_cambio;    /* cuando se toco el buffer por ultima vez      */
    int    resaltado_sucio;  /* hay cambios que todavia no se pintaron       */
    char   tmp_path[64];     /* el archivo temporal que lee paed             */

    /* ── La consola (F10) ── */
    int  consola_abierta;
    int  consola_h;          /* su alto en pixeles; lo cambia el arrastre    */
    char consola[XASOL_CONSOLA_LINEAS][XASOL_CONSOLA_COL];
    int  consola_n;
    int  consola_scroll;     /* primera linea visible de la salida           */
    int  consola_codigo;     /* con que termino el programa: 0 = bien        */
    int  corrio;             /* ya se corrio algo al menos una vez           */

    /* ── La terminal ──
     *
     * No es un panel de salida: es bash corriendo de verdad, con su prompt,
     * su historial y sus colores. F10 no lanza un proceso propio — le escribe
     * el comando a la terminal, como si lo hubieras tipeado. Ver xasol_term.h.
     */
    XasolTerm term;
    int       term_lista;    /* ya se abrio bash                             */
    int       term_foco;     /* el teclado es de la terminal, no del editor  */    /* ya se abrio bash                             */

    /* Arrastre del divisor, igual que en VSCode: se agarra el borde de
       arriba de la consola y se estira. */
    int  arrastrando;
    /* El area del ultimo dibujo. Hace falta porque los eventos de mouse
       llegan en coordenadas de la VENTANA, y el panel solo conoce su rect
       cuando lo dibujan: sin guardarlo no hay contra que comparar. */
    SDL_FRect ultima_area;

    /* Mensaje de la barra de estado: "guardado", "no se pudo abrir". */
    char aviso[120];
} XasolState;

/* ═══════════════════════════════════════════════════════════════════════════
 *  2. DIBUJAR
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
xa_text(SDL_Renderer *r, TTF_Font *f, const char *txt, float x, float y, SDL_Color c)
{
    if (!txt || !txt[0]) return;
    SDL_Surface *s = TTF_RenderText_Blended(f, txt, 0, c);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_FRect dst = {x, y, (float)s->w, (float)s->h};
    SDL_RenderTexture(r, t, NULL, &dst);
    SDL_DestroySurface(s);
    SDL_DestroyTexture(t);
}

static void
xa_fill(SDL_Renderer *r, SDL_FRect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

/* El ancho de un caracter. La fuente del shell es monoespaciada, asi que
   medir una "M" alcanza para saber donde cae cualquier columna — y por eso
   el editor puede dibujar caracter por caracter sin medir cada uno. */
static float
xa_char_w(TTF_Font *f)
{
    int w = 0;
    TTF_GetStringSize(f, "M", 0, &w, NULL);
    return w > 0 ? (float)w : 8.0f;
}

static float
xa_line_h(TTF_Font *f)
{
    int h = 0;
    TTF_GetStringSize(f, "M", 0, NULL, &h);
    return h > 0 ? (float)h + 2.0f : 16.0f;
}

/* ── La paleta del editor ── */
static const SDL_Color XA_FONDO     = { 16,  16,  16, 255};
static const SDL_Color XA_GUTTER    = { 70,  70,  70, 255};
static const SDL_Color XA_GUTTER_ON = {200, 200, 200, 255};
static const SDL_Color XA_LINEA_ON  = { 26,  26,  26, 255};
static const SDL_Color XA_BARRA     = { 30,  30,  30, 255};
static const SDL_Color XA_TENUE     = {110, 110, 110, 255};
static const SDL_Color XA_CLARO     = {225, 225, 225, 255};
static const SDL_Color XA_VERDE     = {  0, 210,  90, 255};
static const SDL_Color XA_NARANJA   = {230, 150,  40, 255};
static const SDL_Color XA_ROJO      = {200,  80,  80, 255};

/* ═══════════════════════════════════════════════════════════════════════════
 *  3. EL BUFFER
 *
 *  Un array de lineas, igual que en bim. Abrir es partir el archivo por '\n';
 *  guardar es pegarlas de vuelta con '\n' en el medio.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
buffer_vaciar(XasolState *s)
{
    memset(s->lineas, 0, sizeof(s->lineas));
    s->n_lineas   = 1;      /* nunca cero: un archivo vacio tiene UNA linea
                               vacia, y asi el cursor siempre tiene donde
                               pararse sin un caso especial en cada funcion */
    s->cur_fila   = 0;
    s->cur_col    = 0;
    s->scroll     = 0;
    s->modo       = XASOL_NORMAL;
    s->pendiente  = 0;
    s->modificado = 0;
    s->undo_n     = 0;
}

static int
buffer_abrir(XasolState *s, const char *path, const char *nombre)
{
    buffer_vaciar(s);
    snprintf(s->path,   sizeof(s->path),   "%s", path);
    snprintf(s->nombre, sizeof(s->nombre), "%s", nombre);

    FILE *f = fopen(path, "r");
    if (!f) {
        /* Un archivo que no existe se abre igual, vacio. Es lo que espera
           quien acaba de crearlo desde el picker. */
        snprintf(s->aviso, sizeof(s->aviso), "archivo nuevo");
        return 1;
    }

    char linea[XASOL_MAX_COL * 2];
    int  n = 0;
    while (fgets(linea, sizeof(linea), f) && n < XASOL_MAX_LINEAS) {
        linea[strcspn(linea, "\r\n")] = '\0';
        snprintf(s->lineas[n], XASOL_MAX_COL, "%s", linea);
        n++;
    }
    fclose(f);

    s->n_lineas = n > 0 ? n : 1;
    snprintf(s->aviso, sizeof(s->aviso), "%d lineas", s->n_lineas);
    return 1;
}

static int
buffer_guardar(XasolState *s)
{
    FILE *f = fopen(s->path, "w");
    if (!f) {
        snprintf(s->aviso, sizeof(s->aviso), "NO SE PUDO GUARDAR %s", s->nombre);
        return 0;
    }
    for (int i = 0; i < s->n_lineas; i++)
        fprintf(f, "%s\n", s->lineas[i]);
    fclose(f);

    s->modificado = 0;
    snprintf(s->aviso, sizeof(s->aviso), "guardado  %d lineas", s->n_lineas);
    return 1;
}

/* Marca que el buffer cambio. Todo lo que edita pasa por aca, asi que es el
   unico lugar donde hay que acordarse de pedir el resaltado de nuevo. */
static void
buffer_toco(XasolState *s)
{
    s->modificado      = 1;
    s->resaltado_sucio = 1;
    s->ultimo_cambio   = SDL_GetTicks();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  4. UNDO
 *
 *  Una foto del buffer entero antes de cada cambio. Es la forma mas cara en
 *  memoria y la mas barata de entender: para deshacer, se vuelve a poner la
 *  ultima foto. No hay que razonar sobre operaciones inversas.
 *
 *  Con 600 lineas de 240 caracteres, cada foto son 144 KB, y se guardan ocho.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
undo_sacar_foto(XasolState *s)
{
    /* Si el anillo esta lleno, se tira la mas vieja corriendo todo una
       posicion. Ocho memmove de 144 KB no se notan al tipear, y la
       alternativa (un indice circular) hace que leer el codigo cueste mas
       que el ahorro. */
    if (s->undo_n == XASOL_UNDO_NIVELES) {
        memmove(&s->undo[0], &s->undo[1],
                sizeof(XasolFoto) * (XASOL_UNDO_NIVELES - 1));
        s->undo_n--;
    }

    XasolFoto *f = &s->undo[s->undo_n++];
    memcpy(f->lineas, s->lineas, sizeof(s->lineas));
    f->n_lineas = s->n_lineas;
    f->cur_fila = s->cur_fila;
    f->cur_col  = s->cur_col;
}

static void
undo_deshacer(XasolState *s)
{
    if (s->undo_n == 0) {
        snprintf(s->aviso, sizeof(s->aviso), "nada que deshacer");
        return;
    }

    XasolFoto *f = &s->undo[--s->undo_n];
    memcpy(s->lineas, f->lineas, sizeof(s->lineas));
    s->n_lineas = f->n_lineas;
    s->cur_fila = f->cur_fila;
    s->cur_col  = f->cur_col;

    /* Un undo puede cambiar el buffer entero, asi que no hay corrimiento que
       lo salve: se pide el resaltado de nuevo YA, sin esperar el debounce.
       ultimo_cambio en 0 es lo que significa "sin esperar". */
    buffer_toco(s);
    s->ultimo_cambio = 0;
    snprintf(s->aviso, sizeof(s->aviso), "deshecho");
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  5. MOVER EL CURSOR
 *
 *  La regla: despues de CUALQUIER movimiento o edicion, el cursor se acota a
 *  lo que existe. Es lo que hacia bim con sus `if` sueltos en cada flecha;
 *  aca esta en una sola funcion, porque el dia que se agrega un movimiento
 *  nuevo no hay que acordarse de repetir los cuatro limites.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
cursor_acotar(XasolState *s)
{
    if (s->cur_fila < 0)             s->cur_fila = 0;
    if (s->cur_fila >= s->n_lineas)  s->cur_fila = s->n_lineas - 1;

    int largo = (int)strlen(s->lineas[s->cur_fila]);

    /* En INSERT el cursor puede pararse UNA posicion despues del ultimo
       caracter — ahi es donde se escribe para agregar al final. En NORMAL no:
       se para SOBRE un caracter, que es lo que hace que 'x' tenga algo que
       borrar. Es la diferencia entre los dos modos de vi. */
    int tope = (s->modo == XASOL_INSERT) ? largo : (largo > 0 ? largo - 1 : 0);

    if (s->cur_col < 0)     s->cur_col = 0;
    if (s->cur_col > tope)  s->cur_col = tope;
}

/* Ajusta el scroll para que el cursor quede a la vista. `visibles` es cuantas
   lineas entran en el area de dibujo. */
static void
scroll_seguir_cursor(XasolState *s, int visibles)
{
    if (visibles < 1) visibles = 1;

    if (s->cur_fila < s->scroll)
        s->scroll = s->cur_fila;
    else if (s->cur_fila >= s->scroll + visibles)
        s->scroll = s->cur_fila - visibles + 1;

    if (s->scroll < 0) s->scroll = 0;
}

/* ¿Es parte de una palabra? Lo que decide donde frenan 'w' y 'b'. Los guiones
   bajos cuentan como letra porque en PAED los nombres los llevan
   (`reg_mae`, `cod_mov`), y frenar en el medio de uno seria molesto. */
static int
es_palabra(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* Al principio de la proxima palabra. Salta lo que queda de la actual y
   despues los espacios; si se acaba la linea, sigue en la de abajo. */
static void
mover_palabra_adelante(XasolState *s)
{
    const char *l = s->lineas[s->cur_fila];
    int largo = (int)strlen(l);
    int i = s->cur_col;

    while (i < largo &&  es_palabra(l[i])) i++;   /* salir de la palabra    */
    while (i < largo && !es_palabra(l[i])) i++;   /* saltar los separadores */

    if (i >= largo && s->cur_fila < s->n_lineas - 1) {
        s->cur_fila++;
        s->cur_col = 0;
        return;
    }
    s->cur_col = i;
}

/* Al principio de la palabra anterior. Es el espejo del de arriba: primero
   retrocede sobre los separadores, despues sobre las letras, y queda parado
   en la primera de la palabra. */
static void
mover_palabra_atras(XasolState *s)
{
    if (s->cur_col == 0) {
        if (s->cur_fila > 0) {
            s->cur_fila--;
            s->cur_col = (int)strlen(s->lineas[s->cur_fila]);
        }
        return;
    }

    const char *l = s->lineas[s->cur_fila];
    int i = s->cur_col - 1;

    while (i > 0 && !es_palabra(l[i])) i--;
    while (i > 0 &&  es_palabra(l[i - 1])) i--;

    s->cur_col = i;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  LAS CUATRO EDICIONES
 *
 *  Todo lo que cambia el texto son estas cuatro, y todas sacan foto antes.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
editar_insertar_texto(XasolState *s, const char *txt)
{
    char *l   = s->lineas[s->cur_fila];
    int largo = (int)strlen(l);
    int n     = (int)strlen(txt);
    if (largo + n >= XASOL_MAX_COL) return;   /* la linea no da mas */

    undo_sacar_foto(s);

    /* Correr para la derecha lo que esta del cursor en adelante, y meter el
       texto en el hueco. El +1 del tamaño mueve tambien el '\0'. */
    memmove(l + s->cur_col + n, l + s->cur_col, (size_t)(largo - s->cur_col + 1));
    memcpy(l + s->cur_col, txt, (size_t)n);

    /* El texto de la derecha se corrio n lugares: el resaltado tambien. */
    xasol_correr_columnas(&s->resaltado, s->cur_fila + 1, s->cur_col + 1, n);

    s->cur_col += n;
    buffer_toco(s);
}

/* Enter: parte la linea en dos por donde esta el cursor. Lo de la izquierda
   se queda; lo de la derecha baja a una linea nueva. */
static void
editar_partir_linea(XasolState *s)
{
    if (s->n_lineas >= XASOL_MAX_LINEAS) return;

    undo_sacar_foto(s);

    /* Correr para abajo todas las lineas que estan debajo de la actual, para
       hacerle lugar a la nueva. */
    for (int i = s->n_lineas; i > s->cur_fila + 1; i--)
        memcpy(s->lineas[i], s->lineas[i - 1], XASOL_MAX_COL);

    char *actual = s->lineas[s->cur_fila];
    snprintf(s->lineas[s->cur_fila + 1], XASOL_MAX_COL, "%s", actual + s->cur_col);
    actual[s->cur_col] = '\0';

    /* Todo lo que estaba de la linea siguiente para abajo bajo un lugar. Sin
       esto, durante el debounce cada linea se pinta con los colores de la de
       arriba — que es el "se cambian de color las lineas de abajo". */
    xasol_correr_lineas(&s->resaltado, s->cur_fila + 2, 1);

    s->n_lineas++;
    s->cur_fila++;
    s->cur_col = 0;
    buffer_toco(s);
}

/* Backspace: borra el caracter de la izquierda. Si el cursor esta al
   principio de la linea, no hay nada que borrar ahi: lo que corresponde es
   PEGAR esta linea al final de la anterior. */
static void
editar_borrar_atras(XasolState *s)
{
    if (s->cur_col > 0) {
        undo_sacar_foto(s);
        char *l = s->lineas[s->cur_fila];
        memmove(l + s->cur_col - 1, l + s->cur_col, strlen(l) - s->cur_col + 1);
        xasol_correr_columnas(&s->resaltado, s->cur_fila + 1, s->cur_col + 1, -1);
        s->cur_col--;
        buffer_toco(s);
        return;
    }

    if (s->cur_fila == 0) return;   /* principio del archivo: no hay atras */

    undo_sacar_foto(s);

    char *arriba = s->lineas[s->cur_fila - 1];
    int   corte  = (int)strlen(arriba);   /* ahi va a quedar el cursor */

    /* Pegar, cuidando de no pasarse del largo maximo de una linea. */
    snprintf(arriba + corte, (size_t)(XASOL_MAX_COL - corte), "%s",
             s->lineas[s->cur_fila]);

    for (int i = s->cur_fila; i < s->n_lineas - 1; i++)
        memcpy(s->lineas[i], s->lineas[i + 1], XASOL_MAX_COL);
    memset(s->lineas[s->n_lineas - 1], 0, XASOL_MAX_COL);

    /* Aca no alcanza con correr las lineas: el contenido de esta se PEGO al
       final de la de arriba, asi que ademas se corrieron sus columnas. Dos
       correcciones encadenadas es donde se cuela el error, y unir lineas no
       es algo que se haga diez veces por segundo — se pide el resaltado de
       nuevo y listo. ultimo_cambio en 0 significa "sin esperar el debounce". */
    s->n_lineas--;
    s->cur_fila--;
    s->cur_col = corte;
    buffer_toco(s);
    s->ultimo_cambio = 0;
}

/* 'x': borra el caracter que esta DEBAJO del cursor. */
static void
editar_borrar_char(XasolState *s)
{
    char *l   = s->lineas[s->cur_fila];
    int largo = (int)strlen(l);
    if (s->cur_col >= largo) return;

    undo_sacar_foto(s);
    memmove(l + s->cur_col, l + s->cur_col + 1, (size_t)(largo - s->cur_col));
    xasol_correr_columnas(&s->resaltado, s->cur_fila + 1, s->cur_col + 2, -1);
    buffer_toco(s);
    cursor_acotar(s);
}

/* 'dd': borra la linea entera. */
static void
editar_borrar_linea(XasolState *s)
{
    undo_sacar_foto(s);

    if (s->n_lineas == 1) {
        /* La ultima linea no se borra: se vacia. El buffer nunca queda con
           cero lineas — ver el comentario de buffer_vaciar. */
        s->lineas[0][0] = '\0';
    } else {
        for (int i = s->cur_fila; i < s->n_lineas - 1; i++)
            memcpy(s->lineas[i], s->lineas[i + 1], XASOL_MAX_COL);
        memset(s->lineas[s->n_lineas - 1], 0, XASOL_MAX_COL);
        s->n_lineas--;
    }

    /* Correr las de abajo dejaria los tokens de la linea borrada encimados
       con los de la que ocupa su lugar. Igual que al unir: se pide de nuevo. */
    s->cur_col = 0;
    buffer_toco(s);
    s->ultimo_cambio = 0;
    cursor_acotar(s);
}

/* 'o' y 'O': abren una linea vacia debajo o arriba, y entran a INSERT. */
static void
editar_abrir_linea(XasolState *s, int debajo)
{
    if (s->n_lineas >= XASOL_MAX_LINEAS) return;

    undo_sacar_foto(s);

    int destino = debajo ? s->cur_fila + 1 : s->cur_fila;
    for (int i = s->n_lineas; i > destino; i--)
        memcpy(s->lineas[i], s->lineas[i - 1], XASOL_MAX_COL);
    memset(s->lineas[destino], 0, XASOL_MAX_COL);
    xasol_correr_lineas(&s->resaltado, destino + 1, 1);

    s->n_lineas++;
    s->cur_fila = destino;
    s->cur_col  = 0;
    s->modo     = XASOL_INSERT;
    buffer_toco(s);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  5b. LA CONSOLA
 *
 *  F10 guarda, corre `paed` y trae la salida. Es todo lo que hace: no hay un
 *  interprete adentro del editor ni una copia de las reglas del lenguaje —
 *  por lo mismo que el resaltado se le pide a `paed --tokens`. Lo que ves en
 *  la consola es exactamente lo que verias corriendolo en la terminal.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Abre bash, una sola vez, en la carpeta del proyecto.
 *
 * Ahi y no en otro lado porque es donde estan `saves/`, el Makefile y todo lo
 * demas: los comandos que uno tipea son relativos a eso. */
static void
term_asegurar(XasolState *s, ShellCtx *ctx, int rows, int cols)
{
    if (s->term_lista) {
        xterm_redimensionar(&s->term, rows, cols);
        return;
    }

    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");

    s->term_lista = xterm_abrir(&s->term, cwd, rows, cols);
    if (s->term_lista && ctx) SDL_StartTextInput(ctx->ventana);
}

/* F10: guarda y le pide a la terminal que lo corra.
 *
 * No lanza un proceso propio. Le ESCRIBE el comando a bash, como si lo
 * hubieras tipeado — asi el programa corre en una terminal de verdad, con su
 * entrada y su salida, y un LEER se contesta escribiendo. */
static void
consola_correr(XasolState *s, ShellCtx *ctx)
{
    if (!buffer_guardar(s)) return;

    s->consola_abierta = 1;
    s->corrio          = 1;

    char cmd[700];
    snprintf(cmd, sizeof(cmd), "paed '%s'", s->path);
    xterm_comando(&s->term, cmd);

    if (ctx) SDL_StartTextInput(ctx->ventana);
}

/* Cuanto alto le toca hoy a la consola, ya acotado al area que hay. Se
   calcula en un solo lugar porque lo necesitan el dibujo (para saber donde
   termina el editor) y el mouse (para saber donde esta el divisor). */
static float
consola_alto(const XasolState *s, SDL_FRect area)
{
    if (!s->consola_abierta) return 0;

    float h   = (float)s->consola_h;
    float max = area.h - 120;               /* dejarle aire al editor */
    if (max < XASOL_CONSOLA_MIN_H) max = XASOL_CONSOLA_MIN_H;
    if (h > max) h = max;
    if (h < XASOL_CONSOLA_MIN_H) h = XASOL_CONSOLA_MIN_H;
    return h;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  6. LOS MODOS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void
modo_normal(XasolState *s, ShellCtx *ctx, SDL_Keycode k, SDL_Keymod mod)
{
    /* ── Comandos de dos teclas ──
       'gg' va al principio y 'dd' borra la linea. La primera tecla no hace
       nada: queda anotada en `pendiente`, y la segunda decide. */
    if (s->pendiente == 'g') {
        s->pendiente = 0;
        if (k == SDLK_G) { s->cur_fila = 0; s->cur_col = 0; cursor_acotar(s); }
        return;
    }
    if (s->pendiente == 'd') {
        s->pendiente = 0;
        if (k == SDLK_D) editar_borrar_linea(s);
        return;
    }

    /* Ctrl+S guarda. Va antes que todo lo demas para que la 's' con Ctrl no
       se confunda con una 's' suelta. */
    if ((mod & SDL_KMOD_CTRL) && k == SDLK_S) { buffer_guardar(s); return; }

    switch (k) {
    /* ── Mover ── */
    case SDLK_H: case SDLK_LEFT:  s->cur_col--;  cursor_acotar(s); break;
    case SDLK_L: case SDLK_RIGHT: s->cur_col++;  cursor_acotar(s); break;
    case SDLK_K: case SDLK_UP:    s->cur_fila--; cursor_acotar(s); break;
    case SDLK_J: case SDLK_DOWN:  s->cur_fila++; cursor_acotar(s); break;

    case SDLK_W: mover_palabra_adelante(s); cursor_acotar(s); break;
    case SDLK_B: mover_palabra_atras(s);    cursor_acotar(s); break;

    case SDLK_0: case SDLK_HOME:
        s->cur_col = 0;
        break;
    case SDLK_4:   /* '$' es Shift+4: el simbolo llega como TEXT_INPUT, pero
                      la TECLA es la del 4. Se pide el Shift para no mandar el
                      cursor al final cada vez que se tipea un 4. */
    case SDLK_END:
        if (k == SDLK_END || (mod & SDL_KMOD_SHIFT)) {
            s->cur_col = (int)strlen(s->lineas[s->cur_fila]);
            cursor_acotar(s);
        }
        break;

    case SDLK_G:
        if (mod & SDL_KMOD_SHIFT) {          /* G: al final del archivo */
            s->cur_fila = s->n_lineas - 1;
            cursor_acotar(s);
        } else {
            s->pendiente = 'g';              /* gg: esperar la segunda  */
        }
        break;

    /* ── Entrar a INSERT ── */
    case SDLK_I:
        s->modo = XASOL_INSERT;
        SDL_StartTextInput(ctx->ventana);
        break;
    case SDLK_A:
        s->modo = XASOL_INSERT;
        s->cur_col++;                        /* 'a' escribe DESPUES del cursor */
        cursor_acotar(s);
        SDL_StartTextInput(ctx->ventana);
        break;
    case SDLK_O:
        editar_abrir_linea(s, !(mod & SDL_KMOD_SHIFT));   /* O abre arriba */
        SDL_StartTextInput(ctx->ventana);
        break;

    /* ── Editar ── */
    case SDLK_X: editar_borrar_char(s); break;
    case SDLK_D: s->pendiente = 'd';    break;
    case SDLK_U: undo_deshacer(s);      break;

    /* ── Salir al picker ── */
    case SDLK_Q:
        if (s->modificado) {
            snprintf(s->aviso, sizeof(s->aviso),
                     "hay cambios sin guardar: Ctrl+S para guardar, Shift+Q para salir igual");
            break;
        }
        s->pantalla = XASOL_PICKER;
        break;

    default: break;
    }

    /* Shift+Q sale sin guardar. Se mira aparte del switch porque 'q' y 'Q'
       son la misma tecla y lo unico que las separa es el modificador. */
    if (k == SDLK_Q && (mod & SDL_KMOD_SHIFT))
        s->pantalla = XASOL_PICKER;
}

static void
modo_insert_tecla(XasolState *s, ShellCtx *ctx, SDL_Keycode k)
{
    switch (k) {
    case SDLK_ESCAPE:
        s->modo = XASOL_NORMAL;
        SDL_StopTextInput(ctx->ventana);
        /* Al volver a NORMAL el cursor tiene que quedar SOBRE un caracter, y
           si estaba una posicion despues del ultimo, sobra. */
        cursor_acotar(s);
        break;

    case SDLK_RETURN: case SDLK_KP_ENTER: editar_partir_linea(s); break;
    case SDLK_BACKSPACE:                  editar_borrar_atras(s); break;
    case SDLK_TAB:                        editar_insertar_texto(s, "    "); break;

    case SDLK_LEFT:  s->cur_col--;  cursor_acotar(s); break;
    case SDLK_RIGHT: s->cur_col++;  cursor_acotar(s); break;
    case SDLK_UP:    s->cur_fila--; cursor_acotar(s); break;
    case SDLK_DOWN:  s->cur_fila++; cursor_acotar(s); break;

    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  7. EL PICKER
 * ═══════════════════════════════════════════════════════════════════════════ */

static int
cmp_nombre(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

static void
picker_escanear(XasolState *s)
{
    s->n_archivos    = 0;
    s->sel           = -1;
    s->scroll_picker = 0;

    DIR *d = opendir("saves");
    if (!d) return;

    struct dirent *ent;
    while ((ent = readdir(d)) && s->n_archivos < XASOL_MAX_ARCHIVOS) {
        int len = (int)strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".paed") == 0)
            snprintf(s->archivos[s->n_archivos++], 64, "%s", ent->d_name);
    }
    closedir(d);

    qsort(s->archivos, (size_t)s->n_archivos, sizeof(s->archivos[0]), cmp_nombre);
    if (s->n_archivos > 0) s->sel = 0;
}

/* Arma la ruta absoluta de un archivo de saves/ y lo abre. */
static void
picker_abrir(XasolState *s, Tab *tab, const char *nombre)
{
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");

    char path[600];
    snprintf(path, sizeof(path), "%s/saves/%s", cwd, nombre);

    buffer_abrir(s, path, nombre);
    s->pantalla        = XASOL_EDITANDO;
    s->resaltado_sucio = 1;
    s->ultimo_cambio   = 0;   /* 0 = pintar ya, sin esperar el debounce */

    snprintf(tab->nombre, sizeof(tab->nombre), "xl: %s", nombre);
}

static void
picker_tecla(XasolState *s, ShellCtx *ctx, Tab *tab, SDL_Keycode k)
{
    /* ── Confirmando un borrado ── */
    if (s->pantalla == XASOL_BORRAR) {
        if (k == SDLK_RETURN && s->sel >= 0 && s->sel < s->n_archivos) {
            char cwd[512], path[600];
            if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
            snprintf(path, sizeof(path), "%s/saves/%s", cwd, s->archivos[s->sel]);
            remove(path);
            picker_escanear(s);
        }
        s->pantalla = XASOL_PICKER;
        return;
    }

    /* ── Tipeando el nombre de uno nuevo ── */
    if (s->pantalla == XASOL_NUEVO) {
        if (k == SDLK_ESCAPE) {
            s->pantalla  = XASOL_PICKER;
            s->nuevo_len = 0;
            s->nuevo_buf[0] = '\0';
            SDL_StopTextInput(ctx->ventana);
        } else if (k == SDLK_BACKSPACE && s->nuevo_len > 0) {
            s->nuevo_buf[--s->nuevo_len] = '\0';
        } else if (k == SDLK_RETURN && s->nuevo_len > 0) {
            char cwd[512], nombre[80], path[600];
            if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");
            snprintf(nombre, sizeof(nombre), "%s.paed", s->nuevo_buf);
            snprintf(path,   sizeof(path),   "%s/saves/%s", cwd, nombre);

            FILE *f = fopen(path, "a");   /* "a" lo crea sin pisar uno que haya */
            if (f) fclose(f);

            s->nuevo_len    = 0;
            s->nuevo_buf[0] = '\0';
            SDL_StopTextInput(ctx->ventana);
            picker_abrir(s, tab, nombre);
        }
        return;
    }

    /* ── La lista ── */
    if (k == SDLK_N) {
        s->pantalla      = XASOL_NUEVO;
        s->saltear_texto = 1;   /* la propia N va a llegar como TEXT_INPUT */
        SDL_StartTextInput(ctx->ventana);
        return;
    }

    if (s->n_archivos == 0) return;

    switch (k) {
    case SDLK_UP: case SDLK_K:
        if (s->sel > 0) {
            s->sel--;
            if (s->sel < s->scroll_picker) s->scroll_picker = s->sel;
        }
        break;
    case SDLK_DOWN: case SDLK_J:
        if (s->sel < s->n_archivos - 1) {
            s->sel++;
            if (s->sel >= s->scroll_picker + XASOL_PICKER_VIS)
                s->scroll_picker = s->sel - XASOL_PICKER_VIS + 1;
        }
        break;
    case SDLK_RETURN: case SDLK_KP_ENTER:
        if (s->sel >= 0) picker_abrir(s, tab, s->archivos[s->sel]);
        break;
    case SDLK_D:
        if (s->sel >= 0) s->pantalla = XASOL_BORRAR;
        break;
    default: break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  8. PINTAR
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Vuelca el buffer a un archivo temporal y le pide los colores a paed.
 *
 * Hace falta el temporal porque `paed --tokens` lee un archivo del disco, y
 * lo que se esta editando todavia no se guardo. Guardar el de verdad para
 * poder pintarlo seria escribir en el archivo del usuario sin que lo pida.
 */
static void
refrescar_resaltado(XasolState *s)
{
    FILE *f = fopen(s->tmp_path, "w");
    if (!f) return;
    for (int i = 0; i < s->n_lineas; i++)
        fprintf(f, "%s\n", s->lineas[i]);
    fclose(f);

    xasol_resaltar(s->tmp_path, &s->resaltado);
    s->resaltado_sucio = 0;
}

/* ── La consola, abajo de todo ──────────────────────────────────────────────
 *
 * `area` es la franja que le toca, ya descontada del editor. Lleva:
 *
 *     ┌──────────────────────────────────────────────┐ <- divisor, se arrastra
 *     │ >_ │ paed: linea 12: falta ';'               │
 *     │    │ ...                                     │
 *     └──────────────────────────────────────────────┘
 *
 * El `>_` va en un margen propio a la izquierda, del mismo modo que los
 * numeros de linea en el editor: marca de que panel es lo que estas leyendo
 * sin robarle ancho al texto de cada linea.
 */
/* ── La terminal, abajo de todo ──────────────────────────────────────────────
 *
 *     ┌──────────────────────────────────────────────┐ <- divisor, se arrastra
 *     │ >_ │ brian@wsl:~/PseudoGames$ paed Hola.paed │
 *     │    │ Introduzca un numero: 7                 │
 *     └──────────────────────────────────────────────┘
 *
 * El `>_` va en un margen propio a la izquierda, del mismo modo que los
 * numeros de linea en el editor.
 */
static void
pintar_consola(XasolState *s, ShellCtx *ctx, SDL_FRect area)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    float cw = xa_char_w(f);
    float lh = xa_line_h(f);

    /* El divisor. Verde cuando lo estas arrastrando; naranja cuando la
       terminal tiene el teclado, que es la unica forma de saber donde van a
       ir las teclas antes de apretarlas. */
    SDL_Color col_div = s->arrastrando ? XA_VERDE
                      : (s->term_foco  ? XA_NARANJA : (SDL_Color){55, 55, 55, 255});
    xa_fill(r, (SDL_FRect){area.x, area.y, area.w, XASOL_DIVISOR_H}, col_div);

    float y0 = area.y + XASOL_DIVISOR_H;
    float h0 = area.h - XASOL_DIVISOR_H;
    xa_fill(r, (SDL_FRect){area.x, y0, area.w, h0}, (SDL_Color){12, 12, 12, 255});

    /* ── El margen con el simbolo ── */
    float margen_w = cw * 4;
    xa_fill(r, (SDL_FRect){area.x, y0, margen_w, h0}, (SDL_Color){20, 20, 20, 255});
    SDL_SetRenderDrawColor(r, 45, 45, 45, 255);
    SDL_RenderLine(r, area.x + margen_w, y0, area.x + margen_w, y0 + h0);
    xa_text(r, f, ">_", area.x + cw * 0.6f, y0 + 4,
            s->term_foco ? XA_NARANJA : XA_VERDE);

    /* ── Abrir bash del tamaño que entra ── */
    float x_txt = area.x + margen_w + 8;
    int   filas = (int)((h0 - 6) / lh);
    int   cols  = (int)((area.w - margen_w - 16) / cw);
    if (filas < 4)  filas = 4;
    if (cols  < 20) cols  = 20;

    term_asegurar(s, ctx, filas, cols);

    if (!s->term.vivo) {
        xa_text(r, f, "bash se cerro. F12 para volver al editor.",
                x_txt, y0 + 4, XA_TENUE);
        return;
    }

    /* ── La pantalla ──
     *
     * Por TRAMOS de un mismo color, igual que el editor y por el mismo
     * motivo: una textura por caracter son miles por frame. */
    for (int fila = 0; fila < s->term.rows && fila < filas; fila++) {
        float y = y0 + 4 + fila * lh;
        const XTCelda *linea = s->term.pantalla[fila];

        int c = 0;
        while (c < s->term.cols && c < cols) {
            /* Saltear los espacios sin color: no hay nada que dibujar. */
            if (linea[c].ch == ' ' && linea[c].bg < 0) { c++; continue; }

            signed char fg = linea[c].fg, bg = linea[c].bg;
            unsigned char ng = linea[c].negrita;

            int fin_tr = c + 1;
            while (fin_tr < s->term.cols && fin_tr < cols &&
                   linea[fin_tr].fg == fg && linea[fin_tr].bg == bg &&
                   linea[fin_tr].negrita == ng &&
                   !(linea[fin_tr].ch == ' ' && bg < 0))
                fin_tr++;

            char tramo[XT_MAX_COLS + 1];
            int  n = fin_tr - c;
            for (int i = 0; i < n; i++) tramo[i] = linea[c + i].ch;
            tramo[n] = '\0';

            if (bg >= 0)
                xa_fill(r, (SDL_FRect){x_txt + c * cw, y, n * cw, lh},
                        xterm_color(bg, 0));

            xa_text(r, f, tramo, x_txt + c * cw, y, xterm_color(fg, ng));
            c = fin_tr;
        }
    }

    /* ── El cursor ──
     * Lleno cuando la terminal tiene el teclado, hueco cuando no. Es la misma
     * convencion que usa cualquier terminal cuando pierde el foco. */
    if (s->term.cur_visible && s->term.cur_row < filas) {
        float cx = x_txt + s->term.cur_col * cw;
        float cy = y0 + 4 + s->term.cur_row * lh;

        if (s->term_foco) {
            if ((SDL_GetTicks() / 500) % 2 == 0)
                xa_fill(r, (SDL_FRect){cx, cy, cw, lh}, XA_NARANJA);
        } else {
            SDL_SetRenderDrawColor(r, XA_TENUE.r, XA_TENUE.g, XA_TENUE.b, 255);
            SDL_FRect hueco = {cx, cy, cw, lh};
            SDL_RenderRect(r, &hueco);
        }
    }

    /* La ayuda, a la derecha del divisor. */
    const char *ayuda = s->term_foco ? "F12 volver al editor"
                                     : "F12 ir a la terminal   F10 correr";
    int aw; TTF_GetStringSize(f, ayuda, 0, &aw, NULL);
    xa_text(r, f, ayuda, area.x + area.w - aw - 10, y0 + 4, XA_TENUE);
}

static void
pintar_editor(XasolState *s, ShellCtx *ctx, SDL_FRect area)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    float cw = xa_char_w(f);
    float lh = xa_line_h(f);

    /* La consola se lleva su franja de abajo, y lo que queda es el editor.
       Se calcula antes que nada porque de esto depende cuantas lineas de
       codigo entran, y por lo tanto donde tiene que estar el scroll. */
    float con_h = consola_alto(s, area);
    SDL_FRect area_con = {area.x, area.y + area.h - con_h, area.w, con_h};
    area.h -= con_h;

    float barra_h  = lh + 8;
    float alto_txt = area.h - barra_h;
    int   visibles = (int)(alto_txt / lh);
    if (visibles < 1) visibles = 1;

    scroll_seguir_cursor(s, visibles);

    /* El resaltado se recalcula recien cuando dejaste de tipear. Ver
       XASOL_DEBOUNCE_MS. */
    if (s->resaltado_sucio &&
        (s->ultimo_cambio == 0 ||
         SDL_GetTicks() - s->ultimo_cambio > XASOL_DEBOUNCE_MS))
        refrescar_resaltado(s);

    float x_gutter = area.x + 8;
    float x_texto  = x_gutter + cw * XASOL_GUTTER;

    /* ── Las lineas ── */
    for (int i = 0; i < visibles; i++) {
        int fila = s->scroll + i;
        if (fila >= s->n_lineas) break;

        float y = area.y + i * lh;

        /* La linea del cursor lleva un fondo apenas mas claro. Es la unica
           pista de donde estas cuando el cursor cae fuera de la vista. */
        if (fila == s->cur_fila)
            xa_fill(r, (SDL_FRect){area.x, y, area.w, lh}, XA_LINEA_ON);

        char num[8];
        snprintf(num, sizeof(num), "%4d", fila + 1);
        xa_text(r, f, num, x_gutter, y,
                fila == s->cur_fila ? XA_GUTTER_ON : XA_GUTTER);

        /* El texto se dibuja por TRAMOS DE UN MISMO COLOR, no caracter por
           caracter.
         *
         * Es lo que arregla el parpadeo al escribir. Cada xa_text() crea una
         * superficie, la sube a una textura y destruye las dos; hacerlo por
         * caracter son unas 2400 texturas por frame en una pantalla llena, y
         * encima sobre un renderer por software. Por tramos son ~15 por
         * linea: el mismo dibujo, dos ordenes de magnitud menos de trabajo.
         *
         * Se puede agrupar asi porque la fuente es monoespaciada: la columna
         * N siempre cae en el mismo x, se dibuje sola o dentro de un tramo.
         */
        const char *linea = s->lineas[fila];
        int largo = (int)strlen(linea);
        int max_cols = (int)((area.x + area.w - x_texto) / cw);
        if (largo > max_cols) largo = max_cols;

        int c = 0;
        while (c < largo) {
            /* paed cuenta lineas y columnas desde 1; el buffer desde 0. */
            const XasolToken *t = xasol_token_en(&s->resaltado, fila + 1, c + 1);
            SDL_Color col = t ? xasol_color_de_rol(t->rol) : XA_CLARO;

            /* Estirar el tramo mientras el color no cambie. Comparar el
               COLOR y no el token junta los espacios entre dos palabras del
               mismo color en una sola llamada. */
            int fin = c + 1;
            while (fin < largo) {
                const XasolToken *t2 = xasol_token_en(&s->resaltado, fila + 1, fin + 1);
                SDL_Color c2 = t2 ? xasol_color_de_rol(t2->rol) : XA_CLARO;
                if (c2.r != col.r || c2.g != col.g || c2.b != col.b) break;
                fin++;
            }

            char tramo[XASOL_MAX_COL];
            int  n = fin - c;
            memcpy(tramo, linea + c, (size_t)n);
            tramo[n] = '\0';

            xa_text(r, f, tramo, x_texto + c * cw, y, col);
            c = fin;
        }
    }

    /* ── El cursor ── */
    if (s->cur_fila >= s->scroll && s->cur_fila < s->scroll + visibles) {
        float cx = x_texto + s->cur_col * cw;
        float cy = area.y + (s->cur_fila - s->scroll) * lh;

        if (s->modo == XASOL_INSERT) {
            /* Barra fina: se mete ENTRE dos caracteres, que es donde va a
               entrar lo que escribas. */
            xa_fill(r, (SDL_FRect){cx, cy, 2, lh}, XA_VERDE);
        } else {
            /* Bloque: se para SOBRE un caracter, que es sobre el que actuan
               los comandos. La forma del cursor dice en que modo estas sin
               tener que mirar la barra. */
            xa_fill(r, (SDL_FRect){cx, cy, cw, lh}, XA_VERDE);
            const char *l = s->lineas[s->cur_fila];
            if (s->cur_col < (int)strlen(l)) {
                char uno[2] = { l[s->cur_col], '\0' };
                xa_text(r, f, uno, cx, cy, XA_FONDO);   /* invertido */
            }
        }
    }

    /* ── La barra de estado ── */
    float by = area.y + area.h - barra_h;
    xa_fill(r, (SDL_FRect){area.x, by, area.w, barra_h}, XA_BARRA);

    const char *nombre_modo = (s->modo == XASOL_INSERT) ? " INSERT " : " NORMAL ";
    SDL_Color   col_modo    = (s->modo == XASOL_INSERT) ? XA_NARANJA : XA_VERDE;
    xa_text(r, f, nombre_modo, area.x + 8, by + 4, col_modo);

    char centro[220];
    snprintf(centro, sizeof(centro), "%s%s   %d:%d%s%s",
             s->nombre,
             s->modificado ? " *" : "",
             s->cur_fila + 1, s->cur_col + 1,
             s->aviso[0] ? "   " : "", s->aviso);
    xa_text(r, f, centro, area.x + 8 + cw * 9, by + 4, XA_TENUE);

    /* Si paed no pinto nada, hay que decirlo: si no, un editor en gris parece
       un editor roto y no un archivo sin resaltar. */
    if (!s->resaltado.valido) {
        const char *msg = "sin resaltado (falta paed en el PATH)";
        int mw; TTF_GetStringSize(f, msg, 0, &mw, NULL);
        xa_text(r, f, msg, area.x + area.w - mw - 10, by + 4, XA_ROJO);
    } else {
        const char *msg = "F10 correr   Ctrl+S guardar   Ctrl+J consola";
        int mw; TTF_GetStringSize(f, msg, 0, &mw, NULL);
        xa_text(r, f, msg, area.x + area.w - mw - 10, by + 4, XA_TENUE);
    }

    /* La consola va ultima para quedar por encima de todo lo demas. */
    if (con_h > 0) pintar_consola(s, ctx, area_con);
}

static void
pintar_picker(XasolState *s, ShellCtx *ctx, SDL_FRect area)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    float lh  = xa_line_h(f);
    float pad = 24;
    float cy  = area.y + pad;

    xa_text(r, f, "xasol — el editorXL", area.x + pad, cy, XA_TENUE);
    cy += lh + 4;

    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
    SDL_RenderLine(r, area.x + pad, cy, area.x + area.w - pad, cy);
    cy += 8;

    /* ── Nombre del archivo nuevo ── */
    if (s->pantalla == XASOL_NUEVO) {
        xa_text(r, f, "Nombre del archivo nuevo:", area.x + pad, cy, XA_TENUE);
        cy += lh + 6;

        SDL_FRect caja = {area.x + pad, cy, 320, lh + 10};
        xa_fill(r, caja, (SDL_Color){30, 30, 30, 255});
        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
        SDL_RenderRect(r, &caja);

        char eco[80];
        snprintf(eco, sizeof(eco), "%s_", s->nuevo_buf);
        xa_text(r, f, eco, area.x + pad + 8, cy + 5, XA_CLARO);
        cy += lh + 16;

        char hint[100];
        snprintf(hint, sizeof(hint), "se guarda como %s.paed",
                 s->nuevo_buf[0] ? s->nuevo_buf : "nombre");
        xa_text(r, f, hint, area.x + pad, cy, XA_TENUE);
        cy += lh + 10;
        xa_text(r, f, "[Enter] crear   [Esc] cancelar", area.x + pad, cy, XA_VERDE);
        return;
    }

    /* ── Confirmar borrado ── */
    if (s->pantalla == XASOL_BORRAR && s->sel >= 0) {
        char msg[120];
        snprintf(msg, sizeof(msg), "Borrar  %s ?", s->archivos[s->sel]);
        xa_text(r, f, msg, area.x + pad, cy, XA_ROJO);
        cy += lh + 10;
        xa_text(r, f, "[Enter] confirmar   [cualquier otra] cancelar",
                area.x + pad, cy, XA_TENUE);
        return;
    }

    /* ── Lista vacia ── */
    if (s->n_archivos == 0) {
        xa_text(r, f, "No hay archivos .paed todavia.", area.x + pad, cy, XA_TENUE);
        cy += lh + 12;
        xa_text(r, f, "[N] crear nuevo", area.x + pad, cy, XA_VERDE);
        return;
    }

    /* ── La lista ── */
    float item_h = lh + 6;
    int   fin    = s->scroll_picker + XASOL_PICKER_VIS;
    if (fin > s->n_archivos) fin = s->n_archivos;

    for (int i = s->scroll_picker; i < fin; i++) {
        if (i == s->sel) {
            xa_fill(r, (SDL_FRect){area.x + pad - 4, cy - 2,
                                   area.w - pad * 2 + 8, item_h},
                    (SDL_Color){28, 28, 28, 255});
            SDL_SetRenderDrawColor(r, XA_VERDE.r, XA_VERDE.g, XA_VERDE.b, 255);
            SDL_RenderLine(r, area.x + pad - 4, cy - 2,
                              area.x + pad - 4, cy - 2 + item_h - 1);
            xa_text(r, f, s->archivos[i], area.x + pad + 8, cy, XA_CLARO);
        } else {
            xa_text(r, f, s->archivos[i], area.x + pad + 8, cy, XA_TENUE);
        }
        cy += item_h;
    }

    if (s->n_archivos > XASOL_PICKER_VIS) {
        char sc[16];
        snprintf(sc, sizeof(sc), "%d/%d", s->sel + 1, s->n_archivos);
        int sw; TTF_GetStringSize(f, sc, 0, &sw, NULL);
        xa_text(r, f, sc, area.x + area.w - pad - sw, area.y + pad, XA_TENUE);
    }

    xa_text(r, f, "[j k] navegar   [Enter] abrir   [D] borrar   [N] nuevo",
            area.x + pad, area.y + area.h - lh - 10, XA_TENUE);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  9. EL PANEL
 * ═══════════════════════════════════════════════════════════════════════════ */

void
xasol_init(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    XasolState *s = calloc(1, sizeof(*s));
    if (!s) return;

    s->pantalla  = XASOL_PICKER;
    s->consola_h = XASOL_CONSOLA_DEF_H;

    /* La terminal arranca ABIERTA. bash se levanta con el panel, no cuando se
       aprieta algo: es una terminal, y una terminal esta ahi. */
    s->consola_abierta = 1;
    buffer_vaciar(s);

    /* El temporal lleva el PID adentro para que dos PseudoGames abiertos al
       mismo tiempo no se pisen el archivo. */
    snprintf(s->tmp_path, sizeof(s->tmp_path), "/tmp/xasol_%d.paed", (int)getpid());

    picker_escanear(s);
    tab->state = s;
}

static int
sobre_el_divisor(const XasolState *s, float mx, float my)
{
    if (!s->consola_abierta) return 0;

    SDL_FRect a = s->ultima_area;
    if (a.w <= 0) return 0;                     /* todavia no se dibujo */
    if (mx < a.x || mx > a.x + a.w) return 0;

    float y = a.y + a.h - consola_alto(s, a);
    return my >= y - XASOL_DIVISOR_GRIP &&
           my <= y + XASOL_DIVISOR_H + XASOL_DIVISOR_GRIP;
}

void
xasol_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    XasolState *s = (XasolState *)tab->state;
    if (!s) return;

    /* ── El arrastre de la consola ──
       Se mira antes que el teclado porque son eventos distintos y no compiten,
       y porque el arrastre tiene que seguir andando aunque el mouse se salga
       del divisor mientras estiras — que es lo que uno hace al arrastrar. */
    if (s->pantalla == XASOL_EDITANDO) {
        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e->button.button == SDL_BUTTON_LEFT &&
            sobre_el_divisor(s, e->button.x, e->button.y)) {
            s->arrastrando = 1;
            return;
        }
        if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            s->arrastrando = 0;
        }
        if (e->type == SDL_EVENT_MOUSE_MOTION && s->arrastrando) {
            SDL_FRect a = s->ultima_area;
            /* El alto es la distancia del mouse al piso del panel: arrastrar
               para arriba agranda, para abajo achica. */
            float h = (a.y + a.h) - e->motion.y;
            if (h < XASOL_CONSOLA_MIN_H) h = XASOL_CONSOLA_MIN_H;
            if (h > a.h - 120)           h = a.h - 120;
            s->consola_h = (int)h;
            return;
        }

        /* La rueda del mouse recorre la salida. */
        if (e->type == SDL_EVENT_MOUSE_WHEEL && s->consola_abierta) {
            s->consola_scroll += (int)e->wheel.y;
            if (s->consola_scroll < 0) s->consola_scroll = 0;
            if (s->consola_scroll > s->consola_n) s->consola_scroll = s->consola_n;
            return;
        }
    }

    /* ── Texto tipeado ──
       SDL manda el texto ya resuelto (acentos, ñ, teclados distintos) en un
       evento aparte del de teclas. Es el unico que hay que escuchar para
       escribir: interpretar codigos de tecla a mano se rompe con el primer
       teclado que no sea el tuyo. */
    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (s->saltear_texto) { s->saltear_texto = 0; return; }

        if (s->term_foco && s->term.vivo) { xterm_texto(&s->term, e->text.text); return; }

        if (s->pantalla == XASOL_NUEVO) {
            int n = (int)strlen(e->text.text);
            if (s->nuevo_len + n < 60) {
                memcpy(s->nuevo_buf + s->nuevo_len, e->text.text, (size_t)n);
                s->nuevo_len += n;
                s->nuevo_buf[s->nuevo_len] = '\0';
            }
            return;
        }

        /* Con el foco en la terminal, lo que tipeas es de bash, no del
           editor. Es lo que hace que se pueda correr codigo de verdad. */
        if (s->pantalla == XASOL_EDITANDO && s->term_foco && s->term.vivo) {
            xterm_texto(&s->term, e->text.text);
            return;
        }

        if (s->pantalla == XASOL_EDITANDO && s->modo == XASOL_INSERT)
            editar_insertar_texto(s, e->text.text);
        return;
    }

    if (e->type != SDL_EVENT_KEY_DOWN) return;

    SDL_Keycode k   = e->key.key;
    SDL_Keymod  mod = e->key.mod;

    if (s->pantalla == XASOL_EDITANDO) {
        /* Cada tecla borra el aviso anterior: un "guardado" que se queda
           pegado tres minutos miente sobre lo que acaba de pasar. */
        if (k != SDLK_LCTRL && k != SDLK_RCTRL) s->aviso[0] = '\0';

        /* ── La consola ──
           Van ACA y no adentro de los modos porque tienen que andar igual
           mientras escribis y mientras te movés. Un F10 que solo funciona en
           NORMAL obliga a salir de INSERT para probar lo que acabas de
           tipear, que es justo el momento en que querés probarlo. */
        /* F12 pasa el foco de un lado al otro: editor <-> terminal.
           Va primero para que ande estando donde estes. */
        if (k == SDLK_F12) {
            s->term_foco = !s->term_foco;
            if (s->term_foco) s->consola_abierta = 1;
            SDL_StartTextInput(ctx->ventana);
            return;
        }

        /* ── Con el foco en la terminal, el teclado es de bash ──
           Va ANTES que todo lo demas: una 'j' es una 'j' y no un movimiento
           del cursor, Ctrl+C corta el programa y no copia nada, y Tab
           autocompleta. */
        if (s->term_foco && s->term.vivo) {
            xterm_tecla(&s->term, k, mod);
            return;
        }

        if (k == SDLK_F10) {
            consola_correr(s, ctx);
            s->term_foco = 1;      /* el foco se va con el programa */
            return;
        }

        if ((mod & SDL_KMOD_CTRL) && k == SDLK_J) {
            s->consola_abierta = !s->consola_abierta;
            return;
        }

        /* Scroll de la salida, cuando hay mas de la que entra. */
        if (s->consola_abierta && (mod & SDL_KMOD_SHIFT)) {
            if (k == SDLK_PAGEUP)   { s->consola_scroll += 5; return; }
            if (k == SDLK_PAGEDOWN) {
                s->consola_scroll -= 5;
                if (s->consola_scroll < 0) s->consola_scroll = 0;
                return;
            }
        }

        if (s->modo == XASOL_NORMAL) modo_normal(s, ctx, k, mod);
        else                         modo_insert_tecla(s, ctx, k);

        if (s->pantalla == XASOL_PICKER) {   /* salio con q */
            SDL_StopTextInput(ctx->ventana);
            picker_escanear(s);
            snprintf(tab->nombre, sizeof(tab->nombre), "Editor Libre");
        }
        return;
    }

    /* En el picker, F12 y el foco de la terminal valen igual. */
    if (k == SDLK_F12) {
        s->term_foco = !s->term_foco;
        SDL_StartTextInput(ctx->ventana);
        return;
    }
    if (s->term_foco && s->term.vivo) { xterm_tecla(&s->term, k, mod); return; }

    picker_tecla(s, ctx, tab, k);
}

void
xasol_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    XasolState *s = (XasolState *)tab->state;
    if (!s) return;

    SDL_Renderer *r = ctx->renderer;

    /* El area se guarda para el hit-test del divisor: los eventos de mouse
       llegan en coordenadas de la ventana y el panel no sabe donde esta
       parado hasta que lo dibujan. */
    s->ultima_area = area;

    SDL_Rect clip = {(int)area.x, (int)area.y, (int)area.w, (int)area.h};
    SDL_SetRenderClipRect(r, &clip);
    xa_fill(r, area, XA_FONDO);

    /* Traer lo que el programa haya escrito. Va en el dibujo y no en los
       eventos porque tiene que pasar en CADA frame, haya o no teclas: el
       programa escribe cuando quiere, no cuando vos apretas algo. */
    /* Traer lo que bash haya escrito. Va en el dibujo y no en los eventos
       porque tiene que pasar en CADA frame: el programa escribe cuando
       quiere, no cuando apretas una tecla. */
    if (s->term_lista) xterm_leer(&s->term);

    if (s->pantalla == XASOL_EDITANDO) {
        pintar_editor(s, ctx, area);
    } else {
        /* En el picker la terminal tambien esta: se puede correr algo antes
           de abrir un archivo, que es la mitad de para lo que sirve. */
        float con_h = consola_alto(s, area);
        SDL_FRect arriba = {area.x, area.y, area.w, area.h - con_h};
        SDL_FRect abajo  = {area.x, area.y + area.h - con_h, area.w, con_h};

        pintar_picker(s, ctx, arriba);
        if (con_h > 0) pintar_consola(s, ctx, abajo);
    }

    SDL_SetRenderClipRect(r, NULL);
}

void
xasol_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    XasolState *s = (XasolState *)tab->state;
    if (!s) return;

    /* bash vivo despues de cerrar el tab quedaria huerfano. */
    if (s->term_lista) xterm_cerrar(&s->term);

    if (s->tmp_path[0]) remove(s->tmp_path);
    free(s);
    tab->state = NULL;
}
