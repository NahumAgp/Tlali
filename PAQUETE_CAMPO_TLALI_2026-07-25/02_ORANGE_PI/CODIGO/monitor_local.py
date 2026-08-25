#!/usr/bin/env python3
"""
Monitor HDMI local para TLALI.

Lee SQLite en modo de solo lectura. No necesita Firebase, navegador ni Internet.
"""
import argparse
import json
import math
import sqlite3
from datetime import datetime, timedelta, timezone
from pathlib import Path
import tkinter as tk
from tkinter import ttk


COLORES = {
    "fondo": "#F3F0E7",
    "papel": "#FFFEFA",
    "tinta": "#18352F",
    "verde": "#1D6A55",
    "verde_2": "#2F8A68",
    "menta": "#DFF2E7",
    "azul": "#256B7A",
    "linea": "#DCE5DF",
    "muted": "#6C7F78",
    "ok": "#249B67",
    "advertencia": "#D69B2D",
    "error": "#C9524F",
    "oscuro": "#153D34",
}

NODO_INVERNADERO = "tlali-npk-01"
NODO_ACTUADORES = "tlali-actuadores-01"
INTERVALO_MS = 3000
LIMITE_EN_LINEA_SEGUNDOS = 120

METRICAS_INVERNADERO = [
    ("soilMoisturePct", "Humedad del suelo", "%", "soil", 1),
    ("soilTemperatureC", "Temperatura del suelo", "°C", "soil", 1),
    ("conductivityUsCm", "Conductividad", "µS/cm", "soil", 0),
    ("ph", "pH del sustrato", "pH", "soil", 2),
    ("nitrogenMgKg", "Nitrógeno", "mg/kg", "soil", 0),
    ("phosphorusMgKg", "Fósforo", "mg/kg", "soil", 0),
    ("potassiumMgKg", "Potasio", "mg/kg", "soil", 0),
    ("airHumidityPct", "Humedad ambiental", "%", "air", 1),
    ("airTemperatureC", "Temperatura ambiental", "°C", "air", 1),
    ("lightLux", "Luminosidad", "lux", "light", 1),
]

METRICAS_ACTUADORES = [
    ("tank1DistanceCm", "Distancia tanque 1", "cm", "tank1", 1),
    ("tank2DistanceCm", "Distancia tanque 2", "cm", "tank2", 1),
    (
        "solutionTemperatureCelsius",
        "Temperatura solución",
        "°C",
        "temperature",
        1,
    ),
    ("phVoltageV", "Señal de pH", "V", "phSignal", 3),
    ("tdsVoltageV", "Señal de TDS", "V", "tdsSignal", 3),
]

NOMBRES_ERROR = {
    "soil": "sensor NPK / sustrato",
    "air": "sensor ambiental",
    "light": "sensor de luz",
    "tank1": "sensor del tanque 1",
    "tank2": "sensor del tanque 2",
    "temperature": "temperatura de solución",
    "phSignal": "señal de pH",
    "tdsSignal": "señal de TDS",
}


def argumentos():
    parser = argparse.ArgumentParser(
        description="Monitor local HDMI de TLALI."
    )
    parser.add_argument("--db", default="datos/tlali.db")
    parser.add_argument(
        "--ventana",
        action="store_true",
        help="Abre en ventana en lugar de pantalla completa.",
    )
    return parser.parse_args()


def parsear_fecha(valor):
    if not valor:
        return None
    try:
        return datetime.fromisoformat(valor.replace("Z", "+00:00"))
    except (TypeError, ValueError):
        return None


def fecha_local(valor):
    fecha = parsear_fecha(valor)
    if not fecha:
        return "Sin fecha"
    return fecha.astimezone().strftime("%d/%m/%Y  %H:%M:%S")


def esta_en_linea(fila):
    if not fila:
        return False
    fecha = parsear_fecha(fila["recibido_utc"])
    if not fecha:
        return False
    return (
        datetime.now(timezone.utc) - fecha.astimezone(timezone.utc)
    ).total_seconds() <= LIMITE_EN_LINEA_SEGUNDOS


def formato_valor(valor, unidad, decimales):
    if valor is None:
        return "Sin lectura"
    try:
        numero = float(valor)
    except (TypeError, ValueError):
        return "Sin lectura"
    if not math.isfinite(numero):
        return "Sin lectura"
    if decimales == 0:
        texto = f"{numero:,.0f}"
    else:
        texto = f"{numero:,.{decimales}f}"
    return f"{texto} {unidad}"


def reducir_serie(valores, etiquetas, maximo=600):
    """Reduce puntos para dibujar sin perder el principio ni el final."""
    if len(valores) <= maximo:
        return valores, etiquetas
    paso = math.ceil(len(valores) / maximo)
    indices = list(range(0, len(valores), paso))
    if indices[-1] != len(valores) - 1:
        indices.append(len(valores) - 1)
    return (
        [valores[indice] for indice in indices],
        [etiquetas[indice] for indice in indices],
    )


class LectorSqlite:
    def __init__(self, ruta):
        self.ruta = Path(ruta).expanduser().resolve()

    def _conectar(self):
        if not self.ruta.is_file():
            raise FileNotFoundError(f"No existe la base: {self.ruta}")
        conexion = sqlite3.connect(
            f"{self.ruta.as_uri()}?mode=ro",
            uri=True,
            timeout=2,
        )
        conexion.row_factory = sqlite3.Row
        return conexion

    def actuales(self):
        consulta = """
            SELECT
                t.recibido_utc,
                t.nodo,
                t.secuencia,
                t.estado_envio,
                t.trama_json
            FROM telemetria AS t
            INNER JOIN (
                SELECT nodo, MAX(id) AS ultimo_id
                FROM telemetria
                GROUP BY nodo
            ) AS ultimas ON ultimas.ultimo_id = t.id
        """
        with self._conectar() as conexion:
            filas = conexion.execute(consulta).fetchall()
        resultado = {}
        for fila in filas:
            try:
                trama = json.loads(fila["trama_json"])
            except (TypeError, json.JSONDecodeError):
                continue
            resultado[fila["nodo"]] = {
                "recibido_utc": fila["recibido_utc"],
                "secuencia": fila["secuencia"],
                "estado_envio": fila["estado_envio"],
                "trama": trama,
            }
        return resultado

    def historial(self, nodo, horas=24, limite=20000):
        desde = (
            (datetime.now(timezone.utc) - timedelta(hours=horas))
            .isoformat(timespec="milliseconds")
            .replace("+00:00", "Z")
        )
        consulta = """
            SELECT recibido_utc, trama_json
            FROM telemetria
            WHERE nodo = ?
              AND recibido_utc >= ?
            ORDER BY id ASC
            LIMIT ?
        """
        with self._conectar() as conexion:
            filas = conexion.execute(
                consulta,
                (nodo, desde, limite),
            ).fetchall()
        resultado = []
        for fila in filas:
            try:
                trama = json.loads(fila["trama_json"])
            except (TypeError, json.JSONDecodeError):
                continue
            resultado.append(
                {
                    "recibido_utc": fila["recibido_utc"],
                    "trama": trama,
                }
            )
        return resultado


class TarjetaMetrica(tk.Frame):
    def __init__(
        self,
        padre,
        titulo,
        valor,
        valido,
        **kwargs,
    ):
        super().__init__(
            padre,
            bg=COLORES["papel"],
            highlightbackground=COLORES["linea"],
            highlightthickness=1,
            padx=18,
            pady=16,
            **kwargs,
        )
        self.etiqueta_titulo = tk.Label(
            self,
            text=titulo,
            bg=COLORES["papel"],
            fg=COLORES["muted"],
            font=("DejaVu Sans", 10, "bold"),
            anchor="w",
        )
        self.etiqueta_titulo.pack(fill="x")
        self.etiqueta_valor = tk.Label(
            self,
            text=valor,
            bg=COLORES["papel"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 20, "bold"),
            anchor="w",
            pady=10,
        )
        self.etiqueta_valor.pack(fill="x")
        color = COLORES["ok"] if valido else COLORES["error"]
        estado = "●  Lectura válida" if valido else "●  Revisar sensor"
        self.etiqueta_estado = tk.Label(
            self,
            text=estado,
            bg=COLORES["papel"],
            fg=color,
            font=("DejaVu Sans", 9, "bold"),
            anchor="w",
        )
        self.etiqueta_estado.pack(fill="x")

    def actualizar(self, valor, valido):
        self.etiqueta_valor.configure(text=valor)
        self.etiqueta_estado.configure(
            text="●  Lectura válida" if valido else "●  Revisar sensor",
            fg=COLORES["ok"] if valido else COLORES["error"],
        )


class MonitorTlali:
    def __init__(self, root, lector, ventana=False):
        self.root = root
        self.lector = lector
        self.ventana = ventana
        self.pagina = "Resumen"
        self.datos = {}
        self.error_lectura = None
        self.ultimo_refresco = None
        self.trabajo_programado = None
        self.vista_construida = False
        self.referencias_resumen = []
        self.tarjetas_metricas = []
        self.referencias_reles = []
        self.detalle_nodo = None
        self.detalle_etiqueta = None
        self.alerta_resumen = None
        self.canvas_historial = None
        self.historial_cabecera = None
        self.historial_tabla = None
        self.ultimo_refresco_historial = None

        root.title("TLALI · Monitor local")
        root.configure(bg=COLORES["fondo"])
        root.minsize(900, 600)
        if ventana:
            root.geometry("1180x760")
        else:
            root.attributes("-fullscreen", True)
        root.bind("<Escape>", lambda _evento: root.destroy())
        root.bind("<F11>", self.alternar_pantalla_completa)
        root.protocol("WM_DELETE_WINDOW", root.destroy)

        self.construir_base()
        self.refrescar()

    def alternar_pantalla_completa(self, _evento=None):
        actual = bool(self.root.attributes("-fullscreen"))
        self.root.attributes("-fullscreen", not actual)

    def construir_base(self):
        self.cabecera = tk.Frame(
            self.root,
            bg=COLORES["papel"],
            height=76,
            padx=28,
            pady=14,
        )
        self.cabecera.pack(fill="x")
        self.cabecera.pack_propagate(False)

        marca = tk.Frame(self.cabecera, bg=COLORES["papel"])
        marca.pack(side="left")
        icono = tk.Label(
            marca,
            text="⌁",
            bg=COLORES["verde"],
            fg="white",
            font=("DejaVu Sans", 24, "bold"),
            width=2,
            pady=3,
        )
        icono.pack(side="left", padx=(0, 12))
        textos = tk.Frame(marca, bg=COLORES["papel"])
        textos.pack(side="left")
        tk.Label(
            textos,
            text="TLALI",
            bg=COLORES["papel"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 20, "bold"),
        ).pack(anchor="w")
        tk.Label(
            textos,
            text="MONITOR LOCAL · ORANGE PI",
            bg=COLORES["papel"],
            fg=COLORES["verde_2"],
            font=("DejaVu Sans", 8, "bold"),
        ).pack(anchor="w")

        self.estado_global = tk.Label(
            self.cabecera,
            text="●  Iniciando",
            bg=COLORES["menta"],
            fg=COLORES["verde"],
            font=("DejaVu Sans", 10, "bold"),
            padx=14,
            pady=9,
        )
        self.estado_global.pack(side="right", padx=(18, 0))
        self.reloj = tk.Label(
            self.cabecera,
            bg=COLORES["papel"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 15, "bold"),
        )
        self.reloj.pack(side="right")

        self.navegacion = tk.Frame(
            self.root,
            bg="#F8F7F1",
            padx=24,
            pady=8,
        )
        self.navegacion.pack(fill="x")
        self.botones = {}
        for pagina in ("Resumen", "Invernadero", "Actuadores", "Historial"):
            boton = tk.Button(
                self.navegacion,
                text=pagina,
                command=lambda nombre=pagina: self.mostrar_pagina(nombre),
                relief="flat",
                bd=0,
                padx=18,
                pady=9,
                cursor="hand2",
                font=("DejaVu Sans", 10, "bold"),
            )
            boton.pack(side="left", padx=3)
            self.botones[pagina] = boton

        self.contenido = tk.Frame(
            self.root,
            bg=COLORES["fondo"],
            padx=28,
            pady=22,
        )
        self.contenido.pack(fill="both", expand=True)

        self.pie = tk.Frame(
            self.root,
            bg="#E7ECE8",
            padx=28,
            pady=8,
        )
        self.pie.pack(fill="x")
        self.pie_estado = tk.Label(
            self.pie,
            text="Base local: esperando…",
            bg="#E7ECE8",
            fg=COLORES["muted"],
            font=("DejaVu Sans", 8),
        )
        self.pie_estado.pack(side="left")
        tk.Label(
            self.pie,
            text="Esc: salir  ·  F11: pantalla completa  ·  actualización: 3 s",
            bg="#E7ECE8",
            fg=COLORES["muted"],
            font=("DejaVu Sans", 8),
        ).pack(side="right")
        self.actualizar_navegacion()

    def actualizar_navegacion(self):
        for nombre, boton in self.botones.items():
            activo = nombre == self.pagina
            boton.configure(
                bg=COLORES["verde"] if activo else "#F8F7F1",
                fg="white" if activo else COLORES["muted"],
                activebackground=COLORES["verde_2"],
                activeforeground="white",
            )

    def mostrar_pagina(self, pagina):
        self.pagina = pagina
        self.actualizar_navegacion()
        self.dibujar()

    def limpiar_contenido(self):
        for widget in self.contenido.winfo_children():
            widget.destroy()

    def titulo_pagina(self, kicker, titulo, subtitulo):
        bloque = tk.Frame(self.contenido, bg=COLORES["fondo"])
        bloque.pack(fill="x", pady=(0, 18))
        tk.Label(
            bloque,
            text=kicker,
            bg=COLORES["fondo"],
            fg=COLORES["verde_2"],
            font=("DejaVu Sans", 9, "bold"),
        ).pack(anchor="w")
        tk.Label(
            bloque,
            text=titulo,
            bg=COLORES["fondo"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 28, "bold"),
        ).pack(anchor="w")
        tk.Label(
            bloque,
            text=subtitulo,
            bg=COLORES["fondo"],
            fg=COLORES["muted"],
            font=("DejaVu Sans", 10),
        ).pack(anchor="w")

    def trama(self, nodo):
        fila = self.datos.get(nodo)
        return fila["trama"] if fila else {}

    def errores(self, nodo):
        trama = self.trama(nodo)
        validez = trama.get("valid", {})
        if not trama:
            return ["sin datos"]
        return [
            NOMBRES_ERROR.get(clave, clave)
            for clave, valor in validez.items()
            if valor is not True and not clave.endswith("Calibrated")
        ]

    def etiqueta_estado_nodo(self, padre, nodo):
        fila = self.datos.get(nodo)
        if not fila:
            texto, fondo, frente = "●  SIN DATOS", "#ECEFEC", COLORES["muted"]
        elif esta_en_linea(fila):
            texto, fondo, frente = "●  EN LÍNEA", COLORES["menta"], COLORES["ok"]
        else:
            texto, fondo, frente = (
                "●  SIN ACTUALIZACIÓN",
                "#FFF0CF",
                COLORES["advertencia"],
            )
        return tk.Label(
            padre,
            text=texto,
            bg=fondo,
            fg=frente,
            font=("DejaVu Sans", 9, "bold"),
            padx=10,
            pady=7,
        )

    def actualizar_etiqueta_estado_nodo(self, etiqueta, nodo):
        fila = self.datos.get(nodo)
        if not fila:
            etiqueta.configure(
                text="●  SIN DATOS",
                bg="#ECEFEC",
                fg=COLORES["muted"],
            )
        elif esta_en_linea(fila):
            etiqueta.configure(
                text="●  EN LÍNEA",
                bg=COLORES["menta"],
                fg=COLORES["ok"],
            )
        else:
            etiqueta.configure(
                text="●  SIN ACTUALIZACIÓN",
                bg="#FFF0CF",
                fg=COLORES["advertencia"],
            )

    def tarjeta_nodo(self, padre, nodo, nombre, metricas, color):
        tarjeta = tk.Frame(
            padre,
            bg=COLORES["papel"],
            highlightbackground=COLORES["linea"],
            highlightthickness=1,
            padx=20,
            pady=18,
        )
        cabecera = tk.Frame(tarjeta, bg=COLORES["papel"])
        cabecera.pack(fill="x")
        tk.Label(
            cabecera,
            text="●",
            fg=color,
            bg=COLORES["papel"],
            font=("DejaVu Sans", 18, "bold"),
        ).pack(side="left")
        estado_nodo = self.etiqueta_estado_nodo(cabecera, nodo)
        estado_nodo.pack(side="right")
        tk.Label(
            tarjeta,
            text=nombre,
            bg=COLORES["papel"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 20, "bold"),
        ).pack(anchor="w", pady=(12, 2))
        tk.Label(
            tarjeta,
            text=nodo.upper(),
            bg=COLORES["papel"],
            fg=COLORES["muted"],
            font=("DejaVu Sans", 8, "bold"),
        ).pack(anchor="w", pady=(0, 14))

        datos = self.trama(nodo).get("data", {})
        etiquetas_valor = []
        for clave, etiqueta, unidad, _valido, decimales in metricas:
            fila = tk.Frame(tarjeta, bg="#F4F7F4", padx=12, pady=9)
            fila.pack(fill="x", pady=3)
            tk.Label(
                fila,
                text=etiqueta,
                bg="#F4F7F4",
                fg=COLORES["muted"],
                font=("DejaVu Sans", 9),
            ).pack(side="left")
            etiqueta_valor = tk.Label(
                fila,
                text=formato_valor(datos.get(clave), unidad, decimales),
                bg="#F4F7F4",
                fg=COLORES["tinta"],
                font=("DejaVu Sans", 11, "bold"),
            )
            etiqueta_valor.pack(side="right")
            etiquetas_valor.append((etiqueta_valor, clave, unidad, decimales))

        errores = self.errores(nodo)
        texto_error = (
            "Sensores OK"
            if not errores
            else "Revisar: " + ", ".join(errores)
        )
        etiqueta_error = tk.Label(
            tarjeta,
            text=texto_error,
            bg=COLORES["papel"],
            fg=COLORES["ok"] if not errores else COLORES["error"],
            font=("DejaVu Sans", 9, "bold"),
            wraplength=430,
            justify="left",
        )
        etiqueta_error.pack(anchor="w", pady=(13, 0))
        tarjeta._tlali_referencias = {
            "nodo": nodo,
            "estado": estado_nodo,
            "valores": etiquetas_valor,
            "error": etiqueta_error,
        }
        return tarjeta

    def actualizar_tarjeta_nodo(self, tarjeta):
        referencias = tarjeta._tlali_referencias
        nodo = referencias["nodo"]
        datos = self.trama(nodo).get("data", {})
        self.actualizar_etiqueta_estado_nodo(referencias["estado"], nodo)
        for etiqueta, clave, unidad, decimales in referencias["valores"]:
            etiqueta.configure(
                text=formato_valor(datos.get(clave), unidad, decimales)
            )
        errores = self.errores(nodo)
        referencias["error"].configure(
            text=(
                "Sensores OK"
                if not errores
                else "Revisar: " + ", ".join(errores)
            ),
            fg=COLORES["ok"] if not errores else COLORES["error"],
        )

    def dibujar_resumen(self):
        self.titulo_pagina(
            "CENTRO DE CONTROL",
            "Estado general",
            "Datos recibidos por LoRa y almacenados en esta Orange Pi.",
        )
        columnas = tk.Frame(self.contenido, bg=COLORES["fondo"])
        columnas.pack(fill="both", expand=True)
        columnas.grid_columnconfigure(0, weight=1, uniform="nodos")
        columnas.grid_columnconfigure(1, weight=1, uniform="nodos")
        columnas.grid_rowconfigure(0, weight=1)

        metricas_verdes = [
            METRICAS_INVERNADERO[0],
            METRICAS_INVERNADERO[8],
            METRICAS_INVERNADERO[9],
        ]
        metricas_agua = [
            METRICAS_ACTUADORES[0],
            METRICAS_ACTUADORES[1],
            METRICAS_ACTUADORES[2],
        ]
        tarjeta_invernadero = self.tarjeta_nodo(
            columnas,
            NODO_INVERNADERO,
            "Invernadero",
            metricas_verdes,
            COLORES["verde_2"],
        )
        tarjeta_invernadero.grid(
            row=0,
            column=0,
            sticky="nsew",
            padx=(0, 9),
        )
        tarjeta_actuadores = self.tarjeta_nodo(
            columnas,
            NODO_ACTUADORES,
            "Agua y actuadores",
            metricas_agua,
            COLORES["azul"],
        )
        tarjeta_actuadores.grid(
            row=0,
            column=1,
            sticky="nsew",
            padx=(9, 0),
        )
        self.referencias_resumen = [
            tarjeta_invernadero,
            tarjeta_actuadores,
        ]

        alertas = self.errores(NODO_INVERNADERO) + self.errores(
            NODO_ACTUADORES
        )
        alertas = [item for item in alertas if item != "sin datos"]
        caja = tk.Frame(
            self.contenido,
            bg="#FFF0EE" if alertas else "#EAF7EF",
            padx=17,
            pady=13,
        )
        caja.pack(fill="x", pady=(16, 0))
        tk.Label(
            caja,
            text="!" if alertas else "✓",
            bg=COLORES["error"] if alertas else COLORES["ok"],
            fg="white",
            font=("DejaVu Sans", 13, "bold"),
            width=2,
        ).pack(side="left", padx=(0, 12))
        etiqueta_alerta = tk.Label(
            caja,
            text=(
                "Revisar sensores: " + ", ".join(alertas)
                if alertas
                else "Todos los sensores reportan correctamente"
            ),
            bg="#FFF0EE" if alertas else "#EAF7EF",
            fg=COLORES["error"] if alertas else COLORES["verde"],
            font=("DejaVu Sans", 10, "bold"),
        )
        etiqueta_alerta.pack(side="left")
        self.alerta_resumen = {
            "caja": caja,
            "icono": caja.winfo_children()[0],
            "etiqueta": etiqueta_alerta,
        }

    def actualizar_resumen(self):
        for tarjeta in self.referencias_resumen:
            self.actualizar_tarjeta_nodo(tarjeta)
        alertas = self.errores(NODO_INVERNADERO) + self.errores(
            NODO_ACTUADORES
        )
        alertas = [item for item in alertas if item != "sin datos"]
        if not self.alerta_resumen:
            return
        fondo = "#FFF0EE" if alertas else "#EAF7EF"
        color = COLORES["error"] if alertas else COLORES["ok"]
        texto = (
            "Revisar sensores: " + ", ".join(alertas)
            if alertas
            else "Todos los sensores reportan correctamente"
        )
        self.alerta_resumen["caja"].configure(bg=fondo)
        self.alerta_resumen["icono"].configure(
            text="!" if alertas else "✓",
            bg=color,
        )
        self.alerta_resumen["etiqueta"].configure(
            text=texto,
            bg=fondo,
            fg=color if alertas else COLORES["verde"],
        )

    def dibujar_metricas(self, nodo, metricas):
        trama = self.trama(nodo)
        datos = trama.get("data", {})
        validez = trama.get("valid", {})
        rejilla = tk.Frame(self.contenido, bg=COLORES["fondo"])
        rejilla.pack(fill="both", expand=True)
        ancho = max(self.root.winfo_width(), 900)
        columnas = 5 if ancho >= 1300 else 4 if ancho >= 1050 else 3
        for columna in range(columnas):
            rejilla.grid_columnconfigure(columna, weight=1, uniform="metricas")
        filas = math.ceil(len(metricas) / columnas)
        for fila in range(filas):
            rejilla.grid_rowconfigure(fila, weight=1)
        for indice, metrica in enumerate(metricas):
            clave, etiqueta, unidad, clave_valida, decimales = metrica
            tarjeta = TarjetaMetrica(
                rejilla,
                etiqueta,
                formato_valor(datos.get(clave), unidad, decimales),
                validez.get(clave_valida) is True,
            )
            tarjeta.grid(
                row=indice // columnas,
                column=indice % columnas,
                sticky="nsew",
                padx=6,
                pady=6,
            )
            self.tarjetas_metricas.append(
                (tarjeta, nodo, metrica)
            )
        return rejilla

    def actualizar_metricas(self):
        for tarjeta, nodo, metrica in self.tarjetas_metricas:
            clave, _etiqueta, unidad, clave_valida, decimales = metrica
            trama = self.trama(nodo)
            datos = trama.get("data", {})
            validez = trama.get("valid", {})
            tarjeta.actualizar(
                formato_valor(datos.get(clave), unidad, decimales),
                validez.get(clave_valida) is True,
            )

    def franja_detalle(self, nodo):
        fila = self.datos.get(nodo)
        trama = self.trama(nodo)
        radio = trama.get("radio", {})
        texto = (
            f"Secuencia: {trama.get('seq', '—')}     "
            f"RSSI: {radio.get('rssiDbm', '—')} dBm     "
            f"SNR: {radio.get('snrDb', '—')} dB     "
            f"Última lectura: {fecha_local(fila['recibido_utc']) if fila else '—'}"
        )
        etiqueta = tk.Label(
            self.contenido,
            text=texto,
            bg="#E6ECE7",
            fg=COLORES["muted"],
            font=("DejaVu Sans", 9, "bold"),
            padx=15,
            pady=11,
            anchor="w",
        )
        etiqueta.pack(fill="x", pady=(15, 0))
        self.detalle_nodo = nodo
        self.detalle_etiqueta = etiqueta

    def actualizar_franja_detalle(self):
        if not self.detalle_nodo or not self.detalle_etiqueta:
            return
        nodo = self.detalle_nodo
        fila = self.datos.get(nodo)
        trama = self.trama(nodo)
        radio = trama.get("radio", {})
        texto = (
            f"Secuencia: {trama.get('seq', '—')}     "
            f"RSSI: {radio.get('rssiDbm', '—')} dBm     "
            f"SNR: {radio.get('snrDb', '—')} dB     "
            f"Última lectura: "
            f"{fecha_local(fila['recibido_utc']) if fila else '—'}"
        )
        self.detalle_etiqueta.configure(text=texto)

    def dibujar_invernadero(self):
        self.titulo_pagina(
            "NODO 01 · TLALI-NPK-01",
            "Invernadero",
            "Sustrato, nutrientes, ambiente y luminosidad.",
        )
        self.dibujar_metricas(
            NODO_INVERNADERO,
            METRICAS_INVERNADERO,
        )
        self.franja_detalle(NODO_INVERNADERO)

    def dibujar_actuadores(self):
        self.titulo_pagina(
            "NODO 02 · TLALI-ACTUADORES-01",
            "Agua y actuadores",
            "Tanques, solución nutritiva y estado de salidas.",
        )
        self.dibujar_metricas(
            NODO_ACTUADORES,
            METRICAS_ACTUADORES,
        )
        trama = self.trama(NODO_ACTUADORES)
        datos = trama.get("data", {})
        reles = tk.Frame(self.contenido, bg=COLORES["fondo"])
        reles.pack(fill="x", pady=(15, 0))
        for indice, clave in enumerate(("relay1On", "relay2On"), start=1):
            activo = datos.get(clave) is True
            tarjeta = tk.Frame(
                reles,
                bg=COLORES["papel"],
                highlightbackground=COLORES["linea"],
                highlightthickness=1,
                padx=18,
                pady=14,
            )
            tarjeta.pack(
                side="left",
                fill="x",
                expand=True,
                padx=(0, 7) if indice == 1 else (7, 0),
            )
            etiqueta_rele = tk.Label(
                tarjeta,
                text="●",
                bg=COLORES["papel"],
                fg=COLORES["ok"] if activo else COLORES["muted"],
                font=("DejaVu Sans", 20, "bold"),
            ).pack(side="left", padx=(0, 12))
            tk.Label(
                tarjeta,
                text=f"RELÉ {indice}\n{'ENCENDIDO' if activo else 'APAGADO'}",
                bg=COLORES["papel"],
                fg=COLORES["tinta"],
                font=("DejaVu Sans", 11, "bold"),
                justify="left",
            )
            etiqueta_rele.pack(side="left")
            self.referencias_reles.append(
                (tarjeta.winfo_children()[0], etiqueta_rele, clave, indice)
            )
        self.franja_detalle(NODO_ACTUADORES)

    def actualizar_reles(self):
        datos = self.trama(NODO_ACTUADORES).get("data", {})
        for luz, etiqueta, clave, indice in self.referencias_reles:
            activo = datos.get(clave) is True
            luz.configure(
                fg=COLORES["ok"] if activo else COLORES["muted"]
            )
            etiqueta.configure(
                text=f"RELÉ {indice}\n"
                f"{'ENCENDIDO' if activo else 'APAGADO'}"
            )

    def dibujar_historial(self):
        self.titulo_pagina(
            "BASE LOCAL · SQLITE",
            "Historial de las últimas 24 horas",
            "Los datos permanecen siete días en la Orange Pi.",
        )
        controles = tk.Frame(self.contenido, bg=COLORES["fondo"])
        controles.pack(fill="x", pady=(0, 13))

        if not hasattr(self, "historial_nodo"):
            self.historial_nodo = tk.StringVar(value="Invernadero")
            self.historial_metrica = tk.StringVar(
                value=METRICAS_INVERNADERO[0][1]
            )

        estilo = ttk.Style()
        estilo.theme_use("clam")
        estilo.configure(
            "Tlali.TCombobox",
            fieldbackground=COLORES["papel"],
            background=COLORES["papel"],
            foreground=COLORES["tinta"],
            padding=8,
        )

        tk.Label(
            controles,
            text="Nodo",
            bg=COLORES["fondo"],
            fg=COLORES["muted"],
            font=("DejaVu Sans", 9, "bold"),
        ).pack(side="left", padx=(0, 6))
        selector_nodo = ttk.Combobox(
            controles,
            textvariable=self.historial_nodo,
            values=("Invernadero", "Actuadores"),
            state="readonly",
            width=18,
            style="Tlali.TCombobox",
        )
        selector_nodo.pack(side="left", padx=(0, 18))
        selector_nodo.bind("<<ComboboxSelected>>", self.cambiar_nodo_historial)

        metricas = (
            METRICAS_INVERNADERO
            if self.historial_nodo.get() == "Invernadero"
            else METRICAS_ACTUADORES
        )
        nombres = [metrica[1] for metrica in metricas]
        if self.historial_metrica.get() not in nombres:
            self.historial_metrica.set(nombres[0])
        tk.Label(
            controles,
            text="Variable",
            bg=COLORES["fondo"],
            fg=COLORES["muted"],
            font=("DejaVu Sans", 9, "bold"),
        ).pack(side="left", padx=(0, 6))
        selector_metrica = ttk.Combobox(
            controles,
            textvariable=self.historial_metrica,
            values=nombres,
            state="readonly",
            width=28,
            style="Tlali.TCombobox",
        )
        selector_metrica.pack(side="left")
        selector_metrica.bind(
            "<<ComboboxSelected>>",
            lambda _evento: self.dibujar(),
        )

        nodo = (
            NODO_INVERNADERO
            if self.historial_nodo.get() == "Invernadero"
            else NODO_ACTUADORES
        )
        metrica = next(
            item
            for item in metricas
            if item[1] == self.historial_metrica.get()
        )
        try:
            filas = self.lector.historial(nodo)
        except (OSError, sqlite3.Error):
            filas = []
        valores = []
        etiquetas = []
        for fila in filas:
            valor = fila["trama"].get("data", {}).get(metrica[0])
            if valor is None:
                continue
            try:
                valor = float(valor)
            except (TypeError, ValueError):
                continue
            if not math.isfinite(valor):
                continue
            valores.append(valor)
            fecha = parsear_fecha(fila["recibido_utc"])
            etiquetas.append(
                fecha.astimezone().strftime("%H:%M:%S")
                if fecha
                else "--:--"
            )

        grafica = tk.Frame(
            self.contenido,
            bg=COLORES["papel"],
            highlightbackground=COLORES["linea"],
            highlightthickness=1,
            padx=18,
            pady=16,
        )
        grafica.pack(fill="both", expand=True)
        cabecera = tk.Frame(grafica, bg=COLORES["papel"])
        cabecera.pack(fill="x")
        etiqueta_actual = tk.Label(
            cabecera,
            text=metrica[1],
            bg=COLORES["papel"],
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 17, "bold"),
        ).pack(side="left")
        ultimo = (
            formato_valor(valores[-1], metrica[2], metrica[4])
            if valores
            else "Sin datos"
        )
        tk.Label(
            cabecera,
            text=f"Actual: {ultimo}     ·     {len(valores)} puntos",
            bg=COLORES["papel"],
            fg=COLORES["verde"],
            font=("DejaVu Sans", 10, "bold"),
        )
        etiqueta_actual.pack(side="right")

        canvas = tk.Canvas(
            grafica,
            bg=COLORES["papel"],
            highlightthickness=0,
        )
        canvas.pack(fill="both", expand=True, pady=(10, 0))
        valores_grafica, etiquetas_grafica = reducir_serie(
            valores,
            etiquetas,
        )
        canvas.bind(
            "<Configure>",
            lambda evento: self.trazar_grafica(
                canvas,
                valores_grafica,
                etiquetas_grafica,
                metrica[2],
                evento.width,
                evento.height,
            ),
        )
        self.canvas_historial = canvas
        self.historial_cabecera = etiqueta_actual
        self.historial_tabla = tk.Frame(
            grafica,
            bg="#F4F7F4",
            padx=12,
            pady=8,
        )
        self.historial_tabla.pack(fill="x", pady=(8, 0))
        self.actualizar_tabla_historial(
            filas,
            metrica,
        )
        self.ultimo_refresco_historial = datetime.now()

    def actualizar_tabla_historial(self, filas, metrica):
        if not self.historial_tabla:
            return
        for widget in self.historial_tabla.winfo_children():
            widget.destroy()

        valores = []
        for fila in filas:
            valor = fila["trama"].get("data", {}).get(metrica[0])
            try:
                numero = float(valor)
            except (TypeError, ValueError):
                continue
            if math.isfinite(numero):
                valores.append(numero)

        resumen = tk.Frame(
            self.historial_tabla,
            bg="#F4F7F4",
        )
        resumen.pack(fill="x", pady=(0, 6))
        if valores:
            estadisticas = (
                f"Registros válidos: {len(valores)}     "
                f"Mínimo: {formato_valor(min(valores), metrica[2], metrica[4])}     "
                f"Máximo: {formato_valor(max(valores), metrica[2], metrica[4])}     "
                f"Promedio: {formato_valor(sum(valores) / len(valores), metrica[2], metrica[4])}"
            )
        else:
            estadisticas = "No existen lecturas válidas para esta variable."
        tk.Label(
            resumen,
            text=estadisticas,
            bg="#F4F7F4",
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 8, "bold"),
            anchor="w",
        ).pack(fill="x")

        recientes = []
        for fila in reversed(filas):
            valor = fila["trama"].get("data", {}).get(metrica[0])
            if valor is None:
                continue
            recientes.append((fila["recibido_utc"], valor))
            if len(recientes) == 5:
                break

        if not recientes:
            tk.Label(
                self.historial_tabla,
                text="Sin registros recientes.",
                bg="#F4F7F4",
                fg=COLORES["muted"],
                font=("DejaVu Sans", 8),
            ).pack(anchor="w")
            return

        fila_valores = tk.Frame(
            self.historial_tabla,
            bg="#F4F7F4",
        )
        fila_valores.pack(fill="x")
        for fecha, valor in recientes:
            fecha_dt = parsear_fecha(fecha)
            hora = (
                fecha_dt.astimezone().strftime("%H:%M:%S")
                if fecha_dt
                else "--:--:--"
            )
            bloque = tk.Frame(
                fila_valores,
                bg=COLORES["papel"],
                padx=9,
                pady=6,
            )
            bloque.pack(side="left", fill="x", expand=True, padx=3)
            tk.Label(
                bloque,
                text=hora,
                bg=COLORES["papel"],
                fg=COLORES["muted"],
                font=("DejaVu Sans", 7),
            ).pack()
            tk.Label(
                bloque,
                text=formato_valor(valor, metrica[2], metrica[4]),
                bg=COLORES["papel"],
                fg=COLORES["tinta"],
                font=("DejaVu Sans", 8, "bold"),
            ).pack()

    def actualizar_historial(self):
        if not self.canvas_historial or not self.historial_cabecera:
            return
        if (
            self.ultimo_refresco_historial
            and (
                datetime.now() - self.ultimo_refresco_historial
            ).total_seconds() < 30
        ):
            return
        nodo = (
            NODO_INVERNADERO
            if self.historial_nodo.get() == "Invernadero"
            else NODO_ACTUADORES
        )
        metricas = (
            METRICAS_INVERNADERO
            if nodo == NODO_INVERNADERO
            else METRICAS_ACTUADORES
        )
        metrica = next(
            item
            for item in metricas
            if item[1] == self.historial_metrica.get()
        )
        try:
            filas = self.lector.historial(nodo)
        except (OSError, sqlite3.Error):
            return
        valores = []
        etiquetas = []
        for fila in filas:
            valor = fila["trama"].get("data", {}).get(metrica[0])
            if valor is None:
                continue
            try:
                valor = float(valor)
            except (TypeError, ValueError):
                continue
            if not math.isfinite(valor):
                continue
            valores.append(valor)
            fecha = parsear_fecha(fila["recibido_utc"])
            etiquetas.append(
                fecha.astimezone().strftime("%H:%M:%S")
                if fecha
                else "--:--"
            )
        ultimo = (
            formato_valor(valores[-1], metrica[2], metrica[4])
            if valores
            else "Sin datos"
        )
        self.historial_cabecera.configure(
            text=f"Actual: {ultimo}     ·     {len(valores)} puntos"
        )
        self.actualizar_tabla_historial(filas, metrica)
        valores_grafica, etiquetas_grafica = reducir_serie(
            valores,
            etiquetas,
        )
        self.trazar_grafica(
            self.canvas_historial,
            valores_grafica,
            etiquetas_grafica,
            metrica[2],
            self.canvas_historial.winfo_width(),
            self.canvas_historial.winfo_height(),
        )
        self.ultimo_refresco_historial = datetime.now()

    def cambiar_nodo_historial(self, _evento=None):
        metricas = (
            METRICAS_INVERNADERO
            if self.historial_nodo.get() == "Invernadero"
            else METRICAS_ACTUADORES
        )
        self.historial_metrica.set(metricas[0][1])
        self.dibujar()

    def trazar_grafica(
        self,
        canvas,
        valores,
        etiquetas,
        unidad,
        ancho,
        alto,
    ):
        canvas.delete("all")
        if ancho < 100 or alto < 100:
            return
        margen_izq, margen_der, margen_sup, margen_inf = 70, 25, 20, 40
        x0, y0 = margen_izq, alto - margen_inf
        x1, y1 = ancho - margen_der, margen_sup
        canvas.create_line(x0, y0, x1, y0, fill=COLORES["linea"])
        canvas.create_line(x0, y0, x0, y1, fill=COLORES["linea"])
        if not valores:
            canvas.create_text(
                ancho / 2,
                alto / 2,
                text="Todavía no hay datos para esta variable",
                fill=COLORES["muted"],
                font=("DejaVu Sans", 12),
            )
            return

        minimo = min(valores)
        maximo = max(valores)
        if math.isclose(minimo, maximo):
            minimo -= 1
            maximo += 1
        rango = maximo - minimo
        minimo -= rango * 0.08
        maximo += rango * 0.08

        for indice in range(5):
            fraccion = indice / 4
            y = y0 - fraccion * (y0 - y1)
            valor = minimo + fraccion * (maximo - minimo)
            canvas.create_line(
                x0,
                y,
                x1,
                y,
                fill="#EDF1EE",
            )
            canvas.create_text(
                x0 - 9,
                y,
                text=f"{valor:.1f}",
                anchor="e",
                fill=COLORES["muted"],
                font=("DejaVu Sans", 8),
            )

        puntos = []
        divisor = max(len(valores) - 1, 1)
        for indice, valor in enumerate(valores):
            x = x0 + indice / divisor * (x1 - x0)
            y = y0 - (valor - minimo) / (maximo - minimo) * (y0 - y1)
            puntos.extend((x, y))
        if len(puntos) >= 4:
            relleno = [x0, y0] + puntos + [x1, y0]
            canvas.create_polygon(
                relleno,
                fill="#E4F3E9",
                outline="",
            )
            canvas.create_line(
                puntos,
                fill=COLORES["verde_2"],
                width=3,
                smooth=True,
            )
        else:
            canvas.create_oval(
                puntos[0] - 4,
                puntos[1] - 4,
                puntos[0] + 4,
                puntos[1] + 4,
                fill=COLORES["verde_2"],
                outline="",
            )

        if etiquetas:
            for indice in (0, len(etiquetas) // 2, len(etiquetas) - 1):
                x = x0 + indice / divisor * (x1 - x0)
                canvas.create_text(
                    x,
                    y0 + 18,
                    text=etiquetas[indice],
                    fill=COLORES["muted"],
                    font=("DejaVu Sans", 8),
                )
        canvas.create_text(
            10,
            y1,
            text=unidad,
            anchor="nw",
            fill=COLORES["muted"],
            font=("DejaVu Sans", 8, "bold"),
        )

    def dibujar_error(self):
        self.titulo_pagina(
            "MONITOR LOCAL",
            "No se puede leer la base",
            "El receptor LoRa puede estar iniciando.",
        )
        caja = tk.Frame(
            self.contenido,
            bg="#FFF0EE",
            padx=25,
            pady=25,
        )
        caja.pack(fill="x")
        tk.Label(
            caja,
            text="No se recibieron datos de SQLite",
            bg="#FFF0EE",
            fg=COLORES["error"],
            font=("DejaVu Sans", 16, "bold"),
        ).pack(anchor="w")
        tk.Label(
            caja,
            text=str(self.error_lectura),
            bg="#FFF0EE",
            fg=COLORES["tinta"],
            font=("DejaVu Sans", 10),
            wraplength=900,
            justify="left",
            pady=8,
        ).pack(anchor="w")
        tk.Label(
            caja,
            text="La aplicación volverá a intentarlo automáticamente.",
            bg="#FFF0EE",
            fg=COLORES["muted"],
            font=("DejaVu Sans", 9),
        ).pack(anchor="w")

    def dibujar(self):
        self.limpiar_contenido()
        self.referencias_resumen = []
        self.tarjetas_metricas = []
        self.referencias_reles = []
        self.detalle_nodo = None
        self.detalle_etiqueta = None
        self.alerta_resumen = None
        self.canvas_historial = None
        self.historial_cabecera = None
        self.historial_tabla = None
        self.ultimo_refresco_historial = None
        if self.error_lectura and not self.datos:
            self.dibujar_error()
            self.vista_construida = True
            return
        if self.pagina == "Resumen":
            self.dibujar_resumen()
        elif self.pagina == "Invernadero":
            self.dibujar_invernadero()
        elif self.pagina == "Actuadores":
            self.dibujar_actuadores()
        else:
            self.dibujar_historial()
        self.vista_construida = True

    def actualizar_pagina_visible(self):
        if not self.vista_construida:
            self.dibujar()
            return
        if self.pagina == "Resumen":
            self.actualizar_resumen()
        elif self.pagina == "Invernadero":
            self.actualizar_metricas()
            self.actualizar_franja_detalle()
        elif self.pagina == "Actuadores":
            self.actualizar_metricas()
            self.actualizar_reles()
            self.actualizar_franja_detalle()
        else:
            self.actualizar_historial()

    def actualizar_estado_superior(self):
        ahora = datetime.now().astimezone()
        self.reloj.configure(
            text=ahora.strftime("%H:%M:%S\n%d/%m/%Y"),
        )
        en_linea = sum(
            1 for fila in self.datos.values() if esta_en_linea(fila)
        )
        if self.error_lectura:
            self.estado_global.configure(
                text="●  ERROR DE BASE",
                bg="#FFF0EE",
                fg=COLORES["error"],
            )
        elif en_linea:
            self.estado_global.configure(
                text=f"●  {en_linea}/2 NODOS EN LÍNEA",
                bg=COLORES["menta"],
                fg=COLORES["ok"],
            )
        else:
            self.estado_global.configure(
                text="●  SIN NODOS RECIENTES",
                bg="#FFF0CF",
                fg=COLORES["advertencia"],
            )
        texto_pie = f"Base local: {self.lector.ruta}"
        if self.ultimo_refresco:
            texto_pie += (
                "   ·   última consulta "
                + self.ultimo_refresco.strftime("%H:%M:%S")
            )
        self.pie_estado.configure(text=texto_pie)

    def refrescar(self):
        estaba_sin_datos = self.error_lectura is not None and not self.datos
        try:
            self.datos = self.lector.actuales()
            self.error_lectura = None
            self.ultimo_refresco = datetime.now().astimezone()
        except (OSError, sqlite3.Error, ValueError) as error:
            self.error_lectura = error
        self.actualizar_estado_superior()
        esta_sin_datos = self.error_lectura is not None and not self.datos
        if (
            not self.vista_construida
            or estaba_sin_datos != esta_sin_datos
        ):
            self.dibujar()
        else:
            self.actualizar_pagina_visible()
        self.trabajo_programado = self.root.after(
            INTERVALO_MS,
            self.refrescar,
        )


def main():
    args = argumentos()
    root = tk.Tk()
    MonitorTlali(
        root,
        LectorSqlite(args.db),
        ventana=args.ventana,
    )
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
