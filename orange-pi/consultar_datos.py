#!/usr/bin/env python3
import argparse
import json

from almacen_local import AlmacenLocal


def argumentos():
    parser = argparse.ArgumentParser(
        description="Consulta las tramas guardadas localmente por TLALI."
    )
    parser.add_argument("--db", default="datos/tlali.db")
    parser.add_argument("--ultimas", type=int, default=5)
    return parser.parse_args()


def main():
    args = argumentos()
    if args.ultimas < 0 or args.ultimas > 100:
        raise SystemExit("--ultimas debe estar entre 0 y 100")

    with AlmacenLocal(args.db) as almacen:
        total, pendientes, por_nodo = almacen.resumen()
        print(f"Base local: {almacen.ruta}")
        print(f"Total: {total}")
        print(f"Pendientes de internet: {pendientes}")
        print("Por nodo:")
        if not por_nodo:
            print("  Sin datos")
        for nodo, cantidad, ultima_fecha in por_nodo:
            print(f"  {nodo}: {cantidad} | ultimo {ultima_fecha}")

        if args.ultimas:
            print(f"Ultimas {args.ultimas} tramas:")
        for fila in almacen.ultimas(args.ultimas):
            identificador, fecha, nodo, tipo, secuencia, estado, trama_json = fila
            trama = json.loads(trama_json)
            print(
                f"  id={identificador} {fecha} {nodo} "
                f"{tipo} seq={secuencia} estado={estado}"
            )
            print(f"    {json.dumps(trama['data'], ensure_ascii=False)}")


if __name__ == "__main__":
    main()
