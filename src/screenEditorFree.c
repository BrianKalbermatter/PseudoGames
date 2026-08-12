#include <SDL2/SDL_ttf.h>
#include "editorText.h"

int
screenFreeEditor(SDL_Renderer *renderer, TTF_Font *fuente,
                 int ancho, int alto, int nivel_num)
{
    (void)nivel_num;
    return screenEditorText(renderer, fuente, ancho, alto, NULL, NULL, NULL, NULL, 0);
}
