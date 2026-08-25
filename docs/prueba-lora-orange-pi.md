# Prueba LoRa: invernadero, gateway y Orange Pi

## Arquitectura

```text
Heltec V3 - nodo 1 simulado
  -> LoRa punto a punto 915 MHz
  -> Heltec V3 - gateway
  -> USB serie 115200
  -> Orange Pi
  -> Supabase (fase posterior)
```

El enlace implementado es LoRa punto a punto, no LoRaWAN. El gateway confirma
cada lectura con un ACK que incluye RSSI y SNR. El nodo sabe así si la trama
llegó realmente al equipo conectado a la Orange Pi.

## Material para la primera prueba

- Dos Heltec WiFi LoRa 32 V3 de la misma banda.
- Dos antenas de 915 MHz conectadas antes de energizar o transmitir.
- Un cable USB de datos para programar cada placa.
- Un puerto USB host disponible en la Orange Pi.

No se necesita cablear SPI, UART ni GPIO entre la Orange Pi y la Heltec
gateway. La conexión recomendada es USB porque entrega alimentación, datos,
masa común y aislamiento frente a errores de niveles lógicos.

## Programación de las placas

1. En Arduino IDE instalar `Heltec_ESP32_LoRa_v3` versión 0.9.2. Sus
   dependencias incluyen RadioLib.
2. Seleccionar `Heltec WiFi LoRa 32(V3) / Wireless shell(V3)`.
3. Cargar `01_Nodo_Invernadero_Prueba.ino` en la placa simuladora.
4. Cargar `03_Gateway_OrangePi.ino` en la placa que quedará por USB.
5. Mantener ambas antenas conectadas y separar las placas al menos un metro
   durante la prueba.

Configuración usada por ambos equipos:

| Parámetro | Valor |
|---|---:|
| Frecuencia | 915.0 MHz |
| Ancho de banda | 125 kHz |
| Spreading factor | 9 |
| Coding rate | 4/7 |
| Sync word | `0x12` |
| Preámbulo | 8 símbolos |
| Potencia inicial | 2 dBm |
| Periodo de lectura simulada | 5 segundos |

La potencia es deliberadamente baja para pruebas de mesa. No debe aumentarse
sin revisar antena, distancia y regulación local.

## Trama por radio

Por LoRa se transmite una trama compacta menor de 255 bytes:

```text
D|version|nodo|secuencia|uptime|datos escalados|validez|CRC16
```

Contiene humedad y temperatura del sustrato, conductividad, pH, N, P, K,
humedad y temperatura ambiente y luminosidad. Los valores cambian mediante una
caminata aleatoria dentro de rangos realistas.

El gateway valida el CRC16 y reconstruye el formato completo:

```text
TLALI_DATA:{"v":1,"node":"tlali-npk-01",...}
```

También añade:

```json
{
  "radio": {
    "rssiDbm": -54.2,
    "snrDb": 9.5
  }
}
```

## Conexión con la Orange Pi

1. Conectar el USB-C de la Heltec gateway a un USB host de la Orange Pi usando
   un cable que soporte datos.
2. En Armbian/Ubuntu localizar el puerto estable:

```bash
ls -l /dev/serial/by-id/
```

La Heltec usa un convertidor CP2102 y normalmente también aparece como
`/dev/ttyUSB0`. Es preferible la ruta `/dev/serial/by-id/...` porque no cambia
si se conecta otro dispositivo USB.

3. Autorizar al usuario para abrir puertos serie:

```bash
sudo usermod -aG dialout "$USER"
```

Cerrar sesión y volver a entrar después de este comando.

4. Preparar el receptor:

```bash
cd orange-pi
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python recibir_lora.py /dev/serial/by-id/EL_IDENTIFICADOR_REAL
```

La salida será un objeto JSON cada cinco segundos. Este programa todavía no
escribe en Supabase: constituye el punto de entrada que se conectará a la base
de datos en la siguiente fase.

## Arranque automático

El archivo `orange-pi/tlali-lora.service` ejecuta el receptor con el usuario
`orangepi`, el grupo `dialout` y la ruta USB estable del CP2102. Para instalarlo:

```bash
sudo cp ~/tlali/orange-pi/tlali-lora.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now tlali-lora.service
systemctl status tlali-lora.service
```

Para observar las tramas en tiempo real:

```bash
journalctl -u tlali-lora.service -f
```

## Diagnóstico rápido

- `Sin ACK del gateway`: revisar que ambos firmwares compartan parámetros,
  que las antenas correspondan a la banda y que el gateway esté encendido.
- Error de inicialización de radio: confirmar que la placa seleccionada sea
  exactamente Heltec V3 y no V2.
- No aparece un puerto en Linux: cambiar el cable USB; muchos cables sólo
  proporcionan alimentación.
- Permiso denegado: comprobar pertenencia al grupo `dialout`.
- No transmitir nunca sin antena conectada.
