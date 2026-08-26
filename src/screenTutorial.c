#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "audio.h"

/* ── Constantes de layout ─────────────────────────────────── */
#define MAX_LINEAS   1024
#define MAX_LEN_LIN  512
#define MARGEN_X     60
#define LINE_H       22
#define HEADER_H     52
#define FOOTER_H     52
#define WRAP_W       680

/* ── Tipos de linea ──────────────────────────────────────── */
typedef enum {
    LIN_NORMAL, LIN_H1, LIN_H2, LIN_H3,
    LIN_SEPARADOR, LIN_CODIGO, LIN_CITA, LIN_VACIA
} TipoLinea;

typedef struct {
    char      texto[MAX_LEN_LIN];
    TipoLinea tipo;
} Linea;

typedef struct {
    SDL_Texture *tex;
    int          w, h, extra_x;
} TexCache;

/* ── Estado estatico (sobrevive entre llamadas) ──────────── */
static Linea    lineas[MAX_LINEAS];
static TexCache cache[MAX_LINEAS];
static int      n_lineas  = 0;
static int      cache_ok  = 0;

/* Texturas fijas del header/footer — se crean una sola vez */
static SDL_Texture *tex_titulo  = NULL;  int tw_titulo,  th_titulo;
static SDL_Texture *tex_hint    = NULL;  int tw_hint,    th_hint;
static SDL_Texture *tex_btn     = NULL;  int tw_btn,     th_btn;
static SDL_Texture *tex_esc     = NULL;  int tw_esc,     th_esc;

/* ── Helpers ─────────────────────────────────────────────── */
static TipoLinea
detectar_tipo(const char *s, int en_codigo)
{
    if (en_codigo)                  return LIN_CODIGO;
    if (strncmp(s, "### ", 4) == 0) return LIN_H3;
    if (strncmp(s, "## ",  3) == 0) return LIN_H2;
    if (strncmp(s, "# ",   2) == 0) return LIN_H1;
    if (strncmp(s, "---",  3) == 0) return LIN_SEPARADOR;
    if (strncmp(s, "> ",   2) == 0) return LIN_CITA;
    if (s[0] == '\0')               return LIN_VACIA;
    return LIN_NORMAL;
}

static const char *
limpiar_prefijo(const char *s, TipoLinea tipo)
{
    switch (tipo) {
        case LIN_H1:   return s + 2;
        case LIN_H2:   return s + 3;
        case LIN_H3:   return s + 4;
        case LIN_CITA: return s + 2;
        default:       return s;
    }
}

/* Crea una textura opaca (texto pre-compuesto sobre bg).
   Sin alpha en el blit → en software renderer es un memcpy puro. */
static SDL_Texture *
crear_tex(SDL_Renderer *r, TTF_Font *f, const char *txt,
          SDL_Color col, int wrap, int *ow, int *oh, SDL_Color bg)
{
    SDL_Surface *tsup = TTF_RenderText_Blended_Wrapped(
                            f, txt, 0, col, wrap > 0 ? wrap : 0);
    if (!tsup) { *ow = 0; *oh = LINE_H; return NULL; }

    /* superficie opaca del mismo tamaño en color de fondo */
    /* SDL3: CreateSurface ya no pide flags ni profundidad de bits: el
       formato de pixel solo ya dice cuantos bits tiene cada canal.
       Y SDL_MapRGB toma la SURFACE, no un puntero a su formato, porque
       en SDL3 el formato es un enum y no una estructura. */
    SDL_Surface *bsup = SDL_CreateSurface(tsup->w, tsup->h, SDL_PIXELFORMAT_RGB24);
    SDL_FillSurfaceRect(bsup, NULL, SDL_MapSurfaceRGB(bsup, bg.r, bg.g, bg.b));

    /* compositar texto (con alpha) sobre el fondo opaco */
    SDL_BlitSurface(tsup, NULL, bsup, NULL);
    SDL_DestroySurface(tsup);

    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, bsup);
    *ow = bsup->w; *oh = bsup->h;
    SDL_DestroySurface(bsup);

    /* sin blend: blit = memcpy, sin operaciones por pixel */
    if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    return tex;
}

/* ── Carga el .md ────────────────────────────────────────── */
static int
cargar_md(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int n = 0, en_codigo = 0;
    char buf[MAX_LEN_LIN];
    while (fgets(buf, sizeof(buf), f) && n < MAX_LINEAS) {
        int len = (int)strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
        if (strncmp(buf, "```", 3) == 0) { en_codigo = !en_codigo; continue; }
        lineas[n].tipo = detectar_tipo(buf, en_codigo);
        strncpy(lineas[n].texto, buf, MAX_LEN_LIN - 1);
        lineas[n].texto[MAX_LEN_LIN - 1] = '\0';
        n++;
    }
    fclose(f);
    return n;
}

/* colores de fondo por tipo de linea (deben coincidir con lo que
   dibujamos como rect de fondo en el render loop) */
static SDL_Color bg_por_tipo[] = {
    { 12,  12,  20, 255},  /* LIN_NORMAL    */
    { 12,  12,  20, 255},  /* LIN_H1        */
    { 12,  12,  20, 255},  /* LIN_H2        */
    { 12,  12,  20, 255},  /* LIN_H3        */
    {  0,   0,   0,   0},  /* LIN_SEPARADOR */
    { 22,  22,  32, 255},  /* LIN_CODIGO    */
    { 28,  52,  40, 255},  /* LIN_CITA      */
    {  0,   0,   0,   0},  /* LIN_VACIA     */
};

/* ── Construye cache de texturas del contenido ───────────── */
static void
construir_cache(SDL_Renderer *renderer, TTF_Font *fuente)
{
    SDL_Color fg[] = {
        {200, 200, 215, 255},  /* LIN_NORMAL   */
        {255, 220,  60, 255},  /* LIN_H1       */
        { 80, 200, 240, 255},  /* LIN_H2       */
        {150, 200, 255, 255},  /* LIN_H3       */
        {  0,   0,   0,   0},  /* LIN_SEPARADOR */
        {180, 255, 160, 255},  /* LIN_CODIGO   */
        {160, 240, 180, 255},  /* LIN_CITA     */
        {  0,   0,   0,   0},  /* LIN_VACIA    */
    };

    for (int i = 0; i < n_lineas; i++) {
        cache[i].tex = NULL; cache[i].w = 0;
        cache[i].h = LINE_H; cache[i].extra_x = 0;

        TipoLinea t = lineas[i].tipo;
        if (t == LIN_SEPARADOR || t == LIN_VACIA) continue;

        if (t == LIN_H3) cache[i].extra_x = 16;

        const char *txt = limpiar_prefijo(lineas[i].texto, t);
        if (txt[0] == '\0') continue;

        cache[i].tex = crear_tex(renderer, fuente, txt, fg[t],
                                 WRAP_W - cache[i].extra_x,
                                 &cache[i].w, &cache[i].h,
                                 bg_por_tipo[t]);
    }

    /* ── Texturas fijas del header / footer ── */
    SDL_Color bg_header = { 18, 18, 28, 255 };

    if (tex_titulo) SDL_DestroyTexture(tex_titulo);
    if (tex_hint)   SDL_DestroyTexture(tex_hint);
    if (tex_btn)    SDL_DestroyTexture(tex_btn);
    if (tex_esc)    SDL_DestroyTexture(tex_esc);

    tex_titulo = crear_tex(renderer, fuente, "Tutorial  \xe2\x80\x94  AED",
                           (SDL_Color){ 80, 200, 240, 255}, 0,
                           &tw_titulo, &th_titulo, bg_header);
    tex_hint   = crear_tex(renderer, fuente, "Flechas / Rueda / PgUp-PgDn",
                           (SDL_Color){ 65,  65,  90, 255}, 0,
                           &tw_hint, &th_hint, bg_header);
    tex_btn    = crear_tex(renderer, fuente, "Continuar a Niveles  \xe2\x86\x92",
                           (SDL_Color){230, 245, 255, 255}, 0,
                           &tw_btn, &th_btn, bg_header);
    tex_esc    = crear_tex(renderer, fuente, "ESC / Enter para continuar",
                           (SDL_Color){ 65,  65,  88, 255}, 0,
                           &tw_esc, &th_esc, bg_header);

    cache_ok = 1;
}

static void
blit(SDL_Renderer *r, SDL_Texture *t, int x, int y, int w, int h)
{
    if (!t) return;
    SDL_FRect dst = { x, y, w, h };
    SDL_RenderTexture(r, t, NULL, &dst);
}

/* ── Pantalla principal ──────────────────────────────────── */
int
screenTutorial(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto)
{
    if (n_lineas == 0)
        n_lineas = cargar_md("data/tutorial.md");
    if (!cache_ok)
        construir_cache(renderer, fuente);

    /* altura total del contenido (suma real de cada linea) */
    int contenido_h = HEADER_H;
    for (int i = 0; i < n_lineas; i++)
        contenido_h += (cache[i].h > 0 ? cache[i].h : LINE_H) + 4;
    contenido_h += FOOTER_H + 20;

    int scroll_y = 0, scroll_max = 0;
    SDL_Event evento;

    while (1) {
        Uint32 frame_t0 = SDL_GetTicks();
        int clicked = 0, click_x = 0, click_y = 0;

        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_EVENT_QUIT) return 0;
            if (evento.type == SDL_EVENT_KEY_DOWN) {
                switch (evento.key.key) {
                    case SDLK_ESCAPE:
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        audio_sfx_btn();
                        return 1;
                    case SDLK_PAGEUP:
                        scroll_y -= alto - HEADER_H - FOOTER_H;
                        if (scroll_y < 0) scroll_y = 0; break;
                    case SDLK_PAGEDOWN:
                        scroll_y += alto - HEADER_H - FOOTER_H; break;
                    default: break;
                }
            }
            if (evento.type == SDL_EVENT_MOUSE_WHEEL) {
                scroll_y -= evento.wheel.y * 45;
                if (scroll_y < 0) scroll_y = 0;
            }
            if (evento.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                evento.button.button == SDL_BUTTON_LEFT) {
                clicked = 1; click_x = evento.button.x; click_y = evento.button.y;
            }
        }

        /* scroll fluido con estado del teclado — sin delay del SO */
        const bool *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_DOWN]) scroll_y += 10;
        if (keys[SDL_SCANCODE_UP])   scroll_y -= 10;

        scroll_max = contenido_h - alto;
        if (scroll_max < 0) scroll_max = 0;
        if (scroll_y < 0)          scroll_y = 0;
        if (scroll_y > scroll_max) scroll_y = scroll_max;

        /* ── Render ── */
        SDL_SetRenderDrawColor(renderer, 12, 12, 20, 255);
        SDL_RenderClear(renderer);

        SDL_FRect clip = { 0, HEADER_H, ancho, alto - HEADER_H - FOOTER_H };
        ui_clip(renderer, &clip);

        int cy = HEADER_H - scroll_y;
        for (int i = 0; i < n_lineas; i++) {
            int lh = (cache[i].h > 0 ? cache[i].h : LINE_H);

            /* saltar lineas fuera de pantalla arriba */
            if (cy + lh < HEADER_H) { cy += lh + 4; continue; }
            /* cortar cuando ya pasamos el area visible */
            if (cy > alto - FOOTER_H) break;

            /* fondos */
            if (lineas[i].tipo == LIN_SEPARADOR) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 80, 200);
                SDL_RenderLine(renderer,
                    MARGEN_X, cy + lh / 2, ancho - MARGEN_X, cy + lh / 2);
            } else if (lineas[i].tipo == LIN_CODIGO) {
                SDL_SetRenderDrawColor(renderer, 22, 22, 32, 255);
                SDL_FRect bg = { MARGEN_X - 8, cy, ancho - MARGEN_X * 2 + 16, lh };
                SDL_RenderFillRect(renderer, &bg);
            } else if (lineas[i].tipo == LIN_CITA) {
                SDL_SetRenderDrawColor(renderer, 40, 80, 60, 120);
                SDL_FRect bg = { MARGEN_X - 8, cy, ancho - MARGEN_X * 2 + 16, lh };
                SDL_RenderFillRect(renderer, &bg);
                SDL_SetRenderDrawColor(renderer, 60, 200, 100, 255);
                SDL_FRect barra = { MARGEN_X - 8, cy, 3, lh };
                SDL_RenderFillRect(renderer, &barra);
            }

            blit(renderer, cache[i].tex,
                 MARGEN_X + cache[i].extra_x, cy,
                 cache[i].w, cache[i].h);

            cy += lh + 4;
        }

        /* barra de scroll */
        if (scroll_max > 0) {
            int area_h = alto - HEADER_H - FOOTER_H;
            int bar_h  = area_h * area_h / contenido_h;
            if (bar_h < 20) bar_h = 20;
            int bar_y = HEADER_H + (int)((long long)scroll_y *
                        (area_h - bar_h) / scroll_max);
            SDL_SetRenderDrawColor(renderer, 50, 50, 70, 200);
            SDL_FRect bg_bar = { ancho - 6, HEADER_H, 5, area_h };
            SDL_RenderFillRect(renderer, &bg_bar);
            SDL_SetRenderDrawColor(renderer, 100, 100, 160, 255);
            SDL_FRect bar = { ancho - 6, bar_y, 5, bar_h };
            SDL_RenderFillRect(renderer, &bar);
        }

        ui_clip(renderer, NULL);

        /* ── Header ── */
        SDL_SetRenderDrawColor(renderer, 18, 18, 28, 255);
        SDL_FRect header = { 0, 0, ancho, HEADER_H };
        SDL_RenderFillRect(renderer, &header);
        SDL_SetRenderDrawColor(renderer, 80, 200, 240, 180);
        SDL_RenderLine(renderer, 0, HEADER_H - 1, ancho, HEADER_H - 1);
        blit(renderer, tex_titulo, MARGEN_X, 8,  tw_titulo, th_titulo);
        blit(renderer, tex_hint,   MARGEN_X, 30, tw_hint,   th_hint);

        /* ── Footer ── */
        int btn_w = 220, btn_h = 34;
        int btn_x = ancho - btn_w - MARGEN_X;
        int btn_y = alto - FOOTER_H + (FOOTER_H - btn_h) / 2;
        int mx, my;
        ui_mouse(&mx, &my);
        int hover = mx >= btn_x && mx <= btn_x + btn_w &&
                    my >= btn_y && my <= btn_y + btn_h;

        SDL_SetRenderDrawColor(renderer, 18, 18, 28, 255);
        SDL_FRect footer = { 0, alto - FOOTER_H, ancho, FOOTER_H };
        SDL_RenderFillRect(renderer, &footer);
        SDL_SetRenderDrawColor(renderer, 80, 200, 240, 180);
        SDL_RenderLine(renderer, 0, alto - FOOTER_H, ancho, alto - FOOTER_H);

        SDL_SetRenderDrawColor(renderer,
            hover ? 40 : 20, hover ? 160 : 110, hover ? 200 : 160, 255);
        SDL_FRect btn = { btn_x, btn_y, btn_w, btn_h };
        SDL_RenderFillRect(renderer, &btn);
        blit(renderer, tex_btn, btn_x + 10, btn_y + (btn_h - th_btn) / 2,
             tw_btn, th_btn);
        blit(renderer, tex_esc, MARGEN_X,   btn_y + (btn_h - th_esc) / 2,
             tw_esc, th_esc);

        if (clicked &&
            click_x >= btn_x && click_x <= btn_x + btn_w &&
            click_y >= btn_y && click_y <= btn_y + btn_h) {
            audio_sfx_btn();
            return 1;
        }

        presente(renderer);
        /* Si vsync esta activo, RenderPresent ya esperó ~16ms.
           Solo dormimos lo que falta para no doblar la espera. */
        Uint32 frame_ms = SDL_GetTicks() - frame_t0;
        if (frame_ms < 16) SDL_Delay(16 - frame_ms);
    }
    return 1;
}
