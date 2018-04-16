/* Hamshield
   Example: HandyTalkie
   This is a simple example to demonstrate HamShield receive
   and transmit functionality.
   Connect the HamShield to your Arduino. Screw the antenna
   into the HamShield RF jack. Plug a pair of headphones into
   the HamShield. Connect the Arduino to wall power and then
   to your computer via USB. After uploading this program to
   your Arduino, open the Serial Monitor. Press the button on
   the HamShield to begin setup. After setup is complete, type
   your desired Tx/Rx frequency, in hertz, into the bar at the
   top of the Serial Monitor and click the "Send" button.
   To test with another HandyTalkie (HT), key up on your HT
   and make sure you can hear it through the headphones
   attached to the HamShield. Key up on the HamShield by
   holding the button.
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

const uint32_t HOP_FREQ_LOWER = 430000;     //Minimum frequency for hopping routine.
const uint32_t HOP_FREQ_UPPER = 435000;     //Max frequency for hopping routine
const uint32_t HOP_FREQ_INCREMENT = 500;    //Frequency Increment between hopping channels
const int HOP_FREQ_WAIT_TIME_MS = 1000;     //Time between hops

const int RADIO_ACTIVE_RSSI = -90;
const int RADIO_EMPTY_RSSI = -110;

const int RADIO_REQ_TIMEOUT_MS = 0;       //How long do we wait to see the initial request?
const int RADIO_ACK_TIMEOUT_MS = 0;       //How long do we wait until we see an ACK?
const int RADIO_REQ_LENGTH_MS = 1000;       //How long is the request length held high?
const int RADIO_ACK_LENGTH_MS = 500;        //How long is the acknowledge tone held high?
const int RADIO_ACK_SILENCE_MS = 500;       //How long is the acknowledge silence period on the channel? Or how long between ACKS on the ACKNOWLEDGE. 


bool blinkState = false;

// There is no method to get the current tx or rx mode from the radio. This is used to keep track of it. See setRadioToReceive, setRadioToTransmit, and isRadioInTxMode.
bool is_radio_in_tx_mode = false;

bool isTransmitter = false;
bool isInTransmission = false;
bool justBeganTransmission = false;


long latency = 0;
long last_switch_isr_time = 0;

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
  //Serial.begin(9600);
  //  Serial.println("press the switch to begin...");
  //  while (digitalRead(SWITCH_PIN));

  // let the AU ot of reset
  digitalWrite(RESET_PIN, HIGH);

  //Serial.println("beginning radio setup");

  // verify connection
  //Serial.println("Testing device connections...");
  //Serial.println(radio.testConnection() ? "RDA radio connection successful" : "RDA radio connection failed");

  // initialize device
  //Serial.println("Initializing I2C devices...");
  radio.initialize(); // initializes automatically for UHF 12.5kHz channel

  //Serial.println("setting default Radio configuration");
  radio.dangerMode();

  // set frequency
  //Serial.println("changing frequency");

  radio.setSQOn();
  radio.setSQHiThresh(-80);
  radio.setSQLoThresh(-110);
  // Initialize to the lower hopping frequency
  radio.frequency(HOP_FREQ_LOWER);

  // set to receive
  setRadioToReceive();
//  Serial.print("config register is: ");
//  Serial.println(radio.readCtlReg());
//  Serial.println(radio.readRSSI());
  radio.setRfPower(8);

  // configure Arduino LED for
  pinMode(LED_PIN, OUTPUT);
  rssi_timeout = 0;
  radio.setVolume1(0xA);
  radio.setVolume2(0xA);

  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switch_isr, FALLING);

} //setup

void loop() {
  justBeganTransmission = false;

  if (isInTransmission && syncRadio()) {
    setRadioToTransmit();
    isTransmitter = true;
    hopFreq();
    setRadioToReceive();
  } else if (!isInTransmission) {

    //Serial.println("Waiting For ACK Activity");

    if ( waitForSyncREQ(RADIO_REQ_TIMEOUT_MS, RADIO_REQ_LENGTH_MS, RADIO_ACTIVE_RSSI, RADIO_EMPTY_RSSI) ) {
      syncAndRecieve();
    } else {
      //isInTransmission = false; //reset and continue with wait
      //Serial.println("LOOP:: Stopping activity wait");
    }
  }
} //loop

/*  hopFreq

    -Performs frequency hopping routine

*/
void hopFreq() {

  uint32_t cur_freq = radio.getFrequency();
  if (cur_freq != HOP_FREQ_LOWER) {
    //Serial.println("hopFreq:: Current frequency is expected to be the lower bound of the hopping scheme. Setting to that value now");
    radio.frequency(HOP_FREQ_LOWER);
    cur_freq = HOP_FREQ_LOWER;
  }

  while ( isInTransmission ) {
    if (cur_freq == HOP_FREQ_UPPER) {
      cur_freq = HOP_FREQ_LOWER;
    } else {
      cur_freq = cur_freq + HOP_FREQ_INCREMENT;
    }
    radio.frequency(cur_freq);
    //Serial.print("hopFreq:: Frequency: ");
    //Serial.println(cur_freq);
    delay(HOP_FREQ_WAIT_TIME_MS);
    //need to add activity check if not transmitter
    if (!isTransmitter) {
      //TODO check if channel is dead, probably for two cycles?
    }
  }
  //Transmitter should send tone on exit.
  if ( isRadioInTxMode() ){
    if (cur_freq == HOP_FREQ_UPPER) {
      cur_freq = HOP_FREQ_LOWER;
    } else {
      cur_freq = cur_freq + HOP_FREQ_INCREMENT;
    }
    radio.frequency(cur_freq);
    tone(PWM_PIN,6000, HOP_FREQ_WAIT_TIME_MS);
    delay (HOP_FREQ_WAIT_TIME_MS);
  }
  //Reset our radio to the lower hopping frequnecy.
  radio.frequency(HOP_FREQ_LOWER);
}

void sendSyncACK(int radio_ack_length_ms, int radio_ack_silence_ms){
    setRadioToTransmit();
    tone(PWM_PIN, 6000, radio_ack_length_ms);
    delay(radio_ack_length_ms);
    setRadioToReceive();
    //Serial.println("creating silence period");
    delay(radio_ack_silence_ms);
    setRadioToTransmit();
    //Serial.println("transmitting second pulse");
    tone(PWM_PIN, 6000, radio_ack_length_ms);
    delay(radio_ack_length_ms);
    setRadioToReceive();
}

void sendSyncREQ(int radio_req_length_ms){
    //Make sure we are in transmit mode.
    setRadioToTransmit();
    //Serial.println("Transmitting Tone for initial Sync");
    tone(PWM_PIN, 6000, radio_req_length_ms);
    delay(radio_req_length_ms);
    //Switch to Reciver Mode.
     setRadioToReceive();
}


void switch_isr() {

  long interrupt_time = millis();
  if ( interrupt_time - last_switch_isr_time < 200 ) {
    //Serial.println("switch_isr:: Ignoring Switch Press");
    return;
  }

  //Serial.print("switch_isr:: isInTransmission=");
  //Serial.println(isInTransmission);
  // We need to sync before anyone can transmit.
  if ( !isInTransmission ) {
    isInTransmission = true;
    justBeganTransmission = true;
  } else {
    isInTransmission = false;
  }

  //DEBUG
//  Serial.println("switch_isr:: DEBUG LINE");
//  Serial.println(isTransmitter);
//  Serial.println(isInTransmission);
//  Serial.println(justBeganTransmission);
//  Serial.println("switch_isr:: END DEBUG");

  last_switch_isr_time = interrupt_time;

} //switch_isr
void syncAndRecieve() {
  //We are now in a Transmission since we're syncing.
  isInTransmission = true;
  //Serial.println("transmitting ack");
  sendSyncACK(RADIO_ACK_LENGTH_MS, RADIO_ACK_SILENCE_MS); //Send the acknowledge to the transmitting radio.
  delay(latency);//make receiver wait roughly the time for the other radio. Based upon experiments. Could change with distance, power, etc.
  //Assuming we are going into the frequency hopping.
  isTransmitter = false;
  hopFreq();
  //We are done with transmision and we can have interrupts again
  isInTransmission = false;
}


bool syncRadio() {
  if (!isRadioInTxMode())
  {
    ////// TRANSMIT INITIAL TONE /////
    sendSyncREQ(RADIO_REQ_LENGTH_MS);
    ////// RECIEVE ACKNOWLEDGE FROM RECEIVER /////
    //Serial.println("Waiting For ACK Activity");
    // Waiting for Receive pulses
    return waitForSyncACK(RADIO_ACK_TIMEOUT_MS, RADIO_ACK_LENGTH_MS, RADIO_ACTIVE_RSSI, RADIO_EMPTY_RSSI);
    
  } else {
    //Serial.println("SyncRadio:: WARNING: Why are we currently transmitting and trying to sync???");
    return false;
  }

} //syncRadio


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
  for (int x = 0; x < 20; x++) {
    rssi = radio.readRSSI();  // "warm up" to get past RSSI hysteresis
  }
  //Serial.println( "waitForAciviity:: Going waiting for signal");
  long timer = millis() + timeout;                                              // Setup the timeout value
  if (timeout == 0) {
    timer = 4294967295;  // If we want to wait forever, set it to the max millis()
  }
  while (timer > millis()) {                                                    // while our timer is not timed out.
    rssi = radio.readRSSI();   // Read signal strength
    //Serial.println(rssi);
    if ( justBeganTransmission ) {
      //We are beginning a transmission and are the transmitter. Exit so we can begin sync routine.
      return false;
    }
    //Serial.println(rssi);
    if ( rssi > activityRSSI ) {
      timer = millis() + activitywindow;
      //Serial.println("waitForActivity:: ActivityDetected on the channel");
      while (timer > millis()) {
        rssi = radio.readRSSI();
        //if the channel goes dead
        if ( rssi <= emptyRSSI ) {
          //Serial.println("waitForActivity:: Channel went dead before the end of the activity window");
          return false; // Timeout, the channel went dead
        }
      }
      //Serial.println("waitForActivity:: End of activity window checking that channel is dead");
      int FUDGE_FACTOR_FOR_ENDING_COMMUNICATION = 100;
      long diff_time = millis() - timer;

      while ( diff_time < FUDGE_FACTOR_FOR_ENDING_COMMUNICATION ) {
        rssi = radio.readRSSI();
        //Serial.println(rssi);
        if ( rssi <= emptyRSSI ) {
          //Serial.println("waitForActivity:: Activity ended within predetermined window");
          //Serial.print("waitForActivity:: FudgedFudgeFactor =~ ");
          //Serial.println( diff_time );
          latency = diff_time;
          return true;
        }
        diff_time = millis() - timer;
      }
      //Serial.println(millis()-timer);
      //Serial.println("waitForActivity:: Activity did not end within the predetermined window");
      return false; // Timeout, we started activity but the activity is longer than our defined channel useage
    }
  }
  return false;                                                                  // Timed out
}

bool waitForSyncACK(int radio_ack_timeout_ms, int radio_ack_length_ms, int radio_active_rssi, int radio_empty_rssi) {
  if (waitForActivity(radio_ack_timeout_ms, radio_ack_length_ms, radio_active_rssi, radio_empty_rssi)) {
    //Waiting for Second Recieve From Raduio
    if (waitForActivity(radio_ack_timeout_ms, radio_ack_length_ms, radio_active_rssi, radio_empty_rssi)) {
      //Serial.println("RECIEVED ACKNOWLEDGE!!!");
      //We want to move into transmit for the duration of the frequency hopping.
      return true;
    } else {
      //Serial.println("failed second pulse");
      return false;
    }
  }
  else {
    //Serial.println("failed first pulse");
    return false;
  }
} //waitForSyncACK()

bool waitForSyncREQ(int radio_req_timeout_ms, int radio_req_length_ms, int radio_active_rssi, int radio_empty_rssi){
    if (waitForActivity(radio_req_timeout_ms, radio_req_length_ms, radio_active_rssi, radio_empty_rssi)){
        //Serial.println("waitForSyncREQ:: REQ Received Successfully!");
        return true;
    } else {
        //Serial.println("waitForSyncREQ:: REQ Failed to be Received");
        return false;
    }
}

/*
    Sets the radio into transmit mode, and sets the appropriate flags.
*/
void setRadioToTransmit(){
    radio.setModeTransmit();
    is_radio_in_tx_mode = true;
    //Serial.println("TX");
}
/*
    Sets the radio into receive mode, and set the appropriate flags.
*/
void setRadioToReceive(){
    radio.setModeReceive();
    is_radio_in_tx_mode = false;
    //Serial.println("Rx");
}
/*
    Returns whether we are currently in transmit mode or not.
    If true - we are in transmit mode.
    if false - we are in receiver mode.
*/
bool isRadioInTxMode(){
    return is_radio_in_tx_mode;
}
