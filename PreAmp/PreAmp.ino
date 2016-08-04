#include <IRremote.h>
#include <Encoder.h>
#include <EEPROM.h>

#define DEBUG
#define DEBUG2

#define APPLE_REMOTE // Use APPLE Remote control.


/* PIN Definition */
#define IR_RECV_PIN                   0
#define SELECT_ANALOG_NUM_PIN         1

#define ON_OFF_TRIGGER_PIN            3
#define ON_OFF_LED_PIN                4
#define ON_OFF_BUTTON_PIN             5

#define SOURCE_KNOB_PIN1              7
#define SOURCE_KNOB_PIN2              8
#define VOLUME_UP_PIN                 9
#define VOLUME_DOWN_PIN              10
/* PIN 11 is dedicated to on board led */


#define NB_SOURCES 9
#if (NB_SOURCES > 9)
#error "Nomber max of sources is 9"
#endif
#define SOURCE1_ANALOG_SELECT_PIN    13
#define SOURCE2_ANALOG_SELECT_PIN    14
#define SOURCE3_ANALOG_SELECT_PIN    15
#define SOURCE4_ANALOG_SELECT_PIN    16
#define SOURCE5_DIGITAL_SELECT_PIN   17
#define SOURCE6_DIGITAL_SELECT_PIN   18
#define SOURCE7_DIGITAL_SELECT_PIN   19
#define SOURCE8_DIGITAL_SELECT_PIN   20
#define SOURCE9_USB_SELECT_PIN       21

#define LED_ON HIGH
#define LED_OFF LOW

byte mSource = 0;
boolean mSourceChanged = 0;
byte mSources[] = {SOURCE1_ANALOG_SELECT_PIN,
                   SOURCE2_ANALOG_SELECT_PIN,
                   SOURCE3_ANALOG_SELECT_PIN,
                   SOURCE4_ANALOG_SELECT_PIN,
                   SOURCE5_DIGITAL_SELECT_PIN,
                   SOURCE6_DIGITAL_SELECT_PIN,
                   SOURCE7_DIGITAL_SELECT_PIN,
                   SOURCE8_DIGITAL_SELECT_PIN,
                   SOURCE9_USB_SELECT_PIN};
#define FIRST_DIGITAL_SRC_IDX 4
#define USB_SRC_IDX           8

#define EEPROM_SOURCE_ADDRESS 0

IRrecv irrecv(IR_RECV_PIN);
Encoder sourceKnob(SOURCE_KNOB_PIN1, SOURCE_KNOB_PIN2);

decode_results results;
long mKnobPos = 0;

volatile boolean mTrigger = false;
volatile boolean mTriggerChanged = false;

boolean mMute = false;
boolean mMuteChanged = false;

unsigned long mPreviousIR = 0;

/* definition for APPLE REMOTE */
#ifdef APPLE_REMOTE
#define DECODE_TYPE  NEC
#define VOLUME_PLUS  2011287555
#define VOLUME_MINUS 2011279363
#define SOURCE_PLUS  2011291651
#define SOURCE_MINUS 2011238403
#define ON_OFF       2011250691
#define DSP_ON_OFF   2011281923
#define MUTE         2011265539
#define REPEATE      4294967295
#endif

volatile boolean mPowerUp = false;
volatile boolean mPowerUpChanged = false;
volatile boolean mPoweredThxTrigger = false;
volatile boolean mPoweredDownUserForced = false;

// ALPS Motorized potentiometer takes 12s to go through its 300°.
// Volume is segmented in 100 => 300°/100 = 3° for each segment.
// So each segment duration is 120ms.
#define VOLUME_TIME 120


void changeSource(short newVal) {
  mSource = newVal;
  if (mSource < 0) mSource = NB_SOURCES;
  if (mSource > NB_SOURCES-1) mSource = 0;
  mSourceChanged = 1;
}

void changePowerUp(boolean newVal) {
  cli();
  mPowerUp = newVal;
  mPowerUpChanged = 1;
  sei();
}

// Install the interrupt routine.
void isrPwrService() {
  changePowerUp(!mPowerUp);
  cli();
  if (mPowerUp)
    mPoweredThxTrigger = false; // If ISR is triggered, it is due to button pushed.
  else
    mPoweredDownUserForced = true;
  sei();
#ifdef DEBUG
  Serial.println("ISR PWR");
#endif
}

// Volume Up and Down
void VolumeUp() {
#ifdef DEBUG
  Serial.println("Volume UP");
#endif
  digitalWrite(VOLUME_UP_PIN, LOW);     // Volume ON
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF
  delay(VOLUME_TIME);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF  
}
void VolumeDown() {
#ifdef DEBUG
  Serial.println("Volume DOWN");
#endif
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, LOW);   // Volume ON
  delay(VOLUME_TIME);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF  
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
#endif

  // Select Analog inputs or Digital Inputs.
  pinMode(SELECT_ANALOG_NUM_PIN, OUTPUT);
  digitalWrite(SELECT_ANALOG_NUM_PIN, LOW);

  // PowerUp
  mPowerUp = 0;
  // Connect to ON / OFF button.
  pinMode(ON_OFF_BUTTON_PIN, INPUT);
  attachInterrupt(ON_OFF_BUTTON_PIN, isrPwrService, RISING);
  
  pinMode(ON_OFF_LED_PIN, OUTPUT);
  digitalWrite(ON_OFF_LED_PIN, LED_OFF);  // LED PWR OFF

  // Trigger
  mTrigger = 0;
  pinMode(ON_OFF_TRIGGER_PIN, INPUT);

  // List of Output for Volume Up and Down.
  pinMode(VOLUME_UP_PIN, OUTPUT);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  pinMode(VOLUME_DOWN_PIN, OUTPUT);
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF

  // List of Output for sources selection.
  for (int i=0 ; i<NB_SOURCES ; i++) {
    pinMode(mSources[i], OUTPUT);
    digitalWrite(mSources[i], LED_OFF);  // LED off
  }

  // Restore source from EEPROM
  changeSource(EEPROM.read(EEPROM_SOURCE_ADDRESS));
  
  // Start the IR receiver
  irrecv.enableIRIn();
}

void loop() {
#ifdef DEBUG2
  unsigned int *irRawBuf;
#endif
  long newKobPos;

  // ############## IRDA Mgmt ##############
  if (irrecv.decode(&results)) {
    if (results.decode_type == DECODE_TYPE) {
#ifdef DEBUG2
      Serial.print("irRawBuf: ");
      for (int i = 0; i < results.rawlen; i++) {
        Serial.print(results.rawbuf[i]);
        Serial.print(" ");
      }  
    Serial.println();
#endif

      // Check values comming from Remote control
      // Is Menu ? => Used to handle Power ON / OFF
      if(results.value == ON_OFF) {
#ifdef DEBUG
        Serial.println("ON_OFF");
#endif
        changePowerUp(!mPowerUp);
        cli();
        if (mPowerUp)
          mPoweredThxTrigger = false;
        else
          mPoweredDownUserForced = true;
        sei();
      }

      if (mPowerUp) {
        // Is Volume Up ?
        if(results.value == VOLUME_PLUS) {
#ifdef DEBUG
          Serial.println("Volume Up");
#endif
          VolumeUp();
        }
        // Is Volume Down ?
        if(results.value == VOLUME_MINUS) {
#ifdef DEBUG
          Serial.println("Volume Down");
#endif
          VolumeDown();
        }
        // Is Source plus ?
        if(results.value == SOURCE_PLUS) {
#ifdef DEBUG
          Serial.println("Source Plus");
#endif
          changeSource(mSource+1);
        }
        // Is Source minus ?
        if(results.value == SOURCE_MINUS) {
#ifdef DEBUG
          Serial.println("Source Minus");
#endif
          changeSource(mSource-1);
        }
        // Is Mute ?
        if(results.value == MUTE) {
#ifdef DEBUG
          Serial.println("Mute");
#endif
        }
        // Is Repeate ?
        // In that case only handle Volume UP and Down.
        if(results.value == REPEATE) {
#ifdef DEBUG
          Serial.println("Repeate");
#endif
          if(mPreviousIR == VOLUME_PLUS) {
            VolumeUp();
          }
          else if (mPreviousIR == VOLUME_MINUS) {
            VolumeDown();
          }
        }

        irrecv.blink13(true);
      }// if (mPowerUp)
      
      if (results.value != REPEATE) mPreviousIR = results.value;
    }
    else {
#ifdef DEBUG2
      Serial.print("results.decode_type = ");
      Serial.println(results.decode_type);
      Serial.print("results.value = ");
      Serial.println(results.value);
      
      Serial.print("irRawBuf: ");
      for (int i = 0; i < results.rawlen; i++) {
        Serial.print(results.rawbuf[i]);
        Serial.print(" ");
      }
    Serial.println();
#endif
    }
    irrecv.resume(); // Receive the next value
  }

  // ############## ON / OFF Button Mgmt  ##############
  if (mPowerUp) {
    if (mPowerUpChanged) {
      cli();
      mPowerUpChanged = 0;
      sei();
      // Restore source from EEPROM
      changeSource(EEPROM.read(EEPROM_SOURCE_ADDRESS));
      
      digitalWrite(ON_OFF_LED_PIN, LED_ON); // LED ON
    }

    if (digitalRead(ON_OFF_TRIGGER_PIN) == false && mPoweredThxTrigger == true) {
      changePowerUp(!mPowerUp);
    }

    // ############## Knob Mgmt ##############
    newKobPos = sourceKnob.read();
    if (newKobPos != mKnobPos) {
#ifdef DEBUG2
      Serial.print("newKobPos = ");
      Serial.print(newKobPos);
      Serial.println();
#endif
      if (newKobPos < mKnobPos) {
        if ((newKobPos % 4) == 0) {
          changeSource(mSource-1);
        }
      } else {
        if ((newKobPos % 4) == 0) {
          changeSource(mSource+1);
        }
      }
      //sourceKnob.write(0);
      mKnobPos = newKobPos;

#ifdef DEBUG2
      Serial.print("mSource = ");
      Serial.print(mSource);
      Serial.println();
#endif
    }


    // ############## Source Mgmt ##############
    if (mSourceChanged) {
      mSourceChanged = 0;
#ifdef DEBUG
      Serial.print("mSource = ");
      Serial.print(mSource);
      Serial.println();
#endif
      for (int i=0 ; i<NB_SOURCES ; i++) {
        if (i=mSource) digitalWrite(mSources[i], LED_ON);   // LED on
        else digitalWrite(mSources[i], LED_OFF);   // LED off
      }
      
      // Handle Digital Source
      if (mSource >= FIRST_DIGITAL_SRC_IDX)
        digitalWrite(SELECT_ANALOG_NUM_PIN, HIGH);
      else
        digitalWrite(SELECT_ANALOG_NUM_PIN, LOW);
    }

  }// if (mPowerUp)
    
  // ############## Shut Down Mgmt ##############
  else {
    // Reset knob value
    sourceKnob.write(0);
    mKnobPos = 0;
    newKobPos = 0;

    if (digitalRead(ON_OFF_TRIGGER_PIN) && mPoweredDownUserForced == false) {
      changePowerUp(!mPowerUp);
      cli();
      mPoweredThxTrigger = true;
      sei();
    }

    if (mPowerUpChanged) {
      cli();
      mPowerUpChanged = 0;
      sei();

#ifdef DEBUG
      Serial.println("Power Down");
#endif

      // Stop blinking
      irrecv.blink13(false);

      // Store source to EEPROM
      EEPROM.write(EEPROM_SOURCE_ADDRESS, mSource);

      // reset source LED.
      for (int i=0 ; i<NB_SOURCES ; i++) {
        digitalWrite(mSources[i], LED_OFF);   // LED off
      }
      
      digitalWrite(ON_OFF_LED_PIN, LED_OFF); // LED off
    }
  }

}
