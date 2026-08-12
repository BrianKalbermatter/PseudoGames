#include "screenPJ.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Dimensiones internas del raycaster ─────────────────────────── */
#define PJ_W        800
#define PJ_H        600
#define PJ_FOV      (M_PI / 3.0)
#define PJ_MAPA_W   8
#define PJ_MAPA_H   8
#define PJ_MOVE     0.04
#define PJ_ROT      0.03
#define PJ_MSENS    0.0008
#define PJ_TEX_W    32
#define PJ_TEX_H    32
#define PJ_SPR_W    64
#define PJ_SPR_H    64

/* posicion fija de la terminal en el mapa */
#define TERM_X  1.5
#define TERM_Y  6.5
#define TERM_TRIGGER  1.4   /* distancia para activar */

/* antorcha */
#define TORCH_BASE  3.8
#define TORCH_FLICK 0.22

/* ── Mapa ────────────────────────────────────────────────────────── */
static int pj_mapa[PJ_MAPA_H][PJ_MAPA_W] = {
    {1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,0,1,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1},
};

typedef struct { double x, y, ang; } PJ_Jugador;

/* ── Buffers ─────────────────────────────────────────────────────── */
static Uint32 pj_fb[PJ_H * PJ_W];
static double pj_zbuf[PJ_W];

static void pj_set(int x, int y, Uint8 r, Uint8 g, Uint8 b) {
    if ((unsigned)x >= PJ_W || (unsigned)y >= PJ_H) return;
    pj_fb[y * PJ_W + x] = 0xFF000000 | ((Uint32)r<<16) | ((Uint32)g<<8) | b;
}

/* ── Textura adoquin ─────────────────────────────────────────────── */
static Uint8 pj_tex[PJ_TEX_H][PJ_TEX_W][3];

static void pj_gen_textura(void) {
    int bw = 14, bh = 8;
    for (int y = 0; y < PJ_TEX_H; y++) {
        for (int x = 0; x < PJ_TEX_W; x++) {
            int row = y / bh;
            int bx  = (x + (row%2)*(bw/2)) % bw;
            int by  = y % bh;
            if (by <= 1 || bx <= 1) {
                pj_tex[y][x][0] = 10 + rand()%8;
                pj_tex[y][x][1] = 10 + rand()%8;
                pj_tex[y][x][2] = 14 + rand()%8;
            } else {
                int base = 45 + rand()%30;
                pj_tex[y][x][0] = (Uint8)(base * 0.85);
                pj_tex[y][x][1] = (Uint8)(base * 0.80);
                pj_tex[y][x][2] = (Uint8)(base);
                if (rand()%15 == 0) {
                    pj_tex[y][x][0] /= 3;
                    pj_tex[y][x][1] /= 3;
                    pj_tex[y][x][2] /= 3;
                }
            }
        }
    }
}

/* ── Sprite terminal ─────────────────────────────────────────────── */
static Uint8 pj_spr[PJ_SPR_H][PJ_SPR_W][4];

static void pj_gen_terminal(void) {
    memset(pj_spr, 0, sizeof(pj_spr));
    /* mesa */
    for (int y = 46; y < PJ_SPR_H; y++)
        for (int x = 0; x < PJ_SPR_W; x++) {
            Uint8 v = 42 + (y%4<1?10:0) + rand()%8;
            pj_spr[y][x][0]=v; pj_spr[y][x][1]=(Uint8)(v*0.55);
            pj_spr[y][x][2]=8; pj_spr[y][x][3]=255;
        }
    /* cuerpo monitor */
    for (int y = 8; y < 48; y++)
        for (int x = 8; x < 56; x++) {
            pj_spr[y][x][0]=160+rand()%10; pj_spr[y][x][1]=155+rand()%10;
            pj_spr[y][x][2]=130+rand()%10; pj_spr[y][x][3]=255;
        }
    /* bisel */
    for (int i = 8; i < 56; i++) {
        pj_spr[8][i][0]=pj_spr[8][i][1]=pj_spr[8][i][2]=90; pj_spr[8][i][3]=255;
        pj_spr[47][i][0]=pj_spr[47][i][1]=pj_spr[47][i][2]=90; pj_spr[47][i][3]=255;
        pj_spr[i][8][0]=pj_spr[i][8][1]=pj_spr[i][8][2]=90; pj_spr[i][8][3]=255;
        pj_spr[i][55][0]=pj_spr[i][55][1]=pj_spr[i][55][2]=90; pj_spr[i][55][3]=255;
    }
    /* pantalla */
    for (int y = 12; y < 43; y++)
        for (int x = 12; x < 52; x++) {
            pj_spr[y][x][0]=2; pj_spr[y][x][1]=18+rand()%6;
            pj_spr[y][x][2]=5; pj_spr[y][x][3]=255;
        }
    /* texto verde */
    int lineas[] = {14,19,24,29,34};
    for (int l = 0; l < 5; l++) {
        int largo = 18 + rand()%16;
        for (int x = 14; x < 14+largo && x < 50; x++) {
            if (rand()%5==0) continue;
            pj_spr[lineas[l]][x][0]=10;
            pj_spr[lineas[l]][x][1]=200+rand()%55;
            pj_spr[lineas[l]][x][2]=30;
            pj_spr[lineas[l]][x][3]=255;
        }
    }
    /* cursor */
    for (int x = 14; x < 19; x++) {
        pj_spr[39][x][0]=20; pj_spr[39][x][1]=220;
        pj_spr[39][x][2]=40; pj_spr[39][x][3]=255;
    }
    /* pedestal */
    for (int y = 43; y < 47; y++)
        for (int x = 24; x < 40; x++) {
            pj_spr[y][x][0]=100; pj_spr[y][x][1]=95;
            pj_spr[y][x][2]=80;  pj_spr[y][x][3]=255;
        }
    /* teclado */
    for (int y = 49; y < 55; y++)
        for (int x = 10; x < 54; x++) {
            Uint8 v=120+rand()%15;
            pj_spr[y][x][0]=v; pj_spr[y][x][1]=(Uint8)(v*0.95);
            pj_spr[y][x][2]=(Uint8)(v*0.80); pj_spr[y][x][3]=255;
        }
    for (int x = 12; x < 52; x += 4)
        for (int y = 50; y < 54; y++) {
            pj_spr[y][x][0]=60; pj_spr[y][x][1]=58;
            pj_spr[y][x][2]=50; pj_spr[y][x][3]=255;
        }
}

/* ── Antorcha ────────────────────────────────────────────────────── */
static double pj_torch = TORCH_BASE;
static void pj_antorcha_tick(void) {
    double t = TORCH_BASE + ((rand()%100)/100.0 - 0.5)*2.0*TORCH_FLICK;
    pj_torch = pj_torch*0.85 + t*0.15;
}

/* ── Glitch ──────────────────────────────────────────────────────── */
static int pj_glitch[PJ_H];
static int pj_gtimer = 0;
static void pj_glitch_tick(void) {
    pj_gtimer++;
    if (pj_gtimer % 10 != 0) return;
    for (int y = 0; y < PJ_H; y++) pj_glitch[y] = 0;
    for (int f = 0; f < 2+rand()%3; f++) {
        int ini=rand()%PJ_H, alto=2+rand()%10, desp=(rand()%9)-4;
        for (int y=ini; y<ini+alto && y<PJ_H; y++) pj_glitch[y]=desp;
    }
}

/* ── Raycast columna ─────────────────────────────────────────────── */
static void pj_raycast(int x, PJ_Jugador *p) {
    double ang = p->ang - PJ_FOV/2.0 + (PJ_FOV*x)/(double)PJ_W;
    double rx=cos(ang), ry=sin(ang);
    int mx=(int)p->x, my=(int)p->y;
    double dX=fabs(rx)<1e-10?1e30:fabs(1.0/rx);
    double dY=fabs(ry)<1e-10?1e30:fabs(1.0/ry);
    double sX,sY; int stX,stY,side=0,hit=0;
    if(rx<0){stX=-1;sX=(p->x-mx)*dX;}else{stX=1;sX=(mx+1.0-p->x)*dX;}
    if(ry<0){stY=-1;sY=(p->y-my)*dY;}else{stY=1;sY=(my+1.0-p->y)*dY;}
    while(!hit){
        if(sX<sY){sX+=dX;mx+=stX;side=0;}else{sY+=dY;my+=stY;side=1;}
        if(mx<0||mx>=PJ_MAPA_W||my<0||my>=PJ_MAPA_H)return;
        if(pj_mapa[my][mx]==1)hit=1;
    }
    double dc=(side==0)?(sX-dX):(sY-dY);
    if(dc<0.001)dc=0.001;
    pj_zbuf[x]=dc;
    int hp=(int)(PJ_H/dc), yini=PJ_H/2-hp/2, yfin=PJ_H/2+hp/2;
    double wX=(side==0)?p->y+dc*ry:p->x+dc*rx;
    wX-=floor(wX);
    int tx=(int)(wX*PJ_TEX_W);
    if(side==0&&rx>0)tx=PJ_TEX_W-tx-1;
    if(side==1&&ry<0)tx=PJ_TEX_W-tx-1;
    tx&=(PJ_TEX_W-1);
    double fog=1.0-(dc/pj_torch);
    if(fog<0.0)fog=0.0; if(fog>0.88)fog=0.88;
    double warm=fog*fog;
    if(side==1)fog*=0.75;
    int gy=pj_glitch[x%PJ_H];
    for(int y=yini;y<yfin;y++){
        if(y<0||y>=PJ_H)continue;
        int ty=(int)((double)(y-yini)*PJ_TEX_H/(double)(yfin-yini))&(PJ_TEX_H-1);
        double r=pj_tex[ty][tx][0]*fog*(1.0+warm*1.1);
        double g=pj_tex[ty][tx][1]*fog*(1.0+warm*0.4);
        double b=pj_tex[ty][tx][2]*fog*(1.0-warm*0.6);
        pj_set(x,y+gy,(Uint8)(r>255?255:r),(Uint8)(g>255?255:g),(Uint8)(b<0?0:b));
    }
}

/* ── Sprite ──────────────────────────────────────────────────────── */
static void pj_sprite(PJ_Jugador *p, double sx, double sy) {
    double dx=sx-p->x, dy=sy-p->y, dist=sqrt(dx*dx+dy*dy);
    if(dist<0.2)return;
    double as=atan2(dy,dx), ad=as-p->ang;
    while(ad> M_PI)ad-=2.0*M_PI;
    while(ad<-M_PI)ad+=2.0*M_PI;
    if(fabs(ad)>PJ_FOV*0.75)return;
    int cx=(int)((0.5+ad/PJ_FOV)*PJ_W);
    int sh=(int)(PJ_H/dist), sw=sh;
    int y0=PJ_H/2-sh/2, x0=cx-sw/2;
    double fog=1.0-(dist/pj_torch);
    if(fog<0.0)fog=0.0; if(fog>0.9)fog=0.9;
    double warm=fog*fog;
    for(int col=0;col<sw;col++){
        int sx2=x0+col;
        if(sx2<0||sx2>=PJ_W)continue;
        if(dist>=pj_zbuf[sx2])continue;
        int tx=col*PJ_SPR_W/sw;
        for(int row=0;row<sh;row++){
            int sy2=y0+row;
            if(sy2<0||sy2>=PJ_H)continue;
            int ty=row*PJ_SPR_H/sh;
            if(pj_spr[ty][tx][3]==0)continue;
            double r=pj_spr[ty][tx][0]*fog*(1.0+warm*0.8);
            double g=pj_spr[ty][tx][1]*fog*(1.0+warm*0.3);
            double b=pj_spr[ty][tx][2]*fog*(1.0-warm*0.4);
            pj_set(sx2,sy2,(Uint8)(r>255?255:r),(Uint8)(g>255?255:g),(Uint8)(b<0?0:b));
        }
    }
}

/* ── Antorcha HUD ────────────────────────────────────────────────── */
static double pj_bob_fase=0.0, pj_bob_y=0.0, pj_bob_x=0.0;

static void pj_hud_antorcha(int caminando) {
    if(caminando) pj_bob_fase+=0.12; else pj_bob_fase*=0.85;
    pj_bob_y=sin(pj_bob_fase)*10.0;
    pj_bob_x=sin(pj_bob_fase*0.5)*5.0;
    int cx=(int)(PJ_W/2+90+pj_bob_x);
    int cy=(int)(PJ_H+20+pj_bob_y);
    int flick=(int)((pj_torch-TORCH_BASE)*35);
    for(int dy=0;dy<220;dy++){
        int gr=(dy<30)?6:5;
        for(int dx=-gr;dx<=gr;dx++){
            Uint8 veta=(dy%7<2)?15:0;
            pj_set(cx+dx,cy-dy,60+veta+(dx&1)*5,32+veta,12);
        }
    }
    for(int dy=0;dy<18;dy++){
        int gr=9-dy/3;
        for(int dx=-gr;dx<=gr;dx++)
            pj_set(cx+dx,cy-215-dy,80+rand()%20,45+rand()%15,10);
    }
    for(int dy=0;dy<44+flick;dy++){
        int aw=13-dy/4; if(aw<1)aw=1;
        for(int dx=-aw;dx<=aw;dx++)
            pj_set(cx+dx+(rand()%5-2),cy-228-dy+(rand()%3),180+rand()%60,30+rand()%25,0);
    }
    for(int dy=0;dy<32+flick;dy++){
        int aw=9-dy/4; if(aw<1)aw=1;
        for(int dx=-aw;dx<=aw;dx++)
            pj_set(cx+dx+(rand()%3-1),cy-229-dy+(rand()%2),235+rand()%20,110+rand()%50,0);
    }
    for(int dy=0;dy<18+flick;dy++){
        int aw=4-dy/5; if(aw<1)aw=1;
        for(int dx=-aw;dx<=aw;dx++)
            pj_set(cx+dx,cy-230-dy,255,225+rand()%30,90+rand()%80);
    }
}

/* ── Post efectos ────────────────────────────────────────────────── */
static void pj_scanlines(void) {
    for(int y=0;y<PJ_H;y+=2)
        for(int x=0;x<PJ_W;x++){
            Uint32 p=pj_fb[y*PJ_W+x];
            pj_fb[y*PJ_W+x]=0xFF000000
                |((Uint32)(((p>>16)&0xFF)/2)<<16)
                |((Uint32)(((p>>8)&0xFF)/2)<<8)
                |(((p)&0xFF)/2);
        }
}
static void pj_noise(void) {
    for(int i=0;i<600+rand()%300;i++){
        int nx=rand()%PJ_W, ny=rand()%PJ_H;
        Uint8 v=rand()%60;
        Uint32 p=pj_fb[ny*PJ_W+nx];
        Uint8 r=(Uint8)(((p>>16)&0xFF)+v>255?255:((p>>16)&0xFF)+v);
        Uint8 g=(Uint8)(((p>>8)&0xFF)+v>255?255:((p>>8)&0xFF)+v);
        Uint8 b=(Uint8)(((p)&0xFF)+v>255?255:((p)&0xFF)+v);
        pj_fb[ny*PJ_W+nx]=0xFF000000|((Uint32)r<<16)|((Uint32)g<<8)|b;
    }
}

/* ── Función principal de la intro ──────────────────────────────── */
void screenPJ_intro(SDL_Renderer *renderer, SDL_Window *ventana, int ancho, int alto) {
    pj_gen_textura();
    pj_gen_terminal();

    SDL_Texture *screen = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, PJ_W, PJ_H);

    /* ── notificacion "UsuarioAnonimo Conectado" ── */
    TTF_Font *fnt = TTF_OpenFont("assets/fonts/main.ttf", 14);
    SDL_Texture *notif_tex = NULL;
    SDL_Rect notif_rect = {0,0,0,0};
    int notif_frames = 120;   /* 2 seg a 60fps */
    if (fnt) {
        SDL_Color verde = {80, 220, 80, 255};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(fnt, "● UsuarioAnonimo Conectado", verde);
        if (surf) {
            notif_tex = SDL_CreateTextureFromSurface(renderer, surf);
            notif_rect.w = surf->w;
            notif_rect.h = surf->h;
            notif_rect.x = 18;
            notif_rect.y = alto - surf->h - 14;
            SDL_FreeSurface(surf);
        }
        TTF_CloseFont(fnt);
    }

    /* mouse */
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
    Uint8 cdata[1]={0}, cmask[1]={0};
    SDL_Cursor *cur = SDL_CreateCursor(cdata, cmask, 8, 1, 0, 0);
    SDL_SetCursor(cur);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(ventana, SDL_TRUE);
    SDL_CaptureMouse(SDL_TRUE);
    SDL_WarpMouseInWindow(ventana, ancho/2, alto/2);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    PJ_Jugador p = { 3.5, 3.5, 0.0 };
    SDL_Event ev;
    const Uint8 *keys;
    int corriendo = 1;
    int fade = 0;          /* 0 = jugando, 1..255 = fade a negro */
    int triggered = 0;

    /* rect destino: escala el buffer 800x600 a la ventana real */
    SDL_Rect dest = { 0, 0, ancho, alto };

    while (corriendo) {
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { corriendo = 0; fade = 255; }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE)
                corriendo = 0;
            if (ev.type == SDL_MOUSEMOTION && !triggered)
                p.ang += ev.motion.xrel * PJ_MSENS;
        }
        SDL_WarpMouseInWindow(ventana, ancho/2, alto/2);

        keys = SDL_GetKeyboardState(NULL);

        if (!triggered) {
            double fdx=cos(p.ang)*PJ_MOVE, fdy=sin(p.ang)*PJ_MOVE;
            double sdx=cos(p.ang+M_PI/2.0)*PJ_MOVE, sdy=sin(p.ang+M_PI/2.0)*PJ_MOVE;
            if(keys[SDL_SCANCODE_W]){double nx=p.x+fdx,ny=p.y+fdy;if(pj_mapa[(int)ny][(int)nx]==0){p.x=nx;p.y=ny;}}
            if(keys[SDL_SCANCODE_S]){double nx=p.x-fdx,ny=p.y-fdy;if(pj_mapa[(int)ny][(int)nx]==0){p.x=nx;p.y=ny;}}
            if(keys[SDL_SCANCODE_A]){double nx=p.x-sdx,ny=p.y-sdy;if(pj_mapa[(int)ny][(int)nx]==0){p.x=nx;p.y=ny;}}
            if(keys[SDL_SCANCODE_D]){double nx=p.x+sdx,ny=p.y+sdy;if(pj_mapa[(int)ny][(int)nx]==0){p.x=nx;p.y=ny;}}
            if(keys[SDL_SCANCODE_LEFT]) p.ang-=PJ_ROT;
            if(keys[SDL_SCANCODE_RIGHT])p.ang+=PJ_ROT;

            /* detectar proximidad a la terminal */
            double dx=p.x-TERM_X, dy2=p.y-TERM_Y;
            if(sqrt(dx*dx+dy2*dy2) < TERM_TRIGGER) triggered = 1;
        }

        /* fade a negro al acercarse */
        if (triggered) {
            fade += 6;
            if (fade >= 255) { fade = 255; corriendo = 0; }
        }

        /* ── render ── */
        for(int i=0;i<PJ_W*(PJ_H/2);i++) pj_fb[i]=0xFF05000A;
        for(int i=PJ_W*(PJ_H/2);i<PJ_W*PJ_H;i++) pj_fb[i]=0xFF0F0303;

        pj_antorcha_tick();
        pj_glitch_tick();
        for(int x=0;x<PJ_W;x++) pj_raycast(x,&p);
        pj_sprite(&p, TERM_X, TERM_Y);

        int cam = keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_S]
                ||keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_D];
        pj_hud_antorcha(cam);

        /* crosshair */
        for(int dy=-4;dy<=4;dy++)
            for(int dx=-4;dx<=4;dx++)
                if(dx==0||dy==0) pj_set(PJ_W/2+dx,PJ_H/2+dy,255,255,255);

        pj_scanlines();
        pj_noise();

        SDL_UpdateTexture(screen, NULL, pj_fb, PJ_W * sizeof(Uint32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, screen, NULL, &dest);

        /* ── notificacion servidor ── */
        if (notif_tex && notif_frames > 0) {
            /* fadeout en los ultimos 30 frames */
            Uint8 alpha = (notif_frames < 30) ? (Uint8)(notif_frames * 8) : 220;
            SDL_SetTextureAlphaMod(notif_tex, alpha);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            /* fondo semitransparente detras del texto */
            SDL_Rect fondo = { notif_rect.x - 8, notif_rect.y - 4,
                               notif_rect.w + 16, notif_rect.h + 8 };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(alpha * 0.6));
            SDL_RenderFillRect(renderer, &fondo);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_RenderCopy(renderer, notif_tex, NULL, &notif_rect);
            notif_frames--;
        }

        /* overlay negro del fade */
        if (fade > 0) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)fade);
            SDL_RenderFillRect(renderer, NULL);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    /* liberar mouse */
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_SetWindowGrab(ventana, SDL_FALSE);
    SDL_ShowCursor(SDL_ENABLE);
    SDL_FreeCursor(cur);
    if (notif_tex) SDL_DestroyTexture(notif_tex);
    SDL_DestroyTexture(screen);
}
