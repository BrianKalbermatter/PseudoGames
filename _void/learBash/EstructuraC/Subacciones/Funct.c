// Ejercicio 1.2.1
/*
AMBIENTE
    - Si se coloca como parametro a los valores asi, ya llegan de afuera los valores desde quien llama la funcion. Por eso el LEER no tiene sentido. De esta manera le decis al usuario que ingrese valores que ya deberian venir dados.
    - La funcion declara que retorna ENTERO pero no retorna nada porque no fue dado los datos desde antes.

    FUNCION algoritmo_cuadrado (var1: ENTERO, var2: ENTERO ): ENTERO ES
        LEER(var1);
        LEER(var2);
        ESCRIBIR("El numero de var1 elevado al cuadrado es: ", var1 ** 2);
        ESCRIBIR("El numero de var2 elevado al cuadrado es: ", var2 ** 2);
    FIN_FUNCION
    PROCESO
        algoritmo_cuadrado;
















*El "retorno" en este estilo de pseudocódigo

  En lugar de escribir RETORNAR, el valor de retorno se indica asignando al nombre de la función:

  Suma := Total;

  Esa línea es el retorno. Le estás diciendo: "el resultado de esta función es Total".

  Es el estilo clásico de Pascal — en el que está basado este pseudocódigo. No existe RETORNAR, existe
   asignar al nombre de la función.

  ---
  Lo que hace cada parte

  Funcion Suma (x, y: Entero): Entero;   ← recibe x, y → devuelve Entero
      Ambiente
          Total: Entero;                  ← variable LOCAL (solo existe acá adentro)
      Algoritmo
          Total := x + y;                 ← calcula
          Suma := Total;                  ← esto ES el retorno
  Fin;

  Y en el algoritmo principal:
  Escribir('La suma es: ', Suma(b, c));   ← llama la función y usa su valor devuelto

  ---
  Tu ejercicio anterior tenía este problema

  En tu Funct.c no asignabas al nombre de la función. Si seguís este estilo, el retorno correcto
  sería:

  algoritmo_cuadrado := var1 ** 2;   ← así retornarías

  ¿Querés reescribir tu ejercicio con este estilo ahora?

❯ o sea es lo que quiero que salga por el culo?. Llamando a la funcion recursivamente := asignandole
el valor que quiera devolver que seria var1**2 y var2**2?

● Sí exactamente — es lo que "sale por el culo" de la función, el valor que devuelve.

  Pero ojo con algo importante:

  Suma := Total NO es una llamada recursiva. Es solo una asignación especial que este estilo de
  pseudocódigo usa para decir "esto es lo que devuelvo". No se ejecuta la función de nuevo.

  ---
  El problema de tu ejercicio original

  Tu función quería devolver dos cosas:
  - var1 ** 2
  - var2 ** 2

  Pero una función solo puede devolver UN valor. Eso es una limitación fundamental.

  Si querés mostrar ambos cuadrados, tenés dos opciones:

  Opción A — Hacer dos funciones separadas:
  Funcion cuadrado(x: Entero): Entero;
      Algoritmo
          cuadrado := x ** 2;
  Fin;
  Y en el algoritmo principal llamarla dos veces.

  Opción B — Que no sea Función sino Procedimiento, y que escriba adentro (sin retornar nada).

  ---
  ¿Con cuál de las dos opciones querés reescribir tu ejercicio?
