/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * MAQUINA DE ESTADOS CENTRAL (Parte 2)
 *
 * Integra:
 *  - Sensores y clasificación (Parte 1 / doc1)
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

//=========================================================
// CONFIGURACION - MAQUINA DE ESTADOS (doc5)
//=========================================================

const float UMBRAL_LIMPIEZA_BME = 220.0;               // kOhm
const uint32_t TIEMPO_CONFIRMACION_LIMPIEZA = 5000UL;   // ms
const uint32_t TIEMPO_EXTRACCION = 2000UL;              // ms (también dura la captura de muestra)
const uint32_t TIEMPO_EMERGENCIA = 5000UL;               // ms
const uint32_t TIEMPO_ALERTA_INCORRECTO = 3000UL;        // ms
const float UMBRAL_PRESION = 900.0;                      // hPa (bme.pressure / 100.0)

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

  float relacion;
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

unsigned long tiempoInicioCaptura;
unsigned long ultimoMuestreo;
byte contadorMuestras = 0;

//=========================================================
// RESULTADOS (doc1)
//=========================================================

enum ResultadoVariable {
  VOTO_DIESEL,
  VOTO_DUDA,
  VOTO_NAFTA
};

enum Combustible {
  DIESEL,
  NAFTA,
  DUDA
};

Combustible resultadoActual;

//=========================================================
// UMBRALES (doc1)
//=========================================================

struct IntervaloDecision {
  float dieselMin;
  float dieselMax;

  float naftaMin;
  float naftaMax;
};

IntervaloDecision umbralRelacion = {
  0.44,
  0.52,

  0.70,
  0.82
};

IntervaloDecision umbralArea135 = {
  3.10,
  3.80,

  4.20,
  7.20
};

IntervaloDecision umbralVar135 = {
  30.0,
  65.0,

  95.0,
  230.0
};

IntervaloDecision umbralPendiente135 = {
  0.10,
  0.40,

  0.70,
  2.20
};

//=========================================================
// PESOS VARIABLES (doc1)
//=========================================================

const byte PESO_RELACION   = 2;
const byte PESO_AREA       = 1;
const byte PESO_VARIACION  = 1;
const byte PESO_PENDIENTE  = 1;

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
ResultadoVariable evaluarRelacion();
ResultadoVariable evaluarArea();
ResultadoVariable evaluarVariacion();
ResultadoVariable evaluarPendiente();
Combustible decidirFinal();

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
      patronLimpieza();

      if (faseLimpieza == FaseLimpieza::CONFIRMANDO) {
        procesarConfirmacionLimpieza();
      } else {
        procesarTomaReferencia();
      }

      break;

    case Estado::EXTRACCION_LISTA_PARA_HACER:
      patronListo();

      if (botonPresionado()) {
        cambiarEstado(Estado::EXTRAYENDO);
      }

      break;

    case Estado::EXTRAYENDO:
      patronExtraccion();
      procesarExtraccion();

      break;

    case Estado::ANALIZANDO:
      // La clasificación se resuelve de forma instantánea en entrarEstado()
      break;

    case Estado::DUDA:
      patronDuda();

      if (botonPresionado()) {
        cambiarEstado(Estado::LIMPIEZA_TRAMPA);
      }

      break;

    case Estado::DIESEL:
      procesarResultado(DIESEL);
      break;

    case Estado::NAFTA:
      procesarResultado(NAFTA);
      break;

    case Estado::EMERGENCIA:
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
  ultimoMuestreo = 0;
  contadorMuestras = 0;
  pendienteCalculada = false;

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

  contadorMuestras++;

  if (lecturaMQ135 > datos.max135) datos.max135 = lecturaMQ135;
  if (lecturaMQ138 > datos.max138) datos.max138 = lecturaMQ138;
  if (lecturaBME < datos.minBME) datos.minBME = lecturaBME;

  datos.area135 += lecturaMQ135;
  datos.area138 += lecturaMQ138;
  datos.areaBME += (referencia.bme - lecturaBME);

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
  datos.area135 *= 0.1;
  datos.area138 *= 0.1;
  datos.areaBME *= 0.1;

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
  lecturaBME = bme.gas_resistance / 1000.0;
}

//=========================================================
// CLASIFICACION (doc1)
//=========================================================

void calcularFeatures() {
  datos.variacion135 = ((datos.max135 - referencia.mq135) / referencia.mq135) * 100.0;
  datos.variacion138 = ((datos.max138 - referencia.mq138) / referencia.mq138) * 100.0;

  datos.relacion = datos.max138 / datos.max135;

  datos.pendiente135 = (mq135_1s - primerMQ135);
  datos.pendiente138 = (mq138_1s - primerMQ138);
}

ResultadoVariable evaluarRelacion() {
  if (datos.relacion >= umbralRelacion.dieselMin && datos.relacion <= umbralRelacion.dieselMax) {
    return VOTO_DIESEL;
  }
  if (datos.relacion >= umbralRelacion.naftaMin && datos.relacion <= umbralRelacion.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

ResultadoVariable evaluarVariacion() {
  if (datos.variacion135 >= umbralVar135.dieselMin && datos.variacion135 <= umbralVar135.dieselMax) {
    return VOTO_DIESEL;
  }
  if (datos.variacion135 >= umbralVar135.naftaMin && datos.variacion135 <= umbralVar135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

ResultadoVariable evaluarArea() {
  if (datos.area135 >= umbralArea135.dieselMin && datos.area135 <= umbralArea135.dieselMax) {
    return VOTO_DIESEL;
  }
  if (datos.area135 >= umbralArea135.naftaMin && datos.area135 <= umbralArea135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

ResultadoVariable evaluarPendiente() {
  if (datos.pendiente135 >= umbralPendiente135.dieselMin && datos.pendiente135 <= umbralPendiente135.dieselMax) {
    return VOTO_DIESEL;
  }
  if (datos.pendiente135 >= umbralPendiente135.naftaMin && datos.pendiente135 <= umbralPendiente135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

Combustible decidirFinal() {
  byte votosDiesel = 0;
  byte votosNafta = 0;
  byte votosDuda = 0;

  ResultadoVariable r;

  r = evaluarRelacion();
  if (r == VOTO_DIESEL) votosDiesel += PESO_RELACION;
  else if (r == VOTO_NAFTA) votosNafta += PESO_RELACION;
  else votosDuda += PESO_RELACION;

  r = evaluarVariacion();
  if (r == VOTO_DIESEL) votosDiesel += PESO_VARIACION;
  else if (r == VOTO_NAFTA) votosNafta += PESO_VARIACION;
  else votosDuda += PESO_VARIACION;

  r = evaluarArea();
  if (r == VOTO_DIESEL) votosDiesel += PESO_AREA;
  else if (r == VOTO_NAFTA) votosNafta += PESO_AREA;
  else votosDuda += PESO_AREA;

  r = evaluarPendiente();
  if (r == VOTO_DIESEL) votosDiesel += PESO_PENDIENTE;
  else if (r == VOTO_NAFTA) votosNafta += PESO_PENDIENTE;
  else votosDuda += PESO_PENDIENTE;

  if (votosNafta > votosDiesel && votosNafta > votosDuda) return NAFTA;
  if (votosDiesel > votosNafta && votosDiesel > votosDuda) return DIESEL;

  return DUDA;
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
    digitalWrite(BUZZER, HIGH);
    beepActivo = false;
  }
}

void iniciarBeep() {
  digitalWrite(BUZZER, HIGH);
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
