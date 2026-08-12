#!/bin/bash
# pack-windows.sh — empaqueta PseudoGames para Windows
# Uso: ejecutar desde la carpeta PseudoGames/
set -e

OUTPUT="pseudogames-windows.zip"
WIN_LIBS="win-libs/dll"

# Verificar que el exe fue compilado
if [ ! -f "PseudoGames.exe" ]; then
    echo "Error: no se encontro PseudoGames.exe"
    echo "Compila primero con: make windows"
    exit 1
fi

# Crear saves/ vacio si no existe
mkdir -p saves

echo "Empaquetando $OUTPUT..."

zip -r "../$OUTPUT" \
    PseudoGames.exe \
    "$WIN_LIBS/SDL2.dll" \
    "$WIN_LIBS/SDL2_ttf.dll" \
    assets/ \
    data/ \
    saves/ \
    Frankly/ \
    --exclude "*Zone.Identifier*" \
    --exclude "*.sh" \
    --exclude "*.o"

echo "Listo: ../$OUTPUT"
echo "Ahora subilo al release con:"
echo "  gh release upload <tag> ../$OUTPUT --clobber"
