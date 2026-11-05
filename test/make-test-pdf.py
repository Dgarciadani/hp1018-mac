#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
"""Genera una pagina PDF de prueba (texto, lineas finas, grises y marco).

Uso: make-test-pdf.py salida.pdf [ancho alto]   (en puntos, por defecto Carta)
"""
import sys

out = sys.argv[1]
w = int(sys.argv[2]) if len(sys.argv) > 2 else 612
h = int(sys.argv[3]) if len(sys.argv) > 3 else 792

ops = []
# Marco a 18 pt del borde: debe verse completo dentro del area imprimible
ops.append("1 w 0 G 18 18 %d %d re S" % (w - 36, h - 36))
ops.append("BT /F1 22 Tf 40 %d Td (HP LaserJet 1018 - prueba rastertozjs) Tj ET" % (h - 60))
ops.append("BT /F1 11 Tf 40 %d Td (Texto pequeno: ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789) Tj ET" % (h - 85))
ops.append("BT /F1 6 Tf 40 %d Td (Texto de 6 puntos para comprobar nitidez a 600 ppp.) Tj ET" % (h - 100))
# Escala de grises
for i in range(11):
    g = i / 10.0
    ops.append("%.2f g %d %d 40 40 re f" % (g, 40 + i * 45, h - 170))
# Lineas finas
for i in range(10):
    ops.append("%.2f w 0 G %d %d m %d %d l S" % (0.25 + i * 0.25, 40, h - 200 - i * 12, w - 40, h - 200 - i * 12))
# Circulo negro relleno aproximado con curvas
cx, cy, r, k = w / 2, h / 2 - 100, 80, 0.5523 * 80
ops.append("0 g %.1f %.1f m %.1f %.1f %.1f %.1f %.1f %.1f c %.1f %.1f %.1f %.1f %.1f %.1f c "
           "%.1f %.1f %.1f %.1f %.1f %.1f c %.1f %.1f %.1f %.1f %.1f %.1f c f" % (
               cx + r, cy, cx + r, cy + k, cx + k, cy + r, cx, cy + r,
               cx - k, cy + r, cx - r, cy + k, cx - r, cy,
               cx - r, cy - k, cx - k, cy - r, cx, cy - r,
               cx + k, cy - r, cx + r, cy - k, cx + r, cy))
content = "\n".join(ops).encode("latin-1")

objs = [
    b"<< /Type /Catalog /Pages 2 0 R >>",
    b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %d %d] /Contents 4 0 R "
     "/Resources << /Font << /F1 5 0 R >> >> >>" % (w, h)).encode(),
    b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
    b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
]
pdf = bytearray(b"%PDF-1.4\n")
offsets = []
for i, o in enumerate(objs, 1):
    offsets.append(len(pdf))
    pdf += b"%d 0 obj\n" % i + o + b"\nendobj\n"
xref = len(pdf)
pdf += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
for off in offsets:
    pdf += b"%010d 00000 n \n" % off
pdf += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)
open(out, "wb").write(pdf)
