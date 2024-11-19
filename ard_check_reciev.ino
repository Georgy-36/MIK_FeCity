#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

RF24 radio(7, 8); // CE, CSN
const byte address[6] = "00001";
const int maxPacketSize = 30;

char receivedData[maxPacketSize + 1];
unsigned long lastReceiveTime = 0;
const unsigned long receiveTimeout = 1000;

void setup() {
  Serial.begin(9600);
  
  if (!radio.begin()) {
    Serial.println("NRF24L01 is not responding!");
    while (1);
  }
  
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_HIGH);
  radio.startListening();
  
  Serial.println("NRF24L01 receiver started");
}

void loop() {
  if (radio.available()) {
    radio.read(&receivedData, maxPacketSize);
    receivedData[maxPacketSize] = '\0';
    Serial.print("Received: ");
    Serial.println(receivedData);
    lastReceiveTime = millis();
  }
  
  if (millis() - lastReceiveTime > receiveTimeout) {
    Serial.println("No data from beacon...");
    lastReceiveTime = millis();
  }
  
  delay(1);
}
