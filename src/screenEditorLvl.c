#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include "niveles.h"
#include "editorText.h"

int
screenLvLEditor(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto, int nivel_num)
{
    Nivel *n = obtener_nivel(nivel_num);
    if (!n) return 0;

    char nombre[32];
    snprintf(nombre, sizeof(nombre), "nivel_%d", nivel_num);

    /* Editor con consigna a la izquierda.
       La verificacion corre automaticamente al presionar F5 o el boton RUN. */
    screenEditorText(renderer, fuente, ancho, alto, nombre, n->titulo, n->enunciado, n, nivel_num);

    return 0;
}
