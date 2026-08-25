#!/usr/bin/env python3
import argparse
from datetime import datetime, timedelta, timezone
from pathlib import Path

from almacen_local import AlmacenLocal


def argumentos():
    parser = argparse.ArgumentParser(
        description=(
            "Elimina de SQLite solamente tramas TLALI confirmadas en Firebase."
        )
    )
    parser.add_argument("--db", default="datos/tlali.db")
    parser.add_argument(
        "--retener-dias",
        type=int,
        default=7,
        help="Días que se conservan después de confirmar el envío (predeterminado: 7).",
    )
    parser.add_argument(
        "--lote",
        type=int,
        default=5000,
        help="Filas eliminadas por transacción (predeterminado: 5000).",
    )
    parser.add_argument(
        "--simular",
        action="store_true",
        help="Cuenta las filas pero no elimina nada.",
    )
    return parser.parse_args()


def fecha_limite(retener_dias):
    return (
        (datetime.now(timezone.utc) - timedelta(days=retener_dias))
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def tamanio_archivos(ruta):
    ruta = Path(ruta)
    total = 0
    detalles = []
    for sufijo in ("", "-wal", "-shm"):
        archivo = Path(f"{ruta}{sufijo}")
        if archivo.exists():
            tamanio = archivo.stat().st_size
            total += tamanio
            detalles.append((archivo.name, tamanio))
    return total, detalles


def formato_bytes(cantidad):
    valor = float(cantidad)
    for unidad in ("B", "KB", "MB", "GB", "TB"):
        if valor < 1024 or unidad == "TB":
            return f"{valor:.1f} {unidad}"
        valor /= 1024


def limpiar(ruta_db, retener_dias, lote, simular=False):
    if retener_dias < 1 or retener_dias > 3650:
        raise ValueError("--retener-dias debe estar entre 1 y 3650")
    if lote < 100 or lote > 100000:
        raise ValueError("--lote debe estar entre 100 y 100000")

    limite = fecha_limite(retener_dias)
    with AlmacenLocal(ruta_db) as almacen:
        candidatas = almacen.contar_enviadas_anteriores(limite)
        total, pendientes, _ = almacen.resumen()
        antes_bytes, _ = tamanio_archivos(almacen.ruta)

        print(f"Base local: {almacen.ruta}")
        print(f"Conservación: {retener_dias} días")
        print(f"Fecha límite UTC: {limite}")
        print(f"Filas totales antes: {total}")
        print(f"Pendientes protegidas: {pendientes}")
        print(f"Enviadas candidatas: {candidatas}")
        print(f"Espacio antes: {formato_bytes(antes_bytes)}")

        if simular:
            print("Simulación: no se eliminó ninguna fila.")
            return candidatas

        eliminadas = 0
        while True:
            cantidad = almacen.limpiar_enviadas_anteriores(limite, lote)
            eliminadas += cantidad
            if cantidad < lote:
                break

        try:
            resultado_wal = almacen.truncar_wal()
            print(f"Checkpoint WAL: {resultado_wal}")
        except Exception as error:
            # No invalida la limpieza. SQLite reutilizará el WAL y se intentará
            # truncar de nuevo en el siguiente mantenimiento.
            print(f"Aviso: no se pudo truncar WAL: {error}")

        despues_total, despues_pendientes, _ = almacen.resumen()
        paginas = almacen.estadisticas_paginas()
        despues_bytes, _ = tamanio_archivos(almacen.ruta)

        print(f"Filas eliminadas: {eliminadas}")
        print(f"Filas totales después: {despues_total}")
        print(f"Pendientes después: {despues_pendientes}")
        print(f"Espacio actual: {formato_bytes(despues_bytes)}")
        print(
            "Espacio reutilizable dentro de SQLite: "
            f"{formato_bytes(paginas['reusable_bytes'])}"
        )
        return eliminadas


def main():
    args = argumentos()
    try:
        limpiar(
            args.db,
            args.retener_dias,
            args.lote,
            args.simular,
        )
    except ValueError as error:
        raise SystemExit(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
