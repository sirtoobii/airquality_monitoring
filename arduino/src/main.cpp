#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_CCS811.h>
#include <SoftwareSerial.h>

#define CLOCK 400 //Internal clock rate in seconds

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

void writeToBle2(const String & data){
  char buf[data.length()+1];
  data.toCharArray(buf, data.length()+1);
  Serial.println(String(buf));
  bleSerial.write(buf);
  delay(200);
  Serial.println(bleSerial.readString());
}

void writeToBle(const char * data){
  Serial.println(data);
  bleSerial.write(data);
}

String float2Hex(float value){
  int whole = value;
  int remainder = (value - whole) * 100;
  String str_whole = String(whole, HEX);
  String str_remainder = String(remainder, HEX);
  return String(str_whole+str_remainder);
}

String packIntoNBytes(const String & data, int nBytes){
  if (data.length() < nBytes){
    char _pre[nBytes - data.length()];
    for (int i = 0; i < sizeof(_pre); i++){
      _pre[i] = '0';
    }
    return String(_pre + data);
  } else if (data.length() == nBytes){
    return data;
  } else {
    Serial.println("String is too long");
    globalErrorState = 0x5;
    indicateError(false);
  }

}

void writeToUUID(const String & data){
  data.toUpperCase();
  //Determine how many registers we need
  int n_registers = data.length() / 8;
  int tail = data.length() % 8;
  Serial.println(n_registers);
  Serial.println(tail);
  if (n_registers > 4) {
    Serial.println("Too much data for the uuid field. 32 Bytes max!");
    return;
  }
  //Easy base case
  if (n_registers == 0 && tail > 0){
    String _data = "00000000";
    for (int i = 0; i < 8; i++){
      _data[i] = data[i];
    }
    String _toSend = "AT+IBE0" + _data;
    writeToBle2(_toSend);
    return;
  }
  unsigned int array_pointer = 0;
  int last_register = 0;
  //Process all exept tail
  for (int i = 0; i< n_registers; i++){
    String _data = "";
    for (unsigned int j = array_pointer; j < 8 + array_pointer; j++){
      _data += data[j];
    }
    array_pointer += 8;
    last_register = i;
    writeToBle2("AT+IBE" + String(i) + _data);
    delay(100);
  }
  // Process tail
  if (array_pointer < data.length() ) {
    Serial.println("Tail");
    String _data = "00000000";
    for (unsigned int i = 0; i < data.length() - array_pointer ; i++){
      _data[i] = data[i+array_pointer];
    }
    String _toSend = "AT+IBE" + String(last_register + 1) + _data;
    writeToBle2(_toSend);
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
  // writeToBle2("AT"); // Wake up
  // writeToBle2("AT+NAMEsensortag1"); //Set name
  // writeToBle2("AT+ADVIA"); // Set Advertising rate to  2000ms
  // writeToBle2("AT+ADTY3"); // Only advertise, no connect
  // writeToBle2("AT+IBEA1"); // Enable iBeacon mode
  // writeToBle2("AT+PWRM1"); // Disable sleep mode since I found no (working) way to enter operation mode again
  // writeToBle("AT+RESET"); // Restart
  // delay(1000);
  writeToBle2("AT");

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
  String hex_co2 = packIntoNBytes(String(co2, HEX), 4);
  String hex_temp = packIntoNBytes(float2Hex(temp), 4);
  String hex_presure = packIntoNBytes(float2Hex(presure), 5);
  String hex_tvoc = packIntoNBytes(String(tvoc), 2);
  Serial.println(hex_co2 + hex_temp + hex_presure + hex_tvoc);

  writeToUUID(hex_co2 + hex_temp + hex_presure + hex_tvoc);
  //Indicate that data has been sent
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
