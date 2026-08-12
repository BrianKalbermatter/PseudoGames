# Big O — Complejidad algorítmica

> La complejidad mide cómo crece el tiempo o la memoria que necesita un algoritmo
> a medida que crece la cantidad de datos de entrada (n).

---

## Reglas base para calcular Big O

- Ignorás las constantes:        `O(2n)` → `O(n)`
- Ignorás los términos menores:  `O(n² + n)` → `O(n²)`
- Siempre analizás el peor caso salvo que se indique lo contrario.
- Si hay dos bucles independientes: `O(n) + O(n)` → `O(n)`
- Si hay bucles anidados: `O(n) * O(n)` → `O(n²)`

---

## Tabla de complejidades (de mejor a peor)

| Notación      | Nombre              | Para n=10 | Para n=100 | Para n=1000 |
|---------------|---------------------|-----------|------------|-------------|
| O(1)          | Constante           | 1         | 1          | 1           |
| O(log n)      | Logarítmica         | 3         | 7          | 10          |
| O(n)          | Lineal              | 10        | 100        | 1.000       |
| O(n log n)    | Lineal-logarítmica  | 33        | 664        | 9.966       |
| O(n²)         | Cuadrática          | 100       | 10.000     | 1.000.000   |
| O(n³)         | Cúbica              | 1.000     | 1.000.000  | 10⁹         |
| O(2ⁿ)         | Exponencial         | 1.024     | ~10³⁰      | imposible   |
| O(n!)         | Factorial           | 3.628.800 | imposible  | imposible   |

---

## O(1) — Constante

El algoritmo siempre tarda lo mismo, sin importar cuántos datos haya.

```
// Acceder a un elemento de un arreglo por índice
x := A[5];

// Asignar un valor
total := 0;

// Leer o escribir una variable
ESCRIBIR(nombre);
```

**Cuándo aparece:** acceso directo a posición, asignación, operaciones aritméticas simples, acceso a campo de registro.

---

## O(log n) — Logarítmica

Cada paso descarta la mitad de los datos. Con 1000 elementos necesitás ~10 pasos.

```
// Búsqueda binaria
iz := 1
de := N
cen := (iz + de) DIV 2
MIENTRAS (iz < de) Y (A[cen] <> x) HACER
    SI A[cen] > x ENTONCES
        de := cen - 1
    SINO
        iz := cen + 1
    FIN_SI
    cen := (iz + de) DIV 2
FIN_MIENTRAS
```

**Cuándo aparece:** búsqueda binaria, algoritmos divide y vencerás, árboles balanceados.
**Requisito:** los datos deben estar ordenados.

---

## O(n) — Lineal

Si duplicás los datos, duplicás el tiempo. Recorre todo una vez.

```
// Búsqueda lineal
PARA i := 1 a N HACER
    SI A[i] = x ENTONCES
        ESCRIBIR(i)
    FIN_SI
FIN_PARA

// Recorrido de lista enlazada
aux := Prim
MIENTRAS aux <> nil HACER
    ESCRIBIR(*aux.Dato)
    aux := *aux.Prox
FIN_MIENTRAS

// Recorrido de archivo
MIENTRAS NO FDA(arch) HACER
    LEER(arch, reg)
FIN_MIENTRAS
```

**Cuándo aparece:** recorrido de arreglo, búsqueda lineal, lectura completa de archivo, recorrido de lista.

---

## O(n log n) — Lineal-logarítmica

Mejor complejidad posible para ordenar datos por comparación.

```
// Merge Sort (concepto)
// Divide el arreglo en mitades → O(log n) divisiones
// En cada nivel compara todos los elementos → O(n) por nivel
// Total: O(n log n)

// QuickSort (caso promedio)
// También O(n log n) en promedio, O(n²) en peor caso
```

**Cuándo aparece:** algoritmos de ordenamiento eficientes (MergeSort, QuickSort, HeapSort).
**Nota:** los ordenamientos vistos en AED (burbuja, selección, inserción) son O(n²), no O(n log n).

---

## O(n²) — Cuadrática

Por cada elemento, recorrés todos los demás. Un bucle dentro de otro.

```
// Ordenamiento burbuja
MIENTRAS NO Bandera HACER
    Bandera := Verdadero
    PARA j := 1 a n-1 HACER         // O(n)
        SI a[j] < a[j+1] ENTONCES
            // intercambio
            Bandera := Falso
        FIN_SI
    FIN_PARA                         // x O(n) = O(n²)
FIN_MIENTRAS

// Ordenamiento selección directa
PARA i := 1 a n-1 HACER             // O(n)
    PARA j := i+1 a n HACER         // O(n)
        // busca el máximo
    FIN_PARA
FIN_PARA                             // = O(n²)

// Ordenamiento inserción directa
PARA i := 2 a n HACER               // O(n)
    MIENTRAS (j > 0) Y (x < a[j]) HACER  // O(n)
        // desplaza
    FIN_MIENTRAS
FIN_PARA                             // = O(n²)
```

**Cuándo aparece:** dos bucles anidados que recorren n elementos cada uno.
**Problema:** con 10.000 datos hacés 100 millones de operaciones.

---

## O(n³) — Cúbica

Tres bucles anidados. Rara vez deseable.

```
// Multiplicación de matrices (concepto)
PARA i := 1 a n HACER
    PARA j := 1 a n HACER
        PARA k := 1 a n HACER
            C[i][j] := C[i][j] + A[i][k] * B[k][j]
        FIN_PARA
    FIN_PARA
FIN_PARA
```

**Cuándo aparece:** operaciones sobre matrices cuadradas, algunos algoritmos de grafos (Floyd-Warshall).

---

## O(2ⁿ) — Exponencial

Cada elemento nuevo duplica el trabajo. Crece tan rápido que es inviable para n > 30.

```
// Fibonacci recursivo sin memoización
FUNCION fibonacci(E n: ENTERO): ENTERO
    PROCESO
        SI n = 0 ENTONCES
            fibonacci := 0;
        SINO
            SI n = 1 ENTONCES
                fibonacci := 1;
            SINO
                fibonacci := fibonacci(n-1) + fibonacci(n-2);
                // dos llamadas por nivel → duplica el trabajo
            FIN_SI
        FIN_SI
FIN_FUNCION

// Subconjuntos de un conjunto (todos los subconjuntos posibles)
// Para n elementos hay 2^n subconjuntos
```

**Cuándo aparece:** recursión con dos llamadas por nivel sin memoización, fuerza bruta sobre subconjuntos.
**Solución:** usar programación dinámica o memoización para reducirlo a O(n).

---

## O(n!) — Factorial

El peor de todos. Con 12 elementos ya son 479 millones de operaciones.

```
// Permutaciones: todas las formas de ordenar n elementos
// Para n=10 → 3.628.800 permutaciones
// Para n=20 → 2.432.902.008.176.640.000 permutaciones

// Problema del viajante por fuerza bruta:
// Probar todos los caminos posibles entre n ciudades
```

**Cuándo aparece:** fuerza bruta sobre todas las permutaciones posibles.
**Nunca** se usa en sistemas reales para n grande.

---

## Casos: Mejor, Promedio y Peor

Un mismo algoritmo puede tener distintas complejidades según los datos de entrada.

| Algoritmo          | Mejor caso | Caso promedio | Peor caso |
|--------------------|------------|---------------|-----------|
| Búsqueda lineal    | O(1)       | O(n)          | O(n)      |
| Búsqueda binaria   | O(1)       | O(log n)      | O(log n)  |
| Inserción directa  | O(n)       | O(n²)         | O(n²)     |
| Selección directa  | O(n²)      | O(n²)         | O(n²)     |
| Burbuja            | O(n)       | O(n²)         | O(n²)     |
| QuickSort          | O(n log n) | O(n log n)    | O(n²)     |
| MergeSort          | O(n log n) | O(n log n)    | O(n log n)|
| Factorial recursivo| O(n)       | O(n)          | O(n)      |
| Fibonacci recursivo| O(2ⁿ)      | O(2ⁿ)         | O(2ⁿ)     |

> **Burbuja mejor caso O(n):** si el arreglo ya está ordenado, la Bandera corta en la primera vuelta.
> **Inserción mejor caso O(n):** si el arreglo ya está ordenado, el MIENTRAS interno nunca entra.

---

## Complejidad espacial (memoria)

No es solo el tiempo: también importa cuánta RAM extra usa el algoritmo.

| Algoritmo             | Espacio extra |
|-----------------------|---------------|
| Recorrido de arreglo  | O(1)          |
| Búsqueda binaria      | O(1)          |
| Ordenamientos in-place (burbuja, selección, inserción) | O(1) |
| Recursión (factorial) | O(n) — pila de llamadas |
| Fibonacci recursivo   | O(n) — pila de llamadas |
| MergeSort             | O(n) — arreglo auxiliar |
| Lista enlazada de n nodos | O(n)      |

---

## Complejidades en AED (lo que vas a ver en la cursada)

| Operación / Algoritmo            | Complejidad temporal |
|----------------------------------|----------------------|
| Acceso directo a variable        | O(1)                 |
| Asignación                       | O(1)                 |
| LEER / ESCRIBIR una variable     | O(1)                 |
| Recorrido de arreglo             | O(n)                 |
| Búsqueda lineal                  | O(n)                 |
| Búsqueda lineal con centinela    | O(n)                 |
| Búsqueda binaria                 | O(log n)             |
| Inserción directa                | O(n²)                |
| Selección directa                | O(n²)                |
| Burbuja / Intercambio directo    | O(n²)                |
| Recorrido de archivo (FDA)       | O(n)                 |
| Corte de control                 | O(n)                 |
| Mezcla de 2 archivos             | O(n + m)             |
| Actualización secuencial         | O(n + m)             |
| Actualización indexada           | O(1) por operación   |
| Factorial recursivo              | O(n)                 |
| Suma recursiva                   | O(n)                 |
| Fibonacci recursivo              | O(2ⁿ)               |
| Recorrido de lista enlazada      | O(n)                 |
| Inserción al inicio de lista     | O(1)                 |
| Búsqueda en lista enlazada       | O(n)                 |

---

## Cómo leer un algoritmo y calcular su Big O

```
// Paso 1: identificá los bucles

PARA i := 1 a n HACER          // O(n)
    x := A[i] * 2              // O(1) dentro del bucle
FIN_PARA
// Total: O(n) * O(1) = O(n)


// Paso 2: bucles anidados se multiplican

PARA i := 1 a n HACER          // O(n)
    PARA j := 1 a n HACER      // O(n)
        ESCRIBIR(A[i] + A[j])  // O(1)
    FIN_PARA
FIN_PARA
// Total: O(n) * O(n) = O(n²)


// Paso 3: bucles independientes se suman y ganó el mayor

PARA i := 1 a n HACER          // O(n)
    ESCRIBIR(A[i])
FIN_PARA

PARA j := 1 a n HACER          // O(n)
    ESCRIBIR(A[j] * 2)
FIN_PARA
// Total: O(n) + O(n) = O(2n) → O(n)


// Paso 4: recursión → contás cuántas veces se llama a sí misma

FUNCION factorial(n)            // se llama n veces → O(n)
FUNCION fibonacci(n)            // se llama 2 veces por nivel → O(2ⁿ)
```

---

## Notaciones relacionadas (no solo Big O)

| Notación | Nombre    | Qué mide                          |
|----------|-----------|-----------------------------------|
| O(n)     | Big O     | Cota superior (peor caso)         |
| Ω(n)     | Big Omega | Cota inferior (mejor caso)        |
| Θ(n)     | Big Theta | Cota exacta (mejor = peor caso)   |
| o(n)     | Little o  | Cota superior estricta (no exacta)|

> En la práctica y en entrevistas técnicas siempre se habla de **Big O**.
