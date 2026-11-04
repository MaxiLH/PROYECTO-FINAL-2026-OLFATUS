void setup() {
  Serial.begin(9600);
}

void loop() {
  
  int adc_MQ = analogRead(A0); //Lemos la salida analógica del MQ
  float voltaje = adc_MQ * (5.0 / 1023.0); //Convertimos la lectura en un valor de voltaje
  int adc_MQ1 = analogRead(A1); //Lemos la salida analógica del MQ
  float voltaje2 = adc_MQ1 * (5.0 / 1023.0); //Convertimos la lectura en un valor de voltaje

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