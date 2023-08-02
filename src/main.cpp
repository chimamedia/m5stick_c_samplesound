#include <M5StickCPlus.h>

void setup()
{
  M5.begin();

  Serial.begin(115200);
  delay(500);
  Serial.print("M5StickC-plus Start\n");

  M5.Lcd.print("Hello World");

  pinMode(10, OUTPUT);
}

void loop()
{
  digitalWrite(10, HIGH);
  Serial.print("LED TURN ON\n");
  delay(1000);
  digitalWrite(10, LOW);
  Serial.print("LED TURN OFF\n");
  delay(1000);
}