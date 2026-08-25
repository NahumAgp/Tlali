# Dashboard WordPress / Divi

Archivo principal:

```text
TLALI_DASHBOARD_DIVI.html
```

## Instalación

1. Crea o abre la página `Monitoreo TLALI`.
2. Usa el constructor Divi.
3. Agrega una fila de una columna, ancho 95 %, máximo 1400 px.
4. Inserta un módulo **Código**.
5. Copia el archivo HTML completo y pégalo.
6. Guarda y actualiza con `Ctrl+F5`.

## Pantallas

- Resumen.
- Invernadero: todas las variables y gráficas NPK/ambiente.
- Actuadores: litros y porcentaje aproximados de una cisterna medida con dos
  ultrasónicos, pH, conductividad, TDS y relés.
- Historial: fecha, variable, gráfica, tabla y CSV.

El panel lee:

```text
https://tlali-5edc4-default-rtdb.firebaseio.com/tlali
```

Para esta etapa de prueba Firebase debe permitir lectura desde el navegador.
Consulta `INSTRUCCIONES_DIVI.txt`.

## Cálculo de la cisterna

La cisterna se considera de 5 x 6 x 3.5 m y 105,000 litros. Los sensores 1 y 2
miden la misma cisterna: se promedian y después se convierten usando 19 cm como
lleno y 350 cm como vacío. Si uno falla, se utiliza el sensor disponible.

El panel aplica la calibración pH 4/7/10 y la referencia EC de 1413 µS/cm para
mostrar pH, conductividad y TDS. Las señales eléctricas crudas no se muestran.
