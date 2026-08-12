#!/bin/bash
# pack.sh — empaqueta PseudoGames para release
# Uso: ejecutar desde la carpeta PseudoGames/
set -e

OUTPUT="pseudogames-linux.tar.gz"

# Crear saves/ vacio si no existe
mkdir -p saves

# Copiar libs SDL2 necesarias
mkdir -p lib
cp /usr/lib/libSDL2-2.0.so.0     lib/
cp /usr/lib/libSDL2_ttf-2.0.so.0  lib/
cp /usr/lib/libSDL2_mixer-2.0.so.0 lib/

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
