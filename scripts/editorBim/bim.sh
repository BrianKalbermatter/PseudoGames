#!/bin/bash

source ./keys.sh
source ./modes.sh
source ./render.sh
source ./loop.sh
# raw -> no espera Enter para enviar input, cada tecla llega inmediato.
# -echo -> no muestra lo que escibis en pantalla (controlo lo que se va a mostrar)

# stty sane -> Si tu script crashea sin ejecutar esto, tu terminal queda roto (escribis stty sane a ciegas para arreglarlo).

# Pregunta: Cada byte es una tecla ?
# read -rsn1 char -> Lee exactamente 1 byte: Lo que hace esto es leer un caracter del teclado
# -r -> No interpreta backslashes(\).
# -s -> Silencioso (no echo)
# -n1 -> lee 1 caracter
#
# trap -> Para limpiar al salir, pase lo que pase
echo "⠀⠀⠀⠀⠀⠀⠀⠀⠀       ⣀⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡠⢒⣩⣥⠡⠀⠀⠀⠀⠌⣬⣍⡒⢄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡴⠫⣶⣿⣿⣿⣧⡑⢄⡠⢊⣼⣿⣿⣿⣦⠝⢆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⡅⣿⣦⡙⢿⣿⣿⣿⣦⣴⣿⣿⣿⡿⢋⣴⣿⢨⢣⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⡜⣼⡇⣿⣿⣿⣦⡹⣿⣿⣿⣿⣿⣿⢋⣴⣿⣿⣿⢸⣧⢣⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⣿⠗⠛⠻⠿⣿⣷⡘⡿⠟⠻⢿⢃⣾⣿⠿⠟⠛⠺⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠸⠟⠀⣀⢀⣄⣀⠈⠙⡗⣰⠿⠿⣆⢺⠋⠁⣀⣤⡀⣀⠈⠻⠇⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢀⢦⣍⡛⠶⣤⠤⠜⣀⣤⠀⣶⣶⠀⣤⣐⠣⢤⣤⠶⢛⣩⡶⡀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠈⢧⡙⢿⣷⡆⣶⣿⡿⢋⢸⣿⣿⡆⡙⢿⣿⣶⢰⣾⡿⢋⡼⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢙⠦⣍⠰⣿⡟⣡⣿⢸⣿⣿⡇⣿⡌⢻⣿⠆⣩⠶⡋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⢌⢷⢈⢳⣌⠁⣤⡌⢸⣿⣿⡇⢡⣤⠈⣡⡞⡁⡾⡡⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡎⣧⡹⢰⣿⣷⣈⠛⠛⣡⣾⣿⡆⢏⣼⢸⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⣿⣇⢸⣿⣿⣿⣿⣿⣿⣿⣿⡇⣸⣿⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠃⣿⣿⣄⠙⢉⣩⣉⣉⣍⡉⠋⣨⣿⣿⠘⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠐⣄⠻⣿⣿⣷⡆⢀⣀⣀⡀⢰⣾⣿⣿⠟⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠳⣌⠻⢋⡤⠉⠉⠉⠁⣤⡙⠟⣡⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⠿⣿⡇⠀⠀⠀⠀⢸⣿⠟⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠁⠀⠀⠀⠀⠈⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
                 |BIM.|

==> Presiona cualquier Tecla!
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀"
read -rsn1


cleanup() {
    echo "Volviendo a su modo Normal"
    stty sane # Restaura al terminal a su modo normal
}


#VARIABLES
cursor_row=0 #en que linea del buffer estas (empieza en 0)
cursor_col=0 # en que columna estas (empieza en 0)
modo="NORMAL" # Es para iniciar en Normal!



# Esta funcion lo que hace es lee un archivo y mete cada linea en una posicion de un array
# $1 es el primer argumento que le pasas al script en el que estas analizando el archivo... como por ejemplo /bin.sh archivoPrueba.txt
# -t quita el \n salto de linea del final de cada linea.
# buffer es el array donde se guardan
#   Si tu archivo tiene:
#  hola
#  mundo
#  chau
# Después de mapfile, el array queda:
#  buffer[0] = "hola"
#  buffer[1] = "mundo"
#  buffer[2] = "chau"
mapfile -t buffer < "$1"


FILAS=$(tput lines)
COLS=$(tput cols)
trap cleanup EXIT
stty raw -echo # Esto pone en modo raw la terminal!
printf '\e[2J\e[H'    # limpia una sola vez antes del loop



# - \e[2J -> Borra todo
# - \e[H -> Cursor a posicion 1,1



mensaje=""

CLEAN_SCREEN='\e[H'
BG='\e[48;2;40;40;40m'
FG='\e[38;2;235;219;178m'
AMARILLO='\e[38;2;250;189;47m'
BARRA_BG='\e[48;2;80;73;69m'
RESET='\e[0m'



# Solo llama al loopStart porque es donde esta toda la logica y luego llama a las diferentes funciones dentro
loopStart

