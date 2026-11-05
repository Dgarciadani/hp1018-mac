# Third-party components / Componentes de terceros

## Included in this repository / Incluidos en este repositorio

All of them are unmodified copies, licensed GPL-2.0-or-later (compatible with this project).
Todos son copias sin modificar, con licencia GPL-2.0-or-later (compatible con este proyecto).

| Files / Archivos | Author / Autor | Origin / Origen |
|---|---|---|
| `src/third_party/jbig.c`, `jbig.h`, `jbig_ar.c`, `jbig_ar.h` | Markus Kuhn (JBIG-KIT) | Bundled in foo2zjs (see below) |
| `src/third_party/zjs.h` | Rick Richardson (foo2zjs) | foo2zjs |
| `tools/zjsdecode.c` (tests only / solo pruebas) | Rick Richardson (foo2zjs) | foo2zjs |

Source of all of the above / Fuente de todo lo anterior: Debian source package
`foo2zjs_20200505dfsg0.orig.tar.xz`
(<https://deb.debian.org/debian/pool/main/f/foo2zjs/>),
SHA-256 `87465e6a31f8501b90508d50e97901d2cce8b5b3de5418cc156e66d5c500421b`.

`zjs.h` carries no license header of its own; it is distributed as part of foo2zjs under
GPL-2.0-or-later. / `zjs.h` no trae cabecera de licencia propia; se distribuye como parte de
foo2zjs bajo GPL-2.0-or-later.

`src/rastertozjs.c` is original code, but its ZjStream encoding follows `foo2zjs.c`
(model `-z1`, options `-P -L0`). / `src/rastertozjs.c` es código original, pero su
codificación ZjStream sigue a `foo2zjs.c`.

JBIG1 patents expired in 2012; using JBIG-KIT requires no patent license.
/ Las patentes de JBIG1 vencieron en 2012.

## NOT included / NO incluido

**HP LaserJet 1018 firmware (`sihp1018.dl`).** It is proprietary HP software. Its license
forbids copying it "onto any public network" and transferring it. It is never part of this
repository or of any release: `scripts/get-firmware.sh` downloads it on each user's Mac from
the official HPLIP plugin published on OpenPrinting, shows HP's license and requires the
user to accept it.

**Firmware de la HP LaserJet 1018 (`sihp1018.dl`).** Es software propietario de HP. Su
licencia prohíbe copiarlo en una red pública y transferirlo. Nunca forma parte de este
repositorio ni de ninguna versión publicada: `scripts/get-firmware.sh` lo descarga en el Mac
de cada usuario desde el plugin oficial de HPLIP publicado en OpenPrinting, muestra la
licencia de HP y exige aceptarla.

**HP Windows driver.** Proprietary; users download it themselves from Windows Update or the
Microsoft Update Catalog. / **Driver de HP para Windows.** Propietario; cada usuario lo
descarga desde Windows Update o el Microsoft Update Catalog.

## Trademarks / Marcas

HP and LaserJet are trademarks of HP Inc. This project is not affiliated with, endorsed by or
supported by HP Inc. / HP y LaserJet son marcas de HP Inc. Este proyecto no está afiliado,
respaldado ni soportado por HP Inc.
