#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <string.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme(&Wire);

//==============================
// SEGURIDAD BME
//==============================

bool bmeDisponible = false;

unsigned long ultimoIntentoBME = 0;
const unsigned long TIEMPO_REINTENTO_BME = 1000;

float temperatura = NAN;
float humedad = NAN;
float presion = NAN;
float resistenciaGas = NAN;

//==============================
// CONFIG CAPTURA
//==============================

#define NUM_MUESTRAS 25
#define PERIODO_MUESTREO 200 // ms

uint16_t curvaMQ138[NUM_MUESTRAS];
uint16_t curvaMQ135[NUM_MUESTRAS];
uint16_t curvaBME[NUM_MUESTRAS];

//==============================
// REFERENCIAS
//==============================

float baseMQ138 = 0;
float baseMQ135 = 0;
float baseBME = 0;

//==============================
// ESTADOS
//==============================

enum Estado {
  ESPERANDO_REFERENCIA,
  ESPERANDO_INICIO_CAPTURA,
  ESPERANDO_REINICIO
};
Estado estado = ESPERANDO_REFERENCIA;

void setup() {

  Serial.begin(112500);
  while(!Serial);

  Serial.println("PASO 1");
  if (!bme.begin()) {

      Serial.println("PASO 2");
      while(1);
  }

  Serial.println("PASO 3");
  configurarBME();

  Serial.println("PASO 4");
  bmeDisponible = true;

  Serial.println("PASO 5");
  
}

void loop() {
  gestionarBME();

  switch (estado) {
    case ESPERANDO_REFERENCIA:
     
      if (Serial.available()) {
        while (Serial.available()) Serial.read();

        obtenerReferencia();

        Serial.println();
        Serial.println("==================================");
        Serial.println("Referencia obtenida");
        Serial.println("Prepare la succion");
        Serial.println("Envie cualquier caracter");
        Serial.println("para comenzar captura");
        Serial.println("==================================");

        estado = ESPERANDO_INICIO_CAPTURA;
      }

      break;

    case ESPERANDO_INICIO_CAPTURA:

      if (Serial.available()) {
        while (Serial.available()) Serial.read();

        capturarMuestras();

        analizarDatos();

        Serial.println();
        Serial.println("==================================");
        Serial.println("Escriba reinicio");
        Serial.println("para repetir prueba");
        Serial.println("==================================");

        estado = ESPERANDO_REINICIO;
      }

      break;

    case ESPERANDO_REINICIO:

      if (Serial.available()) {
        char buffer[20];
        int n = Serial.readBytesUntil('\n', buffer, sizeof(buffer)-1);
        buffer[n]='\0';

        if (strcmp(buffer, "reinicio") == 0) {
          Serial.println();
          Serial.println("==================================");
          Serial.println("Nueva prueba");
          Serial.println("Envie cualquier caracter");
          Serial.println("para tomar referencia");
          Serial.println("==================================");

          estado = ESPERANDO_REFERENCIA;
        }
      }
      break;
  }
}

//====================================================

void configurarBME() {
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);
}

//====================================================

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

//====================================================

void obtenerReferencia() {
  Serial.println();
  Serial.println("Tomando referencia...");

  float suma138 = 0;
  float suma135 = 0;
  float sumaBME = 0;

  const int N = 20;

  for (int i = 0; i < N; i++) {
    gestionarBME();

    suma138 += analogRead(A0) * (5.0 / 1023.0);
    suma135 += analogRead(A2) * (5.0 / 1023.0);
    sumaBME += resistenciaGas;

    delay(100);
  }

  baseMQ138 = suma138 / N;
  baseMQ135 = suma135 / N;
  baseBME = sumaBME / N;
}

//====================================================

void capturarMuestras() {

  Serial.println();
  Serial.println("Capturando 5 segundos...");

  for(int i=0;i<NUM_MUESTRAS;i++) {

      gestionarBME();

      float mq138=
      analogRead(A0)*(5.0/1023.0);

      float mq135=
      analogRead(A2)*(5.0/1023.0);

      curvaMQ138[i]=
      (uint16_t)(mq138*1000);

      curvaMQ135[i]=
      (uint16_t)(mq135*1000);

      curvaBME[i]=
      (uint16_t)
      (resistenciaGas*100);

      delay(PERIODO_MUESTREO);
  }
  Serial.println("Captura terminada");
}

//====================================================

void analizarDatos() {
  float maxMQ138=
  curvaMQ138[0]/1000.0;

  float maxMQ135=
  curvaMQ135[0]/1000.0;

  float minBME=
  curvaBME[0]/100.0;

  float areaMQ138 = 0;
  float areaMQ135 = 0;
  float areaBME = 0;

  for(int i=0;i<NUM_MUESTRAS;i++){

  float mq138=
  curvaMQ138[i]/1000.0;

  float mq135=
  curvaMQ135[i]/1000.0;

  float bme=
  curvaBME[i]/100.0;

  if(mq138>maxMQ138)
  maxMQ138=mq138;

  if(mq135>maxMQ135)
  maxMQ135=mq135;

  if(bme<minBME)
  minBME=bme;

  areaMQ138+=mq138;

  areaMQ135+=mq135;

  areaBME+=(baseBME-bme);

  }

  areaMQ138 *= 0.1;
  areaMQ135 *= 0.1;
  areaBME *= 0.1;

  float variacionMQ138 = ((maxMQ138 - baseMQ138) / baseMQ138) * 100.0;

  float variacionMQ135 = ((maxMQ135 - baseMQ135) / baseMQ135) * 100.0;

  float variacionBME = ((baseBME - minBME) / baseBME) * 100.0;

  float relacion = maxMQ138 / maxMQ135;

  //==============================
  // T50 Y T90 MQ138
  //==============================

  float objetivo50MQ138 = baseMQ138 + (maxMQ138 - baseMQ138) * 0.5;

  float objetivo90MQ138 = baseMQ138 + (maxMQ138 - baseMQ138) * 0.9;

  long t50MQ138 = -1;
  long t90MQ138 = -1;

  for (int i = 0; i < NUM_MUESTRAS; i++) {
    if (t50MQ138 == -1 && curvaMQ138[i]/1000.0 >= objetivo50MQ138) t50MQ138 = i*PERIODO_MUESTREO;

    if (t90MQ138 == -1 && curvaMQ138[i]/1000.0 >= objetivo90MQ138) t90MQ138 = i*PERIODO_MUESTREO;
  }
  //==============================
  // T50 Y T90 MQ135
  //==============================

  float objetivo50MQ135 = baseMQ135 + (maxMQ135 - baseMQ135) * 0.5;

  float objetivo90MQ135 = baseMQ135 + (maxMQ135 - baseMQ135) * 0.9;

  long t50MQ135 = -1;
  long t90MQ135 = -1;

  for (int i = 0; i < NUM_MUESTRAS; i++) {
    if (t50MQ135 == -1 && curvaMQ135[i]/1000.0 >= objetivo50MQ135) t50MQ135 = i*PERIODO_MUESTREO;

    if (t90MQ135 == -1 && curvaMQ135[i]/1000.0 >= objetivo90MQ135) t90MQ135 = i*PERIODO_MUESTREO;
  }

  //==============================
  // PENDIENTE INICIAL
  //==============================

  float pendienteMQ138= ( (curvaMQ138[5]/1000.0) - (curvaMQ138[0]/1000.0)) / (5.0*PERIODO_MUESTREO/1000.0);

  float pendienteMQ135= ( (curvaMQ135[5]/1000.0) - (curvaMQ135[0]/1000.0)) / (5.0*PERIODO_MUESTREO/1000.0);

  float pendienteBME=

(
(curvaBME[5]/100.0)
-
(curvaBME[0]/100.0)
)

/

(
5.0*PERIODO_MUESTREO/1000.0
);

  //==================================================
  // IMPRIMIR TODAS LAS MUESTRAS
  //==================================================  

  Serial.println();
  Serial.println("===== MUESTRAS =====");
  Serial.println("Tiempo, MQ138, MQ135, BME");

  for(int i=0;i<NUM_MUESTRAS;i++)
{

Serial.print(
i*PERIODO_MUESTREO
);

Serial.print(",");


Serial.print(
curvaMQ138[i]/1000.0,
4
);


Serial.print(",");


Serial.print(
curvaMQ135[i]/1000.0,
4
);


Serial.print(",");


Serial.println(
curvaBME[i]/100.0,
2
);


}

  //==================================================
  // RESUMEN
  //==================================================

  Serial.println();
  Serial.println("===== RESUMEN =====");

  Serial.print("Base MQ138: ");
  Serial.println(baseMQ138, 4);

  Serial.print("Base MQ135: ");
  Serial.println(baseMQ135, 4);

  Serial.print("Base BME: ");
  Serial.println(baseBME, 2);

  Serial.println();

  Serial.print("Max MQ138: ");
  Serial.println(maxMQ138, 4);

  Serial.print("Max MQ135: ");
  Serial.println(maxMQ135, 4);

  Serial.print("Min BME: ");
  Serial.println(minBME, 2);

  Serial.println();

  Serial.print("Variacion MQ138 (%): ");
  Serial.println(variacionMQ138, 2);

  Serial.print("Variacion MQ135 (%): ");
  Serial.println(variacionMQ135, 2);

  Serial.print("Variacion BME (%): ");
  Serial.println(variacionBME, 2);

  Serial.println();

  Serial.print("T50 MQ138 (ms): ");
  Serial.println(t50MQ138);

  Serial.print("T90 MQ138 (ms): ");
  Serial.println(t90MQ138);

  Serial.print("T50 MQ135 (ms): ");
  Serial.println(t50MQ135);

  Serial.print("T90 MQ135 (ms): ");
  Serial.println(t90MQ135);

  Serial.println();

  Serial.print("Pendiente inicial MQ138: ");
  Serial.println(pendienteMQ138, 4);

  Serial.print("Pendiente inicial MQ135: ");
  Serial.println(pendienteMQ135, 4);

  Serial.print("Pendiente inicial BME: ");
  Serial.println(pendienteBME, 4);

  Serial.println();

  Serial.print("Relacion MQ138/MQ135: ");
  Serial.println(relacion, 4);

  Serial.println();

  Serial.print("Area MQ138: ");
  Serial.println(areaMQ138, 4);

  Serial.print("Area MQ135: ");
  Serial.println(areaMQ135, 4);

  Serial.print("Area BME: ");
  Serial.println(areaBME, 4);

  Serial.println("==============================");
}