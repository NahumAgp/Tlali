#!/usr/bin/env python3
import tempfile
import unittest
from pathlib import Path

from almacen_local import AlmacenLocal
from sincronizar_firebase import sincronizar_pendientes


class ReferenciaFalsa:
    def __init__(self, registros, ruta=""):
        self.registros = registros
        self.ruta = ruta

    def child(self, ruta):
        completa = f"{self.ruta}/{ruta}".strip("/")
        return ReferenciaFalsa(self.registros, completa)

    def set(self, valor):
        self.registros[self.ruta] = valor


class SincronizacionFirebaseTest(unittest.TestCase):
    def test_agrupa_por_minuto_y_marca_todas_las_tramas(self):
        with tempfile.TemporaryDirectory() as temporal:
            ruta = Path(temporal) / "tlali.db"
            with AlmacenLocal(ruta) as almacen:
                for secuencia in (1, 2):
                    almacen.guardar(
                        {
                            "v": 1,
                            "node": "tlali-npk-01",
                            "type": "sensor",
                            "seq": secuencia,
                            "uptimeMs": 1000 + secuencia,
                            "data": {"airTemperatureC": 24.0 + secuencia},
                            "valid": {"air": True},
                        }
                    )
                almacen.guardar(
                    {
                        "v": 1,
                        "node": "tlali-actuadores-01",
                        "type": "actuator",
                        "seq": 1,
                        "uptimeMs": 2001,
                        "data": {"relay1On": False},
                        "valid": {"tank1": True},
                    }
                )

                registros = {}
                enviados, errores = sincronizar_pendientes(
                    almacen,
                    ReferenciaFalsa(registros),
                    100,
                )

                self.assertEqual(enviados, 3)
                self.assertEqual(errores, 0)
                self.assertEqual(almacen.resumen()[1], 0)
                self.assertIn("actual/tlali-npk-01", registros)
                self.assertIn("actual/tlali-actuadores-01", registros)
                self.assertTrue(
                    any(
                        ruta.startswith("historial/tlali-npk-01/")
                        for ruta in registros
                    )
                )


if __name__ == "__main__":
    unittest.main()
