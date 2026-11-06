# HP LaserJet 1018 en Windows 10 y 11

[English](README.md) · **Español**

> **Probado en Windows 11** con una HP LaserJet 1018 real, usando la opción 2 (driver del
> catálogo + script). El mismo paquete de driver sirve para Windows 10. Si lo pruebas,
> cuéntanos el resultado abriendo un *issue*.

En Windows 10 y 11 no hace falta un driver nuevo: HP publica el suyo a través de
**Windows Update** y del **Microsoft Update Catalog**. Lo que suele fallar es que
Windows no lo descargue solo. Esta guía cubre los dos caminos y un plan B.

> Escribir un driver de impresora para Windows desde cero exige el WDK y la firma
> de Microsoft, y no aporta nada mientras exista el driver oficial.

## Opción 1: Windows Update (lo más simple)

1. Conecta la impresora por USB y enciéndela.
2. **Configuración → Actualización y seguridad → Windows Update → Buscar actualizaciones**.
   Abre también *Ver actualizaciones opcionales → Actualizaciones de controladores*
   y marca la de HP si aparece.
   En **Windows 11**: **Configuración → Windows Update → Opciones avanzadas →
   Actualizaciones opcionales → Actualizaciones de controladores**.
3. Si no aparece: **Panel de control → Dispositivos e impresoras → Agregar una impresora →
   "La impresora que deseo no está en la lista" → Agregar una impresora local →
   puerto `USB001`**. En la lista de modelos pulsa **Windows Update** (tarda unos
   minutos) y elige **HP → HP LaserJet 1018**.

## Opción 2: Microsoft Update Catalog + script

1. Abre <https://www.catalog.update.microsoft.com> y busca `HP LaserJet 1018`.
2. Elige la entrada **"Hewlett-Packard - Printers - HP LaserJet 1018"** del **17/9/2012**
   (4,4 MB), que dice *"Windows 8.1 and later drivers"*. **"and later" incluye Windows 10 y 11**:
   no existe una entrada que diga "Windows 10". Un mismo `.cab` trae las versiones de 32 y
   64 bits. Pulsa **Download** y luego el enlace `.cab` que aparece (tendrá un nombre largo,
   como `20522205_dd3e17….cab`).
3. Descarga este proyecto en el PC (**Code → Download ZIP** en GitHub) y descomprímelo.
   El script está en la carpeta `windows`.
4. Abre **PowerShell como Administrador** (menú Inicio → escribe *PowerShell* → clic derecho →
   *Ejecutar como administrador*), entra a la carpeta `windows` y ejecuta **una sola línea**:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install-hp1018.ps1 -DriverPath "C:\Users\TU_USUARIO\Downloads\NOMBRE_DEL.cab"
   ```

   Cambia la ruta por la de tu `.cab` (puedes arrastrar el archivo a la ventana de PowerShell
   para que escriba la ruta). Esta forma permite ejecutar el script solo esta vez, sin cambiar
   la configuración de seguridad de Windows.

   El script descomprime el `.cab`, instala el driver con `pnputil`, crea la impresora en el
   puerto USB e imprime una página de prueba. Si el driver ya venía de Windows Update, quita
   la parte `-DriverPath "..."`.

### Alternativa sin script

1. Abre el `.cab` con doble clic (Windows lo muestra como una carpeta), selecciona todo y
   cópialo a una carpeta nueva, por ejemplo `C:\HP1018`.
2. **Panel de control → Dispositivos e impresoras → Agregar una impresora → "La impresora
   que deseo no está en la lista" → Agregar una impresora local → puerto `USB001`**.
3. Pulsa **Usar disco… → Examinar**, elige `C:\HP1018\HPLJ1020.INF` (sirve para la 1018,
   1020 y 1022) y luego el modelo **HP LaserJet 1018**.

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
