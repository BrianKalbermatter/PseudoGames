#include "shell.h"
#include "ui.h"
#include "audio.h"
#include "config.h"
#include "editorBim.h"
#include "screenCEditor.h"
#include "screenPJ.h"
#include "pomodoro_bg.h"
#include <SDL3_image/SDL_image.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  HELPERS DE DIBUJO
 *  Funciones internas del shell. Dibujan en coordenadas exactas (sin offsets).
 * ════════════════════════════════════════════════════════════════════════════ */


static void
sh_text(SDL_Renderer *r, TTF_Font *f, const char *txt, int x, int y, SDL_Color c)
{
    SDL_Surface *s = TTF_RenderText_Blended(f, txt, 0, c);
    if (!s) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_FRect dst   = {x, y, s->w, s->h};
    SDL_RenderTexture(r, t, NULL, &dst);
    SDL_DestroySurface(s);
    SDL_DestroyTexture(t);
}

static void
sh_hline(SDL_Renderer *r, int x, int y, int w, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderLine(r, x, y, x + w - 1, y);
}

static void
sh_vline(SDL_Renderer *r, int x, int y, int h, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderLine(r, x, y, x, y + h - 1);
}

static void
sh_fill(SDL_Renderer *r, SDL_FRect rect, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  NOMBRES DE LOS PANELES EN EL SIDEBAR
 *  Deben coincidir con el orden del enum PanelID en shell.h
 * ════════════════════════════════════════════════════════════════════════════ */
static const char *SIDEBAR_LABELS[PANEL_COUNT] = {
    "DOC",
    "Pomodoro",
    "Editor Libre",
    "Niveles",
    "Soluciones",
    "Config",
    "Mazmorra",
    "Editor C",
};

/* Whitelist: 1 = panel habilitado, 0 = WIP (gris, no clickeable).
 * Indices coinciden con el enum PanelID. Para habilitar un panel,
 * cambiar su 0 por 1 — la UI y el hit-test se actualizan solos. */
static const int PANEL_HABILITADO[PANEL_COUNT] = {
    1,  /* DOC          */
    0,  /* Pomodoro     */
    0,  /* Editor Libre */
    0,  /* Niveles      */
    0,  /* Soluciones   */
    1,  /* Config       */
    0,  /* Mazmorra     */
    0,  /* Editor C     */
};

/* ════════════════════════════════════════════════════════════════════════════
 *  SIDEBAR
 * ════════════════════════════════════════════════════════════════════════════ */

static void
draw_sidebar(ShellCtx *ctx)
{
    SDL_Renderer *r  = ctx->renderer;
    TTF_Font     *f  = ctx->fuente;
    int           sw = ctx->sidebar_w;
    int           h  = ctx->alto;

    int mx, my;
    ui_mouse(&mx, &my);

    /* Fondo del sidebar */
    sh_fill(r, (SDL_FRect){0, 0, sw, h}, COL_SIDEBAR);

    /* Borde derecho */
    sh_vline(r, sw - 1, 0, h, COL_SEP);

    /* ── Sidebar colapsado: solo la flecha > ── */
    if (!ctx->sidebar_abierto) {
        int tw, th;
        TTF_GetStringSize(f, ">", 0, &tw, &th);
        int ax = (sw - tw) / 2;
        int ay = h / 2 - th / 2;
        int hover = (mx >= 0 && mx < sw && my >= 0 && my < h);
        sh_text(r, f, ">", ax, ay, hover ? COL_ACTIVE : COL_TEXT);
        return;
    }

    /* ── Sidebar expandido ── */

    /* Logo en la parte superior — icono PNG (16x16) + texto */
    int logo_y  = 10;
    int icon_sz = 16;
    if (ctx->logo) {
        SDL_FRect dst = {14, logo_y, icon_sz, icon_sz};
        SDL_RenderTexture(r, ctx->logo, NULL, &dst);
    }
    sh_text(r, f, "PSEUDOGAMES", 14 + icon_sz + 4, logo_y, COL_ACTIVE);

    /* Flecha < para colapsar (esquina superior derecha) */
    {
        int tw, th;
        TTF_GetStringSize(f, "<", 0, &tw, &th);
        int ax = sw - tw - 10;
        int ay = logo_y;
        int hover = (mx >= ax - 4 && mx <= ax + tw + 4 &&
                     my >= ay - 4 && my <= ay + th + 4);
        sh_text(r, f, "<", ax, ay, hover ? COL_ACTIVE : COL_TEXT);
    }

    /* Separador bajo el logo */
    int sep_y = logo_y + 22;
    sh_hline(r, 0, sep_y, sw, COL_SEP);

    /* Items de navegacion */
    int item_h = 30;
    int items_y = sep_y + 12;

    for (int i = 0; i < PANEL_COUNT; i++) {
        int iy = items_y + i * item_h;

        /* Panel deshabilitado (WIP): gris dim + sufijo [WIP], sin hover */
        if (!PANEL_HABILITADO[i]) {
            char label[64];
            snprintf(label, sizeof(label), "  %s [WIP]", SIDEBAR_LABELS[i]);
            SDL_Color tc_wip = {70, 70, 70, 255};
            sh_text(r, f, label, 12, iy + (item_h - 16) / 2, tc_wip);
            continue;
        }

        /* Detectar si este panel tiene un tab abierto */
        int is_open   = 0;
        int is_active = 0;
        for (int t = 0; t < ctx->n_tabs; t++) {
            if (ctx->tabs[t].id == (PanelID)i) {
                is_open = 1;
                if (t == ctx->tab_activo) is_active = 1;
            }
        }

        int hover = (mx >= 0 && mx < sw - 1 && my >= iy && my < iy + item_h);

        /* Barra izquierda neon verde si el tab esta activo */
        if (is_active) {
            sh_fill(r, (SDL_FRect){0, iy, 2, item_h}, COL_ACCENT);
        }

        /* Fondo hover sutil */
        if (hover && !is_active) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 10);
            SDL_FRect hbg = {1, iy, sw - 2, item_h};
            SDL_RenderFillRect(r, &hbg);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }

        /* Prefijo: > si activo, · si abierto pero no activo, espacio si cerrado */
        const char *prefix = is_active ? "> " : (is_open ? ". " : "  ");
        char label[48];
        snprintf(label, sizeof(label), "%s%s", prefix, SIDEBAR_LABELS[i]);

        SDL_Color tc = (is_active || hover) ? COL_ACTIVE : COL_TEXT;
        sh_text(r, f, label, 12, iy + (item_h - 16) / 2, tc);
    }

    /* Separador antes de Salir */
    int salir_y = h - 36;
    sh_hline(r, 0, salir_y - 6, sw, COL_SEP);

    /* Salir */
    int hover_salir = (mx >= 0 && mx < sw && my >= salir_y && my < salir_y + 28);
    sh_text(r, f, "  Salir", 12, salir_y + 6, hover_salir ? COL_ACTIVE : COL_TEXT);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  TAB BAR
 * ════════════════════════════════════════════════════════════════════════════ */

static void
draw_tab_bar(ShellCtx *ctx, SDL_FRect bar)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    int mx, my;
    ui_mouse(&mx, &my);

    /* Fondo de la barra */
    sh_fill(r, bar, (SDL_Color){10, 10, 10, 255});

    /* Linea inferior */
    sh_hline(r, bar.x, bar.y + bar.h - 1, bar.w, COL_SEP);

    if (ctx->n_tabs == 0) return;

    int tx      = bar.x + 2;
    int pad_x   = 10;
    int tab_h   = bar.h - 1;

    for (int i = 0; i < ctx->n_tabs; i++) {
        int tw, th;
        TTF_GetStringSize(f, ctx->tabs[i].nombre, 0, &tw, &th);

        int close_w = 14;
        int tab_w   = pad_x + tw + 6 + close_w + pad_x;

        int is_active  = (i == ctx->tab_activo);
        int hover_tab  = (mx >= tx && mx < tx + tab_w &&
                          my >= bar.y && my < bar.y + tab_h);

        /* Fondo del tab activo */
        if (is_active) {
            sh_fill(r, (SDL_FRect){tx, bar.y, tab_w, tab_h}, COL_BG);
        }

        /* Linea inferior: verde neon si activo, separador si no */
        if (is_active) {
            SDL_SetRenderDrawColor(r, 0, 220, 70, 255);
        } else {
            SDL_SetRenderDrawColor(r, COL_SEP.r, COL_SEP.g, COL_SEP.b, 255);
        }
        SDL_RenderLine(r, tx, bar.y + tab_h - 1, tx + tab_w - 2, bar.y + tab_h - 1);

        /* Separador derecho del tab */
        sh_vline(r, tx + tab_w - 1, bar.y + 4, tab_h - 8, COL_SEP);

        /* Texto del tab */
        SDL_Color tc = (is_active || hover_tab) ? COL_ACTIVE : COL_TEXT;
        int text_y   = bar.y + (tab_h - th) / 2;
        sh_text(r, f, ctx->tabs[i].nombre, tx + pad_x, text_y, tc);

        /* Boton × (solo visible en hover o si es el tab activo) */
        int cx       = tx + pad_x + tw + 6;
        int cy       = text_y;
        int hover_x  = (mx >= cx && mx < cx + 12 &&
                        my >= bar.y && my < bar.y + tab_h);

        if (hover_x) {
            sh_text(r, f, "x", cx, cy, COL_CLOSE);
        } else if (hover_tab || is_active) {
            sh_text(r, f, "x", cx, cy, COL_TEXT);
        }

        tx += tab_w;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  GESTION DE TABS
 * ════════════════════════════════════════════════════════════════════════════ */

static void
abrir_o_activar_tab(ShellCtx *ctx, PanelID id)
{
    /* Si ya existe un tab con este panel, solo activarlo */
    for (int i = 0; i < ctx->n_tabs; i++) {
        if (ctx->tabs[i].id == id) {
            ctx->tab_activo = i;
            return;
        }
    }

    /* Crear un nuevo tab */
    if (ctx->n_tabs >= MAX_TABS) return;

    Tab *t   = &ctx->tabs[ctx->n_tabs];
    t->id    = id;
    t->state = NULL;
    strncpy(t->nombre, SIDEBAR_LABELS[id], 31);
    t->nombre[31] = '\0';

    /* Llamar al init del panel (carga recursos, aloca state) */
    ctx->defs[id].init(ctx, t);

    ctx->tab_activo = ctx->n_tabs;
    ctx->n_tabs++;
}

static void
cerrar_tab(ShellCtx *ctx, int idx)
{
    if (idx < 0 || idx >= ctx->n_tabs) return;

    /* Limpiar recursos del panel */
    ctx->defs[ctx->tabs[idx].id].cleanup(ctx, &ctx->tabs[idx]);

    /* Correr los tabs siguientes una posicion hacia la izquierda */
    for (int i = idx; i < ctx->n_tabs - 1; i++)
        ctx->tabs[i] = ctx->tabs[i + 1];
    ctx->n_tabs--;

    /* Ajustar tab_activo */
    if (ctx->tab_activo >= ctx->n_tabs)
        ctx->tab_activo = ctx->n_tabs - 1;
    if (ctx->tab_activo < 0)
        ctx->tab_activo = 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  HIT TESTING (detectar donde hizo click el mouse)
 * ════════════════════════════════════════════════════════════════════════════ */

/* Retorna:
 *   >= 0          → indice de PanelID (click en item del sidebar)
 *   SHELL_TOGGLE  → click en la flecha de colapsar/expandir
 *   SHELL_SALIR   → click en "Salir"
 *   -1            → click en ninguna zona especial del sidebar
 */
#define SHELL_TOGGLE -2
#define SHELL_SALIR  -3

static int
sidebar_hit(ShellCtx *ctx, int cx, int cy)
{
    int sw = ctx->sidebar_w;
    if (cx < 0 || cx >= sw) return -1;

    if (!ctx->sidebar_abierto) {
        /* Colapsado: cualquier click expande */
        return SHELL_TOGGLE;
    }

    /* Flecha < (zona aproximada: esquina superior derecha del sidebar) */
    if (cy >= 10 && cy <= 40 && cx >= sw - 34)
        return SHELL_TOGGLE;

    /* Items de navegacion */
    int item_h  = 30;
    int items_y = 14 + 22 + 12;   /* logo_y + sep_offset + gap */

    for (int i = 0; i < PANEL_COUNT; i++) {
        int iy = items_y + i * item_h;
        if (cy >= iy && cy < iy + item_h) {
            /* Panel deshabilitado (WIP): el click muere aca */
            if (!PANEL_HABILITADO[i]) return -1;
            return i;
        }
    }

    /* Salir */
    int salir_y = ctx->alto - 36;
    if (cy >= salir_y - 6 && cy < ctx->alto) return SHELL_SALIR;

    return -1;
}

/* Retorna el indice del tab clickeado, -1 si nada.
 * *es_close = 1 si se hizo click en el boton × */
static int
tabbar_hit(ShellCtx *ctx, int px, int py, SDL_FRect bar, int *es_close)
{
    *es_close = 0;
    if (py < bar.y || py >= bar.y + bar.h) return -1;
    if (px < bar.x || px >= bar.x + bar.w) return -1;

    int tx    = bar.x + 2;
    int pad_x = 10;

    for (int i = 0; i < ctx->n_tabs; i++) {
        int tw;
        TTF_GetStringSize(ctx->fuente, ctx->tabs[i].nombre, 0, &tw, NULL);
        int close_w = 14;
        int tab_w   = pad_x + tw + 6 + close_w + pad_x;

        if (px >= tx && px < tx + tab_w) {
            int cx = tx + pad_x + tw + 6;
            if (px >= cx && px < cx + 12) *es_close = 1;
            return i;
        }
        tx += tab_w;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANTALLA DE BIENVENIDA (cuando no hay ningun tab abierto)
 * ════════════════════════════════════════════════════════════════════════════ */

static void
draw_welcome(ShellCtx *ctx, SDL_FRect area)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    const char *titulo = "PseudoGames";
    const char *sub    = "Selecciona una opcion del sidebar para comenzar.";

    int tw, th;

    TTF_GetStringSize(f, titulo, 0, &tw, &th);
    sh_text(r, f, titulo,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 - th - 10,
        COL_ACCENT);

    TTF_GetStringSize(f, sub, 0, &tw, &th);
    sh_text(r, f, sub,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 + 10,
        COL_TEXT);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANELES — IMPLEMENTACIONES
 *
 *  Por ahora todos son "placeholders": muestran el nombre del panel y
 *  el cartel "En construccion". Se iran portando uno por uno.
 *
 *  El patron que se repite en cada panel real:
 *    init    → malloc del estado, cargar recursos
 *    event   → procesar teclado/mouse dentro del area
 *    draw    → SDL_SetRenderClipRect + dibujar + ui_clip(NULL)
 *    cleanup → liberar texturas, free del estado
 * ════════════════════════════════════════════════════════════════════════════ */

static void panel_noop_init   (ShellCtx *ctx, Tab *tab)              { (void)ctx; (void)tab; }
static void panel_noop_event  (ShellCtx *ctx, Tab *tab, SDL_Event *e){ (void)ctx; (void)tab; (void)e; }
static void panel_noop_cleanup(ShellCtx *ctx, Tab *tab)              { (void)ctx; (void)tab; }

/* Puente temporal al editorBim.
   Ese archivo lo mantiene Brian y todavia dibuja con SDL_Rect (enteros).
   El resto del shell ya pasó a SDL_FRect, que es lo que pide SDL3 para
   dibujar. Cuando editorBim.c migre, se borra este wrapper y se vuelve a
   asignar editor_bim_draw directo en la tabla de paneles. */
static void
editor_bim_draw_panel(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    SDL_Rect ir = { (int)area.x, (int)area.y, (int)area.w, (int)area.h };
    editor_bim_draw(ctx, tab, ir);
}

static void
panel_placeholder_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    ui_clip(r, &area);

    int tw, th;

    /* Nombre del panel centrado */
    TTF_GetStringSize(f, tab->nombre, 0, &tw, &th);
    sh_text(r, f, tab->nombre,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 - th - 8,
        COL_ACCENT);

    /* Subtitulo */
    const char *sub = "En construccion...";
    TTF_GetStringSize(f, sub, 0, &tw, &th);
    sh_text(r, f, sub,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 + 8,
        COL_TEXT);

    ui_clip(r, NULL);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  HELPER: LAUNCHER GENERICO
 *
 *  Dibuja un panel con titulo, descripcion y boton de accion.
 *  Usado por Pomodoro, DOC y Niveles que todavia no estan portados.
 * ════════════════════════════════════════════════════════════════════════════ */

static void
draw_launcher(ShellCtx *ctx, SDL_FRect area,
              const char *titulo, const char *desc, const char *btn_lbl,
              int *btn_clicked)
{
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;
    int mx, my;
    ui_mouse(&mx, &my);

    ui_clip(r, &area);

    /* Titulo */
    int tw, th;
    TTF_GetStringSize(f, titulo, 0, &tw, &th);
    sh_text(r, f, titulo,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 - 60,
        COL_ACCENT);

    /* Descripcion */
    TTF_GetStringSize(f, desc, 0, &tw, &th);
    sh_text(r, f, desc,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 - 30,
        COL_TEXT);

    /* Boton */
    int bw = (int)(strlen(btn_lbl) * 9 + 40);
    int bh = 30;
    int bx = area.x + (area.w - bw) / 2;
    int by = area.y + area.h / 2 + 10;
    int hover = (mx >= bx && mx < bx + bw && my >= by && my < by + bh);

    SDL_SetRenderDrawColor(r,
        hover ? COL_ACCENT.r : 20,
        hover ? COL_ACCENT.g : 20,
        hover ? COL_ACCENT.b : 20, 255);
    SDL_FRect btn = {bx, by, bw, bh};
    SDL_RenderFillRect(r, &btn);
    SDL_SetRenderDrawColor(r, COL_SEP.r, COL_SEP.g, COL_SEP.b, 255);
    SDL_RenderRect(r, &btn);

    TTF_GetStringSize(f, btn_lbl, 0, &tw, NULL);
    SDL_Color tc = hover ? COL_BG : COL_TEXT;
    sh_text(r, f, btn_lbl, bx + (bw - tw) / 2, by + 7, tc);

    *btn_clicked = hover && (ui_mouse(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT));

    ui_clip(r, NULL);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: POMODORO
 * ════════════════════════════════════════════════════════════════════════════ */

static void pomodoro_init   (ShellCtx *ctx, Tab *tab) { (void)ctx; (void)tab; }
static void pomodoro_cleanup(ShellCtx *ctx, Tab *tab) { (void)ctx; (void)tab; }

static void
pomodoro_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    (void)tab;
    if (e->type == SDL_EVENT_KEY_DOWN && e->key.key == SDLK_RETURN) {
        SDL_GetWindowSize(ctx->ventana, &ctx->ancho, &ctx->alto);
        screenPomodoro(ctx->renderer, ctx->fuente, ctx->ancho, ctx->alto);
    }
}

static void
pomodoro_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    (void)tab;
    int clicked = 0;
    draw_launcher(ctx, area,
        "Pomodoro",
        "Timer de estudio con sesiones y descansos.",
        "Abrir Pomodoro  [Enter]",
        &clicked);
    if (clicked) {
        SDL_GetWindowSize(ctx->ventana, &ctx->ancho, &ctx->alto);
        screenPomodoro(ctx->renderer, ctx->fuente, ctx->ancho, ctx->alto);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: DOC
 * ════════════════════════════════════════════════════════════════════════════ */

static void
doc_init(ShellCtx *ctx, Tab *tab)
{
    tab->state = doc_crear(ctx->fuente);
}

static void
doc_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    doc_destruir((DocState *)tab->state);
    tab->state = NULL;
}

static void
doc_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    if (!tab->state) return;
    SDL_FRect area = {
        ctx->sidebar_w, TAB_BAR_H,
        ctx->ancho - ctx->sidebar_w, ctx->alto - TAB_BAR_H
    };
    doc_evento((DocState *)tab->state, ctx->fuente, e, area);
}

static void
doc_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    if (!tab->state) return;
    doc_dibujar((DocState *)tab->state, ctx->renderer, ctx->fuente, area);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: NIVELES
 * ════════════════════════════════════════════════════════════════════════════ */

static void
niveles_init(ShellCtx *ctx, Tab *tab)
{
    tab->state = niveles_crear(ctx->fuente);
}

static void
niveles_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    niveles_destruir((NivelesState *)tab->state);
    tab->state = NULL;
}

static void
niveles_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    if (!tab->state) return;
    SDL_FRect area = { ctx->sidebar_w, TAB_BAR_H,
                      ctx->ancho - ctx->sidebar_w, ctx->alto - TAB_BAR_H };
    niveles_evento((NivelesState *)tab->state, e, area);
}

static void
niveles_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    if (!tab->state) return;
    niveles_dibujar((NivelesState *)tab->state, ctx->renderer, ctx->fuente, area);

    /* si el usuario selecciono un nivel, abrirlo */
    int nivel = niveles_consumir_nivel((NivelesState *)tab->state);
    if (nivel > 0) {
        SDL_GetWindowSize(ctx->ventana, &ctx->ancho, &ctx->alto);
        screenLvLEditor(ctx->renderer, ctx->fuente, ctx->ancho, ctx->alto, nivel);
        /* refrescar total y nivel actual despues de completar */
        niveles_refrescar((NivelesState *)tab->state);
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: CONFIG
 *
 *  Port de screenConfig.c al patron Panel.
 *  La logica es la misma; lo que cambio:
 *    - Sin while() propio: el shell llama handle_event una vez por evento
 *    - Sin SDL_RenderClear/Present: el shell los hace
 *    - Coordenadas relativas a `area` en vez de ancho/alto absolutos
 *    - Estado en ConfigState (heap) en vez de variables locales de funcion
 * ════════════════════════════════════════════════════════════════════════════ */

/* Opcion con nombre, valores posibles e indice actual */
typedef struct {
    const char *nombre;
    const char *valores[4];
    int         n_valores;
    int         seleccion;
} OpcionCfg;

/* Estado privado del panel Config */
typedef struct {
    OpcionCfg opciones[3];
    int       n_opciones;
    int       opcion_sel;   /* fila seleccionada por teclado */
    int       tab_actual;   /* 0=General  1=Atajos           */
    int       vol;          /* volumen 0-100                 */
    int       drag_vol;     /* 1 = arrastrando el slider     */
} ConfigState;

/* Calcular el rect del panel central dentro del area de contenido */
static SDL_FRect
cfg_panel_rect(ShellCtx *ctx)
{
    int pw = 520, ph = 440;
    int cx = ctx->sidebar_w;
    int cy = TAB_BAR_H;
    int cw = ctx->ancho - ctx->sidebar_w;
    int ch = ctx->alto  - TAB_BAR_H;
    return (SDL_FRect){
        cx + (cw - pw) / 2,
        cy + (ch - ph) / 2,
        pw, ph
    };
}

/* ── config_init ── */
static void
config_init(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    config_cargar();

    ConfigState *s = malloc(sizeof(ConfigState));
    memset(s, 0, sizeof(ConfigState));

    s->opciones[0] = (OpcionCfg){ "Velocidad lluvia",  {"Lenta","Normal","Rapida"}, 3, config_get_lluvia()  };
    s->opciones[1] = (OpcionCfg){ "Brillo",            {"Bajo","Normal","Alto"},  3, config_get_brillo()    };
    s->n_opciones  = 2;
    s->opcion_sel  = 0;
    s->tab_actual  = 0;
    s->vol         = config_get_volumen();
    s->drag_vol    = 0;

    tab->state = s;
}

/* ── config_handle_event ── */
static void
config_handle_event(ShellCtx *ctx, Tab *tab, SDL_Event *e)
{
    ConfigState *s = (ConfigState *)tab->state;
    SDL_FRect p     = cfg_panel_rect(ctx);

    int track_x  = p.x + 140;
    int track_w  = p.w - 160;
    int handle_w = 14;
    int handle_h = 22;

    if (e->type == SDL_EVENT_KEY_DOWN && s->tab_actual == 0) {
        switch (e->key.key) {
            case SDLK_UP:
                s->opcion_sel = (s->opcion_sel - 1 + s->n_opciones) % s->n_opciones;
                break;
            case SDLK_DOWN:
                s->opcion_sel = (s->opcion_sel + 1) % s->n_opciones;
                break;
            case SDLK_LEFT:
                s->opciones[s->opcion_sel].seleccion--;
                if (s->opciones[s->opcion_sel].seleccion < 0)
                    s->opciones[s->opcion_sel].seleccion = s->opciones[s->opcion_sel].n_valores - 1;
                break;
            case SDLK_RIGHT: case SDLK_RETURN:
                s->opciones[s->opcion_sel].seleccion++;
                if (s->opciones[s->opcion_sel].seleccion >= s->opciones[s->opcion_sel].n_valores)
                    s->opciones[s->opcion_sel].seleccion = 0;
                break;
            default: break;
        }
    }

    if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == SDL_BUTTON_LEFT) {
        int cx = e->button.x;
        int cy = e->button.y;

        /* Click en tabs General/Atajos */
        int tab_w = 80, tab_h = 22;
        int tab0_x = p.x + p.w - tab_w * 2 - 16;
        int tab_y  = p.y + 10;
        for (int t = 0; t < 2; t++) {
            int tx = tab0_x + t * (tab_w + 4);
            if (cx >= tx && cx <= tx + tab_w && cy >= tab_y && cy <= tab_y + tab_h)
                s->tab_actual = t;
        }

        if (s->tab_actual == 0) {
            /* Click en < > de las opciones */
            int item_y = p.y + 65;
            int item_h = 80;
            for (int i = 0; i < s->n_opciones; i++) {
                int oy  = item_y + i * item_h;
                int vx  = p.x + p.w - 200;
                /* boton < */
                if (cx >= vx && cx <= vx + 20 && cy >= oy && cy <= oy + 24) {
                    s->opciones[i].seleccion = (s->opciones[i].seleccion - 1 + s->opciones[i].n_valores) % s->opciones[i].n_valores;
                }
                /* boton > */
                if (cx >= vx + 150 && cx <= vx + 180 && cy >= oy && cy <= oy + 24) {
                    s->opciones[i].seleccion = (s->opciones[i].seleccion + 1) % s->opciones[i].n_valores;
                }
            }

            /* Click en el track del slider de volumen */
            int sl_y = p.y + 340;
            if (cx >= track_x && cx <= track_x + track_w &&
                cy >= sl_y    && cy <= sl_y + handle_h) {
                s->drag_vol = 1;
                s->vol = (cx - track_x) * 100 / (track_w - handle_w);
                if (s->vol < 0)   s->vol = 0;
                if (s->vol > 100) s->vol = 100;
                audio_set_volumen(s->vol);
            }
        }
    }

    if (e->type == SDL_EVENT_MOUSE_BUTTON_UP && e->button.button == SDL_BUTTON_LEFT) {
        if (s->drag_vol) {
            config_set_volumen(s->vol);
            config_guardar();
        }
        s->drag_vol = 0;
    }

    if (e->type == SDL_EVENT_MOUSE_MOTION && s->drag_vol) {
        s->vol = (e->motion.x - track_x) * 100 / (track_w - handle_w);
        if (s->vol < 0)   s->vol = 0;
        if (s->vol > 100) s->vol = 100;
        audio_set_volumen(s->vol);
    }
}

/* ── config_draw ── */
static void
config_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    ConfigState  *s = (ConfigState *)tab->state;
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;
    SDL_FRect      p = cfg_panel_rect(ctx);

    ui_clip(r, &area);

    /* Fondo del panel */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 14, 14, 14, 255);
    SDL_RenderFillRect(r, &p);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    /* Borde del panel */
    SDL_SetRenderDrawColor(r, COL_SEP.r, COL_SEP.g, COL_SEP.b, 255);
    SDL_RenderRect(r, &p);

    /* Titulo */
    sh_text(r, f, "Config", p.x + 16, p.y + 14, COL_ACTIVE);

    /* Tabs General / Atajos */
    const char *tab_lbl[] = { "General", "Atajos" };
    int tab_w = 80, tab_h = 22;
    int tab0_x = p.x + p.w - tab_w * 2 - 16;
    int tab_y  = p.y + 10;
    int mx, my;
    ui_mouse(&mx, &my);

    for (int t = 0; t < 2; t++) {
        int tx      = tab0_x + t * (tab_w + 4);
        int is_act  = (t == s->tab_actual);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, is_act ? 30 : 14, is_act ? 30 : 14, is_act ? 30 : 14, 255);
        SDL_FRect tbr = {tx, tab_y, tab_w, tab_h};
        SDL_RenderFillRect(r, &tbr);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

        SDL_SetRenderDrawColor(r, is_act ? COL_ACCENT.r : COL_SEP.r,
                                  is_act ? COL_ACCENT.g : COL_SEP.g,
                                  is_act ? COL_ACCENT.b : COL_SEP.b, 255);
        SDL_RenderRect(r, &tbr);

        int tlw; TTF_GetStringSize(f, tab_lbl[t], 0, &tlw, NULL);
        SDL_Color tc = is_act ? COL_ACTIVE : COL_TEXT;
        sh_text(r, f, tab_lbl[t], tx + (tab_w - tlw) / 2, tab_y + 4, tc);
    }

    /* Separador bajo el titulo */
    sh_hline(r, p.x + 1, p.y + 38, p.w - 2, COL_SEP);

    /* ── Tab General ── */
    if (s->tab_actual == 0) {
        int item_y = p.y + 65;
        int item_h = 80;

        for (int i = 0; i < s->n_opciones; i++) {
            int oy = item_y + i * item_h;
            int hover_row = (mx >= p.x + 12 && mx <= p.x + p.w - 12 &&
                             my >= oy - 8    && my <= oy + 52);

            /* Highlight de fila */
            if (i == s->opcion_sel || hover_row) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 8);
                SDL_FRect hbg = {p.x + 8, oy - 8, p.w - 16, 52};
                SDL_RenderFillRect(r, &hbg);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                if (hover_row) s->opcion_sel = i;
            }

            /* Nombre de la opcion */
            SDL_Color cn = (i == s->opcion_sel) ? COL_ACTIVE : COL_TEXT;
            sh_text(r, f, s->opciones[i].nombre, p.x + 20, oy, cn);

            /* Botones < valor > */
            int vx = p.x + p.w - 200;
            sh_text(r, f, "<", vx, oy, (SDL_Color){80, 120, 255, 255});

            const char *val = s->opciones[i].valores[s->opciones[i].seleccion];
            int vw; TTF_GetStringSize(f, val, 0, &vw, NULL);
            sh_text(r, f, val, vx + 30 + (120 - vw) / 2, oy, COL_ACTIVE);

            sh_text(r, f, ">", vx + 160, oy, (SDL_Color){80, 120, 255, 255});
        }

        /* Slider de volumen */
        int sl_y     = p.y + 340;
        int track_x  = p.x + 140;
        int track_w  = p.w - 160;
        int handle_w = 14;
        int handle_h = 22;
        int handle_x = track_x + s->vol * (track_w - handle_w) / 100;
        int track_cy = sl_y + handle_h / 2;

        sh_text(r, f, "Volumen:", p.x + 20, sl_y + 3, COL_TEXT);

        /* Track fondo */
        SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
        SDL_FRect tr_bg = {track_x, track_cy - 3, track_w, 6};
        SDL_RenderFillRect(r, &tr_bg);

        /* Fill hasta el handle */
        SDL_SetRenderDrawColor(r, COL_ACCENT.r, COL_ACCENT.g, COL_ACCENT.b, 255);
        SDL_FRect tr_fill = {track_x, track_cy - 3, handle_x - track_x + handle_w / 2, 6};
        SDL_RenderFillRect(r, &tr_fill);

        /* Handle */
        int hov_h = (mx >= handle_x && mx <= handle_x + handle_w &&
                     my >= sl_y     && my <= sl_y + handle_h);
        SDL_SetRenderDrawColor(r,
            (s->drag_vol || hov_h) ? 200 : 255,
            (s->drag_vol || hov_h) ? 220 : 255,
            (s->drag_vol || hov_h) ? 200 : 255, 255);
        SDL_FRect handle_rect = {handle_x, sl_y, handle_w, handle_h};
        SDL_RenderFillRect(r, &handle_rect);

        /* Porcentaje */
        char vol_str[8];
        snprintf(vol_str, sizeof(vol_str), "%d%%", s->vol);
        sh_text(r, f, vol_str, track_x + track_w + 8, sl_y + 3, COL_ACTIVE);

        /* Ayuda abajo */
        sh_text(r, f, "[flechas] navegar   [</> o click] cambiar   [drag] volumen",
            p.x + 16, p.y + p.h - 24, COL_TEXT);
    }

    /* ── Tab Atajos ── */
    if (s->tab_actual == 1) {
        typedef struct { const char *key; const char *desc; } Atajo;
        static const Atajo atajos[] = {
            { "Ctrl + T",   "Nueva tab"             },
            { "Ctrl + W",   "Cerrar tab"            },
            { "F9",         "Guardar archivo"       },
            { "F5",         "Ejecutar codigo"       },
            { "F8",         "Abrir Wiki"            },
            { "Tab",        "Indentar (4 espacios)" },
            { "Ctrl + C",   "Copiar linea"          },
            { "Ctrl + X",   "Cortar linea"          },
            { "Ctrl + V",   "Pegar"                 },
        };
        int n   = (int)(sizeof(atajos) / sizeof(atajos[0]));
        int ay  = p.y + 50;
        int rh  = 26;

        for (int i = 0; i < n; i++) {
            int ry = ay + i * rh;
            if (i % 2 == 0) {
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
                SDL_FRect stripe = {p.x + 8, ry - 2, p.w - 16, rh - 2};
                SDL_RenderFillRect(r, &stripe);
                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
            }
            sh_text(r, f, atajos[i].key,  p.x + 20,  ry, (SDL_Color){80, 160, 255, 255});
            sh_text(r, f, "->",            p.x + 160, ry, COL_SEP);
            sh_text(r, f, atajos[i].desc,  p.x + 200, ry, COL_TEXT);
        }
    }

    ui_clip(r, NULL);
}

/* ── config_cleanup ── */
static void
config_cleanup(ShellCtx *ctx, Tab *tab)
{
    (void)ctx;
    ConfigState *s = (ConfigState *)tab->state;
    /* Guardar configuracion al cerrar el tab */
    config_set_lluvia(s->opciones[0].seleccion);
    config_set_brillo(s->opciones[1].seleccion);
    config_set_volumen(s->vol);
    config_guardar();
    free(s);
    tab->state = NULL;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: EDITOR LIBRE → editorBim (PTY)
 *  Lanza scripts/editorBim/bim.sh en un pseudo-terminal embebido en SDL.
 *  Ver src/editorBim.c para la implementacion completa.
 * ════════════════════════════════════════════════════════════════════════════ */

/* ════════════════════════════════════════════════════════════════════════════
 *  PANEL: MAZMORRA
 *
 *  La intro 3D de la mazmorra (screenPJ_intro) se ejecuta al abrir el tab.
 *  Cuando termina, el tab queda como recordatorio de que ya la recorriste.
 * ════════════════════════════════════════════════════════════════════════════ */

static void
mazmorra_init(ShellCtx *ctx, Tab *tab)
{
    (void)tab;
    /* Lanzar la mazmorra inmediatamente al abrir el tab */
    SDL_GetWindowSize(ctx->ventana, &ctx->ancho, &ctx->alto);
    screenPJ_intro(ctx->renderer, ctx->ventana, ctx->ancho, ctx->alto);
}

static void mazmorra_event  (ShellCtx *ctx, Tab *tab, SDL_Event *e) { (void)ctx; (void)tab; (void)e; }
static void mazmorra_cleanup(ShellCtx *ctx, Tab *tab)               { (void)ctx; (void)tab; }

static void
mazmorra_draw(ShellCtx *ctx, Tab *tab, SDL_FRect area)
{
    (void)tab;
    SDL_Renderer *r = ctx->renderer;
    TTF_Font     *f = ctx->fuente;

    ui_clip(r, &area);

    const char *msg = "Mazmorra completada";
    int tw, th;
    TTF_GetStringSize(f, msg, 0, &tw, &th);
    sh_text(r, f, msg,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 - th - 8,
        COL_ACCENT);

    const char *sub = "Cierra el tab para volver. Volvelo a abrir para reiniciar.";
    TTF_GetStringSize(f, sub, 0, &tw, &th);
    sh_text(r, f, sub,
        area.x + (area.w - tw) / 2,
        area.y + area.h / 2 + 8,
        COL_TEXT);

    /* Boton relanzar */
    int bw = 180, bh = 28;
    int bx = area.x + (area.w - bw) / 2;
    int by = area.y + area.h / 2 + th + 28;
    int mx, my;
    ui_mouse(&mx, &my);
    int hover = (mx >= bx && mx < bx + bw && my >= by && my < by + bh);

    SDL_SetRenderDrawColor(r, hover ? 30 : 14, hover ? 30 : 14, hover ? 30 : 14, 255);
    SDL_FRect btn = {bx, by, bw, bh};
    SDL_RenderFillRect(r, &btn);
    SDL_SetRenderDrawColor(r, COL_SEP.r, COL_SEP.g, COL_SEP.b, 255);
    SDL_RenderRect(r, &btn);

    const char *blbl = "Recorrer de nuevo  [Enter]";
    TTF_GetStringSize(f, blbl, 0, &tw, NULL);
    sh_text(r, f, blbl, bx + (bw - tw) / 2, by + 6,
        hover ? COL_ACTIVE : COL_TEXT);

    if (hover && (ui_mouse(NULL, NULL) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) {
        SDL_GetWindowSize(ctx->ventana, &ctx->ancho, &ctx->alto);
        screenPJ_intro(ctx->renderer, ctx->ventana, ctx->ancho, ctx->alto);
    }

    ui_clip(r, NULL);
}

/* ── Registro de todos los paneles ── */
static void
registrar_paneles(ShellCtx *ctx)
{
    /* Placeholders para todos */
    for (int i = 0; i < PANEL_COUNT; i++) {
        ctx->defs[i].id           = (PanelID)i;
        ctx->defs[i].nombre       = SIDEBAR_LABELS[i];
        ctx->defs[i].init         = panel_noop_init;
        ctx->defs[i].handle_event = panel_noop_event;
        ctx->defs[i].draw         = panel_placeholder_draw;
        ctx->defs[i].cleanup      = panel_noop_cleanup;
    }

    /* Paneles portados al nuevo sistema (reemplazan el placeholder) */
    ctx->defs[PANEL_POMODORO].init         = pomodoro_init;
    ctx->defs[PANEL_POMODORO].handle_event = pomodoro_handle_event;
    ctx->defs[PANEL_POMODORO].draw         = pomodoro_draw;
    ctx->defs[PANEL_POMODORO].cleanup      = pomodoro_cleanup;

    ctx->defs[PANEL_DOC].init         = doc_init;
    ctx->defs[PANEL_DOC].handle_event = doc_handle_event;
    ctx->defs[PANEL_DOC].draw         = doc_draw;
    ctx->defs[PANEL_DOC].cleanup      = doc_cleanup;

    ctx->defs[PANEL_NIVELES].init         = niveles_init;
    ctx->defs[PANEL_NIVELES].handle_event = niveles_handle_event;
    ctx->defs[PANEL_NIVELES].draw         = niveles_draw;
    ctx->defs[PANEL_NIVELES].cleanup      = niveles_cleanup;

    ctx->defs[PANEL_CONFIG].init         = config_init;
    ctx->defs[PANEL_CONFIG].handle_event = config_handle_event;
    ctx->defs[PANEL_CONFIG].draw         = config_draw;
    ctx->defs[PANEL_CONFIG].cleanup      = config_cleanup;

    ctx->defs[PANEL_EDITOR_LIBRE].init         = editor_bim_init;
    ctx->defs[PANEL_EDITOR_LIBRE].handle_event = editor_bim_handle_event;
    ctx->defs[PANEL_EDITOR_LIBRE].draw         = editor_bim_draw_panel;
    ctx->defs[PANEL_EDITOR_LIBRE].cleanup      = editor_bim_cleanup;

    ctx->defs[PANEL_MAZMORRA].init         = mazmorra_init;
    ctx->defs[PANEL_MAZMORRA].handle_event = mazmorra_event;
    ctx->defs[PANEL_MAZMORRA].draw         = mazmorra_draw;
    ctx->defs[PANEL_MAZMORRA].cleanup      = mazmorra_cleanup;

    ctx->defs[PANEL_C_EDITOR].init         = ceditor_init;
    ctx->defs[PANEL_C_EDITOR].handle_event = ceditor_handle_event;
    ctx->defs[PANEL_C_EDITOR].draw         = ceditor_draw;
    ctx->defs[PANEL_C_EDITOR].cleanup      = ceditor_cleanup;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  screenShell — EL LOOP PRINCIPAL
 * ════════════════════════════════════════════════════════════════════════════ */

void
screenShell(SDL_Renderer *renderer, TTF_Font *fuente,
            int ancho, int alto, SDL_Window *ventana)
{
    /* Inicializar el contexto */
    ShellCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.renderer       = renderer;
    ctx.fuente         = fuente;
    ctx.ventana        = ventana;
    ctx.ancho          = ancho;
    ctx.alto           = alto;
    ctx.salir          = 0;
    ctx.sidebar_abierto = 1;
    ctx.sidebar_w      = SIDEBAR_W;
    ctx.n_tabs         = 0;
    ctx.tab_activo     = 0;

    /* Registrar las definiciones de paneles */
    registrar_paneles(&ctx);

    /* Cargar el icono del logo desde PNG */
    {
        SDL_Surface *s = IMG_Load("assets/Icono/into-logo/logoIcon.png");
        ctx.logo = s ? SDL_CreateTextureFromSurface(renderer, s) : NULL;
        if (s) SDL_DestroySurface(s);
    }

    /* ── Loop principal ── */
    int pending_resize = 0;   /* 1 = recrear renderer al inicio del próximo frame */

    while (!ctx.salir) {

        /* ── Recrear renderer si hubo resize (diferido para evitar dangling ptr) ── */
        if (pending_resize) {
            pending_resize = 0;
            SDL_Surface *ns = SDL_GetWindowSurface(ventana);
            if (ns) {
                if (ctx.logo) { SDL_DestroyTexture(ctx.logo); ctx.logo = NULL; }
                SDL_DestroyRenderer(renderer);
                renderer     = NULL;   /* evitar dangling ptr si CreateSoftware falla */
                ctx.renderer = NULL;
                SDL_Renderer *nr = SDL_CreateSoftwareRenderer(ns);
                if (nr) {
                    renderer     = nr;
                    ctx.renderer = nr;
                    SDL_Surface *ls = IMG_Load("assets/Icono/into-logo/logoIcon.png");
                    ctx.logo = ls ? SDL_CreateTextureFromSurface(renderer, ls) : NULL;
                    if (ls) SDL_DestroySurface(ls);
                } else {
                    /* Surface existe pero renderer falló — reintentar */
                    pending_resize = 1;
                    SDL_Delay(16);
                    continue;
                }
            } else {
                /* Surface no disponible todavía — reintentar */
                pending_resize = 1;
                SDL_Delay(16);
                continue;
            }
        }

        /* Protección adicional: nunca renderizar sin renderer válido */
        if (!renderer) { SDL_Delay(16); continue; }

        /* Leer el tamaño real de la ventana (captura resize/fullscreen) */
        SDL_GetWindowSize(ventana, &ctx.ancho, &ctx.alto);

        /* Ancho del sidebar segun estado */
        ctx.sidebar_w = ctx.sidebar_abierto ? SIDEBAR_W : SIDEBAR_COLLAPSED_W;

        /* Calcular los rects de cada zona */
        SDL_FRect content_rect = {
            ctx.sidebar_w,
            TAB_BAR_H,
            ctx.ancho - ctx.sidebar_w,
            ctx.alto  - TAB_BAR_H
        };
        SDL_FRect tabbar_rect = {
            ctx.sidebar_w,
            0,
            ctx.ancho - ctx.sidebar_w,
            TAB_BAR_H
        };

        /* Servicios de fondo */
        audio_tick();
        pom_tick();

        /* ── Procesar eventos ── */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {

            if (e.type == SDL_EVENT_QUIT) {
                ctx.salir = 1;
                break;
            }

            /* Resize/maximize: marcar para recrear el renderer en el próximo frame.
             * NO recrear aquí dentro del event loop — SDL puede disparar múltiples
             * eventos SIZE_CHANGED + RESIZED + MAXIMIZED en sucesión rápida y la
             * window surface queda transitoriamente inválida en WSL2/XWayland.
             * La recreación diferida (pending_resize) evita dangling pointers. */
            /* SDL3: se acabo el evento paraguas SDL_WINDOWEVENT con un
               subcampo .window.event. Ahora cada uno es un type propio. */
            if (e.type == SDL_EVENT_WINDOW_RESIZED            ||
                e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
                e.type == SDL_EVENT_WINDOW_MAXIMIZED          ||
                e.type == SDL_EVENT_WINDOW_RESTORED) {
                pending_resize = 1;
            }

            /* Click del mouse */
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                e.button.button == SDL_BUTTON_LEFT) {

                int cx = e.button.x;
                int cy = e.button.y;

                /* ¿Click en el sidebar? */
                int hit = sidebar_hit(&ctx, cx, cy);
                if (hit == SHELL_TOGGLE) {
                    ctx.sidebar_abierto = !ctx.sidebar_abierto;
                } else if (hit == SHELL_SALIR) {
                    ctx.salir = 1;
                } else if (hit >= 0) {
                    abrir_o_activar_tab(&ctx, (PanelID)hit);
                }

                /* ¿Click en la barra de tabs? */
                int es_close = 0;
                int tab_hit  = tabbar_hit(&ctx, cx, cy, tabbar_rect, &es_close);
                if (tab_hit >= 0) {
                    if (es_close) {
                        cerrar_tab(&ctx, tab_hit);
                    } else {
                        ctx.tab_activo = tab_hit;
                    }
                }
            }

            /* Pasar el evento al panel activo (si hay tabs abiertos) */
            if (ctx.n_tabs > 0 && e.type != SDL_EVENT_QUIT) {
                Tab *tab = &ctx.tabs[ctx.tab_activo];
                ctx.defs[tab->id].handle_event(&ctx, tab, &e);
            }
        }

        /* ── Render ── */

        /* Si este frame tuvo un resize, saltear el render:
         * la window surface ya fue invalidada por SDL.
         * El renderer se recrea al inicio del próximo frame. */
        if (pending_resize) {
            SDL_Delay(16);
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        /* Sidebar */
        draw_sidebar(&ctx);

        /* Tab bar */
        draw_tab_bar(&ctx, tabbar_rect);

        /* Contenido del tab activo (o pantalla de bienvenida) */
        if (ctx.n_tabs > 0) {
            Tab *tab = &ctx.tabs[ctx.tab_activo];
            ctx.defs[tab->id].draw(&ctx, tab, content_rect);
        } else {
            draw_welcome(&ctx, content_rect);
        }

        SDL_UpdateWindowSurface(ctx.ventana);
        SDL_Delay(16);
    }

    /* ── Cleanup: cerrar todos los tabs abiertos ── */
    for (int i = ctx.n_tabs - 1; i >= 0; i--)
        cerrar_tab(&ctx, i);

    if (ctx.logo) SDL_DestroyTexture(ctx.logo);
}
