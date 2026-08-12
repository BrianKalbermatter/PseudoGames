#!/bin/bash
# Aca van a ir todas las teclas que se van a usar en los modos!
readKey() {
    KEY=""
    read -rsn1 char
    if [[ "$char" == $'\e' ]]; 
    then
        read -rsn2 -t 0.05 rest
        # Flecha Arriba
        if [[ "$rest" == "[A" ]];
        then
        #Lo que hace el -rsn2 lee solo dos bytes nomas y sino es -rsn1, da igual
        #Lo que hace el -t es que hace un timeout como un sleep
        #cursor_row=$((cursor_row - 1))
            KEY="UP"
##############################################
            # Flecha Abajo
            elif [[ "$rest" == "[B" ]]; 
            then
                KEY="DOWN"
##############################################
            # Flecha Derecha
            elif [[ "$rest" == "[C" ]];
            then
                KEY="RIGHT"
##############################################
            # Flecha Izquierda
            elif [[ "$rest" == "[D" ]]; 
            then
                KEY="LEFT"
##############################################
         else
             KEY="ESC"
         fi
     else
         if [[ "$char" == $'\x7f' ]]; 
         then
             KEY="BACKSPACE"
         elif [[ "$char" == $'\r' ]];
         then
             KEY="ENTER"
         else
            KEY="$char"
         fi
     fi
}
