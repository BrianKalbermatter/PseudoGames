 # La base de todo esta en main.c 
 init -> loop -> cleanup
 El loop corre 60 veces por segundos: 
 1 - Captura eventos(teclado, mouse)
 2 - Actualiza estado
 3 - Dibujar
 El menu, editor, pomodoro funcionan de esta manera.
 Como se sabe en que momento esta el usuario en el programa? 
 Por el valor de retorno de cada funcion.
 En el main.c hay un switch de opciones
 opcion = screenMenu(...)  // retorna 1, 2, 3...
  switch (opcion) {
      case 1: screenLvLs(...)
      case 2: screenDoc(...)
      ...
}
Cada una de las screens el usuario decide cuando salir por lo tanto es un loop hasta
que el usuario desea salir.

---
# Luego estan los sistemas de datos
niveles -> carga el JSON -> struct Nivel, seria un registro con todos los niveles
progreso -> guarda que completaste
config -> preferencias de usuario
Son independiente por lo tanto cualquier screen puede usarlo

- Structs -> Nivel, CasoPrueba, EditorPanel - **son la columna vertebral**. Donde esta el CasoPrueba, Nivel, EditorPanel

- Punteros -> Cada funcion recibe renderer, fuente - Por que no los copia?

- Header files -> niveles.h existe separado de niveles.c

- El loop de eventos SDL -> SDL_PollEvent: Como funciona y porque es un while

# Los helper:
Se pueden usar para definirla a una funcion en el archivo en el que esta, en este caso es static, por lo tanto sirve para llamarla multiples veces luego de forma mas corta.

Todo programa en C arranca aca. 
## argc/argv
son argumentos de lineas de comandos. El (void) es para decirle al compilador "se que esta, no lo uso y no me avises"

----

# Linea 24-31 - Init de SDL
if(SDL_Init(SDL_INIT_VIDEO) != 0){ ... }
// si da 0 entra en el if y dice hay un error en el SDL_Init
if(TTF_Init() != 0){ ... }
# Se ponen las fuentes. Si falla, sale con return 1. **El patron != 0 es el estandar de SDL para chequear errores**

----

# Linea 33-37 - Init de sistemas propios

audio_init();
config_cargar();
audio_set_volumen(config_get_volumen());
cargar_niveles("data/niveles.json");
cargar_progreso("saves/progreso.json");

# Linea 39-45 La fuente
TTF_Font *fuente = TTF_OpenFont("assets/fonts/main.ttf", 16);

Abre el archivo .ttf y lo convierte en un objeto TTF_Font *.
Ese puntero se pasa a TODAS las pantallas. Una sola fuente, compartida por todo.

# Linea 47-67 Ventana y renderer
Líneas 47-67 — Ventana y renderer

SDL_Window *ventana = SDL_CreateWindow("PseudoGames", ...);
SDL_Renderer *renderer = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_ACCELERATED...);

if (!renderer)
    renderer = SDL_CreateRenderer(ventana, -1,
SDL_RENDERER_SOFTWARE);

Primero intenta usar la GPU. Si falla (como en WSL sin OpenGL), cae al software renderer. Ese if (!renderer) es el fallback.

# Fallback:
    Significa plan de escape o segunda opcion o otra opcion que hay si no hay renderizado con GPU se va al CPU.

----

# Líneas 74-79 — Aplicar fullscreen guardado

if (config_get_fullscreen() == 0) {
    SDL_SetWindowFullscreen(ventana,
SDL_WINDOW_FULLSCREEN_DESKTOP);

Si el usuario tenía fullscreen guardado en config, lo aplica apenas arranca.

----

# Líneas 85-87 — Resolución dinámica

int ancho, alto;
SDL_GetWindowSize(ventana, &ancho, &alto);
screen_poweron(renderer, ancho, alto);

No asume 800x600. Lee el tamaño real de la ventana. Eso permite que todo funcione en cualquier resolución.

----

# Líneas 91-95 — La intro (una sola vez en la vida)

if (!intro_ya_vista()) {
    screenPJ_intro(renderer, ventana, ancho, alto);
    marcar_intro_vista();
    screen_transition(renderer, ancho, alto);
}

Consulta progreso.json. Si nunca viste la intro cinemática, la muestra y la marca como vista para siempre.

----

# Líneas 98-130 — EL LOOP PRINCIPAL

int opcion = 0;
do {
    SDL_GetWindowSize(ventana, &ancho, &alto);  // re-leer
resolución
    audio_tick();                               // gestión de
  música

      opcion = screenMenu(renderer, fuente, ancho, alto);  //
  BLOQUEA hasta que el usuario elige

      switch (opcion) {
          case 1: screenTutorial(...); screenLvLs(...); ...
          case 2: screenDoc(...);
          ...
      }
  } while (opcion != 0);

Esto es clave — screenMenu no retorna hasta que el usuario elige algo. Es un loop adentro de un loop. Cuando retorna, el switch llama a la pantalla correspondiente, que también tiene su propio loop interno. Cuando esa termina, vuelve al menú.

----

# Líneas 132-143 — Cleanup

audio_fade_out(800);
screen_poweroff(renderer, ancho, alto);
pom_cleanup();
audio_cleanup();
SDL_DestroyRenderer(renderer);
SDL_DestroyWindow(ventana);
TTF_Quit();
SDL_Quit();
return 0;

Libera TODO en orden inverso a como se creó. En C esto es tu responsabilidad — si no liberás, hay memory leak.

audio_fade_out(800) — apaga la música con un fade de 800ms antes de destruir todo.

screen_poweroff — animación de apagado de pantalla.

Después viene la destrucción en orden: primero el renderer, después la ventana, después TTF, después SDL. El orden importa — no podés destruir SDL antes que el renderer porque el renderer depende de SDL.


