 Estructura del proyecto

  PseudoGames/
  ├── Makefile
  │
  ├── assets/
  │   ├── fonts/
  │   │   ├── main.ttf              ← fuente principal (texto normal)
  │   │   └── mono.ttf              ← fuente monoespaciada (editor/consola)
  │   ├── backgrounds/
  │   │   ├── menu.png
  │   │   ├── mundo_1.png           ← fondo por mundo/capítulo
  │   │   ├── boss.png
  │   │   └── wiki.png
  │   ├── sprites/
  │   │   ├── cursor.png            ← cursor animado del editor
  │   │   ├── recompensas.png       ← iconos de logros
  │   │   └── boss_frames.png       ← sprite sheet del boss
  │   └── ui/
  │       ├── panel_editor.png      ← marco del editor
  │       ├── panel_consola.png     ← marco de la consola
  │       └── barra_vida_boss.png
  │
  ├── data/
  │   ├── niveles.json              ← ya lo tenés
  │   ├── wiki.txt                  ← contenido de tu wiki personal (estilo vimtutor) — FORMATO TXT
  │   │                                Estructura:
  │   │                                {
  │   │                                  "titulo": "AED Wiki",
  │   │                                  "capitulos": [
  │   │                                    {
  │   │                                      "id": 1,
  │   │                                      "titulo": "Estructuras Lineales",
  │   │                                      "temas": [
  │   │                                        {
  │   │                                          "id": "arrays",
  │   │                                          "titulo": "Arrays",
  │   │                                          "descripcion": "...",
  │   │                                          "sintaxis": "...",
  │   │                                          "ejemplo": "..."
  │   │                                        }
  │   │                                      ]
  │   │                                    }
  │   │                                  ]
  │   │                                }
  │   │                                Pantalla: documento largo continuo scrolleable,
  │   │                                buscador con / como vim, secciones con separadores
  │   ├── stdlib.json               ← libreria estandar de pseudocodigo
  │   │                                Funciones ya hechas reutilizables en niveles/proyectos
  │   │                                Idea: el jugador puede usar estas funciones como
  │   │                                "imports" en su pseudocodigo. Ejemplo:
  │   │                                {
  │   │                                  "libreria": "PseudoSTD",
  │   │                                  "version": "1.0",
  │   │                                  "funciones": [
  │   │                                    {
  │   │                                      "id": "sumar",
  │   │                                      "firma": "sumar(a, b)",
  │   │                                      "descripcion": "Retorna la suma de a y b",
  │   │                                      "ejemplo": "resultado = sumar(3, 5)  -> 8"
  │   │                                    },
  │   │                                    {
  │   │                                      "id": "mostrar",
  │   │                                      "firma": "mostrar(texto)",
  │   │                                      "descripcion": "Imprime texto en consola",
  │   │                                      "ejemplo": "mostrar('hola mundo')"
  │   │                                    }
  │   │                                  ]
  │   │                                }
  │   │                                Futuro: con suficientes funciones se podria hacer
  │   │                                una calculadora, manejo de pilas, ordenamiento, etc.
  │   │                                todo en pseudocodigo puro usando la libreria.
  │   ├── sintaxis.json             ← reglas de syntax highlighting para pseudocodigo
  │   │                                Define categorias de palabras y su color ANSI.
  │   │                                En C: al dibujar cada linea del editor, se compara
  │   │                                cada palabra contra este JSON y se le aplica color.
  │   │                                {
  │   │                                  "categorias": [
  │   │                                    {
  │   │                                      "nombre": "keywords",
  │   │                                      "color": "amarillo",
  │   │                                      "palabras": ["si","sino","mientras","para","retornar","funcion"]
  │   │                                    },
  │   │                                    {
  │   │                                      "nombre": "tipos",
  │   │                                      "color": "azul",
  │   │                                      "palabras": ["entero","real","texto","booleano"]
  │   │                                    },
  │   │                                    {
  │   │                                      "nombre": "funciones_std",
  │   │                                      "color": "verde",
  │   │                                      "palabras": ["mostrar","sumar","push","pop"]
  │   │                                    },
  │   │                                    {
  │   │                                      "nombre": "numeros",
  │   │                                      "color": "cyan",
  │   │                                      "patron": "[0-9]+"
  │   │                                    },
  │   │                                    {
  │   │                                      "nombre": "strings",
  │   │                                      "color": "naranja",
  │   │                                      "patron": "\".*\""
  │   │                                    },
  │   │                                    {
  │   │                                      "nombre": "comentarios",
  │   │                                      "color": "gris",
  │   │                                      "patron": "#"
  │   │                                    }
  │   │                                  ]
  │   │                                }
  │   ├── recompensas.json          ← definición de logros/rewards
  │   └── bosses.json               ← config de bosses y sus retos
  │
  ├── saves/
  │   └── progreso.json             ← auto-guardado del jugador
  │
  ├── scripts/
  │   ├── build.sh                  ← compilar el proyecto
  │   └── run.sh                    ← compilar + ejecutar
  │
  └── src/
      ├── main.c                    ← init SDL + game loop principal
      │
      ├── game.c / game.h           ← estado global del juego
      │
      ├── render.c / render.h       ← dibujar fondos, texto, UI
      │
      ├── input.c / input.h         ← captura de teclado + atajos
      │
      ├── editor.c / editor.h       ← el editor de pseudocódigo
      │                                (ya tenés una base)
      │
      ├── consola.c / consola.h     ← la consola toggle
      │
      ├── niveles.c / niveles.h     ← carga de niveles (ya lo tenés)
      │
      ├── wiki.c / wiki.h           ← navegación de la wiki
      │
      ├── boss.c / boss.h           ← lógica de bosses/retos
      │
      ├── progreso.c / progreso.h   ← guardado/carga (ya lo tenés)
      │                                + auto-save con timer
      │
      ├── recompensas.c / recompensas.h  ← sistema de rewards
      │
      ├── cJSON.c / cJSON.h         ← ya lo tenés
      │
      └── defs.h                    ← constantes, enums, structs globales

       Las pantallas del juego (estados)

  ┌─────────────────────────────────────────────────────┐
  │                    MENU PRINCIPAL                     │
  │                                                       │
  │   [1] Jugar        [2] Wiki       [3] Progreso       │
  │   [4] Config       [ESC] Salir                        │
  └──────────┬──────────────┬────────────────────────────┘
             │              │
             ▼              ▼
  ┌──────────────┐  ┌──────────────┐
  │  SELECCION   │  │    WIKI      │
  │  DE NIVEL    │  │              │
  │              │  │  Índice por   │
  │  Cap 1: ...  │  │  tema, como   │
  │  Cap 2: ...  │  │  tu cátedra   │
  │  Boss: ...   │  │              │
  └──────┬───────┘  └──────────────┘
         │
         ▼
  ┌─────────────────────────────────────────────────────┐
  │                   PANTALLA DE JUEGO                   │
  │                                                       │
  │  ┌─── Zona de lectura/enunciado ──────────────────┐  │
  │  │  "Implementá una función que reciba una pila    │  │
  │  │   y devuelva el elemento mínimo..."             │  │
  │  └─────────────────────────────────────────────────┘  │
  │                                                       │
  │  ┌─── Editor de pseudocódigo ─────────────────────┐  │
  │  │  1│ funcion minimo(pila P):                     │  │
  │  │  2│   min = tope(P)                             │  │
  │  │  3│   mientras no vacia(P):                     │  │
  │  │  4│     █  ← cursor                             │  │
  │  │  └─────────────────────────── auto-save: ✓ ──┘  │
  │                                                       │
  │  ┌─── Consola (toggle con `) ────────────────────-─┐  │
  │  │  > ejecutar                                     │  │
  │  │  Resultado: OK ✓                                │  │
  │  │  > ayuda                                        │  │
  │  │  Comandos: ejecutar, limpiar, pista, salir      │  │
  │  └─────────────────────────────────────────────────┘  │
  │                                                       │
  │  [TAB] Editor  [`] Consola  [F1] Wiki  [ESC] Menu     │
  └───────────────────────────────────────────────────--──┘

  Pantalla de Boss

  ┌─────────────────────────────────────────────────────┐
  │  BOSS: "El Árbol Binario Corrupto"                   │
  │  ████████████░░░░░░░░  HP: 60%                       │
  │                                                       │
  │  Reto 3/5: "Encontrá el error en este recorrido      │
  │  inorden y corregilo en menos de 45 segundos"        │
  │                                          ⏱ 00:32     │
  │  ┌─── Editor ─────────────────────────────────────┐  │
  │  │  1│ proc inorden(nodo):                         │  │
  │  │  2│   si nodo != null:                          │  │
  │  │  3│     inorden(nodo.der)   ← ¿error?           │  │
  │  │  4│     mostrar(nodo.dato)                      │  │
  │  │  5│     inorden(nodo.izq)                       │  │
  │  └─────────────────────────────────────────────────┘  │
  │                                                       │
  │  [ENTER] Enviar solución    [F1] Pista (-puntos)     │
  └─────────────────────────────────────────────────────┘

  Flujo de atajos de teclado

  Global (funcionan siempre):
    ESC        → Volver / Menú
    ` (grave)  → Toggle consola
    F1         → Toggle wiki
    TAB        → Foco al editor
    Ctrl+S     → Guardar manual

  En el editor:
    Escribís normalmente (SDL_TEXTINPUT)
    Backspace  → Borrar
    Enter      → Nueva línea
    Flechas    → Mover cursor
    (auto-save después de 2seg sin typear)

  En la consola:
    Escribís comandos: ejecutar, pista, limpiar, salir
    Enter      → Ejecutar comando
    ` (grave)  → Cerrar consola

  Cómo se conecta todo en main.c

  main()
    ├── init_sdl()          ← ventana, renderer, TTF, IMG
    ├── cargar_assets()     ← fondos, fuentes, sprites
    ├── cargar_progreso()   ← saves/progreso.json
    │
    ├── while (corriendo):  ← GAME LOOP
    │     ├── capturar_input()
    │     ├── actualizar_estado()
    │     │     ├── si estado == MENU → lógica menú
    │     │     ├── si estado == JUGANDO → lógica nivel
    │     │     ├── si estado == BOSS → lógica boss
    │     │     ├── si estado == WIKI → navegación wiki
    │     │     └── check_auto_save() ← timer de 2seg
    │     └── renderizar()
    │           ├── dibujar_fondo()
    │           ├── dibujar_panel_activo()  ← editor/consola/wiki
    │           ├── dibujar_ui()            ← barra, atajos, estado
    │           └── SDL_RenderPresent()
    │
    └── cleanup()           ← liberar memoria, cerrar SDL

  El enum central en defs.h

  typedef enum {
      ESTADO_MENU,
      ESTADO_SELECCION_NIVEL,
      ESTADO_JUGANDO,
      ESTADO_BOSS,
      ESTADO_WIKI,
      ESTADO_RESULTADO,
      ESTADO_CONFIG
  } EstadoJuego;

  typedef enum {
      FOCO_EDITOR,
      FOCO_CONSOLA,
      FOCO_WIKI
  } FocoActivo;






NOTA IMPORTANTE A SABER: Yo voy hacer todo para aprender... solo quiero que me ayudes a pensar y analizarlo, para que pueda realizarlo. Voy a tener dudas y quiero que estes como mi profe, mi mentor a mi lado... 
- Siempre al inicio fijate lo que ya tengo armado, lo que ya avance. Preguntame por donde comenzamos.
- Las tareas del dia: dame tareas cortas pero productivas para realizar.
- Ayudame a realizar cada procesos. 
- Dame consejos, educame y ayudame aprender mas... 
- Yo quiero typear y picar codigo todo yo, solo ayudame en lo repetitivo. 
- Ultimo dato, no supongas ni imagines nada... Estoy aprendiendo sintaxis de JSON, C, bash(los tres lenguajes que quiero aprender bien...) no logica desde 0. La logica la se entiendo, tengo que practicarla y ayudame a mentalizar eso...
