# Nodo 1: invernadero real

Abre `01_Nodo_Invernadero_Real.ino` con Arduino IDE.

Este sketch:

- consulta CWT-NPKPHCTH-S por RS485/Modbus;
- lee DHT22;
- detecta BH1750 en `0x23` o `0x5C`;
- muestra estado, valores y errores en OLED;
- transmite como radio `G1` y nodo `tlali-npk-01`;
- espera ACK del gateway;
- gira la pantalla cada minuto y actualiza inmediatamente al cambiar un error.

Monitor serial: 115200 baudios.

Antes de cargar comprueba los pines y librerías en el README de la carpeta
superior. No energices el RS485 ni el sensor CWT con un voltaje incorrecto.

