#include <Wire.h>

//Set the address of the RDA chip
const byte RADIO_ADDR = 0b0101110;//One option is 46. The other is 113 

const int CONT2_PIN = 5;
const int CONT1_PIN = 6;
const int MODE_PIN = 7;
const int SEN_PIN = 8;


void setup() {
  
  //Setup for debugging serial communication.
  Serial.begin(9600);

  //set control pins to outputs
  pinMode(CONT1_PIN,OUTPUT);
  pinMode(CONT2_PIN,OUTPUT);

  //set MODE,SEN pins to output
  pinMode(MODE_PIN,OUTPUT);
  
  //Join as Master.
  Wire.begin();
}


void loop() {
//   put your main code here, to run repeatedly:
//  setRegister(9,127,1,50);
//  readRegister(9, 10);
//  delay(1);


  //int addr = pollI2cAddr();
  Serial.print("Address is: ");
  Serial.println(addr);
  delay(100*1000);


// Change the frequency in increments of one
   float freq = 409.75;
//   for(int i=0; i < 100; i++){
      //changeFreq(freq); //change the frequency
      setRegister(RADIO_ADDR, 0x29, B00000000, B00110010);
      delay(100);
      setRegister(RADIO_ADDR, 0x2a, B00000100, B10110000);
      delay(100);//delay one second
//      setRegister(RADIO_ADDR, 0x29, B00000001, B00110010);
//      setRegister(RADIO_ADDR, 0x2a, B00000100, B10110000); 
//      delay(1000);     
//      freq = freq + 0.5;
//   }
  
}

int* longToBitArr(long l){
   int* arr = malloc(sizeof(l)*8*sizeof(int));
   for(int i=0; i<(sizeof(l)*8)-1;i++){
      arr[i]=bitRead(l,i);
   }
   Serial.println(sizeof(arr));
   return arr;
}

long bitArrToLong(int* arr){
  
  long ret=0;
  //Serial.println(sizeof(arr)*8);
  for(int i =0; i<sizeof(arr); i++){
    bitWrite(ret, i, arr[i]);
  }
  return ret;
} //bitArrToLong

void printArr( int* arr){
  
  for(int i=0; i<sizeof(arr);i++){
    Serial.print(arr[i]);
    //Dont print comma on last line
    if ( i<sizeof(arr)-1 ){
      Serial.print(", ");
    }
  }
  Serial.print("\n");
} //printArr


void changeFreq(float frequency){

  long temp_freq = (frequency*8000);
  //shift our 32 bit to a 29 bit number;
  Serial.println(temp_freq,BIN);
  int* arr = longToBitArr(temp_freq);
  printArr(arr);
  Serial.println(bitArrToLong(arr));
  Serial.println(sizeof(temp_freq)*8);
  int high_freq = (temp_freq >>16);
  byte high_freq_high = highByte(high_freq);
  byte high_freq_low = lowByte(high_freq);

  int low_freq = (temp_freq << 16) >> 16;
  byte low_freq_high = highByte(low_freq);
  byte low_freq_low = lowByte(low_freq);

  Serial.println( low_freq_high );
  Serial.println( low_freq_low );
  Serial.println(" ");

  setRegister( RADIO_ADDR, 0x29, high_freq_high, high_freq_low );
  setRegister( RADIO_ADDR, 0x2a, low_freq_high, low_freq_low );
  
} //changeFrequency


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

int pollI2cAddr(){
  int retAddr = -1;//First loop will add this to be 0
  int retCond = -1;
  
  while(retCond !=0){
    retAddr=retAddr+1;
    Wire.beginTransmission(retAddr);
    delay(100);
    retCond = Wire.endTransmission();
    Serial.print("Return Condition: ");
    Serial.println(retCond);
    delay(100);
    
  }
  return retAddr;

}

void setControl(boolean normalOperation){
  if(normalOperation){
    digitalWrite(CONT1_PIN,HIGH);
    digitalWrite(CONT2_PIN,LOW);
  }else{
    digitalWrite(CONT1_PIN,LOW);
    digitalWrite(CONT2_PIN,HIGH):
  }
}

void setMode(boolean modeLow){
  if(modeLow){
    digitalWrite(MODE_PIN,LOW);
  }else{
    digitalWrite(MODE_PIN,HIGH);
  }
}

void setSen(boolean senLow){
  if(senLow){
    pinMode(SEN_PIN,OUTPUT);
    digitalWrite(SEN_PIN,LOW);
  }else{
    pinMode(SEN_PIN,INPUT);
  }
}

