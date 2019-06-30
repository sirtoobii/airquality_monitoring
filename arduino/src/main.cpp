#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_CCS811.h>
#include <SoftwareSerial.h>

#define CLOCK 20000 //Internal clock rate in seconds


SoftwareSerial bleSerial(2, 3);
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);
Adafruit_CCS811 ccs;



/*
 * Possibile Error states
 * 0x0: No error
 * 0x1: Error at init BMP085
 * 0x2: Error at init CSS811
 * 0x3: Error reading sensor BMP085
 * 0x4: Error reading sensor CSS811
 * 0x5: Error converting the data
 */
char globalErrorState = 0x0;
char HEX_C[17]="0123456789ABCDEF";
int ctr = 0;

void indicateError(bool blocking=false){
  if (globalErrorState != 0x0) {
    while(true){
    for (int i=0; i<globalErrorState; i++){
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
      if (!blocking) {
        break;
      }
      delay(1000);
    }
    }
}
/**
 * Takes a char array a fills it with a given character
 **/
void fillWithChar(char data[], char character, int data_size){
  for (int i = 0; i < data_size; i++){
    data[i] = character;
  }
}

/**
 * Just writes an input char array to the BLE module. Also prints the response
 * to UART.
 **/
void writeToBle(const char * data){
  Serial.println(data);
  bleSerial.write(data);
  Serial.println(bleSerial.readString());
}

/**
 * Converts an integer to its HEX representation and packs it into an char
 * array of a given length. The unused space in the array is padded with '0'.
 *
 **/
void packIntoHexChar(char data[], int buf_size, int value){
  static char hb = 0x0;
  static char lb = 0x0;
  static char int_chars[5] = "0000";
  //Fill with zero characters
  for (int i = 0; i<buf_size -1; i++){
    data[i] = '0';
  }
  //Convert value to hex
  hb = highByte(value);
  lb = lowByte(value);
  int_chars[0] = HEX_C[hb>>4 & 0x0F];
  int_chars[1] = HEX_C[hb & 0x0F];
  int_chars[2]=  HEX_C[lb>>4 & 0x0F];
  int_chars[3] = HEX_C[lb & 0x0F];
  //set value
  for (int i = buf_size -2; i >= 0; i--){
    if (buf_size -i <=5 ){
      data[i] = int_chars[(i+1) - (buf_size - 4)];
    }
  }
}
/**
 * Implemention of the "Fletcher's Checksum". To fit the result into two
 * HEX character we compute the modulo 16 of each value.
 **/
int sum1 = 0;
int sum2 = 0;
void computeChecksum(char input[], int input_size, char output[]){
  sum1 = 0;
  sum2 = 0;
  for (int i = 0; i < input_size; i++) {
    sum1 = (sum1 + input[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }

output[0] = HEX_C[lowByte(sum1 % 16) & 0x0F];
output[1] = HEX_C[lowByte(sum2 % 16) & 0x0F];
}

/**
 * Takes an float value and converts it to a hex representation:
 * 16.16 => 000F000F (if the size is set to 5 (4))
 **/
void float2Hex(float value, double precision, char whole[], int whole_size, char remainder[], int remainder_size){
  static int v_whole = 0;
  static int v_remainder = 0;
  v_whole = value;
  v_remainder = (value - v_whole) * (int) pow(10, precision);
  packIntoHexChar(whole, whole_size, v_whole);
  packIntoHexChar(remainder, remainder_size, v_remainder);
}

/**
 * Takes an char array with a maximum length of 32 and distributes its contents
 * over the UUID field of the BLE module.
 */



void writeToUUID(char data[], int data_size){
  static char _data[9] = "00000000";
  static char _toSend[17];
  static char _command[8];
  static char _checksum[3] = "00";
  static int array_pointer = 0;
  static int n_registers = 0;
  static int tail = 0;
  //Determine how many registers we need
  n_registers = data_size / 8;
  tail = data_size % 8;
  //Set checksum
  computeChecksum(data, data_size, _checksum);
  if (data_size > 30) {
    Serial.println("Too much data for the uuid field. 30 Bytes max! (last two are the checksum)");
    return;
  }
  //Easy base case
  if (n_registers == 0 && tail > 0){
    fillWithChar(_data, '0', 8);
    for (int i = 0; i < 8; i++){
      _data[i] = data[i];
    }
    strcpy(_toSend, "AT+IBE0");
    strcat(_toSend, _data);
    writeToBle(_toSend);
    return;
  }
  array_pointer = 0;
  //Process all except tail
  for (int i = 0; i< n_registers; i++){
    fillWithChar(_data, '0', 8);
    for (int j = array_pointer; j < 8 + array_pointer; j++){
      _data[j - array_pointer] = data[j];
    }
    array_pointer += 8;
    //Only send if this is not the last register
    if (i < 3){
      sprintf(_command, "AT+IBE%d", i);
      strcpy(_toSend, _command);
      strcat(_toSend, _data);
      writeToBle(_toSend);
    }
  }
  //Process tail
  if (array_pointer < data_size ) {
    fillWithChar(_data, '0', 8);
    for (int i = 0; i < data_size - array_pointer ; i++){
      _data[i] = data[i+array_pointer];
    }
    //Only send if this is not the last register
    if (n_registers < 3){
      sprintf(_command, "AT+IBE%d", n_registers);
      strcpy(_toSend, _command);
      strcat(_toSend, _data);
      writeToBle(_toSend);
    }
  }
  if (n_registers < 3){
    fillWithChar(_data, '0', 8);
  }
  //Write last register with checksum
  sprintf(_command, "AT+IBE%d", 3);
  _data[6] = _checksum[0];
  _data[7] = _checksum[1];
  strcpy(_toSend, _command);
  strcat(_toSend, _data);
  writeToBle(_toSend);
}

void setup(void)
{
  // Setupo led
  pinMode(LED_BUILTIN, OUTPUT);

  //Setup main serial
  Serial.begin(9600);
  Serial.println("Starting module");

  //Setup ble serial
  bleSerial.begin(9600); // Sometimes the default baud rate is also 115200
  writeToBle("AT"); // Wake up
  writeToBle("AT+NAMEsensortag1"); //Set name
  writeToBle("AT+ADVIA"); // Set Advertising rate to  2000ms
  writeToBle("AT+ADTY3"); // Only advertise, no connect
  writeToBle("AT+IBEA1"); // Enable iBeacon mode
  //Reset id (we somehow can't set them to 0x0)
  writeToBle("AT+IBE000000001");
  writeToBle("AT+IBE100000001");
  writeToBle("AT+IBE200000001");
  writeToBle("AT+IBE300000001");
  writeToBle("AT+PWRM1"); // Disable sleep mode since I found no (working) way to enter operation mode again
  writeToBle("AT+RESET"); // Restart
  delay(1000);
  writeToBle("AT");

  Serial.println("Module is ready, initializing sensors...");
  //Indicate BLE ready
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);

  // Setup BMP085
  if(!bmp.begin())
  {
    Serial.print("Ooops, no BMP085 detected");
    globalErrorState = 0x1;
    indicateError(true);
  }
  //Setup CCS811
  if (!ccs.begin()) {
    Serial.println("Failed to start CSS811 Sensor");
    globalErrorState = 0x2;
    indicateError(true);
  }

  //calibrate temperature sensor
  while(!ccs.available());
  float temp = ccs.calculateTemperature();
  ccs.setTempOffset(temp - 25.0);

  Serial.println("Sensors initialized successfully");
}

void loop(void){
  static float temp = 0.0;
  static float presure = 0.0;
  static int co2 = 0;
  static int tvoc = 0;

  //Send buffers
  static char buf_co2[5] = "0000";
  static char buf_temp_whole[3] = "00";
  static char buf_temp_remainder[3] = "00";
  static char buf_pres_whole[5] = "0000";
  static char buf_pres_remainder[3] = "00";
  static char buf_tvoc[4] = "000";

  //Prepare whole buffer (only 18 since 23 - 5)
  static char buf_toSend[18];
  // Get a new sensor event
  sensors_event_t event;
  bmp.getEvent(&event);

  if (event.pressure)
  {
    presure = event.pressure;
    bmp.getTemperature(&temp);
    globalErrorState = 0x0;
  }
  else
  {
    globalErrorState = 0x3;
  }

  delay(100);

  if(ccs.available()){
    if(!ccs.readData()){
      co2 = ccs.geteCO2();
      tvoc = ccs.getTVOC();
      globalErrorState = 0x0;
    }
    else{
      globalErrorState = 0x4;
    }
  }

  indicateError();
  Serial.println(co2);
  fillWithChar(buf_co2, '0', 4);
  packIntoHexChar(buf_co2, 5, co2);

  Serial.println(temp);
  fillWithChar(buf_temp_whole, '0', 2);
  fillWithChar(buf_temp_remainder, '0', 2);
  float2Hex(temp, 2.0, buf_temp_whole, 3, buf_temp_remainder, 3);

  Serial.println(presure);
  fillWithChar(buf_pres_whole, '0', 4);
  fillWithChar(buf_pres_remainder, '0', 2);
  float2Hex(presure, 2.0, buf_pres_whole, 5, buf_pres_remainder, 3);

  Serial.println(tvoc);
  fillWithChar(buf_tvoc, '0', 3);
  packIntoHexChar(buf_tvoc, 4, tvoc);

  strcpy(buf_toSend, buf_co2);
  strcat(buf_toSend, buf_temp_whole);
  strcat(buf_toSend, buf_temp_remainder);
  strcat(buf_toSend, buf_pres_whole);
  strcat(buf_toSend, buf_pres_remainder);
  strcat(buf_toSend, buf_tvoc);

  //Set UUID (only 17 since we don't need the termination character)
  writeToUUID(buf_toSend, 17);

  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  delay(CLOCK);
}
