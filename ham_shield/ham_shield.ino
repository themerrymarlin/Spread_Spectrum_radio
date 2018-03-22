#include <Wire.h>
#include <HamShield.h>

HamShield radio;

#define LED_PIN 13
#define REPORT_RATE 5000
#define PWM_PIN 3
#define RESET_PIN A3
#define SWITCH_PIN 2

uint32_t freq;

void setup() {
  // put your setup code here, to run once:
  pinMode(PWM_PIN,OUTPUT);
  digitalWrite(PWM_PIN,LOW);

  pinMode(SWITCH_PIN,INPUT_PULLUP);

  Serial.begin(9600);

  while(digitalRead(SWITCH_PIN));

  
}

void loop() {
  // put your main code here, to run repeatedly:

}
