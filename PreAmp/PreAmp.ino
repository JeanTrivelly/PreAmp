#include <IRremote.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <Wire.h>
#include <PGA2310.h>

/* This code is used to be run on Teensy 2++ */

#define DEBUG1
#define DEBUG2
//#define DEBUG_IR

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
#define VOLUME_SPI_CS                 0
#define VOLUME_SPI_SDATA_I            1
#define VOLUME_SPI_SCLK               2
#define VOLUME_ZCEN                   3
#define VOLUME_HARD_MUTE              7
PGA2310 cs3310(VOLUME_SPI_CS,
               VOLUME_SPI_SDATA_I,
               VOLUME_SPI_SCLK,
               VOLUME_ZCEN,
               VOLUME_HARD_MUTE);

#define VOLUME_UP_PIN                39 /* VOLUME MOTOR UP */
#define VOLUME_DOWN_PIN              40 /* VOLUME MOTOR DOWN */
#define VOLUME_SENSE_GENERAL          3 /* PIN A3 / 41 */
#define VOLUME_SENSE_BALANCE          4 /* PIN A4 / 42 */
#define VOLUME_SENSE_TWEETER          5 /* PIN A5 / 43 */
#define VOLUME_SENSE_MEDIUM           6 /* PIN A6 / 44 */
#define VOLUME_SENSE_BASS             7 /* PIN A7 / 45 */

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

unsigned int mVolumeGeneral             = 0;
unsigned int mVolumeGeneralRead         = 0;
unsigned int mPreviousVolumeGeneralRead = 0;
unsigned int mBalance                   = 0;
unsigned int mBalanceRead               = 0;
unsigned int mPreviousBalanceRead       = 0;
unsigned int mVolumeTweeter             = 0;
unsigned int mVolumeTweeterRead         = 0;
unsigned int mPreviousVolumeTweeterRead = 0;
unsigned int mVolumeMedium              = 0;
unsigned int mVolumeMediumRead          = 0;
unsigned int mPreviousVolumeMediumRead  = 0;
unsigned int mVolumeBass                = 0;
unsigned int mVolumeBassRead            = 0;
unsigned int mPreviousVolumeBassRead    = 0;
uint8_t mRightBass            = 0;
uint8_t mLeftBass             = 0;
uint8_t mRightMedium          = 0;
uint8_t mLeftMedium           = 0;
uint8_t mRightTweeter         = 0;
uint8_t mLeftTweeter          = 0;

boolean mVolumeChanged     = false;

#define VOLUME_PRECISION     4

/* TODO: Nexts values need to be calibrated according to max value of each potentiometer */
#define MAX_VOLUME_TWEETER 255
#define MAX_VOLUME_MEDIUM  255
#define MAX_VOLUME_BASS    255
#define MID_VOLUME_BALANCE 127


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
    /* Set all 0 */
    uint8_t volumeValues[6] = {0, 0, 0, 0, 0, 0};
    cs3310.setMultiVolume(6, volumeValues);
  }
  else {
    /* Set Volumes values */
    uint8_t volumeValues[6] = {mRightBass, mLeftBass, mRightMedium, mLeftMedium, mRightTweeter, mLeftTweeter};
    cs3310.setMultiVolume(6, volumeValues);
  }
  mMute = !mMute;
}

void hard_mute(){
  /* Set all 0 */
  uint8_t volumeValues[6] = {0, 0, 0, 0, 0, 0};
  cs3310.setMultiVolume(6, volumeValues);
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

uint8_t normalize_uint8(int16_t value) {
  if (value <= 0)   return 0;
  if (value >= 255) return 255;
  return (uint8_t)value;
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
//  attachInterrupt(ON_OFF_BUTTON_PIN, isrPwrService, RISING);
  
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

  // Set Analog reference to EXTERNAL
  analogReference(EXTERNAL);

  // Restore source from EEPROM
  changeSource(EEPROM.read(EEPROM_SOURCE_ADDRESS));
  
  // Start the IR receiver
  irrecv.enableIRIn();

  // Init CS3310
  cs3310.begin(true);
}

void loop() {

  long newKobPos;

  // ############## IRDA Mgmt ##############
  if (irrecv.decode(&results)) {
    if (results.decode_type == DECODE_TYPE) {
#ifdef DEBUG_IR
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
#ifdef DEBUG_IR
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

  if (digitalRead(ON_OFF_BUTTON_PIN)) {
    isrPwrService();
#ifdef DEBUG1
    Serial.println("power button pushed");
#endif
    delay(500);
  }

  bool volumeWriteRepeate = false;
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
#ifdef DEBUG1
      Serial.println("power up changed");
#endif
      mVolumeChanged = true;
      volumeWriteRepeate = true;
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
     * Volume is able to be managed between -95.5dB up to +31.5dB by 0.5dB steps.
     * Since analogRead provide a value between 0 and 1023,
     * it is needed to divied it by 4 to have a value between 0 and 255.
     * Where 255 coresponds to +31.5dB and 0 to -95.5dB.
     */
    mVolumeGeneralRead = analogRead(VOLUME_SENSE_GENERAL);
    int16_t sup = ((mPreviousVolumeGeneralRead + 4) >= 1023) ? 1023 : (mPreviousVolumeGeneralRead + 4);
    int16_t inf = ((mPreviousVolumeGeneralRead - 4) <=    0) ?    0 : (mPreviousVolumeGeneralRead - 4);
    if (mVolumeGeneralRead < 4) {
      mVolumeGeneral = 0;
      if (mPreviousVolumeGeneralRead >= 4) {
        mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeGeneral = ");
      Serial.println(mVolumeGeneral);
#ifdef DEBUG2
      Serial.print("mVolumeGeneralRead = ");
      Serial.println(mVolumeGeneralRead);
      Serial.print("mPreviousVolumeGeneralRead = ");
      Serial.println(mPreviousVolumeGeneralRead);
#endif
#endif
      }
      mPreviousVolumeGeneralRead = mVolumeGeneralRead;
    }
    else if (((mVolumeGeneralRead >  sup)||
              (mVolumeGeneralRead <= inf)  )) {
      mVolumeGeneral = mVolumeGeneralRead >> 2;
      mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeGeneral = ");
      Serial.println(mVolumeGeneral);
#ifdef DEBUG2
      Serial.print("mVolumeGeneralRead = ");
      Serial.println(mVolumeGeneralRead);
      Serial.print("mPreviousVolumeGeneralRead = ");
      Serial.println(mPreviousVolumeGeneralRead);
      Serial.print("sup = ");
      Serial.println(sup);
      Serial.print("inf = ");
      Serial.println(inf);
#endif
#endif
      mPreviousVolumeGeneralRead = mVolumeGeneralRead;
    }

    /*
     * Regarding Balance, midle corresponds to 63,
     * full left corresponds to 0 and full right to 127
     */
    mBalanceRead = analogRead(VOLUME_SENSE_BALANCE);
    sup = ((mPreviousBalanceRead + 4) >= 1023) ? 1023 : (mPreviousBalanceRead + 4);
    inf = ((mPreviousBalanceRead - 4) <=    0) ?    0 : (mPreviousBalanceRead - 4);
    if (mBalanceRead <  4) {
      mBalance = 0;
      if (mPreviousBalanceRead >= 4) {
        mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mBalance = ");
      Serial.println(mBalance);
#ifdef DEBUG2
      Serial.print("mBalanceRead = ");
      Serial.println(mBalanceRead);
      Serial.print("mPreviousBalanceRead = ");
      Serial.println(mPreviousBalanceRead);
#endif
#endif
      }
      mPreviousBalanceRead = mBalanceRead;
    }
    else if (((mBalanceRead >  sup)||
              (mBalanceRead <= inf)  )) {
      mBalance = mBalanceRead >> 2;
      mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mBalance = ");
      Serial.println(mBalance);
#ifdef DEBUG2
      Serial.print("mBalanceRead = ");
      Serial.println(mBalanceRead);
      Serial.print("mPreviousBalanceRead = ");
      Serial.println(mPreviousBalanceRead);
#endif
#endif
      mPreviousBalanceRead = mBalanceRead;
    }

    mVolumeTweeterRead = analogRead(VOLUME_SENSE_TWEETER);
    sup = ((mPreviousVolumeTweeterRead + 4) >= 1023) ? 1023 : (mPreviousVolumeTweeterRead + 4);
    inf = ((mPreviousVolumeTweeterRead - 4) <=    0) ?    0 : (mPreviousVolumeTweeterRead - 4);
    if (mVolumeTweeterRead <  4) {
      mVolumeTweeter = 0;
      if (mPreviousVolumeTweeterRead >= 4) {
        mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeTweeter = ");
      Serial.println(mVolumeTweeter);
#ifdef DEBUG2
      Serial.print("mVolumeTweeterRead = ");
      Serial.println(mVolumeTweeterRead);
      Serial.print("mPreviousVolumeTweeterRead = ");
      Serial.println(mPreviousVolumeTweeterRead);
#endif
#endif
      }
      mPreviousVolumeTweeterRead = mVolumeTweeterRead;
    }
    else if (((mVolumeTweeterRead >  sup)||
              (mVolumeTweeterRead <= inf)  )) {
      mVolumeTweeter = mVolumeTweeterRead >> 2;
      mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeTweeter = ");
      Serial.println(mVolumeTweeter);
#ifdef DEBUG2
      Serial.print("mVolumeTweeterRead = ");
      Serial.println(mVolumeTweeterRead);
      Serial.print("mPreviousVolumeTweeterRead = ");
      Serial.println(mPreviousVolumeTweeterRead);
#endif
#endif
      mPreviousVolumeTweeterRead = mVolumeTweeterRead;
    }

    mVolumeMediumRead = analogRead(VOLUME_SENSE_MEDIUM);
    sup = ((mPreviousVolumeMediumRead + 4) >= 1023) ? 1023 : (mPreviousVolumeMediumRead + 4);
    inf = ((mPreviousVolumeMediumRead - 4) <=    0) ?    0 : (mPreviousVolumeMediumRead - 4);
    if (mVolumeMediumRead <  4) {
      mVolumeMedium = 0;
      if (mPreviousVolumeMediumRead >= 4) {
        mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeMedium = ");
      Serial.println(mVolumeMedium);
#ifdef DEBUG2
      Serial.print("mVolumeMediumRead = ");
      Serial.println(mVolumeMediumRead);
      Serial.print("mPreviousVolumeMediumRead = ");
      Serial.println(mPreviousVolumeMediumRead);
#endif
#endif
      }
      mPreviousVolumeMediumRead = mVolumeMediumRead;
    }
    else if (((mVolumeMediumRead >  sup)||
              (mVolumeMediumRead <= inf)  )) {
      mVolumeMedium = mVolumeMediumRead >> 2;
      mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeMedium = ");
      Serial.println(mVolumeMedium);
#ifdef DEBUG2
      Serial.print("mVolumeMediumRead = ");
      Serial.println(mVolumeMediumRead);
      Serial.print("mPreviousVolumeMediumRead = ");
      Serial.println(mPreviousVolumeMediumRead);
#endif
#endif
      mPreviousVolumeMediumRead = mVolumeMediumRead;
    }

    mVolumeBassRead = analogRead(VOLUME_SENSE_BASS);
    sup = ((mPreviousVolumeBassRead + 4) >= 1023) ? 1023 : (mPreviousVolumeBassRead + 4);
    inf = ((mPreviousVolumeBassRead - 4) <=    0) ?    0 : (mPreviousVolumeBassRead - 4);
    if (mVolumeBassRead <  4) {
      mVolumeBass = 0;
      if (mPreviousVolumeBassRead >= 4) {
        mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeBass = ");
      Serial.println(mVolumeBass);
#ifdef DEBUG2
      Serial.print("mVolumeBassRead = ");
      Serial.println(mVolumeBassRead);
      Serial.print("mPreviousVolumeBassRead = ");
      Serial.println(mPreviousVolumeBassRead);
#endif
#endif
      }
      mPreviousVolumeBassRead = mVolumeBassRead;
    }
    else if (((mVolumeBassRead >  sup)||
              (mVolumeBassRead <= inf)  )) {
      mVolumeBass = mVolumeBassRead >> 2;
      mVolumeChanged = true;
#ifdef DEBUG1
      Serial.print("mVolumeBass = ");
      Serial.println(mVolumeBass);
#ifdef DEBUG2
      Serial.print("mVolumeBassRead = ");
      Serial.println(mVolumeBassRead);
      Serial.print("mPreviousVolumeBassRead = ");
      Serial.println(mPreviousVolumeBassRead);
#endif
#endif
      mPreviousVolumeBassRead = mVolumeBassRead;
    }

    /* Compute and set Volumes Values */
    if (mVolumeChanged) {
      mVolumeChanged = false;

      uint8_t val1, val2, val3;
      val1 = val2 = val3 = 0;

      val2 = normalize_uint8(MID_VOLUME_BALANCE - mBalance);
      val3 = normalize_uint8(mBalance - MID_VOLUME_BALANCE);
      /* Compute right bass */
      val1 = normalize_uint8(MAX_VOLUME_BASS - mVolumeBass);
      mRightBass = normalize_uint8(mVolumeGeneral - val1 - val2);
      /* Compute left bass */
      mLeftBass  = normalize_uint8(mVolumeGeneral - val1 - val3);

      /* Compute right medium */
      val1 = normalize_uint8(MAX_VOLUME_MEDIUM - mVolumeMedium);
      mRightMedium  = normalize_uint8(mVolumeGeneral - val1 - val2);
      /* Compute left medium */
      mLeftMedium   = normalize_uint8(mVolumeGeneral - val1 - val3);
      
      /* Compute right tweeter */
      val1 = normalize_uint8(MAX_VOLUME_TWEETER - mVolumeTweeter);
      mRightTweeter = normalize_uint8(mVolumeGeneral - val1 - val2);
      /* Compute left tweeter */
      mLeftTweeter  = normalize_uint8(mVolumeGeneral - val1 - val3);

#ifdef DEBUG1
      Serial.print("mRighBass = ")      ; Serial.print(mRightBass);
      Serial.print(", mLeftBass = ")    ; Serial.print(mLeftBass);
      Serial.print(", mRightMedium = ") ; Serial.print(mRightMedium);
      Serial.print(", mLeftMedium = ")  ; Serial.print(mLeftMedium);
      Serial.print(", mRightTweeter = "); Serial.print(mRightTweeter);
      Serial.print(", mLeftTweeter = ") ; Serial.println(mLeftTweeter);
#endif

      /* Set Volumes values */
      uint8_t volumeValues[6] = {mRightBass, mLeftBass, mRightMedium, mLeftMedium, mRightTweeter, mLeftTweeter};
      cs3310.setMultiVolume(6, volumeValues);
    }
    if (volumeWriteRepeate) {
      volumeWriteRepeate = false;
      delay(100);
      uint8_t volumeValues[6] = {mRightBass, mLeftBass, mRightMedium, mLeftMedium, mRightTweeter, mLeftTweeter};
      cs3310.setMultiVolume(6, volumeValues);
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
