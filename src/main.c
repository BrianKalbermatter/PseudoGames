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
       porque las ventanas nacen visibles.
     *
     * Que no reciba posicion es lo que rompia la ventana: se pedia una
     * BORDERLESS del tamaño exacto del display, pero quien decidia DONDE
     * ponerla era el gestor de ventanas. Si la corria aunque sea un poco, la
     * parte derecha quedaba fuera de la pantalla — y a la derecha esta el
     * panel, asi que el sidebar se veia y el contenido no.
     *
     * El arreglo son tres cosas, y las tres hacen falta:
     *
     *   1. La ventana se pide un poco mas CHICA que el display (90%), para
     *      que entre aunque el escritorio tenga barras arriba o al costado.
     *   2. Se la POSICIONA a mano despues de crearla, centrada en el display
     *      que se detecto.
     *   3. Lleva borde y es RESIZABLE, asi el que la usa puede moverla y
     *      agrandarla. Una borderless que ademas no se puede mover no deja
     *      ninguna salida cuando queda mal ubicada.
     *
     * Para volver a pantalla completa de verdad, la linea es
     * SDL_SetWindowFullscreen(ventana, true) — y no una borderless del
     * tamaño del display, que es lo que se estaba intentando aca.
     */
    int win_w = display.w * 9 / 10;
    int win_h = display.h * 9 / 10;
    if (win_w < 960) win_w = display.w < 960 ? display.w : 960;
    if (win_h < 600) win_h = display.h < 600 ? display.h : 600;

    SDL_Window *ventana = SDL_CreateWindow("PseudoGames",
        win_w, win_h,
        SDL_WINDOW_RESIZABLE);
    if (!ventana) {
        fatal("Error ventana", SDL_GetError());
        return 1;
    }

    SDL_SetWindowPosition(ventana,
                          display.x + (display.w - win_w) / 2,
                          display.y + (display.h - win_h) / 2);

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
