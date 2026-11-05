#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
#
# Instala el driver HP LaserJet 1018 (rastertozjs) y crea la cola de impresion.
#
# Uso:  make && sh scripts/get-firmware.sh && sudo sh scripts/install.sh
#
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=/Library/Printers/HP1018
PPD_DIR=/Library/Printers/PPDs/Contents/Resources
PPD_NAME="HP LaserJet 1018 rastertozjs.gz"
QUEUE=${QUEUE:-HP_LaserJet_1018}
# Mismo SHA-256 que scripts/get-firmware.sh
FW_SHA256=b7517a2b6670e964de9df23fec600e05e3c54e09bb62183b453a1ffaae5711bf

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

[ "$(id -u)" -eq 0 ] || fail "ejecuta con sudo / run with sudo: sudo sh $0"
case "$QUEUE" in
    *[!A-Za-z0-9_-]*|"") fail "nombre de cola invalido / invalid queue name: $QUEUE" ;;
esac
for f in build/rastertozjs build/sihp1018.dl build/HP-LaserJet-1018.ppd.gz; do
    [ -f "$ROOT/$f" ] || fail "falta / missing $f. Ejecuta primero / run first: make all ppd && sh scripts/get-firmware.sh"
done

# Comprobaciones antes de instalar nada como root
[ "$(shasum -a 256 "$ROOT/build/sihp1018.dl" | cut -d' ' -f1)" = "$FW_SHA256" ] \
    || fail "build/sihp1018.dl no es el firmware verificado / is not the verified firmware"
file "$ROOT/build/rastertozjs" | grep -q "Mach-O" \
    || fail "build/rastertozjs no es un ejecutable de macOS / is not a macOS executable"
gzip -t "$ROOT/build/HP-LaserJet-1018.ppd.gz" \
    || fail "build/HP-LaserJet-1018.ppd.gz esta danado / is damaged"

echo "Instalando archivos en $DEST ..."
install -d -o root -g wheel -m 755 "$DEST" "$DEST/Filter" "$DEST/Firmware"
install -o root -g wheel -m 755 "$ROOT/build/rastertozjs" "$DEST/Filter/rastertozjs"
install -o root -g wheel -m 644 "$ROOT/build/sihp1018.dl" "$DEST/Firmware/sihp1018.dl"
install -o root -g wheel -m 644 "$ROOT/build/HP-LaserJet-1018.ppd.gz" "$PPD_DIR/$PPD_NAME"

# Busca la impresora conectada por USB
URI=${DEVICE_URI:-$(lpinfo -v 2>/dev/null | awk '/usb:\/\/.*LaserJet%201018/ { print $2; exit }')}
if [ -z "$URI" ]; then
    echo "Driver instalado, pero no se encontro la 1018 por USB."
    echo "Conectala y encendela, y vuelve a ejecutar este script para crear la cola."
    echo "(Driver installed, but no 1018 found on USB: connect it, power it on and run this again.)"
    exit 0
fi

echo "Creando la cola / creating queue $QUEUE en $URI ..."
lpadmin -p "$QUEUE" -E -v "$URI" -P "$PPD_DIR/$PPD_NAME" \
    -D "HP LaserJet 1018" -L "USB" \
    -o printer-is-shared=false -o printer-error-policy=retry-job
cupsenable "$QUEUE"
cupsaccept "$QUEUE"

echo "Listo / done. Prueba / test:  lp -d $QUEUE archivo.pdf"
