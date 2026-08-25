#!/usr/bin/env bash
set -euo pipefail

ORIGEN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="/home/orangepi/tlali/orange-pi"
CREDENCIAL="/home/orangepi/tlali/credenciales/firebase-service-account.json"
MARCA="$(date -u +%Y%m%dT%H%M%SZ)"
RESPALDO="${DESTINO}/respaldos/${MARCA}-firebase"

if [[ ! -f "${CREDENCIAL}" ]]; then
  echo "Falta la credencial: ${CREDENCIAL}" >&2
  exit 1
fi

chmod 600 "${CREDENCIAL}"
mkdir -p "${DESTINO}" "${DESTINO}/datos" "${RESPALDO}"

"${DESTINO}/.venv/bin/python" -c \
  'import sys; raise SystemExit(0 if sys.version_info >= (3, 9) else 1)' || {
    echo "Firebase Admin requiere Python 3.9 o superior." >&2
    exit 1
  }

for ARCHIVO in almacen_local.py sincronizar_firebase.py requirements.txt; do
  if [[ -f "${DESTINO}/${ARCHIVO}" ]]; then
    cp -a "${DESTINO}/${ARCHIVO}" "${RESPALDO}/${ARCHIVO}"
  fi
  install -m 0644 "${ORIGEN}/${ARCHIVO}" "${DESTINO}/${ARCHIVO}"
done

"${DESTINO}/.venv/bin/python" -m pip install \
  --disable-pip-version-check \
  -r "${DESTINO}/requirements.txt"

if [[ -f /etc/systemd/system/tlali-firebase.service ]]; then
  sudo cp -a \
    /etc/systemd/system/tlali-firebase.service \
    "${RESPALDO}/tlali-firebase.service"
fi

sudo install -m 0644 \
  "${ORIGEN}/tlali-firebase.service" \
  /etc/systemd/system/tlali-firebase.service

sudo systemctl daemon-reload
sudo systemctl enable --now tlali-firebase.service

echo
echo "Sincronizador Firebase instalado."
echo "Respaldo: ${RESPALDO}"
echo
sudo systemctl status tlali-firebase.service --no-pager
