#include <Wire.h>

const byte RADIO_ADDR = 1; 


void setup() {
  
  //Setup for debugging serial communication.
  Serial.begin(9600);
  //Join as Master.
  Wire.begin();
}


void loop() {
  // put your main code here, to run repeatedly:
//  setRegister(9,127,1,50);
//  readRegister(9, 10);
//  delay(1);

// Change the frequency in increments of one
   float freq = 409.0;
   for(int i=0; i < 100; i++){
      //changeFreq(freq); //change the frequency
      setRegister(RADIO_ADDR, 0x29, B00000000, B00110010);
      setRegister(RADIO_ADDR, 0x2a, B00000100, B10110000);
      delay(1000);//delay one second
      
      //freq = freq + 0.5;
   }
   
}

void changeFreq(float frequency){

  int temp_freq = frequency*1000*8;
  //shift our 32 bit to a 29 bit number;
  temp_freq = temp_freq>>3;  
  Serial.println(temp_freq);
  int high_freq = (int)(temp_freq >>16);
  byte high_freq_high = highByte(high_freq);
  byte high_freq_low = lowByte(high_freq);

  int low_freq = (temp_freq << 16) >> 16;
  byte low_freq_high = highByte(low_freq);
  byte low_freq_low = lowByte(low_freq);

  Serial.println( low_freq_high );
  Serial.println( low_freq_low );
  Serial.println( " ");

  setRegister( RADIO_ADDR, 0x29, high_freq_high, high_freq_low );
  setRegister( RADIO_ADDR, 0x2a, low_freq_high, low_freq_low);
  
}


//Set the register of a i2c device at a specific register_address with specific 8bit address
// The data is split into two bytes, register_data_high => 16:8, register_data_low => 7:0
void setRegister(byte i2c_address, byte register_address, byte register_data_high, byte register_data_low) {
  
  //Special procedure for addresses larger than 0x7F
  if(register_address > 0x7F){
    Wire.beginTransmission(i2c_address);
    //Write trhe address of register
    Wire.write(0x7F);
    //Write the 16bit value;
    Wire.write(0x00);
    Wire.write(0x01);
    Wire.endTransmission();

    //Write Data
    ///Offset the address by 0x80.
    byte new_addr = register_address-0x80;
    Wire.beginTransmission(i2c_address);
    Wire.write(new_addr);
    Wire.write(register_data_high);
    Wire.write(register_data_low);
    Wire.endTransmission();

    Wire.beginTransmission(i2c_address);
    //Write trhe address of register
    Wire.write(0x7F);
    //Write the 16bit value;
    Wire.write(0x00);
    Wire.write(0x00);
    Wire.endTransmission(false);
  }else{
    Wire.beginTransmission(i2c_address);
    Wire.write(register_address);
    Wire.write(register_data_high);
    Wire.write(register_data_low);
    Wire.endTransmission();
  }
} //setRegister(....

//Read the data from the specified register and return it as pointer to an array
byte* readRegister(byte i2c_address, byte register_address){

  byte* ret_ptr = malloc(sizeof(byte)*2);
  Wire.beginTransmission(i2c_address);
  Wire.write(register_address);
  Wire.endTransmission(false);
  Wire.requestFrom(i2c_address, 2);
  
  if(Wire.available() == 2){
    ret_ptr[1] = Wire.read();
  } else if (Wire.available() == 1){
    ret_ptr[0] = Wire.read();
  } else {
    Serial.println("WARNING::readRegister: Unexpected number of bytes available.");
  }

  Serial.print("ret_ptr[1]: ");
  Serial.println(ret_ptr[1], BIN);
  Serial.print("ret_ptr[0]: ");
  Serial.println(ret_ptr[1], BIN); 

  return ret_ptr;  
  
} //readRegister

