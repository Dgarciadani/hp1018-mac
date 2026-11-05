# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 Gregorio Garcia
<#
.SYNOPSIS
  Instala la HP LaserJet 1018 en Windows 10 con el driver oficial de HP.

.DESCRIPTION
  BETA: este script aun no se ha probado en un PC real con Windows 10.
  BETA: this script has not been tested on a real Windows 10 PC yet.

  1. Si el driver ya esta en el sistema (Windows Update), solo crea la impresora.
  2. Si no, usa el paquete descargado del Microsoft Update Catalog
     (archivo .cab o carpeta ya descomprimida) indicado con -DriverPath.

  Ejecutar en PowerShell como Administrador, con la impresora conectada
  por USB y encendida:

    Set-ExecutionPolicy -Scope Process Bypass
    .\install-hp1018.ps1
    .\install-hp1018.ps1 -DriverPath "$env:USERPROFILE\Downloads\driver.cab"

.PARAMETER DriverPath
  Archivo .cab o carpeta con el driver del Microsoft Update Catalog.

.PARAMETER PrinterName
  Nombre de la impresora en Windows (por defecto "HP LaserJet 1018").
#>
param(
    [string]$DriverPath,
    [string]$PrinterName = "HP LaserJet 1018"
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Abre PowerShell como Administrador y vuelve a ejecutar el script."
}

function Find-Driver {
    Get-PrinterDriver -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*LaserJet 1018*" } |
        Select-Object -First 1
}

# 1. Que Windows detecte el USB y busque el driver por su cuenta
Write-Host "Buscando la impresora en los puertos USB..."
pnputil /scan-devices | Out-Null
Start-Sleep -Seconds 5

$driver = Find-Driver

# 2. Driver del Microsoft Update Catalog
if (-not $driver) {
    if (-not $DriverPath) {
        Write-Host ""
        Write-Host "El driver no esta instalado. Descargalo asi:"
        Write-Host "  1. Abre https://www.catalog.update.microsoft.com"
        Write-Host "  2. Busca: HP LaserJet 1018"
        Write-Host "  3. Descarga la version para Windows 10 x64 (archivo .cab)"
        Write-Host "  4. Ejecuta: .\install-hp1018.ps1 -DriverPath <ruta al .cab>"
        exit 1
    }

    $src = Resolve-Path $DriverPath
    if ((Get-Item $src).PSIsContainer) {
        $dir = $src
    } else {
        $dir = Join-Path $env:TEMP "hp1018-driver"
        Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory $dir | Out-Null
        Write-Host "Descomprimiendo $src ..."
        expand.exe -F:* "$src" "$dir" | Out-Null
    }

    $infs = Get-ChildItem $dir -Recurse -Filter *.inf |
        Where-Object { Select-String -Path $_.FullName -Pattern "1018" -Quiet }
    if (-not $infs) {
        Write-Error "No se encontro ningun .inf para la 1018 en $dir"
    }
    foreach ($inf in $infs) {
        Write-Host "Agregando driver $($inf.Name) ..."
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
            Write-Host "No se pudo registrar '$n': $($_.Exception.Message)"
        }
    }
    $driver = Find-Driver
    if (-not $driver) {
        Write-Error "El driver no quedo registrado. Revisa la guia (README-es.md)."
    }
}
Write-Host "Driver: $($driver.Name)"

# 3. Crear la impresora en el puerto USB
$existing = Get-Printer -ErrorAction SilentlyContinue |
    Where-Object { $_.DriverName -eq $driver.Name }
if ($existing) {
    Write-Host "Ya existe la impresora '$($existing[0].Name)' con este driver."
    $PrinterName = $existing[0].Name
} else {
    $port = Get-PrinterPort | Where-Object { $_.Name -like "USB*" } |
        Select-Object -First 1
    if (-not $port) {
        Write-Error "No hay puerto USB de impresora. Conecta y enciende la 1018."
    }
    Add-Printer -Name $PrinterName -DriverName $driver.Name -PortName $port.Name
    Write-Host "Impresora '$PrinterName' creada en $($port.Name)"
}

# 4. Pagina de prueba
$wmiName = $PrinterName.Replace("\", "\\").Replace("'", "\'")
$p = Get-CimInstance Win32_Printer -Filter "Name='$wmiName'"
Invoke-CimMethod -InputObject $p -MethodName PrintTestPage | Out-Null
Write-Host "Pagina de prueba enviada."
