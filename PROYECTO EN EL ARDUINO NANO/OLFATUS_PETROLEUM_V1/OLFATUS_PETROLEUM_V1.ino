#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme(&Wire);  // I2C
//----- BME SEGURIDAD -----
//Esto es para que no se bloquee todo cuando el BME tenga algún falso contacto, se desconecte o falle
bool bmeDisponible = false;

unsigned long ultimoIntentoBME = 0;
const unsigned long TIEMPO_REINTENTO_BME = 1000;

// Variables donde se guardan las últimas mediciones válidas
float temperatura = NAN;
float humedad = NAN;
float presion = NAN;
float resistenciaGas = NAN;

void setup() {
  Serial.begin(115200);
  if (bme.begin()) {
    configurarBME();

    bmeDisponible = true;

    Serial.println("BME680 conectado");
  } else {
    Serial.println("BME680 no encontrado");
  }
}

void loop() {
  int adc_MQ = analogRead(A0);                //Leemos la salida analógica del MQ
  float voltaje = adc_MQ * (5.0 / 1023.0);    //Convertimos la lectura en un valor de voltaje
  int adc_MQ1 = analogRead(A2);               //Leemos la salida analógica del MQ
  float voltaje2 = adc_MQ1 * (5.0 / 1023.0);  //Convertimos la lectura en un valor de voltaje
  // Gestiona el BME sin bloquear nada
  gestionarBME();

  Serial.print("adc:");
  Serial.print(adc_MQ);
  Serial.print("    voltaje:");
  Serial.print(voltaje);
  Serial.print("     ");
  Serial.print("adc:");
  Serial.print(adc_MQ1);
  Serial.print("    voltaje:");
  Serial.println(voltaje2);

  Serial.print("Temperature = ");
  Serial.print(temperatura);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Humidity = ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Gas = ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" KOhms");

  Serial.print("Approx. Altitude = ");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.println(" m");

  Serial.println();
  delay(200);
}

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
      Serial.println("Error leyendo BME680");

      bmeDisponible = false;
    }
  } else {
    if (millis() - ultimoIntentoBME >= TIEMPO_REINTENTO_BME) {
      ultimoIntentoBME = millis();

      if (bme.begin()) {
        configurarBME();

        bmeDisponible = true;

        Serial.println("BME680 reconectado");
      }
    }
  }
}