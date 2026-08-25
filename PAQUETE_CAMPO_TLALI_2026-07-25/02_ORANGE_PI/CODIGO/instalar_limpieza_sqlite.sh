#!/usr/bin/env bash
set -euo pipefail

ORIGEN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="/home/orangepi/tlali/orange-pi"
MARCA="$(date -u +%Y%m%dT%H%M%SZ)"
RESPALDO="${DESTINO}/respaldos/${MARCA}-limpieza-sqlite"

mkdir -p "${DESTINO}" "${DESTINO}/datos" "${RESPALDO}"

for ARCHIVO in almacen_local.py limpiar_sqlite.py; do
  if [[ -f "${DESTINO}/${ARCHIVO}" ]]; then
    cp -a "${DESTINO}/${ARCHIVO}" "${RESPALDO}/${ARCHIVO}"
  fi
  install -m 0644 "${ORIGEN}/${ARCHIVO}" "${DESTINO}/${ARCHIVO}"
done

for UNIDAD in tlali-limpieza.service tlali-limpieza.timer; do
  if [[ -f "/etc/systemd/system/${UNIDAD}" ]]; then
    sudo cp -a "/etc/systemd/system/${UNIDAD}" "${RESPALDO}/${UNIDAD}"
  fi
  sudo install -m 0644 \
    "${ORIGEN}/${UNIDAD}" \
    "/etc/systemd/system/${UNIDAD}"
done

sudo systemctl daemon-reload
sudo systemctl enable --now tlali-limpieza.timer

# Primera ejecución inmediata. Conservará siete días y puede no encontrar aún
# registros suficientemente antiguos.
sudo systemctl start tlali-limpieza.service

echo
echo "Limpieza automática de SQLite instalada."
echo "Respaldo: ${RESPALDO}"
echo
sudo systemctl status tlali-limpieza.timer --no-pager
echo
sudo journalctl -u tlali-limpieza.service -n 30 --no-pager
