# Guía de calibración de sensores de agua TLALI

Esta guía corresponde al nodo `tlali-actuadores-01` y al firmware actual:

- pH en GPIO 1, con divisor 10 kΩ / 20 kΩ y factor de reconstrucción 1.5;
- TDS en GPIO 2, conexión analógica directa;
- DS18B20 en GPIO 47, pull-up 4.7 kΩ a 3.3 V;
- dos RCWL-1655 en TRIG/ECHO 4/5 y 6/7;
- monitor serial a 115200 baudios.

## Resultado esperado

Al terminar debes tener:

- voltaje promedio en pH 7;
- voltaje promedio en pH 4;
- verificación opcional en pH 10;
- uno o dos puntos de TDS/EC con temperatura;
- offset del DS18B20 si fuera necesario;
- distancia de tanque vacío y tanque lleno para cada depósito.

No conviertas voltajes a pH o ppm hasta completar y verificar estos puntos.

## Material

- buffer pH 7.00 y pH 4.00;
- buffer pH 10.00 opcional;
- solución patrón TDS/EC con valor conocido;
- agua destilada para enjuague;
- vasos separados, limpios y etiquetados;
- KCl 3 mol/L para almacenamiento del electrodo de pH;
- papel sin pelusa;
- termómetro de referencia;
- multímetro;
- laptop con Arduino IDE y cable USB.

## Precauciones

1. Mantén seco el conector BNC y la placa acondicionadora.
2. No golpees ni frotes el bulbo de vidrio del electrodo de pH.
3. Enjuaga con agua destilada entre soluciones y retira las gotas suavemente.
4. No regreses solución usada al frasco original.
5. No guardes el electrodo de pH en agua destilada. Usa solución KCl.
6. No mezcles electrodos de pH y TDS en el mismo vaso durante la calibración si
   observas interferencia. Calibra uno a la vez.
7. Registra la temperatura: buffers y patrones dependen de ella.
8. Comprueba el modelo exacto de cada placa antes de usar una fórmula del
   fabricante.

La documentación oficial de DFRobot recomienda calibración de pH a dos puntos,
pH 7 y pH 4, limpiar con agua destilada y mantener seca la electrónica. Para el
SEN0244 recomienda compensación de temperatura; su salida nominal es 0–2.3 V.
Estas referencias solo se aplican directamente si tus módulos son esos modelos:

- https://wiki.dfrobot.com/sen0169-v2/docs/24420
- https://wiki.dfrobot.com/sen0244/

## 1. Preparar el nodo

1. Desconecta bombas y cargas de potencia.
2. Conecta únicamente sensores, ESP32, LoRa y USB.
3. Abre Arduino IDE > Monitor serial > 115200.
4. Busca estas líneas:

```text
pH - voltaje en Po: X.XXX V
TDS - voltaje en A: X.XXX V
Temperatura DS18B20: XX.XX C
```

5. Espera unos minutos con alimentación estable.
6. Verifica con multímetro que el voltaje físico concuerde razonablemente.

El valor `pH - voltaje en Po` ya reconstruye el voltaje del módulo mediante el
factor 1.5. No vuelvas a multiplicarlo al calcular pH.

## 2. Calibrar pH por dos puntos

### Punto neutro, pH 7

1. Coloca buffer pH 7 en un vaso limpio.
2. Enjuaga la sonda, retira gotas sin frotar y sumérgela correctamente.
3. Agita suavemente y espera hasta que el voltaje se estabilice.
4. Registra 20 lecturas consecutivas y su temperatura.
5. Calcula el promedio: `V7`.

### Punto ácido, pH 4

1. Retira la sonda, enjuaga y cambia de vaso.
2. Sumérgela en buffer pH 4.
3. Espera estabilidad.
4. Registra 20 lecturas y calcula el promedio: `V4`.

### Calcular la ecuación

Usa una recta:

```text
pH = m × V + b
m = (4.00 - 7.00) / (V4 - V7)
b = 7.00 - m × V7
```

Ejecuta la calculadora incluida:

```bash
python 07_HERRAMIENTAS/calcular_calibracion_agua.py \
  --tipo ph --v1 V7 --ref1 7.00 --v2 V4 --ref2 4.00
```

Sustituye `V7` y `V4` por los promedios reales.

### Verificar

1. Repite pH 7 sin recalcular. El error debería ser pequeño y estable.
2. Si tienes pH 10, úsalo como punto independiente.
3. No ajustes la ecuación con pH 10 hasta comprobar si el rango de trabajo será
   ácido o alcalino.
4. Si hay deriva continua, ruido o tarda demasiado, revisa hidratación, tierra
   común, fuente, humedad en BNC y estado del electrodo.

## 3. Calibrar TDS

Primero identifica el modelo y la escala del patrón:

- ppm 500, ppm 640 y ppm 700 no son escalas intercambiables;
- una solución marcada 1413 µS/cm puede indicarse como 707 ppm solamente bajo
  determinada conversión;
- registra exactamente lo escrito en la etiqueta.

### Procedimiento

1. Enjuaga la sonda TDS.
2. Coloca la solución patrón en un vaso limpio.
3. Mide y registra la temperatura con DS18B20.
4. Sumerge hasta la profundidad recomendada, sin burbujas.
5. Espera estabilidad y registra 30 lecturas de voltaje.
6. Calcula el promedio.
7. Repite con un segundo patrón dentro del rango de uso, si está disponible.

### Conversión empírica de dos puntos

Si el manual de tu módulo no proporciona una biblioteca o ecuación, usa:

```text
TDS_ppm = a × V + c
a = (ppm2 - ppm1) / (V2 - V1)
c = ppm1 - a × V1
```

Calculadora:

```bash
python 07_HERRAMIENTAS/calcular_calibracion_agua.py \
  --tipo tds --v1 V1 --ref1 PPM1 --v2 V2 --ref2 PPM2
```

Esta aproximación debe verificarse con un tercer punto. No extrapoles fuera del
rango de los patrones.

### Si el módulo es DFRobot SEN0244

Usa preferentemente su biblioteca y procedimiento oficial. El fabricante
especifica 0–2.3 V, 0–1000 ppm y compensación usando temperatura. No copies su
polinomio a otro módulo sin identificarlo.

## 4. Comprobar DS18B20

1. Coloca DS18B20 y termómetro de referencia juntos en agua agitada suavemente.
2. Espera estabilidad.
3. Registra cinco pares de temperaturas.
4. Calcula:

```text
offset = temperatura_referencia - temperatura_DS18B20
temperatura_corregida = temperatura_DS18B20 + offset
```

Comprueba al menos dos temperaturas relevantes para el sistema. No uses el punto
de ebullición sin corregir por altitud y presión.

## 5. Calibrar nivel de los tanques

El firmware actual reporta distancia desde el sensor hasta el agua.

Para cada tanque registra:

- `D_vacio`: distancia con tanque vacío;
- `D_lleno`: distancia al nivel máximo permitido;
- altura/volumen útil real.

Conversión:

```text
nivel_pct = 100 × (D_vacio - D_actual) / (D_vacio - D_lleno)
```

Limita el resultado entre 0 y 100 %. El firmware define una zona ciega de 19 cm:
el nivel máximo debe mantener el agua fuera de esa zona. Monta el sensor
perpendicular a la superficie y evita paredes, tuberías, espuma y turbulencia.

Para volumen de un tanque vertical de sección constante:

```text
volumen_actual = volumen_util × nivel_pct / 100
```

Para formas irregulares se necesita una tabla distancia-volumen medida.

## 6. Qué registrar antes de modificar el firmware

Completa `HOJA_CALIBRACION_AGUA.csv` y conserva:

- fotografía de cada módulo y modelo;
- fecha, ubicación y temperatura;
- voltajes individuales y promedios;
- valor y escala de cada patrón;
- pendiente/intercepto calculados;
- error al verificar con un patrón independiente;
- distancias de lleno/vacío;
- observaciones sobre ruido y estabilidad.

Después de campo se deben incorporar constantes al firmware, enviar campos como
`waterPh` y `tdsPpm`, y cambiar `phCalibrated` / `tdsCalibrated` a `true` solo
cuando la verificación sea satisfactoria.

