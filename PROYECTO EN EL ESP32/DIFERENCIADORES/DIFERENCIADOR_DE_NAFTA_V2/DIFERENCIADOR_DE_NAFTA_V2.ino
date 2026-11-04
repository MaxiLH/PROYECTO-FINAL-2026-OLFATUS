/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * VERSION 4 - Calibrada con 10 muestras de diesel y 10 de nafta
 *
 * Santino Leguizamo
 * Constantino Guagnini
 * Maximiliano Huang
 * Ivan Musto
 **********************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

#define SEALEVELPRESSURE_HPA 1013.25

Adafruit_BME680 bme(&Wire);

//=========================================================
// PINES
//=========================================================

const byte PIN_MQ138 = A3;
const byte PIN_MQ135 = A2;

//=========================================================
// CONFIGURACION
//=========================================================

const uint16_t PERIODO_MUESTREO = 200;    // ms
const uint16_t TIEMPO_REFERENCIA = 2000;  // ms
const uint16_t TIEMPO_CAPTURA = 5000;     // ms

// Escala de area: mantiene los valores en la misma escala
// de la tabla de calibracion (promedio * 2.5), sin importar
// cuantas muestras entraron realmente en la captura.
const float FACTOR_AREA = 2.5;

const bool MODO_DIAGNOSTICO = true;

//=========================================================
// BME
//=========================================================

bool bmeDisponible = false;
unsigned long ultimoIntentoBME = 0;
const uint16_t TIEMPO_REINTENTO_BME = 1000;

//=========================================================
// REFERENCIAS
//=========================================================

struct Referencia {
  float mq135;
  float mq138;
  float bme;
};

Referencia referencia;

//=========================================================
// FEATURES
//=========================================================

struct Features {
  float max135;
  float max138;
  float minBME;

  float area135;
  float area138;
  float areaBME;

  float pendiente135;
  float pendiente138;

  float variacion135;
  float variacion138;
  float variacionBME;

  float relacion;
  float firma;  // variacion138 / variacion135
};

Features datos;

//=========================================================
// ACUMULADORES DE CAPTURA
//=========================================================

float suma135 = 0;
float suma138 = 0;
float sumaBME = 0;

//=========================================================
// VARIABLES AUXILIARES
//=========================================================

float primerMQ135;
float primerMQ138;
float primerBME;

float mq135_1s;
float mq138_1s;
float bme_1s;

bool pendienteCalculada = false;

//=========================================================
// LECTURAS ACTUALES
//=========================================================

float lecturaMQ135 = 0;
float lecturaMQ138 = 0;
float lecturaBME = 0;

//=========================================================
// VARIABLES INTERNAS
//=========================================================

unsigned long tiempoInicioCaptura;
unsigned long ultimoMuestreo;

byte contadorMuestras = 0;

bool bmeValidoEnCaptura = false;

//=========================================================
// RESULTADOS
//=========================================================

enum Combustible {
  DIESEL,
  NAFTA,
  DUDA
};

enum MotivoDuda {
  SIN_MOTIVO,
  SIN_MUESTRA,
  FIRMA_INCOMPATIBLE,
  FUERA_DE_RANGO,
  INCOHERENCIA,
  ZONA_GRIS
};

MotivoDuda motivo = SIN_MOTIVO;

//=========================================================
// MAQUINA DE ESTADOS
//=========================================================

enum Estado {
  ESPERANDO_REFERENCIA,
  TOMANDO_REFERENCIA,
  ESPERANDO_INICIO_CAPTURA,
  CAPTURANDO,
  CALCULANDO,
  CLASIFICANDO,
  MOSTRANDO
};

Estado estado = ESPERANDO_REFERENCIA;

//=========================================================
// MODELO DE CADA VARIABLE
//
//  <= dieselMax .......... puntaje -1
//  >= naftaMin ........... puntaje +1
//  entre medio ........... rampa lineal (zona gris)
//
//  fuera de limiteInf..limiteSup -> la variable NO vota
//
//  Rangos medidos (10 diesel / 10 nafta):
//
//    relacion   diesel 0.46 - 0.48    nafta 0.72 - 1.00
//    area135    diesel 3.25 - 3.66    nafta 4.39 - 6.19
//    var135     diesel 35.7 - 57.4    nafta 108  - 205
//    pend135    diesel 0.13 - 0.34    nafta 0.53 - 1.47
//    varBME     diesel 33.0 - 52.0    nafta 73.4 - 90.3
//=========================================================

struct Modelo {
  float limiteInf;
  float dieselMax;
  float naftaMin;
  float limiteSup;
};

//                   limInf  dieselMax  naftaMin  limSup
Modelo mRelacion  = { 0.36,   0.55,      0.66,     1.15 };
Modelo mArea135   = { 2.40,   3.95,      4.15,     7.60 };
Modelo mVar135    = { 22.0,   70.0,      95.0,     265.0 };
Modelo mPendiente = { 0.05,   0.42,      0.48,     1.90 };
Modelo mVarBME    = { 22.0,   60.0,      67.0,     96.0 };

//=========================================================
// FIRMA DE COMBUSTIBLE
//
//  variacion138 / variacion135
//
//  Medido:  diesel 2.00 - 2.32    nafta 2.12 - 2.78
//
//  No sirve para distinguir diesel de nafta, pero si para
//  distinguir un combustible de otra cosa: un compuesto que
//  ataca desproporcionadamente a uno de los dos MQ (alcohol,
//  solventes) se va de este rango.
//=========================================================

const float FIRMA_MIN = 1.65;
const float FIRMA_MAX = 3.40;

//=========================================================
// PESOS
//=========================================================

const byte PESO_RELACION  = 3;
const byte PESO_VARBME    = 2;
const byte PESO_AREA      = 1;
const byte PESO_VARIACION = 1;
const byte PESO_PENDIENTE = 1;

//=========================================================
// CRITERIOS DE DECISION
//=========================================================

// Fraccion del peso disponible que tiene que haber votado
const float FRACCION_MINIMA_VALIDA = 0.66;

// Cuanto tiene que inclinarse el puntaje global (-1 .. +1)
const float UMBRAL_CONFIANZA = 0.40;

// Peso que, votando fuerte en contra, invalida la decision
const byte PESO_CONTRADICCION = 2;

// Debajo de esta variacion se asume que no hay muestra
const float VARIACION_MINIMA = 15.0;  // %

// Referencia minima del MQ138 para confiar en la firma
const float REF138_MINIMA = 0.05;

//=========================================================
// VOTOS
//=========================================================

struct Voto {
  float puntaje;
  bool valido;
  bool participa;  // false = la variable no estaba disponible
};

const byte N_VARIABLES = 5;

Voto votos[N_VARIABLES];

const char *nombresVariables[N_VARIABLES] = {
  "Relacion",
  "VarBME  ",
  "Area135 ",
  "Var135  ",
  "Pend135 "
};

float valoresVariables[N_VARIABLES];

float puntajeGlobal = 0;
byte pesoValido = 0;
byte pesoDisponible = 0;

//=========================================================
// PROTOTIPOS
//=========================================================

void configurarBME();
void gestionarBME();
void leerSensores();
void obtenerReferencia();
void iniciarCaptura();
void actualizarCaptura();
void calcularFeatures();

Voto evaluarVariable(float valor, const Modelo &m);
Combustible decidirFinal();

void imprimirDiagnostico();
void imprimirResultado();
void resetCaptura();

void setup() {
  Serial.begin(115200);

  while (!Serial)
    ;

  if (bme.begin()) {
    configurarBME();
    bmeDisponible = true;
  }

  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F(" OLFATUS PETROLEUM "));
  Serial.println(F(" Clasificador Diesel / Nafta "));
  Serial.println(F("==================================="));
  Serial.println();
  Serial.println(F("Envie cualquier caracter"));
  Serial.println(F("para comenzar."));
}

void loop() {
  gestionarBME();

  switch (estado) {

    case ESPERANDO_REFERENCIA:

      if (Serial.available()) {
        while (Serial.available())
          Serial.read();

        estado = TOMANDO_REFERENCIA;
      }

      break;

    case TOMANDO_REFERENCIA:

      obtenerReferencia();

      Serial.println();
      Serial.println(F("============================"));
      Serial.println(F("Referencia obtenida."));
      Serial.println(F("Prepare la muestra."));
      Serial.println(F("Envie cualquier caracter"));
      Serial.println(F("para comenzar la deteccion."));
      Serial.println(F("============================"));

      estado = ESPERANDO_INICIO_CAPTURA;

      break;

    case ESPERANDO_INICIO_CAPTURA:

      if (Serial.available()) {
        while (Serial.available())
          Serial.read();

        iniciarCaptura();

        estado = CAPTURANDO;
      }

      break;

    case CAPTURANDO:

      actualizarCaptura();

      break;

    case CALCULANDO:

      calcularFeatures();

      estado = CLASIFICANDO;

      break;

    case CLASIFICANDO:

      imprimirResultado();

      estado = MOSTRANDO;

      break;

    case MOSTRANDO:

      if (Serial.available()) {
        while (Serial.available())
          Serial.read();

        resetCaptura();

        estado = TOMANDO_REFERENCIA;
      }

      break;
  }
}

//=========================================================
// BME
//=========================================================

void configurarBME() {
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);
}

void gestionarBME() {
  if (bmeDisponible) {
    if (!bme.performReading()) {
      bmeDisponible = false;
    }
  } else {
    if (millis() - ultimoIntentoBME >= TIEMPO_REINTENTO_BME) {
      ultimoIntentoBME = millis();

      if (bme.begin()) {
        configurarBME();
        bmeDisponible = true;
      }
    }
  }
}

//=========================================================
// LECTURA SENSORES
//=========================================================

void leerSensores() {
  lecturaMQ138 = analogRead(PIN_MQ138) * (5.0 / 1023.0);
  lecturaMQ135 = analogRead(PIN_MQ135) * (5.0 / 1023.0);

  if (bmeDisponible) {
    lecturaBME = bme.gas_resistance / 1000.0;
  }
}

//=========================================================
// REFERENCIA
//=========================================================

void obtenerReferencia() {
  Serial.println();
  Serial.println(F("Tomando referencia..."));

  float s135 = 0;
  float s138 = 0;
  float sBME = 0;

  const byte N = TIEMPO_REFERENCIA / 100;

  for (byte i = 0; i < N; i++) {
    gestionarBME();
    leerSensores();

    s135 += lecturaMQ135;
    s138 += lecturaMQ138;
    sBME += lecturaBME;

    delay(100);
  }

  referencia.mq135 = s135 / N;
  referencia.mq138 = s138 / N;
  referencia.bme = sBME / N;
}

//=========================================================
// CAPTURA
//=========================================================

void iniciarCaptura() {
  Serial.println();
  Serial.println(F("Capturando..."));

  tiempoInicioCaptura = millis();
  ultimoMuestreo = millis();

  contadorMuestras = 0;

  pendienteCalculada = false;

  bmeValidoEnCaptura = bmeDisponible;

  leerSensores();

  primerMQ135 = lecturaMQ135;
  primerMQ138 = lecturaMQ138;
  primerBME = lecturaBME;

  datos.max135 = lecturaMQ135;
  datos.max138 = lecturaMQ138;
  datos.minBME = lecturaBME;

  suma135 = 0;
  suma138 = 0;
  sumaBME = 0;
}

void actualizarCaptura() {
  if (millis() - ultimoMuestreo < PERIODO_MUESTREO) return;

  ultimoMuestreo = millis();

  gestionarBME();
  leerSensores();

  if (!bmeDisponible) bmeValidoEnCaptura = false;

  contadorMuestras++;

  //--------------------------
  // MAXIMOS
  //--------------------------

  if (lecturaMQ135 > datos.max135) datos.max135 = lecturaMQ135;
  if (lecturaMQ138 > datos.max138) datos.max138 = lecturaMQ138;
  if (lecturaBME < datos.minBME) datos.minBME = lecturaBME;

  //--------------------------
  // ACUMULADOS
  //--------------------------

  suma135 += lecturaMQ135;
  suma138 += lecturaMQ138;
  sumaBME += (referencia.bme - lecturaBME);

  //--------------------------
  // PENDIENTE
  //--------------------------

  if (!pendienteCalculada) {
    if (millis() - tiempoInicioCaptura >= 1000) {
      mq135_1s = lecturaMQ135;
      mq138_1s = lecturaMQ138;
      bme_1s = lecturaBME;

      pendienteCalculada = true;
    }
  }

  //--------------------------
  // FIN CAPTURA
  //--------------------------

  if (millis() - tiempoInicioCaptura >= TIEMPO_CAPTURA) {
    if (contadorMuestras > 0) {
      datos.area135 = (suma135 / contadorMuestras) * FACTOR_AREA;
      datos.area138 = (suma138 / contadorMuestras) * FACTOR_AREA;
      datos.areaBME = (sumaBME / contadorMuestras) * FACTOR_AREA;
    }

    estado = CALCULANDO;
  }
}

//=========================================================
// FEATURES
//=========================================================

void calcularFeatures() {
  datos.variacion135 = ((datos.max135 - referencia.mq135) / referencia.mq135) * 100.0;

  if (referencia.mq138 > REF138_MINIMA) {
    datos.variacion138 = ((datos.max138 - referencia.mq138) / referencia.mq138) * 100.0;
  } else {
    datos.variacion138 = -1;  // no confiable
  }

  if (bmeValidoEnCaptura && referencia.bme > 1.0) {
    datos.variacionBME = ((referencia.bme - datos.minBME) / referencia.bme) * 100.0;
  } else {
    datos.variacionBME = -1;
  }

  datos.relacion = datos.max138 / datos.max135;

  if (datos.variacion138 > 0 && datos.variacion135 > 1.0) {
    datos.firma = datos.variacion138 / datos.variacion135;
  } else {
    datos.firma = -1;  // no evaluable
  }

  if (pendienteCalculada) {
    datos.pendiente135 = (mq135_1s - primerMQ135);
    datos.pendiente138 = (mq138_1s - primerMQ138);
  } else {
    datos.pendiente135 = -999;  // fuerza abstencion
    datos.pendiente138 = -999;
  }
}

//=========================================================
// EVALUACION CONTINUA
//=========================================================

Voto evaluarVariable(float valor, const Modelo &m) {
  Voto v;

  v.puntaje = 0;
  v.valido = true;
  v.participa = true;

  if (valor < m.limiteInf || valor > m.limiteSup) {
    v.valido = false;
    return v;
  }

  if (valor <= m.dieselMax) {
    v.puntaje = -1.0;
    return v;
  }

  if (valor >= m.naftaMin) {
    v.puntaje = 1.0;
    return v;
  }

  float t = (valor - m.dieselMax) / (m.naftaMin - m.dieselMax);

  v.puntaje = (2.0 * t) - 1.0;

  return v;
}

//=========================================================
// DECISION
//=========================================================

Combustible decidirFinal() {
  motivo = SIN_MOTIVO;

  puntajeGlobal = 0;
  pesoValido = 0;
  pesoDisponible = 0;

  for (byte i = 0; i < N_VARIABLES; i++) {
    votos[i].puntaje = 0;
    votos[i].valido = false;
    votos[i].participa = false;
  }

  valoresVariables[0] = datos.relacion;
  valoresVariables[1] = datos.variacionBME;
  valoresVariables[2] = datos.area135;
  valoresVariables[3] = datos.variacion135;
  valoresVariables[4] = datos.pendiente135;

  //-----------------------
  // 1) Hay muestra?
  //-----------------------

  if (datos.variacion135 < VARIACION_MINIMA) {
    motivo = SIN_MUESTRA;
    return DUDA;
  }

  //-----------------------
  // 2) Firma de combustible
  //-----------------------

  if (datos.firma > 0) {
    if (datos.firma < FIRMA_MIN || datos.firma > FIRMA_MAX) {
      motivo = FIRMA_INCOMPATIBLE;
      return DUDA;
    }
  }

  //-----------------------
  // 3) Puntaje de cada variable
  //-----------------------

  byte pesos[N_VARIABLES] = {
    PESO_RELACION,
    PESO_VARBME,
    PESO_AREA,
    PESO_VARIACION,
    PESO_PENDIENTE
  };

  votos[0] = evaluarVariable(datos.relacion, mRelacion);

  if (datos.variacionBME >= 0) {
    votos[1] = evaluarVariable(datos.variacionBME, mVarBME);
  }

  votos[2] = evaluarVariable(datos.area135, mArea135);
  votos[3] = evaluarVariable(datos.variacion135, mVar135);
  votos[4] = evaluarVariable(datos.pendiente135, mPendiente);

  //-----------------------
  // 4) Acumulacion ponderada
  //-----------------------

  float suma = 0;

  byte pesoFuerteDiesel = 0;
  byte pesoFuerteNafta = 0;

  for (byte i = 0; i < N_VARIABLES; i++) {
    if (!votos[i].participa) continue;

    pesoDisponible += pesos[i];

    if (!votos[i].valido) continue;

    suma += votos[i].puntaje * pesos[i];

    pesoValido += pesos[i];

    if (votos[i].puntaje <= -0.6) pesoFuerteDiesel += pesos[i];
    if (votos[i].puntaje >= 0.6) pesoFuerteNafta += pesos[i];
  }

  //-----------------------
  // 5) Alcanza la evidencia?
  //-----------------------

  if (pesoValido < (pesoDisponible * FRACCION_MINIMA_VALIDA)) {
    motivo = FUERA_DE_RANGO;
    return DUDA;
  }

  puntajeGlobal = suma / pesoValido;

  //-----------------------
  // 6) Se contradicen entre si?
  //-----------------------

  byte contradiccion = (pesoFuerteDiesel < pesoFuerteNafta) ? pesoFuerteDiesel : pesoFuerteNafta;

  if (contradiccion >= PESO_CONTRADICCION) {
    motivo = INCOHERENCIA;
    return DUDA;
  }

  //-----------------------
  // 7) Decision por confianza
  //-----------------------

  if (puntajeGlobal <= -UMBRAL_CONFIANZA) return DIESEL;

  if (puntajeGlobal >= UMBRAL_CONFIANZA) return NAFTA;

  motivo = ZONA_GRIS;

  return DUDA;
}

//=========================================================
// SALIDA
//=========================================================

void imprimirDiagnostico() {
  Serial.println();
  Serial.println(F("---- DIAGNOSTICO ----"));

  Serial.print(F("Ref  135/138/BME = "));
  Serial.print(referencia.mq135, 2);
  Serial.print(F(" / "));
  Serial.print(referencia.mq138, 2);
  Serial.print(F(" / "));
  Serial.println(referencia.bme, 1);

  Serial.print(F("Max  135/138     = "));
  Serial.print(datos.max135, 2);
  Serial.print(F(" / "));
  Serial.println(datos.max138, 2);

  Serial.print(F("Var138 = "));
  Serial.print(datos.variacion138, 1);
  Serial.print(F("   Firma = "));
  Serial.println(datos.firma, 2);

  Serial.print(F("Muestras = "));
  Serial.println(contadorMuestras);

  for (byte i = 0; i < N_VARIABLES; i++) {
    Serial.print(nombresVariables[i]);
    Serial.print(F(" = "));
    Serial.print(valoresVariables[i], 3);
    Serial.print(F("  -> "));

    if (!votos[i].participa) {
      Serial.println(F("no disponible"));
    } else if (!votos[i].valido) {
      Serial.println(F("FUERA DE RANGO (no vota)"));
    } else {
      Serial.println(votos[i].puntaje, 2);
    }
  }

  Serial.print(F("Peso valido = "));
  Serial.print(pesoValido);
  Serial.print(F(" / "));
  Serial.println(pesoDisponible);

  Serial.print(F("Puntaje global = "));
  Serial.println(puntajeGlobal, 3);

  Serial.println(F("---------------------"));
}

void imprimirResultado() {
  Combustible resultado = decidirFinal();

  Serial.println();
  Serial.println(F("============================"));
  Serial.println(F("RESULTADO"));

  switch (resultado) {
    case DIESEL:
      Serial.println(F("DIESEL"));
      break;

    case NAFTA:
      Serial.println(F("NAFTA"));
      break;

    case DUDA:
      Serial.println(F("DUDA"));

      switch (motivo) {
        case SIN_MUESTRA:
          Serial.println(F("No se detecto muestra"));
          break;

        case FIRMA_INCOMPATIBLE:
          Serial.println(F("La sustancia no responde"));
          Serial.println(F("como un combustible"));
          break;

        case FUERA_DE_RANGO:
          Serial.println(F("Valores fuera de rango"));
          Serial.println(F("conocido"));
          break;

        case INCOHERENCIA:
          Serial.println(F("Los sensores se contradicen"));
          break;

        case ZONA_GRIS:
          Serial.println(F("Evidencia insuficiente"));
          break;

        default:
          break;
      }

      Serial.println(F("Repetir medicion"));
      break;
  }

  Serial.println(F("============================"));

  if (MODO_DIAGNOSTICO) imprimirDiagnostico();

  Serial.println();
  Serial.println(F("Envie cualquier caracter"));
  Serial.println(F("para repetir."));
}

//=========================================================
// RESET
//=========================================================

void resetCaptura() {
  memset(&datos, 0, sizeof(datos));

  primerMQ135 = 0;
  primerMQ138 = 0;
  primerBME = 0;

  mq135_1s = 0;
  mq138_1s = 0;
  bme_1s = 0;

  suma135 = 0;
  suma138 = 0;
  sumaBME = 0;

  contadorMuestras = 0;

  pendienteCalculada = false;

  motivo = SIN_MOTIVO;

  puntajeGlobal = 0;
  pesoValido = 0;
  pesoDisponible = 0;
}