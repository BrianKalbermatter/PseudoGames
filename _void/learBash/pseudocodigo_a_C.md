# Pseudocódigo AED → C
## La misma lógica, distinta sintaxis

> La lógica ya la sabés. Lo único nuevo es cómo C escribe eso.

---

## 1. Asignación

**Pseudocódigo**
```
x := 5
precio := 100.0
nombre := "Juan"
```

**C**
```c
int x = 5;
float precio = 100.0;
char nombre[] = "Juan";
```

Diferencias importantes:
- En C necesitás declarar el TIPO antes de la variable (`int`, `float`, `char`)
- En C usás `=` en lugar de `:=`
- Cada línea termina con `;`

---

## 2. Tipos de datos básicos

| Pseudocódigo | C        | Para qué                        |
|--------------|----------|---------------------------------|
| entero       | `int`    | Números enteros: 1, -5, 100     |
| real         | `float`  | Números con decimal: 3.14       |
| real (doble) | `double` | Decimal más preciso             |
| caracter     | `char`   | Una letra: 'a', 'Z'             |
| cadena       | `char[]` | Texto: "hola mundo"             |
| booleano     | `int`    | En C: 0 es falso, 1 es verdadero|

**Ejemplo:**
```c
int edad = 20;
float temperatura = 36.5;
char inicial = 'J';
char nombre[] = "Juan";
int esMayor = 1;   // 1 = verdadero
```

---

## 3. SI ... SINO (if / else)

**Pseudocódigo**
```
SI edad >= 18 ENTONCES
    ESCRIBIR("Mayor de edad")
SINO
    ESCRIBIR("Menor de edad")
FIN SI
```

**C**
```c
if (edad >= 18) {
    printf("Mayor de edad\n");
} else {
    printf("Menor de edad\n");
}
```

Con múltiples condiciones:

**Pseudocódigo**
```
SI nota >= 90 ENTONCES
    ESCRIBIR("Excelente")
SINO SI nota >= 70 ENTONCES
    ESCRIBIR("Aprobado")
SINO
    ESCRIBIR("Reprobado")
FIN SI
```

**C**
```c
if (nota >= 90) {
    printf("Excelente\n");
} else if (nota >= 70) {
    printf("Aprobado\n");
} else {
    printf("Reprobado\n");
}
```

---

## 4. MIENTRAS (while)

**Pseudocódigo**
```
i := 1
MIENTRAS i <= 5 HACER
    ESCRIBIR(i)
    i := i + 1
FIN MIENTRAS
```

**C**
```c
int i = 1;
while (i <= 5) {
    printf("%d\n", i);
    i = i + 1;   // también podés escribir: i++;
}
```

---

## 5. PARA (for)

**Pseudocódigo**
```
PARA i := 1 HASTA 10 HACER
    ESCRIBIR(i)
FIN PARA
```

**C**
```c
for (int i = 1; i <= 10; i++) {
    printf("%d\n", i);
}
```

El `for` tiene 3 partes separadas por `;`:
```
for ( inicio ; condicion ; incremento )
      i=1      i<=10       i++
```

---

## 6. ESCRIBIR (printf)

**Pseudocódigo**
```
ESCRIBIR(x)
ESCRIBIR("El resultado es: ", resultado)
ESCRIBIR(nombre)
```

**C**
```c
printf("%d\n", x);                          // entero
printf("El resultado es: %d\n", resultado); // entero con texto
printf("%s\n", nombre);                     // texto/cadena
printf("%f\n", precio);                     // real/float
printf("%.2f\n", precio);                   // real con 2 decimales
```

Tabla de formatos:
| Tipo     | Formato |
|----------|---------|
| int      | `%d`    |
| float    | `%f`    |
| double   | `%lf`   |
| char     | `%c`    |
| char[]   | `%s`    |

El `\n` es el salto de línea (como un Enter).

---

## 7. LEER (scanf)

**Pseudocódigo**
```
LEER(edad)
LEER(precio)
```

**C**
```c
int edad;
scanf("%d", &edad);

float precio;
scanf("%f", &precio);
```

El `&` antes de la variable es OBLIGATORIO en scanf.
Le dice a C: "guardá el valor en la dirección de memoria de esta variable".
Por ahora aprendételo así. Cuando veamos punteros va a tener todo el sentido.

---

## 8. FUNCION / PROCEDIMIENTO

**Pseudocódigo**
```
FUNCION sumar(a, b): entero
    retornar a + b
FIN FUNCION

PROCEDIMIENTO saludar(nombre)
    ESCRIBIR("Hola, ", nombre)
FIN PROCEDIMIENTO
```

**C**
```c
int sumar(int a, int b) {
    return a + b;
}

void saludar(char nombre[]) {
    printf("Hola, %s\n", nombre);
}
```

- `FUNCION` → tiene tipo de retorno (`int`, `float`, etc.) + `return`
- `PROCEDIMIENTO` → usa `void` (no devuelve nada, no lleva `return`)

Cómo se llama:
```c
int resultado = sumar(3, 5);   // resultado = 8
saludar("Juan");               // imprime: Hola, Juan
```

---

## 9. ARREGLO (array)

**Pseudocódigo**
```
ARREGLO notas[1..5] DE entero
notas[1] := 90
notas[2] := 75
ESCRIBIR(notas[1])
```

**C**
```c
int notas[5];          // 5 posiciones: [0], [1], [2], [3], [4]
notas[0] = 90;         // OJO: C empieza en 0, no en 1
notas[1] = 75;
printf("%d\n", notas[0]);
```

Diferencia clave: en AED el arreglo empieza en 1. En C empieza en 0.

Recorrer un arreglo:
```c
int notas[5] = {90, 75, 80, 60, 95};

for (int i = 0; i < 5; i++) {
    printf("Nota %d: %d\n", i, notas[i]);
}
```

---

## 10. REGISTRO (struct)

**Pseudocódigo**
```
REGISTRO Alumno
    nombre: cadena
    edad:   entero
    nota:   real
FIN REGISTRO

a1: Alumno
a1.nombre := "Juan"
a1.edad   := 20
a1.nota   := 8.5
```

**C**
```c
struct Alumno {
    char nombre[50];
    int edad;
    float nota;
};

struct Alumno a1;
a1.edad = 20;
a1.nota = 8.5;
// para texto se usa strcpy:
strcpy(a1.nombre, "Juan");

printf("%s tiene %d años\n", a1.nombre, a1.edad);
```

---

## 11. PILA (stack manual)

Una pila en AED es un concepto. En C la implementás vos con un struct + array + índice.

**La idea:**
```
TOPE →  [ 3 ]   ← último en entrar, primero en salir
        [ 7 ]
        [ 1 ]   ← primero en entrar
```

**C**
```c
#define MAX 100

struct Pila {
    int datos[MAX];
    int tope;
};

// Inicializar
void inicializar(struct Pila *p) {
    p->tope = -1;   // -1 significa vacía
}

// Push: agregar elemento
void push(struct Pila *p, int valor) {
    p->tope++;
    p->datos[p->tope] = valor;
}

// Pop: sacar elemento
int pop(struct Pila *p) {
    int valor = p->datos[p->tope];
    p->tope--;
    return valor;
}

// Está vacía?
int vacia(struct Pila *p) {
    return p->tope == -1;
}
```

Uso:
```c
struct Pila p;
inicializar(&p);
push(&p, 1);
push(&p, 7);
push(&p, 3);
printf("%d\n", pop(&p));   // imprime 3
printf("%d\n", pop(&p));   // imprime 7
```

---

## Estructura básica de un programa en C

Todo programa en C tiene esta forma mínima:

```c
#include <stdio.h>    // para poder usar printf y scanf

int main() {

    // tu código acá

    return 0;
}
```

- `#include <stdio.h>` → importa la librería de entrada/salida
- `int main()` → el punto de entrada, como el inicio del pseudocódigo
- `return 0` → le dice al sistema que el programa terminó bien

---

## Ejemplo completo: nivel 1 de PseudoGames en C puro

El enunciado: dado C, R, A y N, calcular `P = C * (1 + R) ^ (N - A)`

**Pseudocódigo AED**
```
LEER(C, R, A, N)
anios := N - A
P := C * (1 + R) ^ anios
ESCRIBIR(P)
```

**C**
```c
#include <stdio.h>
#include <math.h>     // para pow()

int main() {
    float C, R;
    int A, N;

    scanf("%f %f %d %d", &C, &R, &A, &N);

    int anios = N - A;
    float P = C * pow(1 + R, anios);

    printf("%.2f\n", P);

    return 0;
}
```

Compilar y ejecutar:
```bash
gcc programa.c -o programa -lm
./programa
```

El `-lm` le dice al compilador que use la librería matemática (donde está `pow`).

---

## Para recordar siempre

1. La lógica es la misma — solo cambia cómo se escribe
2. En C todo tiene tipo: `int`, `float`, `char`, `void`
3. Cada instrucción termina con `;`
4. Los bloques se delimitan con `{ }` en lugar de `FIN SI`, `FIN MIENTRAS`
5. Los arreglos empiezan en 0
6. El `&` en `scanf` es obligatorio (por ahora memorízalo, después lo vas a entender con punteros)
7. `void` = procedimiento, tipo = función con retorno
