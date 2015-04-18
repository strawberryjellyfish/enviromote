/*  Enviromote generic sensor node
    The will send short sensor data transmissions to the gateway
    then sleep to preserve power until the next transmission.
    After transmitting it will expect and ACK from the gateway,
    if the ACK contains a command string the none will execute the command
    before sleeping again.
*/

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <stdlib.h>   // maths
#include <LowPower.h> // https://github.com/rocketscream/Low-Power
#include <DHT.h>      // https://github.com/adafruit/DHT-sensor-library
#include <string.h>

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
int soilDryThreshold = 250; // Low Soil Moisture warning
int soilWetThreshold = 850; // High Soil Moisture warning

// DHT Humidity + Temperature sensor
#define DHTREADPIN 5 // Data pin (D5) for DHT
#define DHTENABLEPIN 4 // Sensor enable pin
#define DHTTYPE DHT11
DHT dht(DHTREADPIN, DHTTYPE);
int temperatureLowThreshold = 5; // Low Temperature warning
int temperatureHighThreshold = 35; // High Temperature warning
int humidityLowThreshold = 20; // High Humidity warning
int humidityHighThreshold = 80; // Low humidity warning

// Battery Level
#define VOLTAGEREADPIN A7 // analogue voltage read pin for battery meter
#define VOLTAGEENABLEPIN A3 // current sink pin. ( enable voltage divider )
#define VOLTAGEREF 3.3 // reference voltage on system. use to calculate voltage from ADC
#define VOLTAGEDIVIDER 2 // if you have a voltage divider to read voltages, enter the multiplier here.
float voltageLowThreshold = 4; // low battery threshold. 4 volts.
int voltageRead;
float voltage;

// Radio Settings
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "1234567890123456" // the same 16 characters on all nodes!
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

// Error Codes
#define LOW_BATTERY       1
#define HIGH_RADIO_TEMP   2
#define LOW_MOISTURE      4
#define HIGH_MOISTURE     8
#define LOW_TEMPERATURE   16
#define HIGH_TEMPERATURE  32
#define LOW_HUMIDITY      64
#define HIGH_HUMIDITY     128

// Only 6 error flags/bits used, so a char is fine
unsigned char errorFlags;

// Misc default values
String sensorData; // sensor data STRING
int sleepCycle = 1; // How many lowpower library 8 second sleeps *450 = 1 hour
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
  ledBlink(50);
  ledBlink(50);

  // Initialize the radio
  radio.initialize(FREQUENCY, NODEID, NETWORKID);
  radio.encrypt(ENCRYPTKEY);

  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif

  radio.sleep(); // ensure radio is off until we need it

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

  ledPulse();
  errorFlags = null; // Reset error level
  
  // Don't really need to sample battery voltage as regularly as other sensors, 
  // but we'll do it anyway so we are transmitting a consistent data set
  getBatteryLevel(voltage);

  if (voltage < voltageLowThreshold)
    errorFlags |= LOW_BATTERY;

  char batteryVoltage[10];
  dtostrf(voltage, 5, 3, batteryVoltage); // convert float Voltage to string

  // Might as well sample the radio temperature sensor for diagnostic purposes
  byte radioTemperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
  
  // Soil Moisture sensor reading
  int moistReadAvg = 0; // reset the moisture level before reading
  int moistCycle = 3; // how many times to read the moisture level. default is 3 times
  for ( int moistReadCount = 0; moistReadCount < moistCycle; moistReadCount++ ) {
    moistReadAvg += getMoistureLevel();
  }
  moistReadAvg = moistReadAvg / moistCycle; // average the results
  
  if ( moistReadAvg < soilDryThreshold ) 
  {
    errorFlags |= LOW_MOISTURE;
  } 
  else if ( moistReadAvg > soilWetThreshold ) 
  {
    errorFlags |= HIGH_MOISTURE;
  }


  // Humidity + Temperature sensor reading
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  digitalWrite(DHTENABLEPIN, HIGH); // turn on sensor
  delay (100); // wait for sensor to stabilize
  int dhtHumid = dht.readHumidity(); // read humidity
  int dhtTempC = dht.readTemperature(); // read temperature as Celsius
  digitalWrite(DHTENABLEPIN, LOW); // turn off sensor

  if ( dhtTempC < temperatureLowThreshold ) 
  {
    errorFlags |= LOW_TEMPERATURE;
  } 
  else if ( dhtTempC > temperatureHighThreshold ) 
  {
    errorFlags |= HIGH_TEMPERATURE;
  }

  if ( dhtHumid < humidityLowThreshold ) 
  {
    errorFlags |= LOW_HUMIDITY;
  } 
  else if ( dhtHumid > humidityHighThreshold ) 
  {
    errorFlags |= HIGH_HUMIDITY;
  }


  // Coarse Light Level
  digitalWrite(LIGHTENABLEPIN, HIGH); // turn on sensor
  int lightLevel = 1023 - analogRead(LIGHTREADPIN);
  digitalWrite(LIGHTENABLEPIN, LOW); // turn off sensor


  // prepare readings for transmission
  sensorData = String(NODEID);
  sensorData += ":";
  sensorData += int(errorFlags);
  sensorData += ":";
  sensorData += SOILMOISTURE;
  sensorData += ":";
  sensorData += String(moistReadAvg);
  sensorData += ":";
  sensorData += TEMPERATURE;
  sensorData += ":";
  sensorData += String(dhtTempC);
  sensorData += ":";
  sensorData += HUMIDITY;
  sensorData += ":";
  sensorData += String(dhtHumid);
  sensorData += ":";
  sensorData += AMBIENTLIGHT;
  sensorData += ":";
  sensorData += String(lightLevel);
  sensorData += ":";
  sensorData += VOLTAGE;
  sensorData += ":";
  sensorData += batteryVoltage;
  sensorData += ":";
  sensorData += RADIOTEMP;
  sensorData += ":";
  sensorData += String(radioTemperature);
  byte sendSize = sensorData.length();
  sendSize += 1;
  char payload[sendSize];
  sensorData.toCharArray(payload, sendSize); // convert string to char array for transmission
  
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
    String ackType = ackString;
    ackType = ackType.substring(0, 3);
    Serial.println();
    Serial.print("Gateway Responded: ");
    Serial.println(ackType);
    
    // Gateway sent ACK with command.
    if (ackType == "CMD") {
      String cmdString = ackString.substring(3);
      int startParse = 0;
      int endParse = -1;
      String thisCmd;
      int cmd;
      String value;
      int sep;

      do {
        // split ack string into command and value pairs
        startParse = cmdString.indexOf("|", startParse);
        if (startParse > -1) 
        {
          endParse = cmdString.indexOf("|", startParse + 1);
          if (endParse > -1) 
          {
            thisCmd = cmdString.substring(startParse + 1, endParse);
            sep = thisCmd.indexOf(":");
            if (sep > -1) 
            {
              cmd = thisCmd.substring(0, sep).toInt();
              value = thisCmd.substring(sep +1, thisCmd.length() + 1);
              Serial.print("CMD ");
              Serial.println(cmd);
              Serial.print("VAL ");
              Serial.println(value);

              // update variables based on key value
              switch (cmd) {
                case 1:
                  sleepCycle = value.toInt();
                  break;
                case 2:
                  voltageLowThreshold = value.toFloat();
                  break;
                case 3:
                  temperatureLowThreshold = value.toInt();
                  break;
                case 4:
                  temperatureHighThreshold = value.toInt();
                  break;
                case 5:
                  humidityLowThreshold = value.toInt();
                  break;
                case 6:
                  humidityHighThreshold = value.toInt();
                  break;
                case 7:
                  soilDryThreshold = value.toInt();
                  break;
                case 8:
                  soilWetThreshold = value.toInt();
                  break;
              }
            }
          }
          startParse = endParse - 4;
        }
      } 
      while (startParse > -1);
    }
  }
  else 
  {
    // Last transmission has not been acknowledged
    Serial.println();
    Serial.print("WARNING: No acknowledgment from gateway");
  }
  sendSize = (sendSize + 1) % 31;
  Serial.println();

  // Error Level handing
  // If any error level is generated, halve the sleep cycle
  if ( errorFlags != null ) {
    sleepCycle = sleepCycle / 2;
    ledBlink(50);
    ledBlink(50);
    ledBlink(50);
  }

  Sleep();
}

void Sleep()
// Power Saving, go to sleep between data burst
{
  int currentSleep = sleepCycle + random(8); // Randomize sleep cycle a little to reduce collisions with other nodes
  Serial.print("Sleeping for ");
  Serial.print(currentSleep * 8);
  Serial.print(" seconds (");
  Serial.print(sleepCycle);
  Serial.println(")");
  delay(5);
  radio.sleep(); // turn off radio  
  for ( int sleepTime = 0; sleepTime < currentSleep; sleepTime++ ) 
  {
    LowPower.powerDown(SLEEP_1S, ADC_OFF, BOD_OFF); // sleep duration is 8 seconds multiply by the sleep cycle variable.
  }
}

void ledBlink(int DELAY_MS)
// turn LED on and off for DELAY_MS (LED blink)
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(DELAY_MS);
  digitalWrite(LED, LOW);
  delay(DELAY_MS);
}

// LED Pulse fade in and out
void ledPulse() {
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
int getMoistureLevel() {
  int moistReadDelay = 88; // delay to reduce capacitive effects
  int moistAvg = 0;
  // polarity 1 read
  digitalWrite(MOISTPIN1, HIGH);
  digitalWrite(MOISTPIN2, LOW);
  delay (moistReadDelay);
  int moistVal1 = analogRead(MOISTREADPIN);
  digitalWrite(MOISTPIN1, LOW);
  delay (moistReadDelay);
  // polarity 2 read
  digitalWrite(MOISTPIN1, LOW);
  digitalWrite(MOISTPIN2, HIGH);
  delay (moistReadDelay);
  int moistVal2 = analogRead(MOISTREADPIN);
  //Make sure all the pins are off to save power
  digitalWrite(MOISTPIN2, LOW);
  digitalWrite(MOISTPIN1, LOW);
  moistVal1 = 1023 - moistVal1; // invert the reading
  moistAvg = (moistVal1 + moistVal2) / 2; // average readings. report the levels
  return moistAvg;
}

// Battery level check, take 3 readings and use the last to allow circuit to stabilize after waking
void getBatteryLevel(float& voltage) {
  pinMode(VOLTAGEENABLEPIN, OUTPUT); // change pin mode
  digitalWrite(VOLTAGEENABLEPIN, LOW); // turn on the battery meter (sink current)
  for ( int i = 0 ; i < 3 ; i++ ) {
    delay(50); // delay, wait for circuit to stabilize
    voltageRead = analogRead(VOLTAGEREADPIN); // read the voltage 3 times. keep last reading
  }
  voltage = ((voltageRead * VOLTAGEREF) / 1023) * VOLTAGEDIVIDER; // calculate the voltage
  pinMode(VOLTAGEENABLEPIN, INPUT); // turn off the battery meter
}
