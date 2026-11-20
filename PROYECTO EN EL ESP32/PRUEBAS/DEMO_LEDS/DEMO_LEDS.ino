/*
 *  DEMO_LEDS - OLFATUS
 *  Programa de muestra: solo prende los LEDs y los deja prendidos.
 *  Pines iguales a OLFATUS_PETROLEUM_V7.
 */

#define LED_AZUL     18
#define LED_AMARILLO 32
#define LED_ROJO     17

void setup() {
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);

  digitalWrite(LED_AZUL, HIGH);
  digitalWrite(LED_AMARILLO, HIGH);
  digitalWrite(LED_ROJO, HIGH);
}

void loop() {
}
