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
├── src/                    ← juego en C + SDL3
│   ├── main.c              ✓ base
│   ├── editor.c/h          ✓ base
│   ├── niveles.c/h         ✓ hecho
│   ├── progreso.c/h        ✓ hecho
│   ├── ui.c/h              ✓ base
│   └── cJSON.c/h           ✓ hecho
│
├── scripts/
│   └── editorBim/          (los .sh del editor se borraron: ver xasol)
│       └── pomodoro.sh     lo lanza pomodoro_bg.c
│
└── saves/
    └── progreso.json       pendiente
```

---

## xasol — el editorXL (2026-08-29)

Reemplaza a **bim/editorBim**, que eran dos programas: un editor escrito en
Bash (`scripts/editorBim/*.sh`) y un panel en C que lo corría adentro de un
pseudo-terminal y pintaba sus secuencias ANSI.

**Por qué se reemplazó.** Ese camino tenía tres capas entre la tecla y el píxel:

```
tecla SDL ──> PTY ──> bash ──> secuencias ANSI ──> parser ──> píxel
```

Y el parser ANSI de `editorBim.c` sólo entendía mover el cursor y borrar (`J`,
`H`, `A`, `B`, `C`, `D`, `K`). **No entendía `m`, que es el código de color**,
así que el resaltado de sintaxis era imposible de raíz: bash podía emitir los
colores y se descartaban en el camino. Además el C había quedado en SDL2 y ya
no compilaba.

xasol tiene una capa: `tecla SDL ──> buffer ──> píxel`.

```
src/xasol.c           el editor: buffer, modos, teclas, undo, guardar, dibujo
src/xasol_sintaxis.c  los colores — se los pide a `paed --tokens`
src/xasol.h           la interfaz de panel y el contrato del resaltado
```

**La sintaxis la trae PAED.** xasol no sabe pseudocódigo: no tiene lista de
palabras clave ni lexer. Corre `paed --tokens archivo.paed`, que devuelve una
línea por token con `linea ⇥ col ⇥ largo ⇥ rol ⇥ texto`, y elige un color por
rol. Con dos gramáticas, un día el editor pinta de verde una palabra que el
intérprete rechaza; con una, no puede pasar.

**Los colores son los de Helix**, no los de la terminal. PAED tiene dos paletas
y no son la misma: `paed --colores` pinta con los nombres de `data/sintaxis.json`
traducidos a xterm-256, y Helix con los scopes de `helix/queries/highlights.scm`
repartidos en `helix/tema.toml.ejemplo`. xasol sigue la de Helix, que es contra
la que uno tiene el ojo hecho al escribir PAED:

| | |
|---|---|
| `ACCION` / `PROCESO` / `AMBIENTE` | verde `#00ff66` |
| `SI` / `SINO` / `ENTONCES` | oro `#ffd700` |
| `MIENTRAS` / `PARA` / `REPETIR` | azul `#3377ff` |
| `Y` / `O` / `NO` / `MOD` / `DIV` | naranja `#ff8800` |
| lo que definís vos | magenta `#ff00ff` |

El **rojo queda libre a propósito**: es el color del error — lo que subraya
`paed-lsp` y lo que sale en la consola. Si el código también tuviera rojo,
competirían.

### El dibujo, y por qué parpadeaba

El texto se dibuja **por tramos de un mismo color**, no carácter por carácter.
Cada `xa_text()` crea una superficie, la sube a una textura y destruye las dos;
hacerlo por carácter son unas **2400 texturas por frame** en una pantalla llena,
y encima sobre un renderer por software. Por tramos son ~15 por línea: el mismo
dibujo, dos órdenes de magnitud menos de trabajo. Se puede agrupar así porque la
fuente es monoespaciada — la columna N siempre cae en el mismo x.

La otra mitad era `xasol_token_en()`, que se consulta una vez por carácter de la
pantalla y recorría los 4000 tokens del archivo en cada consulta. Ahora hay un
**índice por línea** (`idx_desde` / `idx_cuantos` en `XasolResaltado`) armado de
una pasada, aprovechando que paed emite los tokens en orden.

Lo que **no** era el problema: correr `paed --tokens`. Medido, tarda 2 ms — nada
frente a un frame de 16 ms. Por eso el debounce de 150 ms alcanza y sobra.

**Lo que se conservó de bim**: el buffer como array de líneas, los dos modos,
el cursor como par (fila, columna) acotado a lo que existe, y la barra de
estado. Era la forma correcta y es la de vi.

**Lo que se agregó**: guardar (bim cargaba el archivo y nunca lo escribía —
editabas y perdías todo), resaltado, undo, scroll, y los movimientos de
palabra, línea y archivo.

### Teclas

| NORMAL | |
|---|---|
| `h j k l` / flechas | mover |
| `w` / `b` | palabra adelante / atrás |
| `0` / `$` | principio / fin de línea |
| `gg` / `G` | principio / fin del archivo |
| `i` / `a` | insertar acá / después del cursor |
| `o` / `O` | abrir línea debajo / arriba |
| `x` / `dd` | borrar carácter / línea |
| `u` | deshacer |
| `Ctrl+S` | guardar |
| `q` | volver al picker (`Shift+Q` sin guardar) |

| INSERT | |
|---|---|
| `Esc` | volver a NORMAL |
| texto, Enter, Backspace, Tab | escribir |

El cursor dice el modo sin mirar la barra: **bloque** en NORMAL (se para sobre
un carácter, que es sobre el que actúan los comandos), **barra fina** en INSERT
(se mete entre dos, que es donde va a entrar lo que escribas).

### La terminal (bash de verdad)

Abajo del editor corre **bash**, no un panel de salida. Se abre sola con el
panel, en la raíz de PseudoGames — que es donde están `saves/`, el Makefile y
todo lo demás, así que los comandos relativos funcionan.

| | |
|---|---|
| `F12` | pasar el teclado del editor a la terminal y volver |
| `F10` | guardar y escribirle `paed <archivo>` a la terminal |
| arrastrar el divisor | cambiarle el alto |

Con el foco en la terminal andan `Ctrl+C`, `Tab` para autocompletar, las
flechas para el historial y todo lo demás: es una terminal.

**Cómo se sabe dónde van a ir las teclas antes de apretarlas:** el divisor y
el `>_` se ponen naranjas cuando la terminal tiene el foco, y el cursor pasa de
hueco a lleno.

**Por qué un PTY y no dos pipes.** Un programa como bash no escribe "texto":
escribe texto mezclado con órdenes — "poné el cursor en 1,1", "de acá en
adelante en verde". Con pipes, bash detecta que no hay terminal y se apaga: sin
prompt, sin colores, sin edición de línea. El pseudo-terminal es lo que lo
convence de que sí la hay.

**Lo que se aprendió de editorBim.** Aquel también usaba un PTY, y su parser
entendía siete códigos: mover el cursor y borrar. No entendía `m`, que es el
del **color**, y eso fue lo que lo condenó. Acá cada celda del buffer guarda su
carácter **y** su color, y el parser maneja SGR desde el principio.

`F10` no lanza un proceso propio: le **escribe el comando** a bash, como si lo
hubieras tipeado. Por eso un `LEER` se contesta escribiendo, igual que en una
terminal normal.

Verificado sin abrir la ventana, con un programa que usa el módulo suelto:
bash arranca, `echo` y `pwd` corren, `paed saves/Cuadrado.paed` para en el
`LEER`, se le escribe `7` y responde `El cuadrado de 7 es 49.` Con colores.

### El dibujo, y por qué parpadeaba### El dibujo, y por qué parpadeaba

El texto se dibuja **por tramos de un mismo color**, no carácter por carácter.
Cada `xa_text()` crea una superficie, la sube a una textura y destruye las dos;
hacerlo por carácter son unas **2400 texturas por frame** en una pantalla llena,
y encima sobre un renderer por software. Por tramos son ~15 por línea: el mismo
dibujo, dos órdenes de magnitud menos de trabajo. Se puede agrupar así porque la
fuente es monoespaciada — la columna N siempre cae en el mismo x.

La otra mitad era `xasol_token_en()`, que se consulta una vez por carácter de la
pantalla y recorría los 4000 tokens del archivo en cada consulta. Ahora hay un
**índice por línea** (`idx_desde` / `idx_cuantos` en `XasolResaltado`) armado de
una pasada, aprovechando que paed emite los tokens en orden.

Lo que **no** era el problema: correr `paed --tokens`. Medido, tarda 2 ms — nada
frente a un frame de 16 ms. Por eso el debounce de 150 ms alcanza y sobra.

**Lo que se conservó de bim**: el buffer como array de líneas, los dos modos,
el cursor como par (fila, columna) acotado a lo que existe, y la barra de
estado. Era la forma correcta y es la de vi.

**Lo que se agregó**: guardar (bim cargaba el archivo y nunca lo escribía —
editabas y perdías todo), resaltado, undo, scroll, y los movimientos de
palabra, línea y archivo.

### Teclas

| NORMAL | |
|---|---|
| `h j k l` / flechas | mover |
| `w` / `b` | palabra adelante / atrás |
| `0` / `$` | principio / fin de línea |
| `gg` / `G` | principio / fin del archivo |
| `i` / `a` | insertar acá / después del cursor |
| `o` / `O` | abrir línea debajo / arriba |
| `x` / `dd` | borrar carácter / línea |
| `u` | deshacer |
| `Ctrl+S` | guardar |
| `q` | volver al picker (`Shift+Q` sin guardar) |

| INSERT | |
|---|---|
| `Esc` | volver a NORMAL |
| texto, Enter, Backspace, Tab | escribir |

El cursor dice el modo sin mirar la barra: **bloque** en NORMAL (se para sobre
un carácter, que es sobre el que actúan los comandos), **barra fina** en INSERT
(se mete entre dos, que es donde va a entrar lo que escribas).

### La consola (F10)

Una barra abajo del editor, como la terminal de VSCode: se abre pegada al piso,
se agarra del borde de arriba y se estira.

| | |
|---|---|
| `F10` | guardar, compilar y correr — abre la consola con la salida |
| `Ctrl+J` | mostrar / ocultar la consola |
| arrastrar el divisor | cambiarle el alto |
| rueda del mouse, `Shift+PgUp` / `PgDn` | recorrer la salida |

**F10 guarda antes de correr**, y no es un atajo: `paed` lee del disco, así que
correr sin guardar ejecutaría la versión anterior del programa. Ver un error que
ya arreglaste, o no ver el que acabás de escribir, es peor que no tener el botón.

A la izquierda va un margen propio con el símbolo `>_`, del mismo modo que los
números de línea en el editor. **El símbolo dice cómo salió**: verde si el
programa terminó bien, rojo si no.

El comando que corre es `timeout 5 paed '<archivo>' < /dev/null 2>&1`, y las
tres partes atajan algo distinto:

- `timeout 5` corta un programa que no termina. Sin esto, un `MIENTRAS` infinito
  cuelga PseudoGames entero: la consola espera a que el proceso cierre y el
  proceso no cierra nunca. (PAED además se protege solo, cortando a los 2
  millones de pasos — son dos redes independientes.)
- `< /dev/null` — un `LEER` sin nadie que tipee se quedaría esperando para
  siempre. Con la entrada cerrada falla y sigue.
- `2>&1` — los errores de paed salen por stderr, y son justamente lo que uno
  viene a leer acá.

Igual que con el resaltado: **no hay un intérprete adentro del editor**. Lo que
ves en la consola es exactamente lo que verías corriendo `paed` en la terminal.

---

## Flujo de pantallas (C + SDL3)

```
MENU
  - Limpio, sin distracciones
  - Cascadia Code, colores gruvbox
  - Navegable con mouse o teclado (flechas + ENTER)

SELECCION DE NIVEL / ESTUDIO
  - Acá arranca el POMODORO
  - Siempre visible mientras estudiás/jugás

NIVEL / JUEGO
  - editor xasol + consola + pomodoro siempre visible

BOSS
  - editor xasol + consola + pomodoro siempre visible

WIKI
PROGRESO

MODO LIBRE
  - editor xasol sin restricciones
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
