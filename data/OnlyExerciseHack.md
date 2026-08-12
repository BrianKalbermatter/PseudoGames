# OnlyExerciseHack — Hackathones y trampas

> Dos tipos acá:
> - **HACK:** ejercicios de hackathón real, tiempo limitado, presión, creatividad obligatoria.
> - **TRAMPA:** parecen fáciles. No lo son. Si no leés bien, fallás y no sabés por qué.
>
> En ambos casos: leé dos veces antes de escribir una sola línea.

---

# PARTE 1 — HACKATHONES

---

## HACK 1 — Sistema de votación en tiempo real (24hs)

**Contexto hackathón:**
Maratón de programación universitaria. Tenés 6 horas. El cliente es una ONG que hace votaciones comunitarias barriales.

**Consigna:**
Desarrollar un sistema que procese votos en tiempo real. Cada voto contiene: ID del votante, ID de la opción elegida (1 a 5), timestamp. El sistema debe:
- Detectar y descartar votos duplicados (mismo votante vota dos veces).
- Mostrar el resultado parcial actualizado después de cada voto válido.
- Al finalizar, mostrar el ganador, el porcentaje de cada opción y la participación total sobre el padrón (archivo separado con los IDs habilitados a votar).
- Si hay empate, mostrar todos los ganadores.

**Trampa oculta del hackathón:**
El padrón tiene IDs pero el archivo de votos puede tener IDs que NO están en el padrón. Esos votos son inválidos aunque no sean duplicados. Muchos equipos lo ignoran y aprueban votos de personas no habilitadas.

**Presión de tiempo:**
Primero diseñá el flujo completo en papel. Los equipos que arrancan a codear de entrada pierden 2 horas rehaciendo cosas.

---

## HACK 2 — Optimización de rutas de reparto (48hs)

**Contexto hackathón:**
Hackathón de logística organizado por una startup de última milla. Premio: pasantía paga.

**Consigna:**
Una empresa de reparto tiene N paquetes para entregar y K repartidores. Cada repartidor sale desde el depósito central. Cada paquete tiene: dirección (representada como distancia en km desde el depósito), peso y ventana horaria de entrega (hora mínima y máxima de entrega). Asignar los paquetes a los repartidores de forma que:
- Ningún repartidor supere 30 kg de carga total.
- Todos los paquetes se entreguen dentro de su ventana horaria.
- La distancia total recorrida por todos los repartidores sea la menor posible.

Mostrar la asignación final y la distancia total.

**Trampa oculta del hackathón:**
Si dos paquetes tienen ventanas horarias que se solapan y están en extremos opuestos de la ciudad, es imposible asignarlos al mismo repartidor aunque el peso lo permita. Los equipos que no modelan el tiempo fallan silenciosamente (asignan paquetes imposibles de entregar a tiempo).

---

## HACK 3 — Detector de noticias duplicadas (12hs, sprint)

**Contexto hackathón:**
Sprint de 12 horas en una empresa de medios digitales. Necesitan filtrar noticias repetidas de distintas agencias.

**Consigna:**
Se recibe un archivo con noticias del día. Cada noticia tiene: ID, agencia, título, timestamp. Dos noticias se consideran duplicadas si sus títulos comparten más del 70% de las palabras (ignorando artículos: el, la, los, las, un, una, de, en, a, y). Generar un archivo con las noticias únicas (quedarse con la más antigua de cada grupo de duplicadas) y otro con el listado de noticias descartadas indicando a cuál duplican.

**Trampa oculta del hackathón:**
El 70% se calcula sobre el total de palabras del título más corto, no sobre el promedio. Equipos que promedian o usan el más largo clasifican mal los duplicados. Además "el" y "la" suenan triviales de ignorar pero si no lo implementás desde el principio, todo el cálculo de porcentaje está contaminado.

---

## HACK 4 — Simulador de cola bancaria (6hs)

**Contexto hackathón:**
Hackathón interno de un banco para mejorar la experiencia en sucursales.

**Consigna:**
Simular el flujo de clientes en una sucursal con 3 ventanillas. Cada cliente llega con: timestamp de llegada, tipo de trámite (caja=5min, consulta=15min, crédito=30min) y si es cliente preferencial (mayores de 65, embarazadas, discapacidad). Los preferenciales siempre pasan antes que los no preferenciales. Entre no preferenciales, orden de llegada. Entre preferenciales, orden de llegada. Simular 8 horas de atención y calcular: tiempo promedio de espera por tipo de trámite, ventanilla más ocupada y cantidad de clientes que se fueron sin ser atendidos (llegaron y la cola tenía más de 10 personas esperando).

**Trampa oculta del hackathón:**
Los clientes que se van sin atenderse siguen "existiendo" en el archivo de entrada. Si los contás como atendidos o los ignorás mal, los promedios de espera quedan distorsionados. Además la ventanilla más "ocupada" puede medirse por tiempo total o por cantidad de clientes: son resultados distintos y el enunciado no especifica. Hay que elegir y justificarlo.

---

# PARTE 2 — EJERCICIOS CON TRAMPA

---

## TRAMPA 1 — El acumulador que miente

**Consigna (aparentemente simple):**
Dado un arreglo de N números enteros, calcular el promedio. Mostrar cuántos elementos están por encima del promedio y cuántos por debajo.

**La trampa:**
El promedio es REAL aunque todos los números sean ENTEROS. Si declarás `promedio: ENTERO` perdés los decimales y clasificás mal los elementos. Ejemplo: arreglo [1, 2, 3] → promedio real = 2.0, promedio entero = 2. El 2 no está ni por encima ni por debajo: está exactamente en el promedio. ¿Lo contás o no? El enunciado no lo dice. Hay que decidirlo y documentarlo.

**Segunda trampa dentro de la trampa:**
Necesitás dos pasadas por el arreglo: una para calcular el promedio y otra para comparar. Si intentás hacerlo en una sola pasada, no podés porque no sabés el promedio hasta terminar la primera pasada.

---

## TRAMPA 2 — La búsqueda que siempre encuentra

**Consigna (aparentemente simple):**
Usar búsqueda lineal con centinela para encontrar un elemento X en un arreglo de N elementos. Mostrar en qué posición está o informar que no existe.

**La trampa:**
La condición del MIENTRAS es `(i <= N) Y (A[i] <> x)`. Si X no está en el arreglo, cuando i llega a N+1 intentás acceder a A[N+1] que no existe: acceso fuera de rango. La condición correcta es `(i < N) Y (A[i] <> x)` y después verificar A[N] por separado, o usar `(i <= N) Y (A[i] <> x)` pero con el índice correcto.

Muchos escriben la condición mal y el algoritmo "funciona" con sus datos de prueba porque X siempre está en el arreglo. Falla solo cuando X no existe.

---

## TRAMPA 3 — El archivo que termina antes

**Consigna (aparentemente simple):**
Leer un archivo de empleados y mostrar el nombre y sueldo de todos los que ganan más de $100.000.

**La trampa:**
```
ABRIR(arch, lectura)
MIENTRAS NO FDA(arch) HACER
    LEER(arch, reg)
    SI reg.sueldo > 100000 ENTONCES
        ESCRIBIR(reg.nombre, reg.sueldo)
    FIN_SI
FIN_MIENTRAS
CERRAR(arch)
```

Este código parece correcto. Falla si el archivo está vacío: FDA es verdadero desde el inicio, no entra al MIENTRAS, perfecto. Pero falla con el patrón incorrecto de primer LEER afuera: si ponés el LEER antes del MIENTRAS (patrón de corte de control) y el archivo está vacío, leés un registro que no existe y procesás basura.

La trampa no está en el código sino en cuándo aplicar cada patrón de lectura.

---

## TRAMPA 4 — La mezcla que pierde el último registro

**Consigna (aparentemente simple):**
Mezclar dos archivos ordenados en uno solo.

**La trampa:**
```
LEER(arch1, reg1)
LEER(arch2, reg2)
MIENTRAS NFDA(arch1) Y NFDA(arch2) HACER
    SI reg1.clave <= reg2.clave ENTONCES
        ESCRIBIR(arch_sal, reg1)
        LEER(arch1, reg1)
    SINO
        ESCRIBIR(arch_sal, reg2)
        LEER(arch2, reg2)
    FIN_SI
FIN_MIENTRAS
```

Este código pierde todos los registros del archivo que NO se agotó primero. Cuando arch1 llega a FDA, el ciclo termina y los registros restantes de arch2 nunca se escriben. Faltan los dos ciclos residuales. El error es silencioso: el archivo de salida se genera sin error, simplemente incompleto. Con datos de prueba donde ambos archivos tienen el mismo largo nunca lo detectás.

---

## TRAMPA 5 — El corte que no cierra el último grupo

**Consigna (aparentemente simple):**
Archivo de ventas ordenado por vendedor. Mostrar el total de ventas por vendedor.

**La trampa:**
```
MIENTRAS NO FDA(arch) HACER
    LEER(arch, reg)
    SI reg.vendedor <> vendedorActual ENTONCES
        ESCRIBIR(vendedorActual, totalVendedor)  // cierre del grupo anterior
        totalVendedor := 0
        vendedorActual := reg.vendedor
    FIN_SI
    totalVendedor := totalVendedor + reg.monto
FIN_MIENTRAS
```

El último grupo NUNCA se cierra. El corte solo se activa cuando cambia el vendedor, pero después del último registro no hay cambio. El último vendedor del archivo queda sin mostrarse. Siempre hay que agregar un cierre explícito después del MIENTRAS:

```
FIN_MIENTRAS
ESCRIBIR(vendedorActual, totalVendedor)  // cierre del ULTIMO grupo
```

Error que cometen el 80% de los alumnos en el primer parcial.

---

## TRAMPA 6 — La recursión que no llega al caso base

**Consigna (aparentemente simple):**
Calcular la suma de dígitos de un número entero positivo usando recursividad.

**La trampa:**
```
FUNCION suma_digitos(E n: ENTERO): ENTERO
    PROCESO
        SI n = 0 ENTONCES
            suma_digitos := 0
        SINO
            suma_digitos := (n MOD 10) + suma_digitos(n DIV 10)
        FIN_SI
FIN_FUNCION
```

Parece correcto. Falla con n negativo: `(-123) MOD 10` da resultados distintos según el lenguaje (puede dar -3 o 7). Y `(-123) DIV 10` puede dar -12 o -13. Si n nunca llega a 0 el programa se cuelga infinitamente.

La función debería verificar primero si n < 0 y trabajar con su valor absoluto, o documentar explícitamente que solo acepta enteros positivos.

---

## TRAMPA 7 — El puntero que apunta a la nada después de liberar

**Consigna (aparentemente simple):**
Eliminar el primer nodo de una lista enlazada.

**La trampa:**
```
DISPONER(Prim)          // libero el primer nodo
Prim := *Prim.Prox      // intento avanzar Prim al siguiente
```

Después de `DISPONER(Prim)` la memoria fue liberada. `*Prim.Prox` accede a memoria liberada: comportamiento indefinido. El orden correcto es siempre guardar lo que necesitás ANTES de liberar:

```
aux := Prim
Prim := *Prim.Prox      // primero avanzo
DISPONER(aux)           // después libero
```

El código incorrecto puede "funcionar" en algunas implementaciones porque la memoria liberada no se borra inmediatamente. Falla de forma intermitente e impredecible, lo cual lo hace muy difícil de debuggear.

---

## TRAMPA 8 — El índice que empieza en el lugar equivocado

**Consigna (aparentemente simple):**
Ordenar un arreglo de N elementos con inserción directa.

**La trampa:**
```
PARA i := 1 a n HACER      // empieza en 1
    x := a[i]
    j := i - 1
    MIENTRAS (j > 0) Y (x < a[j]) HACER
        a[j+1] := a[j]
        j := j - 1
    FIN_MIENTRAS
    a[j+1] := x
FIN_PARA
```

En la primera iteración con i=1: `x := a[1]`, `j := 0`. El MIENTRAS tiene condición `j > 0` que es Falso de entrada, no entra. `a[1] := x`. No hace nada útil. El algoritmo correcto empieza en i=2 porque el primer elemento ya está "ordenado" por definición. Empezar en i=1 no rompe el resultado (da el mismo ordenamiento) pero hace una iteración completamente inútil. En un parcial te bajan puntos por no entender qué hace el algoritmo.

---

## TRAMPA 9 — La condición de FDA que se evalúa una vez de más

**Consigna (aparentemente simple):**
Contar cuántos registros tiene un archivo.

**La trampa:**
```
contador := 0
ABRIR(arch, lectura)
MIENTRAS NO FDA(arch) HACER
    LEER(arch, reg)
    contador := contador + 1
FIN_MIENTRAS
CERRAR(arch)
```

Parece perfecto. El problema: después del último LEER, FDA se vuelve Verdadero. El MIENTRAS vuelve a evaluar la condición, la encuentra Falsa y sale. Correcto.

Pero si alguien escribe:
```
MIENTRAS NO FDA(arch) HACER
    contador := contador + 1
    LEER(arch, reg)
FIN_MIENTRAS
```

El contador se incrementa ANTES de leer. En la última iteración, después del último registro, FDA todavía es Falso (porque no intentaste leer más allá). Contás un registro de más. El contador queda en N+1.

El orden importa: primero LEER, después procesar.

---

## TRAMPA 10 — El HIGH VALUE que no es tan alto

**Consigna (aparentemente simple):**
Implementar mezcla incluyente con HV para dos archivos de claves numéricas.

**La trampa:**
HV debe ser un valor MAYOR a cualquier clave posible en el archivo. Si las claves son números de legajo de 5 cifras (máximo 99999) y definís `HV = 99999`, cuando el archivo tenga un legajo 99999 el sistema lo tratará como fin de archivo sin haberlo procesado.

HV debe ser mayor ESTRICTAMENTE que cualquier valor posible: si la clave es N(5) (hasta 99999), HV debe ser al menos 100000.

El error es silencioso: el último registro con clave máxima desaparece del archivo de salida sin ningún mensaje de error.
