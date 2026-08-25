# Firmware Arduino / ESP32

## Sketches reales

### `01_Nodo_Invernadero_Real`

Lee el sensor CWT-NPKPHCTH-S mediante RS485/Modbus, DHT22 y BH1750. Envía la
trama del nodo `tlali-npk-01` mediante LoRa y muestra errores en OLED.

Pines principales:

| Función | GPIO |
|---|---:|
| RS485 RX / RO con divisor | 7 |
| RS485 TX / DI | 6 |
| RS485 DE | 5 |
| DHT22 | 4 |
| BH1750 SDA/SCL | 41 / 42 |
| OLED SDA/SCL/RST | 17 / 18 / 21 |

### `02_Nodo_Actuadores_Real`

Lee dos RCWL-1655, voltajes de pH/TDS, DS18B20 y estados de dos relés. Envía
`tlali-actuadores-01`. Los relés permanecen apagados en modo seguro.

| Función | GPIO |
|---|---:|
| Ultrasonido 1 TRIG/ECHO | 4 / 5 |
| Ultrasonido 2 TRIG/ECHO | 6 / 7 |
| pH | 1 |
| TDS | 2 |
| Relé 1 / Relé 2 | 33 / 34 |
| DS18B20 | 47 |
| OLED SDA/SCL/RST | 17 / 18 / 21 |

La salida de pH usa divisor: 10 kΩ entre Po y ADC, 20 kΩ entre ADC y GND. El
DS18B20 usa resistencia de 4.7 kΩ entre DATA y 3.3 V.

### `03_Gateway_OrangePi`

Recibe G1 y A1, verifica CRC, responde ACK y entrega líneas `TLALI_DATA:` por
USB a 115200 baudios.

## Placa y librerías

Placa usada: **Heltec WiFi LoRa 32 V3 / ESP32-S3**.

Instala en Arduino IDE:

- soporte de placas ESP32;
- Heltec ESP32 LoRa V3 / `heltec_unofficial`;
- RadioLib, incluida según la biblioteca Heltec utilizada;
- Adafruit GFX Library;
- Adafruit SSD1306;
- DHT sensor library;
- Adafruit Unified Sensor;
- OneWire;
- DallasTemperature;
- Wire y SPI, incluidas con ESP32.

## Carga segura

1. Conecta siempre una antena de 915 MHz.
2. Selecciona la placa y puerto correctos.
3. Compila antes de cargar.
4. Carga el gateway primero, luego G1 y finalmente A1.
5. Abre monitor serial a 115200.
6. Comprueba que `radio.begin()` y parámetros LoRa devuelvan `ERR_NONE`.
7. No cambies un parámetro LoRa en un solo equipo: deben coincidir los tres.

## Estado de calibración

El firmware real reporta `phVoltageV` y `tdsVoltageV`. Todavía no convierte a
pH ni ppm. Sigue `../04_GUIAS/CALIBRACION_SENSORES_AGUA.md`, registra los
coeficientes y actualiza el firmware después de verificar las mediciones.

