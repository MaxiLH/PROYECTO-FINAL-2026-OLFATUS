/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 * Máquina de estados central
 *
 * Arduino Nano
 *
 * SIN DELAY()
 **********************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

//=========================================================
// BME680
//=========================================================

#define SEALEVELPRESSURE_HPA 1013.25

Adafruit_BME680 bme(&Wire);

//=========================================================
// PINES SENSORES
//=========================================================

const byte PIN_MQ138 = A0;
const byte PIN_MQ135 = A2;

//=========================================================
// PINES INTERFAZ
//=========================================================

#define LED_AZUL 3
#define LED_AMARILLO 4
#define LED_ROJO 5
#define BUZZER 13

//=========================================================
// PINES CONTROL
//=========================================================

#define BOTON 6
#define ELECTROVALVULA_ENTRADA 8
#define ELECTROVALVULA_SALIDA 12

//=========================================================
// COMBUSTIBLE CORRECTO
//=========================================================

enum Combustible {
  DIESEL_R,
  NAFTA_R,
  DUDA_R
};

// CAMBIAR ACA SEGUN EL COMBUSTIBLE CORRECTO
#define COMBUSTIBLE_CORRECTO DIESEL_R

//=========================================================
// CONFIGURACIONES REGULABLES
//=========================================================

// BME para determinar si la trampa esta limpia
const float UMBRAL_LIMPIEZA_BME = 220.0;

// Tiempo que debe mantenerse limpia
const unsigned long TIEMPO_CONFIRMACION_LIMPIEZA = 5000;

// Tiempo de referencia
const unsigned long TIEMPO_REFERENCIA = 2000;

// Periodo entre muestras de referencia
const unsigned long PERIODO_REFERENCIA = 100;

// Tiempo de extraccion
const unsigned long TIEMPO_EXTRACCION = 2000;

// Tiempo de alarma por combustible incorrecto
const unsigned long TIEMPO_ALERTA_INCORRECTO = 3000;

// Tiempo de emergencia
const unsigned long TIEMPO_EMERGENCIA = 5000;

// Umbral de presion BME680
const float UMBRAL_PRESION = 900.0;

// Periodo de muestreo de los sensores durante extraccion
const unsigned long PERIODO_MUESTREO = 200;

//=========================================================
// ESTADOS CENTRALES
//=========================================================

enum Estado {

  LIMPIEZA_TRAMPA,
  EXTRACCION_LISTA_PARA_HACER,
  EXTRAYENDO,
  ANALIZANDO,
  DUDA,
  DIESEL,
  NAFTA,
  EMERGENCIA
};

Estado estadoActual = LIMPIEZA_TRAMPA;

//=========================================================
// BME
//=========================================================

bool bmeDisponible = false;

unsigned long ultimoIntentoBME = 0;

const unsigned long TIEMPO_REINTENTO_BME = 1000;

//=========================================================
// LECTURAS ACTUALES
//=========================================================

float lecturaMQ135 = 0;
float lecturaMQ138 = 0;
float lecturaBME = 0;
float lecturaPresion = 0;

//=========================================================
// BOTON
//=========================================================

bool botonAnterior = LOW;

//=========================================================
// TEMPORIZADORES GENERALES
//=========================================================

unsigned long tiempoEstado = 0;

//=========================================================
// LIMPIEZA
//=========================================================

bool confirmandoLimpieza = false;
unsigned long inicioConfirmacionLimpieza = 0;

//=========================================================
// REFERENCIA
//=========================================================

bool referenciaEnCurso = false;

unsigned long inicioReferencia = 0;
unsigned long ultimoMuestreoReferencia = 0;

float sumaReferencia135 = 0;
float sumaReferencia138 = 0;
float sumaReferenciaBME = 0;

byte muestrasReferencia = 0;

//=========================================================
// ESTRUCTURA REFERENCIA
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

  float relacion;
};

Features datos;

//=========================================================
// VARIABLES PARA PENDIENTE
//=========================================================

float primerMQ135;
float primerMQ138;
float primerBME;

float mq135_1s;
float mq138_1s;
float bme_1s;

bool pendienteCalculada = false;

//=========================================================
// CAPTURA
//=========================================================

unsigned long tiempoInicioCaptura = 0;
unsigned long ultimoMuestreo = 0;

byte contadorMuestras = 0;

//=========================================================
// RESULTADO
//=========================================================

Combustible resultadoFinal = DUDA_R;

//=========================================================
// ALARMA RESULTADO INCORRECTO
//=========================================================

bool alarmaIncorrectaActiva = false;
unsigned long inicioAlarmaIncorrecta = 0;

//=========================================================
// EMERGENCIA
//=========================================================

unsigned long inicioEmergencia = 0;

//=========================================================
// UMBRALES DE CLASIFICACION
//=========================================================

struct IntervaloDecision {

  float dieselMin;
  float dieselMax;

  float naftaMin;
  float naftaMax;
};

//=========================================================
// RELACION
//=========================================================

IntervaloDecision umbralRelacion = {

  0.44,
  0.52,

  0.70,
  0.82
};

//=========================================================
// AREA MQ135
//=========================================================

IntervaloDecision umbralArea135 = {

  3.10,
  3.80,

  4.20,
  7.20
};

//=========================================================
// VARIACION MQ135
//=========================================================

IntervaloDecision umbralVar135 = {

  30.0,
  65.0,

  95.0,
  230.0
};

//=========================================================
// PENDIENTE MQ135
//=========================================================

IntervaloDecision umbralPendiente135 = {

  0.10,
  0.40,

  0.70,
  2.20
};

//=========================================================
// PESOS
//=========================================================

const byte PESO_RELACION = 2;
const byte PESO_AREA = 1;
const byte PESO_VARIACION = 1;
const byte PESO_PENDIENTE = 1;

//=========================================================
// RESULTADO VARIABLE
//=========================================================

enum ResultadoVariable {

  VOTO_DIESEL,
  VOTO_DUDA,
  VOTO_NAFTA
};

//=========================================================
// VARIABLES INTERFAZ
//=========================================================

unsigned long tiempoUI = 0;

byte pasoUI = 0;

Estado estadoUIAnterior = LIMPIEZA_TRAMPA;

//=========================================================
// BEEP
//=========================================================

bool beepActivo = false;

unsigned long tiempoBeep = 0;

const unsigned long DURACION_BEEP = 80;

//=========================================================
// SETUP
//=========================================================

void setup() {

  Serial.begin(115200);

  //-----------------------------------------
  // LEDS
  //-----------------------------------------

  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  //-----------------------------------------
  // BUZZER
  //-----------------------------------------

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, HIGH);

  //-----------------------------------------
  // BOTON
  //-----------------------------------------

  pinMode(BOTON, INPUT);

  //-----------------------------------------
  // ELECTROVALVULAS
  //-----------------------------------------
  pinMode(ELECTROVALVULA_ENTRADA, OUTPUT);
  pinMode(ELECTROVALVULA_SALIDA, OUTPUT);

  //-----------------------------------------
  // INICIALIZAR VALVULAS
  //-----------------------------------------
  controlarElectrovalvulas( false, true);

  //-----------------------------------------
  // BME
  //-----------------------------------------
  if (bme.begin()) {

    configurarBME();
    bmeDisponible = true;
  }

  //-----------------------------------------
  // ESTADO INICIAL
  //-----------------------------------------

  estadoActual = LIMPIEZA_TRAMPA;

  tiempoEstado = millis();

  tiempoUI = millis();

  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F("       OLFATUS PETROLEUM"));
  Serial.println(F("   Clasificador Diesel / Nafta"));
  Serial.println(F("==================================="));
  Serial.println();
  Serial.println(F("Iniciando limpieza de trampa..."));
}

//=========================================================
// LOOP
//=========================================================

void loop() {
  //-----------------------------------------
  // BME
  //-----------------------------------------
  gestionarBME();

  //-----------------------------------------
  // LEER SENSORES
  //-----------------------------------------
  if (bmeDisponible) {
    leerSensores();
  }

  //-----------------------------------------
  // PRESION
  //-----------------------------------------
  vigilarPresion();

  //-----------------------------------------
  // REFERENCIA
  //-----------------------------------------
  actualizarReferencia();

  //-----------------------------------------
  // MAQUINA DE ESTADOS
  //-----------------------------------------
  if (!referenciaEnCurso) {
    actualizarMaquinaEstados();
  }
}

//=========================================================
// EVALUAR RELACION
//=========================================================

ResultadoVariable evaluarRelacion() {

  if (datos.relacion >= umbralRelacion.dieselMin && datos.relacion <= umbralRelacion.dieselMax) {
    return VOTO_DIESEL;
  }

  if (datos.relacion >= umbralRelacion.naftaMin && datos.relacion <= umbralRelacion.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

//=========================================================
// EVALUAR VARIACION
//=========================================================

ResultadoVariable evaluarVariacion() {

  if (datos.variacion135 >= umbralVar135.dieselMin && datos.variacion135 <= umbralVar135.dieselMax) {
    return VOTO_DIESEL;
  }

  if (datos.variacion135 >= umbralVar135.naftaMin && datos.variacion135 <= umbralVar135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

//=========================================================
// EVALUAR AREA
//=========================================================

ResultadoVariable evaluarArea() {

  if (datos.area135 >= umbralArea135.dieselMin && datos.area135 <= umbralArea135.dieselMax) {
    return VOTO_DIESEL;
  }

  if (datos.area135 >= umbralArea135.naftaMin && datos.area135 <= umbralArea135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

//=========================================================
// EVALUAR PENDIENTE
//=========================================================

ResultadoVariable evaluarPendiente() {

  if (datos.pendiente135 >= umbralPendiente135.dieselMin && datos.pendiente135 <= umbralPendiente135.dieselMax) {
    return VOTO_DIESEL;
  }

  if (datos.pendiente135 >= umbralPendiente135.naftaMin && datos.pendiente135 <= umbralPendiente135.naftaMax) {
    return VOTO_NAFTA;
  }
  return VOTO_DUDA;
}

//=========================================================
// DECISION FINAL
//=========================================================

Combustible decidirFinal() {

  byte votosDiesel = 0;
  byte votosNafta = 0;
  byte votosDuda = 0;

  ResultadoVariable r;

  //-------------------------
  // RELACION
  //-------------------------

  r = evaluarRelacion();

  if (r == VOTO_DIESEL) votosDiesel += PESO_RELACION;

  else if (r == VOTO_NAFTA) votosNafta += PESO_RELACION;

  else votosDuda += PESO_RELACION;

  //-------------------------
  // VARIACION
  //-------------------------

  r = evaluarVariacion();

  if (r == VOTO_DIESEL) votosDiesel += PESO_VARIACION;

  else if (r == VOTO_NAFTA) votosNafta += PESO_VARIACION;

  else votosDuda += PESO_VARIACION;

  //-------------------------
  // AREA
  //-------------------------

  r = evaluarArea();

  if (r == VOTO_DIESEL) votosDiesel += PESO_AREA;

  else if (r == VOTO_NAFTA) votosNafta += PESO_AREA;

  else votosDuda += PESO_AREA;

  //-------------------------
  // PENDIENTE
  //-------------------------

  r = evaluarPendiente();

  if (r == VOTO_DIESEL) votosDiesel += PESO_PENDIENTE;

  else if (r == VOTO_NAFTA) votosNafta += PESO_PENDIENTE;

  else votosDuda += PESO_PENDIENTE;

  //-------------------------
  // DECISION
  //-------------------------

  if (votosNafta > votosDiesel && votosNafta > votosDuda) {
    return NAFTA_R;
  }

  if (votosDiesel > votosNafta && votosDiesel > votosDuda) {
    return DIESEL_R;
  }
  return DUDA_R;
}

//=========================================================
// CONFIGURAR BME
//=========================================================

void configurarBME() {

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);
}

//=========================================================
// GESTIONAR BME
//=========================================================

void gestionarBME() {

  if (bmeDisponible) {

    if (!bme.performReading()) {

      bmeDisponible = false;
    }
  }

  else {

    if ( millis() - ultimoIntentoBME >= TIEMPO_REINTENTO_BME) {

      ultimoIntentoBME = millis();

      if (bme.begin()) {

        configurarBME();

        bmeDisponible = true;
      }
    }
  }
}

//=========================================================
// LEER SENSORES
//=========================================================

void leerSensores() {

  lecturaMQ138 = analogRead(PIN_MQ138) * (5.0 / 1023.0);

  lecturaMQ135 = analogRead(PIN_MQ135) * (5.0 / 1023.0);

  if (bmeDisponible) {

    lecturaBME = bme.gas_resistance / 1000.0;

    lecturaPresion = bme.pressure / 100.0;
  }
}

//=========================================================
// INICIAR REFERENCIA
//=========================================================

void iniciarReferencia() {

  referenciaEnCurso = true;

  inicioReferencia = millis();

  ultimoMuestreoReferencia = 0;

  sumaReferencia135 = 0;
  sumaReferencia138 = 0;
  sumaReferenciaBME = 0;

  muestrasReferencia = 0;

  Serial.println();
  Serial.println(F("Tomando referencia..."));
}

//=========================================================
// ACTUALIZAR REFERENCIA
//=========================================================

void actualizarReferencia() {

  if (!referenciaEnCurso) return;

  unsigned long ahora = millis();

  //-----------------------------------------
  // TERMINO LA REFERENCIA
  //-----------------------------------------

  if (ahora - inicioReferencia >= TIEMPO_REFERENCIA) {

    if (muestrasReferencia > 0) {

      referencia.mq135 = sumaReferencia135 / muestrasReferencia;

      referencia.mq138 = sumaReferencia138 / muestrasReferencia;

      referencia.bme = sumaReferenciaBME / muestrasReferencia;
    }

    referenciaEnCurso = false;

    Serial.println();
    Serial.println(F("Referencia obtenida."));

    estadoActual = EXTRACCION_LISTA_PARA_HACER;

    tiempoEstado = ahora;

    return;
  }

  //-----------------------------------------
  // TOMAR MUESTRA
  //-----------------------------------------

  if (ahora - ultimoMuestreoReferencia < PERIODO_REFERENCIA) {
    return;
  }

  ultimoMuestreoReferencia = ahora;

  leerSensores();

  sumaReferencia135 += lecturaMQ135;
  sumaReferencia138 += lecturaMQ138;
  sumaReferenciaBME += lecturaBME;

  muestrasReferencia++;
}

//=========================================================
// INICIAR CAPTURA
//=========================================================

void iniciarCaptura() {

  Serial.println();
  Serial.println(F("EXTRACCION INICIADA"));

  tiempoInicioCaptura = millis();

  ultimoMuestreo = 0;

  contadorMuestras = 0;

  pendienteCalculada = false;

  leerSensores();

  //-----------------------------------------
  // PRIMERA MUESTRA
  //-----------------------------------------
  primerMQ135 = lecturaMQ135;
  primerMQ138 = lecturaMQ138;
  primerBME = lecturaBME;

  //-----------------------------------------
  // MAXIMOS
  //-----------------------------------------
  datos.max135 = lecturaMQ135;
  datos.max138 = lecturaMQ138;

  datos.minBME = lecturaBME;

  //-----------------------------------------
  // AREAS
  //-----------------------------------------
  datos.area135 = 0;
  datos.area138 = 0;
  datos.areaBME = 0;
}

//=========================================================
// ACTUALIZAR CAPTURA
//=========================================================

void actualizarCaptura() {

  unsigned long ahora = millis();

  //-----------------------------------------
  // MUESTREO
  //-----------------------------------------

  if (ahora - ultimoMuestreo >= PERIODO_MUESTREO) {

    ultimoMuestreo = ahora;

    leerSensores();

    contadorMuestras++;

    //---------------------------------------
    // MAXIMOS
    //---------------------------------------

    if (lecturaMQ135 > datos.max135) {

      datos.max135 = lecturaMQ135;
    }

    if (lecturaMQ138 > datos.max138) {

      datos.max138 = lecturaMQ138;
    }

    if (lecturaBME < datos.minBME) {

      datos.minBME = lecturaBME;
    }

    //---------------------------------------
    // AREAS
    //---------------------------------------

    datos.area135 += lecturaMQ135;
    datos.area138 += lecturaMQ138;

    datos.areaBME += referencia.bme - lecturaBME;

    //---------------------------------------
    // PENDIENTE INICIAL
    //---------------------------------------

    if (!pendienteCalculada) {

      if (ahora - tiempoInicioCaptura >= 1000) {

        mq135_1s = lecturaMQ135;
        mq138_1s = lecturaMQ138;
        bme_1s = lecturaBME;

        pendienteCalculada = true;
      }
    }
  }

  //-----------------------------------------
  // FIN DE LOS 2 SEGUNDOS
  //-----------------------------------------

  if (ahora - tiempoInicioCaptura >= TIEMPO_EXTRACCION) {

    datos.area135 *= 0.1;
    datos.area138 *= 0.1;
    datos.areaBME *= 0.1;

    estadoActual = ANALIZANDO;

    tiempoEstado = ahora;
  }
}

//=========================================================
// CALCULAR FEATURES
//=========================================================

void calcularFeatures() {

  //-----------------------------------------
  // VARIACIONES
  //-----------------------------------------
  datos.variacion135 = ((datos.max135 - referencia.mq135)/ referencia.mq135)* 100.0;

  datos.variacion138 = ((datos.max138 - referencia.mq138)/ referencia.mq138)* 100.0;

  //-----------------------------------------
  // RELACION
  //-----------------------------------------
  datos.relacion = datos.max138 / datos.max135;

  //-----------------------------------------
  // PENDIENTES
  //-----------------------------------------

  datos.pendiente135 = mq135_1s - primerMQ135;

  datos.pendiente138 = mq138_1s - primerMQ138;
}

//=========================================================
// SET LED
//=========================================================

void setLED(
  byte azul,
  byte amarillo,
  byte rojo) {

  analogWrite(LED_AZUL, azul);
  analogWrite(LED_AMARILLO, amarillo);
  analogWrite(LED_ROJO, rojo);
}

//=========================================================
// APAGAR LED
//=========================================================

void apagarLEDs() {
  setLED(0, 0, 0);
}

//=========================================================
// BEEP
//=========================================================

void iniciarBeep() {

  digitalWrite(BUZZER, LOW);
  beepActivo = true;
  tiempoBeep = millis();
}

//=========================================================
// ACTUALIZAR BEEP
//=========================================================

void actualizarBeep() {

  if (!beepActivo)
    return;

  if (
    millis() - tiempoBeep >= DURACION_BEEP) {

    digitalWrite(BUZZER, HIGH);

    beepActivo = false;
  }
}

//=========================================================
// LIMPIEZA
//=========================================================

void patronLimpieza() {

  if (millis() - tiempoUI < 15) {
    return;
  }

  tiempoUI = millis();

  static int brillo = 40;
  static int direccion = 1;

  brillo += direccion;

  if (brillo >= 255) direccion = -1;

  if (brillo <= 40) direccion = 1;

  byte azul = brillo;

  byte amarillo = map((brillo + 85) % 255, 0, 255, 40, 255);

  byte rojo = map ((brillo + 170) % 255, 0, 255, 40, 255);

  setLED( azul, amarillo, rojo);

  digitalWrite(BUZZER, HIGH);
}

//=========================================================
// LISTO
//=========================================================

void patronListo() {

  if (millis() - tiempoUI < 250) {
    return;
  }

  tiempoUI = millis();

  pasoUI = !pasoUI;

  if (pasoUI) {
    setLED(255,255,255);
  }

  else {

    setLED(0,0,0);
  }
  digitalWrite(BUZZER, HIGH);
}

//=========================================================
// EXTRACCION
//=========================================================

void patronExtraccion() {

  if (millis() - tiempoUI < 200) {
    return;
  }

  tiempoUI = millis();
  pasoUI++;

  if (pasoUI > 2) pasoUI = 0;

  switch (pasoUI) {

    case 0:
      setLED(255, 0, 0);
      break;

    case 1:
      setLED(0, 255, 0);
      break;

    case 2:
      setLED(0, 0, 255);
      break;
  }

  digitalWrite(BUZZER, HIGH);
}

//=========================================================
// ANALISIS
//=========================================================

void patronAnalisis() {

  if (millis() - tiempoUI < 90) {

    return;
  }

  tiempoUI = millis();

  pasoUI++;

  if (pasoUI > 2) pasoUI = 0;

  switch (pasoUI) {

    case 0:
      setLED(255, 0, 0);
      break;

    case 1:
      setLED(0, 255, 0);
      break;

    case 2:
      setLED(0, 0, 255);
      break;
  }
  digitalWrite(BUZZER, HIGH);
}

//=========================================================
// RESULTADO DUDA
//=========================================================

void patronDuda() {

  setLED(0,0,255);

  if (!beepActivo) {
    iniciarBeep();
  }
  actualizarBeep();
}

//=========================================================
// RESULTADO DIESEL
//=========================================================

void patronDiesel() {
  // Amarillo = DIESEL
  setLED(0,255,0);

  actualizarBeep();
}

//=========================================================
// RESULTADO NAFTA
//=========================================================

void patronNafta() {
  // Azul = NAFTA
  setLED(255,0,0);

  actualizarBeep();
}

//=========================================================
// ALARMA COMBUSTIBLE INCORRECTO
//=========================================================

void patronAlarmaIncorrecta() {

  static bool estadoAlarma = false;

  if (millis() - tiempoUI >= 100) {
    tiempoUI = millis();
    estadoAlarma = !estadoAlarma;
  }

  //-----------------------------------------
  // MANTENER COLOR DEL RESULTADO
  //-----------------------------------------

  if (resultadoFinal == DIESEL_R) {

    if (estadoAlarma) {
      setLED(0,255,0);
    }

    else {
      setLED(0,40,0);
    }
  }

  else if (resultadoFinal == NAFTA_R) {
    if (estadoAlarma) {
      setLED(255,0,0);
    }

    else {
      setLED(40,0,0);
    }
  }

  //-----------------------------------------
  // BUZZER
  //-----------------------------------------

  if (estadoAlarma) {

    digitalWrite(BUZZER,LOW);
  }

  else {

    digitalWrite(BUZZER,HIGH);
  }
}

//=========================================================
// EMERGENCIA
//=========================================================

void patronEmergencia() {

  if (
    millis() - tiempoUI < 100) {

    return;
  }

  tiempoUI = millis();

  pasoUI = !pasoUI;

  if (pasoUI) {
    setLED(255,255,255);
    digitalWrite(BUZZER,LOW);

  }

  else {
    setLED(0,0,0);
    digitalWrite(BUZZER,HIGH);
  }
}

//=========================================================
// CAMBIAR ESTADO
//=========================================================

void cambiarEstado(Estado nuevoEstado) {

  estadoActual = nuevoEstado;
  tiempoEstado = millis();

  //-----------------------------------------
  // REINICIAR INTERFAZ
  //-----------------------------------------
  tiempoUI = millis();
  pasoUI = 0;
  beepActivo = false;

  //-----------------------------------------
  // BUZZER APAGADO
  //-----------------------------------------
  digitalWrite(BUZZER,HIGH);

  //-----------------------------------------
  // ACCIONES AL ENTRAR
  //-----------------------------------------

  switch (nuevoEstado) {

    case LIMPIEZA_TRAMPA:
      controlarElectrovalvulas(false,true);
      confirmandoLimpieza = false;

      break;


    case EXTRACCION_LISTA_PARA_HACER:
      controlarElectrovalvulas(false,false);

      break;


    case EXTRAYENDO:
      controlarElectrovalvulas(true,true);
      iniciarCaptura();

      break;


    case ANALIZANDO:
      controlarElectrovalvulas(false,false);

      break;


    case DUDA:

      controlarElectrovalvulas(false,false);
      iniciarBeep();

      break;


    case DIESEL:
      controlarElectrovalvulas(false,false);

      //-----------------------------------
      // ¿ES CORRECTO?
      //-----------------------------------

      if (COMBUSTIBLE_CORRECTO == DIESEL) {
        iniciarBeep();
      }

      else {

        alarmaIncorrectaActiva = true;
        inicioAlarmaIncorrecta = millis();
      }

      break;


    case NAFTA:
      controlarElectrovalvulas(false,false);

      //-----------------------------------
      // ¿ES CORRECTO?
      //-----------------------------------

      if (COMBUSTIBLE_CORRECTO == NAFTA) {
        iniciarBeep();
      }

      else {
        alarmaIncorrectaActiva = true;
        inicioAlarmaIncorrecta = millis();
      }

      break;


    case EMERGENCIA:

      controlarElectrovalvulas(true,true);

      inicioEmergencia = millis();

      break;
  }
}

//=========================================================
// DETECTAR PULSACION
//=========================================================
bool botonPresionado() {

  bool estadoBoton = digitalRead(BOTON);

  bool pulsacion = (estadoBoton == HIGH && botonAnterior == LOW);

  botonAnterior = estadoBoton;

  return pulsacion;
}

//=========================================================
// ACTUALIZAR LIMPIEZA
//=========================================================
void actualizarLimpieza() {

  leerSensores();

  //-----------------------------------------
  // BME SUPERA UMBRAL
  //-----------------------------------------

  if (lecturaBME > UMBRAL_LIMPIEZA_BME) {

    //---------------------------------------
    // COMIENZA CONFIRMACION
    //---------------------------------------
    if (!confirmandoLimpieza) {
      confirmandoLimpieza = true;

      inicioConfirmacionLimpieza = millis();
    }

    //---------------------------------------
    // CONFIRMACION COMPLETA
    //---------------------------------------
    else if (millis() - inicioConfirmacionLimpieza >= TIEMPO_CONFIRMACION_LIMPIEZA) {
      Serial.println();
      Serial.println(F("CAMARA LIMPIA"));

      cambiarEstado(EXTRACCION_LISTA_PARA_HACER);
    }
  }

  //-----------------------------------------
  // BME BAJO UMBRAL
  //-----------------------------------------

  else {
    confirmandoLimpieza = false;

    inicioConfirmacionLimpieza = 0;
  }
}

//=========================================================
// MAQUINA DE ESTADOS
//=========================================================

void actualizarMaquinaEstados() {

  switch (estadoActual) {

      //=====================================================
      // LIMPIEZA
      //=====================================================

    case LIMPIEZA_TRAMPA:

      controlarElectrovalvulas(false,true);
      patronLimpieza();
      actualizarLimpieza();

      break;


      //=====================================================
      // LISTO PARA EXTRAER
      //=====================================================

    case EXTRACCION_LISTA_PARA_HACER:

      controlarElectrovalvulas(false,false);
      patronListo();

      if (botonPresionado()) {
        cambiarEstado(EXTRAYENDO);
      }

      break;


      //=====================================================
      // EXTRAYENDO
      //=====================================================

    case EXTRAYENDO:

      controlarElectrovalvulas(true,true);
      patronExtraccion();

      actualizarCaptura();

      break;


      //=====================================================
      // ANALIZANDO
      //=====================================================

    case ANALIZANDO:

      controlarElectrovalvulas(false,false);

      patronAnalisis();

      //-----------------------------------------
      // CALCULAR FEATURES
      //-----------------------------------------
      calcularFeatures();

      //-----------------------------------------
      // DECIDIR
      //-----------------------------------------
      resultadoFinal = decidirFinal();

      //-----------------------------------------
      // PASAR AL RESULTADO
      //-----------------------------------------
      if (resultadoFinal == DIESEL_R) {
        cambiarEstado(DIESEL);
      }

      else if (resultadoFinal == NAFTA_R) {
        cambiarEstado(NAFTA);
      }

      else {
        cambiarEstado(DUDA);
      }

      break;


      //=====================================================
      // DUDA
      //=====================================================
    case DUDA:
      controlarElectrovalvulas(false,false);

      patronDuda();

      if (botonPresionado()) {
        cambiarEstado(LIMPIEZA_TRAMPA);
      }

      break;


      //=====================================================
      // DIESEL
      //=====================================================
    case DIESEL:

      controlarElectrovalvulas(false,false);

      //-----------------------------------------
      // COMBUSTIBLE INCORRECTO
      //-----------------------------------------
      if (COMBUSTIBLE_CORRECTO != DIESEL && alarmaIncorrectaActiva) {
        patronAlarmaIncorrecta();

        if (millis() - inicioAlarmaIncorrecta >= TIEMPO_ALERTA_INCORRECTO) {

          alarmaIncorrectaActiva = false;

          digitalWrite(BUZZER,HIGH);

          tiempoUI = millis();
        }
      }

      //-----------------------------------------
      // RESULTADO NORMAL
      //-----------------------------------------

      else {
        patronDiesel();
      }

      //-----------------------------------------
      // BOTON
      //-----------------------------------------

      if (botonPresionado()) {
        cambiarEstado(LIMPIEZA_TRAMPA);
      }

      break;


      //=====================================================
      // NAFTA
      //=====================================================

    case NAFTA:

      controlarElectrovalvulas(false,false);

      //-----------------------------------------
      // COMBUSTIBLE INCORRECTO
      //-----------------------------------------

      if (COMBUSTIBLE_CORRECTO != NAFTA && alarmaIncorrectaActiva) {

        patronAlarmaIncorrecta();

        if (millis() - inicioAlarmaIncorrecta >= TIEMPO_ALERTA_INCORRECTO) {
          alarmaIncorrectaActiva = false;
          digitalWrite(BUZZER,HIGH);
          tiempoUI = millis();
        }
      }

      //-----------------------------------------
      // RESULTADO NORMAL
      //-----------------------------------------
      else {
        patronNafta();
      }

      //-----------------------------------------
      // BOTON
      //-----------------------------------------
      if (botonPresionado()) {

        cambiarEstado(LIMPIEZA_TRAMPA);
      }

      break;


      //=====================================================
      // EMERGENCIA
      //=====================================================

    case EMERGENCIA:
      controlarElectrovalvulas(true,true);
      patronEmergencia();

      //-----------------------------------------
      // FIN EMERGENCIA
      //-----------------------------------------
      if (millis() - inicioEmergencia >= TIEMPO_EMERGENCIA) {
        digitalWrite(BUZZER,HIGH);
        cambiarEstado(LIMPIEZA_TRAMPA);
      }

      break;
  }
}

//=========================================================
// VIGILANCIA GLOBAL DE PRESION
//=========================================================

void vigilarPresion() {

  if (!bmeDisponible)return;

  //-----------------------------------------
  // NO VOLVER A ENTRAR A EMERGENCIA
  //-----------------------------------------
  if (
    estadoActual == EMERGENCIA) {
    return;
  }

  //-----------------------------------------
  // PRESION BAJA
  //-----------------------------------------
  if (lecturaPresion < UMBRAL_PRESION) {

    Serial.println();
    Serial.println(F("!!! EMERGENCIA: PRESION BAJA !!!"));

    cambiarEstado(EMERGENCIA);
  }
}

//=========================================================
// ELECTROVALVULAS
//=========================================================

void controlarElectrovalvulas(bool entrada, bool salida) {

  digitalWrite(ELECTROVALVULA_ENTRADA, entrada ? HIGH : LOW);
  digitalWrite(ELECTROVALVULA_SALIDA, salida ? HIGH : LOW);
}