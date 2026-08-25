# Paquete de campo TLALI

Fecha de preparación: 25 de julio de 2026.

Este paquete reúne los archivos necesarios para instalar, comprobar y dar
mantenimiento al sistema TLALI en campo:

- firmware real de los dos nodos y el gateway LoRa;
- receptor, almacenamiento, Firebase, limpieza y monitor HDMI de Orange Pi;
- dashboard para WordPress/Divi;
- comandos de instalación y diagnóstico;
- documentación de tramas y puesta en marcha;
- guía y hoja de calibración de sensores de agua;
- instaladores ZIP que se pueden enviar por SSH.

## Orden recomendado mañana

1. Lee `CHECKLIST_CAMPO.md`.
2. Lleva las antenas colocadas antes de energizar cualquier radio LoRa.
3. Enciende primero Orange Pi y gateway.
4. Comprueba `tlali-lora.service` y `tlali-firebase.service`.
5. Enciende nodo de invernadero y comprueba ACK.
6. Enciende nodo de actuadores y comprueba ACK.
7. Abre **TLALI Monitor Local** en el escritorio de la Orange Pi.
8. Verifica Firebase solo si existe conexión a Internet.
9. Calibra agua siguiendo `04_GUIAS/CALIBRACION_SENSORES_AGUA.md`.
10. Copia o fotografía la hoja de resultados antes de retirarte.

## Carpetas

| Carpeta | Contenido |
|---|---|
| `01_FIRMWARE_ARDUINO` | Sketches `.ino`, pines y librerías |
| `02_ORANGE_PI` | Servicios, aplicaciones e instaladores |
| `03_DASHBOARD_WORDPRESS_DIVI` | Panel web para pegar en Divi |
| `04_GUIAS` | Calibración, tramas y puesta en marcha |
| `05_COMANDOS_RAPIDOS` | Comandos listos para copiar |
| `06_RESPALDO_Y_SEGURIDAD` | Datos, credenciales y recuperación |
| `07_HERRAMIENTAS` | Calculadora de calibración |

## Datos y credenciales

Este paquete **no contiene** la llave privada
`firebase-service-account.json`, contraseñas ni una copia de la base real de la
Orange Pi.

- Los datos actuales están en la Orange Pi:
  `/home/orangepi/tlali/orange-pi/datos/tlali.db`
- Los datos sincronizados están en Firebase:
  `https://tlali-5edc4-default-rtdb.firebaseio.com/tlali`
- La credencial está en la Orange Pi:
  `/home/orangepi/tlali/credenciales/firebase-service-account.json`

Consulta `06_RESPALDO_Y_SEGURIDAD/README.md` antes de copiar esos archivos.

## Configuración LoRa confirmada

- Frecuencia: 915.0 MHz
- Ancho de banda: 125 kHz
- Spreading factor: 9
- Coding rate: 4/7
- Sync word: `0x12`
- Potencia: 17 dBm
- Nodo invernadero: radio `G1`, nodo `tlali-npk-01`
- Nodo actuadores: radio `A1`, nodo `tlali-actuadores-01`

Los tres dispositivos deben conservar exactamente los mismos parámetros LoRa.

