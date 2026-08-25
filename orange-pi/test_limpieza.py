#!/usr/bin/env python3
import tempfile
import unittest
from pathlib import Path

from almacen_local import AlmacenLocal
from limpiar_sqlite import limpiar


def trama(secuencia):
    return {
        "v": 1,
        "node": "tlali-npk-01",
        "type": "sensor",
        "seq": secuencia,
        "uptimeMs": 1000 + secuencia,
        "data": {"airTemperatureC": 24.0},
        "valid": {"air": True},
    }


class LimpiezaSqliteTest(unittest.TestCase):
    def test_borra_solo_enviadas_antiguas(self):
        with tempfile.TemporaryDirectory() as temporal:
            ruta = Path(temporal) / "tlali.db"
            with AlmacenLocal(ruta) as almacen:
                ids = []
                for secuencia in range(1, 5):
                    identificador, _ = almacen.guardar(trama(secuencia))
                    ids.append(identificador)

                # 1: enviada antigua, se debe eliminar.
                # 2: enviada reciente, se conserva.
                # 3: pendiente antigua, se conserva.
                # 4: error antiguo, se conserva.
                almacen.conexion.execute(
                    """
                    UPDATE telemetria
                    SET estado_envio = 'enviado',
                        enviado_utc = '2020-01-01T00:00:00.000Z'
                    WHERE id = ?
                    """,
                    (ids[0],),
                )
                almacen.conexion.execute(
                    """
                    UPDATE telemetria
                    SET estado_envio = 'enviado',
                        enviado_utc = '2999-01-01T00:00:00.000Z'
                    WHERE id = ?
                    """,
                    (ids[1],),
                )
                almacen.conexion.execute(
                    """
                    UPDATE telemetria
                    SET recibido_utc = '2020-01-01T00:00:00.000Z'
                    WHERE id = ?
                    """,
                    (ids[2],),
                )
                almacen.conexion.execute(
                    """
                    UPDATE telemetria
                    SET estado_envio = 'error',
                        recibido_utc = '2020-01-01T00:00:00.000Z',
                        ultimo_error = 'sin internet'
                    WHERE id = ?
                    """,
                    (ids[3],),
                )
                almacen.conexion.commit()

            eliminadas = limpiar(ruta, retener_dias=7, lote=100)
            self.assertEqual(eliminadas, 1)

            with AlmacenLocal(ruta) as almacen:
                filas = almacen.conexion.execute(
                    "SELECT id, estado_envio FROM telemetria ORDER BY id"
                ).fetchall()
                self.assertEqual(
                    filas,
                    [
                        (ids[1], "enviado"),
                        (ids[2], "pendiente"),
                        (ids[3], "error"),
                    ],
                )
                self.assertEqual(almacen.resumen()[1], 2)

    def test_simulacion_no_borra(self):
        with tempfile.TemporaryDirectory() as temporal:
            ruta = Path(temporal) / "tlali.db"
            with AlmacenLocal(ruta) as almacen:
                identificador, _ = almacen.guardar(trama(1))
                almacen.conexion.execute(
                    """
                    UPDATE telemetria
                    SET estado_envio = 'enviado',
                        enviado_utc = '2020-01-01T00:00:00.000Z'
                    WHERE id = ?
                    """,
                    (identificador,),
                )
                almacen.conexion.commit()

            candidatas = limpiar(ruta, retener_dias=7, lote=100, simular=True)
            self.assertEqual(candidatas, 1)
            with AlmacenLocal(ruta) as almacen:
                self.assertEqual(almacen.resumen()[0], 1)


if __name__ == "__main__":
    unittest.main()
