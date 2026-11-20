// Placa ESP32: MQ135 en IO35 (A_135_1), MQ138 en IO32 (A_138_1)
const int pinMQ135 = 34;
const int pinMQ138 = 35;

// Si hay divisor entre el MQ y el pin: (R1 + R2) / R2. 1.0 = sin divisor
const float FACTOR_DIVISOR_MQ = 1.0;

void setup() {
  Serial.begin(115200);
}

void loop() {

  int adc_MQ = analogRead(pinMQ135); //MQ135 (ADC de 12 bits: 0-4095)
  float voltaje = analogReadMilliVolts(pinMQ135) / 1000.0 * FACTOR_DIVISOR_MQ; //voltaje del sensor
  int adc_MQ1 = analogRead(pinMQ138); //MQ138
  float voltaje2 = analogReadMilliVolts(pinMQ138) / 1000.0 * FACTOR_DIVISOR_MQ;

  Serial.print("adc:");
  Serial.print(adc_MQ);
  Serial.print("    voltaje:");
  Serial.print(voltaje);
  Serial.print("     ");
  Serial.print("adc:");
  Serial.print(adc_MQ1);
  Serial.print("    voltaje:");
  Serial.println(voltaje2);
}
