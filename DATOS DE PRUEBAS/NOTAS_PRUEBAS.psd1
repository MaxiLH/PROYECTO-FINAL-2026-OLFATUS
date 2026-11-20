# NOTAS DE LAS PRUEBAS - OLFATUS PETROLEUM
#
# Lo usa HERRAMIENTAS\log_a_excel.ps1 para lo que no sale del log
# del monitor serie. Se puede editar a mano (guardar en UTF-8).
#
#  Cortes    : por programa y version, el texto del corte que se
#              pone en el Excel antes de las pruebas de esa version.
#  Filas     : observacion corta de una prueba (columna Observaciones).
#              Clave = 'Programa|dd/MM/yyyy|HH:mm:ss' con la hora en
#              que arranco la extraccion (columna Hora del Excel).
#  NotasHoja : nota extra arriba de la hoja de un programa.
#  Generales : filas de la hoja Observaciones.
#
# Programas: Diferenciador, Completo, Demo.

@{
  Cortes = @{
    Completo = @{
      '7.1' = @{
        Titulo = 'CORTE - Cambios hechos el 05/11/2026 después de estas pruebas (programa completo versión 7.1). Las pruebas de abajo son con estos cambios:'
        Cambios = @(
          'Limpieza: 60 s -> 45 s (el BME tiene que quedar arriba de 220 kOhm durante 45 s).'
          'Aviso 5 s antes de tomar la referencia: un pitido por segundo y un mensaje en el monitor serie.'
          'Al tomar la referencia se imprimen los valores (MQ135, MQ138 y BME).'
          'Extracción de 3 s: 25 muestras cada 120 ms. Antes duraba 9,25 s y no 5 s, porque cada lectura del BME frenaba el programa unos 370 ms; ahora el BME se lee sin frenar.'
          'BME sin el tope de 655 kOhm: antes todo valor arriba de 655 se guardaba como 655 y, con diesel, el VarBME salía de la referencia y no de la muestra.'
          'VarBME: techo del diesel 40 -> 60 y piso de la nafta 41 -> 61 (ver hoja Observaciones).'
          'DUDA: 2 pitidos (antes 1, igual que el combustible correcto).'
          'Combustible incorrecto (hoy, nafta): se prende el LED del combustible y además el rojo.'
        )
      }
      '7.2' = @{
        Titulo = 'CORTE - Versión 7.2 (05/11/2026): se volvió a los tiempos de la versión 7.0 porque los de la 7.1 no funcionaron bien. Las pruebas de abajo son con estos cambios:'
        Cambios = @(
          'Limpieza: 45 s -> 60 s otra vez.'
          'Extracción: 25 muestras cada 200 ms otra vez, con la lectura del BME de la 7.0 (frena el programa ~370 ms), así que la captura vuelve a durar ~9,25 s.'
          'Se mantienen de la 7.1: aviso 5 s antes de la referencia, impresión de la referencia, BME sin el tope de 655 kOhm, VarBME 60 / 61, DUDA con 2 pitidos y LED rojo con el combustible incorrecto.'
        )
      }
      '7.3' = @{
        Titulo = 'CORTE - Versión 7.3 (05/11/2026): con muchas naftas seguidas la referencia quedaba contaminada. Las pruebas de abajo son con estos cambios:'
        Cambios = @(
          'Relación: techo del diesel 1,23 -> 1,10 y piso de la nafta 1,24 -> 1,10 (también en el DIFERENCIADOR_V7).'
          'Limpieza: 60 s -> 1 min 5 s (el aviso de la referencia suena a los 60 s).'
        )
      }
    }
    CompletoESP32 = @{
      '7.3.1' = @{
        Titulo = 'CORTE - Versión 7.3.1 ESP32 (12/11/2026). Las pruebas de abajo son con estos cambios:'
        Cambios = @(
          'Botón con antirrebote: se lee cada 5 ms (Ticker), aparte del loop, y un cambio cuenta si dura 40 ms. Las pulsaciones de antes de entrar a un estado no cuentan.'
          'El botón de la placa del ESP da HIGH suelto y LOW apretado. Desde las 14:29 cuenta al apretarlo (en la prueba de las 14:26 todavía contaba al soltarlo).'
          'Buzzer: se probó HIGH = suena y sonaba todo el tiempo; se volvió a LOW = suena, como en el Nano.'
          '(No es del programa) Entre las 14:34 y las 15:34 se modificó la placa: las referencias de los MQ bajaron de ~2,6-3,0 V a ~0,42-0,53 V. Las 10 pruebas de la tarde (desde las 16:20) son con la placa modificada.'
        )
      }
      '7.3.2' = @{
        Titulo = 'CORTE - Versión 7.3.2 ESP32 (12/11/2026): cambio hecho después de las 10 pruebas (todavía sin pruebas):'
        Cambios = @(
          'Area135: 1,40 / 2,90 / 3,00 / 7,60 (los del Nano) -> 0,65 / 1,33 / 1,34 / 3,50. El resto de los intervalos no cambió.'
        )
      }
    }
  }

  Filas = @{
    'Completo|05/11/2026|09:40:59' = 'VarBME mal: 2,9 % (la nafta da 88-92 %), por eso DUDA MEDIO. Causa probable: BME sin calentar. Era la 1ª prueba: la referencia se tomó 1 min después de prender la placa y dio 674,8 kOhm, la más baja del día, y el BME no bajó de 655 kOhm en toda la captura. Ver hoja Observaciones.'
    'Completo|05/11/2026|09:44:00' = 'VarBME 37,8 % = tope de 655 kOhm (el BME no bajó de 655): no midió la muestra. Igual dio DIESEL.'
    'Completo|05/11/2026|10:13:59' = 'VarBME 51,2 % = tope de 655 kOhm, no la muestra. Votó NAFTA (techo 40) y dio DUDA MEDIO. Con los parámetros actuales da DIESEL.'
    'Completo|05/11/2026|10:25:04' = 'Referencia del BME baja (546,2 kOhm; la anterior 1342,5): puede que hayan quedado vapores, la muestra anterior (DUDA) quedó 1 min encerrada antes de limpiar. Igual dio DIESEL.'
    'Completo|05/11/2026|10:30:00' = 'VarBME 44,4 % = tope de 655 kOhm. Votó NAFTA (techo 40) y dio DUDA MEDIO. Con los parámetros actuales da DIESEL.'
    'Completo|05/11/2026|10:32:02' = 'VarBME 51,5 % = tope de 655 kOhm. Votó NAFTA (techo 40) y dio DUDA MEDIO. Con los parámetros actuales da DIESEL.'
    'Completo|05/11/2026|11:23:22' = 'Versión 7.1, extracción de 3 s: el BME bajó apenas 8 %. Falta cargar el combustible real.'
    'Completo|05/11/2026|11:25:39' = 'Versión 7.1, extracción de 3 s. Falta cargar el combustible real.'
    'Completo|05/11/2026|11:36:54' = 'Nafta después de otra nafta: Relación 1,229 votó DIESEL (piso de la nafta 1,24) y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|11:39:43' = 'Nafta después de otra nafta: Relación 1,146 votó DIESEL y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|11:49:08' = 'Nafta después de otra nafta: Relación 1,146 votó DIESEL y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|11:56:02' = 'Nafta después de otra nafta: Relación 1,138 votó DIESEL y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|11:58:23' = 'Nafta después de otra nafta: Relación 1,189 votó DIESEL y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|11:59:59' = 'Nafta después de otra nafta: Relación 1,203 votó DIESEL y dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|12:02:12' = 'Relación 1,067 con nafta (referencia del MQ138 en 1,727 V, la más alta del día): ni con 1,10 vota nafta y sigue en DUDA MEDIO.'
    'Completo|05/11/2026|12:04:56' = 'Relación 1,237, entre 1,23 y 1,24 (zona DUDA MEDIO): dio DUDA MEDIO. Con 1,10 da NAFTA.'
    'Completo|05/11/2026|14:27:57' = 'Nafta después de otra nafta muy fuerte (en la 28 el MQ138 subió 83 %): la referencia del MQ138 quedó en 1,791 V (la anterior, 1,016) y el MQ138 no subió (-0,4 %). Relación 1,088 votó DIESEL aun con el límite en 1,10; VarBME, Area135 y Var135 dieron NAFTA. Con 1,08 daría NAFTA.'
    'Completo|05/11/2026|14:32:00' = 'Primera después del reinicio de las 14:30:31 (la referencia de antes del reinicio había quedado en MQ138 1,938 V y BME 767,6 kOhm). Con la referencia nueva (MQ138 1,190 V) dio NAFTA con Relación 1,435.'
    'Completo|05/11/2026|14:42:11' = 'Referencia del MQ138 en 1,814 V y el MQ138 bajó 10 % durante la captura (se seguía limpiando de la nafta anterior): Relación 0,993 votó DIESEL. VarBME (68,3 %), Area135 y Var135 dieron NAFTA. Es la menos segura de las naftas deducidas.'
    'Completo|05/11/2026|14:47:05' = 'Nafta después de otra nafta: referencia del MQ138 en 1,692 V, Relación 1,087 votó DIESEL y dio DUDA MEDIO. Con 1,08 daría NAFTA.'
    'Completo|05/11/2026|14:54:22' = 'Area135 3,196 votó NAFTA (piso de la nafta 3,00); igual dio DIESEL con 86 %.'
    'Completo|05/11/2026|15:04:27' = 'Nafta después de otra nafta y 6 min 8 s entre la referencia y el botón: Relación 1,097, apenas debajo de 1,10, votó DIESEL y dio DUDA MEDIO. Con 1,08 daría NAFTA.'
    'Completo|05/11/2026|15:19:22' = 'VarBME 56,1 %: el diesel más alto sin el tope de 655 kOhm, a 4 puntos del techo (60). Igual dio DIESEL.'
    'Completo|05/11/2026|15:29:46' = 'Nafta después de otra nafta: referencia del MQ138 en 1,823 V y el MQ138 bajó 4 %: Relación 1,050 votó DIESEL y dio DUDA MEDIO.'
    'Completo|05/11/2026|15:43:09' = 'El MQ135 subió 82 %, la más fuerte de la tarde. La referencia siguiente quedó con el MQ135 en 1,344 V (las demás de la tarde, 0,92-1,30 V) y antes de la próxima prueba la placa se reinició (15:44:59).'
    'CompletoESP32|12/11/2026|14:13:46' = 'SIN MUESTRA: arrancó sola 0,35 s después de la referencia (ningún sensor reaccionó). El botón de la placa del ESP da HIGH suelto y el programa lo tomaba como pulsación. Corregido en la 7.3.1.'
    'CompletoESP32|12/11/2026|14:26:38' = 'Primera con la 7.3.1: arrancó al soltar el botón (todavía contaba con HIGH). Referencias de los MQ en ~2,6 V.'
    'CompletoESP32|12/11/2026|14:32:54' = 'Max MQ135 = Max MQ138 = 3,100 V: saturado, es el tope del ADC del ESP32. Las referencias estaban en ~3,0 V.'
    'CompletoESP32|12/11/2026|16:20:34' = 'Primera de las 10 pruebas de la tarde, con la placa modificada: referencias de los MQ en ~0,45 V (a las 14:00, ~2,6-3,0 V).'
    'CompletoESP32|12/11/2026|16:23:21' = 'Area135 1,251: la nafta más baja, por debajo de 3 diesel (1,267 / 1,299 / 1,324). Fue la de referencia del MQ135 más baja (0,419 V). Con los intervalos de la 7.3.2 vota DIESEL; el resultado igual da NAFTA.'
    'Completo|05/11/2026|15:46:20' ='Primera después del reinicio de las 15:44:59: la referencia del BME dio 384,3 kOhm (las demás de la tarde, 716-1150 kOhm) y el VarBME (52,7 %) votó DIESEL; dio DUDA MEDIO con 71 %. Puede ser el BME recién reiniciado (como en la 1ª prueba de la mañana) o vapores que quedaron de la 63.'
  }

  NotasHoja = @{
    CompletoESP32 = 'EMPEZAMOS CON EL ESP32 (12/11/2026). Combustible real de las 10 pruebas de la tarde: lo dijo el grupo (diesel y nafta alternados, la placa acertó todas). Las 4 primeras quedaron en blanco (la de las 14:13 fue sin muestra).'
    Completo = 'DESDE EL 12/11/2026 SEGUIMOS CON EL ESP32: ver hoja "Programa completo ESP32". Combustible real del 05/11: lo dedujo Claude de los datos y de lo que contó el grupo, también el de la versión 7.3 (ver hoja Observaciones). Las 2 pruebas de la versión 7.1 quedaron en blanco. Confirmarlo.'
  }

  Generales = @(
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '1 (09:40)'
      Tema = 'VarBME de la primera prueba'
      Texto = 'El VarBME dio 2,9 % cuando la nafta da 88-92 %, y salió DUDA MEDIO en vez de NAFTA (las otras variables que votan dieron NAFTA). Qué pasó: en toda la captura el BME no bajó de 655 kOhm, el tope con que se guardaba [VarBME = (674,8 - 655) / 674,8 = 2,9 %]; o sea, el BME casi no reaccionó a la nafta. Causa más probable: el BME todavía no estaba caliente ni estable. Era la primera prueba: la placa arrancó a las 09:37:13 y la referencia se tomó a las 09:38:14, 1 minuto después, y dio 674,8 kOhm, la más baja de la sesión (las siguientes, 912-1352 kOhm). En los primeros minutos el sensor de gas del BME todavía está subiendo y reacciona menos. La limpieza no lo frena porque su umbral (220 kOhm) está muy por debajo del aire limpio. Además pasaron 2 min 38 s entre la referencia y el botón. Ya había pasado en la primera prueba del 03/11 (referencia 437 kOhm, VarBME -2,1 %). Para evitarlo: dejar la placa prendida unos minutos antes de la primera prueba, o hacer una primera prueba de descarte.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '1, 2, 6, 10 y 11'
      Tema = 'Tope de 655 kOhm del BME'
      Texto = 'El programa guardaba el BME en un entero x 100, con tope en 655 kOhm. En aire limpio el BME da 900-1350 kOhm, así que cuando la muestra no lo baja de 655 (el diesel casi no lo mueve) el mínimo queda clavado en 655 y VarBME = (Ref - 655) / Ref: depende de la referencia, no de la muestra. Pasó en 5 de las 11 pruebas (columna "BME en el tope de 655 kOhm"). Por eso el diesel daba 37,8 / 44,4 / 51,2 / 51,5 % y votaba NAFTA. Corregido en la versión 7.1 y en el DIFERENCIADOR_V7: ahora se guarda x 10, hasta 6553 kOhm.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = 'todas'
      Tema = 'VarBME: techo del diesel'
      Texto = 'Pedido: subir el techo del diesel a 70. Datos: diesel medido de verdad (sin el tope) 17,5 y 31,5 % hoy y 25,2 % el 03/11; los valores más altos (37,8-51,5 %) eran el tope de 655 kOhm. Nafta: 88,0-92,5 % hoy y 65,4 % el 03/11 (diferenciador). Con 70, la nafta del 03/11 hubiera votado diesel; con 60 todas las pruebas quedan del lado correcto. Se puso techo del diesel 60 y piso de la nafta 61 (programa completo y diferenciador). Con estos parámetros, las 11 pruebas de hoy dan 10 de 11 (falla la 1, por el BME frío). Ojo: con la extracción de 3 s el BME tiene menos tiempo para bajar y los valores van a ser más chicos; hay que revisar estos límites con la próxima sesión.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '1, 6, 10 y 11'
      Tema = 'DUDA'
      Texto = 'Dio DUDA MEDIO 4 veces. El grupo comprobó que la duda sigue funcionando y que su alarma sonó bien. Desde la versión 7.1 la duda suena 2 veces, para no confundirla con el combustible correcto (1 pitido).'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = 'todas'
      Tema = 'Duración de la captura'
      Texto = 'El diagnóstico dice 25 muestras en 9,25 s, no en 5 s (25 x 200 ms): cada lectura del BME frenaba el programa unos 370 ms. En la versión 7.1 el BME se lee sin frenar y la extracción dura 3 s (25 x 120 ms). El BME se sigue actualizando cada ~370 ms, así que en los 3 s entran unas 8 lecturas del BME.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '4, 5 y 7'
      Tema = 'Esperas largas'
      Texto = 'Entre la referencia y el botón pasaron 10 min 53 s (prueba 4), 10 min 31 s (prueba 5) y 8 min 46 s (prueba 7). Con esperas largas la referencia puede quedar vieja; conviene apretar el botón poco después de que termine la referencia. Las tres dieron bien.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = 'todas'
      Tema = 'Combustible real'
      Texto = 'El combustible real de cada prueba lo dedujo Claude de los datos y de lo que contó el grupo: la 1 nafta (el VarBME dio mal y el resto dio nafta); la 6, 10 y 11 diesel (todo dio diesel menos el VarBME). En la versión 7.2: la 14, 18 y 19 diesel y el resto nafta (lo que dieron VarBME y Var135, y coincide con las muchas naftas seguidas que contó el grupo). Las 2 de la versión 7.1 (12 y 13) quedaron en blanco porque no se pueden deducir. En la versión 7.3 (28 a 67): diesel las 6 que dieron DIESEL (39, 45, 48, 49, 56 y 67) y nafta el resto, incluidas las 6 DUDA MEDIO: en todas el MQ135 subió 15-23 % y el VarBME dio 53-90 %, como la nafta (el diesel nunca pasó de 9,2 % en Var135). La menos segura es la 35. Confirmar y corregir en las celdas amarillas si hace falta.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '12 y 13'
      Tema = 'Versión 7.1 (3 s)'
      Texto = 'Con la limpieza de 45 s y la extracción de 3 s, las 2 pruebas dieron DUDA MEDIO. En 3 s los sensores tienen menos tiempo para reaccionar (en la 12 el BME bajó apenas 8 %). El grupo decidió volver a los tiempos de la 7.0 (versión 7.2).'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '14 a 27'
      Tema = 'Naftas seguidas (versión 7.2)'
      Texto = 'Después de una nafta, la referencia del MQ138 quedó en 1,43-1,73 V; después de un diesel, en 1,15-1,17 V. Con la referencia contaminada (más alta) la nafta sube menos respecto de ella: las naftas que vinieron después de un diesel dieron Relación 1,42 y 1,46, y las que vinieron después de otra nafta 1,07-1,28. Como el piso de la nafta era 1,24, la Relación (peso 3) votó DIESEL y 8 de las 11 naftas dieron DUDA MEDIO, aunque VarBME y Var135 seguían dando nafta. Por eso la versión 7.3 pone el techo del diesel y el piso de la nafta en 1,10 y la limpieza en 1 min 5 s. Con 1,10 estas 14 pruebas dan 13 de 14; la 26 (Relación 1,067) sigue en DUDA. Ojo: el diesel llegó a 1,077, muy cerca de 1,10, y la nafta de la 26 quedó por debajo: con la referencia contaminada la Relación ya no separa del todo.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '28 a 67'
      Tema = 'Versión 7.3: resultados'
      Texto = '40 pruebas (34 naftas y 6 diesel, deducidas por Claude): la placa acertó 34 de 40 (85 %); en la 7.2, 6 de 14. Los 6 diesel dieron DIESEL. De las 34 naftas, 28 dieron NAFTA y 6 DUDA MEDIO. Ningún diesel dio NAFTA y ninguna nafta dio DIESEL. La limpieza duró 65,5-73,3 s.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '29, 35, 37, 41 y 55'
      Tema = 'Referencia del MQ138 (versión 7.3)'
      Texto = 'La limpieza de 1 min 5 s no bajó la referencia del MQ138 después de una nafta: quedó en 1,54-1,85 V (salvo la 42, 1,27 V), contra 1,43-1,73 V en la 7.2 con 60 s. Después de un diesel quedó en 1,25-1,40 V. Las naftas que vinieron después de un diesel dieron Relación 1,32-1,47; las que vinieron después de otra nafta, 0,99-1,44. 5 naftas quedaron debajo de 1,10 (1,088 / 0,993 / 1,087 / 1,097 / 1,050): la Relación (peso 3) votó DIESEL y dieron DUDA MEDIO, aunque VarBME, Area135 y Var135 dieron NAFTA. El diesel dio Relación 0,91-1,01 en la 7.3 y como máximo 1,077 en todas las pruebas (la 7). Con el límite en 1,08, la 29, la 37 y la 41 darían NAFTA (37 de 40) y ningún diesel cambia, pero el margen con el diesel más alto sería de 0,003. Se puede probar cambiando la hoja Parámetros.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '28 a 67'
      Tema = 'Var135, VarBME y Area135 (versión 7.3)'
      Texto = 'Var135 separó todas: diesel -0,3 a 6,5 %, nafta 14,9 a 82,5 % (límites 8,0 / 8,5). VarBME, ya sin el tope de 655 kOhm: diesel 18,6-56,1 %, nafta 67,6-92,8 % salvo la 64 (52,7 %, referencia del BME baja); el límite 60 / 61 separa todo, pero el diesel 49 quedó a 4 puntos. Area135 se mezcla: diesel 2,53-3,20 y nafta 2,83-3,98; votó al revés en la 39 (diesel con 3,196) y en la 62 (nafta con 2,826), sin cambiar el resultado porque pesa 1.'
    }
    @{
      Programa = 'Completo'
      Fecha = '05/11/2026'
      Pruebas = '30 y 64'
      Tema = 'Reinicios de la placa'
      Texto = 'La placa se reinició 2 veces mientras esperaba el botón: a las 14:30:31 y a las 15:44:59. El log no dice si fue el botón de reset o un corte de alimentación. Las dos veces la referencia anterior había quedado alta (14:29: MQ138 1,938 V y BME 767,6 kOhm; 15:44: MQ135 1,344 V), así que puede que la hayan reiniciado a mano para volver a tomarla. Después del de las 14:30 la referencia salió limpia y la 30 dio NAFTA. Después del de las 15:44 el BME dio 384,3 kOhm de referencia y la 64 dio DUDA MEDIO. Si no fue a mano, revisar la alimentación y el cable USB.'
    }
    @{
      Programa = 'CompletoESP32'
      Fecha = '12/11/2026'
      Pruebas = 'todas'
      Tema = 'Empezamos con el ESP32'
      Texto = 'El 12/11/2026 se empezó a probar el programa completo en la placa con ESP32: OLFATUS_PETROLEUM_V7 de la carpeta PROYECTO EN EL ESP32 (versión 7.3 ESP32 y después 7.3.1). Es el programa del Nano 7.3 con los pines del ESP32, los MQ leídos con analogReadMilliVolts (ADC de 12 bits, hasta ~3,1 V), el switch de combustible guardado en la EEPROM y una sola electroválvula. Las pruebas del ESP32 se guardan en HERRAMIENTAS\logs\serial_esp32.log y van en la hoja "Programa completo ESP32", con sus propios intervalos (hoja Parámetros, segunda tabla). La carpeta del Nano no se tocó.'
    }
    @{
      Programa = 'CompletoESP32'
      Fecha = '12/11/2026'
      Pruebas = '1 a 4 y 5 a 14'
      Tema = 'Referencias de los MQ en el ESP32'
      Texto = 'Con la placa como estaba a las 14:00, las referencias de los MQ daban 2,6-3,0 V, muy cerca del tope del ADC del ESP32 (~3,1 V): en la prueba de las 14:32 el máximo de los dos MQ quedó clavado en 3,100 V (saturado). Entre las 14:34 y las 15:34 se modificó la placa y las referencias bajaron a 0,42-0,53 V (en el Nano daban ~1,0 V). Si se agregó un divisor, falta cargar su factor en FACTOR_DIVISOR_MQ para que los voltajes queden en la misma escala que en el Nano.'
    }
    @{
      Programa = 'CompletoESP32'
      Fecha = '12/11/2026'
      Pruebas = '1 y 2'
      Tema = 'Botón y buzzer del ESP32'
      Texto = 'El botón de la placa del ESP da HIGH suelto y LOW apretado (al revés que el programa del Nano): con el programa sin cambios, la primera extracción después de cada reinicio arrancaba sola (14:13, sin muestra). En la 7.3.1 el botón se lee cada 5 ms con antirrebote de 40 ms y cuenta al apretarlo. El buzzer no se escuchaba con el programa del Nano; se probó HIGH = suena y sonaba todo el tiempo, así que se volvió a LOW = suena, como en el Nano.'
    }
    @{
      Programa = 'CompletoESP32'
      Fecha = '12/11/2026'
      Pruebas = '5 a 14'
      Tema = 'Area135 en el ESP32'
      Texto = 'Las 10 pruebas de la tarde acertaron todas, pero el Area135 votó DUDA DEBAJO en las 10: con la placa modificada dio diesel 1,129-1,324 y nafta 1,251-1,400, todo debajo del piso del Nano (1,40). Además diesel y nafta se superponen: el Area135 es la suma de los 25 voltajes del MQ135 x 0,1 (2,5 x el promedio), así que sigue a la referencia, que varió entre 0,42 y 0,53 V. La nafta de las 16:23 (1,251) tuvo la referencia más baja (0,419 V). Patrón: dividiendo el Area135 por 2,5 x la referencia del MQ135 (promedio de la captura / referencia), el diesel da 0,98-1,01 y la nafta 1,13-1,22, sin superponerse. En la 7.3.2 solo se cambiaron los intervalos del Area135 a 0,65 / 1,33 / 1,34 / 3,50: de las 10, 9 votan bien y la nafta de las 16:23 vota DIESEL; el resultado de las 10 no cambia.'
    }
    @{
      Programa = 'Diferenciador'
      Fecha = '05/11/2026'
      Pruebas = '-'
      Tema = 'Cambios en el diferenciador'
      Texto = 'Desde el 05/11/2026 el DIFERENCIADOR_V7 tiene los mismos cambios de VarBME (techo del diesel 60, piso de la nafta 61), el BME sin el tope de 655 kOhm y la Relación en 1,10 / 1,10 (igual que el programa completo 7.3). En las pruebas del 03/11 el VarBME no estaba en el tope y cae en la misma zona que antes.'
    }
  )
}
