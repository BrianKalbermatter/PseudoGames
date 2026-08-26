#ifndef UI_H
#define UI_H
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

/* ── presente: reemplaza SDL_RenderPresent en todos los screens ──────────────
 * Con SDL_CreateSoftwareRenderer sobre la window surface, SDL_RenderPresent
 * es un no-op. Esta funcion llama SDL_UpdateWindowSurface para que el frame
 * realmente aparezca en pantalla (necesario en WSL2/XWayland con visual ARGB).
 * Llamar presente(renderer) en vez de SDL_RenderPresent(renderer).           */
void ui_set_ventana(SDL_Window *v);
void presente(SDL_Renderer *renderer);


/* ── Puentes SDL2 -> SDL3 ────────────────────────────────────────────────
 * SDL3 movio tres cosas de lugar. En vez de repartir la conversion por
 * diez archivos, se concentra aca: un solo sitio que arreglar si cambia.  */

/* SDL3 da la posicion del mouse en float (soporta subpixel y escalado de
   pantalla). El UI razona en pixeles enteros, asi que se redondea aca.    */
SDL_MouseButtonFlags ui_mouse(int *x, int *y);

/* SDL3 DIBUJA con SDL_FRect (float) pero RECORTA con SDL_Rect (int).
   Es la trampa mas facil de pasar por alto: fill y draw pasaron a float,
   clip y viewport se quedaron en enteros.                                 */
void ui_clip(SDL_Renderer *r, const SDL_FRect *area);
void ui_viewport(SDL_Renderer *r, const SDL_FRect *area);

/* SDL3 pide la ventana para saber donde ubicar el candidato del IME.
   Usa la ventana registrada con ui_set_ventana().                         */
void ui_text_input(bool on);

// helpers de UI compartidos entre pantallas
void dibujadoTexto(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y);
void dibujadoTextoColor(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, SDL_Color color);

void dibujadoTextoMultilinea(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, int max_ancho);
void dibujadoTextoMultilineaColor(SDL_Renderer *renderer, TTF_Font *fuente, const char *texto, int x, int y, int max_ancho, SDL_Color color);

void screen_transition(SDL_Renderer *renderer, int ancho, int alto);
void screen_poweron(SDL_Renderer *renderer, int ancho, int alto);
void screen_poweroff(SDL_Renderer *renderer, int ancho, int alto);
void dibujarArandela(SDL_Renderer *renderer, int cx, int cy, int radio, SDL_Color color, SDL_Color bg);

// declaraciones de pantallas
int screenMenu(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);
int screenDoc(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);

/* API de panel para el shell de tabs */
typedef struct DocState DocState;
DocState *doc_crear   (TTF_Font *fuente);
void      doc_destruir(DocState *s);
void      doc_evento  (DocState *s, TTF_Font *fuente, SDL_Event *e, SDL_FRect area);
void      doc_dibujar (DocState *s, SDL_Renderer *r, TTF_Font *fuente, SDL_FRect area);
int screenLvLs(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);

/* API de panel para el shell de tabs (Niveles) */
typedef struct NivelesState NivelesState;
NivelesState *niveles_crear         (TTF_Font *fuente);
void          niveles_destruir      (NivelesState *s);
int           niveles_consumir_nivel(NivelesState *s);
void          niveles_refrescar     (NivelesState *s);
void          niveles_evento        (NivelesState *s, SDL_Event *e, SDL_FRect area);
void          niveles_dibujar       (NivelesState *s, SDL_Renderer *r,
                                     TTF_Font *fuente, SDL_FRect area);
int screenLvLEditor(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto, int nivel_num);
int screenSoluciones(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);
int screenPomodoro(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);
int screenConfig(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto, SDL_Window *ventana);
int screenFeedback(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);
int screenFreeEditor(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto, int nivel_num);
int screenTutorial(SDL_Renderer *renderer, TTF_Font *fuente, int ancho, int alto);
#endif
