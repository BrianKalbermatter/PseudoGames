Plan: Guía paso a paso para construir "bim" - Editor tipo Vim en Bash

Contexto

El usuario quiere construir desde cero un editor de texto tipo Vim en Bash
puro como reto de aprendizaje. Solo necesita guía, NO código escrito por mí.

Enfoque

Guiarlo en 7 etapas incrementales, donde cada etapa produce algo funcional
que puede probar. Iré explicando conceptos y dándole pistas, pero él escribe
todo el código.

Archivo: /root/VimMon/retosBash/bim.sh

---
Etapa 1: Terminal en modo raw + leer teclas

Objetivo: Entender cómo Bash puede tomar control total del terminal.
- Conceptos: stty raw -echo, stty sane, read -rsn1, códigos de escape
- Tareas:
  a. Hacer un script que ponga el terminal en modo raw
  b. Leer teclas una por una y mostrar su código (para entender qué manda cada tecla)
  c. Detectar ESC, flechas, Enter, Backspace
  d. Restaurar el terminal al salir (trap EXIT)
- Pista clave: Las flechas envían 3 bytes: \e[A, \e[B, \e[C, \e[D

Etapa 2: Dibujar en pantalla con ANSI

Objetivo: Controlar qué se muestra y dónde.
- Conceptos: \e[H (home), \e[2J (clear), \e[row;colH (posicionar cursor), colores ANSI
- Tareas:
  a. Limpiar pantalla y posicionar cursor
  b. Dibujar texto en posiciones específicas
  c. Obtener tamaño del terminal (tput lines, tput cols o $LINES/$COLUMNS)
  d. Dibujar una barra de estado en la última línea
- Pista clave: printf es más eficiente que echo -e para escape codes

Etapa 3: Buffer de texto + abrir archivos

Objetivo: Cargar un archivo en memoria y mostrarlo.
- Conceptos: arrays de Bash (declare -a), IFS, mapfile/readarray
- Tareas:
  a. Cargar archivo línea por línea en un array
  b. Dibujar las líneas en pantalla (con scroll si hay más líneas que pantalla)
  c. Mostrar números de línea
  d. Manejar el caso de archivo vacío o nuevo
- Pista clave: mapfile -t buffer < "$archivo" carga todo de golpe

Etapa 4: Modo NORMAL - Navegación

Objetivo: Moverse por el archivo como en Vim.
- Conceptos: variable de modo, posición cursor (fila/columna), offset de scroll
- Tareas:
  a. Implementar h/j/k/l para moverse
  b. Limitar movimiento a los bordes del texto
  c. Scroll vertical cuando el cursor sale de la pantalla visible
  d. Implementar gg, G, 0, $, w
- Pista clave: Necesitás 4 variables: cursor_row, cursor_col, offset_row, offset_col

Etapa 5: Modo INSERT - Editar texto

Objetivo: Poder escribir y modificar texto.
- Conceptos: manipulación de strings en Bash (${var:0:pos}, ${var:pos})
- Tareas:
  a. i entra a modo INSERT, ESC vuelve a NORMAL
  b. Insertar caracteres en la posición del cursor
  c. Backspace para borrar
  d. Enter para partir línea
  e. a, o, O como modos de entrada alternativos
- Pista clave: Para insertar en medio de una línea: "${linea:0:col}${char}${linea:col}"

Etapa 6: Modo COMMAND + Guardar/Salir

Objetivo: Implementar la línea de comandos :.
- Conceptos: leer input en la barra de estado, escribir archivos
- Tareas:
  a. : entra a modo COMMAND, mostrar : en la barra de estado
  b. Leer el comando caracter por caracter
  c. Implementar :w (guardar), :q (salir), :wq, :q!
  d. dd en modo normal (borrar línea)
  e. x (borrar caracter), r (reemplazar caracter)
  f. Trackear si el archivo fue modificado (dirty flag)

Etapa 7: Extras - Búsqueda y syntax highlighting

Objetivo: Darle el toque final.
- Conceptos: regex en Bash, colores ANSI por token
- Tareas:
  a. :/patron para buscar texto, n para siguiente coincidencia
  b. Resaltar keywords de Bash (if/then/else/fi, for/do/done, function, etc.)
  c. Colorear strings entre comillas y comentarios (#)
  d. Undo básico (guardar estado anterior de la línea modificada)
  e. Manejar SIGWINCH (resize de terminal)

---
Verificación

- Cada etapa debe poder ejecutarse: chmod +x bim.sh && ./bim.sh archivo.txt
- Al final: abrir un .sh, editarlo, guardarlo, y verificar que el contenido es correcto








# Las organice por dias asi que ya hice el dia 1: Sigo con los otros!
Día 1 — Etapa 1: Terminal en modo raw + leer teclas

  1. Script que ponga el terminal en modo raw (stty raw -echo)
  2. Leer teclas una por una y mostrar su código ASCII
  3. Detectar ESC, flechas (3 bytes), Enter, Backspace
  4. Restaurar el terminal al salir (trap EXIT + stty sane)

  Día 2 — Etapa 2: Dibujar en pantalla con ANSI

  1. Limpiar pantalla (\e[2J) y posicionar cursor (\e[H)
  2. Dibujar texto en posiciones específicas (\e[row;colH)
  3. Obtener tamaño del terminal (tput lines/tput cols)
  4. Dibujar una barra de estado en la última línea

  Día 3 — Etapa 3: Buffer de texto + abrir archivos

  1. Cargar archivo línea por línea en un array (mapfile -t)
  2. Dibujar las líneas en pantalla (con scroll si hay más líneas que pantalla)
  3. Mostrar números de línea
  4. Manejar archivo vacío o nuevo

  Día 4 — Etapa 4: Modo NORMAL - Navegación

  1. Implementar h/j/k/l para moverse
  2. Limitar movimiento a los bordes del texto
  3. Scroll vertical cuando el cursor sale de la pantalla visible
  4. Implementar gg, G, 0, $, w

  Día 5 — Etapa 5: Modo INSERT - Editar texto

  1. i entra a INSERT, ESC vuelve a NORMAL
  2. Insertar caracteres en la posición del cursor
  3. Backspace para borrar
  4. Enter para partir línea
  5. a, o, O como entradas alternativas

  Día 6 — Etapa 6: Modo COMMAND + Guardar/Salir

  1. : entra a modo COMMAND, mostrar : en la barra de estado
  2. Leer el comando carácter por carácter
  3. Implementar :w, :q, :wq, :q!
  4. dd en modo normal (borrar línea)
  5. x (borrar carácter), r (reemplazar carácter)
  6. Trackear si el archivo fue modificado (dirty flag)

  Día 7 — Etapa 7: Extras - Búsqueda y syntax highlighting

  1. /patron para buscar texto, n para siguiente coincidencia
  2. Resaltar keywords de Bash (if/then/else/fi, for/do/done, etc.)
  3. Colorear strings entre comillas y comentarios (#)
  4. Undo básico (guardar estado anterior de la línea)
  5. Manejar SIGWINCH (resize de terminal)

  ---
  Total: 7 días, 30 tareas.

  Cada día terminás con algo que funciona y podés probar con ./bim.sh
  archivo.txt. Cuando termines una etapa, avisame y te doy las pistas y conceptos
   para la siguiente.



