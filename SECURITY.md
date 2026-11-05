# Security / Seguridad

## What runs with privileges / Qué corre con privilegios

| Component | Runs as | Notes |
|---|---|---|
| `scripts/install.sh`, `scripts/uninstall.sh` | root (via `sudo`) | Only copy files to `/Library/Printers/HP1018` and the PPD folder, and create/remove the `HP_LaserJet_1018` queue. `install.sh` verifies the firmware's SHA-256, that the filter is a Mach-O binary and that the PPD is intact before installing. |
| `rastertozjs` (CUPS filter) | CUPS (`_lp`), inside the macOS CUPS sandbox | Reads raster from the print job and writes to the printer. No network access. |
| `scripts/get-firmware.sh` | the user | Downloads HP's HPLIP plugin over HTTPS only, never executes it, extracts two named files, requires accepting HP's license, and pins the firmware's SHA-256. |

## Hardening in place / Medidas aplicadas

- The filter validates every raster page header (resolution, bit depth, dimensions,
  bytes per line, imaging box) before allocating memory or touching the printer, and fails
  the job with an error instead of printing garbage.
- The filter only sends a firmware file that is under 1 MB and starts with HP's PJL/ACL
  signature. The `HP1018_FIRMWARE` override is ignored when running under CUPS.
- `make check` runs: clang static analyzer, AddressSanitizer + UndefinedBehaviorSanitizer on
  real and malformed rasters (huge sizes, inconsistent line length, NaN imaging box, bad
  resolution/depth, truncated, empty and random data, fake firmware), and shellcheck.

## Reporting a vulnerability / Reportar una vulnerabilidad

Please **do not** open a public issue. Use GitHub's **"Report a vulnerability"** button
(Security tab → Advisories) to report it privately. Include the macOS version, steps to
reproduce and, if possible, a sample file.

Por favor **no** abras un *issue* público. Usa el botón **"Report a vulnerability"** de
GitHub (pestaña Security) para reportarlo en privado.

This is a volunteer project: there is no guaranteed response time, but reports are taken
seriously. / Es un proyecto voluntario: no hay plazo de respuesta garantizado.

## Out of scope / Fuera de alcance

- HP's proprietary firmware and HP's Windows driver (report those to HP).
- Issues that require an attacker who already has administrator access to the Mac.
- Printer hangs caused by querying the printer while its firmware boots (documented,
  not a security issue).
