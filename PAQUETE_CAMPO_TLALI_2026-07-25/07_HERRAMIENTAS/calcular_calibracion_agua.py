#!/usr/bin/env python3
import argparse
import math


def argumentos():
    parser = argparse.ArgumentParser(
        description="Calcula una calibración lineal de dos puntos para pH o TDS."
    )
    parser.add_argument("--tipo", choices=("ph", "tds"), required=True)
    parser.add_argument("--v1", type=float, required=True)
    parser.add_argument("--ref1", type=float, required=True)
    parser.add_argument("--v2", type=float, required=True)
    parser.add_argument("--ref2", type=float, required=True)
    parser.add_argument(
        "--verificar-v",
        type=float,
        help="Voltaje de un tercer patrón para calcular el resultado esperado.",
    )
    return parser.parse_args()


def main():
    args = argumentos()
    valores = (args.v1, args.ref1, args.v2, args.ref2)
    if not all(math.isfinite(valor) for valor in valores):
        raise SystemExit("Todos los valores deben ser números finitos.")
    if math.isclose(args.v1, args.v2, abs_tol=1e-9):
        raise SystemExit("V1 y V2 no pueden ser iguales.")

    pendiente = (args.ref2 - args.ref1) / (args.v2 - args.v1)
    intercepto = args.ref1 - pendiente * args.v1
    unidad = "pH" if args.tipo == "ph" else "ppm"

    print(f"Tipo: {args.tipo.upper()}")
    print(f"Pendiente: {pendiente:.9f}")
    print(f"Intercepto: {intercepto:.9f}")
    print(
        f"Ecuación: {unidad} = ({pendiente:.9f} × voltaje_V) "
        f"+ ({intercepto:.9f})"
    )
    print(
        f"Comprobación punto 1: "
        f"{pendiente * args.v1 + intercepto:.4f} {unidad}"
    )
    print(
        f"Comprobación punto 2: "
        f"{pendiente * args.v2 + intercepto:.4f} {unidad}"
    )
    if args.verificar_v is not None:
        if not math.isfinite(args.verificar_v):
            raise SystemExit("--verificar-v debe ser finito.")
        calculado = pendiente * args.verificar_v + intercepto
        print(
            f"Resultado para {args.verificar_v:.6f} V: "
            f"{calculado:.4f} {unidad}"
        )
    print()
    print(
        "Importante: verifica la ecuación con un patrón independiente antes "
        "de modificar el firmware."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
