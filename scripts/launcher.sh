#!/bin/bash
# launcher.sh — auto-updater de PseudoGames
# Descargalo una sola vez y usalo para jugar siempre.
# Requiere: curl, tar

REPO="BrianKalbermatter/VimMon"
ASSET="pseudogames-linux.tar.gz"
VERSION_FILE=".version"

CURRENT=$(cat "$VERSION_FILE" 2>/dev/null || echo "ninguna")

echo "╔══════════════════════════════╗"
echo "║     PseudoGames Launcher     ║"
echo "╚══════════════════════════════╝"
echo "Version local: $CURRENT"

# Verificar si hay internet y obtener ultima version
LATEST=$(curl -sf "https://api.github.com/repos/$REPO/releases/latest" \
         | grep '"tag_name"' | cut -d'"' -f4)

if [ -z "$LATEST" ]; then
    echo "Sin conexion — iniciando con version local..."
elif [ "$LATEST" = "$CURRENT" ]; then
    echo "Ya tenes la ultima version ($CURRENT)."
else
    echo "Nueva version disponible: $LATEST"
    echo "Descargando actualizacion..."
    curl -L --progress-bar \
        "https://github.com/$REPO/releases/download/$LATEST/$ASSET" \
        -o "$ASSET"

    echo "Instalando..."
    tar -xzf "$ASSET"
    rm "$ASSET"

    # Crear carpeta saves si no existe (nunca se sobreescribe)
    mkdir -p saves

    echo "$LATEST" > "$VERSION_FILE"
    echo "Actualizado a $LATEST."
fi

# Verificar que el binario existe
if [ ! -f "./aed" ]; then
    echo "Error: no se encontro el binario 'aed'."
    echo "Borra el archivo .version y ejecuta el launcher de nuevo para descargar todo."
    exit 1
fi

export LD_LIBRARY_PATH="./lib:$LD_LIBRARY_PATH"
exec ./aed
