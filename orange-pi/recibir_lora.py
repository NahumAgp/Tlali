#!/usr/bin/env python3
import argparse
import json
import sqlite3
import sys

import serial

from almacen_local import AlmacenLocal


PREFIJO = "TLALI_DATA:"


def argumentos():
    parser = argparse.ArgumentParser(
        description="Recibe las tramas TLALI desde la Heltec gateway."
    )
    parser.add_argument(
        "puerto",
        help="Puerto estable de la Heltec, por ejemplo /dev/serial/by-id/usb-Silicon_Labs_...",
    )
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument(
        "--db",
        default="datos/tlali.db",
        help="Ruta de la base SQLite local.",
    )
    return parser.parse_args()


def main():
    args = argumentos()
    try:
        with AlmacenLocal(args.db) as almacen, serial.Serial(
            args.puerto,
            args.baud,
            timeout=2,
        ) as enlace:
            print(f"Escuchando {args.puerto} a {args.baud} baud", flush=True)
            print(f"Guardando tramas en {almacen.ruta}", flush=True)
            while True:
                linea = enlace.readline().decode("utf-8", errors="replace").strip()
                if not linea.startswith(PREFIJO):
                    continue

                try:
                    trama = json.loads(linea[len(PREFIJO) :])
                except json.JSONDecodeError as error:
                    print(f"Trama JSON invalida: {error}", file=sys.stderr)
                    continue

                try:
                    identificador, insertada = almacen.guardar(trama)
                except (ValueError, sqlite3.Error) as error:
                    print(
                        f"No se pudo guardar la trama: {error}",
                        file=sys.stderr,
                        flush=True,
                    )
                    continue

                print(json.dumps(trama, ensure_ascii=False), flush=True)
                if not insertada:
                    print(
                        f"Trama duplicada omitida: id={identificador}",
                        flush=True,
                    )
    except serial.SerialException as error:
        print(f"No se pudo abrir el puerto: {error}", file=sys.stderr)
        return 1
    except sqlite3.Error as error:
        print(f"No se pudo abrir la base local: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("\nRecepcion detenida.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
