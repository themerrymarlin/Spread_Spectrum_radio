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

bool blinkState = false;
bool currently_tx;

uint32_t freq;

bool isTransmitter=true;

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
  Serial.println("press the switch to begin...");
  
  while (digitalRead(SWITCH_PIN));
  
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
  radio.setSQHiThresh(-50);
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
  radio.setVolume1(0x8);
  radio.setVolume2(0x8);

}


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
    long timer = millis() + timeout;                                              // Setup the timeout value
    if(timeout == 0) { timer = 4294967295; }                                      // If we want to wait forever, set it to the max millis()    
    Serial.println( "waitForAciviity:: Going waiting for signal");
    while(timer > millis()) {                                                     // while our timer is not timed out.
        rssi = radio.readRSSI();   // Read signal strength
        //Serial.println(rssi);
        if ( rssi > activityRSSI ){
          Serial.println("waitForActivity:: ActivityDetected on the channel");
          timer = millis()+activitywindow;
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
          while( millis()-timer < FUDGE_FACTOR_FOR_ENDING_COMMUNICATION ){
            rssi = radio.readRSSI();
            Serial.println(rssi);
            if ( rssi <= emptyRSSI ){
              Serial.println("waitForActivity:: Activity ended within predetermined window");
              
              return true;
            }
          }
          Serial.println(millis()-timer);
          Serial.println("waitForActivity:: Activity did not end within the predetermined window");
          return false; // Timeout, we started activity but the activity is longer than our defined channel useage
        }
    }
    return false;                                                                  // Timed out
}


void loop() {  
  //Switch between transmit and recieve depending on transmit button
  if (!digitalRead(SWITCH_PIN))
  {
    if (!currently_tx) 
    {
      currently_tx = true;
      
      // set to transmit
      radio.setModeTransmit();
      Serial.println("Tx");
      //radio.setTxSourceMic();
      //radio.setRfPower(1);

      if (isTransmitter==true){
        tone(PWM_PIN, 6000, 1000);
        delay(1000);
      }
      
    }
  } else if (currently_tx) {
    radio.setModeReceive();
    currently_tx = false;
    Serial.println("Rx");
    if(isTransmitter==true){
      Serial.println("Waiting For ACK Activity");
        if (waitForActivity(0,500, -75)){
          //delay(500);
          if(waitForActivity(0,500, -75)){
            Serial.println("RECIEVED ACKNOWLEDGE!!!"); 
          } else {
            Serial.println("failed second pulse");
          }
        }
        else{
          Serial.println("failed first pulse");
        }
    }
  }

 /* if( cur_freq == 435000 ){
    cur_freq = 430000;
  }else{
    cur_freq = cur_freq + 500;  
  }
  
  radio.frequency(cur_freq);
  Serial.print("Frequency: ");
  Serial.println(cur_freq);
  delay(500);
` */

  //Serial.println(radio.readRSSI());
  
  if ( isTransmitter==false){
    Serial.println("Waiting For ACK Activity");
    boolean temp=waitForActivity(0,1000, -75);
    Serial.println(temp);
    if (temp==1){
      Serial.println("transmitting ack");
      currently_tx = true;
      radio.setModeTransmit();
      tone(PWM_PIN,6000,500);
      delay(500);
      radio.setModeReceive();
      Serial.println("creating silence period");
      delay(500);
      radio.setModeTransmit();
      Serial.println("transmitting second pulse");
      tone(PWM_PIN,6000,500);
      delay(500);
    }
  }
  
  
  
  //delay(100);
  
}



