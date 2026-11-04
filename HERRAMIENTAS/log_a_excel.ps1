<#
  LOG SERIAL -> EXCEL - OLFATUS PETROLEUM

  Lee HERRAMIENTAS\logs\serial.log (lo que guarda monitor_serial.ps1)
  y arma un Excel con una fila por muestra del DIFERENCIADOR_V7.

  Usa el Excel instalado en la PC (no hace falta Python).

  Uso:
    powershell -ExecutionPolicy Bypass -File HERRAMIENTAS\log_a_excel.ps1 -Reales "Diesel,Nafta,Diesel"

  -Reales  : combustible que realmente se puso en cada prueba, en
             orden y separado por comas (opcional, se puede
             completar despues en las celdas amarillas).
  -Nota    : texto que se agrega arriba de la tabla.
  -Salida  : ruta del .xlsx (por defecto DATOS DE PRUEBAS\PRUEBAS_<fecha>.xlsx)
#>

param(
  [string]$Log = (Join-Path $PSScriptRoot "logs\serial.log"),
  [string]$Salida = "",
  [string]$Reales = "",
  [string]$Nota = "Limpiezas de 1 minuto entre pruebas."
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

if (-not $Salida) {
  $carpeta = Join-Path (Split-Path $PSScriptRoot -Parent) "DATOS DE PRUEBAS"
  New-Item -ItemType Directory -Force $carpeta | Out-Null
  $Salida = Join-Path $carpeta ("PRUEBAS_{0}.xlsx" -f (Get-Date -Format "yyyy-MM-dd"))
}

#---------------------------------------------------------
# 1) LEER EL LOG
#---------------------------------------------------------

$pruebas = New-Object System.Collections.ArrayList
$p = $null
$esperaResultado = $false
$enResultado = $false

foreach ($linea in (Get-Content $Log -Encoding UTF8)) {
  if ($linea -notmatch '^\[(\d\d:\d\d:\d\d)\.\d+\] ?(.*)$') { continue }

  $hora = $Matches[1]
  $t = $Matches[2].Trim()

  if ($t -match '^Capturando') {
    $p = [ordered]@{ Hora = $hora; Detalle = @() }
    [void]$pruebas.Add($p)
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

# Solo las pruebas que llegaron al diagnostico
$pruebas = @($pruebas | Where-Object { $_.Contains("Resultado") })

if ($pruebas.Count -eq 0) {
  Write-Host "No se encontraron muestras en $Log" -ForegroundColor Red
  exit 1
}

$listaReales = @()
if ($Reales) { $listaReales = $Reales.Split(",") | ForEach-Object { $_.Trim() } }

#---------------------------------------------------------
# 2) ARMAR EL EXCEL
#---------------------------------------------------------

$xl = New-Object -ComObject Excel.Application
$xl.Visible = $false

# Excel toma los formatos de numero con el idioma de Windows: en
# espanol "0.000" usa el punto como separador de miles y 1.133 se
# veia como 0.001. Se arma el formato con el separador decimal que
# usa este Excel.
$separadorDecimal = $xl.International(3)
function Fmt([int]$decimales) { return "0" + $separadorDecimal + ("0" * $decimales) }
$xl.DisplayAlerts = $false

$wb = $xl.Workbooks.Add()
$ws = $wb.Worksheets.Item(1)
$ws.Name = "Muestras"
$wp = $wb.Worksheets.Add([Type]::Missing, $ws)
$wp.Name = "Parámetros"

$amarillo = 0x99FFFF   # BGR: amarillo claro
$gris = 0xE0E0E0
$azulTexto = 0xFF0000  # BGR: azul

#--------------------------
# Hoja Parametros (copiada de DIFERENCIADOR_V7.ino del Nano)
#--------------------------

$wp.Cells.Font.Name = "Arial"
$wp.Cells.Font.Size = 10
Set-Celda $wp (1) (1) ("Intervalos y pesos de DIFERENCIADOR_V7 (Arduino Nano)")
$wp.Cells.Item(1, 1).Font.Bold = $true
$wp.Cells.Item(1, 1).Font.Size = 12
Set-Celda $wp (2) (1) ("Copiados del programa. Si se cambian aca se recalculan las columnas 'con parametros actuales' de la hoja Muestras, pero NO cambian en el Arduino.")

$encP = "Variable", "Diesel min", "Diesel max", "Nafta min", "Nafta max", "Peso", "Nota"
for ($c = 0; $c -lt $encP.Count; $c++) { Set-Celda $wp (4) ($c + 1) ($encP[$c]) }
$rangoEnc = $wp.Range("A4:G4")
$rangoEnc.Font.Bold = $true
$rangoEnc.Interior.Color = $gris

$parametros = @(
  @("Relacion", 0.6, 1.23, 1.24, 10.0, 3, "(max138 + max135) / (ref138 + ref135). Factor de subida, 1.00 = sin cambio."),
  @("VarBME", -10.0, 40.0, 41.0, 105.0, 2, "%. -999 = BME no disponible."),
  @("Area135", 1.40, 2.90, 3.00, 7.60, 1, "Aproximado con las pruebas del 03/11/2026 (diesel 2.56 / 2.76, nafta 3.18). Antes: 1.40 / 3.3 / 3.40 / 7.60."),
  @("Var135", -5.0, 8.0, 8.5, 950.0, 1, "%"),
  @("Pend135", 0.0, 0.42, 0.43, 1.90, 0, "Peso 0: no vota.")
)

for ($i = 0; $i -lt $parametros.Count; $i++) {
  $fila = 5 + $i
  for ($c = 0; $c -lt 7; $c++) { Set-Celda $wp ($fila) ($c + 1) ($parametros[$i][$c]) }
  $wp.Range("B${fila}:F${fila}").Font.Color = $azulTexto
}

$wp.Range("B5:E9").NumberFormatLocal = (Fmt 2)
$wp.Columns.Item(1).ColumnWidth = 12
for ($c = 2; $c -le 6; $c++) { $wp.Columns.Item($c).ColumnWidth = 11 }
$wp.Columns.Item(7).ColumnWidth = 90

# Fila de Parametros de cada variable
$filaParam = @{ Relacion = 5; VarBME = 6; Area135 = 7; Var135 = 8; Pend135 = 9 }

#--------------------------
# Hoja Muestras
#--------------------------

$ws.Cells.Font.Name = "Arial"
$ws.Cells.Font.Size = 10

Set-Celda $ws (1) (1) ("OLFATUS PETROLEUM - Pruebas DIFERENCIADOR_V7 (Arduino Nano) - " + (Get-Item $Log).LastWriteTime.ToString("dd/MM/yyyy"))
$ws.Cells.Item(1, 1).Font.Bold = $true
$ws.Cells.Item(1, 1).Font.Size = 14
Set-Celda $ws (2) (1) ("NOTA: " + $Nota)
$ws.Cells.Item(2, 1).Font.Bold = $true
Set-Celda $ws (3) (1) ("Resultado, valores y zonas 'placa' = lo que imprimio el Arduino con el programa cargado en ese momento. Columnas 'con parametros actuales' = zonas recalculadas con la hoja Parametros.")
Set-Celda $ws (4) (1) ("Celdas amarillas = combustible que realmente se puso (cargado a mano, corregir si hace falta).")

$encabezados = @(
  "N", "Hora", "Combustible real", "Resultado placa", "Acerto?", "Captura (ms)",
  "Ref MQ135 (V)", "Ref MQ138 (V)", "Ref BME (kOhm)", "Max MQ135 (V)", "Max MQ138 (V)",
  "Max/Ref 138", "Max/Ref 135", "Var138 (%)", "Firma", "Muestra detectada por",
  "Relacion", "Zona Relacion (placa)", "VarBME (%)", "Zona VarBME (placa)",
  "Area135", "Zona Area135 (placa)", "Var135 (%)", "Zona Var135 (placa)",
  "Pend135", "Zona Pend135 (placa)",
  "Peso DUDA DEBAJO", "Peso DIESEL", "Peso DUDA MEDIO", "Peso NAFTA", "Peso DUDA ENCIMA",
  "Posicion global", "Confianza", "Detalle / avisos",
  "Zona Relacion (param. actuales)", "Zona VarBME (param. actuales)", "Zona Area135 (param. actuales)",
  "Zona Var135 (param. actuales)", "Zona Pend135 (param. actuales)"
)

$filaEnc = 6
for ($c = 0; $c -lt $encabezados.Count; $c++) { Set-Celda $ws ($filaEnc) ($c + 1) ($encabezados[$c]) }

$ultimaCol = $encabezados.Count
$rangoEnc = $ws.Range($ws.Cells.Item($filaEnc, 1), $ws.Cells.Item($filaEnc, $ultimaCol))
$rangoEnc.Font.Bold = $true
$rangoEnc.Interior.Color = $gris
$rangoEnc.WrapText = $true
$rangoEnc.VerticalAlignment = -4108  # centro

[void]$ws.Cells.Item($filaEnc, 2).AddComment($Nota)

# Columna de valor de cada variable en esta hoja
$colValor = @{ Relacion = "Q"; VarBME = "S"; Area135 = "U"; Var135 = "W"; Pend135 = "Y" }
$colZonaNueva = @{ Relacion = 35; VarBME = 36; Area135 = 37; Var135 = 38; Pend135 = 39 }

$fila = $filaEnc + 1

for ($i = 0; $i -lt $pruebas.Count; $i++) {
  $m = $pruebas[$i]
  $r = $fila + $i

  Set-Celda $ws ($r) (1) ($i + 1)
  Set-Celda $ws ($r) (2) ($m.Hora)

  if ($i -lt $listaReales.Count) { Set-Celda $ws ($r) (3) ($listaReales[$i].ToUpper()) }
  $ws.Cells.Item($r, 3).Interior.Color = $amarillo

  Set-Celda $ws ($r) (4) ($m.Resultado)
  $ws.Cells.Item($r, 5).Formula = "=IF(C$r=`"`",`"`",IF(UPPER(C$r)=UPPER(D$r),`"SI`",`"NO`"))"
  Set-Celda $ws ($r) (6) ($m.Duracion)

  Set-Celda $ws ($r) (7) ($m.Ref135)
  Set-Celda $ws ($r) (8) ($m.Ref138)
  Set-Celda $ws ($r) (9) ($m.RefBME)
  Set-Celda $ws ($r) (10) ($m.Max135)
  Set-Celda $ws ($r) (11) ($m.Max138)
  $ws.Cells.Item($r, 12).Formula = "=IF(H$r>0.05,K$r/H$r,`"`")"
  $ws.Cells.Item($r, 13).Formula = "=IF(G$r>0.01,J$r/G$r,`"`")"
  Set-Celda $ws ($r) (14) ($m.Var138)
  Set-Celda $ws ($r) (15) ($m.Firma)
  Set-Celda $ws ($r) (16) ($m.MuestraPor)

  $c = 17
  foreach ($v in "Relacion", "VarBME", "Area135", "Var135", "Pend135") {
    Set-Celda $ws ($r) ($c) ($m["V_" + $v])
    Set-Celda $ws ($r) ($c + 1) ($m["Z_" + $v])
    $c += 2
  }

  $c = 27
  foreach ($z in "DUDA DEBAJO", "DIESEL", "DUDA MEDIO", "NAFTA", "DUDA ENCIMA") {
    Set-Celda $ws ($r) ($c) ($m["P_" + $z])
    $c++
  }

  Set-Celda $ws ($r) (32) ($m.Posicion)
  Set-Celda $ws ($r) (33) ($m.Confianza)
  Set-Celda $ws ($r) (34) (($m.Detalle -join " "))

  # Zonas recalculadas con la hoja Parametros
  foreach ($v in $colValor.Keys) {
    $x = $colValor[$v] + $r
    $k = $filaParam[$v]
    $pr = "'Parámetros'!"
    $formula = "=IF($x=`"`",`"`",IF($x<$pr`$B`$$k,`"DUDA DEBAJO`",IF($x<=$pr`$C`$$k,`"DIESEL`",IF($x<$pr`$D`$$k,`"DUDA MEDIO`",IF($x<=$pr`$E`$$k,`"NAFTA`",`"DUDA ENCIMA`")))))"
    if ($v -eq "VarBME") { $formula = "=IF($x=-999,`"no disponible`"," + $formula.Substring(1) + ")" }
    $ws.Cells.Item($r, $colZonaNueva[$v]).Formula = $formula
  }
}

$ultimaFila = $fila + $pruebas.Count - 1

# Resumen
$rRes = $ultimaFila + 2
Set-Celda $ws ($rRes) (1) ("Aciertos")
$ws.Cells.Item($rRes, 1).Font.Bold = $true
$ws.Cells.Item($rRes, 3).Formula = "=COUNTIF(E${fila}:E${ultimaFila},`"SI`")&`" de `"&COUNTA(C${fila}:C${ultimaFila})"

# Formatos de numero
$ws.Range("G${fila}:H${ultimaFila}").NumberFormatLocal = (Fmt 3)
$ws.Range("I${fila}:I${ultimaFila}").NumberFormatLocal = (Fmt 1)
$ws.Range("J${fila}:M${ultimaFila}").NumberFormatLocal = (Fmt 3)
$ws.Range("N${fila}:N${ultimaFila}").NumberFormatLocal = (Fmt 1)
$ws.Range("O${fila}:O${ultimaFila}").NumberFormatLocal = (Fmt 2)
foreach ($col in "Q", "U", "Y") { $ws.Range("${col}${fila}:${col}${ultimaFila}").NumberFormatLocal = (Fmt 3) }
foreach ($col in "S", "W") { $ws.Range("${col}${fila}:${col}${ultimaFila}").NumberFormatLocal = (Fmt 1) }
$ws.Range("AF${fila}:AF${ultimaFila}").NumberFormatLocal = (Fmt 2)
$ws.Range("AG${fila}:AG${ultimaFila}").NumberFormat = "0%"

# Colores de zona: DIESEL amarillo (LED amarillo), NAFTA azul (LED azul), dudas gris
# (un rango por vez: Excel en espanol no acepta rangos separados por coma)
$coloresZona = @{ "DIESEL" = 0x66E6FF; "NAFTA" = 0xF7D9BD; "DUDA DEBAJO" = 0xD9D9D9; "DUDA MEDIO" = 0xD9D9D9; "DUDA ENCIMA" = 0xD9D9D9 }

foreach ($rango in "C${fila}:D${ultimaFila}", "R${fila}:R${ultimaFila}", "T${fila}:T${ultimaFila}", "V${fila}:V${ultimaFila}", "X${fila}:X${ultimaFila}", "Z${fila}:Z${ultimaFila}", "AI${fila}:AM${ultimaFila}") {
  foreach ($zona in $coloresZona.Keys) {
    $fc = $ws.Range($rango).FormatConditions.Add(1, 3, "=`"$zona`"")
    $fc.Interior.Color = $coloresZona[$zona]
  }
}

$fc = $ws.Range("E${fila}:E${ultimaFila}").FormatConditions.Add(1, 3, "=`"SI`""); $fc.Font.Color = 0x008000; $fc.Font.Bold = $true
$fc = $ws.Range("E${fila}:E${ultimaFila}").FormatConditions.Add(1, 3, "=`"NO`""); $fc.Font.Color = 0x0000FF; $fc.Font.Bold = $true

# Bordes y anchos
$tabla = $ws.Range($ws.Cells.Item($filaEnc, 1), $ws.Cells.Item($ultimaFila, $ultimaCol))
$tabla.Borders.LineStyle = 1
$tabla.Borders.Color = 0xBFBFBF

$ws.Rows.Item($filaEnc).RowHeight = 40
for ($c = 1; $c -le $ultimaCol; $c++) { $ws.Columns.Item($c).ColumnWidth = 12 }
$ws.Columns.Item(1).ColumnWidth = 5
$ws.Columns.Item(34).ColumnWidth = 60
$ws.Range("AH${fila}:AH${ultimaFila}").WrapText = $true

# Congelar encabezado y columnas N / Hora / real / resultado
$ws.Activate()
$xl.ActiveWindow.SplitRow = $filaEnc
$xl.ActiveWindow.SplitColumn = 4
$xl.ActiveWindow.FreezePanes = $true

#---------------------------------------------------------
# 3) CONTROL DE ERRORES Y GUARDADO
#---------------------------------------------------------

$xl.CalculateFull()

$errores = 0
foreach ($hoja in $ws, $wp) {
  foreach ($celda in $hoja.UsedRange.Cells) {
    if ($celda.Text -like "#*") { $errores++; Write-Host ("Error en {0}!{1}: {2}" -f $hoja.Name, $celda.Address(), $celda.Text) -ForegroundColor Red }
  }
}

if (Test-Path $Salida) { Remove-Item $Salida -Force }
$wb.SaveAs($Salida, 51)
$wb.Close($false)
$xl.Quit()
[void][Runtime.InteropServices.Marshal]::ReleaseComObject($xl)

Write-Host ("{0} muestras -> {1}" -f $pruebas.Count, $Salida) -ForegroundColor Green
Write-Host ("Errores de formula: {0}" -f $errores)
