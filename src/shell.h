#ifndef SHELL_H
#define SHELL_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* ── Dimensiones del layout ──────────────────────────────────────────────── */
#define SIDEBAR_W           180   /* ancho del sidebar expandido             */
#define SIDEBAR_COLLAPSED_W  24   /* ancho cuando esta colapsado (solo ">")  */
#define TAB_BAR_H            28   /* alto de la barra de tabs                */
#define MAX_TABS              8   /* maxima cantidad de tabs abiertos         */

/* ── Paleta minimalista ──────────────────────────────────────────────────── */
/*    Se usan como literales: (SDL_Color)COL_XXX                             */
#define COL_BG       (SDL_Color){  0,   0,   0, 255}   /* negro puro         */
#define COL_SIDEBAR  (SDL_Color){  8,   8,   8, 255}   /* gris muy oscuro    */
#define COL_SEP      (SDL_Color){ 34,  34,  34, 255}   /* separadores        */
#define COL_TEXT     (SDL_Color){136, 136, 136, 255}   /* texto inactivo     */
#define COL_ACTIVE   (SDL_Color){255, 255, 255, 255}   /* texto activo/hover */
#define COL_ACCENT   (SDL_Color){  0, 220,  70, 255}   /* verde neon         */
#define COL_CLOSE    (SDL_Color){200,  60,  60, 255}   /* rojo para cerrar   */

/* ── IDs de paneles ──────────────────────────────────────────────────────── */
typedef enum {
    PANEL_DOC = 0,
    PANEL_POMODORO,
    PANEL_EDITOR_LIBRE,
    PANEL_NIVELES,
    PANEL_SOLUCIONES,
    PANEL_CONFIG,
    PANEL_MAZMORRA,
    PANEL_C_EDITOR,
    PANEL_COUNT             /* siempre al final: dice cuantos hay en total   */
} PanelID;

/* ── Tab: una entrada en la barra de tabs ───────────────────────────────── */
typedef struct {
    PanelID id;             /* que panel es                                  */
    char    nombre[32];     /* nombre que aparece en el tab                  */
    void   *state;          /* puntero al estado privado del panel (heap)    */
} Tab;

/* Forward declaration necesaria porque PanelDef y ShellCtx se referencian  */
typedef struct ShellCtx ShellCtx;

/* ── PanelDef: interfaz que cada panel debe implementar ─────────────────── */
/*
 * Es el patron "vtable" de C: una struct con punteros a funcion.
 * Equivale a una "interfaz" en OOP. El shell llama estas funciones
 * sin saber que hay adentro de cada panel.
 */
typedef struct {
    PanelID     id;
    const char *nombre;

    /* Se llama UNA vez cuando el usuario abre el tab por primera vez.
       Debe alocar tab->state con malloc y cargar recursos.             */
    void (*init)        (ShellCtx *ctx, Tab *tab);

    /* Se llama por cada SDL_Event que le corresponde a este panel.     */
    void (*handle_event)(ShellCtx *ctx, Tab *tab, SDL_Event *e);

    /* Se llama cada frame. Debe dibujar SOLO dentro del rect `area`.
       Usar SDL_RenderSetClipRect al inicio y al final.                 */
    void (*draw)        (ShellCtx *ctx, Tab *tab, SDL_Rect area);

    /* Se llama cuando el tab se cierra. Liberar state y texturas.      */
    void (*cleanup)     (ShellCtx *ctx, Tab *tab);
} PanelDef;

/* ── ShellCtx: estado global del shell ──────────────────────────────────── */
struct ShellCtx {
    SDL_Renderer *renderer;
    TTF_Font     *fuente;
    SDL_Window   *ventana;

    int ancho, alto;        /* tamaño actual de la ventana                   */
    int salir;              /* 1 = el loop debe terminar                     */

    /* Sidebar */
    int sidebar_abierto;    /* 1 = expandido, 0 = colapsado                  */
    int sidebar_w;          /* ancho calculado segun sidebar_abierto          */

    /* Tabs */
    Tab tabs[MAX_TABS];
    int n_tabs;
    int tab_activo;         /* indice en tabs[]                              */

    /* Definiciones de paneles (una por PanelID, registradas al inicio)     */
    PanelDef defs[PANEL_COUNT];

    /* Icono del logo (nuez) cargado desde PNG                              */
    SDL_Texture *logo;
};

/* ── Funcion publica ─────────────────────────────────────────────────────── */
void screenShell(SDL_Renderer *renderer, TTF_Font *fuente,
                 int ancho, int alto, SDL_Window *ventana);

#endif
