#include <SDL2/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define W        800
#define H        600
#define FOV      (M_PI / 3.0)
#define MAPA_W   8
#define MAPA_H   8
#define MOVE_VEL   0.04
#define ROT_VEL    0.03
#define MOUSE_SENS 0.0008
#define TEX_W      32
#define TEX_H      32

/* antorcha */
#define TORCH_BASE  3.8    /* radio base de iluminacion en metros */
#define TORCH_FLICK 0.22   /* amplitud del parpadeo */

/* ── Mapa ────────────────────────────────────────────────────────── */
static int mapa[MAPA_H][MAPA_W] = {
    {1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1},
};

/* ── Jugador ─────────────────────────────────────────────────────── */
typedef struct { double x, y, angulo; } Jugador;

/* ── Bob de la antorcha al caminar ───────────────────────────────── */
static double bob_fase  = 0.0;
static double bob_y     = 0.0;
static double bob_x     = 0.0;

/* ── Textura adoquin de mazmorra (32x32) ─────────────────────────── */
static Uint8 tex[TEX_H][TEX_W][3];

static void generar_textura(void) {
    /* ladrillo: cada ladrillo 14px ancho x 8px alto, juntas de 2px */
    int bw = 14;   /* ancho ladrillo */
    int bh =  8;   /* alto ladrillo  */

    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            int row    = y / bh;
            int offset = (row % 2) * (bw / 2);   /* ladrillos alternados */
            int bx     = (x + offset) % bw;
            int by     = y % bh;

            int junta = (by <= 1 || bx <= 1);

            if (junta) {
                /* junta oscura entre piedras */
                tex[y][x][0] = 10 + rand() % 8;
                tex[y][x][1] = 10 + rand() % 8;
                tex[y][x][2] = 14 + rand() % 8;
            } else {
                /* piedra: gris azulado con variacion pixelada */
                int base = 45 + rand() % 30;
                tex[y][x][0] = (Uint8)(base * 0.85);
                tex[y][x][1] = (Uint8)(base * 0.80);
                tex[y][x][2] = (Uint8)(base);

                /* manchas oscuras "desgastadas" */
                if (rand() % 15 == 0) {
                    tex[y][x][0] /= 3;
                    tex[y][x][1] /= 3;
                    tex[y][x][2] /= 3;
                }
            }
        }
    }
}

/* ── Framebuffer y z-buffer ──────────────────────────────────────── */
static Uint32 fb[H * W];
static double zbuf[W];    /* distancia de pared por columna */

static void fb_set(int x, int y, Uint8 r, Uint8 g, Uint8 b) {
    if ((unsigned)x >= W || (unsigned)y >= H) return;
    fb[y * W + x] = (0xFF000000) | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
}

/* ── Sprite: terminal vieja ──────────────────────────────────────── */
#define SPR_W 64
#define SPR_H 64

static Uint8 spr[SPR_H][SPR_W][4];   /* RGBA — A=0 transparente */


static void generar_sprite_terminal(void) {
    memset(spr, 0, sizeof(spr));

    /* ── mesa: madera oscura en la franja inferior ── */
    for (int y = 46; y < SPR_H; y++)
        for (int x = 0; x < SPR_W; x++) {
            Uint8 v = 42 + (y % 4 < 1 ? 10 : 0) + rand() % 8;
            spr[y][x][0] = v;
            spr[y][x][1] = (Uint8)(v * 0.55);
            spr[y][x][2] = 8;
            spr[y][x][3] = 255;
        }

    /* ── cuerpo del monitor: gris beige viejo ── */
    for (int y = 8; y < 48; y++)
        for (int x = 8; x < 56; x++) {
            spr[y][x][0] = 160 + rand() % 10;
            spr[y][x][1] = 155 + rand() % 10;
            spr[y][x][2] = 130 + rand() % 10;
            spr[y][x][3] = 255;
        }

    /* ── bisel del monitor (borde mas oscuro) ── */
    for (int i = 8; i < 56; i++) {
        spr[8][i][0]  = spr[8][i][1]  = spr[8][i][2]  = 90; spr[8][i][3]  = 255;
        spr[47][i][0] = spr[47][i][1] = spr[47][i][2] = 90; spr[47][i][3] = 255;
        spr[i][8][0]  = spr[i][8][1]  = spr[i][8][2]  = 90; spr[i][8][3]  = 255;
        spr[i][55][0] = spr[i][55][1] = spr[i][55][2] = 90; spr[i][55][3] = 255;
    }

    /* ── pantalla CRT: negro verdoso ── */
    for (int y = 12; y < 43; y++)
        for (int x = 12; x < 52; x++) {
            spr[y][x][0] = 2;
            spr[y][x][1] = 18 + rand() % 6;
            spr[y][x][2] = 5;
            spr[y][x][3] = 255;
        }

    /* ── texto verde fosforescente: 5 lineas ── */
    int lineas[] = {14, 19, 24, 29, 34};
    for (int l = 0; l < 5; l++) {
        int largo = 18 + rand() % 16;
        for (int x = 14; x < 14 + largo && x < 50; x++) {
            if (rand() % 5 == 0) continue;   /* espacios en el texto */
            spr[lineas[l]][x][0] = 10;
            spr[lineas[l]][x][1] = 200 + rand() % 55;
            spr[lineas[l]][x][2] = 30;
            spr[lineas[l]][x][3] = 255;
        }
    }

    /* ── cursor parpadeante (siempre visible en la textura) ── */
    for (int x = 14; x < 19; x++) {
        spr[39][x][0] = 20;
        spr[39][x][1] = 220;
        spr[39][x][2] = 40;
        spr[39][x][3] = 255;
    }

    /* ── base/pedestal del monitor ── */
    for (int y = 43; y < 47; y++)
        for (int x = 24; x < 40; x++) {
            spr[y][x][0] = 100; spr[y][x][1] = 95;
            spr[y][x][2] = 80;  spr[y][x][3] = 255;
        }

    /* ── teclado sobre la mesa ── */
    for (int y = 49; y < 55; y++)
        for (int x = 10; x < 54; x++) {
            Uint8 v = 120 + rand() % 15;
            spr[y][x][0] = v; spr[y][x][1] = (Uint8)(v*0.95);
            spr[y][x][2] = (Uint8)(v*0.80); spr[y][x][3] = 255;
        }
    /* teclas: lineas oscuras sobre el teclado */
    for (int x = 12; x < 52; x += 4)
        for (int y = 50; y < 54; y++) {
            spr[y][x][0] = 60; spr[y][x][1] = 58;
            spr[y][x][2] = 50; spr[y][x][3] = 255;
        }
}

/* ── Antorcha: radio que parpadea ───────────────────────────────── */
static double torch_radio = TORCH_BASE;

static void actualizar_antorcha(void) {
    /* ruido suave: mezcla el valor anterior con uno nuevo */
    double target = TORCH_BASE + ((rand() % 100) / 100.0 - 0.5) * 2.0 * TORCH_FLICK;
    torch_radio = torch_radio * 0.85 + target * 0.15;
}

/* ── Glitch ──────────────────────────────────────────────────────── */
static int glitch_offset[H];
static int glitch_timer = 0;

static void actualizar_glitch(void) {
    glitch_timer++;
    if (glitch_timer % 10 != 0) return;

    for (int y = 0; y < H; y++) glitch_offset[y] = 0;
    int franjas = 2 + rand() % 3;
    for (int f = 0; f < franjas; f++) {
        int ini  = rand() % H;
        int alto = 2 + rand() % 10;
        int desp = (rand() % 9) - 4;
        for (int y = ini; y < ini + alto && y < H; y++)
            glitch_offset[y] = desp;
    }
}

/* ── Raycast una columna y pinta en el fb ────────────────────────── */
static void raycast_columna(int x, Jugador *p) {
    double ang = p->angulo - FOV / 2.0 + (FOV * x) / (double)W;
    double rx  = cos(ang);
    double ry  = sin(ang);

    /* celda inicial del jugador */
    int mapX = (int)p->x;
    int mapY = (int)p->y;

    /* distancia que recorre el rayo para cruzar una celda completa */
    double deltaX = fabs(rx) < 1e-10 ? 1e30 : fabs(1.0 / rx);
    double deltaY = fabs(ry) < 1e-10 ? 1e30 : fabs(1.0 / ry);

    /* distancia hasta el primer borde de celda + direccion de paso */
    double sideX, sideY;
    int stepX, stepY;

    if (rx < 0) { stepX = -1; sideX = (p->x - mapX) * deltaX; }
    else        { stepX =  1; sideX = (mapX + 1.0 - p->x) * deltaX; }
    if (ry < 0) { stepY = -1; sideY = (p->y - mapY) * deltaY; }
    else        { stepY =  1; sideY = (mapY + 1.0 - p->y) * deltaY; }

    /* DDA: salta de celda en celda hasta encontrar pared */
    int side = 0;   /* 0 = golpeo pared vertical (X), 1 = horizontal (Y) */
    int hit  = 0;
    while (!hit) {
        if (sideX < sideY) { sideX += deltaX; mapX += stepX; side = 0; }
        else               { sideY += deltaY; mapY += stepY; side = 1; }
        if (mapX < 0 || mapX >= MAPA_W || mapY < 0 || mapY >= MAPA_H) return;
        if (mapa[mapY][mapX] == 1) hit = 1;
    }

    /* distancia perpendicular exacta — sin fish-eye */
    double dc = (side == 0) ? (sideX - deltaX) : (sideY - deltaY);
    if (dc < 0.001) dc = 0.001;
    zbuf[x] = dc;   /* guardar para ocluir sprites */

    /* altura de pared en pantalla */
    int hp   = (int)((double)H / dc);
    int yini = H / 2 - hp / 2;
    int yfin = H / 2 + hp / 2;

    /* coordenada X exacta en la textura segun el punto de impacto */
    double wallX;
    if (side == 0) wallX = p->y + dc * ry;
    else           wallX = p->x + dc * rx;
    wallX -= floor(wallX);

    int tx = (int)(wallX * TEX_W);
    if (side == 0 && rx > 0) tx = TEX_W - tx - 1;
    if (side == 1 && ry < 0) tx = TEX_W - tx - 1;
    tx &= (TEX_W - 1);

    /* visibilidad: radio de la antorcha, despues negro */
    double fog = 1.0 - (dc / torch_radio);
    if (fog < 0.0)  fog = 0.0;
    if (fog > 0.88) fog = 0.88;

    /* calidez: cuanto mas cerca mas naranja */
    double warm = fog * fog;

    /* paredes Y un poco mas oscuras para dar sensacion de profundidad */
    if (side == 1) fog *= 0.75;

    int gy = glitch_offset[x % H];

    for (int y = yini; y < yfin; y++) {
        if (y < 0 || y >= H) continue;

        /* ty: mapeo exacto de textura con precision entera */
        int ty = (int)((double)(y - yini) * TEX_H / (double)(yfin - yini)) & (TEX_H - 1);

        double r = tex[ty][tx][0] * fog * (1.0 + warm * 1.1);
        double g = tex[ty][tx][1] * fog * (1.0 + warm * 0.4);
        double b = tex[ty][tx][2] * fog * (1.0 - warm * 0.6);

        Uint8 rr = (Uint8)(r > 255 ? 255 : r);
        Uint8 gg = (Uint8)(g > 255 ? 255 : g);
        Uint8 bb = (Uint8)(b < 0   ?   0 : b);

        fb_set(x, y + gy, rr, gg, bb);
    }
}

/* ── Renderizar sprite billboard (objeto fijo en el mapa) ───────── */
static void dibujar_sprite(Jugador *p, double sx, double sy) {
    double dx   = sx - p->x;
    double dy   = sy - p->y;
    double dist = sqrt(dx*dx + dy*dy);
    if (dist < 0.2) return;

    double ang_spr  = atan2(dy, dx);
    double ang_diff = ang_spr - p->angulo;
    while (ang_diff >  M_PI) ang_diff -= 2.0 * M_PI;
    while (ang_diff < -M_PI) ang_diff += 2.0 * M_PI;
    if (fabs(ang_diff) > FOV * 0.75) return;

    int scr_cx = (int)((0.5 + ang_diff / FOV) * W);
    int sh = (int)(H / dist);
    int sw = sh;
    int y0 = H / 2 - sh / 2;
    int x0 = scr_cx - sw / 2;

    double fog = 1.0 - (dist / torch_radio);
    if (fog < 0.0) fog = 0.0;
    if (fog > 0.9) fog = 0.9;
    double warm = fog * fog;

    for (int col = 0; col < sw; col++) {
        int scr_x = x0 + col;
        if (scr_x < 0 || scr_x >= W) continue;
        if (dist >= zbuf[scr_x]) continue;
        int tx = col * SPR_W / sw;
        for (int row = 0; row < sh; row++) {
            int scr_y = y0 + row;
            if (scr_y < 0 || scr_y >= H) continue;
            int ty = row * SPR_H / sh;
            if (spr[ty][tx][3] == 0) continue;
            double r = spr[ty][tx][0] * fog * (1.0 + warm * 0.8);
            double g = spr[ty][tx][1] * fog * (1.0 + warm * 0.3);
            double b = spr[ty][tx][2] * fog * (1.0 - warm * 0.4);
            fb_set(scr_x, scr_y,
                   (Uint8)(r > 255 ? 255 : r),
                   (Uint8)(g > 255 ? 255 : g),
                   (Uint8)(b <   0 ?   0 : b));
        }
    }
}

/* ── HUD: llama de antorcha en el centro-inferior ───────────────── */
static void dibujar_antorcha_hud(int caminando) {
    /* bob: oscila cuando caminas */
    if (caminando) bob_fase += 0.12;
    else           bob_fase *= 0.85;   /* se amortigua al parar */

    bob_y = sin(bob_fase)       * 10.0;
    bob_x = sin(bob_fase * 0.5) *  5.0;

    /* posicion base: centro-derecha inferior, como si la sostuvieras */
    int cx = (int)(W / 2 + 90 + bob_x);
    int cy = (int)(H + 20 + bob_y);      /* empieza por debajo del borde */

    int flick = (int)((torch_radio - TORCH_BASE) * 35);

    /* ── palo del mango: baja desde la llama hasta fuera de pantalla ── */
    for (int dy = 0; dy < 220; dy++) {
        int grosor = (dy < 30) ? 6 : 5;
        for (int dx = -grosor; dx <= grosor; dx++) {
            /* variacion de madera: vetas horizontales */
            Uint8 veta = (dy % 7 < 2) ? 15 : 0;
            fb_set(cx + dx, cy - dy,
                   60 + veta + (dx & 1) * 5,
                   32 + veta,
                   12);
        }
    }

    /* ── trapo/estopa en el extremo: base de la llama ── */
    for (int dy = 0; dy < 18; dy++) {
        int grosor = 9 - dy / 3;
        for (int dx = -grosor; dx <= grosor; dx++) {
            fb_set(cx + dx, cy - 215 - dy,
                   80 + rand()%20, 45 + rand()%15, 10);
        }
    }

    /* ── llama exterior: rojo-naranja ── */
    for (int dy = 0; dy < 44 + flick; dy++) {
        int ancho = 13 - dy / 4;
        if (ancho < 1) ancho = 1;
        for (int dx = -ancho; dx <= ancho; dx++) {
            int nx = cx + dx + (rand() % 5 - 2);
            int ny = cy - 228 - dy + (rand() % 3);
            fb_set(nx, ny, 180 + rand()%60, 30 + rand()%25, 0);
        }
    }

    /* ── llama media: naranja ── */
    for (int dy = 0; dy < 32 + flick; dy++) {
        int ancho = 9 - dy / 4;
        if (ancho < 1) ancho = 1;
        for (int dx = -ancho; dx <= ancho; dx++) {
            int nx = cx + dx + (rand() % 3 - 1);
            int ny = cy - 229 - dy + (rand() % 2);
            fb_set(nx, ny, 235 + rand()%20, 110 + rand()%50, 0);
        }
    }

    /* ── nucleo: amarillo brillante ── */
    for (int dy = 0; dy < 18 + flick; dy++) {
        int ancho = 4 - dy / 5;
        if (ancho < 1) ancho = 1;
        for (int dx = -ancho; dx <= ancho; dx++) {
            fb_set(cx + dx, cy - 230 - dy,
                   255, 225 + rand()%30, 90 + rand()%80);
        }
    }
}

/* ── Scanlines en el fb ──────────────────────────────────────────── */
static void aplicar_scanlines(void) {
    for (int y = 0; y < H; y += 2) {
        for (int x = 0; x < W; x++) {
            Uint32 p = fb[y * W + x];
            Uint8 r  = (p >> 16) & 0xFF;
            Uint8 g  = (p >>  8) & 0xFF;
            Uint8 bl = (p      ) & 0xFF;
            /* oscurecer la fila impar */
            fb[y * W + x] = 0xFF000000
                | ((Uint32)(r / 2) << 16)
                | ((Uint32)(g / 2) <<  8)
                | (bl / 2);
        }
    }
}

/* ── Noise en el fb ──────────────────────────────────────────────── */
static void aplicar_noise(void) {
    int cant = 500 + rand() % 300;
    for (int i = 0; i < cant; i++) {
        int nx = rand() % W;
        int ny = rand() % H;
        Uint8 v = rand() % 60;
        Uint32 p = fb[ny * W + nx];
        Uint8 r  = (Uint8)(((p >> 16) & 0xFF) + v > 255 ? 255 : ((p >> 16) & 0xFF) + v);
        Uint8 g  = (Uint8)(((p >>  8) & 0xFF) + v > 255 ? 255 : ((p >>  8) & 0xFF) + v);
        Uint8 bl = (Uint8)(((p      ) & 0xFF) + v > 255 ? 255 : ((p      ) & 0xFF) + v);
        fb[ny * W + nx] = 0xFF000000 | ((Uint32)r << 16) | ((Uint32)g << 8) | bl;
    }
}

/* ── Main ────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    srand((unsigned)time(NULL));

    /* forzar warp-based relative mouse — necesario en WSL2/WSLg */
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window   *win = SDL_CreateWindow(
        "screenPJ - mazmorra corrupta",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W, H,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALWAYS_ON_TOP);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    /* textura SDL para el framebuffer */
    SDL_Texture *screen = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        W, H);

    generar_textura();
    generar_sprite_terminal();

    Jugador p = { 3.5, 3.5, 0.0 };

    /* traer ventana al frente y darle foco */
    SDL_RaiseWindow(win);
    SDL_SetWindowInputFocus(win);
    SDL_Delay(50);

    /* cursor invisible: creamos uno de 1x1 completamente transparente */
    Uint8 cdata[1] = {0};
    Uint8 cmask[1] = {0};
    SDL_Cursor *cursor_vacio = SDL_CreateCursor(cdata, cmask, 8, 1, 0, 0);
    SDL_SetCursor(cursor_vacio);
    SDL_ShowCursor(SDL_DISABLE);

    /* capturar mouse */
    SDL_SetWindowGrab(win, SDL_TRUE);
    SDL_CaptureMouse(SDL_TRUE);
    SDL_WarpMouseInWindow(win, W / 2, H / 2);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    const Uint8 *keys;
    int corriendo = 1;
    SDL_Event ev;

    while (corriendo) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) corriendo = 0;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
                corriendo = 0;
            if (ev.type == SDL_MOUSEMOTION)
                p.angulo += ev.motion.xrel * MOUSE_SENS;
        }
        /* warp cada frame — respaldo para WSL2 donde el confinamiento falla */
        SDL_WarpMouseInWindow(win, W / 2, H / 2);

        keys = SDL_GetKeyboardState(NULL);

        /* vector adelante y vector lateral (perpendicular) */
        double fdx = cos(p.angulo) * MOVE_VEL;
        double fdy = sin(p.angulo) * MOVE_VEL;
        double sdx = cos(p.angulo + M_PI / 2.0) * MOVE_VEL;
        double sdy = sin(p.angulo + M_PI / 2.0) * MOVE_VEL;

        /* W/S — caminar adelante/atras */
        if (keys[SDL_SCANCODE_W]) {
            double nx = p.x + fdx, ny = p.y + fdy;
            if (mapa[(int)ny][(int)nx] == 0) { p.x = nx; p.y = ny; }
        }
        if (keys[SDL_SCANCODE_S]) {
            double nx = p.x - fdx, ny = p.y - fdy;
            if (mapa[(int)ny][(int)nx] == 0) { p.x = nx; p.y = ny; }
        }

        /* A/D — strafe izquierda/derecha */
        if (keys[SDL_SCANCODE_A]) {
            double nx = p.x - sdx, ny = p.y - sdy;
            if (mapa[(int)ny][(int)nx] == 0) { p.x = nx; p.y = ny; }
        }
        if (keys[SDL_SCANCODE_D]) {
            double nx = p.x + sdx, ny = p.y + sdy;
            if (mapa[(int)ny][(int)nx] == 0) { p.x = nx; p.y = ny; }
        }

        /* flechas — rotar (alternativa al mouse) */
        if (keys[SDL_SCANCODE_LEFT])  p.angulo -= ROT_VEL;
        if (keys[SDL_SCANCODE_RIGHT]) p.angulo += ROT_VEL;

        /* ── limpiar framebuffer ── */
        /* cielo: negro profundo */
        for (int i = 0; i < W * (H / 2); i++)
            fb[i] = 0xFF05000A;
        /* piso: rojo muy oscuro */
        for (int i = W * (H / 2); i < W * H; i++)
            fb[i] = 0xFF0F0303;

        /* ── paredes ── */
        actualizar_antorcha();
        actualizar_glitch();
        for (int x = 0; x < W; x++)
            raycast_columna(x, &p);

        /* ── terminal: objeto fijo en esquina del mapa (1.5, 6.5) ── */
        dibujar_sprite(&p, 1.5, 6.5);

        /* ── hud antorcha ── */
        int caminando = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S]
                     || keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D];
        dibujar_antorcha_hud(caminando);

        /* ── crosshair: punto fijo en el centro ── */
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++)
                if (dx == 0 || dy == 0)
                    fb_set(W/2 + dx, H/2 + dy, 255, 255, 255);

        /* ── post efectos ── */
        aplicar_scanlines();
        aplicar_noise();

        /* ── subir fb a GPU y presentar ── */
        SDL_UpdateTexture(screen, NULL, fb, W * sizeof(Uint32));
        SDL_RenderCopy(ren, screen, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_FreeCursor(cursor_vacio);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_DestroyTexture(screen);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
