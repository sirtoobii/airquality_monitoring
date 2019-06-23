#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_CCS811.h>
#include <SoftwareSerial.h>

#define CLOCK 60000 //Internal clock rate in seconds

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
  Serial.println("Converted: " + String(buf) + "  Orig:" + data);
  bleSerial.write(*buf);
  delay(100);
  Serial.println(bleSerial.readString());
}

void writeToBle(const char * data){
  Serial.println(data);
  bleSerial.write(data);
}

void writeToUUID(const String & data){
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
    String _data = "AT+IBE0" + String(data);
    writeToBle2(_data);
    return;
  }
  unsigned int array_pointer = 0;
  int last_register = 0;
  //Process all exept tail
  for (int i = 0; i< n_registers; i++){
    String _data = "";
    for (int j = array_pointer; j < 8 + array_pointer; j++){
      _data += data[j];
    }
    array_pointer += 8;
    last_register = i;
    writeToBle2("AT+IBE" + String(i) + _data);
  }
  // Process tail
  if (array_pointer < data.length() ) {
    String _data = "";
    for (unsigned int i = array_pointer; i < data.length() ; i++){
      _data += data[i];
    }
    _data = "AT+IBE" + String(last_register + 1) + _data;
    writeToBle2(_data);
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
  //writeToBle("AT"); // Wake up
  bleSerial.write("AT");
  Serial.println(bleSerial.readString());
  writeToBle("AT+NAMEble1"); //Set name
  delay(300);
  Serial.println(bleSerial.readString());
  writeToBle("AT+ADVIA"); // Set Advertising rate to  2000ms
  delay(300);
  writeToBle("AT+ADTY3"); // Only advertise, no connect
  delay(300);
  writeToBle("AT+IBEA1"); // Enable iBeacon mode
  delay(300);
  Serial.println(bleSerial.readString());
  writeToBle("AT+PWRM1"); // Disable sleep mode since I found no (working) way to enter operation mode again
  delay(300);

  Serial.println(bleSerial.readString());
  writeToBle("AT+RESET"); // Restart
  delay(1000);
  Serial.println(bleSerial.readString());
  delay(1500);
  writeToBle("AT");
  delay(300);
  Serial.println(bleSerial.readString());

  writeToUUID("2222222222266bbcc");

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
  char data[20];
  // convert float to char-arrays
  char temp_s[7]; // Buffer big enough for 7-character float
  char pres_s[7];
  dtostrf(temp, 4, 1, temp_s);
  dtostrf(presure, 4, 0, pres_s);
  snprintf(data, 20, "t%sp%sc%dh%d", temp_s, pres_s, co2, tvoc);
  writeToBle2(String(data));

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
