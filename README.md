# PseudoGames

**El editor/IDE para aprender PAED**, el pseudocódigo de AED, en forma de juego:
niveles, wiki, pomodoro, bosses y un editor con resaltado de sintaxis.

El binario se llama `aed`.

## Qué es y qué no es

Es una **aplicación**, no parte del lenguaje. Son tres proyectos separados y la
dependencia va en una sola dirección:

```
VimMon (el OS)  ──lanza──>  PseudoGames (este)  ──usa──>  PAED (el lenguaje)
```

- **PAED corre sin esto.** `paed programa.paed` no necesita ningún editor.
- **VimMon puede lanzar cualquier otro editor.** El comando `edit` abre lo que
  diga `VIMMON_IDE`; PseudoGames es el que viene por defecto, no el único
  posible.
- **PAED no sabe que este proyecto existe.** Al revés sí: el verificador de
  soluciones corre el intérprete de PAED.

## Construir

```bash
make            # el editor: build/*.o -> aed   (necesita SDL3, ttf, mixer, image)
make windows    # PseudoGames.exe, cross-compile con mingw-w64
make clean
```

Tiene que correrse **parado en esta carpeta**: `assets/`, `data/` y `saves/` se
cargan con rutas relativas.

```bash
./aed
```

## Qué hay adentro

```
src/           el editor: pantallas, editor de texto, consola, audio, config
  xasol.c        xasol, el editorXL: editor modal de PAED
  xasol_sintaxis.c  sus colores, pedidos a `paed --tokens`
assets/        fuentes, iconos, audio
data/          niveles.json, wiki.txt y el material de estudio
solutions/     soluciones de referencia de cada nivel
saves/         progreso, configuracion y los .paed que escribis
  secuencias_paed/<programa>/<variable>.txt   la cinta de cada SECUENCIA
screenPJ/      subproyecto con su propio Makefile
scripts/       launcher, empaquetado y release
```

## Las secuencias: de donde salen sus datos

Un programa con `sec: SECUENCIA DE CARACTERES;` necesita SUS DATOS para correr,
y esos datos no estan en el codigo: son el enunciado. Al apretar **F10**, xasol
le pregunta a paed que secuencias declara el programa y cuales todavia no tienen
cinta, y te las pide una por una en una ventanita antes de correr.

Lo que se tipea es la cinta entera, en una linea, con las celdas **una al lado
de la otra y sin separadores**. `hola mundo` son DIEZ celdas y el espacio es una
de ellas: `AVZ` la va a devolver como cualquier otra. La ventanita muestra abajo
la cinta separada en celdas justamente para que eso se vea mientras se escribe.

Se guarda en `saves/secuencias_paed/<programa>/<variable>.txt`. Son dos nombres
y cada uno contesta una pregunta: el del ARCHIVO es el de la variable, y el de
la CARPETA es el del programa — que es lo que ATA esa cinta a ese .paed. `sec`
es el nombre mas comun que hay: puede haber ochenta sec.txt en el proyecto y
ninguno se pisa con otro.

La ruta no la inventa el editor: se la dice paed, en la cuarta columna de
`paed --secuencias <archivo>`. Si el editor la armara por su cuenta, el dia que
el lenguaje la cambie escribiria en un lugar donde nadie va a mirar.

## Dependencia con PAED

El verificador de soluciones ejecuta el intérprete de PAED sobre lo que escribió
el jugador y compara la salida. Se apoya en que `paed` esté **instalado y
alcanzable por PATH** — lo llama por nombre, no por ruta:

```bash
cd ../paed && sudo make install          # /usr/local/bin, que ya está en el PATH
```

o sin sudo, agregando el directorio al PATH una sola vez:

```bash
cd ../paed && make install PREFIX=$HOME/.local
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
```

Comprobalo con `command -v paed`. Si no está, el verificador no se cuelga: la
shell responde `command not found`, el caso da NO SUPERADO y listo.

## Desde VimMon

```
vimmon> edit
```

`plugins/editor/editor.c` lo compila solo si hace falta, entra a esta carpeta y
lo ejecuta. Para abrir otro editor en su lugar:

```bash
VIMMON_IDE=/usr/bin/vim build/vimmon
```
