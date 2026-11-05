#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
#
# Desinstala el driver HP LaserJet 1018 (rastertozjs) y su cola de impresion.
#
set -eu

QUEUE=${QUEUE:-HP_LaserJet_1018}

if [ "$(id -u)" -ne 0 ]; then
    echo "Ejecuta con sudo: sudo sh $0" >&2
    exit 1
fi

if lpstat -p "$QUEUE" > /dev/null 2>&1; then
    lpadmin -x "$QUEUE"
    echo "Cola $QUEUE eliminada"
fi
rm -f "/Library/Printers/PPDs/Contents/Resources/HP LaserJet 1018 rastertozjs.gz"
rm -rf /Library/Printers/HP1018
echo "Driver desinstalado"
