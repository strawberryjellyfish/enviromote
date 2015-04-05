// Enviromote generic sensor node
// The will send short sensor data transmissions to the gateway
// then sleep to preserve power until the next transmission.
// After transmitting it will expect and ACK from the gateway,
// if the ACK contains a command string the none will execute the command
// before sleeping again.

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <stdlib.h>   // maths
#include <LowPower.h> // https://github.com/rocketscream/Low-Power
#include <DHT.h>      // DHT sensor library

// Node setup
#define GATEWAYID     1
#define NODEID        2    // unique for each node on same network
#define NETWORKID     23  // the same on all nodes that talk to each other

// LIght Sensor
#define LIGHTREADPIN A2 // LDR analogue input pin
#define LIGHTENABLEPIN 6 // output pin to turn on LDR

// Soil Moisture Sensor
#define MOISTPIN1 1 // soil probe pin 1 with 56kohm resistor
#define MOISTPIN2 7 // soil probe pin 2 with 100ohm resistor
#define MOISTREADPIN A0 // analog read pin. connected to A2 PIN with 56kohm resistor
int SoilDryThreshold = 250; // Low Soil Moisture warning
int SoilWetThreshold = 850; // High Soil Moisture warning

// DHT Humidity + Temperature sensor
#define DHTREADPIN 5 // Data pin (D5) for DHT
#define DHTENABLEPIN 4 // Sensor enable pin
#define DHTTYPE DHT11
DHT dht(DHTREADPIN, DHTTYPE);
int TemperatureLowThreshold = 5; // Low Temperature warning
int TemperatureHighThreshold = 35; // High Temperature warning
int HumidityLowThreshold = 20; // High Humidity warning
int HumidityHighThreshold = 80; // Low humidity warning

// Battery Level
#define VOLTAGEREADPIN A7 // analogue voltage read pin for battery meter
#define VOLTAGEENABLEPIN A3 // current sink pin. ( enable voltage divider )
#define VOLTAGEREF 3.3 // reference voltage on system. use to calculate voltage from ADC
#define VOLTAGEDIVIDER 2 // if you have a voltage divider to read voltages, enter the multiplier here.
int VoltageLowThreshold = 4; // low battery threshold. 4 volts.
int VoltageADC;

// Radio Settings
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "1234567890123456" // exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    // uncomment only for RFM69HW

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

#define SERIAL_BAUD   115200

// Define sensor id types
#define SOILMOISTURE    1
#define TEMPERATURE     2
#define HUMIDITY        3
#define AMBIENTLIGHT    4

#define VOLTAGE         100
#define RADIOTEMP       101



// Misc default values
String SensorData; // sensor data STRING
String ErrorLvl = "0"; // Error level. 0 = normal. 1 = soil moisture, 2 = Temperature , 3 = Humidity, 4 = Battery voltage

int SleepCycle = 1; // How many lowpower library 8 second sleeps *450 = 1 hour

RFM69 radio;


// Initialize all the things
void setup() {

  Serial.begin(SERIAL_BAUD);
 
  //LED setup. 
  pinMode(LED, OUTPUT);
  
  // Battery Meter setup
  pinMode(VOLTAGEREADPIN, INPUT);
  pinMode(VOLTAGEENABLEPIN, INPUT);
 
  // Moisture sensor pin setup
  pinMode(MOISTPIN1, OUTPUT);
  pinMode(MOISTPIN2, OUTPUT);
  pinMode(MOISTREADPIN, INPUT);
  
  // Humidity sensor setup
  pinMode(DHTENABLEPIN, OUTPUT);
  dht.begin();

  // power on indicator
  LEDBlink(80);
  LEDBlink(80);
   
  // Initialize the radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  radio.encrypt(ENCRYPTKEY);

  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  
  radio.sleep();
  
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY == RF69_433MHZ ? 433 : FREQUENCY == RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
}

// The main loop simply does this:
//  * turn on sensors and take readings
//  * transmit a data string
//  * act on any received command from gateway
//  * sleep and repeat
void loop() {
  
  LEDPulse();
  ErrorLvl = "0"; // Reset error level
  
  // Don't really need to sample battery voltage as regularly as other sensors, 
  // but we'll do it anyway so we are transmitting a consistent data set
  float BatteryVoltage = GetBatteryLevel();
  char VoltagebufTemp[10];
  dtostrf(BatteryVoltage,5,3,VoltagebufTemp); // convert float Voltage to string

  // Might as well sample the radio temperature sensor for diagnostic purposes
  byte radioTemperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
  
  // Soil Moisture sensor reading
  int moistREADavg = 0; // reset the moisture level before reading
  int moistCycle = 3; // how many times to read the moisture level. default is 3 times
  for ( int moistReadCount = 0; moistReadCount < moistCycle; moistReadCount++ ) {
    moistREADavg += GetMoistureLevel();
  }
  moistREADavg = moistREADavg / moistCycle; // average the results
  
  // if soil is below threshold, error level 1
  if ( moistREADavg < SoilDryThreshold ) {
    ErrorLvl += "1"; // assign error level
      LEDBlink(128);
      LEDBlink(128);
      LEDBlink(128);
  }
    
    
  // Humidity + Temperature sensor reading
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  digitalWrite(DHTENABLEPIN, HIGH); // turn on sensor
  delay (38); // wait for sensor to stabalize
  int dhttempc = dht.readTemperature(); // read temperature as celsius
  int dhthumid = dht.readHumidity(); // read humidity
  //Serial.println(dhttempc);
  
  // check if returns are valid, if they are NaN (not a number) then something went wrong!
  if (isnan(dhttempc) || isnan(dhthumid) || dhttempc == 0 || dhthumid == 0 ) {
    dhttempc = 0;
    dhthumid = 0;
    ErrorLvl += "23";
  }
  delay (18);
  digitalWrite(DHTENABLEPIN, LOW); // turn off sensor
 
    
  // Coarse Light Level
  digitalWrite(LIGHTENABLEPIN, HIGH); // turn on sensor
  delay (38); // wait for sensor to stabalize
  int lightlevel = 1023 - analogRead(LIGHTREADPIN);
  delay (18);
  digitalWrite(LIGHTENABLEPIN, LOW); // turn off sensor


  // PREPARE READINGS FOR TRANSMISSION
  SensorData = String(NODEID);
  SensorData += ":";
  SensorData += ErrorLvl;
  SensorData += ":";
  SensorData += SOILMOISTURE;
  SensorData += ":";
  SensorData += String(moistREADavg);
  SensorData += ":";
  SensorData += TEMPERATURE;
  SensorData += ":";
  SensorData += String(dhttempc);
  SensorData += ":";
  SensorData += HUMIDITY;
  SensorData += ":";
  SensorData += String(dhthumid);
  SensorData += ":";
  SensorData += AMBIENTLIGHT;
  SensorData += ":";
  SensorData += String(lightlevel);
  SensorData += ":";
  SensorData += VOLTAGE;
  SensorData += ":";
  SensorData += VoltagebufTemp;
  SensorData += ":";
  SensorData += RADIOTEMP;
  SensorData += ":";
  SensorData += String(radioTemperature);
  byte sendSize = SensorData.length();
  sendSize = sendSize + 1;
  char payload[sendSize];
  SensorData.toCharArray(payload, sendSize); // convert string to char array for transmission
  
  Serial.print("Sending [");
  Serial.print(sendSize);
  Serial.print("]: ");

  for(byte i = 0; i < sendSize; i++)
    Serial.print((char)payload[i]);

  // Transmit data
  
  if (radio.sendWithRetry(GATEWAYID, payload, sendSize)) 
  {
    // ACK received, check response message
    String ackString;
    for (byte i = 0; i < radio.DATALEN; i++) 
    {
      ackString += (char)radio.DATA[i];
    }
    Serial.println();
    Serial.print("Gateway Responded: ");
    Serial.print(ackString);
    
    if (ackString == "CMD") {
      Serial.println("Received ACK CMD");
      // Gateway sent ACK with command.
      // TODO: parse ackString and act as requested
    }
  }
  else 
  {
    // Last transmission has not beeen acknowledged
    Serial.print(" No acknowledgment from gateway");
  }
  sendSize = (sendSize + 1) % 31;
  Serial.println();

  // Error Level handing
  // If any error level is generated, halve the sleep cycle
  if ( ErrorLvl.toInt() > 0 ) {
    SleepCycle = SleepCycle / 2;
    LEDBlink(30);
    LEDBlink(30);
    LEDBlink(30);
  }

  Sleep();
}

void Sleep()
// Power Saving, go to sleep between data burst
{
  int currentSleep = SleepCycle + random(8); // Randomize sleep cycle a little to reduce collisions with other nodes
  Serial.print("Sleeping for ");
  Serial.print(currentSleep * 8);
  Serial.println(" seconds");
  delay(5);
  radio.sleep(); // turn off radio  
  for ( int sleepTime = 0; sleepTime < currentSleep; sleepTime++ ) 
  {
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF); // sleep duration is 8 seconds multiply by the sleep cycle variable.
  }
}

void LEDBlink(int DELAY_MS)
// turn LED on and off for DELAY_MS (LED blink)
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(DELAY_MS);
  digitalWrite(LED, LOW);
  delay(DELAY_MS);
}

// LED Pulse fade in and out
void LEDPulse() {
  int i;
  delay (88);
  for (int i = 0; i < 128; i++) {
    analogWrite(LED, i);
    delay(12);
  }

  for (int i = 128; i > 0; i--) {
    analogWrite(LED, i);
    delay(12);       
  }
  digitalWrite(LED, LOW);
  delay (128);
}

// Moisture sensor reading function
// function reads 3 times and averages the data
int GetMoistureLevel() {
  int moistREADdelay = 88; // delay to reduce capacitive effects
  int moistAVG = 0;
  // polarity 1 read
  digitalWrite(MOISTPIN1, HIGH);
  digitalWrite(MOISTPIN2, LOW);
  delay (moistREADdelay);
  int moistVal1 = analogRead(MOISTREADPIN);
  //Serial.println(moistVal1);
  digitalWrite(MOISTPIN1, LOW);
  delay (moistREADdelay);
  // polarity 2 read
  digitalWrite(MOISTPIN1, LOW);
  digitalWrite(MOISTPIN2, HIGH);
  delay (moistREADdelay);
  int moistVal2 = analogRead(MOISTREADPIN);
  //Make sure all the pins are off to save power
  digitalWrite(MOISTPIN2, LOW);
  digitalWrite(MOISTPIN1, LOW);
  moistVal1 = 1023 - moistVal1; // invert the reading
  moistAVG = (moistVal1 + moistVal2) / 2; // average readings. report the levels
  return moistAVG;
}

// Battery level check, take 3 readings and use the last to allow circuit to stabilize after waking
float GetBatteryLevel() {
  pinMode(VOLTAGEENABLEPIN, OUTPUT); // change pin mode
  digitalWrite(VOLTAGEENABLEPIN, LOW); // turn on the battery meter (sink current)
  for ( int i = 0 ; i < 3 ; i++ ) {
    delay(50); // delay, wait for circuit to stabilize
    VoltageADC = analogRead(VOLTAGEREADPIN); // read the voltage 3 times. keep last reading
  }
  float Voltage = ((VoltageADC * VOLTAGEREF) / 1023) * VOLTAGEDIVIDER; // calculate the voltage
  if (Voltage < VoltageLowThreshold){
    ErrorLvl = "4";
  }
  pinMode(VOLTAGEENABLEPIN, INPUT); // turn off the battery meter
  return Voltage;

}
