/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * MAQUINA DE ESTADOS CENTRAL (Parte 2)
 *
 * Integra:
 *  - Analisis de gases VERSION 7 (decision por intervalos sobre
 *    una recta de 5 zonas + umbral de confianza del 80%), adaptado
 *    a captura y referencia NO bloqueantes para no congelar los
 *    LEDs ni la vigilancia de presion durante los ~5-8 segundos
 *    que tarda la captura real.
 *  - Patrones de LEDs y buzzer (doc2)
 *  - Maquina de estados, electrovalvulas y boton (doc5)
 *
 *  NOTA: las 3 variantes de DUDA (por debajo / medio / por encima)
 *  caen todas en el unico estado DUDA de la maquina (mismo LED,
 *  mismo buzzer). El diagnostico impreso despues de cada ciclo
 *  muestra cual de las tres fue.
 *
 *  MQ138 confirmado en A3 (no A0).
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
// PINES - SENSORES DE GAS
//=========================================================

const byte PIN_MQ138 = A3;
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
// CONFIGURACION - CAPTURA Y REFERENCIA DE GASES (v7)
//
// Misma cantidad de muestras y mismas cuentas que el
// adquisidor/diferenciador standalone, pero repartidas de
// forma NO bloqueante en el loop() (sin delay()) para que
// patronExtraccion() y vigilarPresion() sigan corriendo
// durante la captura.
//=========================================================

#define NUM_MUESTRAS 25
#define PERIODO_MUESTREO 200  // ms nominal entre muestras

// Indice usado para la pendiente inicial (igual que el adquisidor)
const byte INDICE_PENDIENTE = 5;
const float TIEMPO_PENDIENTE = (INDICE_PENDIENTE * PERIODO_MUESTREO) / 1000.0;

// El adquisidor hace: area = suma_de_las_25_muestras * 0.1
const float FACTOR_AREA = 0.1;

// Referencia: 20 lecturas, ahora por conteo (no por duracion fija)
const byte N_MUESTRAS_REFERENCIA = 20;
const uint16_t PERIODO_REFERENCIA = 100;  // ms nominal entre lecturas

const bool MODO_DIAGNOSTICO = true;

//=========================================================
// CONFIGURACION - MAQUINA DE ESTADOS (doc5)
//=========================================================

const float UMBRAL_LIMPIEZA_BME = 220.0;               // kOhm
const uint32_t TIEMPO_CONFIRMACION_LIMPIEZA = 6000UL;   // ms
const uint32_t TIEMPO_EMERGENCIA = 5000UL;              // ms
const uint32_t TIEMPO_ALERTA_INCORRECTO = 3000UL;       // ms
const float UMBRAL_PRESION = 900.0;                     // hPa (bme.pressure / 100.0)

//=========================================================
// COMBUSTIBLE CORRECTO (configurable)
//=========================================================
// Puede tomar el valor DIESEL o NAFTA (ver enum Zona más abajo)

#define COMBUSTIBLE_CORRECTO DIESEL

//=========================================================
// BME
//=========================================================

bool bmeDisponible = false;
bool bmeValidoEnCaptura = false;

unsigned long ultimoIntentoBME = 0;
const uint16_t TIEMPO_REINTENTO_BME = 1000;

float temperatura = 0;
float humedad = 0;
float presion = 0;
float resistenciaGas = 0;

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
// CURVAS DE CAPTURA (mismo formato que el adquisidor)
//=========================================================

uint16_t curvaMQ138[NUM_MUESTRAS];
uint16_t curvaMQ135[NUM_MUESTRAS];
uint16_t curvaBME[NUM_MUESTRAS];

unsigned long tiempoInicioCaptura;
unsigned long ultimoMuestreo;
byte contadorMuestras = 0;

unsigned long duracionCaptura = 0;

//=========================================================
// REFERENCIA - VARIABLES DE CAPTURA
//=========================================================

unsigned long ultimoMuestreoReferencia = 0;
byte muestrasReferencia = 0;
float sumaRef135, sumaRef138, sumaRefBME;

//=========================================================
// RESULTADO
//
// Las cinco posiciones de la recta numerica, en orden.
// El mismo tipo se usa para la zona de cada variable, para
// el resultado final del analisis de gases, y para avisar
// a la maquina de estados (que solo distingue DIESEL,
// NAFTA, y "el resto" como DUDA).
//=========================================================

enum Zona {
  DUDA_POR_DEBAJO = 0,
  DIESEL = 1,
  DUDA_MEDIO = 2,
  NAFTA = 3,
  DUDA_POR_ENCIMA = 4
};

const byte N_ZONAS = 5;

const char *nombresZonas[N_ZONAS] = {
  "DUDA DEBAJO",
  "DIESEL     ",
  "DUDA MEDIO ",
  "NAFTA      ",
  "DUDA ENCIMA"
};

Zona resultadoActual = DUDA_MEDIO;

//=========================================================
// MOTIVOS Y AVISOS
//=========================================================

enum Motivo {
  SIN_MOTIVO,
  SIN_MUESTRA,
  SIN_DATOS
};

Motivo motivo = SIN_MOTIVO;

bool avisoFirma = false;
bool avisoDispersion = false;

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
// INTERVALOS DE CADA VARIABLE (v7 - NO TOCAR, calibrados)
//
//  valor <  dieselMin ................ DUDA_POR_DEBAJO
//  dieselMin <= valor <= dieselMax ... DIESEL
//  dieselMax <  valor <  naftaMin .... DUDA_MEDIO
//  naftaMin <= valor <= naftaMax ..... NAFTA
//  valor >  naftaMax ................. DUDA_POR_ENCIMA
//=========================================================

struct Intervalo {
  float dieselMin;
  float dieselMax;
  float naftaMin;
  float naftaMax;
};

//                      dslMin  dslMax  nafMin   nafMax
Intervalo iRelacion = { 0.6, 1.23, 1.24, 1.8 };
Intervalo iVarBME = { -10.00, 50.00, 55.00, 105.00 };
Intervalo iArea135 = { 1.40, 3.3, 3.40, 7.60 };
Intervalo iVar135 = {-5.00, 8.00, 8.50, 950.00 };
Intervalo iPendiente = { 0.00, 0.42, 0.48, 1.90 };

/*/                    dslMin  dslMax  nafMin   nafMax
Intervalo iRelacion = { 0.75, 1.1, 1.2, 1.8 };
Intervalo iVarBME = { -10.00, 60.00, 67.00, 105.00 };
Intervalo iArea135 = { 1.40, 3.95, 4.15, 7.60 };
Intervalo iVar135 = {-5.00, 70.00, 80.00, 950.00 };
Intervalo iPendiente = { 0.00, 0.42, 0.48, 1.90 };
*/
/**
Intervalo iRelacion = { 0.75, 1.1, 1.2, 1.8 };
Intervalo iVarBME = { 0.00, 60.00, 67.00, 105.00 };
Intervalo iArea135 = { 1.40, 3.95, 4.15, 7.60 };
Intervalo iVar135 = { 22.00, 70.00, 80.00, 950.00 };
Intervalo iPendiente = { 0.00, 0.42, 0.48, 1.90 };
*/

//=========================================================
// FIRMA DE COMBUSTIBLE
//
//  variacion138 / variacion135
//
//  No distingue diesel de nafta. Sirve para sospechar que
//  la sustancia no es un combustible. Es solo un aviso, no
//  anula el resultado.
//=========================================================

const float FIRMA_MIN = 1.65;
const float FIRMA_MAX = 3.40;

//=========================================================
// PESOS
//=========================================================

const byte PESO_RELACION = 3;
const byte PESO_VARBME = 2;
const byte PESO_AREA = 1;
const byte PESO_VARIACION = 1;
const byte PESO_PENDIENTE = 1;

//=========================================================
// CRITERIOS
//=========================================================

// Debajo de esta variacion se asume que no hay muestra
const float VARIACION_MINIMA = 15.0;  // %

// Referencia minima del MQ138 para confiar en la firma
const float REF138_MINIMA = 0.05;

// Si la zona ganadora junta menos que esto del peso total
// disponible, NO se toma esa zona como resultado final:
// se cae en DUDA_MEDIO y se avisa que las variables estan
// dispersas.
const float UMBRAL_CONFIANZA = 0.80;

//=========================================================
// VOTOS
//=========================================================

struct Voto {
  Zona zona;
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

byte pesoZona[N_ZONAS];
byte pesoTotal = 0;

float posicionGlobal = 0;  // -2 .. +2
float confianza = 0;       // 0 .. 1

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
// PROTOTIPOS
//=========================================================

void configurarBME();
void gestionarBME();

void iniciarCaptura();
void procesarExtraccion();
void finalizarExtraccion();

void iniciarTomaReferencia();
void procesarTomaReferencia();
void finalizarTomaReferencia();
void procesarConfirmacionLimpieza();

void calcularFeatures();
Zona zonaDe(float valor, const Intervalo &in);
float posicionDe(Zona z);
Zona decidirFinal();

void imprimirRecta();
void imprimirDiagnostico();
void imprimirResultadoGases();

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
void prepararResultado(Zona resultado);
void procesarResultado(Zona resultado);

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

      imprimirResultadoGases();

      switch (resultadoActual) {
        case DIESEL:  cambiarEstado(Estado::DIESEL); break;
        case NAFTA:   cambiarEstado(Estado::NAFTA);  break;
        default:      cambiarEstado(Estado::DUDA);   break;
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

void prepararResultado(Zona resultado) {
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

void procesarResultado(Zona resultado) {
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

//=========================================================
// REFERENCIA - no bloqueante, por conteo (v7)
//
// Misma formula que el adquisidor: analogRead directo para
// los MQ, resistenciaGas cacheado (actualizado por
// gestionarBME(), que ya corre una vez por loop()) para el
// BME, descartando NaN.
//=========================================================

void iniciarTomaReferencia() {
  faseLimpieza = FaseLimpieza::TOMANDO_REFERENCIA;

  ultimoMuestreoReferencia = 0;
  muestrasReferencia = 0;

  sumaRef135 = 0;
  sumaRef138 = 0;
  sumaRefBME = 0;
}

void procesarTomaReferencia() {
  if (millis() - ultimoMuestreoReferencia < PERIODO_REFERENCIA) return;

  ultimoMuestreoReferencia = millis();

  sumaRef138 += analogRead(PIN_MQ138) * (5.0 / 1023.0);
  sumaRef135 += analogRead(PIN_MQ135) * (5.0 / 1023.0);

  if (bmeDisponible && !isnan(resistenciaGas)) {
    sumaRefBME += resistenciaGas;
  }

  muestrasReferencia++;

  if (muestrasReferencia >= N_MUESTRAS_REFERENCIA) {
    finalizarTomaReferencia();
  }
}

void finalizarTomaReferencia() {
  referencia.mq138 = sumaRef138 / N_MUESTRAS_REFERENCIA;
  referencia.mq135 = sumaRef135 / N_MUESTRAS_REFERENCIA;
  referencia.bme = sumaRefBME / N_MUESTRAS_REFERENCIA;

  cambiarEstado(Estado::EXTRACCION_LISTA_PARA_HACER);
}

//=========================================================
// EXTRAYENDO: captura de muestra - no bloqueante, por
// conteo (v7)
//
// Mismas 25 muestras, misma cuantizacion a uint16_t y
// mismo recorte de BME que el adquisidor; en vez de un for
// con delay(), se llena un indice por vez cada vez que
// procesarExtraccion() es llamado y ya paso el periodo.
//=========================================================

void iniciarCaptura() {
  tiempoInicioCaptura = millis();
  ultimoMuestreo = 0;
  contadorMuestras = 0;

  bmeValidoEnCaptura = bmeDisponible;
}

void procesarExtraccion() {
  if (millis() - ultimoMuestreo < PERIODO_MUESTREO) return;

  ultimoMuestreo = millis();

  if (!bmeDisponible) bmeValidoEnCaptura = false;

  float mq138 = analogRead(PIN_MQ138) * (5.0 / 1023.0);
  float mq135 = analogRead(PIN_MQ135) * (5.0 / 1023.0);

  curvaMQ138[contadorMuestras] = (uint16_t)(mq138 * 1000);
  curvaMQ135[contadorMuestras] = (uint16_t)(mq135 * 1000);

  // El uint16_t se desborda arriba de 655.35 kohm. Se recorta
  // para no guardar basura (igual que el adquisidor).
  float gas = resistenciaGas;

  if (isnan(gas) || gas < 0) gas = 0;
  if (gas > 655.0) gas = 655.0;

  curvaBME[contadorMuestras] = (uint16_t)(gas * 100);

  contadorMuestras++;

  if (contadorMuestras >= NUM_MUESTRAS) {
    finalizarExtraccion();
  }
}

void finalizarExtraccion() {
  duracionCaptura = millis() - tiempoInicioCaptura;

  cambiarEstado(Estado::ANALIZANDO);
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
    if (bme.performReading()) {
      temperatura = bme.temperature;
      humedad = bme.humidity;
      presion = bme.pressure / 100.0;
      resistenciaGas = bme.gas_resistance / 1000.0;
    } else {
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
// FEATURES
//
// Mismas cuentas que analizarDatos() del adquisidor / v7.
//=========================================================

void calcularFeatures() {
  datos.max138 = curvaMQ138[0] / 1000.0;
  datos.max135 = curvaMQ135[0] / 1000.0;
  datos.minBME = curvaBME[0] / 100.0;

  float suma138 = 0;
  float suma135 = 0;
  float sumaBME = 0;

  for (byte i = 0; i < NUM_MUESTRAS; i++) {
    float mq138 = curvaMQ138[i] / 1000.0;
    float mq135 = curvaMQ135[i] / 1000.0;
    float gas = curvaBME[i] / 100.0;

    if (mq138 > datos.max138) datos.max138 = mq138;
    if (mq135 > datos.max135) datos.max135 = mq135;
    if (gas < datos.minBME) datos.minBME = gas;

    suma138 += mq138;
    suma135 += mq135;
    sumaBME += (referencia.bme - gas);
  }

  datos.area138 = suma138 * FACTOR_AREA;
  datos.area135 = suma135 * FACTOR_AREA;
  datos.areaBME = sumaBME * FACTOR_AREA;

  //--------------------------
  // VARIACIONES
  //--------------------------

  if (referencia.mq135 > 0.01) {
    datos.variacion135 = ((datos.max135 - referencia.mq135) / referencia.mq135) * 100.0;
  } else {
    datos.variacion135 = 0;
  }

  if (referencia.mq138 > REF138_MINIMA) {
    datos.variacion138 = ((datos.max138 - referencia.mq138) / referencia.mq138) * 100.0;
  } else {
    datos.variacion138 = -1;  // no confiable
  }

  if (bmeValidoEnCaptura && referencia.bme > 1.0) {
    datos.variacionBME = ((referencia.bme - datos.minBME) / referencia.bme) * 100.0;
  } else {
    datos.variacionBME = -999;  // no disponible
  }

  //--------------------------
  // RELACION Y FIRMA
  //--------------------------

  datos.relacion = datos.max138 / datos.max135;

  if (datos.variacion138 > 0 && datos.variacion135 > 1.0) {
    datos.firma = datos.variacion138 / datos.variacion135;
  } else {
    datos.firma = -1;  // no evaluable
  }

  //--------------------------
  // PENDIENTE INICIAL
  //
  // Igual que el adquisidor: diferencia entre la muestra 5
  // y la muestra 0, dividida por el tiempo nominal entre
  // esas dos muestras.
  //--------------------------

  datos.pendiente138 = ((curvaMQ138[INDICE_PENDIENTE] / 1000.0) - (curvaMQ138[0] / 1000.0)) / TIEMPO_PENDIENTE;

  datos.pendiente135 = ((curvaMQ135[INDICE_PENDIENTE] / 1000.0) - (curvaMQ135[0] / 1000.0)) / TIEMPO_PENDIENTE;
}

//=========================================================
// UBICACION EN LA RECTA
//=========================================================

Zona zonaDe(float valor, const Intervalo &in) {
  if (valor < in.dieselMin) return DUDA_POR_DEBAJO;
  if (valor <= in.dieselMax) return DIESEL;
  if (valor < in.naftaMin) return DUDA_MEDIO;
  if (valor <= in.naftaMax) return NAFTA;

  return DUDA_POR_ENCIMA;
}

// Posicion numerica de cada zona: -2, -1, 0, +1, +2
float posicionDe(Zona z) {
  return (float)((int)z - 2);
}

//=========================================================
// DECISION
//
// Cada variable ubica la muestra en una zona de la recta.
// Gana la zona que junta mas peso. Si hay empate, gana la
// zona mas cercana a la posicion promedio ponderada.
//
// La zona ganadora solo se devuelve como resultado si junta
// al menos UMBRAL_CONFIANZA del peso total disponible. Si
// no llega, se devuelve DUDA_MEDIO en su lugar.
//=========================================================

Zona decidirFinal() {
  motivo = SIN_MOTIVO;
  avisoFirma = false;
  avisoDispersion = false;

  pesoTotal = 0;
  posicionGlobal = 0;
  confianza = 0;

  for (byte z = 0; z < N_ZONAS; z++) pesoZona[z] = 0;

  for (byte i = 0; i < N_VARIABLES; i++) {
    votos[i].zona = DUDA_MEDIO;
    votos[i].participa = false;
  }

  valoresVariables[0] = datos.relacion;
  valoresVariables[1] = datos.variacionBME;
  valoresVariables[2] = datos.area135;
  valoresVariables[3] = datos.variacion135;
  valoresVariables[4] = datos.pendiente135;

  byte pesos[N_VARIABLES] = {
    PESO_RELACION,
    PESO_VARBME,
    PESO_AREA,
    PESO_VARIACION,
    PESO_PENDIENTE
  };

  //-----------------------
  // 1) Zona de cada variable
  //
  //    Solo se abstiene la variable que no esta
  //    disponible. Quedar fuera de rango YA NO es
  //    abstenerse: es votar DEBAJO o ENCIMA.
  //-----------------------

  votos[0].zona = zonaDe(datos.relacion, iRelacion);
  votos[0].participa = true;

  if (bmeValidoEnCaptura && referencia.bme > 1.0) {
    votos[1].zona = zonaDe(datos.variacionBME, iVarBME);
    votos[1].participa = true;
  }

  votos[2].zona = zonaDe(datos.area135, iArea135);
  votos[2].participa = true;

  votos[3].zona = zonaDe(datos.variacion135, iVar135);
  votos[3].participa = true;

  votos[4].zona = zonaDe(datos.pendiente135, iPendiente);
  votos[4].participa = true;

  //-----------------------
  // 2) Reparto de pesos
  //-----------------------

  float sumaPosicion = 0;

  for (byte i = 0; i < N_VARIABLES; i++) {
    if (!votos[i].participa) continue;

    pesoZona[votos[i].zona] += pesos[i];
    pesoTotal += pesos[i];

    sumaPosicion += posicionDe(votos[i].zona) * pesos[i];
  }

  if (pesoTotal == 0) {
    motivo = SIN_DATOS;
    return DUDA_MEDIO;
  }

  posicionGlobal = sumaPosicion / pesoTotal;

  //-----------------------
  // 3) Hay muestra?
  //-----------------------

  if (datos.variacion135 < VARIACION_MINIMA) {
    motivo = SIN_MUESTRA;
    return DUDA_POR_DEBAJO;
  }

  //-----------------------
  // 4) Firma de combustible (solo aviso)
  //-----------------------

  if (datos.firma > 0) {
    if (datos.firma < FIRMA_MIN || datos.firma > FIRMA_MAX) {
      avisoFirma = true;
    }
  }

  //-----------------------
  // 5) Zona ganadora
  //-----------------------

  byte mejor = 0;

  for (byte z = 1; z < N_ZONAS; z++) {
    if (pesoZona[z] > pesoZona[mejor]) {
      mejor = z;
    } else if (pesoZona[z] == pesoZona[mejor] && pesoZona[z] > 0) {
      float distanciaActual = fabs(posicionGlobal - posicionDe((Zona)mejor));
      float distanciaNueva = fabs(posicionGlobal - posicionDe((Zona)z));

      if (distanciaNueva < distanciaActual) mejor = z;
    }
  }

  confianza = (float)pesoZona[mejor] / (float)pesoTotal;

  //-----------------------
  // 6) Filtro de confianza: si la zona ganadora no junta
  //    al menos UMBRAL_CONFIANZA del peso total, no se
  //    devuelve esa zona. Se cae en DUDA_MEDIO.
  //-----------------------

  if (confianza < UMBRAL_CONFIANZA) {
    avisoDispersion = true;
    return DUDA_MEDIO;
  }

  return (Zona)mejor;
}

//=========================================================
// SALIDA - diagnostico de gases
//
// Mismo contenido que imprime la version standalone v7,
// llamado una vez por ciclo de evaluacion (al entrar a
// Estado::ANALIZANDO), antes de pasar a DIESEL/NAFTA/DUDA.
//=========================================================

void imprimirRecta() {
  Serial.println();
  Serial.println(F("Peso por zona de la recta:"));

  for (byte z = 0; z < N_ZONAS; z++) {
    Serial.print(F("  "));
    Serial.print(nombresZonas[z]);
    Serial.print(F(" : "));
    Serial.print(pesoZona[z]);

    if ((Zona)z == resultadoActual) Serial.print(F("   <== ganadora"));

    Serial.println();
  }

  Serial.print(F("Posicion global (-2..+2) = "));
  Serial.println(posicionGlobal, 2);

  Serial.print(F("Confianza = "));
  Serial.print(confianza * 100.0, 0);
  Serial.print(F(" %  (umbral "));
  Serial.print(UMBRAL_CONFIANZA * 100.0, 0);
  Serial.println(F(" %)"));
}

void imprimirDiagnostico() {
  Serial.println();
  Serial.println(F("---- DIAGNOSTICO ----"));

  Serial.print(F("Muestras = "));
  Serial.print(NUM_MUESTRAS);
  Serial.print(F("  en "));
  Serial.print(duracionCaptura);
  Serial.println(F(" ms"));

  Serial.print(F("Ref  135/138/BME = "));
  Serial.print(referencia.mq135, 3);
  Serial.print(F(" / "));
  Serial.print(referencia.mq138, 3);
  Serial.print(F(" / "));
  Serial.println(referencia.bme, 1);

  Serial.print(F("Max  135/138     = "));
  Serial.print(datos.max135, 3);
  Serial.print(F(" / "));
  Serial.println(datos.max138, 3);

  Serial.print(F("Var138 = "));
  Serial.print(datos.variacion138, 1);
  Serial.print(F("   Firma = "));
  Serial.println(datos.firma, 2);

  Serial.println();

  for (byte i = 0; i < N_VARIABLES; i++) {
    Serial.print(nombresVariables[i]);
    Serial.print(F(" = "));
    Serial.print(valoresVariables[i], 3);
    Serial.print(F("  -> "));

    if (!votos[i].participa) {
      Serial.println(F("no disponible"));
    } else {
      Serial.println(nombresZonas[votos[i].zona]);
    }
  }

  imprimirRecta();

  Serial.println(F("---------------------"));
}

void imprimirResultadoGases() {
  Serial.println();
  Serial.println(F("============================"));
  Serial.println(F("RESULTADO"));

  switch (resultadoActual) {

    case DIESEL:
      Serial.println(F("DIESEL"));
      break;

    case NAFTA:
      Serial.println(F("NAFTA"));
      break;

    case DUDA_POR_DEBAJO:
      Serial.println(F("DUDA POR DEBAJO"));

      if (motivo == SIN_MUESTRA) {
        Serial.println(F("No se detecto muestra"));
      } else {
        Serial.println(F("Los valores quedan por debajo"));
        Serial.println(F("del intervalo del diesel"));
      }

      break;

    case DUDA_MEDIO:
      Serial.println(F("DUDA MEDIO"));

      if (motivo == SIN_DATOS) {
        Serial.println(F("Ninguna variable disponible"));
      } else if (avisoDispersion) {
        Serial.println(F("No se alcanzo el 80% de"));
        Serial.println(F("confianza en una sola zona"));
      } else {
        Serial.println(F("Los valores quedan entre el"));
        Serial.println(F("diesel y la nafta"));
      }

      break;

    case DUDA_POR_ENCIMA:
      Serial.println(F("DUDA POR ENCIMA"));
      Serial.println(F("Los valores quedan por encima"));
      Serial.println(F("del intervalo de la nafta"));
      break;
  }

  if (avisoFirma) {
    Serial.println();
    Serial.println(F("AVISO: la firma no responde"));
    Serial.println(F("como un combustible"));
  }

  if (avisoDispersion) {
    Serial.println();
    Serial.println(F("AVISO: las variables estan"));
    Serial.println(F("dispersas, evidencia debil"));
  }

  Serial.println(F("============================"));

  if (MODO_DIAGNOSTICO) imprimirDiagnostico();
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
