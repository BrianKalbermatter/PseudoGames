#!/bin/bash
source ./render.sh
source ./modes.sh

# Funcion LOOP El Inicio
loopStart() {
    #---------------LOOP----------------
    while true; do
        renderFrame
        if [[ "$modo" == NORMAL ]];
        then
            modeNormal
        elif [[ "$modo" == "INSERT" ]];
        then
            modeInsert
        fi
        if [[ "$quit" == 1 ]];
        then
            break
        fi
    done
}
