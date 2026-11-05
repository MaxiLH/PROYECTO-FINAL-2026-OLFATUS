/**********************************************************************
 * OLFATUS PETROLEUM - MODO DEMO
 *
 * Clasificador Diesel / Nafta
 *
 * Copia de OLFATUS_PETROLEUM_V7 para mostrar el equipo SIN
 * combustibles reales y SIN sensores conectados:
 *
 *  - Cada extraccion analiza una muestra de ejemplo de la tabla
 *    MUESTRAS_DEMO (valores reales de las pruebas del 03/11/2026,
 *    DATOS DE PRUEBAS\PRUEBAS_2026-11-03.xlsx). Cuando se terminan
 *    las muestras vuelve a empezar (ciclico).
 *  - Los valores pasan por la misma votacion que el
 *    DIFERENCIADOR_V7 (mismos intervalos, pesos y criterios), asi
 *    que el diagnostico se imprime igual que con una muestra real.
 *    Orden de resultados: DIESEL, NAFTA, DIESEL, DIESEL, ...
 *  - El BME680, el MQ135 y el MQ138 no se usan: la limpieza se
 *    confirma sola despues de TIEMPO_CONFIRMACION_LIMPIEZA y la
 *    presion se considera siempre normal (no hay EMERGENCIA).
 *  - LEDs, buzzer, boton y electrovalvula funcionan igual que en
 *    el programa real.
 *  - Por el monitor serie solo se imprime cada cambio de estado
 *    (no en cada vuelta del loop) para que se lea el diagnostico.
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
#include <avr/pgmspace.h>

// MODO DEMO: sin BME680 ni MQ135 / MQ138 (no se incluye la
// libreria del BME ni se leen los pines A2 / A3).

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

const byte PIN_VALVULA_ENTRADA = 8;   // HIGH = abierta (unica electrovalvula)
const byte PIN_BOTON = 6;             // INPUT normal, lógica pulldown (HIGH = presionado)

//=========================================================
// CONFIGURACION - CAPTURA Y REFERENCIA DE GASES (v7)
//
// MODO DEMO: se mantienen los mismos tiempos que el programa
// real (25 muestras cada 200 ms, 20 lecturas de referencia
// cada 100 ms) para que los LEDs y la electrovalvula se vean
// igual, pero no se lee ningun sensor: los valores salen de
// MUESTRAS_DEMO.
//=========================================================

#define NUM_MUESTRAS 25
#define PERIODO_MUESTREO 200  // ms nominal entre muestras

// Referencia: 20 lecturas, ahora por conteo (no por duracion fija)
const byte N_MUESTRAS_REFERENCIA = 20;
const uint16_t PERIODO_REFERENCIA = 100;  // ms nominal entre lecturas

const bool MODO_DIAGNOSTICO = true;

//=========================================================
// CONFIGURACION - MAQUINA DE ESTADOS (doc5)
//=========================================================

// MODO DEMO: no hay BME, la limpieza se da por confirmada
// cuando pasa este tiempo (en el programa real es el tiempo
// que la resistencia del BME tiene que quedar arriba de
// 220 kOhm).
const uint32_t TIEMPO_CONFIRMACION_LIMPIEZA = 6000UL;   // ms
const uint32_t TIEMPO_EMERGENCIA = 5000UL;              // ms
const uint32_t TIEMPO_ALERTA_INCORRECTO = 3000UL;       // ms

//=========================================================
// COMBUSTIBLE CORRECTO (configurable)
//=========================================================
// Puede tomar el valor DIESEL o NAFTA (ver enum Zona más abajo)

#define COMBUSTIBLE_CORRECTO DIESEL

//=========================================================
// BME
//
// MODO DEMO: todas las muestras de ejemplo tienen dato del
// BME, asi que VarBME siempre vota.
//=========================================================

bool bmeValidoEnCaptura = true;

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
// MUESTRAS DE EJEMPLO (MODO DEMO)
//
// Valores que imprimio el diagnostico del DIFERENCIADOR_V7
// en las pruebas del 03/11/2026 (limpiezas de 1 minuto).
// Ver DATOS DE PRUEBAS\PRUEBAS_2026-11-03.xlsx.
//
// Para agregar una muestra: copiar una fila con los valores
// del diagnostico. Se recorren en orden y al terminar se
// vuelve a la primera.
//=========================================================

struct MuestraDemo {
  char nombre[16];

  float ref135;
  float ref138;
  float refBME;

  float max135;
  float max138;

  float relacion;
  float variacionBME;
  float area135;
  float variacion135;
  float variacion138;
  float firma;
  float pendiente135;
};

const MuestraDemo MUESTRAS_DEMO[] PROGMEM = {
  //  nombre          ref135 ref138 refBME  max135 max138 relacion varBME   area135 var135  var138 firma  pend135
  { "Diesel 17:13", 1.133, 1.534, 437.0, 1.133, 1.500, 0.987, -2.062, 2.756, -0.038, -2.2, -1.00, -0.029 },
  { "Nafta 17:15", 0.956, 1.342, 959.9, 1.339, 1.818, 1.374, 65.355, 3.178, 40.061, 35.5, 0.89, 0.215 },
  { "Diesel 17:18", 1.046, 1.451, 850.9, 1.036, 1.378, 0.967, 25.219, 2.556, -0.974, -5.0, -1.00, -0.020 }
};

const byte N_MUESTRAS_DEMO = sizeof(MUESTRAS_DEMO) / sizeof(MUESTRAS_DEMO[0]);

byte indiceDemo = 0;        // muestra que se va a analizar
MuestraDemo muestraDemo;    // copia en RAM de la muestra actual

unsigned long tiempoInicioCaptura;
unsigned long ultimoMuestreo;
byte contadorMuestras = 0;

unsigned long duracionCaptura = 0;

//=========================================================
// REFERENCIA - VARIABLES DE CAPTURA
//=========================================================

unsigned long ultimoMuestreoReferencia = 0;
byte muestrasReferencia = 0;

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
bool avisoSinReaccion = false;  // ningun sensor paso su minimo

// Que sensor detecto la muestra (para el diagnostico)
bool muestraPor135 = false;
bool muestraPor138 = false;
bool muestraPorBME = false;

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
// INTERVALOS DE CADA VARIABLE
//
// MODO DEMO: copiados del DIFERENCIADOR_V7 del Nano
// (04/11/2026). Si se cambian alla, copiarlos aca para que
// la demo vote igual.
//
//  valor <  dieselMin ................ DUDA_POR_DEBAJO
//  dieselMin <= valor <= dieselMax ... DIESEL
//  dieselMax <  valor <  naftaMin .... DUDA_MEDIO
//  naftaMin <= valor <= naftaMax ..... NAFTA
//  valor >  naftaMax ................. DUDA_POR_ENCIMA
//
//  La relacion es (max138 + max135) / (ref138 + ref135):
//  factor de subida, 1.00 = sin cambio.
//=========================================================

struct Intervalo {
  float dieselMin;
  float dieselMax;
  float naftaMin;
  float naftaMax;
};

//                      dslMin  dslMax  nafMin   nafMax
Intervalo iRelacion = { 0.6, 1.23, 1.24, 10.0 };
Intervalo iVarBME = { -10.00, 40.00, 41.00, 105.00 };
Intervalo iArea135 = { 1.40, 2.90, 3.00, 7.60 };
Intervalo iVar135 = {-5.00, 8.00, 8.50, 950.00 };
Intervalo iPendiente = { 0.00, 0.42, 0.43, 1.90 };

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
const byte PESO_PENDIENTE = 0;  // igual que DIFERENCIADOR_V7: no vota

//=========================================================
// CRITERIOS
//=========================================================

// Deteccion de muestra (solo aviso): se considera que HAY
// muestra si al menos un sensor reacciono por encima de su
// minimo. Igual que DIFERENCIADOR_V7.
const float VARIACION_MINIMA_135 = 3.0;  // %
const float VARIACION_MINIMA_138 = 3.0;  // %
const float VARIACION_MINIMA_BME = 5.0;  // %

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

void iniciarCaptura();
void procesarExtraccion();
void finalizarExtraccion();

void iniciarTomaReferencia();
void procesarTomaReferencia();
void finalizarTomaReferencia();
void procesarConfirmacionLimpieza();

void cargarMuestraDemo();
void cargarFeaturesDemo();
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

void setElectrovalvula(bool entrada);
bool botonPresionado();

void vigilarPresion();
void cambiarEstado(Estado nuevo);
void entrarEstado(Estado e);
void imprimirEstado(Estado e);
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

  pinMode(PIN_BOTON, INPUT);

  Serial.println();
  Serial.println(F("==================================="));
  Serial.println(F(" OLFATUS PETROLEUM "));
  Serial.println(F(" Maquina de estados central "));
  Serial.println(F(" *** MODO DEMO ***"));
  Serial.print(F(" "));
  Serial.print(N_MUESTRAS_DEMO);
  Serial.println(F(" muestras de ejemplo, en ciclo"));
  Serial.println(F("==================================="));

  entrarEstado(estado); // inicializa LIMPIEZA_TRAMPA
}

void loop() {
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

// MODO DEMO: el estado se imprime una sola vez, al entrar
void imprimirEstado(Estado e) {
  Serial.print(F(">> Estado: "));

  switch (e) {
    case Estado::LIMPIEZA_TRAMPA:             Serial.println(F("LIMPIEZA_TRAMPA")); break;
    case Estado::EXTRACCION_LISTA_PARA_HACER: Serial.println(F("EXTRACCION_LISTA_PARA_HACER (apretar el boton)")); break;
    case Estado::EXTRAYENDO:                  Serial.println(F("EXTRAYENDO")); break;
    case Estado::ANALIZANDO:                  Serial.println(F("ANALIZANDO")); break;
    case Estado::DUDA:                        Serial.println(F("DUDA")); break;
    case Estado::DIESEL:                      Serial.println(F("DIESEL")); break;
    case Estado::NAFTA:                       Serial.println(F("NAFTA")); break;
    case Estado::EMERGENCIA:                  Serial.println(F("EMERGENCIA")); break;
  }
}

void entrarEstado(Estado e) {
  imprimirEstado(e);

  switch (e) {

    case Estado::LIMPIEZA_TRAMPA:
      setElectrovalvula(true); // entrada abierta durante la limpieza

      faseLimpieza = FaseLimpieza::CONFIRMANDO;
      confirmandoActivo = false;

      break;

    case Estado::EXTRACCION_LISTA_PARA_HACER:
      setElectrovalvula(false);
      break;

    case Estado::EXTRAYENDO:
      setElectrovalvula(true); // entrada abierta durante la extraccion
      iniciarCaptura();
      break;

    case Estado::ANALIZANDO:
      setElectrovalvula(false);
      patronAnalisis();

      cargarFeaturesDemo();
      resultadoActual = decidirFinal();

      imprimirResultadoGases();

      // Siguiente muestra de ejemplo (vuelve a la primera al terminar)
      indiceDemo = (indiceDemo + 1) % N_MUESTRAS_DEMO;

      switch (resultadoActual) {
        case DIESEL:  cambiarEstado(Estado::DIESEL); break;
        case NAFTA:   cambiarEstado(Estado::NAFTA);  break;
        default:      cambiarEstado(Estado::DUDA);   break;
      }

      break;

    case Estado::DUDA:
      setElectrovalvula(false);

      beepActivo = false;
      beepResultadoHecho = false;

      break;

    case Estado::DIESEL:
      setElectrovalvula(false);
      prepararResultado(DIESEL);
      break;

    case Estado::NAFTA:
      setElectrovalvula(false);
      prepararResultado(NAFTA);
      break;

    case Estado::EMERGENCIA:
      setElectrovalvula(true);
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
  // MODO DEMO: no hay BME, la presion se considera siempre
  // normal y nunca se entra en EMERGENCIA.
}

//=========================================================
// LIMPIEZA_TRAMPA: confirmación + toma de referencia
//
// MODO DEMO: la limpieza se confirma sola cuando pasa
// TIEMPO_CONFIRMACION_LIMPIEZA desde que se entra al estado.
//=========================================================

void procesarConfirmacionLimpieza() {
  if (!confirmandoActivo) {
    confirmandoActivo = true;
    tiempoInicioConfirmacion = millis();
  } else if (millis() - tiempoInicioConfirmacion >= TIEMPO_CONFIRMACION_LIMPIEZA) {
    iniciarTomaReferencia();
  }
}

//=========================================================
// REFERENCIA - no bloqueante, por conteo (v7)
//
// MODO DEMO: tarda lo mismo que la real (20 lecturas cada
// 100 ms) pero la referencia sale de la muestra de ejemplo
// que se va a analizar.
//=========================================================

void iniciarTomaReferencia() {
  faseLimpieza = FaseLimpieza::TOMANDO_REFERENCIA;

  ultimoMuestreoReferencia = 0;
  muestrasReferencia = 0;
}

void procesarTomaReferencia() {
  if (millis() - ultimoMuestreoReferencia < PERIODO_REFERENCIA) return;

  ultimoMuestreoReferencia = millis();

  muestrasReferencia++;

  if (muestrasReferencia >= N_MUESTRAS_REFERENCIA) {
    finalizarTomaReferencia();
  }
}

void finalizarTomaReferencia() {
  cargarMuestraDemo();

  referencia.mq138 = muestraDemo.ref138;
  referencia.mq135 = muestraDemo.ref135;
  referencia.bme = muestraDemo.refBME;

  Serial.print(F("Muestra de ejemplo "));
  Serial.print(indiceDemo + 1);
  Serial.print(F(" de "));
  Serial.print(N_MUESTRAS_DEMO);
  Serial.print(F(": "));
  Serial.println(muestraDemo.nombre);

  cambiarEstado(Estado::EXTRACCION_LISTA_PARA_HACER);
}

// Copia de la memoria de programa (PROGMEM) a la RAM la
// muestra de ejemplo que toca
void cargarMuestraDemo() {
  memcpy_P(&muestraDemo, &MUESTRAS_DEMO[indiceDemo], sizeof(MuestraDemo));
}

//=========================================================
// EXTRAYENDO: captura de muestra - no bloqueante, por
// conteo (v7)
//
// MODO DEMO: se cuentan las 25 muestras con el mismo
// periodo que la captura real (para que la extraccion dure
// lo mismo y se vea el patron de LEDs), pero no se lee
// ningun sensor.
//=========================================================

void iniciarCaptura() {
  tiempoInicioCaptura = millis();
  ultimoMuestreo = 0;
  contadorMuestras = 0;
}

void procesarExtraccion() {
  if (millis() - ultimoMuestreo < PERIODO_MUESTREO) return;

  ultimoMuestreo = millis();

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
// FEATURES (MODO DEMO)
//
// En vez de calcularlas con las curvas de los sensores, se
// copian de la muestra de ejemplo los mismos valores que
// imprimio el diagnostico del DIFERENCIADOR_V7 en las
// pruebas. Despues pasan por la misma votacion.
//=========================================================

void cargarFeaturesDemo() {
  memset(&datos, 0, sizeof(datos));

  datos.max135 = muestraDemo.max135;
  datos.max138 = muestraDemo.max138;

  datos.relacion = muestraDemo.relacion;
  datos.variacionBME = muestraDemo.variacionBME;
  datos.area135 = muestraDemo.area135;
  datos.variacion135 = muestraDemo.variacion135;
  datos.variacion138 = muestraDemo.variacion138;
  datos.firma = muestraDemo.firma;
  datos.pendiente135 = muestraDemo.pendiente135;

  // Lo que el diagnostico real reconstruye a partir de la curva
  datos.minBME = referencia.bme * (1.0 - datos.variacionBME / 100.0);
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
  avisoSinReaccion = false;

  muestraPor135 = false;
  muestraPor138 = false;
  muestraPorBME = false;

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
  // 3) Hay muestra? (solo aviso, igual que DIFERENCIADOR_V7)
  //
  //    Alcanza con que reaccione UN sensor. Ya NO corta el
  //    resultado: gana la votacion y solo se avisa que
  //    ningun sensor reacciono.
  //-----------------------

  muestraPor135 = datos.variacion135 >= VARIACION_MINIMA_135;
  muestraPor138 = datos.variacion138 >= VARIACION_MINIMA_138;
  muestraPorBME = datos.variacionBME >= VARIACION_MINIMA_BME;

  if (!muestraPor135 && !muestraPor138 && !muestraPorBME) {
    avisoSinReaccion = true;
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

  Serial.print(F("MODO DEMO - muestra de ejemplo: "));
  Serial.println(muestraDemo.nombre);

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

  Serial.print(F("Max/Ref 138 = "));
  Serial.print(referencia.mq138 > REF138_MINIMA ? datos.max138 / referencia.mq138 : 0, 3);
  Serial.print(F("   Max/Ref 135 = "));
  Serial.println(referencia.mq135 > 0.01 ? datos.max135 / referencia.mq135 : 0, 3);

  Serial.print(F("Var138 = "));
  Serial.print(datos.variacion138, 1);
  Serial.print(F("   Firma = "));
  Serial.println(datos.firma, 2);

  Serial.print(F("Muestra detectada por:"));
  if (muestraPor135) Serial.print(F(" MQ135"));
  if (muestraPor138) Serial.print(F(" MQ138"));
  if (muestraPorBME) Serial.print(F(" BME"));
  if (!muestraPor135 && !muestraPor138 && !muestraPorBME) Serial.print(F(" ninguno"));
  Serial.println();

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

  if (avisoSinReaccion) {
    Serial.println();
    Serial.println(F("AVISO: ningun sensor reacciono,"));
    Serial.println(F("puede que no haya muestra"));
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

void  patronLimpieza() {
  if (millis() - tiempoUI < 250) return; // Intervalo de cambio entre estados (250 ms)
  tiempoUI = millis();

  static byte paso = 0;
  paso = (paso + 1) % 6; // Secuencia cíclica de 6 pasos

  // Simula el desfase original usando solo estados DIGITALES (HIGH / LOW)
  bool azul     = (paso == 0 || paso == 1 || paso == 5);
  bool amarillo = (paso == 1 || paso == 2 || paso == 3);
  bool rojo     = (paso == 3 || paso == 4 || paso == 5);

  setLED(azul ? 255 : 0, amarillo ? 255 : 0, rojo ? 255 : 0);
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
// ELECTROVALVULA (solo entrada)
//=========================================================

void setElectrovalvula(bool entrada) {
  digitalWrite(PIN_VALVULA_ENTRADA, entrada ? HIGH : LOW);
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
