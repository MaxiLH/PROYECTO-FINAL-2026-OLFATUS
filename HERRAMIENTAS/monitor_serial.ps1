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
    powershell -ExecutionPolicy Bypass -File HERRAMIENTAS\monitor_serial.ps1 -Esp32

  -Esp32 : para la placa con ESP32. No toca DTR ni RTS al abrir:
           el ESP32 sigue andando sin reiniciarse, y si se aprieta
           su boton de reset arranca normal (con DTR activo
           arrancaria en modo de carga y no correria el programa).
           Guarda en logs\serial_esp32.log, aparte de las pruebas
           del Nano.
  -Reiniciar : con -Esp32, reinicia el ESP32 al abrir (pulso en
           RTS, como el IDE) para ver el cartel de arranque.
#>

param(
  [string]$Puerto = "COM3",
  [int]$Baudios = 115200,
  [switch]$Esp32,
  [switch]$Reiniciar
)

$carpetaLogs = Join-Path $PSScriptRoot "logs"
New-Item -ItemType Directory -Force $carpetaLogs | Out-Null

$archivoLog = Join-Path $carpetaLogs "serial.log"
if ($Esp32) { $archivoLog = Join-Path $carpetaLogs "serial_esp32.log" }
$archivoEnviar = Join-Path $carpetaLogs "enviar.txt"

function Escribir-Log([string]$texto) {
  $linea = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $texto
  Add-Content -Path $archivoLog -Value $linea -Encoding utf8
}

function Nuevo-Puerto {
  $sp = New-Object System.IO.Ports.SerialPort $Puerto, $Baudios, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
  if ($Esp32) {
    $sp.DtrEnable = $false
    $sp.RtsEnable = $false
  } else {
    $sp.DtrEnable = $true  # reinicia el Nano al abrir, igual que el IDE
  }
  return $sp
}

$puertoSerie = Nuevo-Puerto

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

# La fecha la usa log_a_excel.ps1 (las lineas solo tienen la hora)
Escribir-Log "===== Monitor abierto ($(Get-Date -Format 'yyyy-MM-dd')) en $Puerto a $Baudios baudios ====="
Write-Host "Monitor abierto en $Puerto a $Baudios baudios. ESC para salir." -ForegroundColor Green

# Reinicio del ESP32: RTS activo = EN en bajo (reset). Con DTR
# inactivo IO0 queda en alto y arranca el programa normal.
if ($Esp32 -and $Reiniciar) {
  $puertoSerie.RtsEnable = $true
  Start-Sleep -Milliseconds 150
  $puertoSerie.RtsEnable = $false
  Escribir-Log ">> ESP32 reiniciado desde el monitor"
  Write-Host ">> ESP32 reiniciado" -ForegroundColor Cyan
}
Write-Host "Log: $archivoLog" -ForegroundColor DarkGray

$buffer = ""

try {
  while ($true) {

    # Lo que manda la placa, linea por linea
    try {
      $datos = $puertoSerie.ReadExisting()
    } catch {
      # Se desconecto la placa (o se corto el USB): se espera a
      # que vuelva el puerto y se sigue escribiendo el mismo log
      Escribir-Log "===== Se perdio $Puerto, esperando que vuelva ====="
      Write-Host "Se perdio $Puerto (placa desconectada?). Esperando que vuelva... ESC para salir." -ForegroundColor Yellow
      try { $puertoSerie.Close() } catch { }

      $salir = $false
      while ($true) {
        Start-Sleep -Milliseconds 1000
        if ($hayTeclado -and [Console]::KeyAvailable -and [Console]::ReadKey($true).Key -eq [ConsoleKey]::Escape) { $salir = $true; break }
        try { $puertoSerie = Nuevo-Puerto; $puertoSerie.Open(); break } catch { }
      }
      if ($salir) { break }

      $buffer = ""
      Escribir-Log "===== $Puerto recuperado ====="
      Write-Host "$Puerto recuperado." -ForegroundColor Green
      continue
    }

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
