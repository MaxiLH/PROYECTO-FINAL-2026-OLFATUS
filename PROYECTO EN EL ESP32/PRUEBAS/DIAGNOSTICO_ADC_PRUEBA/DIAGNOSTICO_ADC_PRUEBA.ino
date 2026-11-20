// DIAGNOSTICO DEL ADC (placa ESP32)
//
// Lee todos los pines analogicos del ADC1: IO32, IO33, IO34,
// IO35, IO36 (VP) e IO39 (VN). Cada valor sale como
// "cuentas/milivoltios" (cuentas de 0 a 4095).
//
// - Un pin sin nada conectado tiene que dar un valor bajo o
//   que salta. Si TODOS los pines dan 4095 / ~3134 mV, algun
//   pin del ADC esta recibiendo mas de 3,3 V y esa sobretension
//   arrastra las lecturas de todos los demas.
// - El ESP32 aguanta como maximo 3,6 V en un pin (el Nano
//   aguantaba 5 V). Un MQ alimentado con 5 V puede sacar mas
//   de 3,3 V por su salida analogica.

const int pines[] = { 32, 33, 34, 35, 36, 39 };
const int N = sizeof(pines) / sizeof(pines[0]);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("DIAG_ADC");
}

void loop() {
  for (int i = 0; i < N; i++) {
    Serial.print("IO");
    Serial.print(pines[i]);
    Serial.print("=");
    Serial.print(analogRead(pines[i]));
    Serial.print("/");
    Serial.print(analogReadMilliVolts(pines[i]));
    Serial.print("mV  ");
  }
  Serial.println();
  delay(400);
}
