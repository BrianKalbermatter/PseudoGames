#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
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

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fatal("Error SDL_Init", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        fatal("Error TTF_Init", SDL_GetError());
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
        snprintf(msg, sizeof(msg), "No se pudo abrir assets/fonts/main.ttf\n%s", SDL_GetError());
        fatal("Error fuente", msg);
        return 1;
    }

    /* Detectar pantalla principal: buscar el display en coordenada (0,0)
       SDL no garantiza que el índice 0 sea el principal en multi-monitor.
       SDL3: ya no se pide "cuantos displays hay" y se itera por indice.
       SDL_GetDisplays() devuelve un array de IDs que hay que liberar
       con SDL_free(). Los bounds siguen siendo SDL_Rect (enteros): son
       pixeles del escritorio, no coordenadas de dibujo. */
    SDL_Rect display = {0, 0, 1280, 720};
    int n_displays = 0;
    SDL_DisplayID *displays = SDL_GetDisplays(&n_displays);
    if (displays) {
        for (int d = 0; d < n_displays; d++) {
            SDL_Rect r;
            if (SDL_GetDisplayBounds(displays[d], &r) && r.x == 0 && r.y == 0) {
                display = r;
                break;
            }
        }
        SDL_free(displays);
    }

    /* SDL3: CreateWindow ya no recibe posicion, y SDL_WINDOW_SHOWN no existe
       porque las ventanas nacen visibles. */
    SDL_Window *ventana = SDL_CreateWindow("PseudoGames",
        display.w, display.h,
        SDL_WINDOW_BORDERLESS);
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
