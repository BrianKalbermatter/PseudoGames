# Cambios: Migración de SDL2 a SDL3

## Fecha
2026-08-26

## Qué se hizo

Se migró PseudoGames entero de SDL2 a SDL3, incluidas las tres librerías
satélite (`SDL3_ttf`, `SDL3_mixer`, `SDL3_image`). 25 archivos tocados,
~1236 líneas insertadas y ~1080 borradas.

También se migró el subproyecto `screenPJ/` y `scripts/pack.sh`.

---

## El descubrimiento: nunca hubo SDL2

```
local/sdl2-compat 2.32.70-1
    An SDL2 compatibility layer that uses SDL3 behind the scenes
local/sdl3 3.4.14-1
```

El equipo no tenía SDL2 instalado. Tenía **`sdl2-compat`**, un shim que
traduce las llamadas SDL2 a SDL3 por debajo. O sea que el juego ya venía
corriendo sobre SDL3 con una capa de traducción en el medio. Migrar sacó
esa capa, no "trajo" SDL3.

---

## La trampa número uno: el sentido invertido

```c
// SDL2 — devuelve int, 0 es ÉXITO
if (SDL_Init(SDL_INIT_VIDEO) != 0) { error(); }

// SDL3 — devuelve bool, true es ÉXITO
if (!SDL_Init(SDL_INIT_VIDEO)) { error(); }
```

Es el cambio más peligroso de toda la migración, porque un
`if (SDL_Init(...) != 0)` sigue **compilando** en SDL3: `true != 0` es
verdadero, así que el programa entra al error justo cuando todo salió bien.
No hay warning. Hay que leer cada uno.

---

## La trampa número dos: rects de dos tipos

En SDL3 las funciones de **dibujo** pasaron a coordenadas float, pero las de
**recorte** se quedaron en enteros:

| Función | Rect que pide |
|---------|---------------|
| `SDL_RenderFillRect`, `SDL_RenderRect`, `SDL_RenderTexture` | `SDL_FRect` (float) |
| `SDL_SetRenderClipRect`, `SDL_SetRenderViewport` | `SDL_Rect` (int) |

Como el editor razona en píxeles enteros, se convirtió todo a `SDL_FRect`
y se pusieron **puentes** para los dos casos que siguen en enteros, en vez
de repartir 44 conversiones sueltas por diez archivos.

---

## Puentes nuevos en `ui.h` / `ui.c`

| Función | Por qué existe |
|---------|----------------|
| `ui_mouse(int *x, int *y)` | `SDL_GetMouseState` devuelve `float*` en SDL3 (soporta subpíxel y escalado). El UI razona en enteros: se redondea acá. |
| `ui_clip(r, const SDL_FRect*)` | Convierte a `SDL_Rect` para `SDL_SetRenderClipRect`. |
| `ui_viewport(r, const SDL_FRect*)` | Ídem para `SDL_SetRenderViewport`. |
| `ui_text_input(bool)` | `SDL_StartTextInput`/`StopTextInput` ahora piden la ventana. Usa la que ya guardaba `ui_set_ventana()`. |

Un solo lugar que arreglar si SDL vuelve a cambiar.

---

## `audio.c`: reescrito entero

**SDL3_mixer no es un rename: es una API nueva.** No existen más `Mix_Music`,
`Mix_Chunk` ni los canales globales. Ahora hay tres objetos explícitos:

| Objeto | Qué es |
|--------|--------|
| `MIX_Mixer` | El dispositivo de audio abierto |
| `MIX_Audio` | Un sonido cargado en memoria (el archivo) |
| `MIX_Track` | Una "boca" que reproduce un `MIX_Audio` |

Un `MIX_Track` es lo que en SDL2 era "el canal". Ahora hay dos: uno para la
música y otro para el click. No hay estado global escondido: cada operación
dice sobre **qué** track actúa.

Los parámetros de reproducción (loops, fade in) viajan en un
`SDL_PropertiesID` en vez de multiplicarse en funciones (`Mix_PlayMusic`,
`Mix_FadeInMusic`, `Mix_FadeInMusicPos`...). Una sola función, opciones con
nombre.

El fade se mide en **frames de audio**, no en milisegundos:
`MIX_TrackMSToFrames()` hace la cuenta con la frecuencia real del dispositivo.

**Y sin embargo `audio.h` no cambió ni una línea.** Los 15 archivos que
llaman `audio_play()` quedaron intactos. Esa es la prueba de que la interfaz
estaba bien puesta: no filtraba un solo tipo de SDL.

---

## Tabla de renames

| SDL2 | SDL3 |
|------|------|
| `SDL_RenderCopy` | `SDL_RenderTexture` |
| `SDL_RenderDrawLine` / `DrawRect` | `SDL_RenderLine` / `SDL_RenderRect` |
| `SDL_RenderSetClipRect` | `SDL_SetRenderClipRect` |
| `SDL_FreeSurface` | `SDL_DestroySurface` |
| `SDL_QUIT`, `SDL_KEYDOWN` | `SDL_EVENT_QUIT`, `SDL_EVENT_KEY_DOWN` |
| `e.key.keysym.sym` | `e.key.key` (el evento se aplanó) |
| `SDLK_a` | `SDLK_A` (el valor sigue siendo `'a'`) |
| `TTF_RenderUTF8_Blended(f,t,c)` | `TTF_RenderText_Blended(f,t,**0**,c)` |
| `TTF_SizeUTF8` | `TTF_GetStringSize` (+ largo) |
| `TTF_FontHeight` | `TTF_GetFontHeight` |
| `SDL_CreateRGBSurfaceWithFormat` | `SDL_CreateSurface` (sin flags ni bits) |
| `SDL_MapRGB(surf->format,...)` | `SDL_MapSurfaceRGB(surf,...)` |
| `SDL_ShowCursor(SDL_DISABLE)` | `SDL_HideCursor()` |
| `SDL_SetRelativeMouseMode` | `SDL_SetWindowRelativeMouseMode` (por ventana) |
| `SDL_WINDOWEVENT` + `.window.event` | cada evento es su propio `type` |

El `0` de las funciones TTF es el largo del texto: **0 significa "terminado
en NUL"**.

---

## El mapa lo da SDL

SDL3 incluye `SDL_oldnames.h`, que convierte cada símbolo viejo en un
identificador que **dice el nombre nuevo en el error**:

```
use of undeclared identifier 'SDL_KEYDOWN_renamed_SDL_EVENT_KEY_DOWN'
```

Sumado a que muchas funciones cambiaron la **cantidad** de argumentos,
cualquier llamada que se escape es error duro. Nunca falla en silencio.
Por eso esta migración se pudo scriptear con seguridad.

---

## Un bug que estaba escondido

`editorText.c` usaba `calloc` y `free` sin incluir `<stdlib.h>`: los headers
de SDL2 lo arrastraban de rebote. Los de SDL3 no. El bug estaba desde
siempre; SDL3 solo lo destapó.

---

## Archivos MODIFICADOS

`src/`: `audio.c` (reescrito), `audio.h`, `editorText.c/h`, `main.c`,
`screenCEditor.c/h`, `screenConfig.c`, `screenDOC.c`, `screenEditorFree.c`,
`screenEditorLvl.c`, `screenFeedback.c`, `screenLvLs.c`, `screenMenu.c`,
`screenPJ.c/h`, `screenPomodoro.c`, `screenSoluction.c`, `screenTutorial.c/h`,
`shell.c/h`, `ui.c/h`

Build: `Makefile` (de `-lSDL2...` a `pkg-config sdl3 sdl3-ttf sdl3-mixer sdl3-image`),
`scripts/pack.sh`, `screenPJ/Makefile`, `screenPJ/src/main.c`

---

## Lo que quedó pendiente

- ~~**`src/editorBim.c` sin migrar, a propósito.**~~ **Resuelto el 2026-08-29,
  pero no migrando: reemplazando.** `editorBim.c` y los `.sh` del editor se
  borraron, y en su lugar quedó `xasol` (`src/xasol.c`, `src/xasol_sintaxis.c`),
  escrito en SDL3 desde el arranque. El wrapper `editor_bim_draw_panel()` de
  `shell.c` ya no existe: `xasol_draw` toma `SDL_FRect` directo. El motivo del
  reemplazo no fue la migración sino el resaltado — ver `ESTRUCTURA.md`.
- **El cross-compile a Windows sigue en SDL2** (`Makefile` líneas 46-47,
  `scripts/pack-windows.sh`, `scripts/release.sh`): faltan las librerías de
  desarrollo SDL3 para MinGW.
- **`docSDL2.txt`** quedó como referencia histórica de SDL2. Lo que cambió
  está en este archivo.

---

## Verificación

Compila y linkea **sin un solo warning** contra `libSDL3`, `libSDL3_ttf`,
`libSDL3_mixer` y `libSDL3_image`, y el binario arranca sin errores (lo que
incluye el mixer levantando el dispositivo de audio y la fuente cargando).
