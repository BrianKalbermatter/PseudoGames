#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* Intro cinemática: mazmorra 3D con raycasting.
   Corre una sola vez. Retorna cuando el jugador
   se acerca a la terminal. */
void screenPJ_intro(SDL_Renderer *renderer, SDL_Window *ventana, int ancho, int alto);
