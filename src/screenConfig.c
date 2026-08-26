#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include "ui.h"
#include "config.h"
#include "audio.h"

// Opciones de configuracion disponibles
typedef struct {
    const char *nombre;
    const char *valores[4];  // posibles valores
    int n_valores;
    int seleccion;           // indice del valor actual
} OpcionConfig;

int
screenConfig(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto,
             SDL_Window *ventana)
{
    // --- cargar config guardada ---
    config_cargar();

    // --- definir opciones ---
    OpcionConfig opciones[] = {
        { "Velocidad lluvia",   {"Lenta", "Normal", "Rapida"}, 3, config_get_lluvia() },
        { "Brillo",             {"Bajo", "Normal", "Alto"},    3, config_get_brillo() },
    };
    int n_opciones = (int)(sizeof(opciones) / sizeof(opciones[0]));
    int seleccionada = 0;  // fila seleccionada con el teclado

    int vol        = config_get_volumen();
    int drag_vol   = 0;   /* 1 = arrastrando el handle del slider */
    int tab_config = 0;   /* 0 = General, 1 = Atajos */

    SDL_Event evento;
    int corriendo = 1;

    while (corriendo) {
        int clicked = 0, click_x = 0, click_y = 0;
        /* Re-leer tamaño real: cambia al entrar/salir de fullscreen */
        SDL_GetWindowSize(ventana, &ancho, &alto);

        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_EVENT_QUIT)   return 0;
            if (evento.type == SDL_EVENT_KEY_DOWN) {
                switch (evento.key.key) {
                    case SDLK_ESCAPE:
                        config_set_lluvia(opciones[0].seleccion);
                        config_set_brillo(opciones[1].seleccion);
                        config_set_volumen(vol);
                        audio_set_volumen(vol);
                        config_guardar();
                        corriendo = 0;
                        break;
                    case SDLK_UP:
                        seleccionada--;
                        if (seleccionada < 0) seleccionada = n_opciones - 1;
                        break;
                    case SDLK_DOWN:
                        seleccionada++;
                        if (seleccionada >= n_opciones) seleccionada = 0;
                        break;
                    case SDLK_LEFT:
                        opciones[seleccionada].seleccion--;
                        if (opciones[seleccionada].seleccion < 0)
                            opciones[seleccionada].seleccion = opciones[seleccionada].n_valores - 1;
                        break;
                    case SDLK_RIGHT: case SDLK_RETURN:
                        opciones[seleccionada].seleccion++;
                        if (opciones[seleccionada].seleccion >= opciones[seleccionada].n_valores)
                            opciones[seleccionada].seleccion = 0;
                        break;
                    default: break;
                }

            }
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                evento.button.button == SDL_BUTTON_LEFT) {
                clicked  = 1;
                click_x  = evento.button.x;
                click_y  = evento.button.y;
            }
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                evento.button.button == SDL_BUTTON_LEFT) {
                if (drag_vol) {
                    config_set_volumen(vol);
                    config_guardar();
                }
                drag_vol = 0;
            }
            if (evento.type == SDL_EVENT_MOUSE_MOTION && drag_vol) {
                int panel_x2 = (ancho - 520) / 2;
                int track_x2 = panel_x2 + 140;
                int track_w2 = 520 - 160;
                int handle_w2 = 14;
                int rel = evento.motion.x - track_x2;
                vol = rel * 100 / (track_w2 - handle_w2);
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;
                audio_set_volumen(vol);
            }
        }

        // --- render ---
        SDL_SetRenderDrawColor(renderer, 12, 12, 18, 255);
        SDL_RenderClear(renderer);

        // panel central
        int panel_w = 520, panel_h = 460;
        int panel_x = (ancho - panel_w) / 2;
        int panel_y = (alto  - panel_h) / 2;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 20, 20, 35, 220);
        SDL_FRect panel = {panel_x, panel_y, panel_w, panel_h};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
        SDL_RenderRect(renderer, &panel);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // titulo + tabs General / Atajos
        {
            SDL_Color c_titulo = {180, 180, 220, 255};
            dibujadoTextoColor(renderer, fuente, "Configuracion",
                panel_x + 20, panel_y + 10, c_titulo);

            const char *tabs_lbl[] = { "General", "Atajos" };
            int tab_btn_w = 90, tab_btn_h = 24;
            int tab_btn_x = panel_x + panel_w - tab_btn_w * 2 - 20;
            int tab_btn_y = panel_y + 8;
            for (int t = 0; t < 2; t++) {
                int tx = tab_btn_x + t * (tab_btn_w + 4);
                int is_act = (t == tab_config);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer,
                    is_act ? 60 : 25, is_act ? 60 : 25, is_act ? 110 : 50, 220);
                SDL_FRect tr = {tx, tab_btn_y, tab_btn_w, tab_btn_h};
                SDL_RenderFillRect(renderer, &tr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(renderer,
                    is_act ? 120 : 60, is_act ? 120 : 60, is_act ? 220 : 100, 255);
                SDL_RenderRect(renderer, &tr);
                SDL_Color c_tlbl = is_act
                    ? (SDL_Color){220, 220, 255, 255}
                    : (SDL_Color){100, 100, 140, 255};
                int tlw; TTF_GetStringSize(fuente, tabs_lbl[t], 0, &tlw, NULL);
                dibujadoTextoColor(renderer, fuente, tabs_lbl[t],
                    tx + (tab_btn_w - tlw) / 2 - 10, tab_btn_y - 2, c_tlbl);

                if (clicked &&
                    click_x >= tx && click_x <= tx + tab_btn_w &&
                    click_y >= tab_btn_y && click_y <= tab_btn_y + tab_btn_h)
                    tab_config = t;
            }

            SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
            SDL_RenderLine(renderer,
                panel_x + 16, panel_y + 44,
                panel_x + panel_w - 16, panel_y + 44);
        }

        int mx, my;
        ui_mouse(&mx, &my);

        if (tab_config == 0) {  /* ── General ── */

        for (int i = 0; i < n_opciones; i++) {
            int oy = panel_y + 70 + i * 90;
            int hover_row = (mx >= panel_x + 16 && mx <= panel_x + panel_w - 16 &&
                             my >= oy - 10   && my <= oy + 60);

            // highlight de fila
            if (i == seleccionada || hover_row) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 40, 40, 80, 120);
                SDL_FRect row_bg = {panel_x + 12, oy - 10,
                                   panel_w - 24, 62};
                SDL_RenderFillRect(renderer, &row_bg);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                if (hover_row) seleccionada = i;
            }

            // nombre de la opcion
            SDL_Color c_nombre = (i == seleccionada)
                ? (SDL_Color){220, 220, 255, 255}
                : (SDL_Color){140, 140, 160, 255};
            dibujadoTextoColor(renderer, fuente, opciones[i].nombre,
                panel_x + 20, oy, c_nombre);

            // valores como botones < valor >
            int vx = panel_x + panel_w - 200;
            SDL_Color c_flecha = {80, 120, 255, 255};
            SDL_Color c_valor  = {200, 255, 200, 255};

            dibujadoTextoColor(renderer, fuente, "<", vx, oy, c_flecha);

            const char *val_actual = opciones[i].valores[opciones[i].seleccion];
            int vw; TTF_GetStringSize(fuente, val_actual, 0, &vw, NULL);
            dibujadoTextoColor(renderer, fuente, val_actual,
                vx + 30 + (120 - vw) / 2, oy, c_valor);

            dibujadoTextoColor(renderer, fuente, ">", vx + 160, oy, c_flecha);

            // click en < o >
            if (clicked) {
                // boton <
                if (click_x >= vx && click_x <= vx + 20 &&
                    click_y >= oy && click_y <= oy + 24) {
                    opciones[i].seleccion--;
                    if (opciones[i].seleccion < 0)
                        opciones[i].seleccion = opciones[i].n_valores - 1;
                }
                // boton >
                if (click_x >= vx + 150 && click_x <= vx + 180 &&
                    click_y >= oy && click_y <= oy + 24) {
                    opciones[i].seleccion++;
                    if (opciones[i].seleccion >= opciones[i].n_valores)
                        opciones[i].seleccion = 0;
                }
            }
        }

        /* ── Slider de volumen ── */
        {
            int sl_y      = panel_y + 345;
            int track_x   = panel_x + 140;
            int track_w   = panel_w - 160;
            int handle_w  = 14;
            int handle_h  = 22;
            int handle_x  = track_x + vol * (track_w - handle_w) / 100;
            int track_cy  = sl_y + 8;   /* centro vertical del track */

            /* etiqueta */
            SDL_Color c_lbl = {140, 140, 160, 255};
            dibujadoTextoColor(renderer, fuente, "Volumen:", panel_x + 20, sl_y, c_lbl);

            /* track fondo */
            SDL_SetRenderDrawColor(renderer, 40, 40, 65, 255);
            SDL_FRect track_bg = {track_x, track_cy - 4, track_w, 8};
            SDL_RenderFillRect(renderer, &track_bg);

            /* fill hasta el handle */
            SDL_SetRenderDrawColor(renderer, 80, 130, 255, 255);
            SDL_FRect track_fill = {track_x, track_cy - 4, handle_x - track_x + handle_w / 2, 8};
            SDL_RenderFillRect(renderer, &track_fill);

            /* handle */
            int hov_handle = (mx >= handle_x && mx <= handle_x + handle_w &&
                              my >= sl_y      && my <= sl_y + handle_h);
            SDL_SetRenderDrawColor(renderer,
                (drag_vol || hov_handle) ? 160 : 220,
                (drag_vol || hov_handle) ? 200 : 220,
                255, 255);
            SDL_FRect handle = {handle_x, sl_y, handle_w, handle_h};
            SDL_RenderFillRect(renderer, &handle);

            /* porcentaje */
            char vol_str[8];
            snprintf(vol_str, sizeof(vol_str), "%d%%", vol);
            SDL_Color c_val = {200, 255, 200, 255};
            dibujadoTextoColor(renderer, fuente, vol_str,
                track_x + track_w + 8, sl_y, c_val);

            /* click en el track o handle → iniciar drag */
            if (clicked &&
                click_x >= track_x && click_x <= track_x + track_w &&
                click_y >= sl_y    && click_y <= sl_y + handle_h) {
                drag_vol = 1;
                vol = (click_x - track_x) * 100 / (track_w - handle_w);
                if (vol < 0)   vol = 0;
                if (vol > 100) vol = 100;
                audio_set_volumen(vol);
            }
        }

        } /* end if (tab_config == 0) General */

        if (tab_config == 1) {  /* ── Atajos ── */
            typedef struct { const char *key; const char *desc; } Atajo;
            static const Atajo atajos[] = {
                { "Ctrl + T",     "Nueva tab"              },
                { "Ctrl + W",     "Cerrar tab"             },
                { "Ctrl + Tab",   "Siguiente tab"          },
                { "F9",           "Guardar archivo"        },
                { "F5",           "Ejecutar codigo"        },
                { "F8",           "Abrir Wiki"             },
                { "F10",          "Volver al menu"         },
                { "Tab",          "Indentar (4 espacios)"  },
                { "Ctrl + C",     "Copiar linea"           },
                { "Ctrl + X",     "Cortar linea"           },
                { "Ctrl + V",     "Pegar"                  },
                { "Ctrl + Z",     "Deshacer (proximamente)"},
            };
            int n_atajos = (int)(sizeof(atajos) / sizeof(atajos[0]));
            int row_h    = 28;
            int ay       = panel_y + 58;

            SDL_Color c_key  = {100, 160, 255, 255};
            SDL_Color c_sep  = { 60,  60, 100, 255};
            SDL_Color c_desc = {160, 160, 180, 255};

            for (int i = 0; i < n_atajos; i++) {
                int ry = ay + i * row_h;
                if (i % 2 == 0) {
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 30, 30, 55, 80);
                    SDL_FRect stripe = {panel_x + 12, ry - 2, panel_w - 24, row_h - 2};
                    SDL_RenderFillRect(renderer, &stripe);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }
                dibujadoTextoColor(renderer, fuente, atajos[i].key,
                    panel_x + 20, ry, c_key);
                dibujadoTextoColor(renderer, fuente, "->",
                    panel_x + 170, ry, c_sep);
                dibujadoTextoColor(renderer, fuente, atajos[i].desc,
                    panel_x + 210, ry, c_desc);
            }
        } /* end Atajos */

        // boton feedback fuera del panel, centrado abajo
        int fb_w = 130, fb_h = 28;
        int fb_x = (ancho - fb_w) / 2;
        int fb_y = panel_y + panel_h + 18;
        int fb_hover = (mx >= fb_x && mx <= fb_x + fb_w &&
                        my >= fb_y && my <= fb_y + fb_h);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer,
            fb_hover ? 60 : 30, fb_hover ? 45 : 22, fb_hover ? 120 : 60, 200);
        SDL_FRect fb_btn = {fb_x, fb_y, fb_w, fb_h};
        SDL_RenderFillRect(renderer, &fb_btn);
        SDL_SetRenderDrawColor(renderer,
            fb_hover ? 160 : 80, fb_hover ? 120 : 60, fb_hover ? 255 : 160, 255);
        SDL_RenderRect(renderer, &fb_btn);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        SDL_Color c_fb = fb_hover ? (SDL_Color){220,200,255,255}
                                  : (SDL_Color){130,110,200,255};
        int fbw, fbh; TTF_GetStringSize(fuente, "Feedback", 0, &fbw, &fbh);
        // dibujadoTextoColor aplica +10 en x y +12 en y internamente, hay que compensarlo
        dibujadoTextoColor(renderer, fuente, "Feedback",
            fb_x + (fb_w - fbw) / 2 - 10, fb_y + (fb_h - fbh) / 2 - 12, c_fb);

        if (clicked &&
            click_x >= fb_x && click_x <= fb_x + fb_w &&
            click_y >= fb_y && click_y <= fb_y + fb_h)
            screenFeedback(renderer, fuente, ancho, alto);

        // ayuda abajo a la izquierda
        SDL_Color c_help = {70, 70, 90, 255};
        dibujadoTextoColor(renderer, fuente,
            "[flechas] navegar   [</>] cambiar   [drag] volumen   [ESC] guardar",
            panel_x + 20, panel_y + panel_h - 34, c_help);

        presente(renderer);
        SDL_Delay(16);
    }

    return 0;
}
