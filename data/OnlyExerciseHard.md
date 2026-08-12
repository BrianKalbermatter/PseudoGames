# OnlyExerciseHard — Los que nadie puede resolver

> Ejercicios de nivel entrevista técnica en empresas de alto nivel.
> No hay una sola forma de resolverlos. Requieren análisis antes de escribir una sola línea.
> Pensá en papel primero. Siempre.

---

## EJERCICIO H1 — El problema de la caja registradora (cambio exacto)

**Nivel:** Difícil
**Empresa tipo:** Mercado Pago, Ualá, fintech

**Contexto:**
En un sistema de pagos necesitás devolver el vuelto exacto usando la menor cantidad posible de billetes/monedas. El cajero automático tiene disponibilidad limitada de cada denominación.

**Consigna:**
Dado un monto de vuelto a devolver y un arreglo de denominaciones disponibles (con su cantidad en stock), encontrar la combinación que use la menor cantidad de billetes/monedas para completar el monto exacto. Si no es posible dar el vuelto exacto, informarlo. Si hay más de una combinación óptima, mostrar todas.

**Por qué es difícil:**
No alcanza con tomar siempre la denominación más grande (algoritmo greedy falla con ciertos stocks). Requiere explorar combinaciones. La recursividad con backtracking es la herramienta natural, pero hay que podar bien el árbol de búsqueda o se vuelve O(2ⁿ) sin control.

**Pista de análisis:**
- ¿Cuándo descartás una rama entera sin explorarla?
- ¿Qué pasa si el stock de una denominación llega a 0?
- ¿Cómo evitás contar la misma combinación dos veces en distinto orden?

---

## EJERCICIO H2 — Detección de ciclos en historial de navegación

**Nivel:** Difícil
**Empresa tipo:** Google, Meta, cualquier empresa con grafo de datos

**Contexto:**
Un sistema de auditoría necesita detectar si un usuario entró en un bucle de redirecciones al navegar entre páginas internas de un portal corporativo.

**Consigna:**
Se tiene una lista de redirecciones: cada nodo representa una página y tiene un puntero a la siguiente página a la que redirige. Detectar si existe un ciclo en la cadena de redirecciones (es decir, si siguiendo los punteros eventualmente volvés a una página ya visitada). Indicar en qué nodo comienza el ciclo.

**Por qué es difícil:**
No podés simplemente marcar nodos como "visitados" usando un arreglo auxiliar si no sabés cuántos nodos hay. La solución elegante (dos punteros: uno lento y uno rápido) requiere entender profundamente cómo funcionan los punteros y cuándo se encuentran dentro del ciclo.

**Pista de análisis:**
- ¿Qué pasa si avanzás un puntero de a 1 nodo y otro de a 2 nodos al mismo tiempo?
- ¿En qué condición se encuentran si hay un ciclo?
- ¿Cómo encontrás el nodo exacto donde empieza el ciclo una vez que detectaste que existe?

---

## EJERCICIO H3 — Ranking en tiempo real con actualizaciones frecuentes

**Nivel:** Muy difícil
**Empresa tipo:** Mercado Libre, cualquier plataforma de e-commerce con rankings

**Contexto:**
Un sistema de ranking de vendedores recibe miles de actualizaciones de puntaje por minuto. Necesita responder en tiempo real a dos preguntas: "¿Cuál es el vendedor en la posición k del ranking?" y "¿En qué posición está el vendedor X?".

**Consigna:**
Se tiene una lista dinámica de vendedores con su puntaje actual. El sistema recibe operaciones de tres tipos: actualizar puntaje de un vendedor, consultar qué vendedor está en la posición k, y consultar en qué posición está un vendedor dado su ID. El orden del ranking es de mayor a menor puntaje. En caso de empate, va primero el de menor ID.

**Por qué es difícil:**
Si mantenés la lista ordenada, cada actualización cuesta O(n). Si no la mantenés ordenada, cada consulta cuesta O(n log n). El desafío es diseñar la estructura para que ambas operaciones sean eficientes. No hay una estructura simple que lo resuelva directamente.

**Pista de análisis:**
- ¿Qué operaciones son más frecuentes: actualizaciones o consultas?
- ¿Podés separar el problema en dos partes independientes?
- ¿Cómo afecta el criterio de desempate a tu estructura?

---

## EJERCICIO H4 — Mezcla de N archivos con memoria limitada

**Nivel:** Muy difícil
**Empresa tipo:** Sistemas bancarios, procesamiento masivo de datos

**Contexto:**
Un banco necesita consolidar los movimientos del día de 8 sucursales en un único archivo central ordenado por timestamp. El sistema tiene memoria RAM muy limitada y no puede cargar más de un registro de cada archivo a la vez.

**Consigna:**
Dados 8 archivos de movimientos, cada uno ordenado por timestamp, generar un único archivo de salida ordenado por timestamp usando en todo momento solo 8 registros en memoria (uno por archivo). Si dos movimientos tienen el mismo timestamp, el orden entre ellos es por código de sucursal de menor a mayor.

**Por qué es difícil:**
Una mezcla excluyente de 8 archivos requiere 2⁸ - 1 = 255 ciclos. Eso es inmanejable de escribir y mantener. Hay que encontrar una forma de generalizar la mezcla para N archivos con una estructura que siempre tenga el mínimo disponible sin recorrer los 8 registros en cada paso.

**Pista de análisis:**
- ¿Cuántas comparaciones necesitás en cada paso si tenés N archivos?
- ¿Podés mantener los N registros actuales organizados de forma que el mínimo siempre esté accesible en O(1)?
- ¿Qué estructura de datos permite insertar un nuevo elemento y encontrar el mínimo eficientemente?

---

## EJERCICIO H5 — Corte de control con archivo desordenado

**Nivel:** Muy difícil
**Empresa tipo:** Sistemas de facturación, ERP corporativos

**Contexto:**
Una empresa recibe un archivo de ventas de distintas sucursales enviado por diferentes sistemas que no garantizan el orden. Necesita el reporte de ventas por región y sucursal.

**Consigna:**
Se tiene un archivo de ventas con registros que contienen: código de región, código de sucursal, monto de venta. El archivo NO está ordenado. Generar el reporte de total de ventas por sucursal dentro de cada región y total por región, sin modificar el archivo original y sin usar más de O(n) memoria extra.

**Por qué es difícil:**
El corte de control estándar requiere que el archivo esté ordenado. Acá no lo está. Podés ordenarlo primero (O(n log n)) y luego aplicar corte (O(n)), pero el desafío es hacerlo en una sola pasada o con el mínimo de pasadas posibles. Además la restricción de memoria extra O(n) elimina algunas soluciones obvias.

**Pista de análisis:**
- ¿Cuántos valores distintos de región/sucursal puede haber?
- ¿Podés acumular los totales sin ordenar primero?
- ¿Cómo generás el reporte en orden si los acumulaste de forma desordenada?

---

## EJERCICIO H6 — Sistema de turnos con prioridad dinámica

**Nivel:** Muy difícil
**Empresa tipo:** Hospitales, sistemas de atención al cliente, aerolíneas

**Contexto:**
Un sistema de turnos hospitalarios asigna prioridad según la urgencia del paciente (1=crítico, 2=urgente, 3=normal). La prioridad puede cambiar mientras el paciente espera (por ejemplo, un paciente normal que lleva mucho tiempo esperando sube su prioridad). El sistema debe atender siempre al de mayor prioridad actual.

**Consigna:**
Implementar un sistema que permita: agregar un paciente con su prioridad inicial, actualizar la prioridad de un paciente que ya está esperando, atender (retirar) al paciente con mayor prioridad actual. En caso de empate de prioridad, atender al que llegó primero. El sistema debe procesar una secuencia de operaciones de los tres tipos mezcladas.

**Por qué es difícil:**
La lista enlazada simple no permite actualizar eficientemente. Mantener todo ordenado hace que las actualizaciones sean O(n). El verdadero desafío es diseñar la solución con las estructuras vistas en la cursada (listas, punteros, registros) sin acceso a estructuras avanzadas como heap.

**Pista de análisis:**
- ¿Cómo representás la prioridad junto con el tiempo de llegada?
- ¿Podés separar pacientes por nivel de prioridad en listas distintas?
- ¿Qué información adicional necesitás guardar en cada nodo para resolver el desempate?

---

## EJERCICIO H7 — Detección de transacciones fraudulentas

**Nivel:** Extremo
**Empresa tipo:** Visa, Mastercard, Mercado Pago

**Contexto:**
Un sistema antifraude detecta transacciones sospechosas. Una transacción se considera sospechosa si: el mismo usuario realizó más de 3 transacciones en menos de 5 minutos, o si el monto supera 10 veces el promedio de sus últimas 10 transacciones, o si se realizó desde una ciudad diferente a la de las últimas 3 transacciones.

**Consigna:**
Se recibe un stream de transacciones en orden cronológico (timestamp, userID, monto, ciudad). Para cada transacción nueva, determinar en tiempo real si es sospechosa según alguno de los tres criterios. Marcar las sospechosas en un archivo de alertas con el criterio que la disparó. Un usuario puede disparar varios criterios a la vez.

**Por qué es difícil:**
Requiere mantener una "ventana deslizante" de historial por usuario actualizada en tiempo real. La lógica de cada criterio es independiente pero se evalúa sobre el mismo historial. El diseño de la estructura de datos por usuario es el nudo del problema: demasiado historial es lento, muy poco historial pierde información.

**Pista de análisis:**
- ¿Qué estructura por usuario mantiene eficientemente las últimas N transacciones?
- ¿Cómo calculás el promedio de las últimas 10 sin recalcular desde cero cada vez?
- ¿Cómo sabés si dos timestamps están dentro de la ventana de 5 minutos sin convertir fechas?

---

## EJERCICIO H8 — Reconstrucción de árbol genealógico

**Nivel:** Extremo
**Empresa tipo:** Empresas de genealogía, sistemas legales de herencias, RENAPER

**Contexto:**
Un sistema legal necesita determinar el orden de herederos de una persona fallecida. Los herederos se ordenan según su grado de parentesco (hijos primero, luego nietos, luego bisnietos, etc.) y dentro del mismo grado, por edad de mayor a menor.

**Consigna:**
Se tiene un archivo de personas con: ID, nombre, ID del padre (o vacío si es la raíz), fecha de nacimiento. Dado el ID de la persona fallecida, generar la lista de herederos en el orden legal correcto: primero todos los del grado 1 (hijos) ordenados por edad, luego todos los del grado 2 (nietos) ordenados por edad, y así sucesivamente. Solo heredan los descendientes directos vivos (campo activo = V).

**Por qué es difícil:**
Requiere construir la estructura de árbol desde el archivo plano, recorrerla por niveles (no en profundidad) y aplicar ordenamiento dentro de cada nivel. El recorrido por niveles (BFS) sobre una lista enlazada construida dinámicamente desde un archivo es uno de los problemas más complejos de la cursada.

**Pista de análisis:**
- ¿Cómo construís la estructura de árbol desde un archivo donde los registros no están en orden de parentesco?
- ¿Cómo recorrés por niveles sin recursión y sin conocer de antemano la cantidad de niveles?
- ¿Qué estructura auxiliar te permite procesar nivel por nivel?
