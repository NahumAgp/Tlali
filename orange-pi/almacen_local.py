#!/usr/bin/env python3
import json
import sqlite3
from datetime import datetime, timezone
from pathlib import Path


ESQUEMA = """
CREATE TABLE IF NOT EXISTS telemetria (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    recibido_utc TEXT NOT NULL,
    nodo TEXT NOT NULL,
    tipo TEXT NOT NULL,
    secuencia INTEGER NOT NULL,
    uptime_ms INTEGER NOT NULL,
    trama_json TEXT NOT NULL,
    estado_envio TEXT NOT NULL DEFAULT 'pendiente'
        CHECK (estado_envio IN ('pendiente', 'enviando', 'enviado', 'error')),
    intentos_envio INTEGER NOT NULL DEFAULT 0,
    enviado_utc TEXT,
    ultimo_error TEXT,
    UNIQUE (nodo, secuencia, uptime_ms)
);

CREATE INDEX IF NOT EXISTS idx_telemetria_estado_id
ON telemetria (estado_envio, id);

CREATE INDEX IF NOT EXISTS idx_telemetria_nodo_fecha
ON telemetria (nodo, recibido_utc);
"""


def fecha_utc():
    return (
        datetime.now(timezone.utc)
        .isoformat(timespec="milliseconds")
        .replace("+00:00", "Z")
    )


def validar_trama(trama):
    if not isinstance(trama, dict):
        raise ValueError("La trama debe ser un objeto JSON")

    requeridos = {
        "node": str,
        "type": str,
        "seq": int,
        "uptimeMs": int,
        "data": dict,
        "valid": dict,
    }
    for campo, tipo in requeridos.items():
        if campo not in trama:
            raise ValueError(f"Falta el campo obligatorio {campo}")
        if not isinstance(trama[campo], tipo):
            raise ValueError(f"El campo {campo} tiene un tipo incorrecto")


class AlmacenLocal:
    def __init__(self, ruta):
        self.ruta = Path(ruta).expanduser().resolve()
        self.ruta.parent.mkdir(parents=True, exist_ok=True)
        self.conexion = sqlite3.connect(self.ruta, timeout=10)
        self.conexion.execute("PRAGMA journal_mode=WAL")
        self.conexion.execute("PRAGMA synchronous=NORMAL")
        self.conexion.execute("PRAGMA foreign_keys=ON")
        self.conexion.executescript(ESQUEMA)
        self.conexion.commit()

    def guardar(self, trama):
        validar_trama(trama)
        trama_json = json.dumps(
            trama,
            ensure_ascii=False,
            separators=(",", ":"),
            sort_keys=True,
        )
        cursor = self.conexion.execute(
            """
            INSERT OR IGNORE INTO telemetria (
                recibido_utc,
                nodo,
                tipo,
                secuencia,
                uptime_ms,
                trama_json
            ) VALUES (?, ?, ?, ?, ?, ?)
            """,
            (
                fecha_utc(),
                trama["node"],
                trama["type"],
                trama["seq"],
                trama["uptimeMs"],
                trama_json,
            ),
        )
        self.conexion.commit()
        insertada = cursor.rowcount == 1

        if insertada:
            return cursor.lastrowid, True

        fila = self.conexion.execute(
            """
            SELECT id
            FROM telemetria
            WHERE nodo = ? AND secuencia = ? AND uptime_ms = ?
            """,
            (trama["node"], trama["seq"], trama["uptimeMs"]),
        ).fetchone()
        return (fila[0] if fila else None), False

    def resumen(self):
        total = self.conexion.execute(
            "SELECT COUNT(*) FROM telemetria"
        ).fetchone()[0]
        pendientes = self.conexion.execute(
            """
            SELECT COUNT(*)
            FROM telemetria
            WHERE estado_envio IN ('pendiente', 'error')
            """
        ).fetchone()[0]
        por_nodo = self.conexion.execute(
            """
            SELECT nodo, COUNT(*), MAX(recibido_utc)
            FROM telemetria
            GROUP BY nodo
            ORDER BY nodo
            """
        ).fetchall()
        return total, pendientes, por_nodo

    def ultimas(self, limite=10):
        return self.conexion.execute(
            """
            SELECT
                id,
                recibido_utc,
                nodo,
                tipo,
                secuencia,
                estado_envio,
                trama_json
            FROM telemetria
            ORDER BY id DESC
            LIMIT ?
            """,
            (limite,),
        ).fetchall()

    def pendientes(self, limite=500):
        filas = self.conexion.execute(
            """
            SELECT
                id,
                recibido_utc,
                nodo,
                tipo,
                secuencia,
                uptime_ms,
                trama_json,
                intentos_envio
            FROM telemetria
            WHERE estado_envio IN ('pendiente', 'error')
            ORDER BY id
            LIMIT ?
            """,
            (limite,),
        ).fetchall()
        return [
            {
                "id": fila[0],
                "recibido_utc": fila[1],
                "nodo": fila[2],
                "tipo": fila[3],
                "secuencia": fila[4],
                "uptime_ms": fila[5],
                "trama": json.loads(fila[6]),
                "intentos_envio": fila[7],
            }
            for fila in filas
        ]

    def marcar_enviadas(self, identificadores):
        if not identificadores:
            return
        marcadores = ",".join("?" for _ in identificadores)
        self.conexion.execute(
            f"""
            UPDATE telemetria
            SET
                estado_envio = 'enviado',
                intentos_envio = intentos_envio + 1,
                enviado_utc = ?,
                ultimo_error = NULL
            WHERE id IN ({marcadores})
            """,
            (fecha_utc(), *identificadores),
        )
        self.conexion.commit()

    def marcar_error(self, identificadores, error):
        if not identificadores:
            return
        marcadores = ",".join("?" for _ in identificadores)
        self.conexion.execute(
            f"""
            UPDATE telemetria
            SET
                estado_envio = 'error',
                intentos_envio = intentos_envio + 1,
                ultimo_error = ?
            WHERE id IN ({marcadores})
            """,
            (str(error)[:500], *identificadores),
        )
        self.conexion.commit()

    def contar_enviadas_anteriores(self, fecha_limite):
        return self.conexion.execute(
            """
            SELECT COUNT(*)
            FROM telemetria
            WHERE estado_envio = 'enviado'
              AND enviado_utc IS NOT NULL
              AND enviado_utc < ?
            """,
            (fecha_limite,),
        ).fetchone()[0]

    def limpiar_enviadas_anteriores(self, fecha_limite, limite=5000):
        """
        Elimina un lote de tramas ya confirmadas en el servidor.

        Las filas pendientes, enviando o con error quedan fuera de la consulta,
        aunque su fecha sea anterior al límite.
        """
        if limite < 1:
            raise ValueError("El límite debe ser mayor que cero")

        cursor = self.conexion.execute(
            """
            DELETE FROM telemetria
            WHERE id IN (
                SELECT id
                FROM telemetria
                WHERE estado_envio = 'enviado'
                  AND enviado_utc IS NOT NULL
                  AND enviado_utc < ?
                ORDER BY id
                LIMIT ?
            )
            """,
            (fecha_limite, limite),
        )
        self.conexion.commit()
        return cursor.rowcount

    def truncar_wal(self):
        """
        Fuerza un checkpoint e intenta devolver a cero el archivo -wal.

        SQLite puede responder ocupado si otro proceso está escribiendo. En ese
        caso no se borran datos y el próximo mantenimiento vuelve a intentarlo.
        """
        return self.conexion.execute("PRAGMA wal_checkpoint(TRUNCATE)").fetchone()

    def estadisticas_paginas(self):
        tamanio_pagina = self.conexion.execute("PRAGMA page_size").fetchone()[0]
        paginas = self.conexion.execute("PRAGMA page_count").fetchone()[0]
        paginas_libres = self.conexion.execute("PRAGMA freelist_count").fetchone()[0]
        return {
            "page_size": tamanio_pagina,
            "page_count": paginas,
            "free_pages": paginas_libres,
            "database_bytes": tamanio_pagina * paginas,
            "reusable_bytes": tamanio_pagina * paginas_libres,
        }

    def cerrar(self):
        self.conexion.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.cerrar()
