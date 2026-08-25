#!/usr/bin/env bash
set -euo pipefail

ORIGEN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="/home/orangepi/tlali/orange-pi"
MARCA="$(date -u +%Y%m%dT%H%M%SZ)"
RESPALDO="${DESTINO}/respaldos/${MARCA}"

mkdir -p "${DESTINO}" "${DESTINO}/datos" "${RESPALDO}"

for ARCHIVO in recibir_lora.py almacen_local.py consultar_datos.py requirements.txt; do
  if [[ -f "${DESTINO}/${ARCHIVO}" ]]; then
    cp -a "${DESTINO}/${ARCHIVO}" "${RESPALDO}/${ARCHIVO}"
  fi
  install -m 0644 "${ORIGEN}/${ARCHIVO}" "${DESTINO}/${ARCHIVO}"
done

if [[ -f /etc/systemd/system/tlali-lora.service ]]; then
  sudo cp -a \
    /etc/systemd/system/tlali-lora.service \
    "${RESPALDO}/tlali-lora.service"
fi

sudo install -m 0644 \
  "${ORIGEN}/tlali-lora.service" \
  /etc/systemd/system/tlali-lora.service

sudo systemctl daemon-reload
sudo systemctl restart tlali-lora.service

echo
echo "Instalacion terminada."
echo "Base local: ${DESTINO}/datos/tlali.db"
echo "Respaldo: ${RESPALDO}"
echo
sudo systemctl status tlali-lora.service --no-pager
