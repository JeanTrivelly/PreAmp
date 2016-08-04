



#include "wm8816.h"

wm_pin_t pin;

wm8816::wm8816 (uint8_t csbPin, uint8_t mutebPin, uint8_t dataPin, uint8_t cclkPin) {
  pin.csbPin   = csbPin;
  pin.mutebPin = mutebPin;
  pin.dataPin  = dataPin;
  pin.cclkPin  = cclkPin;
}

void wm8816::init () {
  // set pin modes
  pinMode(pin.csbPin,   OUTPUT);
  pinMode(pin.mutebPin, OUTPUT);
  pinMode(pin.dataPin,  OUTPUT);
  pinMode(pin.cclkPin,  OUTPUT);
  
  /* Disable Chip */
  digitalWrite(pin.csbPin, HIGH);
  /* unmute */
  digitalWrite(pin.mutebPin, HIGH);
}

void wm8816::write (byte reg, byte data) {
  int i = 0;
  byte reg_l  = reg;
  byte data_l = data;

  /* Init PINs */
  pinMode(pin.dataPin,  OUTPUT);
  digitalWrite(pin.dataPin, LOW);
  digitalWrite(pin.cclkPin, LOW);
  
  /* Set Low the Chip Select (Active Low) */
  digitalWrite(pin.csbPin, LOW);
  delayMicroseconds(10);

  /* Set to 0 the read/write bit to register */
  reg_l = reg_l & 0b11111011;
  
  /* Send register value */
  for ( i=0 ; i<8 ; i++){
	if (reg_l & 0b10000000) {
	  digitalWrite(pin.dataPin, HIGH);
	} else {
	  digitalWrite(pin.dataPin, LOW);
	}
	reg_l = reg_l<<1;
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, HIGH);
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, LOW);
  }
  /* Send data value */
  for ( i=0 ; i<8 ; i++){
	if (data_l & 0b10000000) {
	  digitalWrite(pin.dataPin, HIGH);
	} else {
	  digitalWrite(pin.dataPin, LOW);
	}
	data_l = data_l<<1;
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, HIGH);
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, LOW);
  }
  /* Set High Chip Select */
  digitalWrite(pin.csbPin, HIGH);
  
  /* Init PINs */
  pinMode(pin.dataPin,  OUTPUT);
  digitalWrite(pin.dataPin, LOW);
  digitalWrite(pin.cclkPin, LOW);
}

void wm8816::read (byte reg, byte *data){
  int i = 0;
  byte reg_l  = reg;

  /* Init PINs */
  pinMode(pin.dataPin,  OUTPUT);
  digitalWrite(pin.dataPin, LOW);
  digitalWrite(pin.cclkPin, LOW);
  
  /* Set Low the Chip Select (Active Low) */
  digitalWrite(pin.csbPin, LOW);
  delayMicroseconds(10);

  /* Set to 1 the read/write bit to register */
  reg_l = reg_l | 0b00000100;
  
  /* Send register value */
  for ( i=0 ; i<8 ; i++){
	if (reg_l & 0b10000000) {
	  digitalWrite(pin.dataPin, HIGH);
	} else {
	  digitalWrite(pin.dataPin, LOW);
	}
	reg_l = reg_l<<1;
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, HIGH);
	delayMicroseconds(5);
	digitalWrite(pin.cclkPin, LOW);
  }

  /* Change data pin from output to input */
  pinMode(pin.dataPin, INPUT);
  
  /* Dummy clock cycle... */
  delayMicroseconds(5);
  digitalWrite(pin.cclkPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(pin.cclkPin, LOW);
  delayMicroseconds(3);

  *data = 0;
  /* Receive data value */
  for ( i=0 ; i<8 ; i++){
	if (digitalRead(pin.dataPin)) {
	  *data |= 1<<(7-i);
	} else {
	  *data &= ~(1<<(7-i));
	}
	delayMicroseconds(2);
	digitalWrite(pin.cclkPin, HIGH);
	delayMicroseconds(5);
    digitalWrite(pin.cclkPin, LOW);
    delayMicroseconds(3);
  }

  /* Set High Chip Select */
  digitalWrite(pin.csbPin, HIGH);
  
  /* Init PINs */
  pinMode(pin.dataPin,  OUTPUT);
  digitalWrite(pin.dataPin, LOW);
  digitalWrite(pin.cclkPin, LOW);
}

void wm8816::mute (){
  digitalWrite(pin.mutebPin, LOW);
}

void wm8816::unmute (){
  digitalWrite(pin.mutebPin, HIGH);
}
