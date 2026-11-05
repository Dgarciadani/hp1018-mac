# HP LaserJet 1018 en Windows 10

[English](README.md) · **Español**

> **Beta:** esta guía y el script aún no se han probado en un PC real con Windows 10.
> Si los pruebas, cuéntanos el resultado abriendo un *issue*.

En Windows 10 no hace falta un driver nuevo: HP publica el suyo a través de
**Windows Update** y del **Microsoft Update Catalog**. Lo que suele fallar es que
Windows no lo descargue solo. Esta guía cubre los dos caminos y un plan B.

> Escribir un driver de impresora para Windows desde cero exige el WDK y la firma
> de Microsoft, y no aporta nada mientras exista el driver oficial.

## Opción 1: Windows Update (lo más simple)

1. Conecta la impresora por USB y enciéndela.
2. **Configuración → Actualización y seguridad → Windows Update → Buscar actualizaciones**.
   Abre también *Ver actualizaciones opcionales → Actualizaciones de controladores*
   y marca la de HP si aparece.
3. Si no aparece: **Panel de control → Dispositivos e impresoras → Agregar una impresora →
   "La impresora que deseo no está en la lista" → Agregar una impresora local →
   puerto `USB001`**. En la lista de modelos pulsa **Windows Update** (tarda unos
   minutos) y elige **HP → HP LaserJet 1018**.

## Opción 2: Microsoft Update Catalog + script

1. Abre <https://www.catalog.update.microsoft.com> y busca `HP LaserJet 1018`.
2. Descarga la versión para Windows 10 (x64 o x86, según tu equipo). Es un `.cab`.
3. Copia `install-hp1018.ps1` al PC, abre **PowerShell como Administrador** y ejecuta:

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\install-hp1018.ps1 -DriverPath "$env:USERPROFILE\Downloads\<archivo>.cab"
   ```

   El script descomprime el `.cab`, instala el `.inf` con `pnputil`, crea la
   impresora en el puerto USB e imprime una página de prueba. Si el driver ya
   venía de Windows Update, basta con `.\install-hp1018.ps1`.

## Problemas frecuentes

| Síntoma | Solución |
|---|---|
| La cola dice "Error" o no sale nada | Apaga la impresora 10 s y vuelve a encenderla: el driver le reenvía el firmware al detectarla. |
| Se instaló como "Dispositivo desconocido" o "HP LaserJet 1018" sin cola | Desconecta el USB, borra el dispositivo en el Administrador de dispositivos, vuelve a conectar y ejecuta el script. |
| Trabajos atascados | `net stop spooler`, borra `C:\Windows\System32\spool\PRINTERS\*`, `net start spooler`. |
| Después de una actualización grande de Windows dejó de imprimir | Quita la impresora y el driver (*Propiedades del servidor de impresión → Controladores*) y repite la opción 1 o 2. |
| Usas un hub USB | Conéctala directo al PC: la carga del firmware falla a menudo con hubs. |

## Plan B: imprimir desde Windows a través del Mac

Si la impresora vive conectada al Mac con el driver `rastertozjs`, puedes compartirla:

1. En el Mac:

   ```bash
   lpadmin -p HP_LaserJet_1018 -o printer-is-shared=true
   cupsctl --share-printers
   ```

2. En Windows: **Agregar impresora → Seleccionar una impresora compartida por nombre** →
   `http://<nombre-del-mac>.local:631/printers/HP_LaserJet_1018`
3. Como driver, elige **Microsoft → Microsoft IPP Class Driver**.

Límite importante: macOS ya no convierte PostScript, así que el Mac solo acepta
trabajos en **PDF o imágenes**. El "IPP Class Driver" de Windows envía PDF cuando la
impresora lo anuncia, que es el caso. Con un driver PostScript genérico **no**
funcionará.
