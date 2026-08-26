#!/bin/bash
# pack.sh — empaqueta PseudoGames para release
# Uso: ejecutar desde la carpeta PseudoGames/
set -e

OUTPUT="pseudogames-linux.tar.gz"

# Crear saves/ vacio si no existe
mkdir -p saves

# Copiar libs SDL3 necesarias.
# Se resuelve el symlink con -L: adentro del tar tiene que ir el archivo
# real, no un enlace que apunte a /usr/lib de la maquina que empaqueto.
mkdir -p lib
cp -L /usr/lib/libSDL3.so.0       lib/
cp -L /usr/lib/libSDL3_ttf.so.0   lib/
cp -L /usr/lib/libSDL3_mixer.so.0 lib/
cp -L /usr/lib/libSDL3_image.so.0 lib/

echo "Empaquetando $OUTPUT..."

tar -czf "../$OUTPUT" \
    --exclude="*Zone.Identifier*" \
    --exclude="*.exe" \
    --transform="s|scripts/launcher.sh|launcher.sh|" \
    aed \
    lib/ \
    assets/ \
    data/ \
    saves/ \
    Frankly/ \
    scripts/launcher.sh

echo "Listo: ../$OUTPUT"
echo "Ahora subilo al release con:"
echo "  gh release upload <tag> ../$OUTPUT --clobber"
