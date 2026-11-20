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

Adafruit_BME680 bme(&Wire); // I2C
//Adafruit_BME680 bme(&Wire1); // example of I2C on another bus
//Adafruit_BME680 bme(BME_CS); // hardware SPI
//Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO,  BME_SCK);

// Placa ESP32 (BME680: SDA = IO21, SCL = IO22)
const int pinMQ135 = 34;  // A_135_1
const int pinMQ138 = 35;  // A_138_1

// Si hay divisor entre el MQ y el pin: (R1 + R2) / R2. 1.0 = sin divisor
const float FACTOR_DIVISOR_MQ = 1.0;

// La placa del ESP32 tiene una sola electrovalvula (entrada)
const int pinValvulaEntrada = 26;

// Pines I2C del ESP32 (esquematico: SDA = IO21, SCL = IO22)
const int PIN_SDA = 21;
const int PIN_SCL = 22;

bool bmeOk = false;
unsigned long ultimoIntentoBME = 0;

//---------------------------------------------------------
// DIAGNOSTICO I2C
//
// 1) Nivel de SDA y SCL sin el Wire andando: tienen que
//    estar en HIGH por las resistencias de pull-up del
//    modulo. Si estan en LOW: modulo sin alimentar, sin
//    pull-ups o linea en corto a GND.
// 2) Escaneo del bus: que direcciones contestan.
// 3) ID del chip en la direccion encontrada (registro 0xD0):
//    0x61 = BME680, 0x60 = BME280, 0x58 = BMP280.
//---------------------------------------------------------

void diagnosticoI2C() {
  Serial.println(F("---- DIAGNOSTICO I2C ----"));

  Wire.end();  // en los reintentos, para poder leer los pines sueltos
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_SCL, INPUT);
  delay(5);
  Serial.print(F("Reposo (tienen que dar 1): SDA(IO21) = "));
  Serial.print(digitalRead(PIN_SDA));
  Serial.print(F("   SCL(IO22) = "));
  Serial.println(digitalRead(PIN_SCL));

  // Con el pull-up interno (~45 kOhm): si ahora dan 1, las
  // lineas estaban al aire (no hay pull-ups / modulo no
  // conectado). Si siguen en 0, algo las tira a GND (modulo
  // sin alimentar que las baja por sus pull-ups, o un corto).
  pinMode(PIN_SDA, INPUT_PULLUP);
  pinMode(PIN_SCL, INPUT_PULLUP);
  delay(5);
  Serial.print(F("Con pull-up interno:       SDA(IO21) = "));
  Serial.print(digitalRead(PIN_SDA));
  Serial.print(F("   SCL(IO22) = "));
  Serial.println(digitalRead(PIN_SCL));
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_SCL, INPUT);

  Wire.begin(PIN_SDA, PIN_SCL);

  byte encontrados = 0;
  for (byte dir = 1; dir < 127; dir++) {
    Wire.beginTransmission(dir);
    if (Wire.endTransmission() == 0) {
      encontrados++;
      Serial.print(F("Contesta la direccion 0x"));
      Serial.print(dir, HEX);

      // ID del chip
      Wire.beginTransmission(dir);
      Wire.write(0xD0);
      Wire.endTransmission(false);
      Wire.requestFrom(dir, (byte)1);
      if (Wire.available()) {
        byte id = Wire.read();
        Serial.print(F("   chip ID = 0x"));
        Serial.print(id, HEX);
        if (id == 0x61) Serial.print(F(" (BME680)"));
        else if (id == 0x60) Serial.print(F(" (BME280, NO es BME680)"));
        else if (id == 0x58) Serial.print(F(" (BMP280, NO es BME680)"));
      }
      Serial.println();
    }
  }

  if (encontrados == 0) Serial.println(F("Ningun dispositivo contesta en el bus I2C"));
  Serial.println(F("------------------------"));
}

// Prueba las dos direcciones posibles del BME680:
// 0x77 (SDO a VCC, la que usa bme.begin() por defecto) y
// 0x76 (SDO a GND, la de muchos modulos chinos)
bool iniciarBME() {
  byte direcciones[2] = { 0x77, 0x76 };

  for (byte i = 0; i < 2; i++) {
    if (bme.begin(direcciones[i])) {
      Serial.print(F("BME680 iniciado en 0x"));
      Serial.println(direcciones[i], HEX);

      bme.setTemperatureOversampling(BME680_OS_8X);
      bme.setHumidityOversampling(BME680_OS_2X);
      bme.setPressureOversampling(BME680_OS_4X);
      bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
      bme.setGasHeater(320, 150); // 320*C for 150 ms
      return true;
    }
  }

  Serial.println(F("BME680 no inicia ni en 0x77 ni en 0x76"));
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // el ESP32 arranca antes de que se abra el monitor
  Serial.println(F("BME680 test"));

  diagnosticoI2C();
  bmeOk = iniciarBME();

  // Ya no se cuelga si no encuentra el BME: los MQ se siguen
  // leyendo y el BME se reintenta cada 5 s en el loop
  pinMode(pinValvulaEntrada, OUTPUT);

  digitalWrite(pinValvulaEntrada, LOW);
  delay(1000);

  digitalWrite(pinValvulaEntrada, HIGH);
}

void loop() {
  int adc_MQ = analogRead(pinMQ135); //MQ135 (ADC de 12 bits: 0-4095)
  float voltaje = analogReadMilliVolts(pinMQ135) / 1000.0 * FACTOR_DIVISOR_MQ; //voltaje del sensor
  int adc_MQ1 = analogRead(pinMQ138); //MQ138
  float voltaje2 = analogReadMilliVolts(pinMQ138) / 1000.0 * FACTOR_DIVISOR_MQ;

  if (!bmeOk && millis() - ultimoIntentoBME > 5000) {
    ultimoIntentoBME = millis();
    diagnosticoI2C();
    bmeOk = iniciarBME();
  }

  if (bmeOk) verificacion_lectura_bme();

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
  Serial.print(bme.temperature);
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

void verificacion_lectura_bme(){
  if (! bme.performReading()) {
    Serial.println("Failed to perform reading :(");
    return;
  }
  return;
} 
