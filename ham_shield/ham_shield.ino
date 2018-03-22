#include <Wire.h>
#include <HamShield.h>

HamShield radio;

#define LED_PIN 13
#define REPORT_RATE 5000
#define PWM_PIN 3
#define RESET_PIN A3
#define SWITCH_PIN 2

uint32_t freq;

bool is_tx;

void setup() {
  // put your setup code here, to run once:
  pinMode(PWM_PIN,OUTPUT);
  digitalWrite(PWM_PIN,LOW);

  pinMode(SWITCH_PIN,INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println("press button to begin...");

  //begin on button press, essentially an on switch
  while(digitalRead(SWITCH_PIN));

  digitalWrite(RESET_PIN,HIGH);

  //test connection
  Serial.println(radio.testConnection() ? "RDA radio connection successful" : "RDA radio connection failed");

  //initialize
  radio.initialize(); //UHF 12.5kHz
  radio.dangerMode(); //default config

  radio.setSQOff();
  freq = 430000;
  radio.frequency(freq);

  radio.setModeReceive();
  is_tx = false;

  radio.setRfPower(0);

  pinMode(LED_PIN,OUTPUT);
  
}

void loop() {
  // put your main code here, to run repeatedly:

}
