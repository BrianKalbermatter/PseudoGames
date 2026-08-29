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
saves/         progreso y configuracion del jugador
screenPJ/      subproyecto con su propio Makefile
scripts/       launcher, empaquetado y release
```

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
