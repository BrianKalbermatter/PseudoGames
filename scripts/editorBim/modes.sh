#!/bin/bash
source ./keys.sh
# Funcion de modoNormal
#
# Aca va a ir la logica de movimiento del cursor, i, q.
modeNormal() {
    #Llama a la funcion de keys.sh
    readKey
##############################################
            if [[ "$KEY" == "UP" ]]
            then
            #Lo que hace el -rsn2 lee solo dos bytes nomas y sino es -rsn1, da igual
            #Lo que hace el -t es que hace un timeout como un sleep
                cursor_row=$((cursor_row - 1))
            if ((cursor_row < 0)); 
            then 
                cursor_row=0; 
            fi 
                # Flecha Abajo
                elif [[ "$KEY" == "DOWN" ]]; 
                then
                    cursor_row=$((cursor_row + 1))
                    if ((cursor_row > ${#buffer[@]} - 1)); 
                    then
                        cursor_row=$((${#buffer[@]} - 1)); 
                    fi
##############################################
            # Flecha Derecha
            elif [[ "$KEY" == "RIGHT" ]];
            then
                cursor_col=$((cursor_col + 1))
                if ((cursor_col > ${#buffer[$cursor_row]})); 
                then 
                    cursor_col=${#buffer[$cursor_row]}; 
                fi
##############################################
            # Flecha Izquierda
            elif [[ "$KEY" == "LEFT" ]]; 
            then
                cursor_col=$((cursor_col - 1))
                if ((cursor_col < 0)); 
                then 
                    cursor_col=0; 
                fi
            
##############################################
            elif [[ "$KEY" == "i" ]]; 
            then
                modo="INSERT"
            elif [[ "$KEY" == "q" ]];
            then
                quit=1
            fi
}
# Funcion de modoInsert
# Aca va ESC para volver, y despues escribir texto
modeInsert() {
    readKey
    echo "TECLA: char=[$KEY] char=[$(printf '%d' "'$KEY")]" >> /tmp/bim_debug.log
    if [[ "$KEY" == "ESC" ]]; then
        modo="NORMAL"
    # backspace:
    elif [[ "$KEY" == "BACKSPACE" ]]; then
        if (( cursor_col > 0 )); then
            buffer[$cursor_row]="${buffer[$cursor_row]:0:$((cursor_col-1))}${buffer[$cursor_row]:$cursor_col}"
                cursor_col=$((cursor_col - 1))
        fi
    # enter:    
    elif [[ "$KEY" == "ENTER" ]]; then
        #El buffer como un array de líneas:
        #
        #  buffer[0] = "hola mundo"
        #  buffer[1] = "chau"
        #  buffer[2] = "adios"
        #
        #  Cuando apretás Enter en el medio de "hola mundo" con el cursor en la
        #   posición 4:
        #
        #  "hola| mundo"
        #        ↑ cursor_col=4
        #
        #  Necesitás convertir una línea en dos:
        #  buffer[0] = "hola"
        #  buffer[1] = " mundo"   ← línea nueva insertada
        #  buffer[2] = "chau"     ← se corrió para abajo
        #  buffer[3] = "adios"    ← se corrió para abajo
        # Teniendo en cuenta esto: 
        #       El buffer es un array definido que se carga con mapfile, por ejemplo selecciono un archivo.txt cualquiera y lo que hace eso es que cada linea dentro de ese archivo se guarde.

        local antes="${buffer[$cursor_row]:0:$cursor_col}"
        # Agarra todo lo que esta antes del cursor. En el ejemplo: "hola". Lo guarda buffer

        local despues="${buffer[$cursor_row]:$cursor_col}"
        # Agarra todo lo que esta despues del cursor. En el ejemplo: "mundo". Lo guarda en el buffer

        buffer[$cursor_row]="$antes"
        # La linea actual queda solo con la parte de antes: "hola"

        buffer=(${buffer[@]:0:$((cursor_row+1))} "$despues" ${buffer[@]:$((cursor_row+1))})
        # Imaginá que cursor_row=0 y el buffer tiene:

        # buffer[0] = "hola"    ← ya modificado en el paso anterior
        # buffer[1] = "chau"
        # buffer[2] = "adios"

        # La línea arma un nuevo array en 3 pedazos:

        # ${buffer[@]:0:$((cursor_row+1))}   →  buffer[0]="hola"          (desde 0, toma 1 elemento)
        # "$despues"                         →  " mundo"                   (la línea nueva)
        # ${buffer[@]:$((cursor_row+1))}     →  buffer[1]="chau", buffer[2]="adios"  (el resto)

        # Resultado:
        # buffer[0] = "hola"
        # buffer[1] = " mundo"   ← insertada
        # buffer[2] = "chau"     ← se corrió
        # buffer[3] = "adios"    ← se corrió

        cursor_row=$((cursor_row + 1)) # El cursor baja a la linea nueva.
        cursor_col=0 # El cursor va al principio de esa linea
    else

        # El cursor está en la posición 7 (cursor_col=7).
        # Apretás la tecla "X":
        # antes  = "hola a"          → ${buffer[$cursor_row]:0:7}
        # char   = "X"
        # despues = " como estas?"   → ${buffer[$cursor_row]:7}
        # Resultado:
        # "hola aX como estas?"
        # Y el cursor se mueve a cursor_col=8, justo después de la X.
        # ---
        # Eso es todo lo que hace ese else. Simple pero elegante.
        buffer[$cursor_row]="${buffer[$cursor_row]:0:$cursor_col}${KEY}${buffer[$cursor_row]:$cursor_col}"
        cursor_col=$((cursor_col + 1))
        
    fi
}















# Funcion de modoInsert
# Aca va ESC para volver, y despues escribir texto
modeInsert() {
    read -rsn1 char
    echo "TECLA: char=[$char] char=[$(printf '%d' "'$char")]" >> /tmp/bim_debug.log
    if [[ "$char" == $'\e' ]]; then
        modo="NORMAL"
    # backspace:
    elif [[ "$char" == $'\x7f' ]]; then
        if (( cursor_col > 0 )); then
            buffer[$cursor_row]="${buffer[$cursor_row]:0:$((cursor_col-1))}${buffer[$cursor_row]:$cursor_col}"
                cursor_col=$((cursor_col - 1))
        fi
    # enter:    
    elif [[ "$char" == $'\r' ]]; then
        #El buffer como un array de líneas:
        #
        #  buffer[0] = "hola mundo"
        #  buffer[1] = "chau"
        #  buffer[2] = "adios"
        #
        #  Cuando apretás Enter en el medio de "hola mundo" con el cursor en la
        #   posición 4:
        #
        #  "hola| mundo"
        #        ↑ cursor_col=4
        #
        #  Necesitás convertir una línea en dos:
        #  buffer[0] = "hola"
        #  buffer[1] = " mundo"   ← línea nueva insertada
        #  buffer[2] = "chau"     ← se corrió para abajo
        #  buffer[3] = "adios"    ← se corrió para abajo
        # Teniendo en cuenta esto: 
        #       El buffer es un array definido que se carga con mapfile, por ejemplo selecciono un archivo.txt cualquiera y lo que hace eso es que cada linea dentro de ese archivo se guarde.

        local antes="${buffer[$cursor_row]:0:$cursor_col}"
        # Agarra todo lo que esta antes del cursor. En el ejemplo: "hola". Lo guarda buffer

        local despues="${buffer[$cursor_row]:$cursor_col}"
        # Agarra todo lo que esta despues del cursor. En el ejemplo: "mundo". Lo guarda en el buffer

        buffer[$cursor_row]="$antes"
        # La linea actual queda solo con la parte de antes: "hola"

        buffer=(${buffer[@]:0:$((cursor_row+1))} "$despues" ${buffer[@]:$((cursor_row+1))})
        # Imaginá que cursor_row=0 y el buffer tiene:

        # buffer[0] = "hola"    ← ya modificado en el paso anterior
        # buffer[1] = "chau"
        # buffer[2] = "adios"

        # La línea arma un nuevo array en 3 pedazos:

        # ${buffer[@]:0:$((cursor_row+1))}   →  buffer[0]="hola"          (desde 0, toma 1 elemento)
        # "$despues"                         →  " mundo"                   (la línea nueva)
        # ${buffer[@]:$((cursor_row+1))}     →  buffer[1]="chau", buffer[2]="adios"  (el resto)

        # Resultado:
        # buffer[0] = "hola"
        # buffer[1] = " mundo"   ← insertada
        # buffer[2] = "chau"     ← se corrió
        # buffer[3] = "adios"    ← se corrió

        cursor_row=$((cursor_row + 1)) # El cursor baja a la linea nueva.
        cursor_col=0 # El cursor va al principio de esa linea
    else

        # El cursor está en la posición 7 (cursor_col=7).
        # Apretás la tecla "X":
        # antes  = "hola a"          → ${buffer[$cursor_row]:0:7}
        # char   = "X"
        # despues = " como estas?"   → ${buffer[$cursor_row]:7}
        # Resultado:
        # "hola aX como estas?"
        # Y el cursor se mueve a cursor_col=8, justo después de la X.
        # ---
        # Eso es todo lo que hace ese else. Simple pero elegante.
        buffer[$cursor_row]="${buffer[$cursor_row]:0:$cursor_col}${char}${buffer[$cursor_row]:$cursor_col}"
        cursor_col=$((cursor_col + 1))
        
    fi
}

