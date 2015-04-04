// Enviromote generic sensor node

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> // https://www.github.com/lowpowerlab/spiflash

#define GATEWAYID     1
#define NODEID        2    // unique for each node on same network
#define NETWORKID     23  // the same on all nodes that talk to each other

//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
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

int TRANSMITPERIOD = 500; //transmit a packet to gateway so often (in ms)
char payload[] = "123 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char buff[20];
byte sendSize=0;
boolean requestACK = false;
RFM69 radio;

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
}

long lastPeriod = 0;
void loop() {
  //process any serial input
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input >= 48 && input <= 57) //[0,9]
    {
      TRANSMITPERIOD = 100 * (input-48);
      if (TRANSMITPERIOD == 0) TRANSMITPERIOD = 1000;
      Serial.print("\nChanging delay to ");
      Serial.print(TRANSMITPERIOD);
      Serial.println("ms\n");
    }

    if (input == 'r') //d=dump register values
      radio.readAllRegs();
      
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      Serial.print( "Radio Temp is ");
      Serial.print(temperature);
      Serial.print("C, ");
      Serial.print(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      Serial.println('F');
    }
  }

  //check for any received packets
  if (radio.receiveDone())
  {
    Serial.print("Received [");
    Serial.print(radio.SENDERID, DEC);
    Serial.print("] ");
    
    Serial.print("[RX_RSSI:");
    Serial.print(radio.RSSI);
    Serial.print("] ");

    Serial.print("Node Says: ");
    for (byte i = 0; i < radio.DATALEN; i++) 
    {
      Serial.print((char)radio.DATA[i]);
    }

    if (radio.ACKRequested())
    {
      radio.sendACK();
      Serial.println();
      Serial.print("Responded to ACK requested by Gateway");
    }
    Blink(LED,3);
    Serial.println();
  }

  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod)
  {
    lastPeriod=currPeriod;
    
    Serial.print("Sending [");
    Serial.print(sendSize);
    Serial.print("]: ");

    for(byte i = 0; i < sendSize; i++)
      Serial.print((char)payload[i]);
  
    if (radio.sendWithRetry(GATEWAYID, payload, sendSize)) 
    {
      Serial.print(" Send OK!");
    }
    else 
    {
      Serial.print(" No acknowkedgement from gateway");
    }
    sendSize = (sendSize + 1) % 31;
    Serial.println();
    Blink(LED,3);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
