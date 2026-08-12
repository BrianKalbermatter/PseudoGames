# OnlyExerciseReal — Ejercicios del mundo real

> Cada ejercicio incluye el contexto de la empresa, la consigna y el algoritmo a aplicar.
> La estructura de datos la decidís vos.

---

## EJERCICIO 1 — Mercado Libre: Ranking de vendedores

**Contexto:**
Mercado Libre necesita generar un reporte mensual con los vendedores ordenados por volumen de ventas para otorgar medallas (Platinum, Gold, Silver).

**Consigna:**
Se tiene un archivo con registros de vendedores. Cada registro contiene: ID de vendedor, nombre, monto total vendido en el mes. Ordenar los vendedores de mayor a menor por monto vendido y mostrar el listado final con su posición en el ranking.

**Algoritmo a aplicar:** Ordenamiento (selección directa o burbuja según el volumen).

---

## EJERCICIO 2 — Banco Nación: Actualización de cuentas

**Contexto:**
El Banco Nación recibe diariamente un archivo de movimientos (depósitos, extracciones, transferencias) que debe aplicarse sobre el archivo maestro de cuentas corrientes.

**Consigna:**
Dado un archivo maestro de cuentas (ordenado por número de cuenta) y un archivo de movimientos del día (con código de movimiento: D=depósito, E=extracción, C=cierre), generar un nuevo archivo maestro actualizado. Registrar en un archivo de errores los movimientos sobre cuentas inexistentes y las extracciones que superen el saldo disponible.

**Algoritmo a aplicar:** Actualización secuencial por lotes.

---

## EJERCICIO 3 — Hospital Italiano: Turnos por especialidad

**Contexto:**
El Hospital Italiano necesita un reporte estadístico de atenciones médicas agrupadas por especialidad, luego por médico, para auditoría trimestral.

**Consigna:**
Se tiene un archivo de atenciones ordenado por especialidad y dentro de cada especialidad por médico. Cada registro contiene: código de especialidad, legajo del médico, nombre del médico, obra social del paciente, duración de la consulta. Mostrar: cantidad de atenciones por médico, cantidad por especialidad y total general. Identificar además la obra social más frecuente por especialidad.

**Algoritmo a aplicar:** Corte de control de dos niveles (especialidad > médico).

---

## EJERCICIO 4 — Despegar: Búsqueda de vuelos disponibles

**Contexto:**
Despegar maneja un catálogo de vuelos ordenado por fecha y código de vuelo. El usuario ingresa una fecha y un destino y el sistema debe encontrar el vuelo más barato disponible.

**Consigna:**
Dado un arreglo de vuelos (ordenado por precio), el usuario ingresa fecha de salida y ciudad de destino. Buscar el primer vuelo que coincida con ambos criterios al menor precio posible. Si no existe, informar que no hay disponibilidad.

**Algoritmo a aplicar:** Búsqueda binaria sobre precio + filtro por fecha y destino.

---

## EJERCICIO 5 — YPF: Liquidación de sueldos por sucursal y región

**Contexto:**
YPF liquida sueldos para sus empleados distribuidos en distintas sucursales agrupadas por región del país. La gerencia necesita el costo total de personal por sucursal, por región y el total nacional.

**Consigna:**
Se tiene un archivo de empleados ordenado por región y dentro de cada región por sucursal. Cada registro contiene: código de región, código de sucursal, legajo, categoría laboral y sueldo bruto. Calcular y mostrar: total de sueldos por sucursal, total por región y gran total nacional. Indicar también cuántos empleados hay en cada nivel.

**Algoritmo a aplicar:** Corte de control de dos niveles (región > sucursal).

---

## EJERCICIO 6 — OCA: Mezcla de archivos de encomiendas

**Contexto:**
OCA opera con dos centros de distribución: Buenos Aires y Córdoba. Al final del día ambos generan sus propios archivos de encomiendas procesadas (ordenados por número de guía). El sistema central necesita consolidar ambos archivos en uno solo para facturación.

**Consigna:**
Dados dos archivos de encomiendas ordenados por número de guía, generar un único archivo consolidado ordenado también por número de guía. Si una guía aparece en ambos archivos, registrar el error (guía duplicada) en un archivo de incidencias.

**Algoritmo a aplicar:** Mezcla directa con ciclo excluyente + control de duplicados.

---

## EJERCICIO 7 — Ministerio de Educación: Aprobados con recursividad

**Contexto:**
El Ministerio necesita calcular la cantidad de combinaciones posibles de materias que puede tomar un alumno dado un crédito máximo disponible (similar a un problema de mochila simplificado).

**Consigna:**
Dado un arreglo de materias con sus créditos requeridos y un total de créditos disponibles, calcular de cuántas formas distintas el alumno puede completar exactamente sus créditos disponibles eligiendo materias sin repetir. Mostrar el total de combinaciones posibles.

**Algoritmo a aplicar:** Recursividad con combinaciones (caso base: créditos = 0 → 1 combinación; créditos < 0 → 0 combinaciones).

---

## EJERCICIO 8 — Supermercados Día: Control de stock por góndola

**Contexto:**
La cadena Día necesita detectar qué productos están por debajo del stock mínimo en cada góndola para generar órdenes de reposición automáticas por proveedor.

**Consigna:**
Se tiene un archivo de productos ordenado por proveedor y dentro de cada proveedor por góndola. Cada registro contiene: código de proveedor, número de góndola, código de producto, descripción, stock actual y stock mínimo. Generar un archivo de órdenes de compra agrupado por proveedor, incluyendo solo los productos con stock actual menor al mínimo. Al final de cada proveedor, mostrar la cantidad de productos a reponer.

**Algoritmo a aplicar:** Corte de control de un nivel (proveedor) + escritura a archivo de salida.

---

## EJERCICIO 9 — Telecom: Actualización indexada de clientes

**Contexto:**
El sistema de atención al cliente de Telecom permite a los operadores dar de alta clientes nuevos, modificar datos de contacto y dar de baja clientes en tiempo real, sin procesar lotes.

**Consigna:**
Sobre un archivo maestro de clientes indexado por número de línea, implementar un sistema interactivo que permita: alta de cliente nuevo (con validación de que no exista), modificación de dirección o plan contratado (con validación de existencia) y baja lógica marcando el registro. Cada operación debe confirmar el éxito o informar el error correspondiente.

**Algoritmo a aplicar:** Actualización indexada (LEER directo, RE-ESCRIBIR, BORRAR lógico).

---

## EJERCICIO 10 — Rappi: Búsqueda del repartidor más cercano

**Contexto:**
Rappi necesita asignar un pedido al repartidor disponible más cercano al local. Los repartidores están almacenados en una lista dinámica que crece y se reduce constantemente a medida que se conectan y desconectan.

**Consigna:**
Se tiene una lista de repartidores disponibles, cada uno con su distancia al local (en km). Implementar: agregar un repartidor a la lista al conectarse, eliminar un repartidor al tomar un pedido o desconectarse, y buscar el repartidor con menor distancia al local para asignarle el próximo pedido.

**Algoritmo a aplicar:** Lista simplemente enlazada con inserción, eliminación y búsqueda lineal del mínimo.

---

## EJERCICIO 11 — AFIP: Declaración jurada con factorial de deducciones

**Contexto:**
Un sistema de AFIP calcula el coeficiente de deducciones impositivas acumuladas para personas con N años de antigüedad laboral continua usando una fórmula que crece factorialmente.

**Consigna:**
Dado el legajo de un contribuyente y su cantidad de años de antigüedad continua, calcular el coeficiente de deducción usando la fórmula: `coeficiente = 1 / factorial(años)`. Si el contribuyente tiene 0 años, el coeficiente es 1. Mostrar el resultado con 6 decimales. Procesar una lista de contribuyentes hasta que se ingrese un legajo centinela.

**Algoritmo a aplicar:** Recursividad (factorial) + acumulador de resultados.

---

## EJERCICIO 12 — Naranja X: Mezcla de consumos de dos tarjetas

**Contexto:**
Naranja X unifica el resumen mensual de dos productos (tarjeta de crédito y tarjeta de débito) en un único estado de cuenta ordenado por fecha de transacción.

**Consigna:**
Se tienen dos archivos: consumos de crédito y consumos de débito, ambos ordenados por fecha. Cada registro contiene: fecha, descripción del comercio, monto y tipo. Generar un único archivo de resumen ordenado por fecha. Cuando dos transacciones tienen la misma fecha, el débito va antes que el crédito. Al final del resumen, mostrar el total gastado en crédito, el total en débito y el gran total.

**Algoritmo a aplicar:** Mezcla incluyente con HV + criterio de desempate por tipo.
