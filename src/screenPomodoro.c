#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "ui.h"
#include "audio.h"

#define MUSICA_MENU "assets/Audio/specular_city_dance_extended.ogg"

/* ── helpers de render ─────────────────────────────────────────────── */

/* Texto sin el offset interno de dibujadoTextoColor */
static void
pom_txt(SDL_Renderer *r, TTF_Font *f, const char *s, int x, int y, SDL_Color c)
{
    SDL_Surface *sup = TTF_RenderText_Blended(f, s, 0, c);
    if (!sup) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, sup);
    SDL_FRect pos = {x, y, sup->w, sup->h};
    SDL_RenderTexture(r, tex, NULL, &pos);
    SDL_DestroySurface(sup);
    SDL_DestroyTexture(tex);
}

/* Fondo base del juego */
static void
pom_fondo(SDL_Renderer *r, int ancho, int alto)
{
    SDL_SetRenderDrawColor(r, 8, 12, 8, 255);
    SDL_RenderClear(r);
    (void)ancho; (void)alto;
}

/* Panel RPG centrado */
static void
pom_panel(SDL_Renderer *r, int px, int py, int pw, int ph)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 4, 10, 4, 220);
    SDL_FRect panel = {px, py, pw, ph};
    SDL_RenderFillRect(r, &panel);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 220, 70, 255);
    SDL_RenderRect(r, &panel);
    SDL_FRect inner = {px+2, py+2, pw-4, ph-4};
    SDL_SetRenderDrawColor(r, 0, 70, 25, 255);
    SDL_RenderRect(r, &inner);
}

/* Cabecera del panel con titulo y subtitulo */
static void
pom_header(SDL_Renderer *r, TTF_Font *ft, TTF_Font *fuente,
           int px, int py, int pw, const char *subtitulo)
{
    SDL_Color c_neon = {0, 230, 70, 255};
    SDL_Color c_dim  = {0,  90, 30, 255};

    SDL_FRect hdr = {px+1, py+1, pw-2, 38};
    SDL_SetRenderDrawColor(r, 0, 35, 12, 255);
    SDL_RenderFillRect(r, &hdr);

    int tw; TTF_GetStringSize(ft, "POMODORO", 0, &tw, NULL);
    pom_txt(r, ft, "POMODORO", px + (pw - tw)/2, py + 6, c_neon);

    if (subtitulo && subtitulo[0]) {
        int sw; TTF_GetStringSize(fuente, subtitulo, 0, &sw, NULL);
        pom_txt(r, fuente, subtitulo, px + pw - sw - 16, py + 12, c_dim);
    }

    SDL_SetRenderDrawColor(r, 0, 130, 45, 255);
    SDL_RenderLine(r, px+8, py+40, px+pw-8, py+40);
}

/* Timer MM:SS grande centrado */
static void
pom_timer(SDL_Renderer *r, TTF_Font *fr, TTF_Font *fuente,
          int cx, int cy, int seg, int pausado,
          const char *label, SDL_Color c_label)
{
    SDL_Color c_dim  = {0, 70, 25, 255};

    int lw; TTF_GetStringSize(fuente, label, 0, &lw, NULL);
    pom_txt(r, fuente, label, cx - lw/2, cy - 80, c_label);

    SDL_SetRenderDrawColor(r, 0, 80, 30, 180);
    SDL_RenderLine(r, cx-160, cy-56, cx+160, cy-56);

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", seg/60, seg%60);
    int tw, th; TTF_GetStringSize(fr, buf, 0, &tw, &th);
    SDL_Color c_t = pausado ? (SDL_Color){80,80,80,255} : c_label;
    /* sombra */
    pom_txt(r, fr, buf, cx - tw/2 + 2, cy - 44 + 2, (SDL_Color){0,30,10,255});
    pom_txt(r, fr, buf, cx - tw/2,     cy - 44,     c_t);

    SDL_SetRenderDrawColor(r, 0, 60, 22, 180);
    SDL_RenderLine(r, cx-160, cy + th - 36, cx+160, cy + th - 36);
    (void)c_dim;
}

/* ── MENU ──────────────────────────────────────────────────────────── */
static int
pom_menu(SDL_Renderer *renderer, TTF_Font *ft, TTF_Font *fuente, int ancho, int alto)
{
    SDL_Color c_neon  = {  0, 230,  70, 255};
    SDL_Color c_hover = {180, 255, 180, 255};
    SDL_Color c_dim   = { 80,  80,  80, 255};

    const char *opciones[] = {
        "[1]  Pomodoro  25+5 min",
        "[2]  Cronometro",
        "[3]  Temporizador custom",
        "[4]  Estadisticas",
        "[ESC] Volver",
    };
    int n  = 5;
    int sel = 0;
    int lh  = TTF_GetFontHeight(fuente) + 12;
    SDL_Event ev;

    int pw = 420, ph = 80 + n * lh + 40;
    int px = (ancho - pw) / 2;
    int py = (alto  - ph) / 2;
    int cx = px + pw / 2;

    while (1) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) return -1;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                switch (ev.key.key) {
                    case SDLK_ESCAPE: return 0;
                    case SDLK_UP:     sel = (sel - 1 + n) % n; break;
                    case SDLK_DOWN:   sel = (sel + 1) % n;     break;
                    case SDLK_RETURN: return sel + 1;
                    case SDLK_1: return 1;
                    case SDLK_2: return 2;
                    case SDLK_3: return 3;
                    case SDLK_4: return 4;
                    default: break;
                }
            }
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw, "MENU");

        for (int i = 0; i < n; i++) {
            int oy = py + 50 + i * lh;
            SDL_Color c = (i == sel) ? c_hover : c_neon;
            if (i == n-1) c = c_dim;

            if (i == sel) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 60, 20, 160);
                SDL_FRect hl = {px+8, oy-4, pw-16, lh};
                SDL_RenderFillRect(renderer, &hl);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(renderer, 0, 180, 60, 255);
                SDL_FRect hlb = {px+8, oy-4, pw-16, lh};
                SDL_RenderRect(renderer, &hlb);
            }

            int tw; TTF_GetStringSize(fuente, opciones[i], 0, &tw, NULL);
            pom_txt(renderer, fuente, opciones[i], cx - tw/2, oy, c);
        }

        const char *h = "[flechas] navegar   [Enter] seleccionar";
        int hw; TTF_GetStringSize(fuente, h, 0, &hw, NULL);
        pom_txt(renderer, fuente, h, cx - hw/2, py + ph - 24, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
}

/* ── POMODORO 25+5 ─────────────────────────────────────────────────── */
static int
pom_modo_pomodoro(SDL_Renderer *renderer, TTF_Font *ft, TTF_Font *fr,
                  TTF_Font *fuente, int ancho, int alto, int *p_ciclos)
{
    int TRABAJO  = 25 * 60;
    int DESCANSO =  5 * 60;
    int fase = 0, seg = TRABAJO, pausado = 0;
    Uint32 ultimo = SDL_GetTicks();
    SDL_Color c_verde = {  0, 220,  80, 255};
    SDL_Color c_amar  = {220, 200,   0, 255};
    SDL_Color c_dim   = { 80,  80,  80, 255};
    SDL_Event ev;
    int cx = ancho/2, cy = alto/2;
    int pw = ancho - 96, ph = alto - 96;
    int px = 48, py = 48;

    while (1) {
        Uint32 ahora = SDL_GetTicks();
        if (!pausado && ahora - ultimo >= 1000) {
            ultimo += 1000; seg--;
            if (seg < 0) {
                fase = 1 - fase;
                if (fase == 0) (*p_ciclos)++;
                seg = (fase == 0) ? TRABAJO : DESCANSO;
            }
        }
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) return -1;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) return 0;
                if (ev.key.key == SDLK_P) { pausado = !pausado; ultimo = SDL_GetTicks(); }
                if (ev.key.key == SDLK_0) { fase=0; seg=TRABAJO; pausado=0; *p_ciclos=0; ultimo=SDL_GetTicks(); }
            }
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw,
                   fase==0 ? "TRABAJO" : "DESCANSO");

        SDL_Color c_fase = fase==0 ? c_verde : c_amar;
        pom_timer(renderer, fr, fuente, cx, cy, seg, pausado,
                  fase==0 ? "[ TRABAJO ]" : "[ DESCANSO ]", c_fase);

        char cbuf[40];
        snprintf(cbuf, sizeof(cbuf), "Ciclos completados: %d", *p_ciclos);
        int cw; TTF_GetStringSize(fuente, cbuf, 0, &cw, NULL);
        pom_txt(renderer, fuente, cbuf, cx - cw/2, cy + 50, c_dim);

        if (pausado && (SDL_GetTicks()/500)%2==0) {
            const char *pt = "PAUSADO";
            int pw2; TTF_GetStringSize(fuente, pt, 0, &pw2, NULL);
            pom_txt(renderer, fuente, pt, cx-pw2/2, cy+78, c_amar);
        }

        const char *help = "[p] pausar   [0] reiniciar   [ESC] menu";
        int hw; TTF_GetStringSize(fuente, help, 0, &hw, NULL);
        pom_txt(renderer, fuente, help, cx-hw/2, py+ph-28, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
}

/* ── CRONOMETRO ────────────────────────────────────────────────────── */
static int
pom_modo_cronometro(SDL_Renderer *renderer, TTF_Font *ft, TTF_Font *fr,
                    TTF_Font *fuente, int ancho, int alto)
{
    Uint32 inicio = SDL_GetTicks(), pausa_acum = 0, pausa_inicio = 0;
    int pausado = 0;
    Uint32 elapsed = 0;
    SDL_Color c_cyan = {  0, 200, 220, 255};
    SDL_Color c_dim  = { 80,  80,  80, 255};
    SDL_Color c_amar = {220, 200,   0, 255};
    SDL_Event ev;
    int cx = ancho/2, cy = alto/2;
    int pw = ancho - 96, ph = alto - 96;
    int px = 48, py = 48;

    while (1) {
        Uint32 ahora = SDL_GetTicks();
        if (!pausado) elapsed = (ahora - inicio - pausa_acum) / 1000;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) return -1;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) return 0;
                if (ev.key.key == SDLK_P) {
                    if (!pausado) { pausado=1; pausa_inicio=ahora; }
                    else { pausa_acum += ahora - pausa_inicio; pausado=0; }
                }
                if (ev.key.key == SDLK_0) {
                    inicio=SDL_GetTicks(); pausa_acum=0; elapsed=0; pausado=0;
                }
            }
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw, "CRONOMETRO");
        pom_timer(renderer, fr, fuente, cx, cy, (int)elapsed, pausado, "[ CRONOMETRO ]", c_cyan);

        if (pausado && (SDL_GetTicks()/500)%2==0) {
            const char *pt = "PAUSADO";
            int pw2; TTF_GetStringSize(fuente, pt, 0, &pw2, NULL);
            pom_txt(renderer, fuente, pt, cx-pw2/2, cy+78, c_amar);
        }

        const char *help = "[p] pausar   [0] reiniciar   [ESC] menu";
        int hw; TTF_GetStringSize(fuente, help, 0, &hw, NULL);
        pom_txt(renderer, fuente, help, cx-hw/2, py+ph-28, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
}

/* ── TEMPORIZADOR CUSTOM ───────────────────────────────────────────── */
static int
pom_modo_timer(SDL_Renderer *renderer, TTF_Font *ft, TTF_Font *fr,
               TTF_Font *fuente, int ancho, int alto)
{
    char input[8] = ""; int input_len = 0;
    int configurando = 1;
    SDL_Color c_verde = {  0, 220,  80, 255};
    SDL_Color c_amar  = {220, 200,   0, 255};
    SDL_Color c_dim   = { 80,  80,  80, 255};
    SDL_Event ev;
    int cx = ancho/2, cy = alto/2;
    int pw = ancho - 96, ph = alto - 96;
    int px = 48, py = 48;

    ui_text_input(true);

    while (configurando) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) { ui_text_input(false); return -1; }
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) { ui_text_input(false); return 0; }
                if (ev.key.key == SDLK_BACKSPACE && input_len > 0)
                    input[--input_len] = '\0';
                if (ev.key.key == SDLK_RETURN && input_len > 0)
                    configurando = 0;
            }
            if (ev.type == SDL_EVENT_TEXT_INPUT && input_len < 3 &&
                ev.text.text[0] >= '0' && ev.text.text[0] <= '9') {
                input[input_len++] = ev.text.text[0];
                input[input_len]   = '\0';
            }
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw, "TEMPORIZADOR");

        const char *preg = "Cuantos minutos?";
        int preg_w; TTF_GetStringSize(fuente, preg, 0, &preg_w, NULL);
        pom_txt(renderer, fuente, preg, cx - preg_w/2, cy - 60, c_verde);

        char disp[16]; snprintf(disp, sizeof(disp), "%s_", input);
        int dw; TTF_GetStringSize(fr, disp, 0, &dw, NULL);
        pom_txt(renderer, fr, disp, cx - dw/2, cy - 20, c_amar);

        const char *h = "[numeros]   [Enter] confirmar   [ESC] volver";
        int hw; TTF_GetStringSize(fuente, h, 0, &hw, NULL);
        pom_txt(renderer, fuente, h, cx-hw/2, py+ph-28, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
    ui_text_input(false);

    int total = atoi(input) * 60;
    if (total <= 0) total = 60;
    int seg = total, pausado = 0;
    Uint32 ultimo = SDL_GetTicks();

    while (1) {
        Uint32 ahora = SDL_GetTicks();
        if (!pausado && ahora - ultimo >= 1000) {
            ultimo += 1000;
            if (seg > 0) seg--;
        }
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) return -1;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) return 0;
                if (ev.key.key == SDLK_P) { pausado = !pausado; ultimo = SDL_GetTicks(); }
                if (ev.key.key == SDLK_0) { seg=total; pausado=0; ultimo=SDL_GetTicks(); }
            }
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw, "TEMPORIZADOR");

        char lbl[32]; snprintf(lbl, sizeof(lbl), "[ %s min ]", input);
        pom_timer(renderer, fr, fuente, cx, cy, seg, pausado, lbl, c_amar);

        if (seg == 0) {
            const char *fin = "TIEMPO COMPLETADO";
            int fw; TTF_GetStringSize(fuente, fin, 0, &fw, NULL);
            pom_txt(renderer, fuente, fin, cx-fw/2, cy+60, c_verde);
        }
        if (pausado && (SDL_GetTicks()/500)%2==0) {
            const char *pt = "PAUSADO";
            int pw2; TTF_GetStringSize(fuente, pt, 0, &pw2, NULL);
            pom_txt(renderer, fuente, pt, cx-pw2/2, cy+60, c_amar);
        }

        const char *help = "[p] pausar   [0] reiniciar   [ESC] menu";
        int hw; TTF_GetStringSize(fuente, help, 0, &hw, NULL);
        pom_txt(renderer, fuente, help, cx-hw/2, py+ph-28, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
}

/* ── ESTADISTICAS ──────────────────────────────────────────────────── */
static void
pom_modo_stats(SDL_Renderer *renderer, TTF_Font *ft, TTF_Font *fuente,
               int ancho, int alto, int ciclos)
{
    SDL_Color c_verde = {  0, 220,  80, 255};
    SDL_Color c_amar  = {220, 200,   0, 255};
    SDL_Color c_dim   = { 80,  80,  80, 255};
    SDL_Event ev;
    int cx = ancho/2, cy = alto/2;
    int pw = ancho - 96, ph = alto - 96;
    int px = 48, py = 48;
    int lh = TTF_GetFontHeight(fuente) + 12;

    while (1) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT)    return;
            if (ev.type == SDL_EVENT_KEY_DOWN) return;
        }

        pom_fondo(renderer, ancho, alto);
        pom_panel(renderer, px, py, pw, ph);
        pom_header(renderer, ft, fuente, px, py, pw, "ESTADISTICAS");

        int box_w = 380, box_h = 3*lh + 24;
        int box_x = cx - box_w/2, box_y = cy - box_h/2;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 30, 10, 180);
        SDL_FRect box = {box_x, box_y, box_w, box_h};
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 140, 50, 255);
        SDL_RenderRect(renderer, &box);

        char buf[64];
        snprintf(buf, sizeof(buf), "Pomodoros completados: %d", ciclos);
        int bw; TTF_GetStringSize(fuente, buf, 0, &bw, NULL);
        pom_txt(renderer, fuente, buf, cx-bw/2, box_y+14, c_verde);

        snprintf(buf, sizeof(buf), "Tiempo de trabajo: %d min", ciclos*25);
        TTF_GetStringSize(fuente, buf, 0, &bw, NULL);
        pom_txt(renderer, fuente, buf, cx-bw/2, box_y+14+lh, c_amar);

        snprintf(buf, sizeof(buf), "Tiempo de descanso: %d min", ciclos*5);
        TTF_GetStringSize(fuente, buf, 0, &bw, NULL);
        pom_txt(renderer, fuente, buf, cx-bw/2, box_y+14+lh*2, c_dim);

        const char *h = "Presiona cualquier tecla para volver";
        int hw; TTF_GetStringSize(fuente, h, 0, &hw, NULL);
        pom_txt(renderer, fuente, h, cx-hw/2, py+ph-28, c_dim);

        presente(renderer);
        SDL_Delay(16);
    }
}

/* ── ENTRY POINT ───────────────────────────────────────────────────── */
int
screenPomodoro(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto)
{
    audio_fade_out(1500);

    TTF_Font *ft = TTF_OpenFont("assets/fonts/main.ttf", 26);
    TTF_Font *fr = TTF_OpenFont("assets/fonts/main.ttf", 64);
    if (!ft) ft = fuente;
    if (!fr) fr = fuente;

    int ciclos = 0;

    while (1) {
        int opcion = pom_menu(renderer, ft, fuente, ancho, alto);
        if (opcion <= 0) break;
        switch (opcion) {
            case 1: if (pom_modo_pomodoro(renderer, ft, fr, fuente, ancho, alto, &ciclos) < 0) goto fin; break;
            case 2: if (pom_modo_cronometro(renderer, ft, fr, fuente, ancho, alto) < 0) goto fin; break;
            case 3: if (pom_modo_timer(renderer, ft, fr, fuente, ancho, alto) < 0) goto fin; break;
            case 4: pom_modo_stats(renderer, ft, fuente, ancho, alto, ciclos); break;
            case 5: goto fin;
        }
    }

fin:
    if (ft != fuente) TTF_CloseFont(ft);
    if (fr != fuente) TTF_CloseFont(fr);

    audio_fade_in(MUSICA_MENU, 3000, 0);
    return 0;
}
