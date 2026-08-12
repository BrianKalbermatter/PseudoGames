#ifndef EDITOR_TEXT_H
#define EDITOR_TEXT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "niveles.h"

#define MAX_LINES 500
#define MAX_TABS  8
#define TAB_H     32

typedef struct {
    char buf[MAX_LINES][512];
    int  n_lines;
    int  cursor_row;
    int  cursor_col;
    int  offset_row;
    char nombre_arch[64];
    int  dirty;
} EditorPanel;

/* Dibuja el editor dentro de un rect sin loop propio (para embeber) */
void drawEditorText(SDL_Renderer *renderer, TTF_Font *fuente, SDL_Rect area);

/* Pantalla completa con loop propio.
 * nombre_fijo    : si no es NULL/""  → carga saves/<nombre>.paed y F9 sobreescribe directo.
 * cons_titulo / cons_texto : si no son NULL → muestra consigna en panel izquierdo.
 * nivel / nivel_num : si nivel != NULL → verifica automaticamente al ejecutar con F5/RUN. */
int screenEditorText(SDL_Renderer *renderer, TTF_Font *fuente,
                     int ancho, int alto, const char *nombre_fijo,
                     const char *cons_titulo, const char *cons_texto,
                     Nivel *nivel, int nivel_num);

/* API de panel para el shell de tabs (Editor Libre) */
#define EDITOR_MAX_SAVES 50
#define EDITOR_OUT_MAX    6

typedef struct EditorLibreState EditorLibreState;
EditorLibreState *editor_libre_crear   (TTF_Font *fuente);
void              editor_libre_destruir(EditorLibreState *s);
void              editor_libre_evento  (EditorLibreState *s, TTF_Font *fuente,
                                        SDL_Event *e, SDL_Rect area);
void              editor_libre_dibujar (EditorLibreState *s, SDL_Renderer *r,
                                        TTF_Font *fuente, SDL_Rect area);

#endif
