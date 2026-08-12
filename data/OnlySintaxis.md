# OnlySintaxis — Referencia rápida AED

---

## CAPITULO 1: Estructura básica

### Estructura ACCION
```
ACCION nombre_del_algoritmo ES
    AMBIENTE
        // variables y constantes
    PROCESO
        // logica
FIN_ACCION
```

### Declaración de variables
```
a: ENTERO;           // numero entero, sin decimales
b: REAL;             // numero con decimales
c: BOOLEANO;         // Verdadero o Falso
nombre: AN(20);      // alfanumerico de hasta 20 caracteres
codigo: N(5);        // numerico de hasta 5 cifras
MAX = 100;           // constante: valor fijo
```

### Asignación
```
a := 23;
b := a + 5;
c := (a - b) / 2;
```

### Comentarios
```
a: ENTERO;   // esto es un comentario
```

### Ejemplo completo
```
ACCION mi_primer_algoritmo ES
    AMBIENTE
        a: ENTERO;
        b: ENTERO;
    PROCESO
        LEER(a)
        b := a * 2;
        ESCRIBIR(b)
FIN_ACCION
```

---

## CAPITULO 2: Funciones y Procedimientos

### FUNCION
```
FUNCION nombre_funcion(E param1: TIPO; E param2: TIPO): TIPO_RETORNO
    AMBIENTE
        // variables locales
    PROCESO
        // logica
        nombre_funcion := valor_a_retornar;
FIN_FUNCION
```

### Llamada a FUNCION
```
resultado := nombre_funcion(arg1, arg2);
```

### PROCEDIMIENTO
```
PROCEDIMIENTO nombre_proc(E param1: TIPO; S param2: TIPO)
    AMBIENTE
        // variables locales
    PROCESO
        // logica
FIN_PROCEDIMIENTO
```

### Llamada a PROCEDIMIENTO
```
nombre_proc(arg1, arg2);
```

### Modos de parámetros
```
E  → Entrada        (entra, no se modifica afuera)
S  → Salida         (sale modificado)
ES → Entrada/Salida (entra y puede salir modificado)
```

---

## CAPITULO 3: Registros

### Declaración de REGISTRO
```
NombreRegistro = REGISTRO
    campo1: TIPO;
    campo2: TIPO;
FIN_REGISTRO
```

### Variable de tipo registro
```
AMBIENTE
    a: NombreRegistro;
```

### Acceso a campos
```
a.campo1 := valor;
ESCRIBIR(a.campo2);
```

### Arreglo de registros
```
AMBIENTE
    alumnos: arreglo[1..30] de Alumno;
    i: ENTERO;
PROCESO
    PARA i := 1 a 30 HACER
        LEER(alumnos[i].legajo);
        LEER(alumnos[i].nombre);
    FIN_PARA
```

### Registro anidado
```
e.fecha_ingr.dia  := 15;
e.fecha_ingr.mes  := 3;
e.fecha_ingr.anio := 2024;
```

---

## CAPITULO 4: Archivos

### Declaración
```
arch: archivo de TIPO;
```

### ABRIR / CERRAR
```
ABRIR(archivo, lectura)
ABRIR(archivo, escritura)
ABRIR(archivo, actualizacion)
CERRAR(archivo)
```

### LEER / ESCRIBIR con archivo
```
LEER(archivo, variable)
ESCRIBIR(archivo, variable)
```

### Patrón FDA (lectura completa)
```
ABRIR(arch, lectura);
LEER(arch, reg);
MIENTRAS NO FDA(arch) HACER
    // procesar reg
    LEER(arch, reg);
FIN_MIENTRAS
CERRAR(arch);
```

---

## CAPITULO 5: Corte de Control

### Esqueleto genérico corte_N
```
SUBACCION corte_N ES
    corte_N-1          // cierra nivel inferior primero (excepto corte_1)
    // emitir resultado del nivel N
    // acumular al nivel superior
    // reiniciar totales de este nivel
    // resguardar nueva clave
FIN_SUBACCION
```

### Tratar_Corte (3 niveles)
```
SUBACCION tratar_corte ES
    SI r.clave3 <> Reg3 ENTONCES
        corte_3
    SINO
        SI r.clave2 <> Reg2 ENTONCES
            corte_2
        SINO
            SI r.clave1 <> Reg1 ENTONCES
                corte_1
            FIN_SI
        FIN_SI
    FIN_SI
FIN_SUBACCION
```

### Algoritmo principal
```
inicializar

MIENTRAS NFDA(Arch) HACER
    tratar_corte
    tratar_registro
    Leer(Arch, r)
FIN_MIENTRAS

corte_3
emitir_totales
Cerrar(Arch)
```

---

## CAPITULO 6: Mezcla

### Mezcla excluyente (2 archivos)
```
LEER(arch1, reg1);
LEER(arch2, reg2);

MIENTRAS NFDA(arch1) Y NFDA(arch2) HACER
    SI reg1.clave <= reg2.clave ENTONCES
        ESCRIBIR(arch_sal, reg1);
        LEER(arch1, reg1);
    SINO
        ESCRIBIR(arch_sal, reg2);
        LEER(arch2, reg2);
    FIN_SI
FIN_MIENTRAS

MIENTRAS NFDA(arch1) HACER
    ESCRIBIR(arch_sal, reg1);
    LEER(arch1, reg1);
FIN_MIENTRAS

MIENTRAS NFDA(arch2) HACER
    ESCRIBIR(arch_sal, reg2);
    LEER(arch2, reg2);
FIN_MIENTRAS
```

### Mezcla incluyente (HV)
```
SUBACCION Leer_Arch1 ES
    LEER(arch1, reg1)
    SI FDA(arch1) ENTONCES
        reg1.clave := HV
    FIN_SI
FIN_SUBACCION

Leer_Arch1
Leer_Arch2

MIENTRAS (reg1.clave <> HV) O (reg2.clave <> HV) HACER
    SI reg1.clave <= reg2.clave ENTONCES
        ESCRIBIR(arch_sal, reg1);
        Leer_Arch1
    SINO
        ESCRIBIR(arch_sal, reg2);
        Leer_Arch2
    FIN_SI
FIN_MIENTRAS
```

---

## CAPITULO 7: Actualización

### Unitaria — algoritmo principal
```
Leer_Maestro
Leer_Movimiento
MIENTRAS (Clave_Mae <> HV) O (Clave_Mov <> HV) HACER
    SI Clave_Mae = Clave_Mov ENTONCES
        Proceso_Iguales
    SINO
        SI Clave_Mae < Clave_Mov ENTONCES
            Reg_sal := Reg_mae
            ESCRIBIR(Arch_sal, Reg_sal)
            Leer_Maestro
        SINO
            Proceso_Distintos
        FIN_SI
    FIN_SI
FIN_MIENTRAS
```

### Por lotes — algoritmo principal
```
MIENTRAS (reg_mov.clave <> HV) O (reg_mae.clave <> HV) HACER
    SI reg_mae.clave < reg_mov.clave ENTONCES
        Reg_sal := Reg_mae
        ESCRIBIR(mae_sal, Reg_sal)
        Leer_Maestro
    SINO
        SI reg_mae.clave = reg_mov.clave ENTONCES
            aux := reg_mae
            MIENTRAS reg_mae.clave = reg_mov.clave HACER
                Proceso_Movim
                Leer_Movimiento
            FIN_MIENTRAS
            reg_sal := aux
            ESCRIBIR(mae_sal, reg_sal)
            Leer_Maestro
        SINO
            // Alta: movimiento sin maestro
            aux.clave := reg_mov.clave
            // ... cargar campos
            Leer_Movimiento
            MIENTRAS aux.clave = reg_mov.clave HACER
                Proceso_Movim
                Leer_Movimiento
            FIN_MIENTRAS
            reg_sal := aux
            ESCRIBIR(mae_sal, reg_sal)
        FIN_SI
    FIN_SI
FIN_MIENTRAS
```

### Indexada
```
ABRIR E/S (arch_mae)
LEER(clave, cod_mov)
MIENTRAS cod_mov EN valido HACER
    reg_mae.clave := clave
    LEER(arch_mae, reg_mae)
    SI NO EXISTE ENTONCES
        // ALTA: cargar campos y ESCRIBIR
    SINO
        SI cod_mov = 'M' ENTONCES
            // modificar campos y RE-ESCRIBIR
        SINO
            // BAJA logica: marcar y RE-ESCRIBIR
            // BAJA fisica: BORRAR(arch_mae, reg_mae)
        FIN_SI
    FIN_SI
    LEER(clave, cod_mov)
FIN_MIENTRAS
CERRAR(arch_mae)
```

---

## CAPITULO 8: Búsqueda y Ordenamiento

### Búsqueda lineal
```
PARA i := 1 a N HACER
    SI A[i] = x ENTONCES
        ESCRIBIR(i)
    FIN_SI
FIN_PARA
```

### Búsqueda lineal con centinela
```
i := 1
MIENTRAS (i < N) Y A[i] <> x HACER
    i := i + 1
FIN_MIENTRAS
SI A[i] = x ENTONCES
    ESCRIBIR(i)
SINO
    ESCRIBIR('No encontrado')
FIN_SI
```

### Búsqueda binaria (arreglo ordenado)
```
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
SI A[cen] = x ENTONCES
    ESCRIBIR(cen)
SINO
    ESCRIBIR('No encontrado')
FIN_SI
```

### Inserción directa (menor a mayor)
```
PARA i := 2 a n HACER
    x := a[i]
    j := i - 1
    MIENTRAS (j > 0) Y (x < a[j]) HACER
        a[j+1] := a[j]
        j := j - 1
    FIN_MIENTRAS
    a[j+1] := x
FIN_PARA
```

### Selección directa (mayor a menor)
```
PARA i := 1 a n-1 HACER
    x   := a[i]
    max := i
    PARA j := i+1 a n HACER
        SI x < a[j] ENTONCES
            max := j
            x   := a[j]
        FIN_SI
    FIN_PARA
    a[max] := a[i]
    a[i]   := x
FIN_PARA
```

### Burbuja / Intercambio directo (mayor a menor)
```
Bandera := Falso
MIENTRAS NO Bandera HACER
    Bandera := Verdadero
    PARA j := 1 a n-1 HACER
        SI a[j] < a[j+1] ENTONCES
            x       := a[j]
            a[j]    := a[j+1]
            a[j+1]  := x
            Bandera := Falso
        FIN_SI
    FIN_PARA
FIN_MIENTRAS
```

---

## CAPITULO 9: Recursividad

### Estructura base
```
FUNCION nombre(E n: ENTERO): ENTERO
    PROCESO
        SI <caso_base> ENTONCES
            nombre := <valor_base>;
        SINO
            nombre := <operacion> + nombre(n - 1);
        FIN_SI
FIN_FUNCION
```

### Factorial
```
FUNCION factorial(E n: ENTERO): ENTERO
    PROCESO
        SI n = 0 ENTONCES
            factorial := 1;
        SINO
            factorial := n * factorial(n - 1);
        FIN_SI
FIN_FUNCION
```

### Suma de 1 a N
```
FUNCION suma(E n: ENTERO): ENTERO
    PROCESO
        SI n = 0 ENTONCES
            suma := 0;
        SINO
            suma := n + suma(n - 1);
        FIN_SI
FIN_FUNCION
```

### Fibonacci
```
FUNCION fibonacci(E n: ENTERO): ENTERO
    PROCESO
        SI n = 0 ENTONCES
            fibonacci := 0;
        SINO
            SI n = 1 ENTONCES
                fibonacci := 1;
            SINO
                fibonacci := fibonacci(n-1) + fibonacci(n-2);
            FIN_SI
        FIN_SI
FIN_FUNCION
```

---

## CAPITULO 10: Punteros

### Declaración
```
p: puntero a ENTERO;
p := nil;
```

### NUEVO / DISPONER
```
NUEVO(p)       // reserva memoria, p apunta a ella
DISPONER(p)    // libera esa memoria
```

### Acceso al valor
```
*p := 10;
ESCRIBIR(*p);
*p := *p + 5;
```

### Dos punteros al mismo lugar
```
NUEVO(p);
*p := 99;
q := p;
ESCRIBIR(*q);   // 99
*q := 7;
ESCRIBIR(*p);   // 7
```

---

## CAPITULO 11: Nodos

### Declaración
```
Nodo = REGISTRO
    Dato: ENTERO;
    Prox: puntero a Nodo;
FIN_REGISTRO

p: puntero a Nodo;
```

### Crear un nodo
```
NUEVO(p);
*p.Dato := 42;
*p.Prox := nil;
```

### Conectar dos nodos
```
NUEVO(p);
*p.Dato := 10;
*p.Prox := nil;

NUEVO(q);
*q.Dato := 20;
*q.Prox := nil;

*p.Prox := q;
```

### Recorrer
```
aux := p;
MIENTRAS aux <> nil HACER
    ESCRIBIR(*aux.Dato);
    aux := *aux.Prox;
FIN_MIENTRAS
```

### Liberar cadena
```
MIENTRAS p <> nil HACER
    q := p;
    p := *p.Prox;
    DISPONER(q);
FIN_MIENTRAS
```

---

## CAPITULO 12: Listas simplemente enlazadas

### Variables principales
```
Prim: puntero a Nodo   // apunta al primer nodo
p:    puntero a Nodo   // puntero de recorrido
```

### Lista vacía
```
Prim := nil;
```

### Recorrido con auxiliar
```
aux := Prim;
MIENTRAS aux <> nil HACER
    ESCRIBIR(*aux.Dato);
    aux := *aux.Prox;
FIN_MIENTRAS
```
