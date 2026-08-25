# Gateway LoRa para Orange Pi

Abre `03_Gateway_OrangePi.ino` con Arduino IDE.

Funciones:

- recibe nodos G1 y A1 en 915 MHz;
- valida versión, contenido y CRC;
- reconstruye JSON completo;
- añade RSSI y SNR;
- responde ACK a cada nodo;
- imprime `TLALI_DATA:{...}` por USB a 115200.

Después de cargarlo, conéctalo por USB de datos a Orange Pi. El servicio
`tlali-lora.service` abre su enlace estable bajo `/dev/serial/by-id/`.

No conectes o energices el gateway LoRa sin antena.

