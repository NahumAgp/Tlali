# Nodos, variables y trama de datos

## Formato común

Cada nodo emite una línea independiente con el prefijo `TLALI_DATA:` seguido
de un objeto JSON. El gateway puede ignorar las líneas de diagnóstico y
procesar únicamente las que comiencen con ese prefijo.

Campos del sobre:

| Campo | Tipo | Uso |
|---|---|---|
| `v` | entero | Versión del contrato de trama; actualmente `1` |
| `node` | texto | Identificador estable y único del nodo |
| `type` | texto | `sensor` o `actuator` |
| `seq` | entero sin signo | Contador de tramas desde el último reinicio |
| `uptimeMs` | entero sin signo | Milisegundos encendido; ayuda a detectar reinicios |
| `data` | objeto | Mediciones y estados propios del nodo |
| `valid` | objeto | Calidad o disponibilidad de cada grupo de datos |

## Nodo NPK y ambiente

Identificador propuesto: `tlali-npk-01`.

| Variable en trama | Tipo/unidad | Fuente |
|---|---|---|
| `soilMoisturePct` | decimal, % | CWT por RS485/Modbus |
| `soilTemperatureC` | decimal, °C | CWT por RS485/Modbus |
| `conductivityUsCm` | entero, µS/cm | CWT por RS485/Modbus |
| `ph` | decimal, 0-14 | CWT por RS485/Modbus |
| `nitrogenMgKg` | entero, mg/kg | CWT por RS485/Modbus |
| `phosphorusMgKg` | entero, mg/kg | CWT por RS485/Modbus |
| `potassiumMgKg` | entero, mg/kg | CWT por RS485/Modbus |
| `airHumidityPct` | decimal, % | DHT22 |
| `airTemperatureC` | decimal, °C | DHT22 |
| `lightLux` | decimal, lux | BH1750 |
| `valid.soil` | booleano | CRC, longitud y encabezado Modbus correctos |
| `valid.air` | booleano | DHT22 entregó temperatura y humedad |
| `valid.light` | booleano | BH1750 entregó dos bytes |

Ejemplo:

```json
{
  "v": 1,
  "node": "tlali-npk-01",
  "type": "sensor",
  "seq": 42,
  "uptimeMs": 185230,
  "data": {
    "soilMoisturePct": 34.8,
    "soilTemperatureC": 22.6,
    "conductivityUsCm": 712,
    "ph": 6.7,
    "nitrogenMgKg": 48,
    "phosphorusMgKg": 31,
    "potassiumMgKg": 96,
    "airHumidityPct": 58.2,
    "airTemperatureC": 25.1,
    "lightLux": 12450.0
  },
  "valid": {"soil": true, "air": true, "light": true}
}
```

Si un sensor falla, su valor se envía como `null` y la bandera correspondiente
queda en `false`. No se reutiliza silenciosamente una lectura anterior.

## Nodo de actuadores y solución

Identificador propuesto: `tlali-actuadores-01`.

| Variable en trama | Tipo/unidad | Fuente o uso |
|---|---|---|
| `tank1DistanceCm` | decimal, cm | RCWL-1655 número 1 |
| `tank2DistanceCm` | decimal, cm | RCWL-1655 número 2 |
| `phVoltageV` | decimal, V | Salida Po del módulo de pH |
| `tdsVoltageV` | decimal, V | Salida A del módulo TDS |
| `solutionTemperatureCelsius` | decimal, °C | DS18B20 de tres cables |
| `relay1On` | booleano | Estado lógico del relé 1 |
| `relay2On` | booleano | Estado lógico del relé 2 |
| `valid.tank1` | booleano | Se recibió eco antes del timeout |
| `valid.tank2` | booleano | Se recibió eco antes del timeout |
| `valid.temperature` | booleano | El DS18B20 respondió dentro de su rango |
| `valid.phCalibrated` | booleano | `false` hasta calibrar y convertir a pH |
| `valid.tdsCalibrated` | booleano | `false` hasta calibrar y convertir a ppm |

Ejemplo:

```json
{
  "v": 1,
  "node": "tlali-actuadores-01",
  "type": "actuator",
  "seq": 108,
  "uptimeMs": 94120,
  "data": {
    "tank1DistanceCm": 42.3,
    "tank2DistanceCm": null,
    "phVoltageV": 2.487,
    "tdsVoltageV": 1.126,
    "solutionTemperatureCelsius": 24.75,
    "relay1On": false,
    "relay2On": false
  },
  "valid": {
    "tank1": true,
    "tank2": false,
    "temperature": true,
    "phCalibrated": false,
    "tdsCalibrated": false
  }
}
```

Los valores de pH y TDS permanecen como voltajes: convertirlos antes de
calibrar produciría datos con apariencia válida pero científicamente
incorrectos.

## Recorrido de la información

```text
Sensores físicos
  -> lectura y validación en cada ESP32
  -> trama TLALI_DATA (JSON por línea)
  -> gateway/transporte
  -> API
  -> base de datos
  -> tablero web
```

La API existente todavía no representa toda la trama. Actualmente conserva
temperatura ambiente, humedad ambiente, humedad de suelo, luz y batería. Para
no perder datos, su siguiente versión debe añadir las variables de suelo,
nutrientes, tanques, voltajes/valores calibrados, estados de relés, versión,
secuencia, tiempo encendido y banderas de validez.
