#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_CCS811.h>
#include <SoftwareSerial.h>

#define CLOCK 2000 //Internal clock rate in seconds


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

void writeToBle(const char * data){
  Serial.println(data);
  bleSerial.write(data);
  delay(200);
  Serial.println(bleSerial.readString());
}

/**
 * Converts an integer to its HEX representation and packs it into an char
 * array of a given length. The unused space in the array is padded with '0'.
 *
 **/
void packIntoHexChar(char data[], int buf_size, int value){
  char HEX_C[17]="0123456789ABCDEF";
  //Fill with zero characters
  for (int i = 0; i<buf_size -1; i++){
    data[i] = '0';
  }
  //Convert value to hex
  char hb = highByte(value);
  char lb = lowByte(value);
  char int_chars[5] {HEX_C[hb>>4 & 0x0F], HEX_C[hb & 0x0F], HEX_C[lb>>4 & 0x0F], HEX_C[lb & 0x0F]};
  //set value
  for (int i = buf_size -2; i >= 0; i--){
    if (buf_size -i <=5 ){
      data[i] = int_chars[(i+1) - (buf_size - 4)];
    }
  }
}

/**
 * Takes an float value and converts it to a hex representation:
 * 16.16 => 000F000F (if the size is set to 5 (4))
 **/
void float2Hex(float value, double precision, char whole[], int whole_size, char remainder[], int remainder_size){
  int v_whole = value;
  int v_remainder = (value - v_whole) * (int) pow(10, precision);
  packIntoHexChar(whole, whole_size, v_whole);
  packIntoHexChar(remainder, remainder_size, v_remainder);
}

/**
 * Takes an char array with a maximum length of 32 and distributes its contents
 * over the UUID field of the BLE module.
 */
void writeToUUID(char data[], int data_size){
  //Determine how many registers we need
  int n_registers = data_size / 8;
  int tail = data_size % 8;
  if (n_registers > 4) {
    Serial.println("Too much data for the uuid field. 32 Bytes max!");
    return;
  }
  //Easy base case
  if (n_registers == 0 && tail > 0){
    char _data[9] = "00000000";
    for (int i = 0; i < 8; i++){
      _data[i] = data[i];
    }
    char _toSend[16];
    strcpy(_toSend, "AT+IBE0");
    strcat(_toSend, _data);
    writeToBle(_toSend);
    return;
  }
  int array_pointer = 0;
  int last_register = 0;
  //Process all except tail
  for (int i = 0; i< n_registers; i++){
    char _data[9] = "00000000";
    for (int j = array_pointer; j < 8 + array_pointer; j++){
      _data[j - array_pointer] = data[j];
    }
    array_pointer += 8;
    last_register = i;
    char _command[7];
    char _toSend[16];
    sprintf(_command, "AT+IBE%d", i);
    strcpy(_toSend, _command);
    strcat(_toSend, _data);
    writeToBle(_toSend);
  }
  // Process tail
  if (array_pointer < data_size ) {
    char _data[9] = "00000000";
    for (int i = 0; i < data_size - array_pointer ; i++){
      Serial.println(i);
      Serial.println(data[i+array_pointer]);
      _data[i] = data[i+array_pointer];
    }
    char _command[7];
    char _toSend[16];
    sprintf(_command, "AT+IBE%d", last_register + 1);
    strcpy(_toSend, _command);
    strcat(_toSend, _data);
    writeToBle(_toSend);
  }
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
  
  Serial.println("Module is ready, initialize sensors...");
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

}

void loop(void)
{
  // Get a new sensor event
  sensors_event_t event;
  bmp.getEvent(&event);

  float temp;
  float presure;
  int co2 = 0;
  int tvoc = 0;
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
  char buf_co2[5] = "0000";
  packIntoHexChar(buf_co2, 5, co2);

  Serial.println(temp);
  char buf_temp_whole[3] = "00";
  char buf_temp_remainder[3] = "00";
  float2Hex(temp, 2.0, buf_temp_whole, 3, buf_temp_remainder, 3);

  Serial.println(presure);
  char buf_pres_whole[5] = "0000";
  char buf_pres_remainder[3] = "00";
  float2Hex(presure, 2.0, buf_pres_whole, 5, buf_pres_remainder, 3);

  Serial.println(tvoc);
  char buf_tvoc[3] = "00";
  packIntoHexChar(buf_tvoc, 3, tvoc);

  //Prepare buffer (only 17 since 22 - 5)
  char buf_toSend[17];
  strcpy(buf_toSend, buf_co2);
  strcat(buf_toSend, buf_temp_whole);
  strcat(buf_toSend, buf_temp_remainder);
  strcat(buf_toSend, buf_pres_whole);
  strcat(buf_toSend, buf_temp_remainder);
  strcat(buf_toSend, buf_tvoc);

  //Set UUID (only 16 since we don't need the termination character)
  writeToUUID(buf_toSend, 16);

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
