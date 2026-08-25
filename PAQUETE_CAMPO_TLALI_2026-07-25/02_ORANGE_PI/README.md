# Software de Orange Pi

Ruta de instalación:

```text
/home/orangepi/tlali/orange-pi
```

## Componentes

| Archivo | Función |
|---|---|
| `recibir_lora.py` | Lee USB, valida JSON y guarda SQLite |
| `almacen_local.py` | Esquema y operaciones de SQLite |
| `consultar_datos.py` | Consulta rápida desde terminal |
| `sincronizar_firebase.py` | Sube pendientes a Firebase |
| `limpiar_sqlite.py` | Elimina enviados de más de 7 días |
| `monitor_local.py` | Aplicación HDMI nativa |
| `test_sincronizacion.py` | Prueba del sincronizador |
| `test_limpieza.py` | Prueba de protección y borrado |

## Servicios instalados

```bash
systemctl status tlali-lora.service --no-pager
systemctl status tlali-firebase.service --no-pager
systemctl status tlali-limpieza.timer --no-pager
```

Ver registros:

```bash
journalctl -u tlali-lora.service -n 50 --no-pager
journalctl -u tlali-firebase.service -n 50 --no-pager
journalctl -u tlali-limpieza.service -n 50 --no-pager
```

Consultar SQLite:

```bash
cd /home/orangepi/tlali/orange-pi
.venv/bin/python consultar_datos.py --ultimas 10
```

## Monitor HDMI

Se abre desde el acceso directo **TLALI Monitor Local**. También puede abrirse:

```bash
/usr/bin/python3 /home/orangepi/tlali/orange-pi/monitor_local.py \
  --db /home/orangepi/tlali/orange-pi/datos/tlali.db
```

- `Esc`: cerrar.
- `F11`: pantalla completa/ventana.
- Actuales: cada 3 segundos.
- Historial local: últimas 24 horas, actualización cada 30 segundos.

## Almacenamiento

- Base: `/home/orangepi/tlali/orange-pi/datos/tlali.db`
- Conservación local confirmada: 7 días.
- Solo se borran filas `enviado`.
- `pendiente` y `error` nunca se eliminan por la limpieza.

## Firebase

- URL: `https://tlali-5edc4-default-rtdb.firebaseio.com/`
- Raíz: `/tlali`
- Actual: `/tlali/actual/{nodo}`
- Historial: `/tlali/historial/{nodo}/{AAAA-MM-DD}/{HH-mm}`

La llave privada no está incluida en este paquete.

