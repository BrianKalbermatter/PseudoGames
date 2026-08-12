// En paed
//      ACCION ... ES
//          AMBIENTE
//          PROCESO
//      FIN_ACCION
//      En C:

// Si querias agregar un imprimir es #include
#include <stdio.h>

int main() {
    // todo el programa
    // printf("Hola mundo");

/*
 * NFDS = '\0'
 * 
 * - Si quiero colocar el hola uno alado del otro y sin indice
 *
 *  
    char sec[] = "hola";
    printf("%s", sec);

    Salida:
    hola
    * */
    char sec[] = "hola";
    int i = 0;

    while (sec[i] != '\0') {
        printf("%c", sec[i]);   // %c = un solo caracter
        i++;
    }

  return 0;
}


