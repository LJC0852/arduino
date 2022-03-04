#include <Arduino.h>
#include<IRremote.hpp>
#define DECODE_NEC
#include "PinDefinitionsAndMore.h"
#include <IRremote.hpp>

const byte ledPin = 2;       // Builtin-LED pin
const byte interruptPin = 0; // BOOT/IO0 button pin
volatile byte state = LOW;

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(interruptPin, INPUT_PULLUP);
  pinmode(LED_BUILTIN, OUTPUT);
  attachInterrupt(digitalPinToInterrupt(interruptPin), blink, CHANGE);
  Serial.begin(115200);
  Serial.println(F("START "_FILE_" from"_DATE_"\r\nUsing library version" VERSION IRREMOTE ));
  Irsender.bedin();
  Serial.print(F("Ready to send IR singals at pin"));
  Serial.println(IR_SEND_PIN);
}

void loop() {
  digitalWrite(ledPin, state);
}

void blink() {
  state = !state;
}
