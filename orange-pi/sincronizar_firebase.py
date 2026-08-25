#!/usr/bin/env python3
import argparse
import time
from collections import defaultdict
from pathlib import Path

from almacen_local import AlmacenLocal, fecha_utc


def argumentos():
    parser = argparse.ArgumentParser(
        description="Sincroniza la cola SQLite TLALI con Firebase Realtime Database."
    )
    parser.add_argument("--db", default="datos/tlali.db")
    parser.add_argument(
        "--credencial",
        required=True,
        help="Archivo JSON privado de la cuenta de servicio Firebase.",
    )
    parser.add_argument("--url", required=True, help="URL de Realtime Database.")
    parser.add_argument("--intervalo", type=int, default=15)
    parser.add_argument("--lote", type=int, default=500)
    parser.add_argument("--una-vez", action="store_true")
    return parser.parse_args()


def validar_argumentos(args):
    if args.intervalo < 5:
        raise SystemExit("--intervalo debe ser de al menos 5 segundos")
    if args.lote < 1 or args.lote > 5000:
        raise SystemExit("--lote debe estar entre 1 y 5000")
    credencial = Path(args.credencial).expanduser().resolve()
    if not credencial.is_file():
        raise SystemExit(f"No existe la credencial: {credencial}")
    return credencial


def iniciar_firebase(credencial, url):
    import firebase_admin
    from firebase_admin import credentials
    from firebase_admin import db

    aplicacion = firebase_admin.initialize_app(
        credentials.Certificate(str(credencial)),
        {
            "databaseURL": url.rstrip("/"),
            "databaseAuthVariableOverride": {"uid": "tlali-orange-pi"},
            "httpTimeout": 20,
        },
        name="tlali-sincronizador",
    )
    return db.reference("/tlali", app=aplicacion)


def clave_minuto(fecha):
    # Las fechas locales están en ISO UTC: 2026-07-25T02:43:53.427Z.
    return fecha[:10], fecha[11:16].replace(":", "-")


def agrupar_por_minuto(filas):
    grupos = defaultdict(list)
    for fila in filas:
        dia, minuto = clave_minuto(fila["recibido_utc"])
        grupos[(fila["nodo"], dia, minuto)].append(fila)
    return grupos


def preparar_dato(fila):
    dato = dict(fila["trama"])
    dato["gateway"] = {
        "recibidoUtc": fila["recibido_utc"],
        "sincronizadoUtc": fecha_utc(),
        "sqliteId": fila["id"],
    }
    return dato


def sincronizar_pendientes(almacen, raiz, limite):
    filas = almacen.pendientes(limite)
    if not filas:
        return 0, 0

    enviados = 0
    errores = 0
    for (nodo, dia, minuto), grupo in agrupar_por_minuto(filas).items():
        # Firebase conserva una muestra representativa por nodo y minuto.
        muestra = grupo[-1]
        dato = preparar_dato(muestra)
        ids = [fila["id"] for fila in grupo]
        try:
            raiz.child(f"actual/{nodo}").set(dato)
            raiz.child(f"historial/{nodo}/{dia}/{minuto}").set(dato)
            almacen.marcar_enviadas(ids)
            enviados += len(ids)
        except Exception as error:
            almacen.marcar_error(ids, error)
            errores += len(ids)
            print(
                f"Firebase ERROR {nodo} {dia} {minuto}: {error}",
                flush=True,
            )

    return enviados, errores


def main():
    args = argumentos()
    credencial = validar_argumentos(args)
    raiz = iniciar_firebase(credencial, args.url)
    print(f"Firebase conectado: {args.url.rstrip('/')}/tlali", flush=True)
    print(f"SQLite: {Path(args.db).expanduser().resolve()}", flush=True)

    with AlmacenLocal(args.db) as almacen:
        while True:
            enviados, errores = sincronizar_pendientes(
                almacen,
                raiz,
                args.lote,
            )
            if enviados or errores:
                print(
                    f"Firebase ciclo: enviados={enviados} errores={errores}",
                    flush=True,
                )
            if args.una_vez:
                return 0 if errores == 0 else 1
            time.sleep(args.intervalo)


if __name__ == "__main__":
    raise SystemExit(main())
