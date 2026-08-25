# Nodo 2: actuadores real

Abre `02_Nodo_Actuadores_Real.ino` con Arduino IDE.

Este sketch:

- lee dos RCWL-1655;
- promedia señales analógicas de pH y TDS;
- lee temperatura DS18B20;
- informa los estados reales de dos relés;
- muestra errores y conexión al central en OLED;
- transmite como radio `A1` y nodo `tlali-actuadores-01`.

Monitor serial: 115200 baudios.

Los relés permanecen apagados de forma segura. No conectes bombas ni cargas
hasta definir voltaje, corriente, protecciones y lógica de control.

pH y TDS todavía son voltajes. Usa la guía:

```text
../../04_GUIAS/CALIBRACION_SENSORES_AGUA.md
```

