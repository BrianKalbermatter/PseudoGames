#!/bin/bash
renderFrame ()
{
    printf "${BG}" # Fondo gruvbox
    printf "${FG}" # Texto gruvbox
        
    # Este printf() limpia la pantalla y pone el cursor arriba
    printf "${CLEAN_SCREEN}"
        # - \e[2J -> Borra todo
        # - \e[H -> Cursor a posicion 1,1
        #Empieza el SHOW!
        #Imprime un mensaje
        #printf "${AMARILLO}==> %s${FG}" "$mensaje"
        # En un for en bash necesita el formato completo con (()) y las tres partes: inicio, condicion, incremento
        # Se empieza en la linea 0 del array(cada linea de un archivo de codigo o de texto o de lo que sea es un 
        # array, una lista de cosas, caracteres, numeros, etc.)
        # i<${#buffer[@]} : Mientras i sea menor que la cantidad de lineas
        
        for ((i=0; i<${#buffer[@]}; i++)); do
            #$((i+1)) te da el número de línea (porque i arranca en 0 pero las líneas se muestran desde 1).
            printf "\e[$((i+1));1H %3d %s" "$((i+1))" "${buffer[$i]}"
        done
        
        #Para un string y lo imprima '' y el "" para comando, ejemplo:
        # El %-${COLS}s le dice a printf: "imprimi este string y rellenalo con espacios hasta ocupar $COLS caracteres". 
        # Asi la barra invertida cubre toda la linea.
        printf "\e[${FILAS};1H${BARRA_BG}${FG}%-${COLS}s${RESET}"  " | $modo | " # ir a la ultima fila
        # Esto fuerza al cursor a ser visible. Algunas terminales ocultan cuando dibujas mucho...
        printf '\e[?25h'
        printf "\e[$((cursor_row+1));$((cursor_col+5))H"
        # Porque +5? Porque las primeras columnas las ocupan los numeros de linea (1). Ajusta ese numero segun cuanto 
        # espacio le diste a los numeros.
}
