/***************************************************************************
  This is a library for the BME680 gas, humidity, temperature & pressure sensor

  Designed specifically to work with the Adafruit BME680 Breakout
  ----> http://www.adafruit.com/products/3660

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/


#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

// Definición de los pines para las válvulas
const int pinValvulaEntrada = 8;
const int pinValvulaSalida = 12;

Adafruit_BME680 bme(&Wire); // I2C
//Adafruit_BME680 bme(&Wire1); // example of I2C on another bus
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

void setup() {
  Serial.begin(112500);
   // Configuramos los pines de las válvulas como salidas
  pinMode(pinValvulaEntrada, OUTPUT);
  pinMode(pinValvulaSalida, OUTPUT);

  // Nos aseguramos de que todo arranque apagado por seguridad
  digitalWrite(pinValvulaEntrada, LOW);
  digitalWrite(pinValvulaSalida, LOW);
  delay(1000);

  digitalWrite(pinValvulaEntrada, HIGH);
  digitalWrite(pinValvulaSalida, HIGH);
}

void loop() {
}

void verificacion_lectura_bme(){
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }
  return;
}