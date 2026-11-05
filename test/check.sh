#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
#
# Revisiones de seguridad / security checks (make check):
#   1. analisis estatico de clang
#   2. filtro compilado con AddressSanitizer + UBSan
#   3. entradas malformadas: deben rechazarse sin crash
#   4. shellcheck de los scripts (si esta instalado)
#
set -eu
cd "$(dirname "$0")/.."

B=build
A=$B/asan
fail=0

ok()  { echo "OK    $*"; }
bad() { echo "FALLA $*"; fail=1; }

# 1. Analisis estatico
if clang --analyze -Xanalyzer -analyzer-output=text -Isrc/third_party \
	src/rastertozjs.c -o /dev/null > $B/analyze.txt 2>&1 \
	&& ! grep -q "warning:" $B/analyze.txt; then
    ok "clang --analyze sin hallazgos"
else
    bad "clang --analyze:"; cat $B/analyze.txt
fi

# 2. Filtro con sanitizers
mkdir -p $A
clang -fsanitize=address,undefined -fno-omit-frame-pointer -g -O1 -w -Isrc/third_party -o $A/rastertozjs \
    src/rastertozjs.c src/third_party/jbig.c src/third_party/jbig_ar.c -lcups
clang -O1 -w -o $A/make-bad-raster test/make-bad-raster.c -lcups
export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

# run CASO ESPERADO(0|1) ARCHIVO [opciones]
run() {
    name=$1 expect=$2 file=$3 opts=${4:-HPFirmware=Never}
    set +e
    $A/rastertozjs 1 test check 1 "$opts" "$file" > $A/out.zjs 2> $A/err.txt
    rc=$?
    set -e
    if grep -q "Sanitizer" $A/err.txt; then
	bad "$name: error de memoria detectado"; head -20 $A/err.txt
    elif [ $rc -ne "$expect" ]; then
	bad "$name: codigo de salida $rc, esperado $expect"; tail -3 $A/err.txt
    else
	ok "$name (salida $rc)"
    fi
}

# Paginas reales de las pruebas normales (las genera test/run-tests.sh)
if [ ! -f $B/test-letter.ras ]; then
    cupsfilter -e -p $B/test.ppd -m application/vnd.cups-raster \
	$B/test-letter.pdf > $B/test-letter.ras 2> /dev/null
fi
run "pagina real con sanitizers" 0 $B/test-letter.ras

for c in ok truncated; do
    $A/make-bad-raster $c > $A/$c.ras
    run "raster $c" 0 $A/$c.ras
done
for c in huge tiny-bpl nan-bbox bad-res bad-bpp; do
    $A/make-bad-raster $c > $A/$c.ras
    run "raster malformado $c (rechazado)" 1 $A/$c.ras
done
: > $A/empty.ras
run "raster vacio (rechazado)" 1 $A/empty.ras
head -c 5000 /dev/urandom > $A/random.ras
run "raster aleatorio (rechazado)" 1 $A/random.ras

# Firmware falso: no debe enviarse nada a la impresora
printf 'esto no es un firmware' > $A/fake.dl
set +e
HP1018_FIRMWARE=$PWD/$A/fake.dl $A/rastertozjs 1 test check 1 HPFirmware=Always \
    $A/ok.ras > $A/out.zjs 2> $A/err.txt
rc=$?
set -e
if [ $rc -ne 0 ] && [ ! -s $A/out.zjs ] && grep -q "not an HP LaserJet firmware" $A/err.txt; then
    ok "firmware falso rechazado sin enviar datos"
else
    bad "firmware falso: rc=$rc, $(wc -c < $A/out.zjs) bytes enviados"
fi

# 4. shellcheck
if command -v shellcheck > /dev/null 2>&1; then
    if shellcheck -s sh scripts/*.sh test/*.sh; then
	ok "shellcheck sin hallazgos"
    else
	bad "shellcheck"
    fi
else
    echo "AVISO shellcheck no esta instalado (brew install shellcheck)"
fi

exit $fail
