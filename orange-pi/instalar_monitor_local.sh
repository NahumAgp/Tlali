#!/usr/bin/env bash
set -euo pipefail

ORIGEN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTINO="/home/orangepi/tlali/orange-pi"
AUTOINICIO="/home/orangepi/.config/autostart"
ESCRITORIO="/home/orangepi/Desktop"
MARCA="$(date -u +%Y%m%dT%H%M%SZ)"
RESPALDO="${DESTINO}/respaldos/${MARCA}-monitor-local"

mkdir -p "${DESTINO}" "${AUTOINICIO}" "${ESCRITORIO}" "${RESPALDO}"

if ! /usr/bin/python3 -c "import tkinter" >/dev/null 2>&1; then
  echo "Instalando la biblioteca gráfica de Python..."
  sudo apt-get update
  sudo apt-get install -y python3-tk
fi

for ARCHIVO in monitor_local.py tlali-monitor.desktop; do
  if [[ -f "${DESTINO}/${ARCHIVO}" ]]; then
    cp -a "${DESTINO}/${ARCHIVO}" "${RESPALDO}/${ARCHIVO}"
  fi
  install -m 0644 "${ORIGEN}/${ARCHIVO}" "${DESTINO}/${ARCHIVO}"
done

install -m 0644 \
  "${ORIGEN}/tlali-monitor.desktop" \
  "${AUTOINICIO}/tlali-monitor.desktop"
install -m 0755 \
  "${ORIGEN}/tlali-monitor.desktop" \
  "${ESCRITORIO}/TLALI-Monitor.desktop"

chown -R orangepi:orangepi "${AUTOINICIO}" "${ESCRITORIO}"

echo
echo "Monitor local TLALI instalado."
echo "Respaldo: ${RESPALDO}"
echo
echo "Puedes abrirlo ahora con:"
echo "  DISPLAY=:0 /usr/bin/python3 ${DESTINO}/monitor_local.py --db ${DESTINO}/datos/tlali.db"
echo
echo "También se abrirá automáticamente en el próximo inicio de sesión gráfico."
echo "Esc cierra la aplicación y F11 cambia el modo de pantalla completa."
