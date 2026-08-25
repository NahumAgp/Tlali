# Respaldo de datos y credenciales

## Datos reales

La base real no vive en esta computadora. Está en:

```text
/home/orangepi/tlali/orange-pi/datos/tlali.db
```

Para copiarla de forma consistente, primero crea un respaldo con SQLite en la
Orange Pi:

```bash
cd /home/orangepi/tlali/orange-pi
.venv/bin/python -c "import sqlite3; a=sqlite3.connect('datos/tlali.db'); b=sqlite3.connect('/home/orangepi/tlali-respaldo.db'); a.backup(b); b.close(); a.close()"
```

Luego, desde Windows:

```powershell
scp orangepi@IP_REAL:/home/orangepi/tlali-respaldo.db .
```

## Firebase

Datos:

```text
https://tlali-5edc4-default-rtdb.firebaseio.com/tlali
```

Credencial en Orange Pi:

```text
/home/orangepi/tlali/credenciales/firebase-service-account.json
```

No coloques esa credencial en repositorios, WordPress ni carpetas públicas. Si
debes llevar una copia en USB, usa una memoria bajo tu control y evita dejarla
conectada. En destino:

```bash
chmod 600 /home/orangepi/tlali/credenciales/firebase-service-account.json
```

El dashboard Divi nunca necesita la cuenta de servicio; solo lee la URL pública
temporal de Firebase.

## Recuperación

Los instaladores crean respaldos bajo:

```text
/home/orangepi/tlali/orange-pi/respaldos/
```

No borres esa carpeta durante las pruebas de campo.

