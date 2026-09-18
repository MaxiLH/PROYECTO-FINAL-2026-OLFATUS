/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * MAQUINA DE ESTADOS CENTRAL (Parte 2)
 *
 * Integra:
 *  - Sensores y clasificación (Parte 1 / doc1)  -> VERSION 4 calibrada
 *  - Patrones de LEDs y buzzer (doc2)
 *  - Máquina de estados, electroválvulas y botón (doc5)
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
// PINES - SENSORES DE GAS (doc1)
//=========================================================

const byte PIN_MQ138 = A0;
const byte PIN_MQ135 = A2;

//=========================================================
// PINES - INTERFAZ (doc2)
//=========================================================

#define LED_AZUL 3
#define LED_AMARILLO 4
#define LED_ROJO 5
#define BUZZER 13

//=========================================================
// PINES - ACTUADORES / ENTRADAS (Parte 2)
//=========================================================

const byte PIN_VALVULA_ENTRADA = 8;   // HIGH = abierta
const byte PIN_VALVULA_SALIDA  = 12;  // HIGH = abierta
const byte PIN_BOTON = 6;             // INPUT normal, lógica pulldown (HIGH = presionado)

//=========================================================
// CONFIGURACION - MUESTREO DE GASES (doc1)
//=========================================================

const uint16_t PERIODO_MUESTREO = 200;    // ms, muestreo de MQ durante la extracción
const uint16_t TIEMPO_REFERENCIA = 2000;  // ms, duración de la toma de referencia

// Escala del area: promedio * 2.5, igual que la tabla de
// calibracion. Al normalizar por cantidad de muestras, el
// valor no cambia si se pierde alguna lectura.
const float FACTOR_AREA = 2.5;

//=========================================================
// CONFIGURACION - MAQUINA DE ESTADOS (doc5)
//=========================================================

const float UMBRAL_LIMPIEZA_BME = 220.0;               // kOhm
const uint32_t TIEMPO_CONFIRMACION_LIMPIEZA = 5000UL;   // ms

// ATENCION: los umbrales de abajo se calibraron con capturas
// de 5000 ms. Si se acorta este tiempo, el MQ135 no llega al
// mismo maximo y hay que recalibrar todo de nuevo.
const uint32_t TIEMPO_EXTRACCION = 5000UL;              // ms (también dura la captura de muestra)

const uint32_t TIEMPO_EMERGENCIA = 5000UL;               // ms
const uint32_t TIEMPO_ALERTA_INCORRECTO = 3000UL;        // ms
const float UMBRAL_PRESION = 900.0;                      // hPa (bme.pressure / 100.0)

const bool MODO_DIAGNOSTICO = true;  // imprime features y puntajes al clasificar

//=========================================================
// COMBUSTIBLE CORRECTO (configurable)
//=========================================================
// Puede tomar el valor DIESEL o NAFTA (ver enum Combustible más abajo)

#define COMBUSTIBLE_CORRECTO DIESEL

//=========================================================
// BME
//=========================================================

bool bmeDisponible = false;
unsigned long ultimoIntentoBME = 0;
const uint16_t TIEMPO_REINTENTO_BME = 1000;

//=========================================================
// REFERENCIAS (doc1)
//=========================================================

struct Referencia {
  float mq135;
  float mq138;
  float bme;
};

Referencia referencia;

//=========================================================
// FEATURES (doc1)
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
// VARIABLES AUXILIARES DE CAPTURA (doc1)
//=========================================================

float primerMQ135;
float primerMQ138;
float primerBME;

float mq135_1s;
float mq138_1s;
float bme_1s;

bool pendienteCalculada = false;

float lecturaMQ135 = 0;
float lecturaMQ138 = 0;
float lecturaBME = 0;

float suma135 = 0;
float suma138 = 0;
float sumaBME = 0;

unsigned long tiempoInicioCaptura;
unsigned long ultimoMuestreo;
byte contadorMuestras = 0;

bool bmeValidoEnCaptura = false;

//=========================================================
// RESULTADOS (doc1)
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

Combustible resultadoActual;
MotivoDuda motivo = SIN_MOTIVO;

//=========================================================
// MODELO DE CADA VARIABLE (doc1 - VERSION 4)
//
//  <= dieselMax .......... puntaje -1
//  >= naftaMin ........... puntaje +1
//  entre medio ........... rampa lineal (zona gris)
//
//  fuera de limiteInf..limiteSup -> la variable NO vota
//
//  Rangos medidos (10 muestras de diesel / 10 de nafta):
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
//  No distingue diesel de nafta, pero si distingue un
//  combustible de otra cosa: un compuesto que ataca
//  desproporcionadamente a uno de los dos MQ (alcohol,
//  solventes) se va de este rango.
//=========================================================

const float FIRMA_MIN = 1.65;
const float FIRMA_MAX = 3.40;

//=========================================================
// PESOS VARIABLES (doc1 - VERSION 4)
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
  bool participa;
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
// INTERFAZ - VARIABLES DE PATRONES (doc2)
//=========================================================

byte pasoUI = 0;
unsigned long tiempoUI = 0;

bool beepActivo = false;
bool beepResultadoHecho = false;
unsigned long tiempoBeep = 0;

//=========================================================
// BUZZER DE ALARMA (alerta incorrecto / emergencia)
//=========================================================

bool buzzerAlarmaEncendido = false;
unsigned long tiempoBuzzerAlarma = 0;
const uint16_t PERIODO_BUZZER_ALARMA = 100;

//=========================================================
// MAQUINA DE ESTADOS CENTRAL
//=========================================================

enum class Estado {
  LIMPIEZA_TRAMPA,
  EXTRACCION_LISTA_PARA_HACER,
  EXTRAYENDO,
  ANALIZANDO,
  DUDA,
  DIESEL,
  NAFTA,
  EMERGENCIA
};

Estado estado = Estado::LIMPIEZA_TRAMPA;

// Sub-fase interna de LIMPIEZA_TRAMPA (confirmación de limpieza -> toma de referencia)
enum class FaseLimpieza {
  CONFIRMANDO,
  TOMANDO_REFERENCIA
};

FaseLimpieza faseLimpieza = FaseLimpieza::CONFIRMANDO;

bool confirmandoActivo = false;
unsigned long tiempoInicioConfirmacion = 0;

unsigned long tiempoInicioReferencia = 0;
unsigned long ultimoMuestreoReferencia = 0;
uint16_t muestrasReferencia = 0;
float sumaRef135, sumaRef138, sumaRefBME;

// Alarma de combustible incorrecto
bool alarmaIncorrectoActiva = false;
unsigned long tiempoInicioAlarmaIncorrecto = 0;

// Emergencia
unsigned long tiempoInicioEmergencia = 0;

// Botón (debounce)
bool botonEstadoAnterior = LOW;
unsigned long tiempoUltimoCambioBoton = 0;
const uint16_t DEBOUNCE_BOTON = 50;

//=========================================================
// PROTOTIPOS
//=========================================================

void configurarBME();
void gestionarBME();
void leerSensores();

void iniciarCaptura();
void procesarExtraccion();
void finalizarExtraccion();

void iniciarTomaReferencia();
void procesarTomaReferencia();
void finalizarTomaReferencia();
void procesarConfirmacionLimpieza();

void calcularFeatures();
Voto evaluarVariable(float valor, const Modelo &m);
Combustible decidirFinal();
void imprimirDiagnostico();

void setLED(byte azul, byte amarillo, byte rojo);
void actualizarBeep();
void iniciarBeep();

void patronLimpieza();
void patronListo();
void patronExtraccion();
void patronAnalisis();
void patronDiesel();
void patronNafta();
void patronDuda();
void patronSobrepresion();

void actualizarBuzzerAlarma();
void detenerBuzzerAlarma();

void setElectrovalvulas(bool entrada, bool salida);
bool botonPresionado();

void vigilarPresion();
void cambiarEstado(Estado nuevo);
void entrarEstado(Estado e);
void prepararResultado(Combustible resultado);
void procesarResultado(Combustible resultado);

void setup() {
  Serial.begin(115200);

  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH); // reposo (LOW = sonando, según patronSobrepresion)

  pinMode(PIN_VALVULA_ENTRADA, OUTPUT);
  pinMode(PIN_VALVULA_SALIDA, OUTPUT);

  pinMode(PIN_BOTON, INPUT);

  if (bme.begin()) {
    configurarBME();
    bmeDisponible = true;
  }

  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F(" OLFATUS PETROLEUM "));
  Serial.println(F(" Maquina de estados central "));
  Serial.println(F("==================================="));

  entrarEstado(estado); // inicializa LIMPIEZA_TRAMPA
}

void loop() {
  gestionarBME();
  vigilarPresion(); // prioridad global sobre cualquier estado

  switch (estado) {

    case Estado::LIMPIEZA_TRAMPA:
      Serial.println("LIMPIEZA_TRAMPA");
      patronLimpieza();

      if (faseLimpieza == FaseLimpieza::CONFIRMANDO) {
        Serial.println("PROCESAR_LIMPIEZA");
        procesarConfirmacionLimpieza();
      } else {
        Serial.println("TOMAR_REFERENCIA");
        procesarTomaReferencia();
      }

      break;

    case Estado::EXTRACCION_LISTA_PARA_HACER:
      Serial.println("EXTRACCION_LISTA_PARA_HACER");
      patronListo();

      if (botonPresionado()) {
        cambiarEstado(Estado::EXTRAYENDO);
      }

      break;

    case Estado::EXTRAYENDO:
      Serial.println("EXTRAYENDO");
      patronExtraccion();
      procesarExtraccion();

      break;

    case Estado::ANALIZANDO:
      Serial.println("ANALIZANDO");
      // La clasificación se resuelve de forma instantánea en entrarEstado()
      break;

    case Estado::DUDA:
      Serial.println("DUDA");
      patronDuda();

      if (botonPresionado()) {
        cambiarEstado(Estado::LIMPIEZA_TRAMPA);
      }

      break;

    case Estado::DIESEL:
      Serial.println("DIESEL");
      procesarResultado(DIESEL);
      break;

    case Estado::NAFTA:
      Serial.println("NAFTA");
      procesarResultado(NAFTA);
      break;

    case Estado::EMERGENCIA:
      Serial.println("EMERGENCIA");
      patronSobrepresion();

      if (millis() - tiempoInicioEmergencia >= TIEMPO_EMERGENCIA) {
        cambiarEstado(Estado::LIMPIEZA_TRAMPA);
      }

      break;
  }
}

//=========================================================
// TRANSICIONES DE ESTADO
//=========================================================

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  entrarEstado(estado);
}

void entrarEstado(Estado e) {
  switch (e) {

    case Estado::LIMPIEZA_TRAMPA:
      setElectrovalvulas(false, true); // entrada cerrada, salida abierta

      faseLimpieza = FaseLimpieza::CONFIRMANDO;
      confirmandoActivo = false;

      break;

    case Estado::EXTRACCION_LISTA_PARA_HACER:
      setElectrovalvulas(false, false);
      break;

    case Estado::EXTRAYENDO:
      setElectrovalvulas(true, true);
      iniciarCaptura();
      break;

    case Estado::ANALIZANDO:
      setElectrovalvulas(false, false);
      patronAnalisis();

      calcularFeatures();
      resultadoActual = decidirFinal();

      if (MODO_DIAGNOSTICO) imprimirDiagnostico();

      switch (resultadoActual) {
        case DIESEL: cambiarEstado(Estado::DIESEL); break;
        case NAFTA:  cambiarEstado(Estado::NAFTA);  break;
        case DUDA:   cambiarEstado(Estado::DUDA);   break;
      }

      break;

    case Estado::DUDA:
      setElectrovalvulas(false, false);

      beepActivo = false;
      beepResultadoHecho = false;

      break;

    case Estado::DIESEL:
      setElectrovalvulas(false, false);
      prepararResultado(DIESEL);
      break;

    case Estado::NAFTA:
      setElectrovalvulas(false, false);
      prepararResultado(NAFTA);
      break;

    case Estado::EMERGENCIA:
      setElectrovalvulas(true, true);
      tiempoInicioEmergencia = millis();
      break;
  }
}

void prepararResultado(Combustible resultado) {
  beepActivo = false;
  beepResultadoHecho = false;

  bool esCorrecto = (resultado == COMBUSTIBLE_CORRECTO);

  if (esCorrecto) {
    alarmaIncorrectoActiva = false;
  } else {
    alarmaIncorrectoActiva = true;
    tiempoInicioAlarmaIncorrecto = millis();
    beepResultadoHecho = true; // evita el beep corto normal, se usa la alarma en su lugar
  }
}

void procesarResultado(Combustible resultado) {
  // Mantener el LED del resultado siempre visible
  if (resultado == DIESEL) setLED(0, 255, 0);   // amarillo
  else                     setLED(255, 0, 0);   // azul

  if (alarmaIncorrectoActiva) {
    actualizarBuzzerAlarma();

    if (millis() - tiempoInicioAlarmaIncorrecto >= TIEMPO_ALERTA_INCORRECTO) {
      alarmaIncorrectoActiva = false;
      detenerBuzzerAlarma();
    }

    return; // mientras suena la alarma no se atiende el botón
  }

  if (!beepResultadoHecho) {
    iniciarBeep();
    beepResultadoHecho = true;
  }

  actualizarBeep();

  if (botonPresionado()) {
    cambiarEstado(Estado::LIMPIEZA_TRAMPA);
  }
}

//=========================================================
// VIGILANCIA DE PRESION (global, prioridad absoluta)
//=========================================================

void vigilarPresion() {
  if (!bmeDisponible) return;

  float presionHPa = bme.pressure / 100.0;

  if (presionHPa < UMBRAL_PRESION && estado != Estado::EMERGENCIA) {
    cambiarEstado(Estado::EMERGENCIA);
  }
}

//=========================================================
// LIMPIEZA_TRAMPA: confirmación + toma de referencia
//=========================================================

void procesarConfirmacionLimpieza() {
  if (!bmeDisponible) {
    confirmandoActivo = false;
    return;
  }

  float resistenciaKOhm = bme.gas_resistance / 1000.0;

  if (resistenciaKOhm > UMBRAL_LIMPIEZA_BME) {
    if (!confirmandoActivo) {
      confirmandoActivo = true;
      tiempoInicioConfirmacion = millis();
    } else if (millis() - tiempoInicioConfirmacion >= TIEMPO_CONFIRMACION_LIMPIEZA) {
      iniciarTomaReferencia();
    }
  } else {
    confirmandoActivo = false;
  }
}

void iniciarTomaReferencia() {
  faseLimpieza = FaseLimpieza::TOMANDO_REFERENCIA;

  tiempoInicioReferencia = millis();
  ultimoMuestreoReferencia = 0;
  muestrasReferencia = 0;

  sumaRef135 = 0;
  sumaRef138 = 0;
  sumaRefBME = 0;
}

void procesarTomaReferencia() {
  if (millis() - ultimoMuestreoReferencia < 100) {
    if (millis() - tiempoInicioReferencia >= TIEMPO_REFERENCIA) {
      finalizarTomaReferencia();
    }
    return;
  }

  ultimoMuestreoReferencia = millis();

  leerSensores();

  sumaRef135 += lecturaMQ135;
  sumaRef138 += lecturaMQ138;
  sumaRefBME += lecturaBME;

  muestrasReferencia++;

  if (millis() - tiempoInicioReferencia >= TIEMPO_REFERENCIA) {
    finalizarTomaReferencia();
  }
}

void finalizarTomaReferencia() {
  if (muestrasReferencia > 0) {
    referencia.mq135 = sumaRef135 / muestrasReferencia;
    referencia.mq138 = sumaRef138 / muestrasReferencia;
    referencia.bme = sumaRefBME / muestrasReferencia;
  }

  cambiarEstado(Estado::EXTRACCION_LISTA_PARA_HACER);
}

//=========================================================
// EXTRAYENDO: captura de muestra (adaptado de doc1)
//=========================================================

void iniciarCaptura() {
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

  datos.area135 = 0;
  datos.area138 = 0;
  datos.areaBME = 0;

  suma135 = 0;
  suma138 = 0;
  sumaBME = 0;
}

void procesarExtraccion() {
  if (millis() - ultimoMuestreo < PERIODO_MUESTREO) {
    if (millis() - tiempoInicioCaptura >= TIEMPO_EXTRACCION) {
      finalizarExtraccion();
    }
    return;
  }

  ultimoMuestreo = millis();

  leerSensores();

  if (!bmeDisponible) bmeValidoEnCaptura = false;

  contadorMuestras++;

  if (lecturaMQ135 > datos.max135) datos.max135 = lecturaMQ135;
  if (lecturaMQ138 > datos.max138) datos.max138 = lecturaMQ138;
  if (lecturaBME < datos.minBME) datos.minBME = lecturaBME;

  suma135 += lecturaMQ135;
  suma138 += lecturaMQ138;
  sumaBME += (referencia.bme - lecturaBME);

  if (!pendienteCalculada) {
    if (millis() - tiempoInicioCaptura >= 1000) {
      mq135_1s = lecturaMQ135;
      mq138_1s = lecturaMQ138;
      bme_1s = lecturaBME;

      pendienteCalculada = true;
    }
  }

  if (millis() - tiempoInicioCaptura >= TIEMPO_EXTRACCION) {
    finalizarExtraccion();
  }
}

void finalizarExtraccion() {
  if (contadorMuestras > 0) {
    datos.area135 = (suma135 / contadorMuestras) * FACTOR_AREA;
    datos.area138 = (suma138 / contadorMuestras) * FACTOR_AREA;
    datos.areaBME = (sumaBME / contadorMuestras) * FACTOR_AREA;
  }

  cambiarEstado(Estado::ANALIZANDO);
}

//=========================================================
// BME FUNCIONES DE FUNCIONAMIENTO (doc1)
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
// LECTURA SENSORES (doc1)
//=========================================================

void leerSensores() {
  lecturaMQ138 = analogRead(PIN_MQ138) * (5.0 / 1023.0);
  lecturaMQ135 = analogRead(PIN_MQ135) * (5.0 / 1023.0);

  if (bmeDisponible) {
    lecturaBME = bme.gas_resistance / 1000.0;
  }
}

//=========================================================
// CLASIFICACION (doc1 - VERSION 4)
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

//---------------------------------------------------------
// Puntaje continuo de una variable:
//   -1 = diesel puro, +1 = nafta pura
//   valido = false -> el valor no se parece a ningun
//                     combustible, la variable no vota
//---------------------------------------------------------

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
// DIAGNOSTICO POR SERIAL
//=========================================================

void imprimirDiagnostico() {
  Serial.println();
  Serial.println(F("---- DIAGNOSTICO ----"));

  switch (resultadoActual) {
    case DIESEL: Serial.println(F("RESULTADO: DIESEL")); break;
    case NAFTA:  Serial.println(F("RESULTADO: NAFTA")); break;
    case DUDA:
      Serial.println(F("RESULTADO: DUDA"));

      switch (motivo) {
        case SIN_MUESTRA:        Serial.println(F("No se detecto muestra")); break;
        case FIRMA_INCOMPATIBLE: Serial.println(F("No responde como combustible")); break;
        case FUERA_DE_RANGO:     Serial.println(F("Valores fuera de rango conocido")); break;
        case INCOHERENCIA:       Serial.println(F("Los sensores se contradicen")); break;
        case ZONA_GRIS:          Serial.println(F("Evidencia insuficiente")); break;
        default: break;
      }

      break;
  }

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

//=========================================================
// INTERFAZ - LEDS (doc2)
//=========================================================

void setLED(byte azul, byte amarillo, byte rojo) {
  analogWrite(LED_AZUL, azul);
  analogWrite(LED_AMARILLO, amarillo);
  analogWrite(LED_ROJO, rojo);
}

void patronLimpieza() {
  if (millis() - tiempoUI < 15) return;
  tiempoUI = millis();

  static int brillo = 40;
  static int dir = 1;

  brillo += dir;

  if (brillo >= 255) dir = -1;
  if (brillo <= 40) dir = 1;

  byte azul = brillo;
  byte amarillo = map((brillo + 85) % 255, 0, 255, 40, 255);
  byte rojo = map((brillo + 170) % 255, 0, 255, 40, 255);

  setLED(azul, amarillo, rojo);
}

void patronListo() {
  if (millis() - tiempoUI < 250) return;
  tiempoUI = millis();

  pasoUI = !pasoUI;

  if (pasoUI) setLED(255, 255, 255);
  else setLED(0, 0, 0);
}

void patronExtraccion() {
  if (millis() - tiempoUI < 200) return;
  tiempoUI = millis();

  pasoUI++;
  if (pasoUI > 2) pasoUI = 0;

  switch (pasoUI) {
    case 0: setLED(255, 0, 0); break;
    case 1: setLED(0, 255, 0); break;
    case 2: setLED(0, 0, 255); break;
  }
}

void patronAnalisis() {
  if (millis() - tiempoUI < 90) return;
  tiempoUI = millis();

  pasoUI++;
  if (pasoUI > 2) pasoUI = 0;

  switch (pasoUI) {
    case 0: setLED(255, 0, 0); break;
    case 1: setLED(0, 255, 0); break;
    case 2: setLED(0, 0, 255); break;
  }
}

void patronNafta() {
  setLED(255, 0, 0);

  if (!beepResultadoHecho) {
    iniciarBeep();
    beepResultadoHecho = true;
  }

  actualizarBeep();
}

void patronDiesel() {
  setLED(0, 255, 0);

  if (!beepResultadoHecho) {
    iniciarBeep();
    beepResultadoHecho = true;
  }

  actualizarBeep();
}

void patronDuda() {
  setLED(0, 0, 255);

  if (!beepResultadoHecho) {
    iniciarBeep();
    beepResultadoHecho = true;
  }

  actualizarBeep();
}

void patronSobrepresion() {
  if (millis() - tiempoUI < 100) return;
  tiempoUI = millis();

  pasoUI = !pasoUI;

  if (pasoUI) {
    setLED(0, 0, 255);
    digitalWrite(BUZZER, LOW);
  } else {
    setLED(0, 0, 0);
    digitalWrite(BUZZER, HIGH);
  }
}

//=========================================================
// INTERFAZ - BUZZER (doc2)
//=========================================================

void actualizarBeep() {
  if (!beepActivo) return;

  if (millis() - tiempoBeep >= 80) {
    digitalWrite(BUZZER, HIGH); // apagar (LOW = sonando, HIGH = silencio)
    beepActivo = false;
  }
}

void iniciarBeep() {
  digitalWrite(BUZZER, LOW); // encender (LOW = sonando, HIGH = silencio)
  beepActivo = true;
  tiempoBeep = millis();
}

//=========================================================
// BUZZER DE ALARMA (alerta combustible incorrecto / emergencia)
//=========================================================

void actualizarBuzzerAlarma() {
  if (millis() - tiempoBuzzerAlarma < PERIODO_BUZZER_ALARMA) return;
  tiempoBuzzerAlarma = millis();

  buzzerAlarmaEncendido = !buzzerAlarmaEncendido;
  digitalWrite(BUZZER, buzzerAlarmaEncendido ? LOW : HIGH);
}

void detenerBuzzerAlarma() {
  digitalWrite(BUZZER, HIGH);
  buzzerAlarmaEncendido = false;
}

//=========================================================
// ELECTROVALVULAS
//=========================================================

void setElectrovalvulas(bool entrada, bool salida) {
  digitalWrite(PIN_VALVULA_ENTRADA, entrada ? HIGH : LOW);
  digitalWrite(PIN_VALVULA_SALIDA, salida ? HIGH : LOW);
}

//=========================================================
// BOTON (INPUT normal, pulldown -> HIGH = presionado)
//=========================================================

bool botonPresionado() {
  bool lecturaActual = digitalRead(PIN_BOTON);
  bool presionadoEsteCiclo = false;

  if (lecturaActual != botonEstadoAnterior) {
    if (millis() - tiempoUltimoCambioBoton > DEBOUNCE_BOTON) {
      tiempoUltimoCambioBoton = millis();

      if (lecturaActual == HIGH) {
        presionadoEsteCiclo = true;
      }

      botonEstadoAnterior = lecturaActual;
    }
  }

  return presionadoEsteCiclo;
}
