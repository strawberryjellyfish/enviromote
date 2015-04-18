/*  Enviromote generic gateway node
    Wait for any data transmissions from sensor nodes and relay them 
    to the base station via serial port.
    Additionally the gateway node listens on serial for any configuration
    commands and passes them to the nodes as required.
*/

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> // https://www.github.com/lowpowerlab/spiflash

#define NODEID        1     // unique for each node on same network
#define NETWORKID     23    // the same on all nodes that talk to each other

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "1234567890123456" // exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    // uncomment only for RFM69HW! Leave out if you have RFM69W!

#define SERIAL_BAUD   115200

#ifdef __AVR_ATmega1284P__
  #define LED           15  // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23  // and FLASH SS on D23
#else
  #define LED           9   // Moteinos have LEDs on D9
  #define FLASH_SS      8   // and FLASH SS on D8
#endif

// Define sensor id types
#define SOILMOISTURE    1
#define TEMPERATURE     2
#define HUMIDITY        3
#define AMBIENTLIGHT    4
#define UVLIGHT         5

#define VOLTAGE         100
#define RADIOTEMP       101

bool promiscuousMode = false; // set to 'true' to sniff all packets on the same network
bool DEBUG = false; // Set to true for debug output on serial

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30); // EF30 for 4mbit  Windbond chip (W25X40CL)

bool configNode = false;
int configNodeId = 0;
String configString = ""; 
byte ackCount = 0;
uint32_t packetCount = 0;


void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  #ifdef IS_RFM69HW
    radio.setHighPower();
  #endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  char buff[50];
  if (DEBUG) {
    sprintf(buff, "\nStarted Listening on %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
  }
}


void loop() {

  // process any serial input
  if (Serial.available() > 0)
  {

    char input = Serial.read();

    if (input == 'D')
    {
      DEBUG = true;
      Serial.println("DEBUG: enabled");
    }

    if (input == 'd')
      DEBUG = false;

    if (input == 'E')
    {
      radio.encrypt(ENCRYPTKEY);
      if (DEBUG) 
        Serial.println("Encryption: enabled");
    }

    if (input == 'e')
    {
      radio.encrypt(null);
      if (DEBUG) 
        Serial.println("Encryption: disabled");
    }

    if (input == 'P')
    {
      radio.promiscuous(true); 
      if (DEBUG) 
        Serial.println("Promiscuous mode: enabled");
    }

    if (input == 'p')
    {
      radio.promiscuous(false);
      if (DEBUG) 
        Serial.println("Promiscuous mode: disabled");
    }

    if (input == 't')
    {
      byte temperature = radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      Serial.print( "Radio Temp: ");
      Serial.print(temperature);
      Serial.print("C");
    }

    if (input == 'r')
    {
      Serial.println("Radio Regs:");
      radio.readAllRegs();
    }

    if (input == 'C') // C = configure remote node
    {
      int startParse;
      int endParse;

      configString = Serial.readStringUntil('\n');
      startParse = configString.indexOf("|", 0);

      if (startParse > -1)
      {
        configNodeId = configString.substring(0, startParse).toInt();
        endParse = configString.lastIndexOf("|");
        if (endParse > -1) {
          configString = "CMD" + configString.substring(startParse, configString.length() );
          configNode = true;          
        }
      }
    }
  }

  // process any received data
  if (radio.receiveDone())
  {
    if (DEBUG) {
      Serial.print("Packets:");
      Serial.print(++packetCount);
      Serial.print(" RX_RSSI:");
      Serial.print(radio.RSSI);
      Serial.print(" Sender ID:");
      Serial.print(radio.SENDERID, DEC);

      if (promiscuousMode)
      {
        Serial.print(" Target ID:");
        Serial.print(radio.TARGETID, DEC);
      }
      Serial.println();
      Serial.print("Received: ");
      for (byte i = 0; i < radio.DATALEN; i++) 
      {
        Serial.print((char)radio.DATA[i]);
      }
      Serial.println();
    } 
    else 
    {
      // Just relay the received data string to serial
      Serial.print("DATA:");
      for (byte i = 0; i < radio.DATALEN; i++) 
      {
        Serial.print((char)radio.DATA[i]);
      }
      Serial.println();
    }
    
    if (radio.ACKRequested())
    {
      String ackString = "ACK";

      // Either configure all nodes or this is the specific node we want to update
      if (configNode && (configNodeId == radio.SENDERID || configNodeId == 0)) {
        ackString = configString;
        LEDBlink(100, 25);
        LEDBlink(450, 25);
        LEDBlink(100, 25);
        LEDBlink(450, 25);
      }
      
      char __ackString[ackString.length() + 1];

      ackString.toCharArray(__ackString, sizeof(__ackString));
      radio.sendACK(__ackString, strlen(__ackString));
      // TODO: Only set configNode false when all nodes have been configured
      // at present only the first node to send a data burst will be reconfigured
      configNode = false;
      configNodeId = 0;
      configString = "";

      if (DEBUG) {
        Serial.println();
        Serial.print("ConfigNode: ");
        Serial.print(configNode);
        Serial.print("  ConfigNodeId: ");
        Serial.print(configNodeId);
        Serial.print("  Radio Sender: ");
        Serial.print(radio.SENDERID);
        Serial.print("ACK sent [");
        Serial.print(__ackString);
        Serial.println("]");    
      }
    }
    LEDPulse(4);
  }
}


// turn LED on and off for DELAY_MS (LED blink)
void LEDBlink(int ON_MS, int OFF_MS)
{
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  delay(ON_MS);
  digitalWrite(LED, LOW);
  delay(OFF_MS);
}

// LED Pulse fade in and out, just a subtle pulse so not going to full value
void LEDPulse(int ON_MS) {
  int i;
  for (int i = 0; i < 100; i++) {
    analogWrite(LED, i);
    delay(ON_MS);
  }
  for (int i = 100; i > 0; i--) {
    analogWrite(LED, i);
    delay(ON_MS);       
  }
  digitalWrite(LED, LOW);
  delay (ON_MS * 100);
}