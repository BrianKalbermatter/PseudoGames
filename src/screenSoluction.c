#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "niveles.h"
#include <dirent.h>
#include <stdlib.h>
int
screenSoluciones(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto){
    SDL_Event evento;
    char *archivos[5];
    int total = 0;
    //opendir();
    //  Abre la carpeta como si fuera un archivo
    DIR *dir = opendir("solutions");
    struct dirent *entrada;
    printf("dir: %p\n", (void*)dir);
    if (dir == NULL){
        printf("No encontro la carpeta solutions\n");
        return 0;
    }
    //readdir();
    //  Lee una entrada por vez:
    //  - Guardamos los nombres en el array archivos[]
    //  Cada archivo de solucion que haya es un struct dirent *entrada con d_name
    while ((entrada = readdir(dir)) != NULL){
        if (entrada->d_name[0] == '.') continue;
        archivos[total] = strdup(entrada->d_name);
        total++;
    }
    closedir(dir);

    while (1) {
        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_QUIT) return 0;
            if (evento.type == SDL_KEYDOWN)
                if (evento.key.keysym.sym == SDLK_ESCAPE) return 0;
        }
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Tarjetas de soluciones
        for (int i = 0; i < total; i++) {
        int card_w = 300;
        int card_h = 60;
        int card_x = (ancho - card_w) / 2;
        int card_y = 50 + (i * 80);
        
        

        SDL_Rect card = {card_x, card_y, card_w, card_h};
        SDL_SetRenderDrawColor(renderer, 40, 80, 120, 255);
        SDL_RenderFillRect(renderer, &card);

        dibujadoTexto(renderer, fuente, archivos[i], card_x, card_y);
        }

        // Boton con el SALIR
        SDL_Rect btn_salir = {10, alto - 50, 100, 35};
        SDL_SetRenderDrawColor(renderer, 0, 120, 200, 255); // azul mas claro
        SDL_RenderFillRect(renderer, &btn_salir);
        dibujadoTexto(renderer, fuente, "Salir", 10, alto - 50);

        // Click en salir
        if (evento.type == SDL_MOUSEBUTTONDOWN) {
            int cx = evento.button.x;
            int cy = evento.button.y;
            if (cx >= 10 && cx <= 110 && cy >= alto - 50 && cy <= alto - 15)
                return 0;
  }
        
        presente(renderer);
    }
    return 0;
}


