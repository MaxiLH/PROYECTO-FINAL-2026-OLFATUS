<#
  MONITOR SERIAL - OLFATUS PETROLEUM

  Reemplaza al Monitor Serie del Arduino IDE para que Claude
  pueda leer lo que imprime la placa.

  - Muestra en pantalla todo lo que manda la placa.
  - Lo guarda con hora en HERRAMIENTAS\logs\serial.log
    (ese archivo es el que lee Claude).
  - Cualquier tecla que se apriete se manda a la placa
    (sirve para el "Envie cualquier caracter").
  - Si aparece el archivo HERRAMIENTAS\logs\enviar.txt, su
    contenido se manda a la placa y el archivo se borra
    (asi Claude tambien puede mandar caracteres).
  - ESC cierra el monitor y libera el puerto.

  IMPORTANTE: el puerto lo puede usar un solo programa a la
  vez. Antes de subir un programa desde el IDE hay que cerrar
  este monitor (ESC), y el Monitor Serie del IDE tiene que
  estar cerrado mientras este corre.

  Uso:
    powershell -ExecutionPolicy Bypass -File HERRAMIENTAS\monitor_serial.ps1
    powershell -ExecutionPolicy Bypass -File HERRAMIENTAS\monitor_serial.ps1 -Puerto COM4
#>

param(
  [string]$Puerto = "COM3",
  [int]$Baudios = 115200
)

$carpetaLogs = Join-Path $PSScriptRoot "logs"
New-Item -ItemType Directory -Force $carpetaLogs | Out-Null

$archivoLog = Join-Path $carpetaLogs "serial.log"
$archivoEnviar = Join-Path $carpetaLogs "enviar.txt"

function Escribir-Log([string]$texto) {
  $linea = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $texto
  Add-Content -Path $archivoLog -Value $linea -Encoding utf8
}

$puertoSerie = New-Object System.IO.Ports.SerialPort $Puerto, $Baudios, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$puertoSerie.DtrEnable = $true  # reinicia el Nano al abrir, igual que el IDE

try {
  $puertoSerie.Open()
} catch {
  Write-Host "No se pudo abrir $Puerto. Cerra el Monitor Serie del IDE (o cualquier otro programa que lo use) y proba de nuevo." -ForegroundColor Red
  Write-Host "Detalle: $($_.Exception.Message)" -ForegroundColor DarkGray
  exit 1
}

# Si no hay teclado (por ejemplo, corriendo en segundo plano)
# se sigue funcionando, solo que sin mandar teclas.
$hayTeclado = $true
try { [void][Console]::KeyAvailable } catch { $hayTeclado = $false }

Escribir-Log "===== Monitor abierto en $Puerto a $Baudios baudios ====="
Write-Host "Monitor abierto en $Puerto a $Baudios baudios. ESC para salir." -ForegroundColor Green
Write-Host "Log: $archivoLog" -ForegroundColor DarkGray

$buffer = ""

try {
  while ($true) {

    # Lo que manda la placa, linea por linea
    $datos = $puertoSerie.ReadExisting()

    if ($datos) {
      $buffer += $datos
      $partes = $buffer -split "`n"

      for ($i = 0; $i -lt $partes.Length - 1; $i++) {
        $linea = $partes[$i].TrimEnd("`r")
        Write-Host $linea
        Escribir-Log $linea
      }

      $buffer = $partes[$partes.Length - 1]
    }

    # Teclas del usuario -> placa
    if ($hayTeclado -and [Console]::KeyAvailable) {
      $tecla = [Console]::ReadKey($true)

      if ($tecla.Key -eq [ConsoleKey]::Escape) { break }

      $puertoSerie.Write([string]$tecla.KeyChar)
      Escribir-Log ">> enviado por teclado: '$($tecla.KeyChar)'"
      Write-Host ">> enviado: '$($tecla.KeyChar)'" -ForegroundColor Cyan
    }

    # Caracteres que deja Claude en enviar.txt -> placa
    if (Test-Path $archivoEnviar) {
      $texto = Get-Content $archivoEnviar -Raw
      Remove-Item $archivoEnviar -Force

      if ($texto) {
        $texto = $texto.Trim()
        $puertoSerie.Write($texto)
        Escribir-Log ">> enviado por archivo: '$texto'"
        Write-Host ">> enviado: '$texto'" -ForegroundColor Cyan
      }
    }

    Start-Sleep -Milliseconds 30
  }
} finally {
  if ($buffer) { Escribir-Log $buffer }
  Escribir-Log "===== Monitor cerrado ====="
  $puertoSerie.Close()
  Write-Host "Puerto $Puerto liberado." -ForegroundColor Green
}
