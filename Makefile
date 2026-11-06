# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
# Driver HP LaserJet 1018 para macOS (CUPS)
#
#   make            compila el filtro
#   make ppd        comprime el PPD
#   make test       pruebas sin impresora
#   make check      pruebas + revisiones de seguridad (analizador, sanitizers)
#
# Por defecto genera un binario universal (Apple Silicon + Intel).
# Solo una arquitectura:  make ARCHS="-arch arm64"

CC	?= clang
ARCHS	?= -arch arm64 -arch x86_64
MINOS	?= -mmacosx-version-min=11.0
CFLAGS	?= -O2
CFLAGS	+= $(ARCHS) $(MINOS) -Wall -Wextra -Werror -Isrc/third_party
LDLIBS	= -lcups

BUILD	= build
JBIG	= $(BUILD)/jbig.o $(BUILD)/jbig_ar.o

all: $(BUILD)/rastertozjs

tools: $(BUILD)/zjsdecode

$(BUILD):
	mkdir -p $@

$(BUILD)/%.o: src/third_party/%.c | $(BUILD)
	$(CC) $(ARCHS) $(MINOS) -O2 -w -c -o $@ $<

$(BUILD)/rastertozjs.o: src/rastertozjs.c src/third_party/zjs.h src/third_party/jbig.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/rastertozjs: $(BUILD)/rastertozjs.o $(JBIG)
	$(CC) $(ARCHS) $(MINOS) -o $@ $^ $(LDLIBS)

# zjsdecode (foo2zjs) solo se usa para verificar la salida en las pruebas
$(BUILD)/zjsdecode: tools/zjsdecode.c $(JBIG)
	$(CC) $(ARCHS) $(MINOS) -O2 -w -Isrc/third_party -o $@ $^

ppd: $(BUILD)/HP-LaserJet-1018.ppd.gz

$(BUILD)/HP-LaserJet-1018.ppd.gz: ppd/HP-LaserJet-1018.ppd | $(BUILD)
	gzip -9 -c $< > $@

test: all tools ppd
	sh test/run-tests.sh

check: test
	sh test/check.sh

clean:
	rm -rf $(BUILD)

.PHONY: all tools ppd test check clean
