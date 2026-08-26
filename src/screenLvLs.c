#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ui.h"
#include "niveles.h"
#include "progreso.h"
#include "pomodoro_bg.h"
#include "audio.h"
#include <math.h>

/* Dibuja una estrella de 5 puntas rellena centrada en (cx, cy) con radio r.
   Usa scanline fill sobre el poligono de 10 vertices (5 ext + 5 int). */
static void
dibujar_estrella(SDL_Renderer *renderer, int cx, int cy, int r, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    float pts[10][2];
    float ri = r * 0.42f;   /* radio interior */

    for (int i = 0; i < 5; i++) {
        float ao = (float)(-M_PI / 2.0 + i * 2.0 * M_PI / 5.0);
        float ai = ao + (float)(M_PI / 5.0);
        pts[i*2  ][0] = cx + cosf(ao) * r;
        pts[i*2  ][1] = cy + sinf(ao) * r;
        pts[i*2+1][0] = cx + cosf(ai) * ri;
        pts[i*2+1][1] = cy + sinf(ai) * ri;
    }

    /* scanline fill */
    for (int y = cy - r; y <= cy + r; y++) {
        float xs[20];
        int   nx = 0;
        for (int i = 0; i < 10; i++) {
            int   j  = (i + 1) % 10;
            float y0 = pts[i][1], y1 = pts[j][1];
            float x0 = pts[i][0], x1 = pts[j][0];
            if ((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                xs[nx++] = x0 + ((float)y - y0) / (y1 - y0) * (x1 - x0);
        }
        /* ordenar intersecciones (burbuja, max 10 elementos) */
        for (int a = 0; a < nx - 1; a++)
            for (int b = a + 1; b < nx; b++)
                if (xs[a] > xs[b]) { float t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
        for (int a = 0; a + 1 < nx; a += 2)
            SDL_RenderLine(renderer, (int)xs[a], y, (int)xs[a+1], y);
    }
}

/* Dibuja N estrellas seguidas desde el punto (x, y) */
static void
dibujar_estrellas(SDL_Renderer *renderer, int n, int x, int y, int r, SDL_Color color)
{
    int gap = r * 2 + 5;
    for (int i = 0; i < n; i++)
        dibujar_estrella(renderer, x + i * gap + r, y, r, color);
}

/* Dibuja un candado centrado en (lx, ly) */
static void
dibujar_candado(SDL_Renderer *renderer, int lx, int ly)
{
    SDL_Color c = {75, 78, 85, 255};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);

    /* argolla: dos laterales + techo */
    SDL_FRect sl  = {lx-10, ly-22, 4, 24};   /* lado izq  */
    SDL_FRect sr  = {lx+ 6, ly-22, 4, 24};   /* lado der  */
    SDL_FRect tch = {lx-10, ly-22, 20, 4};   /* techo     */
    SDL_RenderFillRect(renderer, &sl);
    SDL_RenderFillRect(renderer, &sr);
    SDL_RenderFillRect(renderer, &tch);

    /* cuerpo */
    SDL_FRect body = {lx-16, ly, 32, 24};
    SDL_RenderFillRect(renderer, &body);

    /* ojo del candado */
    SDL_SetRenderDrawColor(renderer, 38, 38, 42, 255);
    SDL_FRect ojo = {lx-4, ly+6, 8, 12};
    SDL_RenderFillRect(renderer, &ojo);
}

/* ── dibuja texto con alpha custom, sin offset ── */
static void
neon_texto(SDL_Renderer *renderer, TTF_Font *fuente,
           const char *texto, int x, int y, SDL_Color color, Uint8 alpha)
{
    SDL_Surface *sup = TTF_RenderText_Blended(fuente, texto, 0, color);
    if (!sup) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, sup);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tex, alpha);
    SDL_FRect pos = {x, y, sup->w, sup->h};
    SDL_RenderTexture(renderer, tex, NULL, &pos);
    SDL_DestroySurface(sup);
    SDL_DestroyTexture(tex);
}

/* ── pool de pseudocódigo para el fondo ── */
static const char *bg_pool[] = {
    "funcion buscar(arr, n, x):",
    "  para i = 0 hasta n-1:",
    "    si arr[i] == x:",
    "      retornar i",
    "  retornar -1",
    " ",
    "proc insertar(pila, val):",
    "  si tope < MAX:",
    "    tope = tope + 1",
    "    pila[tope] = val",
    "  sino:",
    "    mostrar('pila llena')",
    " ",
    "funcion factorial(n):",
    "  si n <= 1:",
    "    retornar 1",
    "  retornar n * factorial(n-1)",
    " ",
    "proc burbuja(arr, n):",
    "  para i = 0 hasta n-1:",
    "    para j = 0 hasta n-i-2:",
    "      si arr[j] > arr[j+1]:",
    "        temp = arr[j]",
    "        arr[j] = arr[j+1]",
    "        arr[j+1] = temp",
    " ",
    "funcion minimo(pila P):",
    "  min = tope(P)",
    "  mientras no vacia(P):",
    "    x = desapilar(P)",
    "    si x < min:",
    "      min = x",
    "  retornar min",
    " ",
    "proc inorden(nodo):",
    "  si nodo != nulo:",
    "    inorden(nodo.izq)",
    "    mostrar(nodo.dato)",
    "    inorden(nodo.der)",
    " ",
    "funcion altura(nodo):",
    "  si nodo == nulo: retornar 0",
    "  izq = altura(nodo.izq)",
    "  der = altura(nodo.der)",
    "  retornar 1 + max(izq, der)",
    " ",
    "proc encolar(cola, val):",
    "  si fin < MAX:",
    "    cola[fin] = val",
    "    fin = fin + 1",
    " ",
    "funcion esta_vacia(pila):",
    "  retornar tope == -1",
    " ",
    "proc intercambiar(arr, i, j):",
    "  temp = arr[i]",
    "  arr[i] = arr[j]",
    "  arr[j] = temp",
};

/* ── sistema de editor vivo ── */
#define BG_VIS 16

typedef enum { BGL_ESTABLE, BGL_BORRANDO, BGL_ESCRIBIENDO } BgLineaEst;

typedef struct {
    char       texto[256];   /* contenido actual visible    */
    char       target[256];  /* contenido objetivo          */
    int        vis;          /* chars visibles ahora        */
    BgLineaEst estado;
    Uint32     timer;
    int        delay;
} BgVis;

typedef struct {
    BgVis  lineas[BG_VIS];
    int    activa;           /* línea que se está editando  */
    Uint32 next_edit;        /* cuándo elegir nueva activa  */
} BgEditor;

#define BG_POOL_N ((int)(sizeof(bg_pool) / sizeof(bg_pool[0])))

static void
bg_editor_init(BgEditor *ed)
{
    int pool_i = 0;
    for (int i = 0; i < BG_VIS; i++) {
        const char *src = bg_pool[pool_i % BG_POOL_N];
        pool_i++;
        strncpy(ed->lineas[i].texto,  src, 255);
        strncpy(ed->lineas[i].target, src, 255);
        ed->lineas[i].texto[255]  = '\0';
        ed->lineas[i].target[255] = '\0';
        ed->lineas[i].vis    = (int)strlen(src);
        ed->lineas[i].estado = BGL_ESTABLE;
        ed->lineas[i].timer  = 0;
        ed->lineas[i].delay  = 45;
    }
    ed->activa    = 0;
    ed->next_edit = SDL_GetTicks() + 600;
}

static void
bg_editor_tick(BgEditor *ed)
{
    Uint32 now = SDL_GetTicks();

    /* elegir nueva línea activa si es tiempo */
    if (now >= ed->next_edit) {
        int candidata = rand() % BG_VIS;
        if (ed->lineas[candidata].estado == BGL_ESTABLE) {
            ed->activa = candidata;
            /* asignar nuevo target del pool */
            const char *nuevo = bg_pool[rand() % BG_POOL_N];
            strncpy(ed->lineas[candidata].target, nuevo, 255);
            ed->lineas[candidata].target[255] = '\0';
            ed->lineas[candidata].estado = BGL_BORRANDO;
            ed->lineas[candidata].delay  = 18 + rand() % 20;
            ed->lineas[candidata].timer  = now + ed->lineas[candidata].delay;
        }
        ed->next_edit = now + 400 + rand() % 800;
    }

    /* actualizar cada línea */
    for (int i = 0; i < BG_VIS; i++) {
        BgVis *l = &ed->lineas[i];
        if (now < l->timer) continue;

        int cur_len = (int)strlen(l->texto);
        int tgt_len = (int)strlen(l->target);

        switch (l->estado) {
            case BGL_ESTABLE:
                break;
            case BGL_BORRANDO:
                if (cur_len > 0) {
                    l->texto[cur_len - 1] = '\0';
                    l->vis = cur_len - 1;
                    l->timer = now + l->delay;
                } else {
                    /* vacío: empezar a escribir el target */
                    l->estado = BGL_ESCRIBIENDO;
                    l->delay  = 40 + rand() % 40;
                    l->timer  = now + l->delay;
                }
                break;
            case BGL_ESCRIBIENDO:
                cur_len = (int)strlen(l->texto);
                if (cur_len < tgt_len) {
                    l->texto[cur_len]     = l->target[cur_len];
                    l->texto[cur_len + 1] = '\0';
                    l->vis  = cur_len + 1;
                    l->timer = now + l->delay;
                } else {
                    l->estado = BGL_ESTABLE;
                }
                break;
        }
    }
}

static void
bg_editor_dibujar(SDL_Renderer *renderer, TTF_Font *fuente,
                  BgEditor *ed, int col_x, int y0, int line_h)
{
    SDL_Color c_est  = {0, 200, 110, 255};
    SDL_Color c_edit = {0, 255, 160, 255};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < BG_VIS; i++) {
        if (ed->lineas[i].vis <= 0 && ed->lineas[i].estado == BGL_ESTABLE) {
            /* línea vacía (separador) */
            continue;
        }
        SDL_Color c = (i == ed->activa) ? c_edit : c_est;
        Uint8     a = (i == ed->activa) ? 55 : 28;
        neon_texto(renderer, fuente, ed->lineas[i].texto,
                   col_x, y0 + i * line_h, c, a);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

int
screenLvLs(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto)
{
    SDL_Event evento;
    int total = total_niveles();

    TTF_Font *fuente_grande = TTF_OpenFont("assets/fonts/main.ttf", 42);

    BgEditor bg_izq, bg_der;
    srand(SDL_GetTicks());
    bg_editor_init(&bg_izq);
    bg_editor_init(&bg_der);

    /* layout de tarjetas */
    int cols   = 3;
    int card_w = 200;
    int card_h = 150;
    int gap_x  = 30;
    int gap_y  = 25;
    int grid_w = cols * card_w + (cols - 1) * gap_x;
    int start_x = (ancho - grid_w) / 2;
    int start_y = 100;

    int rows_total = (total + cols - 1) / cols;
    int rbtn_h     = 36;
    /* altura total del contenido */
    int contenido_h = start_y + rows_total * (card_h + gap_y) + rbtn_h + 40;

    int scroll_y   = 0;
    int scroll_max = 0;   /* se recalcula cada frame */
    int seleccionado = 0; /* índice del episodio enfocado (0-based) */

    while (1) {
        int clicked = 0, click_x = 0, click_y = 0;
        pom_tick();

        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_EVENT_QUIT) {
                if (fuente_grande) TTF_CloseFont(fuente_grande);
                return 0;
            }
            if (evento.type == SDL_EVENT_KEY_DOWN) {
                switch (evento.key.key) {
                    case SDLK_ESCAPE:
                        if (fuente_grande) TTF_CloseFont(fuente_grande);
                        return 0;
                    case SDLK_LEFT: {
                        int sig = seleccionado - 1;
                        while (sig >= 0 && !nivel_desbloqueado(sig + 1)) sig--;
                        if (sig >= 0) seleccionado = sig;
                        break;
                    }
                    case SDLK_RIGHT: {
                        int sig = seleccionado + 1;
                        while (sig < total && !nivel_desbloqueado(sig + 1)) sig++;
                        if (sig < total) seleccionado = sig;
                        break;
                    }
                    case SDLK_UP: {
                        int sig = seleccionado - cols;
                        while (sig >= 0 && !nivel_desbloqueado(sig + 1)) sig -= cols;
                        if (sig >= 0) seleccionado = sig;
                        break;
                    }
                    case SDLK_DOWN: {
                        int sig = seleccionado + cols;
                        while (sig < total && !nivel_desbloqueado(sig + 1)) sig += cols;
                        if (sig < total) seleccionado = sig;
                        break;
                    }
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER: {
                        int num = seleccionado + 1;
                        if (nivel_desbloqueado(num)) {
                            audio_sfx_btn();
                            if (fuente_grande) TTF_CloseFont(fuente_grande);
                            return num;
                        }
                        break;
                    }
                    case SDLK_P: pom_send("p", 1); break;
                    case SDLK_0: pom_send("0", 1); break;
                    default: break;
                }
            }
            if (evento.type == SDL_EVENT_MOUSE_WHEEL) {
                scroll_y -= evento.wheel.y * 45;  /* y>0 = rueda arriba */
                if (scroll_y < 0) scroll_y = 0;
            }
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                evento.button.button == SDL_BUTTON_LEFT) {
                clicked = 1;
                click_x = evento.button.x;
                click_y = evento.button.y;
            }
        }

        /* recalcular clamp del scroll */
        scroll_max = contenido_h - alto;
        if (scroll_max < 0) scroll_max = 0;
        if (scroll_y > scroll_max) scroll_y = scroll_max;

        /* auto-scroll: mantener la tarjeta seleccionada visible */
        {
            int row_sel = seleccionado / cols;
            int cy_sel  = start_y + row_sel * (card_h + gap_y) - scroll_y;
            if (cy_sel < 80)
                scroll_y -= (80 - cy_sel);
            if (cy_sel + card_h > alto - 20)
                scroll_y += (cy_sel + card_h - (alto - 20));
            if (scroll_y < 0) scroll_y = 0;
            if (scroll_y > scroll_max) scroll_y = scroll_max;
        }

        SDL_SetRenderDrawColor(renderer, 15, 15, 22, 255);
        SDL_RenderClear(renderer);

        /* clip: no dibujar fuera de la pantalla */
        SDL_FRect viewport = {0, 0, ancho, alto};
        ui_clip(renderer, &viewport);

        int mx, my;
        ui_mouse(&mx, &my);

        /* ── fondo: dos columnas de código vivo ── */
        int line_h = TTF_GetFontHeight(fuente) + 3;
        bg_editor_tick(&bg_izq);
        bg_editor_tick(&bg_der);
        bg_editor_dibujar(renderer, fuente, &bg_izq, 8,        95, line_h);
        bg_editor_dibujar(renderer, fuente, &bg_der, ancho/2+20, 95, line_h);

        /* ── ETAPA 1: neon con glow ── */
        SDL_Color c_glow = {0, 255, 160, 255};
        int etapa_w = 0;
        if (fuente_grande)
            TTF_GetStringSize(fuente_grande, "ETAPA 1", 0, &etapa_w, NULL);
        int etapa_x = (ancho - etapa_w) / 2;
        int etapa_y = 8;
        if (fuente_grande) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (int g = 8; g >= 1; g--) {
                Uint8 a = (Uint8)(50 / g);
                neon_texto(renderer, fuente_grande, "ETAPA 1",
                           etapa_x - g*2, etapa_y - g, c_glow, a);
                neon_texto(renderer, fuente_grande, "ETAPA 1",
                           etapa_x + g*2, etapa_y + g, c_glow, a);
            }
            neon_texto(renderer, fuente_grande, "ETAPA 1",
                       etapa_x, etapa_y, c_glow, 230);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }


        for (int i = 0; i < total; i++) {
            Nivel *nv = obtener_nivel(i + 1);
            if (!nv) continue;

            int num   = i + 1;
            int col   = i % cols;
            int row   = i / cols;
            int cx    = start_x + col * (card_w + gap_x);
            int cy    = start_y + row  * (card_h + gap_y) - scroll_y;

            /* saltar tarjetas completamente fuera de pantalla */
            if (cy + card_h < 0 || cy > alto) continue;

            int desbloqueado = nivel_desbloqueado(num);
            int completado   = esta_completado(num);
            int hover        = desbloqueado &&
                               mx >= cx && mx <= cx + card_w &&
                               my >= cy && my <= cy + card_h;
            int enfocado     = (i == seleccionado);

            SDL_FRect card = {cx, cy, card_w, card_h};

            /* fondo de la tarjeta */
            if (!desbloqueado) {
                SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
            } else if (completado) {
                SDL_SetRenderDrawColor(renderer,
                    (hover||enfocado) ? 10 : 8,
                    (hover||enfocado) ? 90 : 70,
                    (hover||enfocado) ? 45 : 35, 255);
            } else {
                SDL_SetRenderDrawColor(renderer,
                    (hover||enfocado) ? 25 : 18,
                    (hover||enfocado) ? 60 : 40,
                    (hover||enfocado) ? 120 : 90, 255);
            }
            SDL_RenderFillRect(renderer, &card);

            /* glow neon si está enfocado */
            if (enfocado && desbloqueado) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                for (int g = 4; g >= 1; g--) {
                    Uint8 a = (Uint8)(12 / g);
                    SDL_SetRenderDrawColor(renderer, 0, 255, 160, a);
                    SDL_FRect glow = {cx - g*2, cy - g*2,
                                     card_w + g*4, card_h + g*4};
                    SDL_RenderFillRect(renderer, &glow);
                }
                /* borde sólido encima */
                SDL_SetRenderDrawColor(renderer, 0, 255, 160, 80);
                SDL_FRect b0 = {cx-2,      cy-2,      card_w+4, 2};
                SDL_FRect b1 = {cx-2,      cy+card_h, card_w+4, 2};
                SDL_FRect b2 = {cx-2,      cy-2,      2, card_h+4};
                SDL_FRect b3 = {cx+card_w, cy-2,      2, card_h+4};
                SDL_RenderFillRect(renderer, &b0);
                SDL_RenderFillRect(renderer, &b1);
                SDL_RenderFillRect(renderer, &b2);
                SDL_RenderFillRect(renderer, &b3);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            /* borde superior de color */
            SDL_FRect borde = {cx, cy, card_w, 4};
            if (!desbloqueado)
                SDL_SetRenderDrawColor(renderer, 55, 55, 60, 255);
            else if (completado)
                SDL_SetRenderDrawColor(renderer, 60, 220, 100, 255);
            else
                SDL_SetRenderDrawColor(renderer, 60, 130, 255, 255);
            SDL_RenderFillRect(renderer, &borde);

            if (!desbloqueado) {
                /* ── Nivel bloqueado: solo candado centrado ── */
                dibujar_candado(renderer, cx + card_w / 2, cy + card_h / 2);

                /* numero de nivel en gris muy dim */
                char str_num[32];
                snprintf(str_num, sizeof(str_num), "Episodio %d", num);
                SDL_Color c_dim = {50, 50, 55, 255};
                dibujadoTextoColor(renderer, fuente, str_num, cx, cy + 8, c_dim);

            } else {
                /* ── Nivel desbloqueado: contenido normal ── */
                SDL_Color c_num, c_tit, c_pts;
                if (completado) {
                    c_num = (SDL_Color){80,  220, 110, 255};
                    c_tit = (SDL_Color){200, 255, 210, 255};
                    c_pts = (SDL_Color){60,  180,  90, 255};
                } else {
                    c_num = (SDL_Color){100, 160, 255, 255};
                    c_tit = (SDL_Color){220, 230, 255, 255};
                    c_pts = (SDL_Color){80,  120, 200, 255};
                }

                char str_num[32];
                snprintf(str_num, sizeof(str_num), "Episodio %d", num);
                dibujadoTextoColor(renderer, fuente, str_num,    cx, cy +  8, c_num);
                dibujadoTextoColor(renderer, fuente, nv->titulo, cx, cy + 38, c_tit);

                if (completado)
                    dibujadoTextoColor(renderer, fuente, "[OK]", cx, cy + 68, c_num);

                /* estrellas de dificultad dibujadas con SDL */
                int estrellas = (int)(strlen(nv->dificultad) / 3);
                if (estrellas < 1) estrellas = 1;
                SDL_Color c_dif;
                if      (estrellas <= 1) c_dif = (SDL_Color){ 80, 220, 100, 255};
                else if (estrellas == 2) c_dif = (SDL_Color){255, 200,  50, 255};
                else if (estrellas == 3) c_dif = (SDL_Color){255, 130,  20, 255};
                else                    c_dif = (SDL_Color){255,  50,  50, 255};
                SDL_Color c_lbl = {100, 100, 120, 255};
                int dif_y = cy + card_h - 90;
                dibujadoTextoColor(renderer, fuente, "Dificultad:",
                                   cx, dif_y, c_lbl);
                int lbl_w2 = 0;
                TTF_GetStringSize(fuente, "Dificultad: ", 0, &lbl_w2, NULL);
                dibujar_estrellas(renderer, estrellas,
                                  cx + lbl_w2 + 10, dif_y + 20, 6, c_dif);

                dibujadoTextoColor(renderer, fuente, "100 pts",
                                   cx, cy + card_h - 38, c_pts);

                /* click */
                if (clicked &&
                    click_x >= cx && click_x <= cx + card_w &&
                    click_y >= cy && click_y <= cy + card_h) {
                    audio_sfx_btn();
                    if (fuente_grande) TTF_CloseFont(fuente_grande);
                    return num;
                }
            }
        }

        /* boton resetear progreso */
        int rbtn_w = 220;
        int rbtn_x = (ancho - rbtn_w) / 2;
        int rbtn_y = start_y + rows_total * (card_h + gap_y) + 10 - scroll_y;

        if (rbtn_y + rbtn_h >= 0 && rbtn_y <= alto) {
            int rhover = mx >= rbtn_x && mx <= rbtn_x + rbtn_w &&
                         my >= rbtn_y && my <= rbtn_y + rbtn_h;
            SDL_SetRenderDrawColor(renderer,
                rhover ? 160 : 100, rhover ? 20 : 12, rhover ? 20 : 12, 255);
            SDL_FRect rbtn = {rbtn_x, rbtn_y, rbtn_w, rbtn_h};
            SDL_RenderFillRect(renderer, &rbtn);
            SDL_Color c_reset = {255, 160, 160, 255};
            dibujadoTextoColor(renderer, fuente, "Resetear progreso",
                               rbtn_x, rbtn_y, c_reset);

            if (clicked &&
                click_x >= rbtn_x && click_x <= rbtn_x + rbtn_w &&
                click_y >= rbtn_y && click_y <= rbtn_y + rbtn_h) {
                audio_sfx_btn();
                resetear_progreso();
            }
        }

        /* barra de scroll indicadora (derecha) */
        if (scroll_max > 0) {
            int bar_x = ancho - 6;
            int bar_h = alto * alto / contenido_h;
            if (bar_h < 20) bar_h = 20;
            int bar_y = (int)((long long)scroll_y * (alto - bar_h) / scroll_max);
            SDL_SetRenderDrawColor(renderer, 60, 60, 80, 200);
            SDL_FRect bar_bg = {bar_x, 0, 5, alto};
            SDL_RenderFillRect(renderer, &bar_bg);
            SDL_SetRenderDrawColor(renderer, 120, 120, 160, 255);
            SDL_FRect bar = {bar_x, bar_y, 5, bar_h};
            SDL_RenderFillRect(renderer, &bar);
        }

        ui_clip(renderer, NULL);
        presente(renderer);
        SDL_Delay(16);
    }

    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL API — NivelesState
 *  Mismo patron que DocState / EditorLibreState.
 *  Cuando el usuario selecciona un nivel se setea abrir_nivel > 0;
 *  shell.c lo lee con niveles_consumir_nivel() y llama screenLvLEditor.
 * ════════════════════════════════════════════════════════════════════════════ */

struct NivelesState {
    BgEditor  bg_izq, bg_der;
    int       scroll_y;
    int       seleccionado;
    int       total;
    TTF_Font *fuente_grande;
    int       clicked;
    int       click_x, click_y;
    int       abrir_nivel;   /* > 0 → shell.c abre ese nivel */
};

NivelesState *
niveles_crear(TTF_Font *fuente)
{
    (void)fuente;
    NivelesState *s = calloc(1, sizeof(*s));
    srand((unsigned)SDL_GetTicks());
    bg_editor_init(&s->bg_izq);
    bg_editor_init(&s->bg_der);
    s->fuente_grande = TTF_OpenFont("assets/fonts/main.ttf", 42);
    s->total         = total_niveles();

    /* iniciar seleccion en el primer nivel sin completar (el "nivel actual") */
    s->seleccionado = 0;
    for (int i = 0; i < s->total; i++) {
        if (nivel_desbloqueado(i + 1) && !esta_completado(i + 1)) {
            s->seleccionado = i;
            break;
        }
    }
    return s;
}

void
niveles_destruir(NivelesState *s)
{
    if (!s) return;
    if (s->fuente_grande) TTF_CloseFont(s->fuente_grande);
    free(s);
}

int
niveles_consumir_nivel(NivelesState *s)
{
    int n = s->abrir_nivel;
    s->abrir_nivel = 0;
    return n;
}

void
niveles_refrescar(NivelesState *s)
{
    s->total = total_niveles();
    /* mover seleccion al nuevo nivel actual */
    for (int i = 0; i < s->total; i++) {
        if (nivel_desbloqueado(i + 1) && !esta_completado(i + 1)) {
            s->seleccionado = i;
            break;
        }
    }
}

void
niveles_evento(NivelesState *s, SDL_Event *e, SDL_FRect area)
{
    int total = s->total;
    int cols  = 3;

    if (e->type == SDL_EVENT_KEY_DOWN) {
        switch (e->key.key) {
            case SDLK_LEFT: {
                int sig = s->seleccionado - 1;
                while (sig >= 0 && !nivel_desbloqueado(sig + 1)) sig--;
                if (sig >= 0) s->seleccionado = sig;
                break;
            }
            case SDLK_RIGHT: {
                int sig = s->seleccionado + 1;
                while (sig < total && !nivel_desbloqueado(sig + 1)) sig++;
                if (sig < total) s->seleccionado = sig;
                break;
            }
            case SDLK_UP: {
                int sig = s->seleccionado - cols;
                while (sig >= 0 && !nivel_desbloqueado(sig + 1)) sig -= cols;
                if (sig >= 0) s->seleccionado = sig;
                break;
            }
            case SDLK_DOWN: {
                int sig = s->seleccionado + cols;
                while (sig < total && !nivel_desbloqueado(sig + 1)) sig += cols;
                if (sig < total) s->seleccionado = sig;
                break;
            }
            case SDLK_RETURN:
            case SDLK_KP_ENTER: {
                int num = s->seleccionado + 1;
                if (nivel_desbloqueado(num)) {
                    audio_sfx_btn();
                    s->abrir_nivel = num;
                }
                break;
            }
            default: break;
        }
    }
    if (e->type == SDL_EVENT_MOUSE_WHEEL) {
        s->scroll_y -= e->wheel.y * 45;
        if (s->scroll_y < 0) s->scroll_y = 0;
    }
    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT) {
        s->clicked = 1;
        s->click_x = e->button.x - area.x;
        s->click_y = e->button.y - area.y;
    }
}

void
niveles_dibujar(NivelesState *s, SDL_Renderer *renderer, TTF_Font *fuente, SDL_FRect area)
{
    int ancho = area.w, alto = area.h;
    int total = s->total;
    int cols   = 3;
    int card_w = 200, card_h = 150;
    int gap_x  = 30,  gap_y  = 25;
    int grid_w = cols * card_w + (cols - 1) * gap_x;
    int start_x = (ancho - grid_w) / 2;
    int start_y = 100;
    int rows_total  = (total + cols - 1) / cols;
    int rbtn_h      = 36;
    int contenido_h = start_y + rows_total * (card_h + gap_y) + rbtn_h + 40;

    /* clamp scroll */
    int scroll_max = contenido_h - alto;
    if (scroll_max < 0) scroll_max = 0;
    if (s->scroll_y > scroll_max) s->scroll_y = scroll_max;

    /* auto-scroll: mantener tarjeta seleccionada visible */
    {
        int row_sel = s->seleccionado / cols;
        int cy_sel  = start_y + row_sel * (card_h + gap_y) - s->scroll_y;
        if (cy_sel < 80)              s->scroll_y -= (80 - cy_sel);
        if (cy_sel + card_h > alto - 20) s->scroll_y += (cy_sel + card_h - (alto - 20));
        if (s->scroll_y < 0)          s->scroll_y = 0;
        if (s->scroll_y > scroll_max) s->scroll_y = scroll_max;
    }

    ui_viewport(renderer, &area);
    ui_clip(renderer, NULL);

    /* fondo */
    SDL_SetRenderDrawColor(renderer, 15, 15, 22, 255);
    SDL_RenderFillRect(renderer, &(SDL_FRect){0, 0, ancho, alto});

    /* mouse relativo al area */
    int mx, my;
    ui_mouse(&mx, &my);
    mx -= area.x; my -= area.y;

    /* fondo animado */
    int lh = TTF_GetFontHeight(fuente) + 3;
    bg_editor_tick(&s->bg_izq);
    bg_editor_tick(&s->bg_der);
    bg_editor_dibujar(renderer, fuente, &s->bg_izq, 8,          95, lh);
    bg_editor_dibujar(renderer, fuente, &s->bg_der, ancho/2+20, 95, lh);

    /* titulo ETAPA 1 */
    SDL_Color c_glow = {0, 255, 160, 255};
    int etapa_w = 0;
    if (s->fuente_grande)
        TTF_GetStringSize(s->fuente_grande, "ETAPA 1", 0, &etapa_w, NULL);
    int etapa_x = (ancho - etapa_w) / 2;
    int etapa_y = 8;
    if (s->fuente_grande) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int g = 8; g >= 1; g--) {
            Uint8 a = (Uint8)(50 / g);
            neon_texto(renderer, s->fuente_grande, "ETAPA 1",
                       etapa_x - g*2, etapa_y - g, c_glow, a);
            neon_texto(renderer, s->fuente_grande, "ETAPA 1",
                       etapa_x + g*2, etapa_y + g, c_glow, a);
        }
        neon_texto(renderer, s->fuente_grande, "ETAPA 1",
                   etapa_x, etapa_y, c_glow, 230);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    /* hallar el nivel actual (primero desbloqueado sin completar) */
    int nivel_actual = -1;
    for (int i = 0; i < total; i++) {
        if (nivel_desbloqueado(i + 1) && !esta_completado(i + 1)) {
            nivel_actual = i;
            break;
        }
    }

    /* tarjetas */
    for (int i = 0; i < total; i++) {
        Nivel *nv = obtener_nivel(i + 1);
        if (!nv) continue;

        int num  = i + 1;
        int col  = i % cols;
        int row  = i / cols;
        int cx   = start_x + col * (card_w + gap_x);
        int cy   = start_y + row * (card_h + gap_y) - s->scroll_y;

        if (cy + card_h < 0 || cy > alto) continue;

        int desbloqueado = nivel_desbloqueado(num);
        int completado   = esta_completado(num);
        int hover        = desbloqueado &&
                           mx >= cx && mx < cx+card_w &&
                           my >= cy && my < cy+card_h;
        int enfocado     = (i == s->seleccionado);
        int es_actual    = (i == nivel_actual);

        SDL_FRect card = {cx, cy, card_w, card_h};

        /* fondo tarjeta */
        if (!desbloqueado) {
            SDL_SetRenderDrawColor(renderer, 28, 28, 32, 255);
        } else if (completado) {
            SDL_SetRenderDrawColor(renderer,
                (hover||enfocado)?10:8, (hover||enfocado)?90:70, (hover||enfocado)?45:35, 255);
        } else {
            SDL_SetRenderDrawColor(renderer,
                (hover||enfocado)?25:18, (hover||enfocado)?60:40, (hover||enfocado)?120:90, 255);
        }
        SDL_RenderFillRect(renderer, &card);

        /* glow neon si enfocado */
        if (enfocado && desbloqueado) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            for (int g = 4; g >= 1; g--) {
                Uint8 a = (Uint8)(12 / g);
                SDL_SetRenderDrawColor(renderer, 0, 255, 160, a);
                SDL_FRect glow = {cx-g*2, cy-g*2, card_w+g*4, card_h+g*4};
                SDL_RenderFillRect(renderer, &glow);
            }
            SDL_SetRenderDrawColor(renderer, 0, 255, 160, 80);
            SDL_RenderFillRect(renderer, &(SDL_FRect){cx-2,      cy-2,      card_w+4, 2});
            SDL_RenderFillRect(renderer, &(SDL_FRect){cx-2,      cy+card_h, card_w+4, 2});
            SDL_RenderFillRect(renderer, &(SDL_FRect){cx-2,      cy-2,      2, card_h+4});
            SDL_RenderFillRect(renderer, &(SDL_FRect){cx+card_w, cy-2,      2, card_h+4});
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        /* borde superior de color */
        SDL_FRect borde = {cx, cy, card_w, 4};
        if (!desbloqueado)
            SDL_SetRenderDrawColor(renderer, 55,  55,  60,  255);
        else if (completado)
            SDL_SetRenderDrawColor(renderer, 60,  220, 100, 255);
        else if (es_actual)
            SDL_SetRenderDrawColor(renderer, 255, 200, 50,  255);  /* amarillo = nivel actual */
        else
            SDL_SetRenderDrawColor(renderer, 60,  130, 255, 255);
        SDL_RenderFillRect(renderer, &borde);

        if (!desbloqueado) {
            dibujar_candado(renderer, cx + card_w/2, cy + card_h/2);
            char str_num[32];
            snprintf(str_num, sizeof(str_num), "Episodio %d", num);
            SDL_Color c_dim = {50, 50, 55, 255};
            dibujadoTextoColor(renderer, fuente, str_num, cx, cy + 8, c_dim);
        } else {
            SDL_Color c_num, c_tit, c_pts;
            if (completado) {
                c_num = (SDL_Color){80,  220, 110, 255};
                c_tit = (SDL_Color){200, 255, 210, 255};
                c_pts = (SDL_Color){60,  180,  90, 255};
            } else if (es_actual) {
                c_num = (SDL_Color){255, 200,  50,  255};  /* amarillo */
                c_tit = (SDL_Color){255, 240, 180,  255};
                c_pts = (SDL_Color){200, 160,  40,  255};
            } else {
                c_num = (SDL_Color){100, 160, 255, 255};
                c_tit = (SDL_Color){220, 230, 255, 255};
                c_pts = (SDL_Color){80,  120, 200, 255};
            }

            char str_num[32];
            snprintf(str_num, sizeof(str_num), "Episodio %d", num);
            dibujadoTextoColor(renderer, fuente, str_num,    cx, cy +  8, c_num);
            dibujadoTextoColor(renderer, fuente, nv->titulo, cx, cy + 38, c_tit);

            if (completado)
                dibujadoTextoColor(renderer, fuente, "[OK]",   cx, cy + 68, c_num);
            else if (es_actual)
                dibujadoTextoColor(renderer, fuente, "<-", cx, cy + 68, c_num);

            /* estrellas */
            int estrellas = (int)(strlen(nv->dificultad) / 3);
            if (estrellas < 1) estrellas = 1;
            SDL_Color c_dif;
            if      (estrellas <= 1) c_dif = (SDL_Color){ 80, 220, 100, 255};
            else if (estrellas == 2) c_dif = (SDL_Color){255, 200,  50, 255};
            else if (estrellas == 3) c_dif = (SDL_Color){255, 130,  20, 255};
            else                    c_dif = (SDL_Color){255,  50,  50, 255};
            SDL_Color c_lbl = {100, 100, 120, 255};
            int dif_y = cy + card_h - 90;
            dibujadoTextoColor(renderer, fuente, "Dificultad:", cx, dif_y, c_lbl);
            int lbl_w2 = 0;
            TTF_GetStringSize(fuente, "Dificultad: ", 0, &lbl_w2, NULL);
            dibujar_estrellas(renderer, estrellas, cx + lbl_w2 + 10, dif_y + 20, 6, c_dif);

            dibujadoTextoColor(renderer, fuente, "100 pts", cx, cy + card_h - 38, c_pts);

            /* click */
            if (s->clicked &&
                s->click_x >= cx && s->click_x < cx+card_w &&
                s->click_y >= cy && s->click_y < cy+card_h) {
                audio_sfx_btn();
                s->abrir_nivel = num;
            }
        }
    }

    /* boton resetear progreso */
    int rbtn_w = 220;
    int rbtn_x = (ancho - rbtn_w) / 2;
    int rbtn_y = start_y + rows_total * (card_h + gap_y) + 10 - s->scroll_y;
    if (rbtn_y + rbtn_h >= 0 && rbtn_y <= alto) {
        int rhover = mx >= rbtn_x && mx < rbtn_x+rbtn_w &&
                     my >= rbtn_y && my < rbtn_y+rbtn_h;
        SDL_SetRenderDrawColor(renderer, rhover?160:100, rhover?20:12, rhover?20:12, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){rbtn_x, rbtn_y, rbtn_w, rbtn_h});
        SDL_Color c_reset = {255, 160, 160, 255};
        dibujadoTextoColor(renderer, fuente, "Resetear progreso",
                           rbtn_x, rbtn_y, c_reset);
        if (s->clicked &&
            s->click_x >= rbtn_x && s->click_x < rbtn_x+rbtn_w &&
            s->click_y >= rbtn_y && s->click_y < rbtn_y+rbtn_h) {
            audio_sfx_btn();
            resetear_progreso();
        }
    }

    /* scrollbar */
    if (scroll_max > 0) {
        int bar_x = ancho - 6;
        int bar_h = alto * alto / contenido_h;
        if (bar_h < 20) bar_h = 20;
        int bar_y = (int)((long long)s->scroll_y * (alto - bar_h) / scroll_max);
        SDL_SetRenderDrawColor(renderer, 60, 60, 80, 200);
        SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, 0, 5, alto});
        SDL_SetRenderDrawColor(renderer, 120, 120, 160, 255);
        SDL_RenderFillRect(renderer, &(SDL_FRect){bar_x, bar_y, 5, bar_h});
    }

    s->clicked = 0;
    ui_viewport(renderer, NULL);
}
