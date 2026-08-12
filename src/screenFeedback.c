#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include "ui.h"

#define URL_ISSUES "https://github.com/BrianKalbermatter/VimMon/issues"

/* ---- helpers ---- */
static inline void
fr(SDL_Renderer *r, int x, int y, int w, int h, int cr, int cg, int cb, int ca)
{
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(r, &rc);
}
static inline void
ln(SDL_Renderer *r, int x1, int y1, int x2, int y2, int cr, int cg, int cb, int ca)
{
    SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}
static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------------
 * dibujar_fondo_castillo
 *
 * Pasillo de mazmorra fantástica:
 *   – múltiples arcos semicirculares con perspectiva
 *   – columnas de piedra cálida rojo-marrón
 *   – piso de ladrillos en perspectiva
 *   – antorchas animadas con glow fuerte
 *   – iluminación naranja dominante
 * ------------------------------------------------------------------ */
static void
dibujar_fondo_castillo(SDL_Renderer *renderer, int ancho, int alto, Uint32 ticks)
{
    const int vx = ancho / 2;
    const int vy = (int)(alto * 0.37f);   /* punto de fuga */

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* ── FONDO NEGRO ── */
    SDL_SetRenderDrawColor(renderer, 4, 2, 1, 255);
    SDL_RenderClear(renderer);

    /* ── PARÁMETROS DE ARCOS  (de más cercano a más lejano) ──
       .hw = radio interior del arco (half-width de la apertura)
       .by = y de la base del arco (nivel del suelo en esa profundidad)
       .ft = grosor del marco / ancho de columna               */
    typedef struct { int hw, by, ft; } Arch;
    const Arch A[] = {
        { (int)(ancho * 0.410f), (int)(alto * 0.920f), (int)(ancho * 0.075f) },
        { (int)(ancho * 0.255f), (int)(alto * 0.840f), (int)(ancho * 0.052f) },
        { (int)(ancho * 0.155f), (int)(alto * 0.785f), (int)(ancho * 0.034f) },
        { (int)(ancho * 0.093f), (int)(alto * 0.748f), (int)(ancho * 0.023f) },
    };
    const int NA = 4;

    /* ================================================================
     * 1.  RELLENO INTERIOR DEL CORREDOR
     *     Dibujar de cercano a lejano: cada arco pinta su interior
     *     con warmth decreciente, creando capas de profundidad.
     * ================================================================ */
    for (int i = 0; i < NA; i++) {
        int hw  = A[i].hw;
        int by  = A[i].by;
        int cly = by - hw;        /* y del centro del semicírculo */
        int top = cly - hw;       /* punto más alto del arco      */

        float depth = (float)i / (NA - 1);   /* 0=cerca, 1=lejos */

        for (int y = (top > 0 ? top : 0); y < by && y < alto; y++) {
            int xl, xr;

            if (y < cly) {
                /* zona semicircular */
                float dy = (float)(cly - y);
                float sq = (float)(hw * hw) - dy * dy;
                if (sq < 0) continue;
                int h2 = (int)sqrtf(sq);
                xl = vx - h2;  xr = vx + h2;
            } else {
                /* zona rectangular */
                xl = vx - hw;  xr = vx + hw;
            }

            /* degradado vertical: suelo cálido, techo oscuro */
            float fy = (float)(y - top) / (float)(by - top);
            /* warmth global decrece con la profundidad */
            float warm = (1.0f - depth * 0.75f) * (0.15f + fy * 0.55f);

            int r2 = clampi((int)(warm * 110), 0, 255);
            int g2 = clampi((int)(warm *  55), 0, 255);
            int b2 = clampi((int)(warm *  20), 0, 255);

            SDL_SetRenderDrawColor(renderer, r2, g2, b2, 255);
            SDL_RenderDrawLine(renderer, xl, y, xr, y);
        }
    }

    /* ================================================================
     * 2.  SUELO DE LADRILLOS EN PERSPECTIVA
     * ================================================================ */
    {
        int by0  = A[0].by;
        int hw0  = A[0].hw;

        /* base del suelo: sobreescribir la zona inferior con color ladrillo */
        int floor_top = (int)(vy + (by0 - vy) * 0.35f);

        for (int y = floor_top; y < by0 && y < alto; y++) {
            float t = (float)(y - vy) / (float)(by0 - vy);
            t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
            int xl = vx - (int)(hw0 * t);
            int xr = vx + (int)(hw0 * t);

            /* ladrillo: naranja-marrón con gradiente más cálido hacia el jugador */
            int fl = clampi((int)(25 + t * 65), 0, 255);
            SDL_SetRenderDrawColor(renderer,
                clampi((int)(fl * 1.55f), 0, 255),
                clampi((int)(fl * 0.85f), 0, 255),
                clampi((int)(fl * 0.40f), 0, 255), 255);
            SDL_RenderDrawLine(renderer, xl, y, xr, y);
        }

        /* líneas de perspectiva (juntas de ladrillos convergentes) */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        {
            int n = 12;
            for (int i = 0; i <= n; i++) {
                int xb = (vx - hw0) + (2 * hw0) * i / n;
                ln(renderer, xb, by0, vx, vy, 18, 8, 3, 135);
            }
        }
        /* juntas horizontales (cuadráticas → más densas lejos) */
        {
            int n = 10;
            for (int i = 1; i < n; i++) {
                float f   = (float)i / n;
                int   y   = (int)(vy + (by0 - vy) * f * f);
                if (y < floor_top) continue;
                float tv  = (float)(y - vy) / (float)(by0 - vy);
                int   xl  = vx - (int)(hw0 * tv);
                int   xr  = vx + (int)(hw0 * tv);
                int   al  = clampi((int)(50 + 110 * f), 0, 255);
                ln(renderer, xl, y, xr, y, 20, 9, 4, al);
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    /* ================================================================
     * 3.  COLUMNAS + ARCOS  (de más lejano a más cercano → painter)
     * ================================================================ */
    for (int i = NA - 1; i >= 0; i--) {
        int hw = A[i].hw;
        int by = A[i].by;
        int ft = A[i].ft;
        int ro = hw + ft;          /* radio exterior del arco */
        int cly= by - hw;          /* y del centro del semicírculo */

        float depth = (float)i / (NA - 1);
        float brig  = 1.0f - depth * 0.48f;

        /* piedra: rojo-marrón cálida */
        int sr = clampi((int)(130 * brig), 0, 255);
        int sg = clampi((int)( 72 * brig), 0, 255);
        int sb = clampi((int)( 36 * brig), 0, 255);

        /* ─── Columnas rectangulares ─── */
        /* columna izquierda */
        SDL_SetRenderDrawColor(renderer, sr, sg, sb, 255);
        SDL_Rect col_l = { vx - ro, 0, ft, by };
        SDL_RenderFillRect(renderer, &col_l);
        /* columna derecha */
        SDL_Rect col_r = { vx + hw, 0, ft, by };
        SDL_RenderFillRect(renderer, &col_r);

        /* cara interna de las columnas (más clara → efecto 3D) */
        {
            int fw = clampi(ft / 3, 2, 20);
            int rr = clampi(sr + 35, 0, 255);
            int gg = clampi(sg + 20, 0, 255);
            int bb = clampi(sb + 10, 0, 255);
            SDL_SetRenderDrawColor(renderer, rr, gg, bb, 255);
            SDL_Rect clf = { vx - hw - fw, 0, fw, by };
            SDL_RenderFillRect(renderer, &clf);
            SDL_Rect crf = { vx + hw, 0, fw, by };
            SDL_RenderFillRect(renderer, &crf);
        }

        /* sombra lateral de columnas (cara exterior más oscura) */
        {
            int fw = clampi(ft / 4, 1, 12);
            int rr = clampi(sr - 30, 0, 255);
            int gg = clampi(sg - 16, 0, 255);
            int bb = clampi(sb -  8, 0, 255);
            SDL_SetRenderDrawColor(renderer, rr, gg, bb, 255);
            SDL_Rect cls = { vx - ro, 0, fw, by };
            SDL_RenderFillRect(renderer, &cls);
            SDL_Rect crs = { vx + ro - fw, 0, fw, by };
            SDL_RenderFillRect(renderer, &crs);
        }

        /* juntas horizontales en columnas (bloques de piedra) */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        {
            int bh = clampi(22 - i * 4, 5, 22);
            for (int y = bh; y < by; y += bh) {
                ln(renderer, vx - ro, y, vx - hw, y, 10, 5, 2, 165);
                ln(renderer, vx + hw, y, vx + ro, y, 10, 5, 2, 165);
                /* highlight encima de la junta */
                ln(renderer, vx - ro, y-1, vx - hw, y-1, 160, 90, 45, 40);
                ln(renderer, vx + hw, y-1, vx + ro, y-1, 160, 90, 45, 40);
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        /* ─── Arco semicircular ─── */
        for (int y = cly - ro; y <= cly; y++) {
            if (y < 0 || y >= alto) continue;

            float dy     = (float)(cly - y);
            float sq_out = (float)(ro * ro) - dy * dy;
            if (sq_out < 0) continue;
            int xo = (int)sqrtf(sq_out);

            float sq_in = (float)(hw * hw) - dy * dy;
            int xi = (sq_in >= 0) ? (int)sqrtf(sq_in) : 0;

            /* el anillo del arco → color de piedra con keystone más claro */
            float ks   = 1.0f + 0.30f * (1.0f - dy / (float)ro);
            int rr = clampi((int)(sr * ks), 0, 255);
            int gg = clampi((int)(sg * ks), 0, 255);
            int bb = clampi((int)(sb * ks), 0, 255);
            SDL_SetRenderDrawColor(renderer, rr, gg, bb, 255);

            SDL_RenderDrawLine(renderer, vx - xo, y, vx - xi, y);
            SDL_RenderDrawLine(renderer, vx + xi, y, vx + xo, y);
        }

        /* piedra clave (keystone): destaque en el tope del arco */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        {
            int ksw = clampi(ft / 2, 4, 18);
            int rr  = clampi(sr + 50, 0, 255);
            int gg  = clampi(sg + 28, 0, 255);
            int bb  = clampi(sb + 14, 0, 255);
            fr(renderer, vx - ksw/2, cly - ro, ksw, clampi(ft/2, 4, 16),
               rr, gg, bb, 190);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    /* ================================================================
     * 4.  LUZ LEJANA AL FINAL DEL CORREDOR
     * ================================================================ */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int r = 90; r > 0; r -= 10) {
        float f  = (float)r / 90.0f;
        int   al = (int)((1.0f - f * f) * 60);
        fr(renderer, vx - r, vy - r, r * 2, r * 2, 170, 100, 40, al);
    }
    fr(renderer, vx -  7, vy - 11, 14, 22, 200, 140, 60, 130);
    fr(renderer, vx -  2, vy -  5,  5, 10, 245, 215, 140, 220);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    /* ================================================================
     * 5.  ANTORCHAS ANIMADAS
     * ================================================================ */
    {
        /* flicker orgánico: suma de 3 senos con frecuencias irracionales */
        float flk0 = 1.0f
            + 0.15f * sinf(ticks * 0.00680f)
            + 0.08f * sinf(ticks * 0.01970f)
            + 0.05f * sinf(ticks * 0.04010f);

        float flk1 = 1.0f
            + 0.12f * sinf(ticks * 0.00720f + 1.8f)
            + 0.07f * sinf(ticks * 0.02010f + 0.6f)
            + 0.04f * sinf(ticks * 0.03850f + 2.2f);

        /* ── Antorchas en ARCO 0 (más cercano) ── */
        {
            int hw0 = A[0].hw;
            int ft0 = A[0].ft;
            int by0 = A[0].by;
            int fh  = (int)(30 * flk0);
            int fw  = (int)(12 * (0.87f + 0.13f * sinf(ticks * 0.01220f)));
            fw = clampi(fw, 7, 16);

            /* montadas en la cara INTERNA de cada columna, a ~42% de altura */
            int ty   = (int)(by0 * 0.42f);
            int tx_l = vx - hw0 - ft0 / 2 - fw / 2;
            int tx_r = vx + hw0 + ft0 / 2 - fw / 2;

            for (int side = 0; side < 2; side++) {
                int tx = (side == 0) ? tx_l : tx_r;

                /* halo de calor expandido */
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                for (int gr = 140; gr > 8; gr -= 14) {
                    float gf = (float)gr / 140.0f;
                    int   ga = (int)((1.0f - gf) * (1.0f - gf) * 80);
                    fr(renderer,
                       tx - gr + fw/2, ty - gr + fh/2, gr * 2, gr * 2,
                       235, 105, 18, ga);
                }

                /* soporte de hierro */
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                fr(renderer, tx + 1,       ty,       fw - 2, 15, 45, 30, 15, 255);
                fr(renderer, tx + fw/2 - 1, ty,      3,      22, 36, 24, 12, 255);

                /* llama: capas de color de adentro hacia afuera */
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                fr(renderer, tx,           ty - fh,     fw,     fh,       228, 74,  8, 185);
                fr(renderer, tx + 2,       ty - fh + 3, fw - 4, fh - 3,   255, 152, 24, 215);
                fr(renderer, tx + fw/2-2,  ty - fh + 6, 5,      fh - 7,   255, 235, 92, 232);
                fr(renderer, tx + fw/2-1,  ty - fh,     3,      5,        255, 255, 200, 228);
            }

            /* iluminación cálida sobre paredes y suelo desde estas antorchas */
            fr(renderer, 0,            ty - 110, tx_l + 140, 260, 190, 80, 12, 32);
            fr(renderer, tx_r - 90,    ty - 110, ancho,      260, 190, 80, 12, 32);
        }

        /* ── Antorchas en ARCO 1 (media distancia) ── */
        {
            int hw1 = A[1].hw;
            int ft1 = A[1].ft;
            int by1 = A[1].by;
            int fh2 = (int)(21 * flk1);
            int fw2 = clampi((int)(9 * (0.88f + 0.12f * sinf(ticks * 0.01150f + 1.2f))), 6, 13);

            int ty2  = (int)(by1 * 0.46f);
            int tx_l2 = vx - hw1 - ft1 / 2 - fw2 / 2;
            int tx_r2 = vx + hw1 + ft1 / 2 - fw2 / 2;

            for (int side = 0; side < 2; side++) {
                int tx = (side == 0) ? tx_l2 : tx_r2;

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                for (int gr = 90; gr > 6; gr -= 13) {
                    float gf = (float)gr / 90.0f;
                    int   ga = (int)((1.0f - gf) * (1.0f - gf) * 65);
                    fr(renderer,
                       tx - gr + fw2/2, ty2 - gr + fh2/2, gr * 2, gr * 2,
                       225, 95, 14, ga);
                }

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                fr(renderer, tx + 1, ty2, fw2 - 2, 12, 42, 28, 13, 255);
                fr(renderer, tx + fw2/2 - 1, ty2, 3, 18, 34, 22, 10, 255);

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                fr(renderer, tx,          ty2 - fh2,     fw2,     fh2,       222, 70,  7, 180);
                fr(renderer, tx + 1,      ty2 - fh2 + 3, fw2 - 2, fh2 - 3,   255, 145, 20, 212);
                fr(renderer, tx+fw2/2-1,  ty2 - fh2 + 5, 3,       fh2 - 6,   255, 228, 88, 228);
            }

            /* iluminación secundaria (más suave que las antorchas del frente) */
            fr(renderer, 0,            ty2 - 70, tx_l2 + 90, 180, 160, 65, 10, 22);
            fr(renderer, tx_r2 - 55, ty2 - 70, ancho,        180, 160, 65, 10, 22);
        }
    }

    /* ================================================================
     * 6.  NEBLINA CÁLIDA  (haze de partículas de polvo)
     * ================================================================ */
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (int r = 280; r > 35; r -= 35) {
        float f  = (float)r / 280.0f;
        int   al = (int)((1.0f - f) * (1.0f - f) * 20);
        fr(renderer, vx - r, vy - r * 3 / 4, r * 2, r * 3 / 2, 155, 68, 16, al);
    }

    /* ================================================================
     * 7.  VIGNETTE  (oscurecer bordes para meter al jugador en escena)
     * ================================================================ */
    {
        int gw = ancho / 4;
        int gh = alto  / 5;

        for (int i = 0; i < gw; i += 3) {
            float f = (float)i / gw;
            int   a = clampi((int)((1.0f - f) * (1.0f - f) * 240), 0, 255);
            ln(renderer, i,       0, i,       alto, 0, 0, 0, a);
            ln(renderer, ancho-i, 0, ancho-i, alto, 0, 0, 0, a);
        }
        for (int i = 0; i < gh; i += 3) {
            float f = (float)i / gh;
            int   a = clampi((int)((1.0f - f) * (1.0f - f) * 215), 0, 255);
            ln(renderer, 0, i,      ancho, i,      0, 0, 0, a);
            ln(renderer, 0, alto-i, ancho, alto-i, 0, 0, 0, a);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}

/* ------------------------------------------------------------------
 * screenFeedback
 * ------------------------------------------------------------------ */
int
screenFeedback(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto)
{
    SDL_Event evento;
    int corriendo = 1;
    int mx = 0, my = 0;

    SDL_Rect link_rect = {0, 0, 0, 0};

    while (corriendo) {
        int clicked = 0, click_x = 0, click_y = 0;

        while (SDL_PollEvent(&evento)) {
            if (evento.type == SDL_QUIT) return 0;
            if (evento.type == SDL_KEYDOWN &&
                evento.key.keysym.sym == SDLK_ESCAPE)
                corriendo = 0;
            if (evento.type == SDL_MOUSEBUTTONDOWN &&
                evento.button.button == SDL_BUTTON_LEFT) {
                clicked = 1;
                click_x = evento.button.x;
                click_y = evento.button.y;
            }
        }

        SDL_GetMouseState(&mx, &my);

        /* ── fondo ── */
        dibujar_fondo_castillo(renderer, ancho, alto, SDL_GetTicks());

        /* ── panel de texto ── */
        int line_h  = TTF_FontHeight(fuente) + 10;
        int cx      = ancho / 2;
        int panel_w = 510;
        int panel_h = line_h * 6 + 85;
        int panel_x = cx - panel_w / 2;
        int panel_y = alto / 2 - panel_h / 2 - 5;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        /* sombra */
        fr(renderer, panel_x + 6, panel_y + 6, panel_w, panel_h, 0, 0, 0, 120);
        /* fondo del panel */
        fr(renderer, panel_x, panel_y, panel_w, panel_h, 6, 4, 2, 215);
        /* borde exterior: naranja cálido (acorde al estilo mazmorra) */
        SDL_SetRenderDrawColor(renderer, 140, 72, 20, 220);
        SDL_Rect borde1 = {panel_x, panel_y, panel_w, panel_h};
        SDL_RenderDrawRect(renderer, &borde1);
        /* borde interior fino */
        SDL_SetRenderDrawColor(renderer, 80, 42, 12, 150);
        SDL_Rect borde2 = {panel_x + 3, panel_y + 3, panel_w - 6, panel_h - 6};
        SDL_RenderDrawRect(renderer, &borde2);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        /* ── contenido ── */
        int cy = panel_y + 22;

        /* título */
        SDL_Color c_titulo = {220, 185, 120, 255};
        {
            int tw; TTF_SizeUTF8(fuente, "Feedback y Contacto", &tw, NULL);
            dibujadoTextoColor(renderer, fuente, "Feedback y Contacto",
                cx - tw/2 - 10, cy, c_titulo);
            cy += line_h + 4;
            SDL_SetRenderDrawColor(renderer, 120, 62, 16, 200);
            SDL_RenderDrawLine(renderer, cx - 180, cy - 4, cx + 180, cy - 4);
            cy += 10;
        }

        /* descripción */
        {
            SDL_Color c_desc = {170, 155, 130, 255};
            const char *desc = "Encontraste un bug o tenes una sugerencia?";
            int tw; TTF_SizeUTF8(fuente, desc, &tw, NULL);
            dibujadoTextoColor(renderer, fuente, desc,
                cx - tw/2 - 10, cy, c_desc);
            cy += line_h + 8;
        }

        /* link clickeable */
        {
            int hover = (mx >= link_rect.x && mx <= link_rect.x + link_rect.w &&
                         my >= link_rect.y && my <= link_rect.y + link_rect.h);

            SDL_Color c_link = hover
                ? (SDL_Color){140, 220, 255, 255}
                : (SDL_Color){ 70, 165, 255, 255};

            int tw, th;
            TTF_SizeUTF8(fuente, URL_ISSUES, &tw, &th);
            int lx = cx - tw/2 - 10;

            link_rect.x = lx + 10;
            link_rect.y = cy + 12;
            link_rect.w = tw;
            link_rect.h = th;

            if (hover) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 70, 165, 255, 28);
                SDL_Rect bg2 = {link_rect.x - 6, link_rect.y - 2,
                                link_rect.w + 12, link_rect.h + 4};
                SDL_RenderFillRect(renderer, &bg2);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            dibujadoTextoColor(renderer, fuente, URL_ISSUES, lx, cy, c_link);
            SDL_SetRenderDrawColor(renderer, c_link.r, c_link.g, c_link.b, 200);
            SDL_RenderDrawLine(renderer,
                link_rect.x, link_rect.y + th + 1,
                link_rect.x + link_rect.w, link_rect.y + th + 1);

            if (hover) {
                SDL_Color c_hint = {100, 195, 255, 180};
                dibujadoTextoColor(renderer, fuente,
                    "click para abrir en el navegador",
                    cx - 130, cy + line_h + 2, c_hint);
            }

            if (clicked &&
                click_x >= link_rect.x && click_x <= link_rect.x + link_rect.w &&
                click_y >= link_rect.y && click_y <= link_rect.y + link_rect.h)
                SDL_OpenURL(URL_ISSUES);

            cy += line_h + 16;
        }

        /* separador */
        SDL_SetRenderDrawColor(renderer, 80, 42, 10, 160);
        SDL_RenderDrawLine(renderer, cx - 180, cy, cx + 180, cy);
        cy += 14;

        /* mail */
        {
            SDL_Color c_label = {180, 130, 60, 255};
            SDL_Color c_mail  = {175, 165, 145, 255};
            const char *mail  = "ctejant@gmail.com";
            int tw1, tw2;
            TTF_SizeUTF8(fuente, "Mail: ", &tw1, NULL);
            TTF_SizeUTF8(fuente, mail,     &tw2, NULL);
            int total = tw1 + tw2;
            dibujadoTextoColor(renderer, fuente, "Mail: ",
                cx - total/2 - 10, cy, c_label);
            dibujadoTextoColor(renderer, fuente, mail,
                cx - total/2 + tw1 - 10, cy, c_mail);
            cy += line_h + 6;
        }

        /* versión */
        {
            SDL_Color c_ver = {80, 65, 45, 255};
            const char *ver = "PseudoGames v0.1 - aed_pseudo";
            int tw; TTF_SizeUTF8(fuente, ver, &tw, NULL);
            dibujadoTextoColor(renderer, fuente, ver,
                cx - tw/2 - 10, cy + 10, c_ver);
        }

        /* pie */
        {
            SDL_Color c_esc = {100, 80, 50, 255};
            const char *pie = "[ESC] para volver";
            int tw; TTF_SizeUTF8(fuente, pie, &tw, NULL);
            dibujadoTextoColor(renderer, fuente, pie,
                cx - tw/2 - 10, alto - 40, c_esc);
        }

        presente(renderer);
        SDL_Delay(16);
    }

    return 0;
}
