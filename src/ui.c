#include "ui.h"
#include "niveles.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

static SDL_Window *g_ventana = NULL;

void ui_set_ventana(SDL_Window *v) { g_ventana = v; }

SDL_MouseButtonFlags
ui_mouse(int *x, int *y)
{
    float fx = 0.0f, fy = 0.0f;
    SDL_MouseButtonFlags botones = SDL_GetMouseState(&fx, &fy);
    if (x) *x = (int)fx;
    if (y) *y = (int)fy;
    return botones;
}

void
ui_clip(SDL_Renderer *r, const SDL_FRect *area)
{
    if (!area) { SDL_SetRenderClipRect(r, NULL); return; }
    SDL_Rect ir = { (int)area->x, (int)area->y, (int)area->w, (int)area->h };
    SDL_SetRenderClipRect(r, &ir);
}

void
ui_viewport(SDL_Renderer *r, const SDL_FRect *area)
{
    if (!area) { SDL_SetRenderViewport(r, NULL); return; }
    SDL_Rect ir = { (int)area->x, (int)area->y, (int)area->w, (int)area->h };
    SDL_SetRenderViewport(r, &ir);
}

void
ui_text_input(bool on)
{
    if (!g_ventana) return;
    if (on) SDL_StartTextInput(g_ventana);
    else    SDL_StopTextInput(g_ventana);
}

void
presente(SDL_Renderer *renderer)
{
    SDL_RenderPresent(renderer);
    if (g_ventana)
        SDL_UpdateWindowSurface(g_ventana);
}

/* ── Rellena un cuadrilatero convexo con scanlines ── */
static void
fill_quad(SDL_Renderer *r, SDL_Point p[4])
{
    int miny = p[0].y, maxy = p[0].y;
    for (int k = 1; k < 4; k++) {
        if (p[k].y < miny) miny = p[k].y;
        if (p[k].y > maxy) maxy = p[k].y;
    }
    for (int y = miny; y <= maxy; y++) {
        int xmin = 99999, xmax = -99999;
        for (int k = 0; k < 4; k++) {
            int j  = (k + 1) % 4;
            int y0 = p[k].y, y1 = p[j].y;
            int x0 = p[k].x, x1 = p[j].x;
            if ((y0 <= y && y < y1) || (y1 <= y && y < y0)) {
                int x = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
                if (x < xmin) xmin = x;
                if (x > xmax) xmax = x;
            }
        }
        if (xmin <= xmax)
            SDL_RenderLine(r, xmin, y, xmax, y);
    }
}

/*
 * dibujarArandela — dibuja un engranaje centrado en (cx, cy).
 *   radio   : radio exterior (punta de los dientes)
 *   color   : color del metal
 *   bg      : color del agujero central (igual al fondo donde se dibuja)
 */
void
dibujarArandela(SDL_Renderer *renderer, int cx, int cy, int radio,
                SDL_Color color, SDL_Color bg)
{
    float r_ext  = (float)radio;
    float r_body = r_ext * 0.64f;   /* cuerpo principal */
    float r_hub  = r_ext * 0.22f;   /* agujero central  */
    int   n      = 8;               /* numero de dientes */
    float tooth  = 0.25f;           /* semi-ancho angular de cada diente */

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    /* 1. Cuerpo circular relleno */
    int rb = (int)r_body;
    for (int y = -rb; y <= rb; y++) {
        int xw = (int)sqrtf((float)(rb * rb - y * y));
        SDL_RenderLine(renderer, cx - xw, cy + y, cx + xw, cy + y);
    }

    /* 2. Dientes: trapecios proyectados hacia afuera */
    for (int i = 0; i < n; i++) {
        float ang = (float)i * 2.0f * (float)M_PI / (float)n;
        float a1  = ang - tooth;
        float a2  = ang + tooth;
        SDL_Point pts[4] = {
            { cx + (int)(r_body * cosf(a1)), cy + (int)(r_body * sinf(a1)) },
            { cx + (int)(r_ext  * cosf(a1)), cy + (int)(r_ext  * sinf(a1)) },
            { cx + (int)(r_ext  * cosf(a2)), cy + (int)(r_ext  * sinf(a2)) },
            { cx + (int)(r_body * cosf(a2)), cy + (int)(r_body * sinf(a2)) },
        };
        fill_quad(renderer, pts);
    }

    /* 3. Agujero central en color de fondo */
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    int rh = (int)r_hub;
    for (int y = -rh; y <= rh; y++) {
        int xw = (int)sqrtf((float)(rh * rh - y * y));
        SDL_RenderLine(renderer, cx - xw, cy + y, cx + xw, cy + y);
    }
}

void
dibujadoTextoColor(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, SDL_Color color){
    SDL_Surface *sup = TTF_RenderText_Blended(fuente, texto, 0, color);
    if (!sup) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, sup);
    SDL_FRect pos_txt = {x + 10, y + 12, sup->w, sup->h};
    SDL_RenderTexture(renderer, tex, NULL, &pos_txt);
    SDL_DestroySurface(sup);
    SDL_DestroyTexture(tex);
}

void
dibujadoTexto(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y){
    // Config del screenMenu(){
    // texto del botón
    SDL_Color blanco = {255, 255, 255, 255};
    // El suo->w : cuando hago el TTF_RenderText_Solid() devuelve un SDL_Surface * que es una imagen en memoria del texto renderizado. Esa surface tiene propiedades:
    //  sup->w   ← el ancho en píxeles que ocupa ese texto
    //  sup->h   ← el alto en píxeles

    //  ¿Por qué usarlo? Porque cada label tiene distinto largo:
    //  "1. Jugar"    →  sup->w = ~80px
    //  "3. Progreso" →  sup->w = ~120px

    //  Si ponés un número fijo te puede quedar cortado o con espacio de más. Usando
    //  sup->w el rectángulo del texto se ajusta exacto a lo que mide ese string.
    //  SDL_Surface *sup apunta a una estructura que SDL crea en memoria con la imagen del texto ya renderizada. 
    SDL_Surface *sup = TTF_RenderText_Blended(fuente, texto, 0, blanco);
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, sup);
    SDL_FRect pos_txt = {x + 10, y + 12, sup->w, sup->h};
    SDL_RenderTexture(renderer, tex, NULL, &pos_txt);
    SDL_DestroySurface(sup);
    SDL_DestroyTexture(tex);
    //}

}



/*
 *  char linea[512] = "" — línea 110

  Es el acumulador. Acá vas juntando palabras hasta que no entren más. Empieza vacío "" porque al inicio no tenés nada acumulado todavía.

  ---
  int y_actual = y — línea 114

  y es donde arranca el texto (por ejemplo y=70). y_actual va creciendo cada vez que dibujás una línea y bajás a la siguiente. y no lo tocás, y_actual es el que se mueve.

  ---
  TTF_GetFontHeight(fuente) + 4 — línea 117

  TTF_GetFontHeight() te da la altura en píxeles de la fuente — por ejemplo 16px. El +4 es un margen entre líneas para que no queden pegadas. Sin él el texto quedaría así:
  Implementa una funcion
  que reciba un array       ← pegado arriba
  Con +4 queda con espacio respirable. Podés cambiarlo a +6 o +8 si querés más separación.

  ---
  strtok(copia, " \n\r\t") — línea 119

  strtok parte un string en pedazos usando un separador — en este caso el espacio " ".

  Primera llamada: strtok(copia, " \n\r\t") — le pasás el string, te devuelve la primera palabra.

  Dentro del while, línea 137: strtok(NULL, " \n\r\t") — le pasás NULL para decirle "seguí desde donde quedaste", te devuelve la siguiente palabra.

  Cuando no quedan más palabras devuelve NULL y el while termina.

  strtok("Implementa una funcion", " ")
    → primera llamada → "Implementa"
    → strtok(NULL, " \n\r\t") → "una"
    → strtok(NULL, " \n\r\t") → "funcion"
    → strtok(NULL, " \n\r\t") → NULL  ← fin

  ---
  También noto un bug en la línea 122 — el operador ternario está mal:

  (linea[0] != '\0') ? " " : " "   ← ambos casos son " ", no hace nada

  Debería ser:
  (linea[0] != '\0') ? " " : ""   ← si linea tiene algo, agrega espacio; si está vacía, no

  Corregí eso.*/



































//===
/* La logica que se usa simplemente es palabra por palabra, probando si entra en el box
 *
 * El Enunciado tiene que decir: "Implementa una funcion que reciba un array"
 * 
 * ----Iteracion 1----
 *  palabra = "Implementa"
 *  prueba = "Implementa"  <- entra en max_ancho? ==> SI
 *  linea = "Implementa"
 * 
 * ----Iteracion 2----
 *  palabra = "una"
 *  prueba = "Implementa una"  <- entra en max_ancho? ==> SI
 *  linea = "Implementa una"
 * 
 * ----Iteracion 3----
 *  palabra = "funcion"
 *  prueba = "Implementa una funcion" <- entra en max_ancho? ==> SI
 *  linea = "Implementa una funcion"        
 * 
 * ----Iteracion 4----
 *  palabra = "reciba"
 *  prueba = "que reciba"             <- entra en max_ancho? ==> SI
 *          -> Dibujar "Implementa una funcion"      en y = 70
 *          -> y_actual += alto_linea
 *          -> linea = "que"          <- empieza linea nueva
 * 
 * ----Iteracion 5----
 *  palabra = "reciba"
 *  prueba = "que reciba"           <- entra en max_ancho? ==> SI
 *  linea = "que reciba"
 *
 * ----Iteracion 6----> asi sucesivamente hasta que termine la consigna, lo importante es que va almacenandoce y preguntando palabra por palabra si cabe dentro de max_ancho.
 *
 */ 
//===


void
dibujadoTextoMultilineaColor(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, int max_ancho, SDL_Color color)
{
    char copia[2048];
    strncpy(copia, texto, sizeof(copia) - 1);
    copia[sizeof(copia) - 1] = '\0';

    char linea[512] = "";
    int  y_actual   = y;
    int  alto_linea = TTF_GetFontHeight(fuente) + 4;
    char *palabra   = strtok(copia, " \n\r\t");

    while (palabra != NULL) {
        char prueba[512];
        snprintf(prueba, sizeof(prueba), "%s%s%s", linea, (linea[0] != '\0') ? " " : "", palabra);

        int ancho_prueba;
        TTF_GetStringSize(fuente, prueba, 0, &ancho_prueba, NULL);

        if (ancho_prueba > max_ancho && linea[0] != '\0') {
            dibujadoTextoColor(renderer, fuente, linea, x, y_actual, color);
            y_actual += alto_linea;
            snprintf(linea, sizeof(linea), "%s", palabra);
        } else {
            snprintf(linea, sizeof(linea), "%s", prueba);
        }
        palabra = strtok(NULL, " \n\r\t");
    }

    if (linea[0] != '\0')
        dibujadoTextoColor(renderer, fuente, linea, x, y_actual, color);
}

void
dibujadoTextoMultiLinea(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, int max_ancho)
{
    char copia[2048];
    strncpy(copia, texto, sizeof(copia) - 1);
    copia[sizeof(copia) - 1] = '\0';
    
    // Para que se crea char linea [512]?
    //      
    char linea [512] = "";
    
    // Este es el actual de pixeles en y para que?
    //
    int y_actual = y;
    // Esto lo que hace segun lo que entiendo el alto de la fuente de fuente +4 es asignado al alto_linea porque + 4?
    //
    int alto_linea = TTF_GetFontHeight(fuente) + 4;
    // que es strtok()?
    char *palabra = strtok(copia, " \n\r\t");
    while (palabra != NULL){
        char prueba[512];
        // el "" devuelve un valor? -> " " es nada?
        snprintf(prueba, sizeof(prueba), "%s%s%s", linea, (linea[0] != '\0') ? " " : "", palabra);
        
        int ancho_prueba;
        // TTF_SizeUTF8()
        //      Te dice cuantos pixeles ocupa un string con esa fuente. No contas caracteres, medis pixeles reales. Asi funciona bien con cualquier fuente y tamano.
        TTF_GetStringSize(fuente, prueba, 0, &ancho_prueba, NULL);

        if (ancho_prueba > max_ancho && linea[0] != '\0'){
            dibujadoTexto(renderer, fuente, linea, x, y_actual);
            y_actual += alto_linea;
            snprintf(linea, sizeof(linea), "%s", palabra);
          } else {
              snprintf(linea, sizeof(linea), "%s", prueba);
          }

          palabra = strtok(NULL, " \n\r\t");
      }

      if (linea[0] != '\0')
          dibujadoTexto(renderer, fuente, linea, x, y_actual);
}

void
screen_poweron(SDL_Renderer *renderer, int ancho, int alto)
{
    int cy = alto / 2;
    srand(SDL_GetTicks());

    // FASE 1 (frames 0-24): aparece el punto/linea brillante en el centro
    for (int f = 0; f < 25; f++) {
        float p = (float)f / 24.0f;   // 0 → 1

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // la linea crece en ancho con aceleracion (p^2)
        int lw = (int)(p * p * ancho);
        int lx = (ancho - lw) / 2;
        int lh = 2 + (int)(p * 5);

        // halo alrededor de la linea (bloom)
        for (int g = 10; g >= 1; g--) {
            Uint8 a = (Uint8)(40.0f * p / g);
            SDL_SetRenderDrawColor(renderer, 200, 230, 200, a);
            SDL_FRect halo = {lx - g*3, cy - lh/2 - g*2,
                             lw + g*6, lh + g*4};
            SDL_RenderFillRect(renderer, &halo);
        }

        // nucleo brillante
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, (Uint8)(220 * p));
        SDL_FRect linea = {lx, cy - lh/2, lw, lh};
        SDL_RenderFillRect(renderer, &linea);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        presente(renderer);
        SDL_Delay(14);
    }

    // FASE 2 (frames 0-50): la linea se abre verticalmente
    for (int f = 0; f < 51; f++) {
        float p  = (float)f / 50.0f;
        // ease-out: empieza rapido, frena al llegar al borde
        float ep = 1.0f - (1.0f - p) * (1.0f - p);

        int half_h = (int)(ep * (alto / 2));
        int top    = cy - half_h;
        int bot    = cy + half_h;
        if (top < 0)    top = 0;
        if (bot > alto) bot = alto;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // zona visible: fondo verde oscuro (como el menu)
        SDL_SetRenderDrawColor(renderer, 8, 14, 8, 255);
        SDL_FRect visible = {0, top, ancho, bot - top};
        SDL_RenderFillRect(renderer, &visible);

        // algo de ruido en la zona visible
        if (rand() % 2 == 0) {
            int ny = top + rand() % (bot - top + 1);
            Uint8 v = (Uint8)(rand() % 120);
            SDL_SetRenderDrawColor(renderer, v, v, v, 80);
            SDL_FRect nr = {0, ny, ancho, 1};
            SDL_RenderFillRect(renderer, &nr);
        }

        // bordes brillantes (las "cortinas" que se abren)
        for (int g = 8; g >= 0; g--) {
            Uint8 a = (Uint8)(120 / (g + 1));
            SDL_SetRenderDrawColor(renderer, 180, 220, 180, a);
            if (top - g >= 0) {
                SDL_FRect et = {0, top - g, ancho, 1};
                SDL_RenderFillRect(renderer, &et);
            }
            if (bot + g < alto) {
                SDL_FRect eb = {0, bot + g, ancho, 1};
                SDL_RenderFillRect(renderer, &eb);
            }
        }
        // nucleo del borde
        SDL_SetRenderDrawColor(renderer, 220, 255, 220, 200);
        SDL_FRect et = {0, top,     ancho, 2};
        SDL_FRect eb = {0, bot - 2, ancho, 2};
        SDL_RenderFillRect(renderer, &et);
        SDL_RenderFillRect(renderer, &eb);

        // barras negras (lo que todavia no se ve)
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (top > 0) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_FRect tb = {0, 0, ancho, top};
            SDL_RenderFillRect(renderer, &tb);
        }
        if (bot < alto) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_FRect bb = {0, bot, ancho, alto - bot};
            SDL_RenderFillRect(renderer, &bb);
        }

        presente(renderer);
        SDL_Delay(14);
    }

    // FASE 3 (frames 0-30): pantalla completa, parpadeos hasta estabilizarse
    for (int f = 0; f < 31; f++) {
        float p = (float)f / 30.0f;

        // parpadeos al principio, se van calmando
        int parpadeo = (f % 4 == 0 && p < 0.6f) ? 50 : 0;
        SDL_SetRenderDrawColor(renderer,
            8  + parpadeo,
            14 + parpadeo,
            8  + parpadeo, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        int n_ruido = (int)((1.0f - p) * 20);
        for (int i = 0; i < n_ruido; i++) {
            int ry = rand() % alto;
            int rw = ancho / 2 + rand() % (ancho / 2);
            int rx = rand() % (ancho / 4);
            Uint8 v = (Uint8)(rand() % 160);
            SDL_SetRenderDrawColor(renderer, v, v, v, 90);
            SDL_FRect nl = {rx, ry, rw, 1};
            SDL_RenderFillRect(renderer, &nl);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        presente(renderer);
        SDL_Delay(16);
    }
}

void
screen_transition(SDL_Renderer *renderer, int ancho, int alto)
{
    int cy = alto / 2;
    srand(SDL_GetTicks());

    /* ── MITAD 1: pantalla se comprime hacia el centro (apagado CRT) ── */
    for (int f = 0; f < 22; f++) {
        float p  = (float)f / 21.0f;
        float ep = p * p;   /* ease-in: rapido al final */

        int half_h = (int)((1.0f - ep) * (alto / 2));
        int top    = cy - half_h;
        int bot    = cy + half_h;
        if (top < 0)    top = 0;
        if (bot > alto) bot = alto;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        /* zona visible que colapsa */
        SDL_SetRenderDrawColor(renderer, 8, 14, 8, 255);
        if (bot > top) {
            SDL_FRect vis = {0, top, ancho, bot - top};
            SDL_RenderFillRect(renderer, &vis);
        }

        /* estatica en la zona visible */
        if (bot > top) {
            int n = (int)(ep * 12);
            for (int i = 0; i < n; i++) {
                int ry = top + rand() % (bot - top + 1);
                int rw = rand() % ancho;
                SDL_SetRenderDrawColor(renderer, 180, 200, 180, 100);
                SDL_FRect nl = {rand() % (ancho - rw + 1), ry, rw, 1};
                SDL_RenderFillRect(renderer, &nl);
            }
        }

        /* bordes brillantes que se cierran */
        for (int g = 6; g >= 1; g--) {
            Uint8 a = (Uint8)(120 / g);
            SDL_SetRenderDrawColor(renderer, 160, 220, 160, a);
            if (top - g >= 0) { SDL_FRect e = {0, top-g, ancho, 1}; SDL_RenderFillRect(renderer, &e); }
            if (bot + g < alto){ SDL_FRect e = {0, bot+g, ancho, 1}; SDL_RenderFillRect(renderer, &e); }
        }
        SDL_SetRenderDrawColor(renderer, 200, 255, 200, 200);
        SDL_FRect et = {0, top,     ancho, 2};
        SDL_FRect eb = {0, bot - 2, ancho, 2};
        SDL_RenderFillRect(renderer, &et);
        SDL_RenderFillRect(renderer, &eb);

        /* barras negras */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (top > 0)    { SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_FRect b={0,0,ancho,top};     SDL_RenderFillRect(renderer,&b); }
        if (bot < alto) { SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_FRect b={0,bot,ancho,alto-bot}; SDL_RenderFillRect(renderer,&b); }

        presente(renderer);
        SDL_Delay(9);
    }

    /* ── linea brillante central breve ── */
    for (int f = 0; f < 6; f++) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int g = 8; g >= 1; g--) {
            SDL_SetRenderDrawColor(renderer, 160, 220, 160, (Uint8)(80/g));
            SDL_FRect h = {0, cy - g*2, ancho, 4 + g*4};
            SDL_RenderFillRect(renderer, &h);
        }
        SDL_SetRenderDrawColor(renderer, 230, 255, 230, 240);
        SDL_FRect linea = {0, cy - 1, ancho, 3};
        SDL_RenderFillRect(renderer, &linea);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        presente(renderer);
        SDL_Delay(10);
    }

    /* ── MITAD 2: pantalla se abre desde el centro (encendido CRT) ── */
    for (int f = 0; f < 22; f++) {
        float p  = (float)f / 21.0f;
        float ep = 1.0f - (1.0f - p) * (1.0f - p);  /* ease-out */

        int half_h = (int)(ep * (alto / 2));
        int top    = cy - half_h;
        int bot    = cy + half_h;
        if (top < 0)    top = 0;
        if (bot > alto) bot = alto;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_SetRenderDrawColor(renderer, 8, 14, 8, 255);
        if (bot > top) {
            SDL_FRect vis = {0, top, ancho, bot - top};
            SDL_RenderFillRect(renderer, &vis);
        }

        /* bordes brillantes que se abren */
        for (int g = 6; g >= 1; g--) {
            Uint8 a = (Uint8)(120 / g);
            SDL_SetRenderDrawColor(renderer, 160, 220, 160, a);
            if (top - g >= 0) { SDL_FRect e = {0, top-g, ancho, 1}; SDL_RenderFillRect(renderer, &e); }
            if (bot + g < alto){ SDL_FRect e = {0, bot+g, ancho, 1}; SDL_RenderFillRect(renderer, &e); }
        }
        SDL_SetRenderDrawColor(renderer, 200, 255, 200, 200);
        SDL_FRect et2 = {0, top,     ancho, 2};
        SDL_FRect eb2 = {0, bot - 2, ancho, 2};
        SDL_RenderFillRect(renderer, &et2);
        SDL_RenderFillRect(renderer, &eb2);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (top > 0)    { SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_FRect b={0,0,ancho,top};      SDL_RenderFillRect(renderer,&b); }
        if (bot < alto) { SDL_SetRenderDrawColor(renderer, 0,0,0,255); SDL_FRect b={0,bot,ancho,alto-bot}; SDL_RenderFillRect(renderer,&b); }

        presente(renderer);
        SDL_Delay(9);
    }
}

void
screen_poweroff(SDL_Renderer *renderer, int ancho, int alto)
{
    int cy = alto / 2;
    srand(SDL_GetTicks());

    /* FASE 1 (40 frames): la pantalla se comprime verticalmente hacia el centro
       como un CRT apagandose — rapido al principio, frena al llegar a la linea */
    for (int f = 0; f < 40; f++) {
        float p  = (float)f / 39.0f;
        /* ease-in: lento al principio, rapido al final */
        float ep = p * p;

        int half_h = (int)((1.0f - ep) * (alto / 2));
        int top    = cy - half_h;
        int bot    = cy + half_h;
        if (top < 0)    top = 0;
        if (bot > alto) bot = alto;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        /* zona visible que se va achicando */
        SDL_SetRenderDrawColor(renderer, 8, 14, 8, 255);
        SDL_FRect visible = {0, top, ancho, bot - top};
        SDL_RenderFillRect(renderer, &visible);

        /* ruido estatico mientras colapsa */
        int n_ruido = (int)(ep * 25);
        for (int i = 0; i < n_ruido; i++) {
            if (bot <= top) break;
            int ry = top + rand() % (bot - top + 1);
            int rw = rand() % ancho;
            int rx = rand() % (ancho - rw + 1);
            Uint8 v = (Uint8)(rand() % 200);
            SDL_SetRenderDrawColor(renderer, v, v, v, 120);
            SDL_FRect nl = {rx, ry, rw, 1};
            SDL_RenderFillRect(renderer, &nl);
        }

        /* bordes brillantes que se cierran */
        for (int g = 8; g >= 0; g--) {
            Uint8 a = (Uint8)(150 / (g + 1));
            SDL_SetRenderDrawColor(renderer, 180, 230, 180, a);
            if (top - g >= 0) {
                SDL_FRect et = {0, top - g, ancho, 1};
                SDL_RenderFillRect(renderer, &et);
            }
            if (bot + g < alto) {
                SDL_FRect eb = {0, bot + g, ancho, 1};
                SDL_RenderFillRect(renderer, &eb);
            }
        }
        SDL_SetRenderDrawColor(renderer, 220, 255, 220, 220);
        SDL_FRect et = {0, top,     ancho, 2};
        SDL_FRect eb = {0, bot - 2, ancho, 2};
        SDL_RenderFillRect(renderer, &et);
        SDL_RenderFillRect(renderer, &eb);

        /* barras negras arriba y abajo */
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        if (top > 0) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_FRect tb = {0, 0, ancho, top};
            SDL_RenderFillRect(renderer, &tb);
        }
        if (bot < alto) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_FRect bb = {0, bot, ancho, alto - bot};
            SDL_RenderFillRect(renderer, &bb);
        }

        presente(renderer);
        SDL_Delay(10);
    }

    /* FASE 2 (25 frames): queda solo la linea brillante horizontal,
       se encoge desde los extremos hacia el centro */
    for (int f = 0; f < 25; f++) {
        float p  = (float)f / 24.0f;
        float ep = p * p;

        int lw = (int)((1.0f - ep) * ancho);
        int lx = (ancho - lw) / 2;
        int lh = 3;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        /* halo alrededor de la linea */
        for (int g = 8; g >= 1; g--) {
            Uint8 a = (Uint8)(60 * (1.0f - p) / g);
            SDL_SetRenderDrawColor(renderer, 180, 230, 180, a);
            SDL_FRect halo = {lx - g*2, cy - lh/2 - g*2, lw + g*4, lh + g*4};
            SDL_RenderFillRect(renderer, &halo);
        }
        /* nucleo */
        SDL_SetRenderDrawColor(renderer, 230, 255, 230, (Uint8)(255 * (1.0f - p * 0.5f)));
        SDL_FRect linea = {lx, cy - lh/2, lw, lh};
        SDL_RenderFillRect(renderer, &linea);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        presente(renderer);
        SDL_Delay(12);
    }

    /* FASE 3 (20 frames): el punto central se desvanece */
    for (int f = 0; f < 20; f++) {
        float p = (float)f / 19.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        int pw = (int)((1.0f - p) * 12);
        if (pw > 0) {
            Uint8 a = (Uint8)(255 * (1.0f - p));
            for (int g = 6; g >= 1; g--) {
                SDL_SetRenderDrawColor(renderer, 200, 240, 200, (Uint8)(a / g));
                SDL_FRect halo = {ancho/2 - pw/2 - g*3, cy - g*2, pw + g*6, 4 + g*4};
                SDL_RenderFillRect(renderer, &halo);
            }
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, a);
            SDL_FRect punto = {ancho/2 - pw/2, cy - 1, pw, 3};
            SDL_RenderFillRect(renderer, &punto);
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        presente(renderer);
        SDL_Delay(16);
    }

    /* negro final */
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    presente(renderer);
    SDL_Delay(80);
}


