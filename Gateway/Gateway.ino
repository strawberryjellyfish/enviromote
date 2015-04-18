// Sample RFM69 receiver/gateway sketch, with ACK and optional encryption
// Passes through any wireless received messages to the serial port & responds to ACKs

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> // https://www.github.com/lowpowerlab/spiflash

#define NODEID        1    // unique for each node on same network
#define NETWORKID     23  // the same on all nodes that talk to each other

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "1234567890123456" //exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!

#define SERIAL_BAUD   115200

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

// Define sensor id types
#define SOILMOISTURE    1
#define TEMPERATURE     2
#define HUMIDITY        3
#define AMBIENTLIGHT    4

#define VOLTAGE         100
#define RADIOTEMP       101

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network
bool DEBUG = false;
bool configNode = false; //if true will send a configuration string to the node in response to a data burst
int nodeId = 0; // ID of node to send config to. (0 = all nodes)
String configString = "";


void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); // only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);
  char buff[50];
  if (DEBUG) {
    sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
    Serial.println(buff);
  }
}

byte ackCount=0;
uint32_t packetCount = 0;

void loop() {

  //process any serial input
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

    if (input == 'P') // enable promiscuousMode
    {
      radio.promiscuous(false); 
      if (DEBUG) 
        Serial.println("Promiscuous mode: enabled");
    }

    if (input == 'p') // disable promiscuousMode
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

    if (input == 'C') // c = configure remote node
    {
      String configString;
      String configNode;
      int startParse;
      int endParse;

      configString = Serial.readStringUntil('\n');
      startParse = configString.indexOf("|", 0);

      if (startParse > -1)
      {
        configNode = configString.substring(0, startParse);
        endParse = configString.lastIndexOf("|");
        if (endParse > -1) 
          configString = configString.substring(startParse, configString.length() );
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
      for (byte i = 0; i < radio.DATALEN; i++) 
      {
        Serial.print((char)radio.DATA[i]);
      }
      Serial.println();
    }
    
    if (radio.ACKRequested())
    {
      char* ackString ="ACK";
      //char* ackString = "CMD|1:10|";
      radio.sendACK(ackString, strlen(ackString)); // Send ACK or CMD|key:value|....
      if (DEBUG) {
        Serial.println();
        Serial.print("ACK sent [");
        Serial.print(ackString);
        Serial.println("]");
      }
    }
    LEDPulse();
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