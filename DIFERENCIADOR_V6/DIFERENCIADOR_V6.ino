/**********************************************************************
 * OLFATUS PETROLEUM
 *
 * Clasificador Diesel / Nafta
 *
 * VERSION 6
 *
 *  - Decision por intervalos sobre una recta numerica:
 *
 *      DUDA_POR_DEBAJO | DIESEL | DUDA_MEDIO | NAFTA | DUDA_POR_ENCIMA
 *
 *  - Adquisicion de datos IDENTICA a la del adquisidor:
 *      25 muestras fijas, delay de 200 ms, mismas cuentas,
 *      misma cuantizacion a uint16_t.
 *
 *  - Pines del diferenciador SIN CAMBIOS (MQ138 = A3, MQ135 = A2)
 *
 *  - NUEVO: umbral de confianza del 80%. Si la zona con mas
 *    peso no junta al menos el 80% del peso total disponible,
 *    ya NO se devuelve esa zona (evita, por ejemplo, decir
 *    DIESEL con votos 5-2-1 = 62.5% de confianza). En su lugar
 *    cae en DUDA_MEDIO y se avisa que las variables estan
 *    dispersas.
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
// CONFIGURACION DE CAPTURA
//
// Igual que el adquisidor: 25 muestras fijas con delay de
// 200 ms entre cada una.
//
// OJO: la captura NO dura 5 segundos. Cada muestra incluye
// una lectura del BME que bloquea entre 200 y 350 ms, asi
// que 25 muestras tardan alrededor de 10 segundos reales.
// El adquisidor tarda lo mismo, y por eso la tabla de
// calibracion corresponde a esa ventana. Si algun dia se
// acelera la captura, hay que acelerar los dos programas
// y volver a calibrar.
//=========================================================

#define NUM_MUESTRAS 25
#define PERIODO_MUESTREO 200  // ms

// Indice usado para la pendiente inicial (igual que el adquisidor)
const byte INDICE_PENDIENTE = 5;
const float TIEMPO_PENDIENTE = (INDICE_PENDIENTE * PERIODO_MUESTREO) / 1000.0;

// El adquisidor hace: area = suma_de_las_25_muestras * 0.1
const float FACTOR_AREA = 0.1;

// Referencia: 20 lecturas cada 100 ms (igual que el adquisidor)
const byte N_MUESTRAS_REFERENCIA = 20;
const uint16_t PERIODO_REFERENCIA = 100;

const bool MODO_DIAGNOSTICO = true;

//=========================================================
// CURVAS (mismo formato que el adquisidor)
//=========================================================

uint16_t curvaMQ138[NUM_MUESTRAS];
uint16_t curvaMQ135[NUM_MUESTRAS];
uint16_t curvaBME[NUM_MUESTRAS];

unsigned long duracionCaptura = 0;

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
// RESULTADO
//
// Las cinco posiciones de la recta numerica, en orden.
// El mismo tipo se usa para la zona de cada variable y
// para el resultado final.
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

Zona resultado = DUDA_MEDIO;

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
// INTERVALOS DE CADA VARIABLE
//
//  valor <  dieselMin ................ DUDA_POR_DEBAJO
//  dieselMin <= valor <= dieselMax ... DIESEL
//  dieselMax <  valor <  naftaMin .... DUDA_MEDIO
//  naftaMin <= valor <= naftaMax ..... NAFTA
//  valor >  naftaMax ................. DUDA_POR_ENCIMA
//
//  Rangos medidos (10 diesel / 10 nafta):
//
//    relacion   diesel 0.46 - 0.48    nafta 0.72 - 1.00
//    area135    diesel 3.25 - 3.66    nafta 4.39 - 6.19
//    var135     diesel 35.7 - 57.4    nafta 108  - 205
//    pend135    diesel 0.13 - 0.34    nafta 0.53 - 1.47
//    varBME     diesel 33.0 - 52.0    nafta 73.4 - 90.3
//
//  Los limites de abajo son los mismos que ya tenian en la
//  version 4, solo que ahora los cuatro se usan como bordes
//  de intervalo en vez de zona gris + abstencion.
//=========================================================

struct Intervalo {
  float dieselMin;
  float dieselMax;
  float naftaMin;
  float naftaMax;
};

//                      dslMin  dslMax  nafMin   nafMax
Intervalo iRelacion  = {  0.86,   1.20,   1.30,    1.8 };
Intervalo iVarBME    = { 0.00,  60.00,  67.00,   105.00 };
Intervalo iArea135   = {  1.40,   3.95,   4.15,    7.60 };
Intervalo iVar135    = { 22.00,  70.00,  95.00,  295.00 };
Intervalo iPendiente = {  0.05,   0.42,   0.48,    1.90 };

//=========================================================
// FIRMA DE COMBUSTIBLE
//
//  variacion138 / variacion135
//
//  Medido:  diesel 2.00 - 2.32    nafta 2.12 - 2.78
//
//  No distingue diesel de nafta. Sirve para sospechar que
//  la sustancia no es un combustible. Ahora es solo un
//  aviso, no anula el resultado.
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
// dispersas. Ej: votos 5-2-1 sobre 8 = 62.5% -> no alcanza,
// no se devuelve DIESEL.
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
// PROTOTIPOS
//=========================================================

void configurarBME();
void gestionarBME();
void obtenerReferencia();
void capturarMuestras();
void calcularFeatures();

Zona zonaDe(float valor, const Intervalo &in);
float posicionDe(Zona z);
Zona decidirFinal();

void imprimirDiagnostico();
void imprimirResultado();
void resetCaptura();

//=========================================================
// SETUP
//=========================================================

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
  Serial.println(F(" v6 - decision por intervalos + 80% conf."));
  Serial.println(F("==================================="));

  if (!bmeDisponible) {
    Serial.println(F("AVISO: BME no detectado."));
    Serial.println(F("Se sigue sin esa variable."));
  }

  Serial.println();
  Serial.println(F("Envie cualquier caracter"));
  Serial.println(F("para comenzar."));
}

//=========================================================
// LOOP
//=========================================================

void loop() {
  switch (estado) {

    case ESPERANDO_REFERENCIA:

      gestionarBME();

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

      gestionarBME();

      if (Serial.available()) {
        while (Serial.available())
          Serial.read();

        estado = CAPTURANDO;
      }

      break;

    case CAPTURANDO:

      capturarMuestras();

      estado = CALCULANDO;

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

      gestionarBME();

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
// REFERENCIA
//
// Identica al adquisidor: 20 lecturas, delay(100),
// gestionarBME() antes de cada una.
//=========================================================

void obtenerReferencia() {
  Serial.println();
  Serial.println(F("Tomando referencia..."));

  float suma138 = 0;
  float suma135 = 0;
  float sumaBME = 0;

  for (byte i = 0; i < N_MUESTRAS_REFERENCIA; i++) {
    gestionarBME();

    suma138 += analogRead(PIN_MQ138) * (5.0 / 1023.0);
    suma135 += analogRead(PIN_MQ135) * (5.0 / 1023.0);

    if (bmeDisponible && !isnan(resistenciaGas)) {
      sumaBME += resistenciaGas;
    }

    delay(PERIODO_REFERENCIA);
  }

  referencia.mq138 = suma138 / N_MUESTRAS_REFERENCIA;
  referencia.mq135 = suma135 / N_MUESTRAS_REFERENCIA;
  referencia.bme = sumaBME / N_MUESTRAS_REFERENCIA;
}

//=========================================================
// CAPTURA
//
// Identica al adquisidor: for bloqueante de 25 muestras,
// gestionarBME() + analogRead + guardado cuantizado,
// delay(PERIODO_MUESTREO) al final de cada vuelta.
//=========================================================

void capturarMuestras() {
  Serial.println();
  Serial.print(F("Capturando "));
  Serial.print(NUM_MUESTRAS);
  Serial.println(F(" muestras..."));

  bmeValidoEnCaptura = bmeDisponible;

  unsigned long inicio = millis();

  for (byte i = 0; i < NUM_MUESTRAS; i++) {

    gestionarBME();

    if (!bmeDisponible) bmeValidoEnCaptura = false;

    float mq138 = analogRead(PIN_MQ138) * (5.0 / 1023.0);
    float mq135 = analogRead(PIN_MQ135) * (5.0 / 1023.0);

    curvaMQ138[i] = (uint16_t)(mq138 * 1000);
    curvaMQ135[i] = (uint16_t)(mq135 * 1000);

    // El uint16_t se desborda arriba de 655.35 kohm.
    // Se recorta para no guardar basura. Conviene agregar
    // estas dos lineas tambien en el adquisidor.
    float gas = resistenciaGas;

    if (isnan(gas) || gas < 0) gas = 0;
    if (gas > 655.0) gas = 655.0;

    curvaBME[i] = (uint16_t)(gas * 100);

    delay(PERIODO_MUESTREO);
  }

  duracionCaptura = millis() - inicio;

  Serial.print(F("Captura terminada en "));
  Serial.print(duracionCaptura);
  Serial.println(F(" ms"));
}

//=========================================================
// FEATURES
//
// Mismas cuentas que analizarDatos() del adquisidor.
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
    datos.variacionBME = -1;  // no disponible
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

  if (datos.variacionBME >= 0) {
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
// SALIDA
//=========================================================

void imprimirRecta() {
  Serial.println();
  Serial.println(F("Peso por zona de la recta:"));

  for (byte z = 0; z < N_ZONAS; z++) {
    Serial.print(F("  "));
    Serial.print(nombresZonas[z]);
    Serial.print(F(" : "));
    Serial.print(pesoZona[z]);

    if ((Zona)z == resultado) Serial.print(F("   <== ganadora"));

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

void imprimirResultado() {
  resultado = decidirFinal();

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

  Serial.println();
  Serial.println(F("Envie cualquier caracter"));
  Serial.println(F("para repetir."));
}

//=========================================================
// RESET
//=========================================================

void resetCaptura() {
  memset(&datos, 0, sizeof(datos));

  for (byte i = 0; i < NUM_MUESTRAS; i++) {
    curvaMQ138[i] = 0;
    curvaMQ135[i] = 0;
    curvaBME[i] = 0;
  }

  duracionCaptura = 0;

  motivo = SIN_MOTIVO;
  avisoFirma = false;
  avisoDispersion = false;

  resultado = DUDA_MEDIO;

  pesoTotal = 0;
  posicionGlobal = 0;
  confianza = 0;

  for (byte z = 0; z < N_ZONAS; z++) pesoZona[z] = 0;

  for (byte i = 0; i < N_VARIABLES; i++) {
    votos[i].zona = DUDA_MEDIO;
    votos[i].participa = false;
  }
}
