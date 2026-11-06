# Changelog

## Unreleased

- Windows: tested on **Windows 11** with a real HP LaserJet 1018 (catalog driver + script).
- Windows guide points to the right Microsoft Update Catalog package ("Windows 8.1 and later
  drivers", 2012-09-17, covers Windows 10 and 11) and runs the script with
  `powershell -ExecutionPolicy Bypass -File ...`; adds a no-script "Have Disk" alternative
  and the Windows 11 Windows Update path.
- READMEs now present the repository as the solution for both macOS and Windows 10/11.

## v0.1.0-beta — 2026-11-05

First public release. / Primera versión pública.

- Native CUPS filter `rastertozjs` (arm64; universal build supported) for the HP LaserJet
  1018: CUPS raster → ZjStream/JBIG, equivalent to `foo2zjs -z1 -P -L0`.
- Automatic firmware upload when the printer has just been powered on, waiting 12 s for the
  firmware to boot before querying the printer again.
- PPD with 600 / 1200x600 dpi, density 1–5, EconoMode, firmware mode and 13 paper sizes,
  in English and Spanish.
- `get-firmware.sh`: downloads HP's firmware from the official HPLIP plugin, shows HP's
  license and requires accepting it; SHA-256 pinned.
- Install / uninstall scripts with integrity checks.
- Offline tests (`make test`) and security checks (`make check`).
- Windows 10 guide and PowerShell script for HP's official driver (untested).

Tested on a real HP LaserJet 1018 with macOS 27.2 on Apple Silicon, directly and through a
USB-C dock with chained USB 2.0 hubs.
