# Herramientas

## Calculadora de calibración

`calcular_calibracion_agua.py` obtiene una ecuación lineal de dos puntos:

```text
resultado = pendiente × voltaje + intercepto
```

Ejemplo pH:

```bash
python calcular_calibracion_agua.py \
  --tipo ph --v1 2.50 --ref1 7.00 --v2 3.10 --ref2 4.00
```

Ejemplo TDS:

```bash
python calcular_calibracion_agua.py \
  --tipo tds --v1 0.50 --ref1 342 --v2 1.00 --ref2 707
```

Los números son solo ejemplos. Usa únicamente mediciones reales y patrones
identificados. Verifica siempre con un tercer punto independiente.

