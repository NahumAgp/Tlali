# Puesta en marcha de los nodos reales

## Programas

1. `03_Gateway_OrangePi.ino`: LoRa central conectada por USB a la Orange Pi.
2. `01_Nodo_Invernadero.ino`: nodo real de sustrato y ambiente, identificado como `G1`.
3. `02_Nodo_Actuadores.ino`: nodo real de bomba, depósitos y solución, identificado como `A1`.

Los tres usan la misma configuración LoRa: 915 MHz, ancho de banda de
125 kHz, factor de propagación 9, codificación 4/7 y palabra de sincronización
`0x12`.

## Orden de carga

1. Cargar primero el gateway en la Heltec conectada a la Orange Pi.
2. Volver a conectar esa Heltec por USB a la Orange Pi.
3. Cargar el programa del invernadero en su Heltec.
4. Cargar el programa de actuadores en la Heltec del nodo de bomba.
5. Encender los dos nodos con sus antenas LoRa instaladas.

No se debe transmitir sin una antena adecuada para 915 MHz conectada.

## Comprobación en los nodos

En el monitor serie, el invernadero debe mostrar mensajes `ACK G1` y el nodo
de actuadores debe mostrar `ACK A1`. Si aparece `Sin ACK`, comprobar primero
que el gateway esté encendido, que las antenas estén conectadas y que ninguna
placa tenga abierto otro monitor serie.

## Diagnóstico en las pantallas OLED

Las dos pantallas muestran el resultado del último intercambio:

- `CENTRAL: OK` y el RSSI indican que llegó un ACK válido del gateway.
- `CENTRAL: SIN ACK` indica que no se confirmó la última transmisión.

Las páginas de datos cambian una vez por minuto. Un cambio de estado de un
sensor o del enlace central actualiza la pantalla inmediatamente, sin esperar
al siguiente minuto.

Si falla una lectura, la pantalla deja temporalmente las páginas de datos y
muestra qué conexión debe revisarse.

En el nodo de invernadero puede indicar:

- `Revisa SUELO RS485`
- `Revisa DHT22`
- `Revisa BH1750`

En el nodo de actuadores puede indicar:

- `Revisa ULTRASON 1`
- `Revisa ULTRASON 2`
- `Revisa DS18B20`
- `Revisa PH / ADC`
- `Revisa TDS / ADC`

Los sensores analógicos de pH y TDS no entregan una identificación digital.
Por eso el programa puede detectar voltaje nulo, saturación o una señal fuera
del rango eléctrico, pero no siempre puede distinguir entre un sensor
desconectado y una entrada flotante.

## Visualización en la Orange Pi

El servicio existente acepta los dos tipos de trama. Para ver los datos en
tiempo real:

```bash
journalctl -u tlali-lora.service -f
```

Se verán líneas JSON alternadas con:

- `"node":"tlali-npk-01"` para sustrato y ambiente.
- `"node":"tlali-actuadores-01"` para depósitos, pH/TDS, temperatura y relés.

Para detener únicamente la vista se usa `Ctrl+C`; el servicio continúa
recibiendo en segundo plano.

## Estado seguro del nodo de bomba

Los relés arrancan y permanecen apagados en esta versión. El programa informa
su estado real, pero todavía no toma decisiones automáticas de riego.

Los módulos de pH y TDS se reportan como voltaje (`phVoltageV` y
`tdsVoltageV`). No se convierten todavía a pH o ppm porque primero se deben
calibrar físicamente con soluciones patrón.
