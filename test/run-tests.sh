#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
#
# Prueba sin impresora: PDF -> cgpdftoraster -> rastertozjs -> ZjStream,
# y valida el resultado con zjsdecode (de foo2zjs).
#
set -eu
cd "$(dirname "$0")/.."

B=build
sed "s#/Library/Printers/HP1018/Filter/rastertozjs#$PWD/$B/rastertozjs#" \
    ppd/HP-LaserJet-1018.ppd > $B/test.ppd
python3 test/make-test-pdf.py $B/test-letter.pdf
python3 test/make-test-pdf.py $B/test-a4.pdf 595 842

fail=0

# check NOMBRE PDF "ancho alto papel econo" opciones...
check() {
    name=$1 pdf=$2 expect=$3
    shift 3
    rm -f "$B/page-$name"-*.p?m
    cupsfilter -e -p $B/test.ppd -m printer/foo -o HPFirmware=Never "$@" \
	"$pdf" > "$B/out-$name.zjs" 2> "$B/log-$name.txt"
    got=$($B/zjsdecode -d "$B/page-$name" "$B/out-$name.zjs" 2>&1 | awk '
	/ZJI_RASTER_X,/ { x = $2 } /ZJI_RASTER_Y,/ { y = $2 }
	/ZJI_DMPAPER,/ { p = $2 } /ZJI_ECONOMODE,/ { e = $2 }
	END { gsub(",", "", x); gsub(",", "", y); gsub(",", "", p);
	      gsub(",", "", e); print x, y, p, e }')
    # zjsdecode escribe .pbm a 600 ppp y .pgm a 1200x600 (2 bits/pixel)
    if [ "$got" = "$expect" ] && ls "$B/page-$name"-01-1.p?m > /dev/null 2>&1; then
	echo "OK    $name ($got)"
    else
	echo "FALLA $name: esperado '$expect', obtenido '$got'"
	fail=1
    fi
}

check letter     $B/test-letter.pdf "4908 6408 1 0"
check a4         $B/test-a4.pdf     "4766 6825 9 0" -o PageSize=A4
check letter1200 $B/test-letter.pdf "9816 6408 1 1" \
    -o Resolution=1200x600dpi -o HPEconoMode=On

# Con firmware "Always" el flujo debe empezar con el firmware
if [ -f $B/sihp1018.dl ]; then
    # cupsfilter no pasa variables de entorno: se llama al filtro directamente
    cupsfilter -e -p $B/test.ppd -m application/vnd.cups-raster \
	$B/test-letter.pdf > $B/test-letter.ras 2> /dev/null
    HP1018_FIRMWARE=$PWD/$B/sihp1018.dl $B/rastertozjs 1 test prueba 1 \
	HPFirmware=Always $B/test-letter.ras > $B/out-fw.zjs 2> /dev/null || true
    if head -c "$(stat -f%z $B/sihp1018.dl)" $B/out-fw.zjs \
	| cmp -s - $B/sihp1018.dl; then
	echo "OK    firmware antepuesto al trabajo"
    else
	echo "FALLA firmware no antepuesto"
	fail=1
    fi
else
    echo "SALTA prueba de firmware: falta / skipped, missing $B/sihp1018.dl (sh scripts/get-firmware.sh)"
fi

exit $fail
