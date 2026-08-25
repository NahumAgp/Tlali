# Almacenamiento local en la Orange Pi

El receptor guarda cada trama válida en:

```text
/home/orangepi/tlali/orange-pi/datos/tlali.db
```

La base utiliza SQLite en modo WAL para tolerar reinicios y cortes de energía.
Cada trama queda con estado `pendiente` hasta que posteriormente se confirme
su envío a Supabase.

## Instalación

1. Copiar el contenido del paquete `tlali-orange-pi-almacen-local.zip` a una
   memoria USB.
2. Abrir la memoria desde la Orange Pi y extraer el paquete.
3. Abrir una terminal dentro de la carpeta extraída.
4. Ejecutar:

```bash
chmod +x instalar_almacen_local.sh
./instalar_almacen_local.sh
```

El instalador conserva una copia de los archivos anteriores dentro de
`/home/orangepi/tlali/orange-pi/respaldos/`, instala el servicio y lo reinicia.

## Verificación

Esperar aproximadamente treinta segundos y ejecutar:

```bash
cd ~/tlali/orange-pi
.venv/bin/python consultar_datos.py
```

La salida debe incluir los dos nodos:

```text
tlali-actuadores-01
tlali-npk-01
```

Para ver más registros:

```bash
.venv/bin/python consultar_datos.py --ultimas 20
```

Para observar el receptor:

```bash
journalctl -u tlali-lora.service -f
```

## Datos almacenados

La tabla `telemetria` conserva:

- fecha UTC de recepción;
- identificador del nodo;
- tipo de trama;
- secuencia y tiempo encendido;
- JSON completo;
- estado e intentos de sincronización;
- fecha de envío y último error, cuando correspondan.

La combinación de nodo, secuencia y tiempo encendido evita guardar dos veces
la misma transmisión.
