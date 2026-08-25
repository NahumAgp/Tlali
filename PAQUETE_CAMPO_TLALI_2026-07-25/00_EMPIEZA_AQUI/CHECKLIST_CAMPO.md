# Checklist de instalación y pruebas de campo

## Material físico

- [ ] Laptop y cargador
- [ ] Memoria USB con este paquete
- [ ] Orange Pi, fuente y microSD
- [ ] Pantalla HDMI, cable HDMI, teclado y mouse
- [ ] Gateway Heltec LoRa 32 y cable USB de datos
- [ ] Nodo de invernadero y fuente
- [ ] Nodo de actuadores y fuente
- [ ] Tres antenas de 915 MHz colocadas
- [ ] Cable Ethernet
- [ ] Módem/antena celular y SIM, si se probará Internet móvil
- [ ] Extensión eléctrica y regulador
- [ ] Multímetro
- [ ] Desarmadores, terminales, cinta, termocontráctil
- [ ] Resistencias de repuesto: 4.7 kΩ, 10 kΩ y 20 kΩ
- [ ] Convertidor USB-UART/RS485 de respaldo

## Material de calibración de agua

- [ ] Solución buffer pH 7.00
- [ ] Solución buffer pH 4.00
- [ ] Solución buffer pH 10.00 opcional para verificación
- [ ] Solución patrón TDS/EC con valor y escala claramente indicados
- [ ] Agua destilada para enjuagar
- [ ] Solución KCl para guardar el electrodo de pH
- [ ] Vasos limpios separados para cada solución
- [ ] Papel sin pelusa para retirar gotas sin frotar el bulbo
- [ ] Termómetro de referencia
- [ ] Hoja impresa `HOJA_CALIBRACION_AGUA.csv`

## Antes de energizar

- [ ] Antena conectada en cada Heltec
- [ ] Polaridad y voltaje de fuentes comprobados
- [ ] Tierra común entre ESP32 y módulos analógicos
- [ ] ECHO de ultrasónicos no supera 3.3 V en el ESP32
- [ ] Salida de pH pasa por el divisor 10 kΩ / 20 kΩ
- [ ] TDS no supera el rango del ADC
- [ ] DS18B20 tiene pull-up de 4.7 kΩ a 3.3 V
- [ ] Relés y cargas permanecen desconectados durante pruebas de sensores

## Orange Pi

- [ ] `/dev/ttyUSB0` o enlace `/dev/serial/by-id/` presente
- [ ] `tlali-lora.service` activo
- [ ] `tlali-firebase.service` activo
- [ ] `tlali-limpieza.timer` activo
- [ ] TLALI Monitor Local abre desde el escritorio
- [ ] SQLite recibe ambos nodos
- [ ] Firebase recibe ambos nodos cuando hay Internet

## Prueba LoRa

- [ ] Nodo G1 recibe ACK
- [ ] Nodo A1 recibe ACK
- [ ] RSSI y SNR registrados en la ubicación definitiva
- [ ] Prueba hecha con puertas/cubiertas en su posición real
- [ ] Gateway y antenas lejos de fuentes de ruido y metal

## Antes de retirarse

- [ ] Fotografiar cableado y etiquetas
- [ ] Guardar resultados de calibración
- [ ] Confirmar fecha/hora de Orange Pi
- [ ] Confirmar al menos una lectura reciente por nodo
- [ ] Confirmar que la limpieza conserva 7 días
- [ ] Dejar los relés en estado seguro
- [ ] Respaldar SQLite si la prueba generó datos importantes

