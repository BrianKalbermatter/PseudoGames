#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "niveles.h"
#include "progreso.h"
#include "pomodoro_bg.h"
#include "audio.h"
#include "config.h"
#include "shell.h"
#include "ui.h"
#include <stdio.h>

// Helper privado
static void fatal(const char *titulo, const char *msg) {
    fprintf(stderr, "%s: %s\n", titulo, msg);
}

// main/start
int
main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    SDL_SetHint("SDL_VIDEO_X11_XSHM", "0");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fatal("Error SDL_Init", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fatal("Error TTF_Init", TTF_GetError());
        return 1;
    }

    audio_init();
    config_cargar();
    audio_set_volumen(config_get_volumen());
    cargar_niveles("data/niveles.json");
    cargar_progreso("saves/progreso.json");

    TTF_Font *fuente = TTF_OpenFont("assets/fonts/main.ttf", 16);
    if (!fuente) {
        char msg[256];
        snprintf(msg, sizeof(msg), "No se pudo abrir assets/fonts/main.ttf\n%s", TTF_GetError());
        fatal("Error fuente", msg);
        return 1;
    }

    /* Detectar pantalla principal: buscar el display en coordenada (0,0)
       SDL no garantiza que el índice 0 sea el principal en multi-monitor */
    SDL_Rect display = {0, 0, 1280, 720};
    int n_displays = SDL_GetNumVideoDisplays();
    for (int d = 0; d < n_displays; d++) {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(d, &r) == 0 && r.x == 0 && r.y == 0) {
            display = r;
            break;
        }
    }

    SDL_Window *ventana = SDL_CreateWindow("PseudoGames",
        display.x, display.y,
        display.w, display.h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    if (!ventana) {
        fatal("Error ventana", SDL_GetError());
        return 1;
    }

    /* WSL2/XWayland usa visual ARGB — SDL_RenderPresent deja alpha=0 (transparente).
       Solución: renderizar sobre la window surface directamente con SDL_CreateSoftwareRenderer,
       y usar SDL_UpdateWindowSurface en vez de SDL_RenderPresent. */
    SDL_Surface *wsurface = SDL_GetWindowSurface(ventana);
    if (!wsurface) { fatal("Error surface", SDL_GetError()); return 1; }

    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(wsurface);
    if (!renderer) { fatal("Error renderer", SDL_GetError()); return 1; }

    ui_set_ventana(ventana);
    SDL_RaiseWindow(ventana);
    SDL_SetWindowInputFocus(ventana);

    int ancho, alto;
    SDL_GetWindowSize(ventana, &ancho, &alto);

    screenShell(renderer, fuente, ancho, alto, ventana);

    audio_fade_out(800);

    pom_cleanup();
    audio_cleanup();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(ventana);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
