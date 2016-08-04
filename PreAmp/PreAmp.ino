#include <IRremote.h>
#include <Encoder.h>
#include <EEPROM.h>

/* This code is used to be run on Teensy 2++ */

#define DEBUG
#define DEBUG2

#define APPLE_REMOTE // Use APPLE Remote control.

/************ PINs Definition ************/
/* ON/OFF */
#define ON_OFF_TRIGGER_PIN            1
#define ON_OFF_LED_PIN               38
#define ON_OFF_BUTTON_PIN            27

/* IRDA */
#define IR_RECV_PIN                   0

/* Source Selection */
#define SOURCE_KNOB_PIN1              2
#define SOURCE_KNOB_PIN2              3
#define NB_SOURCES 7
#define SOURCE1_SELECT_PIN            4 /* SPDIF Coax 1 */
#define SOURCE2_SELECT_PIN            5 /* SPDIF Coax 2 */
#define PIN6_RESERVED                 6 /* PIN 6 is dedicated to on board led */
#define SOURCE3_SELECT_PIN            7 /* SPDIF Optical 1 */
#define SOURCE4_SELECT_PIN            8 /* SPDIF Optical 2 */
#define SOURCE5_SELECT_PIN            9 /* USB */
#define SOURCE6_SELECT_PIN           10 /* Analog 1 Aux */
#define SOURCE7_SELECT_PIN           11 /* Analog 2 Phono */

/* Volume Mgmt */
#define VOLUME_UP_PIN                39 /* VOLUME MOTOR UP */
#define VOLUME_DOWN_PIN              40 /* VOLUME MOTOR DOWN */
#define VOLUME_SENSE_GENERAL         A3 /* PIN 41 */
#define VOLUME_SENSE_BALANCE         A4 /* PIN 42 */
#define VOLUME_SENSE_BASS            A5 /* PIN 43 */
#define VOLUME_SENSE_MEDIUM          A6 /* PIN 44 */
#define VOLUME_SENSE_TWEETER         A7 /* PIN 45 */
#define VOLUME_SELECT_TWEETER_RIGHT  12
#define VOLUME_SELECT_TWEETER_LEFT   13
#define VOLUME_SELECT_MEDIUM_RIGHT   14
#define VOLUME_SELECT_MEDIUM_LEFT    15
#define VOLUME_SELECT_BASS_RIGHT     16
#define VOLUME_SELECT_BASS_LEFT      17
#define VOLUME_BIT_7                 26
#define VOLUME_BIT_6                 25
#define VOLUME_BIT_5                 24
#define VOLUME_BIT_4                 23
#define VOLUME_BIT_3                 22
#define VOLUME_BIT_2                 21
#define VOLUME_BIT_1                 20
#define VOLUME_BIT_0                 19

/*****************************************/

#define LED_ON HIGH
#define LED_OFF LOW

byte mSource = 0;
boolean mSourceChanged = 0;
byte mSources[] = {SOURCE1_SELECT_PIN,  /* Digital SPDIF Coax 1 */
                   SOURCE2_SELECT_PIN,  /* Digital SPDIF Coax 2 */
                   SOURCE3_SELECT_PIN,  /* Digital SPDIF Optical 1 */
                   SOURCE4_SELECT_PIN,  /* Digital SPDIF Optical 2 */
                   SOURCE5_SELECT_PIN,  /* Digital USB */
                   SOURCE6_SELECT_PIN,  /* Analog 1 Aux */
                   SOURCE7_SELECT_PIN}; /* Analog 2 Phono */

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
// Volume is segmented in 128 => 300°/128 = 2.34° for each segment.
// So each segment duration is 93.75ms => 94ms.
#define VOLUME_TIME 94

int mVolumeGeneral         = 0;
int mPreviousVolumeGeneral = 0;
int mBalance               = 0;
int mPreviousBalance       = 0;
int mVolumeTweeter         = 0;
int mPreviousVolumeTweeter = 0;
int mVolumeMedium          = 0;
int mPreviousVolumeMedium  = 0;
int mVolumeBass            = 0;
int mPreviousVolumeBass    = 0;
boolean mVolumeChanged     = false;
/* Nexts values need to be calibrated according to max value of each potentiometer */
#define MAX_VOLUME_TWEETER 127
#define MAX_VOLUME_MEDIUM  127
#define MAX_VOLUME_BASS    127
#define MID_VOLUME_BALANCE  63

void setVolumeValue(int val) {
  digitalWrite(VOLUME_BIT_0, ( val       & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_1, ((val >> 1) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_2, ((val >> 2) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_3, ((val >> 3) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_4, ((val >> 4) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_5, ((val >> 5) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_6, ((val >> 6) & 1) ? HIGH : LOW );
  digitalWrite(VOLUME_BIT_7, ((val >> 7) & 1) ? HIGH : LOW );
}

void changeSource(byte newVal) {
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
void VolumeUpMotor() {
#ifdef DEBUG
  Serial.println("Volume UP");
#endif
  digitalWrite(VOLUME_UP_PIN, LOW);     // Volume ON
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF
  delay(VOLUME_TIME);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF  
}
void VolumeDownMotor() {
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

  /* PowerUp */
  mPowerUp = 0;
  /* Connect to ON / OFF button. */
  pinMode(ON_OFF_BUTTON_PIN, INPUT);
  attachInterrupt(ON_OFF_BUTTON_PIN, isrPwrService, RISING);
  
  pinMode(ON_OFF_LED_PIN, OUTPUT);
  digitalWrite(ON_OFF_LED_PIN, LED_OFF);  // LED PWR OFF

  /* Trigger */
  mTrigger = 0;
  pinMode(ON_OFF_TRIGGER_PIN, INPUT);

  /* Volume Mgmt setup */
  pinMode(VOLUME_UP_PIN, OUTPUT);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  pinMode(VOLUME_DOWN_PIN, OUTPUT);
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF


  /* List of Output for sources selection. */
  for (int i=0 ; i<NB_SOURCES ; i++) {
    pinMode(mSources[i], OUTPUT);
    digitalWrite(mSources[i], LED_OFF);  // LED off
  }

  pinMode(VOLUME_SELECT_TWEETER_RIGHT, OUTPUT);
  digitalWrite(VOLUME_SELECT_TWEETER_RIGHT, LOW);
  pinMode(VOLUME_SELECT_TWEETER_LEFT, OUTPUT);
  digitalWrite(VOLUME_SELECT_TWEETER_LEFT, LOW);
  pinMode(VOLUME_SELECT_MEDIUM_RIGHT, OUTPUT);
  digitalWrite(VOLUME_SELECT_MEDIUM_RIGHT, LOW);
  pinMode(VOLUME_SELECT_MEDIUM_LEFT, OUTPUT);
  digitalWrite(VOLUME_SELECT_MEDIUM_LEFT, LOW);
  pinMode(VOLUME_SELECT_BASS_RIGHT, OUTPUT);
  digitalWrite(VOLUME_SELECT_BASS_RIGHT, LOW);
  pinMode(VOLUME_SELECT_BASS_LEFT, OUTPUT);
  digitalWrite(VOLUME_SELECT_BASS_LEFT, LOW);

  pinMode(VOLUME_BIT_7, OUTPUT);
  digitalWrite(VOLUME_BIT_7, LOW);
  pinMode(VOLUME_BIT_6, OUTPUT);
  digitalWrite(VOLUME_BIT_6, LOW);
  pinMode(VOLUME_BIT_5, OUTPUT);
  digitalWrite(VOLUME_BIT_5, LOW);
  pinMode(VOLUME_BIT_4, OUTPUT);
  digitalWrite(VOLUME_BIT_4, LOW);
  pinMode(VOLUME_BIT_3, OUTPUT);
  digitalWrite(VOLUME_BIT_3, LOW);
  pinMode(VOLUME_BIT_2, OUTPUT);
  digitalWrite(VOLUME_BIT_2, LOW);
  pinMode(VOLUME_BIT_1, OUTPUT);
  digitalWrite(VOLUME_BIT_1, LOW);
  pinMode(VOLUME_BIT_0, OUTPUT);
  digitalWrite(VOLUME_BIT_0, LOW);

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
          VolumeUpMotor();
        }
        // Is Volume Down ?
        if(results.value == VOLUME_MINUS) {
#ifdef DEBUG
          Serial.println("Volume Down");
#endif
          VolumeDownMotor();
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
            VolumeUpMotor();
          }
          else if (mPreviousIR == VOLUME_MINUS) {
            VolumeDownMotor();
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
    }

    // ############## Volume Mgmt ##############
    /*
     * Volume is able to be managed between -63.5dB up to 0dB by 0.5dB steps.
     * Since analogRead provide a value between 0 and 1023,
     * it is needed to divied it by 4 to have a value between 0 and 127.
     * Where 127 coresponds to 0dB and 0 to -63.5dB.
     */
    mVolumeGeneral = analogRead(VOLUME_SENSE_GENERAL) >> 2;
    if (mPreviousVolumeGeneral != mVolumeGeneral) {
      mPreviousVolumeGeneral = mVolumeGeneral;
      mVolumeChanged = true;
    }
    /* Regarding Balance, midle corresponds to 63, full left corresponds to 0 and full right to 127 */
    mBalance = analogRead(VOLUME_SENSE_BALANCE) >> 2;
    if (mPreviousBalance != mBalance) {
      mPreviousBalance = mBalance;
      mVolumeChanged = true;
    }
    mVolumeTweeter = analogRead(VOLUME_SENSE_TWEETER) >> 2;
    if (mPreviousVolumeTweeter =! mVolumeTweeter) {
      mPreviousVolumeTweeter = mVolumeTweeter;
      mVolumeChanged = true;
    }
    mVolumeMedium = analogRead(VOLUME_SENSE_MEDIUM) >> 2;
    if (mPreviousVolumeMedium =! mVolumeMedium) {
      mPreviousVolumeMedium = mVolumeMedium;
      mVolumeChanged = true;
    }
    mVolumeBass = analogRead(VOLUME_SENSE_BASS) >> 2;
    if (mPreviousVolumeBass =! mVolumeBass) {
      mPreviousVolumeBass = mVolumeBass;
      mVolumeChanged = true;
    }

    /* Compute and set Volumes Values */
    if (mVolumeChanged) {
      mVolumeChanged = false;
      /* Compute right tweeter */
      int rightTweeter = mVolumeGeneral - (MAX_VOLUME_TWEETER - mVolumeTweeter) - (MID_VOLUME_BALANCE - mBalance);
      /* Compute left tweeter */
      int leftTweeter = mVolumeGeneral - (MAX_VOLUME_TWEETER - mVolumeTweeter) - (mBalance - MID_VOLUME_BALANCE);
      /* Compute right medium */
      int rightMedium = mVolumeGeneral - (MAX_VOLUME_MEDIUM - mVolumeMedium) - (MID_VOLUME_BALANCE - mBalance);
      /* Compute left medium */
      int leftMedium = mVolumeGeneral - (MAX_VOLUME_MEDIUM - mVolumeMedium) - (mBalance - MID_VOLUME_BALANCE);
      /* Compute right bass */
      int rightBass = mVolumeGeneral - (MAX_VOLUME_BASS - mVolumeBass) - (MID_VOLUME_BALANCE - mBalance);
      /* Compute left bass */
      int leftBass = mVolumeGeneral - (MAX_VOLUME_BASS - mVolumeBass) - (mBalance - MID_VOLUME_BALANCE);
      /* TODO: Handle when one of the volume is bigger than 127 due to balance !! */
      
      /* Set Volumes values */
      setVolumeValue(rightTweeter);
      digitalWrite(VOLUME_SELECT_TWEETER_RIGHT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_TWEETER_RIGHT, LOW);
      setVolumeValue(leftTweeter);
      digitalWrite(VOLUME_SELECT_TWEETER_LEFT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_TWEETER_LEFT, LOW);
      setVolumeValue(rightMedium);
      digitalWrite(VOLUME_SELECT_MEDIUM_RIGHT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_MEDIUM_RIGHT, LOW);
      setVolumeValue(leftMedium);
      digitalWrite(VOLUME_SELECT_MEDIUM_LEFT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_MEDIUM_LEFT, LOW);
      setVolumeValue(rightBass);
      digitalWrite(VOLUME_SELECT_BASS_RIGHT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_BASS_RIGHT, LOW);
      setVolumeValue(leftBass);
      digitalWrite(VOLUME_SELECT_BASS_LEFT, HIGH);
      delay(1);
      digitalWrite(VOLUME_SELECT_BASS_LEFT, LOW);
      setVolumeValue(0);
    }

  }// if (mPowerUp)
    
  // ############## Shut Down & OFF Mgmt ##############
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
