# PseudoGames — Estructura y Visión

## Árbol del proyecto

```
PseudoGames/
│
├── assets/
│   └── fonts/
│       ├── main.ttf        ← Cascadia Code Regular
│       └── bold.ttf        ← Cascadia Code Bold
│
├── data/
│   ├── niveles.json        ✓ hecho
│   ├── wiki.txt            ✓ hecho  ← FORMATO TXT (no JSON)
│   ├── stdlib.json         pendiente
│   ├── sintaxis.json       pendiente
│   ├── recompensas.json    pendiente
│   └── bosses.json         pendiente
│
├── src/                    ← juego en C + SDL2
│   ├── main.c              ✓ base
│   ├── editor.c/h          ✓ base
│   ├── niveles.c/h         ✓ hecho
│   ├── progreso.c/h        ✓ hecho
│   ├── ui.c/h              ✓ base
│   └── cJSON.c/h           ✓ hecho
│
├── scripts/
│   └── editorBim/          ← bim: editor standalone de pseudocódigo
│       ├── bim.sh          todo junto aún — refactorizando
│       ├── render.sh       ✓ renderFrame() extraída
│       ├── keys.sh         extraída pero con 2 bugs
│       ├── modes.sh        vacío — pendiente
│       ├── loop.sh         vacío — pendiente
│       └── pomodoro.sh     existe
│
└── saves/
    └── progreso.json       pendiente
```

---

## bim (editor standalone)

```
$ bim archivo.bim           ← modo libre
  - syntax highlighting de pseudocódigo
  - extensión .bim
  - futuro: interpreter + debugger

embebido en PseudoGames     ← lo usan los niveles y el boss internamente
```

---

## Flujo de pantallas (C + SDL2)

```
MENU
  - Limpio, sin distracciones
  - Cascadia Code, colores gruvbox
  - Navegable con mouse o teclado (flechas + ENTER)

SELECCION DE NIVEL / ESTUDIO
  - Acá arranca el POMODORO
  - Siempre visible mientras estudiás/jugás

NIVEL / JUEGO
  - editor bim + consola + pomodoro siempre visible

BOSS
  - editor bim + consola + pomodoro siempre visible

WIKI
PROGRESO

MODO LIBRE
  - editor bim sin restricciones
  - escribís pseudocódigo libre
  - futuro: corre el código con el interpreter
  - como tener un lenguaje completo dentro del juego
```

---

## Diseño

```
Font    →  Cascadia Code (todo el juego)
Fondo   →  #282828
Texto   →  #ebdbb2
Acento  →  #fabd2f
```
