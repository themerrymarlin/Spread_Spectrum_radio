/* Hamshield
 * Example: HandyTalkie
 * This is a simple example to demonstrate HamShield receive
 * and transmit functionality.
 * Connect the HamShield to your Arduino. Screw the antenna 
 * into the HamShield RF jack. Plug a pair of headphones into 
 * the HamShield. Connect the Arduino to wall power and then 
 * to your computer via USB. After uploading this program to 
 * your Arduino, open the Serial Monitor. Press the button on 
 * the HamShield to begin setup. After setup is complete, type 
 * your desired Tx/Rx frequency, in hertz, into the bar at the 
 * top of the Serial Monitor and click the "Send" button. 
 * To test with another HandyTalkie (HT), key up on your HT 
 * and make sure you can hear it through the headphones 
 * attached to the HamShield. Key up on the HamShield by 
 * holding the button.
*/

#include <HamShield.h>

// create object for radio
HamShield radio;

#define LED_PIN 13
#define RSSI_REPORT_RATE_MS 100

//TODO: move these into library
#define PWM_PIN 3
#define RESET_PIN A3
#define SWITCH_PIN 2

const uint32_t HOP_FREQ_LOWER = 430000;
const uint32_t HOP_FREQ_UPPER = 435000;
const uint32_t HOP_FREQ_INCREMENT = 500;
const int HOP_FREQ_WAIT_TIME_MS = 1000;

const int RADIO_ACTIVE_RSSI = -75;
const int RADIO_EMPTY_RSSI = -100;

bool blinkState = false;

bool currently_tx = false;
bool isTransmitter=false;
bool isInTransmission=false;
bool justBeganTransmission = false;

long latency = 0;

uint32_t freq;

unsigned long rssi_timeout;

void setup() {
  // NOTE: if not using PWM out, it should be held low to avoid tx noise
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);
  
  // prep the switch
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  
  // set up the reset control pin
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, LOW);
  
  
  // initialize serial communication
  Serial.begin(9600);
//  Serial.println("press the switch to begin...");  
//  while (digitalRead(SWITCH_PIN));
  
  // let the AU ot of reset
  digitalWrite(RESET_PIN, HIGH);
  
  Serial.println("beginning radio setup");

  // verify connection
  Serial.println("Testing device connections...");
  Serial.println(radio.testConnection() ? "RDA radio connection successful" : "RDA radio connection failed");

  // initialize device
  Serial.println("Initializing I2C devices...");
  radio.initialize(); // initializes automatically for UHF 12.5kHz channel

  Serial.println("setting default Radio configuration");
  radio.dangerMode();

  // set frequency
  Serial.println("changing frequency");
  
  radio.setSQOn();
  radio.setSQHiThresh(-75);
  radio.setSQLoThresh(-90);
  freq = 435000;
  radio.frequency(freq);
  
  // set to receive
  
  radio.setModeReceive();
  currently_tx = false;
  Serial.print("config register is: ");
  Serial.println(radio.readCtlReg());
  Serial.println(radio.readRSSI());
  
/*
  // set to transmit
  radio.setModeTransmit();
  // maybe set PA bias voltage
  Serial.println("configured for transmit");
  radio.setTxSourceMic();
  
  
  */  
  radio.setRfPower(8);
    
  // configure Arduino LED for
  pinMode(LED_PIN, OUTPUT);
  rssi_timeout = 0;
  radio.setVolume1(0xA);
  radio.setVolume2(0xA);

  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switch_isr, FALLING);
  
} //setup


/* 
  -- waitForActivity (based on HamShield::waitForChannel)
  - Wait until there is activity on the channel for a specific period of time.
  - Includes the option to wait for a period of activity followed by a specified period of inactivity

  timeout - (default 0~forever) how long to wait on an empty channel until activity (in mS) 
  activitywindow - (default 0) the length of inactivity on the channel to look for. (in mS)
  setRSSI - (default HAMSHIELD_EMPTY_CHANNEL_RSSI=> -110) The dBm threshold to consider a channel empty

  NOTE - RSSI fluctuates back up to EMPTY Channel thresholds during voice communications but stays high around -50<->-25 when using a 1750hz tone from the baofeng
   
*/

bool waitForActivity(long timeout = 0, long activitywindow = 0, int activityRSSI = -75, int emptyRSSI = HAMSHIELD_EMPTY_CHANNEL_RSSI) { 
    int16_t rssi = 0;                                                              // Set RSSI to max received signal
    for(int x = 0; x < 20; x++) { rssi = radio.readRSSI(); }                       // "warm up" to get past RSSI hysteresis     
    Serial.println( "waitForAciviity:: Going waiting for signal");
    long timer = millis() + timeout;                                              // Setup the timeout value
    if(timeout == 0) { timer = 4294967295; }                                      // If we want to wait forever, set it to the max millis()    
    while(timer > millis()) {                                                     // while our timer is not timed out.
        rssi = radio.readRSSI();   // Read signal strength
        if ( justBeganTransmission == true ){
          //We are beginning a transmission and are the transmitter. Exit so we can begin sync routine.
          return false;
        }
        //Serial.println(rssi);
        if ( rssi > activityRSSI ){
          timer = millis()+activitywindow;
          Serial.println("waitForActivity:: ActivityDetected on the channel");
          while(timer > millis()){
            rssi = radio.readRSSI();
            //if the channel goes dead
            if( rssi <= emptyRSSI ){
                Serial.println("waitForActivity:: Channel went dead before the end of the activity window");
                return false; // Timeout, the channel went dead
            }
          }
          Serial.println("waitForActivity:: End of activity window checking that channel is dead");
          int FUDGE_FACTOR_FOR_ENDING_COMMUNICATION=100;
          long diff_time = millis()-timer;
          
          while( diff_time < FUDGE_FACTOR_FOR_ENDING_COMMUNICATION ){
            rssi = radio.readRSSI();
            //Serial.println(rssi);
            if ( rssi <= emptyRSSI ){
              Serial.println("waitForActivity:: Activity ended within predetermined window");
              //Serial.print("waitForActivity:: FudgedFudgeFactor =~ ");
              //Serial.println( diff_time );
              latency = diff_time;
              return true;
            }
            diff_time = millis()-timer;
          }
          //Serial.println(millis()-timer);
          Serial.println("waitForActivity:: Activity did not end within the predetermined window");
          return false; // Timeout, we started activity but the activity is longer than our defined channel useage
        }
    }
    return false;                                                                  // Timed out
}

/*  hopFreq 
 *   
 *  -Performs frequency hopping routine
 * 
 */

void hopFreq(){
            
  uint32_t cur_freq = radio.getFrequency();
  if (cur_freq != HOP_FREQ_LOWER) {
      Serial.println("hopFreq:: Current frequency is expected to be the lower bound of the hopping scheme. Setting to that value now");
      radio.frequency(HOP_FREQ_LOWER);
      cur_freq = HOP_FREQ_LOWER;
  }
  
  while ( isInTransmission ){
    if(cur_freq == HOP_FREQ_UPPER){
      cur_freq = HOP_FREQ_LOWER;      
    }else{
      cur_freq = cur_freq + HOP_FREQ_INCREMENT;
    }  
    radio.frequency(cur_freq);
    Serial.print("hopFreq:: Frequency: ");
    Serial.println(cur_freq);
    delay(HOP_FREQ_WAIT_TIME_MS);
    //need to add activity check if not transmitter
    if(!isTransmitter){
      //TODO check if channel is dead, probably for two cycles?
    }
  }
}

long debug_timer;

void switch_isr (){

  //isTransmitter=!isTransmitter;
  Serial.print("switch_isr:: isTransmitter=");
  Serial.println(isTransmitter); 
  // We need to sync before anyone can transmit.
  if ( isInTransmission == false ){
    isInTransmission = true;
    justBeganTransmission = true;
  } else{
    isInTransmission = false;
  }
} //switch_isr

bool waitForPulses(){
      if (waitForActivity(0,500, RADIO_ACTIVE_RSSI, RADIO_EMPTY_RSSI)){
          //Waiting for Second Recieve From Raduio
          if(waitForActivity(0,500, RADIO_ACTIVE_RSSI, RADIO_EMPTY_RSSI)){
            Serial.println("RECIEVED ACKNOWLEDGE!!!"); 
            //We want to move into transmit for the duration of the frequency hopping.
            return true;
          } else {
            Serial.println("failed second pulse");
            return false;
          }
        }
        else{
          Serial.println("failed first pulse");
          return false;
        }
} //waitForPulses()

bool syncRadio(){  
  if (!currently_tx) 
   {
      currently_tx = true;
      //We are now in a transmission.
      isInTransmission = true;
      
      ////// TRANSMIT INITIAL TONE /////
      radio.setModeTransmit();
      Serial.println("Tx");
      Serial.println("Transmitting Tone for initial Sync");
      tone(PWM_PIN, 6000, 1000);
      delay(1000);
//        debug_timer=millis();

      ////// RECIEVE ACKNOWLEDGE FROM RECEIVER /////
      radio.setModeReceive();
      debug_timer=millis();
      currently_tx = false;
      Serial.println("Rx");
      Serial.println("Waiting For ACK Activity");
      // Waiting for Receive pulses
      return waitForPulses();
    } else if (currently_tx) {
      Serial.println("SyncRadio:: WARNING: Why are we currently transmitting and trying to sync???");
      return false;
  }
  
} //syncRadio

void loop() {
  justBeganTransmission = false;
  
  if (isInTransmission && syncRadio()){
    radio.setModeTransmit();
    isTransmitter = true;
    currently_tx=true;
    hopFreq();
    radio.setModeReceive();
    currently_tx=false;
  }else if (!isInTransmission){
    
    Serial.println("Waiting For ACK Activity"); 
    
    if ( waitForActivity(0,1000, RADIO_ACTIVE_RSSI, RADIO_EMPTY_RSSI) ){
      //We are now in a Transmission since we're syncing.
      isInTransmission = true; 
      
      Serial.println("transmitting ack");
      currently_tx = true;
      radio.setModeTransmit();
      tone(PWM_PIN,6000,500);
      delay(500);
      radio.setModeReceive();
      currently_tx = false;
      Serial.println("creating silence period");
      delay(500);
      radio.setModeTransmit();
      currently_tx = true;
      Serial.println("transmitting second pulse");
      tone(PWM_PIN,6000,500);
      delay(500);
      radio.setModeReceive();
      currently_tx = false;
      delay(latency);//make receiver wait roughly the time for the other radio. Based upon experiments. Could change with distance, power, etc.
      //Assuming we are going into the frequency hopping.
      isTransmitter = false;
      hopFreq();
      //We are done with transmision and we can have interrupts again
      isInTransmission = false;
    } else { 
      isInTransmission = false; //reset and continue with wait
      Serial.println("Stopping activity wait");
    }
  }
 
  //delay(100);
  
}



