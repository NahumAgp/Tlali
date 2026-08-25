# Comandos rápidos

Esta carpeta contiene archivos de comandos usados durante la instalación.

## IP de Orange Pi

En Orange Pi:

```bash
hostname -I
```

No escribas literalmente `IP_DE_LA_ORANGE_PI`; reemplázalo por el número real.

## Copiar desde Windows

Ejemplo:

```powershell
scp "C:\ruta\archivo.zip" orangepi@192.168.1.77:/home/orangepi/
```

## Puertos USB

```bash
ls -l /dev/serial/by-id/ /dev/ttyUSB* 2>/dev/null
```

## Ver datos en vivo

```bash
journalctl -u tlali-lora.service -f
```

Salir de esa vista con `Ctrl+C`; el servicio continúa.

## Servicios

```bash
systemctl status tlali-lora.service --no-pager
systemctl status tlali-firebase.service --no-pager
systemctl status tlali-limpieza.timer --no-pager
```

