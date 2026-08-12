#!/bin/bash
#Banderas
#  ┌──────┬──────────────────────────────────────┐ 
#  │ Flag │               Para qué               │
#  ├──────┼──────────────────────────────────────┤
#  │ -p   │ Mostrar mensaje antes de leer        │
#  ├──────┼──────────────────────────────────────┤
#  │ -s   │ Ocultar lo que escribe (contraseñas) │
#  ├──────┼──────────────────────────────────────┤
#  │ -t 5 │ Timeout de 5 segundos                │
#  ├──────┼──────────────────────────────────────┤
#  │ -n 1 │ Leer solo 1 caracter                 │
#  ├──────┼──────────────────────────────────────┤
#  │ -a   │ Guardar en array                     │
#  └──────┴──────────────────────────────────────┘

huevo=""
arina=""
BOOLEANO=""
echo "Coloca true o false segun tengas el ingrediente: "
read -p "Tengo huevo?: " huevo
echo "Tengo arina?: "
read arina
#Encerrar todo en un bucle para que cuando de E: te puedas volver a colocar en el primer if!
if [ "$huevo" = true ] && [ "$arina" = true ]; 
then
    echo "Tengo todos los ingredientes y puedo comenzar con la elavoracion de la torta!"
    
    if [ "$huevo" = false ] && [ "$arina" = false ]; 
    then
        echo "No tenes ni huevo, ni arina."
    
        if [ "$huevo" = true ] && [ "$arina" = false ]; 
        then
            echo "Tenes huevo pero no arina!"
        fi
    fi
    else
        echo "E: Solo son verdadero o falso!"
fi


