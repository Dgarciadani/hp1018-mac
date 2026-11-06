# HP LaserJet 1018 on Windows 10

**English** · [Español](README-es.md)

> **Beta:** this guide and the script have not been tested on a real Windows 10 PC yet.
> If you try them, please report the result by opening an issue.

Windows 10 does not need a new driver: HP publishes its own through **Windows Update** and
the **Microsoft Update Catalog**. What usually fails is Windows not downloading it on its own.
This guide covers both routes and a plan B. HP's driver is proprietary, so it is not included
here: you download it from Microsoft.

## Option 1: Windows Update (simplest)

1. Connect the printer by USB and switch it on.
2. **Settings → Update & Security → Windows Update → Check for updates**. Also open
   *View optional updates → Driver updates* and tick the HP one if it appears.
3. If it does not: **Control Panel → Devices and Printers → Add a printer →
   "The printer that I want isn't listed" → Add a local printer → port `USB001`**. In the
   model list click **Windows Update** (takes a few minutes) and choose
   **HP → HP LaserJet 1018**.

## Option 2: Microsoft Update Catalog + script

1. Open <https://www.catalog.update.microsoft.com> and search for `HP LaserJet 1018`.
2. Pick the entry **"Hewlett-Packard - Printers - HP LaserJet 1018"** dated **9/17/2012**
   (4.4 MB), labelled *"Windows 8.1 and later drivers"*. **"and later" includes Windows 10 and
   11**: there is no entry that says "Windows 10". One `.cab` holds both the 32- and 64-bit
   versions. Click **Download** and then the `.cab` link that appears (it has a long name,
   like `20522205_dd3e17….cab`).
3. Download this project on the PC (**Code → Download ZIP** on GitHub) and unzip it. The
   script is in the `windows` folder.
4. Open **PowerShell as Administrator** (Start menu → type *PowerShell* → right-click →
   *Run as administrator*), go to the `windows` folder and run **a single line**:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install-hp1018.ps1 -DriverPath "C:\Users\YOUR_USER\Downloads\FILE_NAME.cab"
   ```

   Replace the path with your `.cab` (you can drag the file onto the PowerShell window to
   type its path). This runs the script just this once, without changing Windows' security
   settings.

   The script expands the `.cab`, installs the driver with `pnputil`, creates the printer on
   the USB port and prints a test page. If the driver already came from Windows Update, leave
   out the `-DriverPath "..."` part.

### Without the script

1. Double-click the `.cab` (Windows shows it as a folder), select everything and copy it to a
   new folder, for example `C:\HP1018`.
2. **Control Panel → Devices and Printers → Add a printer → "The printer that I want isn't
   listed" → Add a local printer → port `USB001`**.
3. Click **Have Disk… → Browse**, choose `C:\HP1018\HPLJ1020.INF` (it covers the 1018, 1020
   and 1022) and then the **HP LaserJet 1018** model.

## Common problems

| Symptom | Fix |
|---|---|
| Queue shows "Error" or nothing comes out | Switch the printer off for 10 s and on again: the driver resends the firmware when it detects it. |
| Installed as "Unknown device" or with no queue | Unplug USB, remove the device in Device Manager, plug it back in and run the script. |
| Stuck jobs | `net stop spooler`, delete `C:\Windows\System32\spool\PRINTERS\*`, `net start spooler`. |
| Stopped printing after a big Windows update | Remove the printer and its driver (*Print server properties → Drivers*) and repeat option 1 or 2. |
| Using a USB hub | Try connecting it directly to the PC. |

## Plan B: print from Windows through the Mac

If the printer lives on a Mac with the `rastertozjs` driver, you can share it:

1. On the Mac:

   ```bash
   lpadmin -p HP_LaserJet_1018 -o printer-is-shared=true
   cupsctl --share-printers
   ```

2. On Windows: **Add a printer → Select a shared printer by name** →
   `http://<mac-name>.local:631/printers/HP_LaserJet_1018`
3. As driver, choose **Microsoft → Microsoft IPP Class Driver**.

Important limit: macOS no longer converts PostScript, so the Mac only accepts jobs as **PDF or
images**. Windows' "IPP Class Driver" sends PDF when the printer advertises it, which is the
case here. A generic PostScript driver **will not** work.
