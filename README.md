# HP LaserJet 1018 on macOS and Windows 10/11

**English** · [Español](README.es.md)

Keep using the **HP LaserJet 1018** on current systems, where HP no longer ships drivers:

| System | Solution | Guide |
|---|---|---|
| **macOS** (Apple Silicon and Intel) | Free native driver in this repository (no Rosetta, no Ghostscript) | [below](#step-by-step-installation) |
| **Windows 10 and 11** | HP's official driver from the Microsoft Update Catalog, with a step-by-step guide and an install script | [windows/README.md](windows/README.md) |

> **Status: beta.** Tested with a real 1018 on macOS 27.2 (Apple Silicon Mac), connected
> directly and through a USB-C dock, and on **Windows 11** with the install script. If you
> try it on another computer, please let us know how it went by opening an issue with the
> "Test report" template.

> Independent project, **not affiliated with HP**. HP and LaserJet are trademarks of HP Inc.
> Provided **without any warranty** (see [LICENSE](LICENSE)).

## How it works

The 1018 is a "host-based" printer: it does not understand PostScript or PCL, only
**ZjStream** (JBIG-compressed bitmaps), and it **forgets its firmware every time it is
switched off**.

```
App → PDF → cgpdftoraster (Apple) → rastertozjs (this driver) → USB
```

Before each job, `rastertozjs` asks the printer whether its firmware is loaded. If not (just
powered on), it sends it, waits 12 seconds for it to boot, and then prints.

## Step-by-step installation

You need a Mac with macOS 11 or later, the 1018 connected by USB and switched on, and an
internet connection.

**1. Install Apple's developer tools** (free; first time only). Open the **Terminal** app
and type:

```bash
xcode-select --install
```

Accept the window that appears and wait until it finishes. If it says they are already
installed, move on.

**2. Download this project.** On the project's GitHub page: green **Code → Download ZIP**
button. Safari unzips it for you; otherwise double-click the ZIP. The folder can be anywhere.

In Terminal, type `cd` followed by a space (do not press Enter yet), **drag the project
folder** from the Finder onto the Terminal window and press Enter. Terminal types the path for
you, wherever it is. For example, if it ended up in Downloads it looks like this:

```bash
cd ~/Downloads/hp1018-mac-main
```

Check that you are in the right folder:

```bash
ls Makefile
```

It should answer `Makefile`. If it says `No such file or directory`, you dragged a different
folder: repeat the `cd` with the folder that contains the `Makefile` file.

If you use git, instead of downloading the ZIP you can run:

```bash
git clone https://github.com/Dgarciadani/hp1018-mac.git
cd hp1018-mac
```

**3. Build the driver:**

```bash
make all ppd
```

**4. Download HP's firmware.** The script shows HP's license; read it and type `accept` to
continue. The firmware is not included in this project because HP's license does not allow
redistributing it.

```bash
sh scripts/get-firmware.sh
```

**5. (Optional) Check that everything works without printing:**

```bash
make test
```

You should see 4 lines starting with `OK`.

**6. Install** (asks for your Mac password; nothing shows while you type, that is normal):

```bash
sudo sh scripts/install.sh
```

When it says `Listo / done`, the printer shows up as **HP LaserJet 1018** in every app's
print dialog.

**7. Print a test page** from any app. The first job after switching the printer on takes
about 15 seconds longer: the orange light blinks and the rollers spin while the firmware
loads. That is normal.

## Print options

They are in the print dialog under "Printer Features", or can be passed with `lp -o`:

| Option | Values |
|---|---|
| `Resolution` | `600dpi` (default), `1200x600dpi` |
| `PageSize` | Letter, Legal, Executive, A4, A5, A6, B5, envelopes... |
| `HPDensity` | `1` to `5` (default 3) |
| `HPEconoMode` | `Off` / `On` (toner saving) |
| `HPFirmware` | `Auto` (default), `Always`, `Never` |

## Uninstall

```bash
sudo sh scripts/uninstall.sh
```

Removes the driver, the installed firmware and the print queue.

## Known issues and fixes

| Symptom | What to do |
|---|---|
| Rollers spin but no paper is picked up, and the queue stays at "sending data" | Switch the printer off for 10 s and on again. Do not query the printer (e.g. with `lpinfo -l -v`) while the firmware is booting: that hangs it. |
| The first page is slow | Normal: the firmware is loading after power-on. |
| `install.sh` says it did not find the 1018 | Connect it and switch it on, wait a few seconds and run it again. |
| `get-firmware.sh` cannot download the plugin | Check your connection. If the file is gone from OpenPrinting, download `hplip-3.24.4-plugin.run` yourself, type `sh scripts/get-firmware.sh ` and drag that file onto the Terminal window (it is verified the same way). |

To see what is going on in detail:

```bash
sudo cupsctl --debug-logging
```

Then look at `/var/log/cups/error_log`, and when you are done run
`sudo cupsctl --no-debug-logging`.

## Security

- The driver never goes online. Only `get-firmware.sh` downloads HP's official plugin over
  HTTPS, **without running anything from it**, and checks the firmware against a pinned
  SHA-256 hash.
- The filter validates every page before processing it, and only sends the printer a
  firmware file with the right format.
- `make check` runs the clang static analyzer, AddressSanitizer/UBSan tests, malformed
  inputs and shellcheck.
- To report a security issue, see [SECURITY.md](SECURITY.md).

## For developers

```bash
make check                                  # tests + security checks
make ARCHS="-arch arm64"                    # one architecture only (default: universal)
```

| Path | Contents |
|---|---|
| `src/rastertozjs.c` | CUPS filter |
| `src/third_party/` | JBIG-KIT and `zjs.h` (from foo2zjs) |
| `ppd/HP-LaserJet-1018.ppd` | Printer description for CUPS |
| `scripts/` | Firmware, install and uninstall |
| `test/` | Offline tests and security checks |
| `tools/zjsdecode.c` | foo2zjs decoder, tests only |
| `windows/` | Windows 10/11 guide and script |

## License and credits

**GPL-2.0-or-later** ([LICENSE](LICENSE)). Based on **foo2zjs** by Rick Richardson and
**JBIG-KIT** by Markus Kuhn; details in [THIRD_PARTY.md](THIRD_PARTY.md). HP's firmware is
**not** part of this project and is subject to the HP license you accept when downloading it.

Copyright (C) 2026 Gregorio Garcia.
