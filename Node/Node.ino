// Enviromote generic sensor node
// The will send short sensor data transmissions to the gateway
// then sleep to preserve power until the next transmission.
// After transmitting it will expect and ACK from the gateway,
// if the ACK contains a command string the none will execute the command
// before sleeping again.

#include <RFM69.h>    // https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <LowPower.h> // https://github.com/rocketscream/Low-Power

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


int sleepCycle = 1; // How many lowpower library 8 second sleeps *450 = 1 hour
char payload[] = "123 ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char buff[20];
byte sendSize=0;
RFM69 radio;

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);

  #ifdef IS_RFM69HW
    radio.setHighPower(); //uncomment only for RFM69HW!
  #endif
  
  radio.encrypt(ENCRYPTKEY);
  radio.sleep();
  
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
}

void loop() {
  
  // TODO: plugin sensor read code
  //getSensorData();
  
  Serial.print("Sending [");
  Serial.print(sendSize);
  Serial.print("]: ");

  for(byte i = 0; i < sendSize; i++)
    Serial.print((char)payload[i]);
  
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
    Serial.println(ackString);
    
    if (ackString == "CMD") {
      Serial.println("Received ACK CMD");
      // Gateway sent ACK with command.
      // TODO: parse ackString and act as requested
    }
  }
  else 
  {
    // Last transmission has not beeen acknowledged
    Serial.print(" No acknowkedgement from gateway");
  }
  sendSize = (sendSize + 1) % 31;
  Serial.println();
  Blink(LED,3);
  Sleep();
}

void Sleep()
// Power Saving, go to sleep between data burst
{
  int currentSleep = sleepCycle + random(8); // Randomize sleep cycle a little to reduce collisions with other nodes
  Serial.print("Sleeping for ");
  Serial.print(currentSleep * 8);
  Serial.println(" seconds");
  delay(5);
  radio.sleep(); // turn off radio  
  for ( int sleepTime = 0; sleepTime < currentSleep; sleepTime++ ) 
  {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); // sleep duration is 8 seconds multiply by the sleep cycle variable.
  }
}

void Blink(byte PIN, int DELAY_MS)
// set PIN high and low for DELAY_MS (LED blink)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
  delay(DELAY_MS);
}
