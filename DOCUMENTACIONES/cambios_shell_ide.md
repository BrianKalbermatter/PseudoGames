# Cambios: Refactorización a arquitectura IDE Shell

## Fecha
2026-04-09 / 2026-04-10

## Qué se hizo

Se reemplazó el sistema de menú + pantallas separadas por una arquitectura
tipo IDE (similar a VS Code), donde todo convive en una sola pantalla con
sidebar izquierdo colapsable y sistema de tabs.

Adicionalmente se resolvió un problema crítico de **ventana invisible en WSL2**
causado por el visual ARGB de X11 y se reemplazó el sistema de renderizado.

---

## Problema raíz: ventana invisible en WSL2/XWayland

### Causa
SDL2 en WSL2 elige un visual X11 de 32 bits con canal alpha (ARGB).
Cuando se usa `SDL_CreateRenderer` (modo hardware/acelerado), `SDL_RenderPresent`
no escribe el canal alpha — queda en `0` — y la ventana aparece completamente
transparente. Se ve en la barra de tareas de Windows pero el contenido es invisible.

### Solución
Reemplazar el renderer hardware por un **Software Renderer sobre la window surface**:

```c
// ANTES (invisible en WSL2):
SDL_Renderer *renderer = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED);

// DESPUÉS (funciona en WSL2):
SDL_Surface  *wsurface = SDL_GetWindowSurface(ventana);
SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(wsurface);
```

Y en vez de `SDL_RenderPresent`, llamar `SDL_UpdateWindowSurface` para que
el frame realmente llegue a la pantalla.

### El wrapper `presente()`
Como todas las pantallas del juego llaman `SDL_RenderPresent`, se creó una
función wrapper que hace LAS DOS cosas:

```c
// ui.c
static SDL_Window *g_ventana = NULL;

void ui_set_ventana(SDL_Window *v) { g_ventana = v; }

void presente(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);        // no-op con SW renderer, pero seguro
    if (g_ventana)
        SDL_UpdateWindowSurface(g_ventana);  // este es el que realmente muestra
}
```

Después se reemplazó **TODAS** las llamadas a `SDL_RenderPresent(renderer)` en
todos los archivos `.c` por `presente(renderer)`.

> **Gotcha importante**: el `sed` que hizo el reemplazo también reemplazó
> la línea `SDL_RenderPresent(renderer)` DENTRO de `presente()` misma,
> creando una recursión infinita. Se tuvo que restaurar manualmente esa línea.

---

## Archivos CREADOS (nuevos)

| Archivo | Descripción |
|---------|-------------|
| `src/shell.h` | Header con todas las estructuras: `ShellCtx`, `Tab`, `PanelDef`, constantes de layout y colores |
| `src/shell.c` | Implementación del shell: loop principal, sidebar, tab bar, gestión de tabs, paneles placeholder |

---

## Archivos MODIFICADOS

### `src/main.c`

Cambios aplicados:

1. **Posición de ventana**: cambiado de `SDL_WINDOWPOS_CENTERED` a `(100, 100)`.
   - Motivo: en WSL2 con dos monitores, `SDL_WINDOWPOS_CENTERED` calculaba
     `x = 2486` (fuera de la pantalla visible).

2. **Hint XShm desactivado** (antes de `SDL_Init`):
   ```c
   SDL_SetHint("SDL_VIDEO_X11_XSHM", "0");
   ```
   Evita crash o comportamiento errático con la extensión MIT-SHM en XWayland.

3. **Renderer cambiado a Software sobre surface**:
   ```c
   SDL_Surface *wsurface = SDL_GetWindowSurface(ventana);
   SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(wsurface);
   ```

4. **`ui_set_ventana(ventana)`** llamado después de crear el renderer,
   para que `presente()` tenga referencia a la ventana.

5. **`#include "ui.h"`** agregado (necesario para que `ui_set_ventana` esté declarada).

6. **`screenShell()`** reemplaza el `do-while` con switch de 7 estados:
   ```c
   // ANTES: do { switch(estado) { case MENU: ... } } while (!salir);
   // DESPUÉS:
   screenShell(renderer, fuente, ancho, alto, ventana);
   ```

7. **Eliminadas** todas las llamadas a `screen_poweron`, `screenPJ_intro`,
   `screen_transition`, `screen_poweroff` — ya no existen en el nuevo sistema.

Estado final de `main.c`:
```c
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "niveles.h"
#include "progreso.h"
#include "pomodoro_bg.h"
#include "audio.h"
#include "config.h"
#include "shell.h"
#include "ui.h"
#include <stdio.h>
```

---

### `src/ui.h`

Agregadas las declaraciones de las dos funciones nuevas:

```c
void ui_set_ventana(SDL_Window *v);
void presente(SDL_Renderer *renderer);
```

---

### `src/ui.c`

Agregado al principio del archivo (antes de cualquier función existente):

```c
static SDL_Window *g_ventana = NULL;

void ui_set_ventana(SDL_Window *v) { g_ventana = v; }

void presente(SDL_Renderer *renderer) {
    SDL_RenderPresent(renderer);
    if (g_ventana)
        SDL_UpdateWindowSurface(g_ventana);
}
```

Además, todas las llamadas a `SDL_RenderPresent(renderer)` en `screen_poweron`,
`screen_transition` y `screen_poweroff` fueron reemplazadas por `presente(renderer)`.

---

### `src/shell.c` — cambios post-creación

Después de crear el archivo, se hicieron estos ajustes:

1. **`SDL_RenderPresent` → `SDL_UpdateWindowSurface`** en el loop principal:
   ```c
   // ANTES:
   SDL_RenderPresent(ctx.renderer);
   // DESPUÉS:
   SDL_UpdateWindowSurface(ctx.ventana);
   ```

2. **Manejador de resize** agregado en el `SDL_PollEvent`:
   ```c
   if (e.type == SDL_WINDOWEVENT &&
       (e.window.event == SDL_WINDOWEVENT_RESIZED ||
        e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)) {
       SDL_DestroyRenderer(renderer);
       SDL_Surface *ns = SDL_GetWindowSurface(ctx.ventana);
       renderer        = SDL_CreateSoftwareRenderer(ns);
       ctx.renderer    = renderer;
   }
   ```
   Motivo: con `SDL_CreateSoftwareRenderer(surface)`, cuando la ventana cambia
   de tamaño la surface se invalida — el renderer tiene un puntero viejo.
   Hay que destruirlo y recrearlo con la nueva surface.

3. **Función `cfg_fullscreen`** — fullscreen reemplazado por borderless maximize:
   ```c
   static void cfg_fullscreen(SDL_Window *ventana, int activar) {
       if (activar) {
           SDL_SetWindowBordered(ventana, SDL_FALSE);
           SDL_MaximizeWindow(ventana);
       } else {
           SDL_SetWindowBordered(ventana, SDL_TRUE);
           SDL_RestoreWindow(ventana);
       }
       SDL_RaiseWindow(ventana);
       SDL_SetWindowInputFocus(ventana);
       SDL_PumpEvents();
   }
   ```
   Motivo: `SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN_DESKTOP)` en WSL2 con
   Software Renderer causa que la surface se invalide y el renderer quede corrupto.
   `SDL_MaximizeWindow` + `SDL_SetWindowBordered(FALSE)` da el mismo resultado
   visual ("ventana que ocupa toda la pantalla") sin romper el renderer.

4. **Eliminados** todos los `fprintf` de debug y la variable `frame`.

---

### Pantallas que tuvieron `SDL_RenderPresent` → `presente()`:

- `src/screenDOC.c`
- `src/screenLvLs.c`
- `src/screenPomodoro.c`
- `src/editorText.c`
- `src/screenConfig.c`
- `src/screenSoluction.c`
- `src/screenFeedback.c`
- `src/screenTutorial.c`
- `src/screenMenu.c`
- `src/ui.c` (screen_poweron, screen_transition, screen_poweroff)

---

### `saves/config.json`

```json
// ANTES: "fullscreen": 0  (= pantalla completa activada en el código viejo)
// DESPUÉS: "fullscreen": 1  (= desactivada, ventana normal)
```

Motivo: el valor `0` aplicaba `SDL_SetWindowFullscreen` al inicio, lo que
con el nuevo renderer rompía la ventana. Se desactivó para que arranque en
modo ventana y el usuario pueda usar el toggle desde Config.

---

### `Makefile`

Agregado `src/shell.c` a la lista de fuentes `SRC` para que compile.

---

## Archivos que NO se modificaron (referencia histórica)

Los siguientes archivos siguen en el proyecto pero ya **no son llamados** desde
`main.c`. Se portarán uno por uno al nuevo sistema de paneles:

- `src/screenMenu.c` — menú flotante con matrix rain
- `src/screenDOC.c` — DOC screen original
- `src/screenConfig.c` — Config screen original
- `src/screenPomodoro.c` — Pomodoro screen original
- `src/screenLvLs.c` — selección de niveles original
- `src/screenEditorFree.c` — editor libre original
- `src/screenSoluction.c` — soluciones original
- `src/screenTutorial.c` — tutorial original
- `src/screenEditorLvl.c` — editor de niveles original

---

## Arquitectura nueva resumida

```
main.c
  └─> screenShell()           <- UN SOLO LOOP
        ├── draw_sidebar()    <- siempre visible, colapsable con < >
        ├── draw_tab_bar()    <- tabs arriba del contenido
        └── panel.draw()      <- contenido del tab activo
```

Cada pantalla se convierte en un "Panel" con 4 funciones (vtable en C):

```c
typedef struct {
    PanelID     id;
    const char *nombre;
    void (*init)        (ShellCtx *ctx, Tab *tab);   // cargar recursos
    void (*handle_event)(ShellCtx *ctx, Tab *tab, SDL_Event *e);
    void (*draw)        (ShellCtx *ctx, Tab *tab, SDL_Rect area);
    void (*cleanup)     (ShellCtx *ctx, Tab *tab);   // liberar recursos
} PanelDef;
```

---

## Estado actual de los paneles

Todos los paneles están como **placeholder** (muestran texto de prueba).
Se portarán en este orden (menor a mayor complejidad):

| # | Panel | Complejidad |
|---|-------|-------------|
| 1 | Config | Baja |
| 2 | Soluciones | Baja |
| 3 | Pomodoro | Media |
| 4 | DOC | Media-Alta |
| 5 | Editor Libre | Media |
| 6 | Niveles | Alta |

---

## Conceptos clave aprendidos en este proceso

### Por qué WSL2 era invisible
- SDL2 pide un visual X11 ARGB (32 bits con alpha)
- `SDL_RenderPresent` con renderer hardware NO escribe el canal alpha → queda `0` → ventana transparente
- `SDL_CreateSoftwareRenderer(surface)` escribe directamente en memoria de la surface, alpha incluido
- `SDL_UpdateWindowSurface` manda esa surface a XWayland que la muestra

### Por qué el resize destruye el renderer
- La "window surface" que devuelve `SDL_GetWindowSurface` es un puntero al buffer interno de la ventana
- Cuando la ventana cambia de tamaño, SDL recrea ese buffer internamente → el puntero viejo apunta a memoria inválida
- El renderer tiene ese puntero viejo → hay que destruirlo y recrearlo con la nueva surface

### Por qué `SDL_SetWindowFullscreen` rompe el renderer
- `SDL_WINDOW_FULLSCREEN_DESKTOP` hace un resize interno de la ventana
- Con Software Renderer, ese resize invalida la surface (igual que arriba)
- Pero el resize pasa ANTES de que el código llegue al manejador de `SDL_WINDOWEVENT_RESIZED`
- Por eso hay que usar `SDL_MaximizeWindow` + `SDL_SetWindowBordered(SDL_FALSE)` en su lugar
