#include <IRremote.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <Wire.h>

/* This code is used to be run on Teensy 2++ */

#define DEBUG1
#define DEBUG2

#define APPLE_REMOTE // Use APPLE Remote control.

/************ PINs Definition ************/
/* ON/OFF */
#define ON_OFF_TRIGGER_PIN            4
#define ON_LED_PIN                   20
#define OFF_LED_PIN                  21
#define ON_OFF_BUTTON_PIN            27

/* IRDA */
#define IR_RECV_PIN                   5

#define PIN6_RESERVED                 6 /* PIN 6 is dedicated to on board led */

/* Source Selection */
#define SOURCE_KNOB_PIN1             18
#define SOURCE_KNOB_PIN2             19
#define NB_SOURCES 7
#define SOURCE1_SELECT_PIN           11 /* SPDIF Coax 1 */
#define SOURCE2_SELECT_PIN           12 /* SPDIF Coax 2 */
#define SOURCE3_SELECT_PIN           13 /* SPDIF Optical 1 */
#define SOURCE4_SELECT_PIN           14 /* SPDIF Optical 2 */
#define SOURCE5_SELECT_PIN           15 /* USB */
#define SOURCE6_SELECT_PIN           16 /* Analog 1 Aux */
#define SOURCE7_SELECT_PIN           17 /* Analog 2 Phono */

/* Volume Mgmt */
#define VOLUME_I2C_SCL                0
#define VOLUME_I2C_SDA                1
#define VOLUME_UP_PIN                39 /* VOLUME MOTOR UP */
#define VOLUME_DOWN_PIN              40 /* VOLUME MOTOR DOWN */
#define VOLUME_SENSE_GENERAL         A3 /* PIN 41 */
#define VOLUME_SENSE_BALANCE         A4 /* PIN 42 */
#define VOLUME_SENSE_TWEETER         A5 /* PIN 43 */
#define VOLUME_SENSE_MEDIUM          A6 /* PIN 44 */
#define VOLUME_SENSE_BASS            A7 /* PIN 45 */

#define VOLUME_SELECT_BASS_RIGHT_ADDR     0x40 /* OUT1 on Volume Board. */
#define VOLUME_SELECT_BASS_LEFT_ADDR      0x42 /* OUT2 on Volume Board. */
#define VOLUME_SELECT_MEDIUM_RIGHT_ADDR   0x44 /* OUT3 on Volume Board. */
#define VOLUME_SELECT_MEDIUM_LEFT_ADDR    0x46 /* OUT4 on Volume Board. */
#define VOLUME_SELECT_TWEETER_RIGHT_ADDR  0x48 /* OUT5 on Volume Board. */
#define VOLUME_SELECT_TWEETER_LEFT_ADDR   0x4A /* OUT6 on Volume Board. */

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
byte mRightBass            = 0;
byte mLeftBass             = 0;
byte mRightMedium          = 0;
byte mLeftMedium           = 0;
byte mRightTweeter         = 0;
byte mLeftTweeter          = 0;
boolean mVolumeChanged     = false;
/* TODO: Nexts values need to be calibrated according to max value of each potentiometer */
#define MAX_VOLUME_TWEETER 127
#define MAX_VOLUME_MEDIUM  127
#define MAX_VOLUME_BASS    127
#define MID_VOLUME_BALANCE  63

void setVolumeValue(byte val, byte addr) {
#ifdef DEBUG1
  Serial.print("setVolumeValue val = ");
  Serial.println(val);
  Serial.print("setVolumeValue addr = ");
  Serial.println(addr);
#endif
  Wire.beginTransmission(addr);
  Wire.send(val);
  Wire.endTransmission();
}

// Volume Up and Down
void VolumeUpMotor() {
#ifdef DEBUG1
  Serial.println("Volume UP");
#endif
  digitalWrite(VOLUME_UP_PIN, LOW);     // Volume ON
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF
  delay(VOLUME_TIME);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF
}
void VolumeDownMotor() {
#ifdef DEBUG1
  Serial.println("Volume DOWN");
#endif
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, LOW);   // Volume ON
  delay(VOLUME_TIME);
  digitalWrite(VOLUME_UP_PIN, HIGH);    // Volume OFF
  digitalWrite(VOLUME_DOWN_PIN, HIGH);  // Volume OFF
}

void mute(){
  if (mMute) {
    setVolumeValue(mRightBass,    VOLUME_SELECT_BASS_RIGHT_ADDR   );
    setVolumeValue(mLeftBass,     VOLUME_SELECT_BASS_LEFT_ADDR    );
    setVolumeValue(mRightMedium,  VOLUME_SELECT_MEDIUM_RIGHT_ADDR );
    setVolumeValue(mLeftMedium,   VOLUME_SELECT_MEDIUM_LEFT_ADDR  );
    setVolumeValue(mRightTweeter, VOLUME_SELECT_TWEETER_RIGHT_ADDR);
    setVolumeValue(mLeftTweeter,  VOLUME_SELECT_TWEETER_LEFT_ADDR );
  }
  else {
    /* Add 128 since bit 7 is used for Mute. */
    setVolumeValue(mRightBass + 128,    VOLUME_SELECT_BASS_RIGHT_ADDR   );
    setVolumeValue(mLeftBass + 128,     VOLUME_SELECT_BASS_LEFT_ADDR    );
    setVolumeValue(mRightMedium + 128,  VOLUME_SELECT_MEDIUM_RIGHT_ADDR );
    setVolumeValue(mLeftMedium + 128,   VOLUME_SELECT_MEDIUM_LEFT_ADDR  );
    setVolumeValue(mRightTweeter + 128, VOLUME_SELECT_TWEETER_RIGHT_ADDR);
    setVolumeValue(mLeftTweeter + 128,  VOLUME_SELECT_TWEETER_LEFT_ADDR );
  }
  mMute = !mMute;
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
#ifdef DEBUG1
  Serial.println("ISR PWR");
#endif
}


/*
 * INIT
 */

void setup()
{
#ifdef DEBUG1
  Serial.begin(9600);
#endif

  /* PowerUp */
  mPowerUp = 0;
  /* Connect to ON / OFF button. */
  pinMode(ON_OFF_BUTTON_PIN, INPUT);
  attachInterrupt(ON_OFF_BUTTON_PIN, isrPwrService, RISING);
  
  pinMode(ON_LED_PIN, OUTPUT);
  pinMode(OFF_LED_PIN, OUTPUT);
  digitalWrite(ON_LED_PIN, LED_OFF);  // LED PWR OFF
  digitalWrite(OFF_LED_PIN, LED_ON);  // LED PWR OFF

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

  // Restore source from EEPROM
  changeSource(EEPROM.read(EEPROM_SOURCE_ADDRESS));
  
  // Start the IR receiver
  irrecv.enableIRIn();

  // Start I2C for Volume Mgmt
  Wire.begin();
}

void loop() {

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
#ifdef DEBUG1
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
#ifdef DEBUG1
          Serial.println("Volume Up");
#endif
          VolumeUpMotor();
        }
        // Is Volume Down ?
        if(results.value == VOLUME_MINUS) {
#ifdef DEBUG1
          Serial.println("Volume Down");
#endif
          VolumeDownMotor();
        }
        // Is Source plus ?
        if(results.value == SOURCE_PLUS) {
#ifdef DEBUG1
          Serial.println("Source Plus");
#endif
          changeSource(mSource+1);
        }
        // Is Source minus ?
        if(results.value == SOURCE_MINUS) {
#ifdef DEBUG1
          Serial.println("Source Minus");
#endif
          changeSource(mSource-1);
        }
        // Is Mute ?
        if(results.value == MUTE) {
#ifdef DEBUG1
          Serial.println("Mute");
#endif
          mute();
        }
        // Is Repeate ?
        // In that case only handle Volume UP and Down.
        if(results.value == REPEATE) {
#ifdef DEBUG1
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
      
      digitalWrite(ON_LED_PIN, LED_ON); // LED ON
      digitalWrite(OFF_LED_PIN, LED_OFF); // LED ON
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
#ifdef DEBUG1
      Serial.print("mSource = ");
      Serial.print(mSource);
      Serial.println();
#endif
      for (int i=0 ; i<NB_SOURCES ; i++) {
        if (i==mSource) digitalWrite(mSources[i], LED_ON);   // LED on
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
    if (mPreviousVolumeTweeter != mVolumeTweeter) {
      mPreviousVolumeTweeter = mVolumeTweeter;
      mVolumeChanged = true;
    }
    mVolumeMedium = analogRead(VOLUME_SENSE_MEDIUM) >> 2;
    if (mPreviousVolumeMedium != mVolumeMedium) {
      mPreviousVolumeMedium = mVolumeMedium;
      mVolumeChanged = true;
    }
    mVolumeBass = analogRead(VOLUME_SENSE_BASS) >> 2;
    if (mPreviousVolumeBass != mVolumeBass) {
      mPreviousVolumeBass = mVolumeBass;
      mVolumeChanged = true;
    }

    /* Compute and set Volumes Values */
    if (mVolumeChanged) {
      mVolumeChanged = false;
      /* Compute right bass */
      mRightBass    = (byte) (mVolumeGeneral - (MAX_VOLUME_BASS    - mVolumeBass)    - (MID_VOLUME_BALANCE - mBalance));
      /* Compute left bass */
      mLeftBass     = (byte) (mVolumeGeneral - (MAX_VOLUME_BASS    - mVolumeBass)    - (mBalance - MID_VOLUME_BALANCE));
      /* Compute right medium */
      mRightMedium  = (byte) (mVolumeGeneral - (MAX_VOLUME_MEDIUM  - mVolumeMedium)  - (MID_VOLUME_BALANCE - mBalance));
      /* Compute left medium */
      mLeftMedium   = (byte) (mVolumeGeneral - (MAX_VOLUME_MEDIUM  - mVolumeMedium)  - (mBalance - MID_VOLUME_BALANCE));
      /* Compute right tweeter */
      mRightTweeter = (byte) (mVolumeGeneral - (MAX_VOLUME_TWEETER - mVolumeTweeter) - (MID_VOLUME_BALANCE - mBalance));
      /* Compute left tweeter */
      mLeftTweeter  = (byte) (mVolumeGeneral - (MAX_VOLUME_TWEETER - mVolumeTweeter) - (mBalance - MID_VOLUME_BALANCE));

      /* TODO: Handle when one of the volume is bigger than 127 due to balance !! */

      /* Set Volumes values */
      /* Add 128 since bit 7 is used for Mute. */
      setVolumeValue(((mRightBass    ==0) ? 0 : mRightBass    + 128), VOLUME_SELECT_BASS_RIGHT_ADDR   );
      setVolumeValue(((mLeftBass     ==0) ? 0 : mLeftBass     + 128), VOLUME_SELECT_BASS_LEFT_ADDR    );
      setVolumeValue(((mRightMedium  ==0) ? 0 : mRightMedium  + 128), VOLUME_SELECT_MEDIUM_RIGHT_ADDR );
      setVolumeValue(((mLeftMedium   ==0) ? 0 : mLeftMedium   + 128), VOLUME_SELECT_MEDIUM_LEFT_ADDR  );
      setVolumeValue(((mRightTweeter ==0) ? 0 : mRightTweeter + 128), VOLUME_SELECT_TWEETER_RIGHT_ADDR);
      setVolumeValue(((mLeftTweeter  ==0) ? 0 : mLeftTweeter  + 128), VOLUME_SELECT_TWEETER_LEFT_ADDR );
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

#ifdef DEBUG1
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
      
      digitalWrite(ON_LED_PIN, LED_OFF); // LED off
      digitalWrite(OFF_LED_PIN, LED_ON); // LED off
    }
  }

}
