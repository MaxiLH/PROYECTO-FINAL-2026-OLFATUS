<#
  LOG SERIAL -> EXCEL - OLFATUS PETROLEUM

  Lee HERRAMIENTAS\logs\serial.log (lo que guarda monitor_serial.ps1)
  y arma un Excel con una fila por muestra, separando las pruebas
  por programa en hojas distintas:

    Diferenciador      -> DIFERENCIADOR_V7 (standalone)
    Programa completo  -> OLFATUS_PETROLEUM_V7
    Programa completo ESP32 -> OLFATUS_PETROLEUM_V7 en la placa con
                          ESP32 (sale de logs\serial_esp32.log, -LogEsp)
    Demo               -> OLFATUS_PETROLEUM_V7_DEMO

  El programa de cada prueba se reconoce por el cartel que imprime
  la placa al arrancar. El programa completo imprime tambien su
  version ("Version: 7.1 ..."; sin esa linea es la 7.0): entre las
  pruebas de una version y la siguiente se hace un CORTE con la
  lista de cambios.

  Las notas que no salen del log estan en
  DATOS DE PRUEBAS\NOTAS_PRUEBAS.psd1:
    Cortes     -> cambios de cada version (texto del corte)
    Filas      -> observacion de una prueba (columna Observaciones)
    Generales  -> hoja Observaciones
    NotasHoja  -> nota extra arriba de la hoja de un programa

  Usa el Excel instalado en la PC (no hace falta Python).

  Uso:
    powershell -ExecutionPolicy Bypass -File HERRAMIENTAS\log_a_excel.ps1 -Reales "Diferenciador=Diesel,Nafta,Diesel;Completo=Diesel,Nafta"

  -Reales  : combustible que realmente se puso en cada prueba, en
             orden y separado por comas, por programa
             (Diferenciador / Completo / Demo), separados con ";".
             "?" (o nada entre dos comas) = no se sabe.
             Opcional: se puede completar despues en las celdas
             amarillas. Sin "Programa=" se toma como Diferenciador.
  -Salida  : ruta del .xlsx (por defecto DATOS DE PRUEBAS\PRUEBAS_OLFATUS.xlsx)
  -Notas   : archivo de notas (por defecto DATOS DE PRUEBAS\NOTAS_PRUEBAS.psd1)
#>

param(
  [string]$Log = (Join-Path $PSScriptRoot "logs\serial.log"),
  [string]$LogEsp = (Join-Path $PSScriptRoot "logs\serial_esp32.log"),
  [string]$Salida = "",
  [string]$Reales = "",
  [string]$Notas = ""
)

$inv = [Globalization.CultureInfo]::InvariantCulture
function Num([string]$s) { return [double]::Parse($s, $inv) }

# PowerShell recuerda el tipo del primer valor que se escribe en
# una celda por COM y despues rechaza los de otro tipo (string vs
# numero). Por eso cada tipo se escribe en su propia linea.
function Set-Celda($hoja, $f, $c, $v) {
  if ($null -eq $v) { return }
  $celda = $hoja.Cells.Item($f, $c)
  if ($v -is [string]) { $celda.Value2 = [string]$v }
  elseif ($v -is [int]) { $celda.Value2 = [int]$v }
  else { $celda.Value2 = [double]$v }
}

# Numero de columna -> letra (1 = A, 27 = AA)
function Letra([int]$n) {
  $s = ""
  while ($n -gt 0) {
    $m = ($n - 1) % 26
    $s = [char](65 + $m) + $s
    $n = [int][math]::Floor(($n - 1) / 26)
  }
  return $s
}

# Alto de fila (puntos) para un texto con ajuste de linea en un
# ancho dado (en unidades de ancho de columna de Excel)
function Alto([string]$texto, [double]$ancho, [bool]$negrita) {
  $factor = 1.05
  if ($negrita) { $factor = 0.95 }
  $porLinea = [math]::Max(10, [math]::Floor($ancho * $factor))
  $lineas = 0
  foreach ($parrafo in ($texto -split "`n")) {
    $lineas += [math]::Max(1, [math]::Ceiling($parrafo.Length / $porLinea))
  }
  return [math]::Min(409, $lineas * 12.75 + 3)
}

$carpetaDatos = Join-Path (Split-Path $PSScriptRoot -Parent) "DATOS DE PRUEBAS"
New-Item -ItemType Directory -Force $carpetaDatos | Out-Null

if (-not $Salida) { $Salida = Join-Path $carpetaDatos "PRUEBAS_OLFATUS.xlsx" }
if (-not $Notas) { $Notas = Join-Path $carpetaDatos "NOTAS_PRUEBAS.psd1" }

$notasDatos = @{}
if (Test-Path $Notas) { $notasDatos = Import-PowerShellDataFile $Notas }

$cortes = @{}
if ($notasDatos.Cortes) { $cortes = $notasDatos.Cortes }
$filasObs = @{}
if ($notasDatos.Filas) { $filasObs = $notasDatos.Filas }
$notasHoja = @{}
if ($notasDatos.NotasHoja) { $notasHoja = $notasDatos.NotasHoja }
$generales = @()
if ($notasDatos.Generales) { $generales = @($notasDatos.Generales) }

#---------------------------------------------------------
# PROGRAMAS
#---------------------------------------------------------

$programas = [ordered]@{
  Diferenciador = @{
    Hoja = "Diferenciador V7"
    Titulo = "DIFERENCIADOR_V7 (Arduino Nano, standalone por monitor serie)"
    Nota = "Limpiezas de 1 minuto entre pruebas (manuales). Se arranca cada paso mandando un caracter por el monitor serie."
  }
  Completo = @{
    Hoja = "Programa completo"
    Titulo = "OLFATUS_PETROLEUM_V7 (Arduino Nano, programa completo: maquina de estados, boton y electrovalvula)"
    Nota = "Limpieza automatica: el BME tiene que quedar arriba de 220 kOhm durante 60 s (45 s en la version 7.1, 65 s desde la 7.3; ver columna Limpieza). La extraccion se arranca con el boton."
  }
  CompletoESP32 = @{
    Hoja = "Programa completo ESP32"
    Titulo = "OLFATUS_PETROLEUM_V7 en la placa con ESP32 (ESP32 DOIT DevKit V1, carpeta PROYECTO EN EL ESP32)"
    Nota = "Mismo programa que el Nano 7.3, con los pines del ESP32. Limpieza de 1 min 5 s. Las columnas 'param. actuales' usan los intervalos del ESP32 (hoja Parametros, segunda tabla)."
  }
  Demo = @{
    Hoja = "Demo"
    Titulo = "OLFATUS_PETROLEUM_V7_DEMO (muestras de ejemplo)"
    Nota = "NO son mediciones: la demo repite las muestras de ejemplo cargadas en el programa."
  }
}

#---------------------------------------------------------
# 1) LEER EL LOG
#---------------------------------------------------------

$pruebas = New-Object System.Collections.ArrayList

# El log del Nano y, si existe, el del ESP32
$logs = @(@{ Ruta = $Log; Esp = $false })
if ($LogEsp -and (Test-Path $LogEsp)) { $logs += @{ Ruta = $LogEsp; Esp = $true } }

foreach ($archivo in $logs) {
$p = $null
$esperaResultado = $false
$enResultado = $false

# Los logs viejos no tienen cartel ni fecha: eran del diferenciador
# y la fecha es la del dia que se creo el log.
$programa = "Diferenciador"
if ($archivo.Esp) { $programa = "CompletoESP32" }
$version = ""
$fecha = (Get-Item $archivo.Ruta).CreationTime.Date
$inicioLimpieza = $null
$limpiezaSeg = $null
$inicioEspera = $null

foreach ($linea in (Get-Content $archivo.Ruta -Encoding UTF8)) {
  if ($linea -notmatch '^\[(\d\d):(\d\d):(\d\d)\.(\d+)\] ?(.*)$') { continue }

  $hora = "{0}:{1}:{2}" -f $Matches[1], $Matches[2], $Matches[3]
  $momento = $fecha.AddHours([int]$Matches[1]).AddMinutes([int]$Matches[2]).AddSeconds([int]$Matches[3]).AddMilliseconds([int]$Matches[4])
  $t = $Matches[5].Trim()

  # Fecha (el monitor la escribe al abrirse)
  if ($t -match '^===== Monitor abierto \((\d{4}-\d\d-\d\d)\)') {
    $fecha = [datetime]::ParseExact($Matches[1], "yyyy-MM-dd", $inv)
    continue
  }

  # Cartel de arranque -> que programa (y que version) esta cargado
  if ($t -eq 'Clasificador Diesel / Nafta') { $programa = "Diferenciador"; $version = ""; continue }
  if ($t -eq 'Maquina de estados central') {
    $programa = "Completo"
    if ($archivo.Esp) { $programa = "CompletoESP32" }
    $version = "7.0"
    continue
  }
  if ($t -eq '*** MODO DEMO ***') { $programa = "Demo"; $version = ""; continue }
  if ($t -match '^Version: ([\d.]+)') { $version = $Matches[1]; continue }

  # Duracion de la limpieza (programa completo)
  if ($t -eq '>> Estado: LIMPIEZA_TRAMPA') { $inicioLimpieza = $momento; $limpiezaSeg = $null; continue }
  if ($t -like 'Limpieza confirmada*' -and $inicioLimpieza) {
    $limpiezaSeg = [math]::Round(($momento - $inicioLimpieza).TotalSeconds, 1)
    continue
  }

  # Desde que termino la referencia hasta que se apreto el boton
  if ($t -like '>> Estado: EXTRACCION_LISTA_PARA_HACER*') { $inicioEspera = $momento; continue }

  # Comienzo de una captura
  $esInicio = ($t -match '^Capturando') -or ($t -eq '>> Estado: EXTRAYENDO') -or ($t -eq 'EXTRAYENDO')

  if ($esInicio) {
    # "EXTRAYENDO" sin ">>" se repetia en cada vuelta del loop en
    # versiones viejas: solo cuenta la primera.
    if ($null -eq $p -or $p.Contains("Resultado")) {
      $espera = $null
      if ($inicioEspera) { $espera = [math]::Round(($momento - $inicioEspera).TotalSeconds) }

      $p = [ordered]@{ Programa = $programa; Version = $version; Fecha = $fecha.ToString("dd/MM/yyyy"); Hora = $hora; Limpieza = $limpiezaSeg; Espera = $espera; Detalle = @() }
      [void]$pruebas.Add($p)
      $limpiezaSeg = $null
      $inicioEspera = $null
    }
    continue
  }

  if ($null -eq $p) { continue }

  if ($esperaResultado -and $t) {
    $p.Resultado = $t
    $esperaResultado = $false
    $enResultado = $true
    continue
  }

  if ($enResultado) {
    if ($t -like '====*') { $enResultado = $false }
    elseif ($t) { $p.Detalle += $t }
    continue
  }

  switch -Regex ($t) {
    '^Captura terminada en (\d+) ms' { $p.Duracion = [int]$Matches[1] }
    '^Muestras = \d+\s+en (\d+) ms' { if (-not $p.Contains("Duracion")) { $p.Duracion = [int]$Matches[1] } }
    '^RESULTADO$' { $esperaResultado = $true }
    '^Ref\s+135/138/BME = ([\d.-]+) / ([\d.-]+) / ([\d.-]+)' {
      $p.Ref135 = Num $Matches[1]; $p.Ref138 = Num $Matches[2]; $p.RefBME = Num $Matches[3]
    }
    '^Max\s+135/138\s+= ([\d.-]+) / ([\d.-]+)' {
      $p.Max135 = Num $Matches[1]; $p.Max138 = Num $Matches[2]
    }
    '^Var138 = ([\d.-]+)\s+Firma = ([\d.-]+)' {
      $p.Var138 = Num $Matches[1]; $p.Firma = Num $Matches[2]
    }
    '^Muestra detectada por: (.*)$' { $p.MuestraPor = $Matches[1].Trim() }
    '^(Relacion|VarBME|Area135|Var135|Pend135)\s*= ([\d.-]+)\s+-> (.*)$' {
      $p["V_" + $Matches[1]] = Num $Matches[2]
      $p["Z_" + $Matches[1]] = $Matches[3].Trim()
    }
    '^(DUDA DEBAJO|DIESEL|DUDA MEDIO|NAFTA|DUDA ENCIMA)\s*: (\d+)' {
      $p["P_" + $Matches[1]] = [int]$Matches[2]
    }
    '^Posicion global .*= ([\d.-]+)' { $p.Posicion = Num $Matches[1] }
    '^Confianza = (\d+) %' { $p.Confianza = (Num $Matches[1]) / 100.0 }
  }
}
}

# Solo las pruebas que llegaron al diagnostico
$pruebas = @($pruebas | Where-Object { $_.Contains("Resultado") })

if ($pruebas.Count -eq 0) {
  Write-Host "No se encontraron muestras en $Log" -ForegroundColor Red
  exit 1
}

# Combustibles reales por programa
$listaReales = @{}
foreach ($segmento in ($Reales -split ';')) {
  if (-not $segmento.Trim()) { continue }

  if ($segmento -match '^\s*([^=]+)=(.*)$') { $clave = $Matches[1].Trim(); $valores = $Matches[2] }
  else { $clave = "Diferenciador"; $valores = $segmento }

  # Un lugar vacio o "?" = no se sabe (la celda queda en blanco y
  # las demas no se corren de lugar)
  $listaReales[$clave] = @($valores.Split(",") | ForEach-Object { $_.Trim() })
}

#---------------------------------------------------------
# 2) ARMAR EL EXCEL
#---------------------------------------------------------

$xl = New-Object -ComObject Excel.Application
$xl.Visible = $false
$xl.DisplayAlerts = $false

# Excel toma los formatos de numero con el idioma de Windows: en
# espanol "0.000" usa el punto como separador de miles y 1.133 se
# veia como 0.001. Se arma el formato con el separador decimal que
# usa este Excel.
$separadorDecimal = $xl.International(3)
function Fmt([int]$decimales) { return "0" + $separadorDecimal + ("0" * $decimales) }

$amarillo = 0x99FFFF   # BGR: amarillo claro
$gris = 0xE0E0E0
$azulTexto = 0xFF0000  # BGR: azul
$naranja = 0x84B0F4    # BGR: naranja (titulo del corte)
$naranjaClaro = 0xD6E4FC

$wb = $xl.Workbooks.Add()
while ($wb.Worksheets.Count -gt 1) { $wb.Worksheets.Item($wb.Worksheets.Count).Delete() }

#--------------------------
# Hoja Parametros (primero, porque las otras hojas la usan en sus formulas)
#--------------------------

$wp = $wb.Worksheets.Item(1)
$wp.Name = "Parámetros"

$wp.Cells.Font.Name = "Arial"
$wp.Cells.Font.Size = 10
Set-Celda $wp 1 1 "Intervalos y pesos actuales (DIFERENCIADOR_V7 y OLFATUS_PETROLEUM_V7 version 7.3 del Nano, iguales desde el 05/11/2026)"
$wp.Cells.Item(1, 1).Font.Bold = $true
$wp.Cells.Item(1, 1).Font.Size = 12
Set-Celda $wp 2 1 "Copiados del programa. Si se cambian aca se recalculan las columnas 'param. actuales' de las otras hojas, pero NO cambian en el Arduino."

$encP = "Variable", "Diesel min", "Diesel max", "Nafta min", "Nafta max", "Peso", "Nota"
for ($c = 0; $c -lt $encP.Count; $c++) { Set-Celda $wp 4 ($c + 1) $encP[$c] }
$wp.Range("A4:G4").Font.Bold = $true
$wp.Range("A4:G4").Interior.Color = $gris

$parametros = @(
  @("Relacion", 0.6, 1.10, 1.10, 10.0, 3, "(max138 + max135) / (ref138 + ref135). Factor de subida, 1.00 = sin cambio. 05/11/2026 (version 7.3): techo del diesel y piso de la nafta 1.23 / 1.24 -> 1.10 (naftas seguidas contaminan la referencia)."),
  @("VarBME", -10.0, 60.0, 61.0, 105.0, 2, "%. -999 = BME no disponible. 05/11/2026: techo del diesel 40 -> 60 y piso de la nafta 41 -> 61 (ver hoja Observaciones)."),
  @("Area135", 1.40, 2.90, 3.00, 7.60, 1, "Aproximado con las pruebas del 03/11/2026 (diesel 2.56 / 2.76, nafta 3.18). Antes: 1.40 / 3.3 / 3.40 / 7.60."),
  @("Var135", -5.0, 8.0, 8.5, 950.0, 1, "%"),
  @("Pend135", 0.0, 0.42, 0.43, 1.90, 0, "Peso 0: no vota.")
)

for ($i = 0; $i -lt $parametros.Count; $i++) {
  $fila = 5 + $i
  for ($c = 0; $c -lt 7; $c++) { Set-Celda $wp $fila ($c + 1) $parametros[$i][$c] }
  $wp.Range("B${fila}:F${fila}").Font.Color = $azulTexto
}

$wp.Range("B5:E9").NumberFormatLocal = (Fmt 2)

# Umbral de confianza
$filaUmbral = 11
Set-Celda $wp $filaUmbral 1 "Umbral confianza"
Set-Celda $wp $filaUmbral 2 0.80
Set-Celda $wp $filaUmbral 7 "La zona ganadora tiene que juntar al menos este porcentaje del peso total. Si no llega, el resultado es DUDA MEDIO."
$wp.Cells.Item($filaUmbral, 1).Font.Bold = $true
$wp.Cells.Item($filaUmbral, 2).NumberFormat = "0%"
$wp.Cells.Item($filaUmbral, 2).Font.Color = $azulTexto

$wp.Columns.Item(1).ColumnWidth = 16
for ($c = 2; $c -le 6; $c++) { $wp.Columns.Item($c).ColumnWidth = 11 }
$wp.Columns.Item(7).ColumnWidth = 100

# Fila de Parametros de cada variable
$filaParam = @{ Relacion = 5; VarBME = 6; Area135 = 7; Var135 = 8; Pend135 = 9 }

# Segunda tabla: intervalos del ESP32 (hoja Programa completo ESP32)
Set-Celda $wp 13 1 "Intervalos del ESP32 (OLFATUS_PETROLEUM_V7 de la carpeta PROYECTO EN EL ESP32, version 7.3.2). Los usa la hoja 'Programa completo ESP32'. Mismo umbral de confianza."
$wp.Cells.Item(13, 1).Font.Bold = $true
$wp.Cells.Item(13, 1).Font.Size = 12
for ($c = 0; $c -lt $encP.Count; $c++) { Set-Celda $wp 14 ($c + 1) $encP[$c] }
$wp.Range("A14:G14").Font.Bold = $true
$wp.Range("A14:G14").Interior.Color = $gris

$parametrosEsp = @(
  @("Relacion", 0.6, 1.10, 1.10, 10.0, 3, "Igual que el Nano 7.3."),
  @("VarBME", -10.0, 60.0, 61.0, 105.0, 2, "Igual que el Nano 7.3."),
  @("Area135", 0.65, 1.33, 1.34, 3.50, 1, "12/11/2026 (ESP32 7.3.2): con las 10 pruebas de la placa modificada (diesel 1,129-1,324, nafta 1,251-1,400). Con los del Nano (1,40 / 2,90 / 3,00 / 7,60) todo votaba DUDA DEBAJO. Ver hoja Observaciones."),
  @("Var135", -5.0, 8.0, 8.5, 950.0, 1, "Igual que el Nano 7.3."),
  @("Pend135", 0.0, 0.42, 0.43, 1.90, 0, "Igual que el Nano 7.3. Peso 0: no vota.")
)

for ($i = 0; $i -lt $parametrosEsp.Count; $i++) {
  $fila = 15 + $i
  for ($c = 0; $c -lt 7; $c++) { Set-Celda $wp $fila ($c + 1) $parametrosEsp[$i][$c] }
  $wp.Range("B${fila}:F${fila}").Font.Color = $azulTexto
}
$wp.Range("B15:E19").NumberFormatLocal = (Fmt 2)

$filaParamEsp = @{ Relacion = 15; VarBME = 16; Area135 = 17; Var135 = 18; Pend135 = 19 }

#--------------------------
# Columnas de las hojas de muestras: clave, titulo, formato
#--------------------------

$columnas = @(
  @("N", "N", ""),
  @("Fecha", "Fecha", ""),
  @("Hora", "Hora", ""),
  @("Real", "Combustible real", ""),
  @("Resultado", "Resultado placa", ""),
  @("Acerto", "Acerto?", ""),
  @("Obs", "Observaciones", ""),
  @("Limpieza", "Limpieza (s)", "1"),
  @("Espera", "Espera ref. -> boton (s)", ""),
  @("Duracion", "Captura (ms)", ""),
  @("Ref135", "Ref MQ135 (V)", "3"),
  @("Ref138", "Ref MQ138 (V)", "3"),
  @("RefBME", "Ref BME (kOhm)", "1"),
  @("Max135", "Max MQ135 (V)", "3"),
  @("Max138", "Max MQ138 (V)", "3"),
  @("MaxRef138", "Max/Ref 138", "3"),
  @("MaxRef135", "Max/Ref 135", "3"),
  @("Var138", "Var138 (%)", "1"),
  @("Firma", "Firma", "2"),
  @("MuestraPor", "Muestra detectada por", ""),
  @("V_Relacion", "Relacion", "3"),
  @("Z_Relacion", "Zona Relacion (placa)", ""),
  @("V_VarBME", "VarBME (%)", "1"),
  @("Z_VarBME", "Zona VarBME (placa)", ""),
  @("Tope655", "BME en el tope de 655 kOhm", ""),
  @("V_Area135", "Area135", "3"),
  @("Z_Area135", "Zona Area135 (placa)", ""),
  @("V_Var135", "Var135 (%)", "1"),
  @("Z_Var135", "Zona Var135 (placa)", ""),
  @("V_Pend135", "Pend135", "3"),
  @("Z_Pend135", "Zona Pend135 (placa)", ""),
  @("P_DUDA DEBAJO", "Peso DUDA DEBAJO", ""),
  @("P_DIESEL", "Peso DIESEL", ""),
  @("P_DUDA MEDIO", "Peso DUDA MEDIO", ""),
  @("P_NAFTA", "Peso NAFTA", ""),
  @("P_DUDA ENCIMA", "Peso DUDA ENCIMA", ""),
  @("Posicion", "Posicion global", "2"),
  @("Confianza", "Confianza", "pct"),
  @("Detalle", "Detalle / avisos", ""),
  @("N_Relacion", "Zona Relacion (param. actuales)", ""),
  @("N_VarBME", "Zona VarBME (param. actuales)", ""),
  @("N_Area135", "Zona Area135 (param. actuales)", ""),
  @("N_Var135", "Zona Var135 (param. actuales)", ""),
  @("N_Pend135", "Zona Pend135 (param. actuales)", ""),
  @("ResN", "Resultado (param. actuales)", ""),
  @("AcertoN", "Acerto? (param. actuales)", "")
)

$col = @{}
for ($i = 0; $i -lt $columnas.Count; $i++) { $col[$columnas[$i][0]] = $i + 1 }
function L([string]$clave) { return Letra $col[$clave] }

$variables = "Relacion", "VarBME", "Area135", "Var135", "Pend135"
$coloresZona = @{ "DIESEL" = 0x66E6FF; "NAFTA" = 0xF7D9BD; "DUDA DEBAJO" = 0xD9D9D9; "DUDA MEDIO" = 0xD9D9D9; "DUDA ENCIMA" = 0xD9D9D9; "DUDA POR DEBAJO" = 0xD9D9D9; "DUDA POR ENCIMA" = 0xD9D9D9 }

$pr = "'Parámetros'!"
$umbral = "$pr`$B`$$filaUmbral"

#--------------------------
# Una hoja por programa
#--------------------------

$errores = 0
$resumen = @()

foreach ($claveProg in $programas.Keys) {
  $lista = @($pruebas | Where-Object { $_.Programa -eq $claveProg })
  if ($lista.Count -eq 0) { continue }

  $info = $programas[$claveProg]
  $realesHoja = @()
  if ($listaReales.ContainsKey($claveProg)) { $realesHoja = $listaReales[$claveProg] }

  $cortesProg = @{}
  if ($cortes.ContainsKey($claveProg)) { $cortesProg = $cortes[$claveProg] }

  # Intervalos con que se recalcula esta hoja
  $fp = $filaParam
  if ($claveProg -eq "CompletoESP32") { $fp = $filaParamEsp }

  $ws = $wb.Worksheets.Add([Type]::Missing, $wb.Worksheets.Item($wb.Worksheets.Count))
  $ws.Name = $info.Hoja
  $ws.Cells.Font.Name = "Arial"
  $ws.Cells.Font.Size = 10

  Set-Celda $ws 1 1 ("OLFATUS PETROLEUM - Pruebas - " + $info.Hoja)
  $ws.Cells.Item(1, 1).Font.Bold = $true
  $ws.Cells.Item(1, 1).Font.Size = 14
  Set-Celda $ws 2 1 ("PROGRAMA: " + $info.Titulo)
  $ws.Cells.Item(2, 1).Font.Bold = $true
  Set-Celda $ws 3 1 ("NOTA: " + $info.Nota)
  $ws.Cells.Item(3, 1).Font.Bold = $true
  Set-Celda $ws 4 1 "Resultado, valores y zonas 'placa' = lo que imprimio el Arduino con el programa cargado en ese momento. Columnas 'param. actuales' = zonas y resultado recalculados con la hoja Parametros."
  Set-Celda $ws 5 1 "Celdas amarillas = combustible que realmente se puso (cargado a mano, corregir si hace falta). Observaciones largas: hoja Observaciones."
  if ($notasHoja.ContainsKey($claveProg)) {
    Set-Celda $ws 6 1 $notasHoja[$claveProg]
    $ws.Cells.Item(6, 1).Font.Bold = $true
    $ws.Cells.Item(6, 1).Font.Color = 0x0000C0
  }

  $filaEnc = 7
  for ($c = 0; $c -lt $columnas.Count; $c++) { Set-Celda $ws $filaEnc ($c + 1) $columnas[$c][1] }

  $ultimaCol = $columnas.Count
  $colFinCorte = $col["Resultado"]  # el texto del corte va en las columnas fijas (N .. Resultado)
  $rangoEnc = $ws.Range($ws.Cells.Item($filaEnc, 1), $ws.Cells.Item($filaEnc, $ultimaCol))
  $rangoEnc.Font.Bold = $true
  $rangoEnc.Interior.Color = $gris
  $rangoEnc.WrapText = $true
  $rangoEnc.VerticalAlignment = -4108  # centro

  [void]$ws.Cells.Item($filaEnc, $col["Hora"]).AddComment($info.Nota)

  # Fecha y hora como texto: si no, Excel puede leer 03/11 como 11 de marzo
  $ws.Columns.Item($col["Fecha"]).NumberFormat = "@"
  $ws.Columns.Item($col["Hora"]).NumberFormat = "@"

  # Versiones en orden: las de las pruebas y despues las que tienen
  # corte cargado pero todavia no tienen pruebas (asi el corte queda
  # hecho para la proxima sesion)
  $versiones = New-Object System.Collections.ArrayList
  foreach ($m in $lista) { if (-not $versiones.Contains($m.Version)) { [void]$versiones.Add($m.Version) } }
  foreach ($v in ($cortesProg.Keys | Sort-Object)) { if (-not $versiones.Contains($v)) { [void]$versiones.Add($v) } }

  $r = $filaEnc + 1
  $primeraFila = $r
  $n = 0
  $bloques = @()
  $filasPruebas = @()
  $filasCorte = @()   # @(fila, texto, negrita) para calcular el alto despues

  for ($iv = 0; $iv -lt $versiones.Count; $iv++) {
    $v = $versiones[$iv]

    #----- Corte entre versiones -----
    if ($iv -gt 0) {
      $titulo = "CORTE - Version $v del programa (sin descripcion de cambios en NOTAS_PRUEBAS.psd1)"
      $cambios = @()
      if ($cortesProg.ContainsKey($v)) {
        $titulo = $cortesProg[$v].Titulo
        $cambios = @($cortesProg[$v].Cambios)
      }

      # (ArrayList: con += PowerShell desarma los arrays de un solo elemento)
      $filasTexto = New-Object System.Collections.ArrayList
      [void]$filasTexto.Add(@($titulo, $true, $naranja))
      foreach ($cb in $cambios) { [void]$filasTexto.Add(@(("- " + $cb), $false, $naranjaClaro)) }

      foreach ($ft in $filasTexto) {
        $ws.Range($ws.Cells.Item($r, 1), $ws.Cells.Item($r, $ultimaCol)).Interior.Color = $ft[2]
        $celdas = $ws.Range($ws.Cells.Item($r, 1), $ws.Cells.Item($r, $colFinCorte))
        $celdas.Merge()
        $celdas.WrapText = $true
        $celdas.VerticalAlignment = -4160  # arriba
        Set-Celda $ws $r 1 $ft[0]
        $ws.Cells.Item($r, 1).Font.Bold = $ft[1]
        $filasCorte += , @($r, $ft[0], $ft[1])
        $r++
      }

      # El mismo texto como comentario de la celda del corte
      $filaTitulo = $r - $filasTexto.Count
      $textoComentario = $titulo + "`n" + (($cambios | ForEach-Object { "- " + $_ }) -join "`n")
      $comentario = $ws.Cells.Item($filaTitulo, 1).AddComment($textoComentario)
      $comentario.Shape.Width = 430
      $comentario.Shape.Height = [math]::Min(400, (Alto $textoComentario 75 $false) + 10)
    }

    #----- Pruebas de esta version -----
    $desde = $r

    foreach ($m in ($lista | Where-Object { $_.Version -eq $v })) {
      $n++
      Set-Celda $ws $r $col["N"] $n

      foreach ($clave in "Fecha", "Hora", "Resultado", "Limpieza", "Espera", "Duracion", "Ref135", "Ref138", "RefBME", "Max135", "Max138", "Var138", "Firma", "MuestraPor", "Posicion", "Confianza") {
        if ($m.Contains($clave)) { Set-Celda $ws $r $col[$clave] $m[$clave] }
      }

      foreach ($vr in $variables) {
        if ($m.Contains("V_" + $vr)) { Set-Celda $ws $r $col["V_" + $vr] $m["V_" + $vr] }
        if ($m.Contains("Z_" + $vr)) { Set-Celda $ws $r $col["Z_" + $vr] $m["Z_" + $vr] }
      }

      foreach ($z in "DUDA DEBAJO", "DIESEL", "DUDA MEDIO", "NAFTA", "DUDA ENCIMA") {
        if ($m.Contains("P_" + $z)) { Set-Celda $ws $r $col["P_" + $z] $m["P_" + $z] }
      }

      Set-Celda $ws $r $col["Detalle"] ($m.Detalle -join " ")

      # Hasta la version 7.0 (y en el diferenciador hasta el 05/11) el BME
      # se guardaba con tope en 655 kOhm. Si el BME no bajo de 655 en la
      # captura, VarBME = (Ref - 655) / Ref, sin importar la muestra.
      if ($claveProg -ne "Demo" -and ($m.Version -eq "" -or $m.Version -eq "7.0") -and $m.Contains("RefBME") -and $m.Contains("V_VarBME") -and $m.RefBME -gt 0) {
        $tope = ($m.RefBME - 655.0) / $m.RefBME * 100.0
        $esTope = "NO"
        if ([math]::Abs($m.V_VarBME - $tope) -lt 0.02) { $esTope = "SI" }
        Set-Celda $ws $r $col["Tope655"] $esTope
      }

      $claveObs = "{0}|{1}|{2}" -f $m.Programa, $m.Fecha, $m.Hora
      if ($filasObs.ContainsKey($claveObs)) { Set-Celda $ws $r $col["Obs"] $filasObs[$claveObs] }

      if ($n -le $realesHoja.Count -and $realesHoja[$n - 1] -and $realesHoja[$n - 1] -ne "?") {
        Set-Celda $ws $r $col["Real"] $realesHoja[$n - 1].ToUpper()
      }
      $ws.Cells.Item($r, $col["Real"]).Interior.Color = $amarillo

      # Formulas
      $real = (L "Real") + $r
      $res = (L "Resultado") + $r
      $ws.Cells.Item($r, $col["Acerto"]).Formula = "=IF($real=`"`",`"`",IF(UPPER($real)=UPPER($res),`"SI`",`"NO`"))"

      $r138 = (L "Ref138") + $r; $m138 = (L "Max138") + $r
      $r135 = (L "Ref135") + $r; $m135 = (L "Max135") + $r
      $ws.Cells.Item($r, $col["MaxRef138"]).Formula = "=IF($r138>0.05,$m138/$r138,`"`")"
      $ws.Cells.Item($r, $col["MaxRef135"]).Formula = "=IF($r135>0.01,$m135/$r135,`"`")"

      # Zonas recalculadas con la hoja Parametros
      foreach ($vr in $variables) {
        $x = (L ("V_" + $vr)) + $r
        $k = $fp[$vr]
        $formula = "=IF($x=`"`",`"`",IF($x<$pr`$B`$$k,`"DUDA DEBAJO`",IF($x<=$pr`$C`$$k,`"DIESEL`",IF($x<$pr`$D`$$k,`"DUDA MEDIO`",IF($x<=$pr`$E`$$k,`"NAFTA`",`"DUDA ENCIMA`")))))"
        if ($vr -eq "VarBME") { $formula = "=IF($x=-999,`"no disponible`"," + $formula.Substring(1) + ")" }
        $ws.Cells.Item($r, $col["N_" + $vr]).Formula = $formula
      }

      # Resultado recalculado: igual que el Arduino, gana la zona que
      # junta al menos el umbral del peso total; si ninguna llega, DUDA MEDIO
      $terminosTotal = @()
      foreach ($vr in $variables) {
        $z = (L ("N_" + $vr)) + $r
        $w = "$pr`$F`$" + $fp[$vr]
        if ($vr -eq "VarBME") { $terminosTotal += "AND($z<>`"`",$z<>`"no disponible`")*$w" }
        else { $terminosTotal += "($z<>`"`")*$w" }
      }
      $total = "(" + ($terminosTotal -join "+") + ")"

      $peso = @{}
      foreach ($zona in "DIESEL", "NAFTA", "DUDA DEBAJO", "DUDA ENCIMA") {
        $terminos = @()
        foreach ($vr in $variables) { $terminos += "($((L ("N_" + $vr)) + $r)=`"$zona`")*$pr`$F`$$($fp[$vr])" }
        $peso[$zona] = "(" + ($terminos -join "+") + ")"
      }

      $ws.Cells.Item($r, $col["ResN"]).Formula = "=IF($total=0,`"`",IF($($peso['DIESEL'])>=$umbral*$total,`"DIESEL`",IF($($peso['NAFTA'])>=$umbral*$total,`"NAFTA`",IF($($peso['DUDA DEBAJO'])>=$umbral*$total,`"DUDA POR DEBAJO`",IF($($peso['DUDA ENCIMA'])>=$umbral*$total,`"DUDA POR ENCIMA`",`"DUDA MEDIO`")))))"

      $resN = (L "ResN") + $r
      $ws.Cells.Item($r, $col["AcertoN"]).Formula = "=IF($real=`"`",`"`",IF(UPPER($real)=UPPER($resN),`"SI`",`"NO`"))"

      $filasPruebas += $r
      $r++
    }

    if ($r -gt $desde) { $bloques += , @($v, $desde, ($r - 1)) }
  }

  $ultimaFila = $r - 1

  # Resumen: aciertos por version
  $rRes = $ultimaFila + 2
  $a = L "Acerto"; $aN = L "AcertoN"; $re = L "Real"

  foreach ($b in $bloques) {
    $etiqueta = "Aciertos"
    if ($b[0]) { $etiqueta = "Aciertos version " + $b[0] }
    $d = $b[1]; $h = $b[2]

    Set-Celda $ws $rRes 1 $etiqueta
    $ws.Cells.Item($rRes, 1).Font.Bold = $true
    $ws.Cells.Item($rRes, $col["Real"]).Formula = "=COUNTIF(${a}${d}:${a}${h},`"SI`")&`" de `"&COUNTA(${re}${d}:${re}${h})"
    $ws.Cells.Item($rRes, $col["Obs"]).Formula = "=`"Con los parametros actuales: `"&COUNTIF(${aN}${d}:${aN}${h},`"SI`")&`" de `"&COUNTA(${re}${d}:${re}${h})"
    $rRes++
  }

  # Formatos de numero
  foreach ($c in $columnas) {
    $rango = $ws.Range("$(L $c[0])${primeraFila}:$(L $c[0])${ultimaFila}")
    switch ($c[2]) {
      "pct" { $rango.NumberFormat = "0%" }
      "" { }
      default { $rango.NumberFormatLocal = (Fmt ([int]$c[2])) }
    }
  }

  # Colores de zona: DIESEL amarillo (LED amarillo), NAFTA azul (LED azul), dudas gris
  # (un rango por vez: Excel en espanol no acepta rangos separados por coma)
  $columnasZona = @("Real", "Resultado", "ResN") + ($variables | ForEach-Object { "Z_" + $_ }) + ($variables | ForEach-Object { "N_" + $_ })

  foreach ($clave in $columnasZona) {
    $rango = "$(L $clave)${primeraFila}:$(L $clave)${ultimaFila}"
    foreach ($zona in $coloresZona.Keys) {
      $fc = $ws.Range($rango).FormatConditions.Add(1, 3, "=`"$zona`"")
      $fc.Interior.Color = $coloresZona[$zona]
    }
  }

  foreach ($clave in "Acerto", "AcertoN", "Tope655") {
    $rangoSiNo = $ws.Range("$(L $clave)${primeraFila}:$(L $clave)${ultimaFila}")
    $colorSi = 0x008000; $colorNo = 0x0000FF
    if ($clave -eq "Tope655") { $colorSi = 0x0000FF; $colorNo = 0x808080 }  # SI = el VarBME no sirve
    $fc = $rangoSiNo.FormatConditions.Add(1, 3, "=`"SI`""); $fc.Font.Color = $colorSi; $fc.Font.Bold = $true
    $fc = $rangoSiNo.FormatConditions.Add(1, 3, "=`"NO`""); $fc.Font.Color = $colorNo; $fc.Font.Bold = $true
  }

  # Bordes y anchos
  $tabla = $ws.Range($ws.Cells.Item($filaEnc, 1), $ws.Cells.Item($ultimaFila, $ultimaCol))
  $tabla.Borders.LineStyle = 1
  $tabla.Borders.Color = 0xBFBFBF

  $datosTabla = $ws.Range($ws.Cells.Item($primeraFila, 1), $ws.Cells.Item($ultimaFila, $ultimaCol))
  $datosTabla.VerticalAlignment = -4160  # arriba

  $ws.Rows.Item($filaEnc).RowHeight = 40
  for ($c = 1; $c -le $ultimaCol; $c++) { $ws.Columns.Item($c).ColumnWidth = 12 }
  $ws.Columns.Item($col["N"]).ColumnWidth = 5
  $ws.Columns.Item($col["Obs"]).ColumnWidth = 55
  $ws.Columns.Item($col["Detalle"]).ColumnWidth = 60
  $ws.Range("$(L 'Obs')${primeraFila}:$(L 'Obs')${ultimaFila}").WrapText = $true
  $ws.Range("$(L 'Detalle')${primeraFila}:$(L 'Detalle')${ultimaFila}").WrapText = $true

  # Alto de filas: las pruebas se ajustan solas, las del corte (celdas
  # combinadas) se calculan
  foreach ($fp in $filasPruebas) { [void]$ws.Rows.Item($fp).AutoFit() }

  $anchoCorte = 0
  for ($c = 1; $c -le $colFinCorte; $c++) { $anchoCorte += $ws.Columns.Item($c).ColumnWidth }
  foreach ($fc2 in $filasCorte) { $ws.Rows.Item($fc2[0]).RowHeight = (Alto $fc2[1] $anchoCorte $fc2[2]) }

  # Congelar encabezado y columnas N / Fecha / Hora / real / resultado
  $ws.Activate()
  $xl.ActiveWindow.SplitRow = $filaEnc
  $xl.ActiveWindow.SplitColumn = $colFinCorte
  $xl.ActiveWindow.FreezePanes = $true

  $resumen += ("{0}: {1} muestras, {2} corte(s)" -f $info.Hoja, $lista.Count, ($versiones.Count - 1))
}

#--------------------------
# Hoja Observaciones
#--------------------------

if ($generales.Count -gt 0) {
  $wo = $wb.Worksheets.Add([Type]::Missing, $wb.Worksheets.Item($wb.Worksheets.Count))
  $wo.Name = "Observaciones"
  $wo.Cells.Font.Name = "Arial"
  $wo.Cells.Font.Size = 10

  Set-Celda $wo 1 1 "OLFATUS PETROLEUM - Observaciones de las pruebas"
  $wo.Cells.Item(1, 1).Font.Bold = $true
  $wo.Cells.Item(1, 1).Font.Size = 14

  $encO = "Programa", "Fecha", "Pruebas", "Tema", "Observacion"
  for ($c = 0; $c -lt $encO.Count; $c++) { Set-Celda $wo 3 ($c + 1) $encO[$c] }
  $wo.Range("A3:E3").Font.Bold = $true
  $wo.Range("A3:E3").Interior.Color = $gris
  $wo.Columns.Item(2).NumberFormat = "@"

  $fo = 4
  foreach ($o in $generales) {
    $nombreProg = $o.Programa
    if ($programas.Contains($o.Programa)) { $nombreProg = $programas[$o.Programa].Hoja }

    Set-Celda $wo $fo 1 $nombreProg
    Set-Celda $wo $fo 2 $o.Fecha
    Set-Celda $wo $fo 3 $o.Pruebas
    Set-Celda $wo $fo 4 $o.Tema
    Set-Celda $wo $fo 5 $o.Texto
    $wo.Cells.Item($fo, 4).Font.Bold = $true
    $fo++
  }

  $ultimaO = $fo - 1
  $wo.Columns.Item(1).ColumnWidth = 18
  $wo.Columns.Item(2).ColumnWidth = 11
  $wo.Columns.Item(3).ColumnWidth = 14
  $wo.Columns.Item(4).ColumnWidth = 24
  $wo.Columns.Item(5).ColumnWidth = 110

  $tablaO = $wo.Range("A3:E$ultimaO")
  $tablaO.Borders.LineStyle = 1
  $tablaO.Borders.Color = 0xBFBFBF
  $wo.Range("A4:E$ultimaO").VerticalAlignment = -4160
  $wo.Range("C4:E$ultimaO").WrapText = $true
  for ($f = 4; $f -le $ultimaO; $f++) { [void]$wo.Rows.Item($f).AutoFit() }

  $resumen += ("Observaciones: {0}" -f $generales.Count)
}

#---------------------------------------------------------
# 3) CONTROL DE ERRORES Y GUARDADO
#---------------------------------------------------------

# Parametros al final, las hojas de muestras primero
$wp.Move([Type]::Missing, $wb.Worksheets.Item($wb.Worksheets.Count))
$wb.Worksheets.Item(1).Activate()

$xl.CalculateFull()

foreach ($hoja in $wb.Worksheets) {
  foreach ($celda in $hoja.UsedRange.Cells) {
    if ($celda.Text -like "#*") { $errores++; Write-Host ("Error en {0}!{1}: {2}" -f $hoja.Name, $celda.Address(), $celda.Text) -ForegroundColor Red }
  }
}

try {
  if (Test-Path $Salida) { Remove-Item $Salida -Force -ErrorAction Stop }
  $wb.SaveAs($Salida, 51)
  $guardado = $true
} catch {
  Write-Host "No se pudo guardar $Salida. Si esta abierto en Excel, cerralo y volve a correr el script." -ForegroundColor Red
  $guardado = $false
}

$wb.Close($false)
$xl.Quit()
[void][Runtime.InteropServices.Marshal]::ReleaseComObject($xl)

if (-not $guardado) { exit 1 }

$resumen | ForEach-Object { Write-Host $_ }
Write-Host ("Excel -> {0}" -f $Salida) -ForegroundColor Green
Write-Host ("Errores de formula: {0}" -f $errores)
