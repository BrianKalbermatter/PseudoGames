# Calculadora de Dos Botones — Teoría

## La Idea Central

Una variable `x = e^a - ln(b)` puede representar **cualquier número real**.

- `e^a` con `a ∈ ℝ` produce `(0, +∞)`
- `ln(b)` con `b > 0` produce `(-∞, +∞)`
- La resta de ambos cubre todo `(-∞, +∞)`

Con solo dos operaciones — `e^x` y `ln(x)` — y la resta como primitivo,
se puede derivar TODA la aritmética real.

---

## Propiedades Fundamentales (deducidas)

### 1. Inversas
```
e^(ln(x)) = x
ln(e^x)   = x
```
Aplicar una deshace la otra. Como subir y bajar el mismo número de pisos.

### 2. Multiplicación de potencias
```
b^m · b^n = b^(m+n)
```
Ejemplo: `2^3 · 2^4 = 8 · 16 = 128 = 2^7`  →  los exponentes se suman.

### 3. Logaritmo de un producto
```
ln(a · b) = ln(a) + ln(b)
```
Deducción: `a · b = e^ln(a) · e^ln(b) = e^(ln(a)+ln(b))`
Como los exponentes de `e` son iguales → los argumentos del `ln` también.

### 4. Logaritmo de un cociente
```
ln(a / b) = ln(a) - ln(b)
```

### 5. Logaritmo de una potencia
```
ln(b^n) = n · ln(b)
```
Deducción: `ln(b^3) = ln(b·b·b) = ln(b)+ln(b)+ln(b) = 3·ln(b)`

---

## Operaciones Derivadas

### Multiplicación
```
a · b = e^(ln(a) + ln(b))
      = e^(ln(a) - ln(1/b))    ← forma pura e^a - ln(b)
```
Truco: `ln(a) + ln(b) = ln(a) - (-ln(b)) = ln(a) - ln(1/b)`

### División
```
a / b = e^(ln(a) - ln(b))      ← directamente nuestra fórmula
```

### Potenciación (pendiente de deducir)
```
a^b = ?
```

### Raíz (pendiente de deducir)
```
ⁿ√a = ?
```

---

## La Suma — El Caso Difícil

La suma **no es expresable** con solo `e^x` y `ln(x)`.
La resta es el **primitivo fundamental** — ya está en `e^a - ln(b)`.

La suma se construye sobre la resta:
```
a + b = a - (-b)
```

La negación de `b` usando nuestra fórmula:
```
-b = e^0 - ln(e^(1+b))
   = 1 - (1 + b)
   = -b  ✓
```

---

## Primitivos del Sistema

| Primitivo           | Símbolo  |
|---------------------|----------|
| Exponencial natural | `e^x`    |
| Logaritmo natural   | `ln(x)`  |
| Resta               | `-`      |

Todo lo demás se **deriva** de estos tres.

---

## Implementación en C

```c
#include <math.h>

// Primitivo base
double calc(double a, double b) {
    return exp(a) - log(b);
}

// Operaciones derivadas (pendiente)
double multiplicar(double a, double b);
double dividir(double a, double b);
double potencia(double a, double b);
double raiz(double a, double n);
double sumar(double a, double b);
```

---

## Estado

- [x] Inversas `e` y `ln`
- [x] Multiplicación de potencias `b^m · b^n = b^(m+n)`
- [x] `ln(a·b) = ln(a) + ln(b)`
- [x] `ln(a/b) = ln(a) - ln(b)`
- [x] `ln(b^n) = n·ln(b)`
- [x] Multiplicación derivada
- [x] División derivada
- [ ] Potenciación
- [ ] Raíz
- [ ] Suma completa
- [ ] Implementación completa en C
