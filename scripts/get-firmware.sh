#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
#
# Descarga el firmware de la HP LaserJet 1018 desde el plugin oficial de
# HPLIP (publicado por HP en OpenPrinting) y lo deja en build/sihp1018.dl.
# Downloads the HP LaserJet 1018 firmware from HP's official HPLIP plugin.
#
# El firmware es propiedad de HP y no se redistribuye con este driver: cada
# usuario debe aceptar la licencia de HP. No se ejecuta nada del plugin, solo
# se extraen dos archivos y el firmware se verifica con un SHA-256 fijo.
#
# Uso / Usage:
#   scripts/get-firmware.sh [--accept-hp-license] [hplip-X.Y.Z-plugin.run]
#
set -eu

PLUGIN_VER=3.24.4
PLUGIN=hplip-$PLUGIN_VER-plugin.run
URL=https://www.openprinting.org/download/printdriver/auxfiles/HP/plugins/$PLUGIN
# SHA-256 del firmware ya descomprimido (fecha de firmware 20080822)
FW_SHA256=b7517a2b6670e964de9df23fec600e05e3c54e09bb62183b453a1ffaae5711bf

ACCEPT=0
LOCAL=
for arg in "$@"; do
    case "$arg" in
    --accept-hp-license) ACCEPT=1 ;;
    -*) echo "Opcion desconocida / unknown option: $arg" >&2; exit 2 ;;
    *) LOCAL=$arg ;;
    esac
done

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/sihp1018.dl
WORK=$(mktemp -d "${TMPDIR:-/tmp}/hp1018fw.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

fail() {
    echo "ERROR: $*" >&2
    exit 1
}

if [ -n "$LOCAL" ]; then
    cp "$LOCAL" "$WORK/plugin.run"
else
    echo "Descargando / Downloading $URL ..."
    curl -fL --proto '=https' --tlsv1.2 --retry 3 --max-filesize 52428800 \
	-o "$WORK/plugin.run" "$URL" \
	|| fail "no se pudo descargar el plugin / download failed"
fi

# Es un archivo makeself: cabecera shell + tar.gz al final del archivo.
SIZE=$(sed -n 's/^filesizes="\([0-9]*\)".*/\1/p' "$WORK/plugin.run" | head -1)
MD5=$(sed -n 's/^MD5="\([0-9a-f]*\)".*/\1/p' "$WORK/plugin.run" | head -1)
[ -n "$SIZE" ] && [ -n "$MD5" ] \
    || fail "el plugin no tiene el formato esperado / unexpected plugin format"
[ "$SIZE" -lt "$(stat -f%z "$WORK/plugin.run")" ] \
    || fail "el plugin esta truncado / plugin is truncated"
tail -c "$SIZE" "$WORK/plugin.run" > "$WORK/payload.tgz"
[ "$(md5 -q "$WORK/payload.tgz")" = "$MD5" ] \
    || fail "el plugin esta corrupto (MD5) / plugin is corrupt (MD5)"

# Solo se extraen estos dos archivos, por nombre exacto
mkdir "$WORK/x"
tar -xzf "$WORK/payload.tgz" -C "$WORK/x" license.txt hp_laserjet_1018.fw.gz \
    || fail "faltan archivos en el plugin / files missing from plugin"

# Licencia de HP: hay que aceptarla antes de guardar el firmware
echo
echo "=================================================================="
echo " Licencia de HP para el firmware / HP license for the firmware"
echo "=================================================================="
cat "$WORK/x/license.txt"
echo "=================================================================="
if [ "$ACCEPT" -ne 1 ]; then
    printf '\nEscribe "acepto" para aceptar la licencia de HP\n'
    printf '(type "accept" to accept the HP license): '
    read -r answer || answer=
    case "$answer" in
    acepto|ACEPTO|accept|ACCEPT) ;;
    *) fail "licencia no aceptada; no se guardo nada / license not accepted, nothing saved" ;;
    esac
fi

gunzip -c "$WORK/x/hp_laserjet_1018.fw.gz" > "$WORK/sihp1018.dl"
[ "$(shasum -a 256 "$WORK/sihp1018.dl" | cut -d' ' -f1)" = "$FW_SHA256" ] \
    || fail "el firmware no coincide con la version verificada / firmware checksum mismatch"

mkdir -p "$ROOT/build"
mv "$WORK/sihp1018.dl" "$OUT"
echo "Firmware listo / ready: $OUT"
