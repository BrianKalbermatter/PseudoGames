#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "ui.h"
#include "pomodoro_bg.h"
#include "audio.h"
#define MAX_COLS    80
#define N_OPCIONES   7

typedef struct { int x, y, speed; } Columna;

/* ── Particulas para la explosion de letras ── */
#define MAX_PARTS 48
typedef struct { float x, y, vx, vy, alpha; char ch[2]; } Part;

static void
explosion(SDL_Renderer *renderer, TTF_Font *fuente,
          const char *texto, int cx, int cy)
{
    Part pts[MAX_PARTS];
    int  n = 0;
    int  cw, ch_sz;
    TTF_SizeUTF8(fuente, "A", &cw, &ch_sz);
    int  len = (int)strlen(texto);
    int  sx  = cx - (len * cw) / 2;

    for (int i = 0; i < len && n < MAX_PARTS; i++) {
        if (texto[i] == ' ') continue;
        pts[n].x     = (float)(sx + i * cw);
        pts[n].y     = (float)cy;
        float ang    = (rand() % 628) / 100.0f;   /* 0..2π */
        float spd    = 1.5f + (rand() % 35) / 10.0f;
        pts[n].vx    = cosf(ang) * spd;
        pts[n].vy    = sinf(ang) * spd - 2.0f;    /* sesgo hacia arriba */
        pts[n].alpha = 255.0f;
        pts[n].ch[0] = texto[i];
        pts[n].ch[1] = '\0';
        n++;
    }

    Uint32 t0 = SDL_GetTicks();
    while (SDL_GetTicks() - t0 < 380) {
        /* oscurecer el frame anterior */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 50);
        SDL_RenderFillRect(renderer, NULL);

        for (int i = 0; i < n; i++) {
            pts[i].x    += pts[i].vx;
            pts[i].y    += pts[i].vy;
            pts[i].vy   += 0.28f;          /* gravedad */
            pts[i].alpha -= 9.0f;
            if (pts[i].alpha <= 0) continue;

            SDL_Color c = {0, 255, 80, (Uint8)pts[i].alpha};
            SDL_Surface *s = TTF_RenderUTF8_Blended(fuente, pts[i].ch, c);
            if (!s) continue;
            SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
            SDL_Rect dst   = {(int)pts[i].x, (int)pts[i].y, s->w, s->h};
            SDL_RenderCopy(renderer, t, NULL, &dst);
            SDL_FreeSurface(s);
            SDL_DestroyTexture(t);
        }
        presente(renderer);
        SDL_Delay(16);
    }
}

int
screenMenu(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto)
{
    SDL_Event evento;

    /* ── Matrix rain ── */
    srand((unsigned int)SDL_GetTicks());
    Columna cols[MAX_COLS];
    int espaciado = ancho / MAX_COLS;
    for (int i = 0; i < MAX_COLS; i++) {
        cols[i].x     = i * espaciado;
        cols[i].y     = -(rand() % alto);
        cols[i].speed = 2 + rand() % 6;
    }

    SDL_Color verde_cabeza = {180, 255, 180, 255};
    SDL_Color verde_cuerpo = {0,   200,  50, 210};
    SDL_Color verde_cola   = {0,   120,  30, 150};
    SDL_Texture *rain[2][3] = {0};
    SDL_Color paleta[3] = { verde_cabeza, verde_cuerpo, verde_cola };
    const char *bits[2] = {"0", "1"};
    int ch_w = 0, ch_h = 0;
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 3; p++) {
            SDL_Surface *s = TTF_RenderUTF8_Blended(fuente, bits[c], paleta[p]);
            if (s) {
                rain[c][p] = SDL_CreateTextureFromSurface(renderer, s);
                if (c == 0 && p == 0) { ch_w = s->w; ch_h = s->h; }
                SDL_FreeSurface(s);
            }
        }

    /* ── Título pulsante ── */
    TTF_Font *fuente_titulo = TTF_OpenFont("assets/fonts/main.ttf", 72);
    SDL_Texture *tex_titulo = NULL;
    int titulo_w = 0, titulo_h = 0;
    if (fuente_titulo) {
        SDL_Color ambar = {0, 255, 80, 255};
        SDL_Surface *sup = TTF_RenderUTF8_Blended(fuente_titulo, "PSEUDOGAMES", ambar);
        if (sup) {
            titulo_w = sup->w; titulo_h = sup->h;
            tex_titulo = SDL_CreateTextureFromSurface(renderer, sup);
            SDL_FreeSurface(sup);
        }
        TTF_CloseFont(fuente_titulo);
    }
    if (tex_titulo) SDL_SetTextureBlendMode(tex_titulo, SDL_BLENDMODE_BLEND);

    /* ── Labels cacheados (normal=ambar, hover=blanco) ── */
    const char *labels[N_OPCIONES] = {
        "Jugar", "DOC", "Pomodoro", "Editor Libre",
        "Seleccion de Nivel", "Soluciones", "Salir"
    };
    const char *nums[N_OPCIONES] = {
        "[1]","[2]","[3]","[4]","[5]","[6]","[7]"
    };

    SDL_Color c_ambar  = {  0, 230,  70, 255};  /* neon green */
    SDL_Color c_blanco = {180, 255, 180, 255};  /* neon hover  */
    SDL_Color c_dim    = {  0,  70,  25, 255};  /* green oscuro */

    SDL_Texture *lbl_n[N_OPCIONES] = {0};
    SDL_Texture *lbl_h[N_OPCIONES] = {0};
    SDL_Texture *num_t[N_OPCIONES] = {0};
    int lbl_w[N_OPCIONES] = {0}, lbl_hh[N_OPCIONES] = {0};
    int num_w = 0, num_hh = 0;
    TTF_SizeUTF8(fuente, "[1]", &num_w, &num_hh);

    for (int i = 0; i < N_OPCIONES; i++) {
        SDL_Surface *sn  = TTF_RenderUTF8_Blended(fuente, labels[i], c_ambar);
        SDL_Surface *sh  = TTF_RenderUTF8_Blended(fuente, labels[i], c_blanco);
        SDL_Surface *snu = TTF_RenderUTF8_Blended(fuente, nums[i],   c_dim);
        if (sn) {
            lbl_w[i] = sn->w; lbl_hh[i] = sn->h;
            lbl_n[i] = SDL_CreateTextureFromSurface(renderer, sn);
            SDL_FreeSurface(sn);
        }
        if (sh) { lbl_h[i] = SDL_CreateTextureFromSurface(renderer, sh); SDL_FreeSurface(sh); }
        if (snu){ num_t[i] = SDL_CreateTextureFromSurface(renderer, snu); SDL_FreeSurface(snu); }
    }

    /* ── Cursor > (blanco brillante) ── */
    int cur_w = 0, cur_hh = 0;
    TTF_SizeUTF8(fuente, ">", &cur_w, &cur_hh);
    SDL_Texture *tex_cur = NULL;
    {
        SDL_Surface *sc = TTF_RenderUTF8_Blended(fuente, ">", c_blanco);
        if (sc) { tex_cur = SDL_CreateTextureFromSurface(renderer, sc); SDL_FreeSurface(sc); }
    }

    /* ── Ícono engranaje (Nerd Font U+E615) ── */
    SDL_Texture *tex_gear = NULL;
    int gear_iw = 0, gear_ih = 0;
    {
        TTF_Font *fgear = TTF_OpenFont("assets/fonts/nerd.ttf", 28);
        if (fgear) {
            SDL_Color cg = {0, 200, 60, 255};
            SDL_Surface *sg = TTF_RenderUTF8_Blended(fgear, "\xee\x98\x95", cg); /* U+E615 */
            if (sg) {
                gear_iw = sg->w; gear_ih = sg->h;
                tex_gear = SDL_CreateTextureFromSurface(renderer, sg);
                SDL_FreeSurface(sg);
            }
            TTF_CloseFont(fgear);
        }
    }

    /* ── Layout del panel RPG ── */
    int row_h   = lbl_hh[0] + 20;
    int pad_x   = 22;
    int pad_y   = 18;
    int panel_w = num_w + cur_w + 14 + lbl_w[4] + pad_x * 2 + 8; /* ancho minimo para "Seleccion de Nivel" */
    int panel_h = pad_y * 2 + N_OPCIONES * row_h;
    int panel_x = (ancho - panel_w) / 2;
    int panel_y = alto / 2 - panel_h / 2 + 28;

    /* ── Overlay de bienvenida (pre-renderizado) ── */
    /* tipo: 0=cuerpo  1=header(verde)  2=warning(ambar) */
    const char *bv_lineas[] = {
        ">> Busco testers para mi juego indie (ALPHA)",
        "",
        "Estoy desarrollando este entrenador y busco",
        "ayuda para mejorarlo durante la cursada.",
        "Usalo como herramienta de apoyo: refuerza",
        "conceptos cuando tengas dudas, sin reemplazar",
        "la practica en hoja.",
        "",
        ">> Como puedes ayudar?",
        "  + Probando el juego y reportando lo que no anda",
        "  + Creando Issues en GitHub  (seccion Games)",
        "  + Diciendo que partes no se entienden",
        "",
        "[!] Windows aun en progreso (desarrollo en Linux)",
        "    Con tu ayuda tambien lo haremos portable.",
        "",
        "Gracias a todos por la ayuda!",
    };
    int bv_tipo[] = { 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, 0, 0, 1 };
    int bv_n = (int)(sizeof(bv_lineas) / sizeof(bv_lineas[0]));

    SDL_Color c_bv_hdr  = {  0, 220,  70, 255};  /* header verde neon  */
    SDL_Color c_bv_txt  = {180, 180, 190, 255};  /* cuerpo gris claro  */
    SDL_Color c_bv_warn = {255, 160,  40, 255};  /* warning ambar      */
    SDL_Color c_bv_ver  = {255, 200,  50, 255};  /* version            */
    SDL_Color c_bv_cont = {100, 100, 110, 255};  /* press any key      */

#define BV_MAX 20
    SDL_Texture *bv_tex[BV_MAX] = {0};
    int bv_w[BV_MAX] = {0}, bv_h[BV_MAX] = {0};
    for (int i = 0; i < bv_n; i++) {
        if (!bv_lineas[i][0]) continue;
        SDL_Color col = bv_tipo[i] == 1 ? c_bv_hdr
                      : bv_tipo[i] == 2 ? c_bv_warn
                      : c_bv_txt;
        SDL_Surface *s = TTF_RenderUTF8_Blended(fuente, bv_lineas[i], col);
        if (s) { bv_w[i] = s->w; bv_h[i] = s->h;
                 bv_tex[i] = SDL_CreateTextureFromSurface(renderer, s);
                 SDL_FreeSurface(s); }
    }
    SDL_Texture *bv_ver = NULL; int bv_ver_w = 0, bv_ver_h = 0;
    { SDL_Surface *s = TTF_RenderUTF8_Blended(fuente, "v0.14 - alpha", c_bv_ver);
      if (s) { bv_ver_w = s->w; bv_ver_h = s->h;
               bv_ver = SDL_CreateTextureFromSurface(renderer, s); SDL_FreeSurface(s); } }

    SDL_Texture *bv_cont = NULL; int bv_cont_w = 0, bv_cont_h = 0;
    { SDL_Surface *s = TTF_RenderUTF8_Blended(fuente,
          "[ Presiona cualquier tecla o haz click ]", c_bv_cont);
      if (s) { bv_cont_w = s->w; bv_cont_h = s->h;
               bv_cont = SDL_CreateTextureFromSurface(renderer, s); SDL_FreeSurface(s); } }
    if (bv_cont) SDL_SetTextureBlendMode(bv_cont, SDL_BLENDMODE_BLEND);

    int bv_line_h = 21;
    int bv_pad    = 24;
    int bv_pw     = 560;
    int bv_ph     = bv_pad * 2 + titulo_h + 14 + bv_n * bv_line_h + 44;
    int bv_px     = (ancho - bv_pw) / 2;
    int bv_py     = (alto  - bv_ph) / 2;

    static int bienvenida_activa = 1;  /* static: solo 1 en toda la sesion */

    /* ── Estado de corrupcion/glitch ── */
    int   glitch_activo = 0;
    int   glitch_frames = 0;
    Uint32 next_glitch  = SDL_GetTicks() + 1500 + (Uint32)(rand() % 4000);

    int resultado = -1;   /* -1 = seguir corriendo */
    Uint32 menu_habilitado_en = 0;  /* 0 = no habilitado aun */

    while (resultado < 0) {
        int clicked = 0, click_x = 0, click_y = 0;
        int tecla   = 0;   /* cualquier tecla presionada este frame */
        pom_tick();

        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_QUIT) { resultado = 0; break; }
            if (evento.type == SDL_KEYDOWN) {
                tecla = 1;
                if (bienvenida_activa) break;  /* solo cierra la bienvenida */
                if (menu_habilitado_en && SDL_GetTicks() < menu_habilitado_en) break; /* cooldown */
                int k = evento.key.keysym.sym;
                if (k >= SDLK_1 && k <= SDLK_7) {
                    audio_sfx_btn();
                    int idx  = k - SDLK_1;
                    int ex   = panel_x + pad_x + num_w + cur_w + 14 + lbl_w[idx] / 2;
                    int ey   = panel_y + pad_y + idx * row_h + row_h / 2;
                    explosion(renderer, fuente, labels[idx], ex, ey);
                    resultado = (idx == N_OPCIONES - 1) ? 0 : idx + 1;
                    break;
                }
                if (k == SDLK_p) pom_send("p", 1);
                if (k == SDLK_0) pom_send("0", 1);
            }
            if (evento.type == SDL_MOUSEBUTTONDOWN &&
                evento.button.button == SDL_BUTTON_LEFT) {
                if (bienvenida_activa) break;  /* solo cierra la bienvenida */
                if (menu_habilitado_en && SDL_GetTicks() < menu_habilitado_en) break; /* cooldown */
                clicked = 1;
                click_x = evento.button.x;
                click_y = evento.button.y;
            }
        }

        /* ── Matrix rain ── */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 25);
        SDL_RenderFillRect(renderer, NULL);

        for (int i = 0; i < MAX_COLS; i++) {
            int ci = rand() % 2;
            SDL_Rect d0 = { cols[i].x, cols[i].y, ch_w, ch_h };
            if (rain[ci][0]) SDL_RenderCopy(renderer, rain[ci][0], NULL, &d0);
            if (cols[i].y - ch_h >= 0) {
                int c2 = rand() % 2;
                SDL_Rect d1 = { cols[i].x, cols[i].y - ch_h, ch_w, ch_h };
                if (rain[c2][1]) SDL_RenderCopy(renderer, rain[c2][1], NULL, &d1);
            }
            if (cols[i].y - ch_h * 2 >= 0) {
                int c3 = rand() % 2;
                SDL_Rect d2 = { cols[i].x, cols[i].y - ch_h*2, ch_w, ch_h };
                if (rain[c3][2]) SDL_RenderCopy(renderer, rain[c3][2], NULL, &d2);
            }
            cols[i].y += cols[i].speed;
            if (cols[i].y > alto) {
                cols[i].y     = -(rand() % 300);
                cols[i].speed = 2 + rand() % 6;
            }
        }

        /* ── Vidrio: velo oscuro sobre la lluvia ── */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 4, 0, 140);
        SDL_RenderFillRect(renderer, NULL);

        /* ── Titulo pulsante ── */
        if (tex_titulo) {
            float t     = SDL_GetTicks() / 1000.0f;
            float scale = 1.0f + 0.07f * sinf(t * 0.8f);
            int   tw    = (int)(titulo_w * scale);
            int   th    = (int)(titulo_h * scale);
            Uint8 alpha = (Uint8)(160 + 60 * sinf(t * 1.1f));
            SDL_SetTextureAlphaMod(tex_titulo, alpha);
            SDL_Rect dst = {(ancho - tw) / 2, panel_y - th - 16, tw, th};
            SDL_RenderCopy(renderer, tex_titulo, NULL, &dst);
        }

        /* ── Panel: fondo + borde doble estilo RPG ── */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 4, 6, 4, 210);
        SDL_Rect panel = {panel_x, panel_y, panel_w, panel_h};
        SDL_RenderFillRect(renderer, &panel);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer, 0, 220, 70, 255);   /* borde externo neon */
        SDL_RenderDrawRect(renderer, &panel);
        SDL_Rect inner = {panel_x+2, panel_y+2, panel_w-4, panel_h-4};
        SDL_SetRenderDrawColor(renderer, 0, 70, 25, 255);    /* borde interno oscuro */
        SDL_RenderDrawRect(renderer, &inner);

        /* ── Glitch de corrupcion ── */
        Uint32 ahora = SDL_GetTicks();
        if (!glitch_activo && ahora >= next_glitch) {
            glitch_activo = 1;
            glitch_frames = 3 + rand() % 7;
        }
        if (glitch_activo) {
            /* scanlines de ruido horizontal */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            int n_lines = 2 + rand() % 5;
            for (int g = 0; g < n_lines; g++) {
                int gy  = rand() % alto;
                int gh  = 1 + rand() % 4;
                int gx  = rand() % (ancho / 2);   /* offset horizontal aleatorio */
                Uint8 ga = (Uint8)(40 + rand() % 100);
                SDL_SetRenderDrawColor(renderer, 0, 255, 80, ga);
                SDL_Rect gr = {gx, gy, ancho - gx, gh};
                SDL_RenderFillRect(renderer, &gr);
            }
            glitch_frames--;
            if (glitch_frames <= 0) {
                glitch_activo = 0;
                next_glitch   = ahora + 1200 + (Uint32)(rand() % 5000);
            }
        }

        /* ── Opciones ── */
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        float t = SDL_GetTicks() / 1000.0f;
        int cursor_pulse = (int)(4.0f * sinf(t * 5.0f));  /* pulso horizontal del > */

        for (int i = 0; i < N_OPCIONES; i++) {
            int oy    = panel_y + pad_y + i * row_h;
            int ox    = panel_x + pad_x;
            int hover = (mx >= panel_x + 4 && mx <= panel_x + panel_w - 4 &&
                         my >= oy + 4       && my <= oy + row_h - 4);

            /* corrupcion: alpha y offset random cuando hay glitch */
            int   gx_off = (glitch_activo && rand() % 3 == 0) ? (rand() % 9 - 4) : 0;
            Uint8 g_alpha = (glitch_activo && rand() % 4 == 0) ? (Uint8)(40 + rand() % 80) : 255;

            /* [N] en gris/dim */
            if (num_t[i]) {
                SDL_Rect rn = {ox, oy + (row_h - num_hh) / 2, num_w, num_hh};
                SDL_RenderCopy(renderer, num_t[i], NULL, &rn);
            }

            int text_x = ox + num_w + cur_w + 14;

            if (hover) {
                /* cursor > con pulso */
                if (tex_cur) {
                    SDL_Rect rc = {ox + num_w + 4 + cursor_pulse,
                                   oy + (row_h - cur_hh) / 2, cur_w, cur_hh};
                    SDL_RenderCopy(renderer, tex_cur, NULL, &rc);
                }
                /* label hover: sin glitch */
                if (lbl_h[i]) {
                    SDL_SetTextureAlphaMod(lbl_h[i], 255);
                    SDL_Rect rl = {text_x, oy + (row_h - lbl_hh[i]) / 2,
                                   lbl_w[i], lbl_hh[i]};
                    SDL_RenderCopy(renderer, lbl_h[i], NULL, &rl);
                }
                /* click */
                if (clicked &&
                    click_x >= panel_x + 4 && click_x <= panel_x + panel_w - 4 &&
                    click_y >= oy + 4       && click_y <= oy + row_h - 4) {
                    audio_sfx_btn();
                    int ex = text_x + lbl_w[i] / 2;
                    int ey = oy + row_h / 2;
                    explosion(renderer, fuente, labels[i], ex, ey);
                    resultado = (i == N_OPCIONES - 1) ? 0 : i + 1;
                }
            } else {
                /* label neon: con glitch de alpha y offset */
                if (lbl_n[i]) {
                    SDL_SetTextureAlphaMod(lbl_n[i], g_alpha);
                    SDL_Rect rl = {text_x + gx_off, oy + (row_h - lbl_hh[i]) / 2,
                                   lbl_w[i], lbl_hh[i]};
                    SDL_RenderCopy(renderer, lbl_n[i], NULL, &rl);
                    SDL_SetTextureAlphaMod(lbl_n[i], 255);  /* resetear */
                }
            }
        }

        /* ── Engranaje config (esquina inferior derecha) ── */
        int gear_size = 34;
        int gear_x    = ancho - gear_size - 14;
        int gear_y    = alto  - gear_size - 14;
        int gear_cx   = gear_x + gear_size / 2;
        int gear_cy   = gear_y + gear_size / 2;

        /* medir etiqueta para expandir zona de click */
        int cfg_lw = 0, cfg_lh = 0;
        TTF_SizeUTF8(fuente, "Configuracion", &cfg_lw, &cfg_lh);
        int lbl_x  = gear_x - cfg_lw - 10;   /* a la izquierda del engranaje */
        int lbl_y  = gear_y + (gear_size - cfg_lh) / 2;

        /* zona de hover unificada: texto + engranaje */
        int hit_x1 = lbl_x;
        int hit_x2 = gear_x + gear_size;
        int hit_y1 = gear_y;
        int hit_y2 = gear_y + gear_size;
        int gear_hover = (mx >= hit_x1 && mx <= hit_x2 &&
                          my >= hit_y1 && my <= hit_y2);

        SDL_Color c_cfg_lbl = gear_hover ? (SDL_Color){180, 255, 180, 255}
                                         : (SDL_Color){  0, 100,  35, 255};
        dibujadoTextoColor(renderer, fuente, "Configuracion",
                           lbl_x - 10, lbl_y - 12, c_cfg_lbl);

        if (tex_gear) {
            /* ícono ⚙ cacheado */
            Uint8 ga = gear_hover ? 255 : 160;
            SDL_SetTextureColorMod(tex_gear,
                gear_hover ? 0   : 0,
                gear_hover ? 255 : 160,
                gear_hover ? 80  : 50);
            SDL_SetTextureAlphaMod(tex_gear, ga);
            SDL_Rect gr = {gear_x + (gear_size - gear_iw) / 2,
                           gear_y + (gear_size - gear_ih) / 2,
                           gear_iw, gear_ih};
            SDL_RenderCopy(renderer, tex_gear, NULL, &gr);
        } else {
            /* fallback: arandela dibujada */
            SDL_Color c_gear = gear_hover ? (SDL_Color){0,255,80,255}
                                          : (SDL_Color){0,100,35,255};
            SDL_Color c_hole = {4, 6, 4, 255};
            dibujarArandela(renderer, gear_cx, gear_cy, gear_size/2-2, c_gear, c_hole);
        }

        if (clicked &&
            click_x >= hit_x1 && click_x <= hit_x2 &&
            click_y >= hit_y1 && click_y <= hit_y2) {
            audio_sfx_btn();
            resultado = 7;
        }

        /* ── Overlay de bienvenida ── */
        if (bienvenida_activa) {
            float t2 = SDL_GetTicks() / 1000.0f;

            /* sombra sobre el menu */
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
            SDL_RenderFillRect(renderer, NULL);

            /* panel */
            SDL_SetRenderDrawColor(renderer, 4, 8, 4, 240);
            SDL_Rect bvp = {bv_px, bv_py, bv_pw, bv_ph};
            SDL_RenderFillRect(renderer, &bvp);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 0, 200, 60, 255);
            SDL_RenderDrawRect(renderer, &bvp);
            SDL_Rect bvi = {bv_px+2, bv_py+2, bv_pw-4, bv_ph-4};
            SDL_SetRenderDrawColor(renderer, 0, 60, 20, 255);
            SDL_RenderDrawRect(renderer, &bvi);

            /* version arriba a la derecha del panel */
            if (bv_ver) {
                SDL_Rect dr = {bv_px + bv_pw - bv_ver_w - 12,
                               bv_py + 10, bv_ver_w, bv_ver_h};
                SDL_RenderCopy(renderer, bv_ver, NULL, &dr);
            }

            /* titulo pulsante */
            if (tex_titulo) {
                float sc    = 0.65f + 0.04f * sinf(t2 * 1.4f);
                int   tw    = (int)(titulo_w * sc);
                int   th    = (int)(titulo_h * sc);
                Uint8 alp   = (Uint8)(180 + 60 * sinf(t2 * 1.8f));
                SDL_SetTextureAlphaMod(tex_titulo, alp);
                SDL_Rect dr = {bv_px + (bv_pw - tw) / 2, bv_py + bv_pad, tw, th};
                SDL_RenderCopy(renderer, tex_titulo, NULL, &dr);
                SDL_SetTextureAlphaMod(tex_titulo, 255);
            }

            /* separador */
            int sep = bv_py + bv_pad + titulo_h + 10;
            SDL_SetRenderDrawColor(renderer, 0, 70, 25, 255);
            SDL_RenderDrawLine(renderer, bv_px+14, sep, bv_px+bv_pw-14, sep);

            /* lineas de texto */
            int ty = sep + 10;
            for (int i = 0; i < bv_n; i++) {
                if (bv_tex[i]) {
                    SDL_Rect dr = {bv_px + 28, ty, bv_w[i], bv_h[i]};
                    SDL_RenderCopy(renderer, bv_tex[i], NULL, &dr);
                }
                ty += bv_line_h;
            }

            /* "presiona tecla" parpadeante */
            if (bv_cont) {
                float fase  = fmodf(t2, 1.2f);
                Uint8 alp   = fase < 0.85f ? (Uint8)(80 + 120 * sinf(fase * 3.14f)) : 0;
                SDL_SetTextureAlphaMod(bv_cont, alp);
                SDL_Rect dr = {bv_px + (bv_pw - bv_cont_w) / 2,
                               bv_py + bv_ph - bv_cont_h - 12,
                               bv_cont_w, bv_cont_h};
                SDL_RenderCopy(renderer, bv_cont, NULL, &dr);
            }

            /* cerrar con tecla o click: arrancar musica con fade in suave */
            if (tecla || clicked) {
                audio_sfx_btn();
                bienvenida_activa = 0;
                menu_habilitado_en = SDL_GetTicks() + 1000; /* 1 seg de gracia */
                audio_fade_in("assets/Audio/specular_city_dance_extended.ogg", 2000, 0);
            }
        }

        presente(renderer);
        SDL_Delay(16);
    }

    /* ── Cleanup ── */
    if (bv_ver)  SDL_DestroyTexture(bv_ver);
    if (bv_cont) SDL_DestroyTexture(bv_cont);
    for (int i = 0; i < BV_MAX; i++)
        if (bv_tex[i]) SDL_DestroyTexture(bv_tex[i]);
    if (tex_titulo) SDL_DestroyTexture(tex_titulo);
    if (tex_cur)    SDL_DestroyTexture(tex_cur);
    if (tex_gear)   SDL_DestroyTexture(tex_gear);
    for (int c = 0; c < 2; c++)
        for (int p = 0; p < 3; p++)
            if (rain[c][p]) SDL_DestroyTexture(rain[c][p]);
    for (int i = 0; i < N_OPCIONES; i++) {
        if (lbl_n[i]) SDL_DestroyTexture(lbl_n[i]);
        if (lbl_h[i]) SDL_DestroyTexture(lbl_h[i]);
        if (num_t[i]) SDL_DestroyTexture(num_t[i]);
    }
    return resultado;
}
