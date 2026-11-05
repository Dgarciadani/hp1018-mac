# Driver HP LaserJet 1018 para macOS (Apple Silicon e Intel)

[English](README.md) · **Español**

Driver libre para seguir usando la **HP LaserJet 1018** en macOS moderno, para el que HP ya
no publica drivers. Es nativo (sin Rosetta ni Ghostscript) y se instala con unos pocos
comandos. Para **Windows 10**, ver [windows/README-es.md](windows/README-es.md).

> **Estado: beta.** Probado con una 1018 real en macOS 27.2 (Mac con Apple Silicon),
> conectada directo y a través de un dock USB-C. Si lo pruebas en otro Mac, cuéntanos cómo
> te fue abriendo un *issue* con la plantilla "Reporte de prueba".

> Proyecto independiente, **no afiliado a HP**. HP y LaserJet son marcas de HP Inc.
> Se entrega **sin ninguna garantía** (ver [LICENSE](LICENSE)).

## Cómo funciona

La 1018 es una impresora "host-based": no entiende PostScript ni PCL, solo **ZjStream**
(imágenes comprimidas en JBIG), y **olvida su firmware cada vez que se apaga**.

```
Aplicación → PDF → cgpdftoraster (de Apple) → rastertozjs (este driver) → USB
```

Antes de cada trabajo, `rastertozjs` pregunta a la impresora si tiene el firmware cargado.
Si no lo tiene (recién encendida), se lo envía, espera 12 segundos a que arranque y luego
imprime.

## Instalación paso a paso

Necesitas un Mac con macOS 11 o posterior, la 1018 conectada por USB y encendida, y conexión
a internet.

**1. Instala las herramientas de desarrollo de Apple** (gratis; solo la primera vez).
Abre la app **Terminal** y escribe:

```bash
xcode-select --install
```

Acepta la ventana que aparece y espera a que termine. Si dice que ya están instaladas, sigue.

**2. Descarga este proyecto.** En la página de GitHub del proyecto: botón verde **Code →
Download ZIP**. Descomprime el ZIP (por ejemplo en tu carpeta Descargas). En Terminal, entra
a la carpeta; escribe `cd ` (con espacio) y arrastra la carpeta a la ventana de Terminal:

```bash
cd ~/Downloads/hp1018-mac-main
```

(El nombre exacto de la carpeta puede variar.)

**3. Compila el driver:**

```bash
make all ppd
```

**4. Descarga el firmware de HP.** El script muestra la licencia de HP; léela y escribe
`acepto` para continuar. El firmware no viene incluido en este proyecto porque la licencia
de HP no permite redistribuirlo.

```bash
sh scripts/get-firmware.sh
```

**5. (Opcional) Comprueba que todo funciona sin imprimir:**

```bash
make test
```

Deben salir 4 líneas que empiezan con `OK`.

**6. Instala** (pide la contraseña de tu Mac; mientras la escribes no se ve nada, es normal):

```bash
sudo sh scripts/install.sh
```

Si dice `Listo`, la impresora ya aparece como **HP LaserJet 1018** en el diálogo de impresión
de cualquier app.

**7. Imprime una prueba** desde cualquier app. La primera vez después de encender la
impresora tarda unos 15 segundos más: la luz naranja parpadea y los rodillos giran mientras
carga el firmware. Es normal.

## Opciones de impresión

Están en el diálogo de impresión, en "Funciones de la impresora", o se pasan con `lp -o`:

| Opción | Valores |
|---|---|
| `Resolution` | `600dpi` (por defecto), `1200x600dpi` |
| `PageSize` | Carta, Oficio, Ejecutivo, A4, A5, A6, B5, sobres... |
| `HPDensity` | `1` a `5` (por defecto 3) |
| `HPEconoMode` | `Off` / `On` (ahorro de tóner) |
| `HPFirmware` | `Auto` (por defecto), `Always`, `Never` |

## Desinstalar

```bash
sudo sh scripts/uninstall.sh
```

Borra el driver, el firmware instalado y la cola de impresión.

## Problemas conocidos y soluciones

| Síntoma | Qué hacer |
|---|---|
| Gira los rodillos pero no toma papel, y la cola queda "enviando datos" | Apaga la impresora 10 s y enciéndela. No consultes la impresora (por ejemplo con `lpinfo -l -v`) mientras arranca el firmware: eso la cuelga. |
| La primera página tarda | Normal: es la carga del firmware tras encenderla. |
| `install.sh` dice que no encontró la 1018 | Conéctala y enciéndela, espera unos segundos y vuelve a ejecutarlo. |
| `get-firmware.sh` no puede descargar el plugin | Revisa tu conexión. Si el archivo ya no existe en OpenPrinting, descarga `hplip-3.24.4-plugin.run` por tu cuenta y pásalo: `sh scripts/get-firmware.sh ruta/al/archivo.run` (se verifica igual). |

Para ver qué pasa en detalle:

```bash
sudo cupsctl --debug-logging
```

Luego mira `/var/log/cups/error_log`, y al terminar ejecuta `sudo cupsctl --no-debug-logging`.

## Seguridad

- El driver no se conecta a internet. Solo `get-firmware.sh` descarga el plugin oficial de
  HP por HTTPS, **sin ejecutar nada de él**, y verifica el firmware con una huella SHA-256 fija.
- El filtro valida cada página antes de procesarla y solo envía a la impresora un firmware
  con el formato correcto.
- `make check` ejecuta el analizador estático de clang, pruebas con AddressSanitizer/UBSan,
  entradas malformadas y shellcheck.
- Para reportar un problema de seguridad, ver [SECURITY.md](SECURITY.md).

## Para desarrolladores

```bash
make check                                  # pruebas + revisiones de seguridad
make ARCHS="-arch arm64 -arch x86_64"       # binario universal (Apple Silicon + Intel)
```

| Ruta | Contenido |
|---|---|
| `src/rastertozjs.c` | Filtro CUPS |
| `src/third_party/` | JBIG-KIT y `zjs.h` (de foo2zjs) |
| `ppd/HP-LaserJet-1018.ppd` | Descripción de la impresora para CUPS |
| `scripts/` | Firmware, instalación y desinstalación |
| `test/` | Pruebas sin impresora y revisiones de seguridad |
| `tools/zjsdecode.c` | Decodificador de foo2zjs, solo para pruebas |
| `windows/` | Guía y script para Windows 10 |

## Licencia y créditos

**GPL-2.0-or-later** ([LICENSE](LICENSE)). Basado en **foo2zjs** de Rick Richardson y en
**JBIG-KIT** de Markus Kuhn; detalles en [THIRD_PARTY.md](THIRD_PARTY.md). El firmware de
HP **no** forma parte de este proyecto y está sujeto a la licencia de HP que aceptas al
descargarlo.

Copyright (C) 2026 Gregorio Garcia.
