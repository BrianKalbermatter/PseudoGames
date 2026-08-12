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

    //En C esto es una asignacion
    char sec[] = "Brian"; // Cadena
    char = 'Kalbermatter';// String
    // En C no son true o false sino int... enteros en binario(1 y 0)
    // int esMayor = 1;
    

    // Bucle Mientras
    //definir una variable de indice cualquiera en este caso j
    int j = 1; // Va a ir del 1 al 5
    while(j <= 5){ // aca va a preguntar j=1 es menor que 5 o igual? Si es verdadero entonces entra en el bucle. Sino sale y va al siguiente bucle
        printf("%d\n", j);
        j = j + 1; // Esto es lo mismo que j++;
    }

    int i = 0;
    while (sec[i] != '\0') {
        printf("%c", sec[i]);   // %c = un solo caracter
        i++; // En el bucle de arriba esta para que incremente de otra manera
    }

    // Condicion Simple
    if(edad >= 18){
        printf("Mayor de edad\n");
    }
    else
        printf("Menor de edad\n");
    }
    // Condicion Multiple
    if (nota >= 90){
        printf("Excelente\n");
    }
    else if(nota >= 70) {
        printf("Aprobado/a\n");
    }
    else{
        printf("Reprobado\n");
    }
    // Bucle PARA(for)
    // En el for en C tiene 3 partes separadas por ';'
    // for( inicio ; condicion ; incremento )
    //       i=1       i<=10        i=i+1
  return 0;
}


