/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * VERSION BASE
 *
 * Parte 1
 *
 * Arquitectura general
 *
 * Santino Leguizamo
 * Constantino Guagnini
 * Maximiliano Hunag
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

const byte PIN_MQ138 = A0;
const byte PIN_MQ135 = A2;

//=========================================================
// CONFIGURACION
//=========================================================

const uint16_t PERIODO_MUESTREO = 200;    // ms
const uint16_t TIEMPO_REFERENCIA = 2000;  // ms
const uint16_t TIEMPO_CAPTURA = 5000;     // ms

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
  //-------------------------
  // Valores extremos
  //-------------------------

  float max135;
  float max138;

  float minBME;

  //-------------------------
  // Areas
  //-------------------------

  float area135;
  float area138;
  float areaBME;

  //-------------------------
  // Pendientes
  //-------------------------

  float pendiente135;
  float pendiente138;

  //-------------------------
  // Variaciones
  //-------------------------

  float variacion135;
  float variacion138;

  //-------------------------
  // Relacion
  //-------------------------

  float relacion;
};

Features datos;

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

//=========================================================
// RESULTADOS
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

//=========================================================
// MAQUINA DE ESTADOS
//=========================================================

enum Estado {
  ESPERANDO_COMANDO,
  TOMANDO_REFERENCIA,
  CAPTURANDO,
  CALCULANDO,
  CLASIFICANDO,
  MOSTRANDO
};

Estado estado = ESPERANDO_COMANDO;
//=========================================================
// UMBRALES
//=========================================================

struct ZonaDecision {
  float dieselSeguro;
  float naftaSegura;
};

ZonaDecision umbralRelacion = {
  0.55,
  0.65
};

ZonaDecision umbralArea135 = {
  3.80,
  4.40
};

ZonaDecision umbralVar135 = {
  70.0,
  95.0
};

ZonaDecision umbralPendiente135 = {
  0.45,
  0.65
};

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

ResultadoVariable evaluarRelacion();

ResultadoVariable evaluarArea();

ResultadoVariable evaluarVariacion();

ResultadoVariable evaluarPendiente();

Combustible decidirFinal();

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

    case ESPERANDO_COMANDO:

      if (Serial.available()) {
        while (Serial.available())
          Serial.read();

        estado = TOMANDO_REFERENCIA;
      }

      break;

    case TOMANDO_REFERENCIA:

      obtenerReferencia();

      iniciarCaptura();

      estado = CAPTURANDO;

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
// BME FUNCIONES DE FUNCIONAMIENTO
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

  lecturaBME = bme.gas_resistance / 1000.0;
}

//=========================================================
// TOMANDO REFERENCIA
//=========================================================

void obtenerReferencia() {
  Serial.println();
  Serial.println(F("Tomando referencia..."));

  float suma135 = 0;
  float suma138 = 0;
  float sumaBME = 0;

  const byte N =
    TIEMPO_REFERENCIA / 100;

  for (byte i = 0; i < N; i++) {
    gestionarBME();

    leerSensores();

    suma135 += lecturaMQ135;
    suma138 += lecturaMQ138;
    sumaBME += lecturaBME;

    delay(100);
  }

  referencia.mq135 = suma135 / N;
  referencia.mq138 = suma138 / N;
  referencia.bme = sumaBME / N;
}

void iniciarCaptura() {
  Serial.println();
  Serial.println(F("Capturando..."));

  tiempoInicioCaptura =
    millis();

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

//=========================================================
// CAPTURANDO
//=========================================================

void actualizarCaptura() {
  if (millis() - ultimoMuestreo < PERIODO_MUESTREO) return;

  ultimoMuestreo = millis();

  gestionarBME();

  leerSensores();

  contadorMuestras++;

  //--------------------------
  // MAXIMOS
  //--------------------------

  if (lecturaMQ135 > datos.max135) datos.max135 = lecturaMQ135;

  if (lecturaMQ138 > datos.max138) datos.max138 = lecturaMQ138;

  if (lecturaBME < datos.minBME) datos.minBME = lecturaBME;

  //--------------------------
  // AREAS
  //--------------------------

  datos.area135 += lecturaMQ135;

  datos.area138 += lecturaMQ138;

  datos.areaBME += (referencia.bme - lecturaBME);

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
    datos.area135 *= 0.1;

    datos.area138 *= 0.1;

    datos.areaBME *= 0.1;

    estado = CALCULANDO;
  }
}

//=========================================================
// CALCULANDO
//=========================================================

void calcularFeatures() {
  //-----------------------
  // Variaciones
  //-----------------------

  datos.variacion135 = ((datos.max135 - referencia.mq135)/referencia.mq135)* 100.0;

  datos.variacion138 = ((datos.max138 - referencia.mq138)/referencia.mq138)* 100.0;

  //-----------------------
  // Relacion
  //-----------------------

  datos.relacion = datos.max138 / datos.max135;

  //-----------------------
  // Pendientes
  //-----------------------

  datos.pendiente135 = (mq135_1s - primerMQ135);

  datos.pendiente138 = (mq138_1s - primerMQ138);
}

//=========================================================
// CLASIFICANDO
//=========================================================

ResultadoVariable evaluarRelacion() {
  if (datos.relacion <= umbralRelacion.dieselSeguro)
    return VOTO_DIESEL;

  if (datos.relacion >= umbralRelacion.naftaSegura)
    return VOTO_NAFTA;

  return VOTO_DUDA;
}

ResultadoVariable evaluarVariacion() {
  if (datos.variacion135 <= umbralVar135.dieselSeguro)
    return VOTO_DIESEL;

  if (datos.variacion135 >= umbralVar135.naftaSegura)
    return VOTO_NAFTA;

  return VOTO_DUDA;
}

ResultadoVariable evaluarArea() {
  if (datos.area135 <= umbralArea135.dieselSeguro)
    return VOTO_DIESEL;

  if (datos.area135 >= umbralArea135.naftaSegura)
    return VOTO_NAFTA;

  return VOTO_DUDA;
}

ResultadoVariable evaluarPendiente() {
  if (datos.pendiente135 <= umbralPendiente135.dieselSeguro)
    return VOTO_DIESEL;

  if (datos.pendiente135 >= umbralPendiente135.naftaSegura)
    return VOTO_NAFTA;

  return VOTO_DUDA;
}

Combustible decidirFinal() {
  byte votosDiesel = 0;
  byte votosNafta = 0;
  byte votosDuda = 0;

  ResultadoVariable r;

  //-----------------------
  // Relación
  //-----------------------

  r = evaluarRelacion();

  if (r == VOTO_DIESEL) votosDiesel++;
  else if (r == VOTO_NAFTA) votosNafta++;
  else votosDuda++;

  //-----------------------
  // Variación
  //-----------------------

  r = evaluarVariacion();

  if (r == VOTO_DIESEL) votosDiesel++;
  else if (r == VOTO_NAFTA) votosNafta++;
  else votosDuda++;

  //-----------------------
  // Área
  //-----------------------

  r = evaluarArea();

  if (r == VOTO_DIESEL) votosDiesel++;
  else if (r == VOTO_NAFTA) votosNafta++;
  else votosDuda++;

  //-----------------------
  // Pendiente
  //-----------------------

  r = evaluarPendiente();

  if (r == VOTO_DIESEL) votosDiesel++;
  else if (r == VOTO_NAFTA) votosNafta++;
  else votosDuda++;

  //-----------------------
  // DECISIÓN
  //-----------------------

  if (votosNafta > votosDiesel && votosNafta > votosDuda)
    return NAFTA;

  if (votosDiesel > votosNafta && votosDiesel > votosDuda)
    return DIESEL;

  return DUDA;
}

void imprimirResultado() {
  Combustible resultado =
    decidirFinal();

  Serial.println();
  Serial.println(F("============================"));

  switch (resultado) {
    case DIESEL:

      Serial.println(F("RESULTADO"));
      Serial.println(F("DIESEL"));

      break;

    case NAFTA:

      Serial.println(F("RESULTADO"));
      Serial.println(F("NAFTA"));

      break;

    case DUDA:

      Serial.println(F("RESULTADO"));
      Serial.println(F("DUDA"));
      Serial.println(F("Repetir medicion"));

      break;
  }

  Serial.println(F("============================"));
  Serial.println();
  Serial.println(F("Envie cualquier caracter"));
  Serial.println(F("para repetir."));
}

//=========================================================
// MOSTRANDO
//=========================================================

void resetCaptura() {
  memset(&datos, 0, sizeof(datos));

  primerMQ135 = 0;
  primerMQ138 = 0;
  primerBME = 0;

  mq135_1s = 0;
  mq138_1s = 0;
  bme_1s = 0;

  contadorMuestras = 0;

  pendienteCalculada = false;
}