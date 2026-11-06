# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
<#
.SYNOPSIS
  Instala la HP LaserJet 1018 en Windows 10 y 11 con el driver oficial de HP.
  Installs the HP LaserJet 1018 on Windows 10 and 11 with HP's official driver.

.DESCRIPTION
  Probado en Windows 11 con una LaserJet 1018 real (tested on Windows 11).

  1. Si el driver ya esta en el sistema (Windows Update), solo crea la impresora.
     If the driver is already installed (Windows Update), it only adds the printer.
  2. Si no, usa el paquete del Microsoft Update Catalog indicado con -DriverPath
     (archivo .cab o carpeta ya descomprimida).
     Otherwise it uses the Microsoft Update Catalog package given with -DriverPath
     (.cab file or an already expanded folder).

  Ejecutar en PowerShell como Administrador, con la impresora conectada por USB y
  encendida. Run it in PowerShell as Administrator, printer on and connected by USB.
  Usa las rutas completas de tus archivos / use the full paths of your files:

    powershell -ExecutionPolicy Bypass -File "<...>\windows\install-hp1018.ps1"
    powershell -ExecutionPolicy Bypass -File "<...>\windows\install-hp1018.ps1" -DriverPath "<...>\<file>.cab"

  En el catalogo, el paquete correcto es / the right catalog package is
  "Hewlett-Packard - Printers - HP LaserJet 1018", 2012-09-17,
  "Windows 8.1 and later drivers" (Windows 10 y 11 / Windows 10 and 11).

.PARAMETER DriverPath
  Archivo .cab o carpeta con el driver del Microsoft Update Catalog.
  .cab file or folder with the Microsoft Update Catalog driver.

.PARAMETER PrinterName
  Nombre de la impresora en Windows / printer name (default "HP LaserJet 1018").
#>
param(
    [string]$DriverPath,
    [string]$PrinterName = "HP LaserJet 1018"
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Abre PowerShell como Administrador y vuelve a ejecutarlo. / Open PowerShell as Administrator and run it again."
}

function Find-Driver {
    Get-PrinterDriver -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*LaserJet 1018*" } |
        Select-Object -First 1
}

# 1. Que Windows detecte el USB y busque el driver por su cuenta
Write-Host "Buscando la impresora en USB... / Looking for the printer on USB..."
pnputil /scan-devices | Out-Null
Start-Sleep -Seconds 5

$driver = Find-Driver

# 2. Driver del Microsoft Update Catalog
if (-not $driver) {
    if (-not $DriverPath) {
        Write-Host ""
        Write-Host "El driver no esta instalado. / The driver is not installed."
        Write-Host "  1. https://www.catalog.update.microsoft.com"
        Write-Host "  2. Busca / search: HP LaserJet 1018"
        Write-Host "  3. Descarga / download 'Hewlett-Packard - Printers - HP LaserJet 1018', 2012-09-17"
        Write-Host "     ('Windows 8.1 and later drivers' = Windows 10 / 11)"
        Write-Host "  4. Ejecuta / run (cambia <...> por la ruta del .cab / replace <...> with the .cab path):"
        Write-Host "     powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -DriverPath `"<...>.cab`""
        exit 1
    }

    $src = Resolve-Path $DriverPath
    if ((Get-Item $src).PSIsContainer) {
        $dir = $src
    } else {
        $dir = Join-Path $env:TEMP "hp1018-driver"
        Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory $dir | Out-Null
        Write-Host "Descomprimiendo / expanding $src ..."
        expand.exe -F:* "$src" "$dir" | Out-Null
    }

    $infs = Get-ChildItem $dir -Recurse -Filter *.inf |
        Where-Object { Select-String -Path $_.FullName -Pattern "1018" -Quiet }
    if (-not $infs) {
        Write-Error "No hay un .inf para la 1018 en / no .inf for the 1018 in $dir"
    }
    foreach ($inf in $infs) {
        Write-Host "Agregando driver / adding driver $($inf.Name) ..."
        pnputil /add-driver "$($inf.FullName)" /install
    }

    # El nombre exacto del modelo viene del .inf
    $names = $infs | Select-String -Pattern '"([^"]*LaserJet 1018[^"]*)"' |
        ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
    foreach ($n in $names) {
        try {
            Add-PrinterDriver -Name $n
            break
        } catch {
            Write-Host "No se pudo registrar / could not register '$n': $($_.Exception.Message)"
        }
    }
    $driver = Find-Driver
    if (-not $driver) {
        Write-Error "El driver no quedo registrado / the driver was not registered. Ver / see README.md, README-es.md"
    }
}
Write-Host "Driver: $($driver.Name)"

# 3. Crear la impresora en el puerto USB
$existing = Get-Printer -ErrorAction SilentlyContinue |
    Where-Object { $_.DriverName -eq $driver.Name }
if ($existing) {
    Write-Host "Ya existe / already exists: '$($existing[0].Name)'"
    $PrinterName = $existing[0].Name
} else {
    $port = Get-PrinterPort | Where-Object { $_.Name -like "USB*" } |
        Select-Object -First 1
    if (-not $port) {
        Write-Error "No hay puerto USB de impresora / no USB printer port. Conecta y enciende la 1018 / connect and power on the 1018."
    }
    Add-Printer -Name $PrinterName -DriverName $driver.Name -PortName $port.Name
    Write-Host "Impresora creada / printer added: '$PrinterName' ($($port.Name))"
}

# 4. Pagina de prueba
$wmiName = $PrinterName.Replace("\", "\\").Replace("'", "\'")
$p = Get-CimInstance Win32_Printer -Filter "Name='$wmiName'"
Invoke-CimMethod -InputObject $p -MethodName PrintTestPage | Out-Null
Write-Host "Pagina de prueba enviada / test page sent."
