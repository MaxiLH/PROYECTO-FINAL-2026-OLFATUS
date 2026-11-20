const int buzzerPin = 23;

void setup() {
  // Configura el pin 23 como salida
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  // Enciende el buzzer (aplica voltaje)
  digitalWrite(buzzerPin, HIGH); 
  delay(1000);                    // Espera medio segundo (500 milisegundos)
  
  // Apaga el buzzer (quita el voltaje)
  digitalWrite(buzzerPin, LOW);     
  delay(1000);                    // Espera medio segundo en silencio
}