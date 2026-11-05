---
name: Test report / Reporte de prueba
about: Tell us whether the driver works on your Mac / Cuéntanos si el driver funciona en tu Mac
title: "[Test] macOS X.Y, Mac model"
labels: test-report
---

**Result / Resultado:** works / partly works / does not work — funciona / funciona en parte / no funciona

**Mac model / Modelo de Mac** (Apple Silicon or Intel):

**macOS version / Versión de macOS** (Apple menu → About This Mac):

**Connection / Conexión:** direct USB / hub / dock

**What did you print and what came out? / ¿Qué imprimiste y qué salió?**

**Output of / Salida de** `make test`:

```
(paste here / pega aquí)
```

**If it failed / Si falló:** run `sudo cupsctl --debug-logging`, try to print again, then
paste the lines of `/var/log/cups/error_log` from that job (remove anything private), and
afterwards run `sudo cupsctl --no-debug-logging`.

```
(paste here / pega aquí)
```
