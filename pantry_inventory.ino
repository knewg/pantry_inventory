String sendMessage;
String receivedMessage;

#define RXD2 16
#define TXD2 17
#define MODE_BUTTON 19
#define SCAN_BUTTON 18
#define DEBOUNCE_TIME  50

#define SCANMODE_OUT 0
#define SCANMODE_IN 1

#define SCAN_OFF 0
#define SCAN_ON 1

#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "credentials.h"

// OLED Display
#define SSD1306_NO_SPLASH 1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* mainTopic = "foodInventoryControl/";
const char* statusTopic = "foodInventoryControl/status";
const char* commandTopic = "foodInventoryControl/control";
const char* barcodeTopic = "foodInventoryControl/barcode";
const char* modeTopic = "foodInventoryControl/mode";

WiFiClient wificlient;
PubSubClient client(wificlient);
unsigned long lastMessage = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Mode button
bool scanMode = SCANMODE_IN;
int modeButtonLast = HIGH;  // the previous state from the input pin
int modeButtonDebounce = HIGH;
int modeButtonCurrent;

bool scan = SCAN_ON;
int scanButtonLast = HIGH;  // the previous state from the input pin
int scanButtonDebounce = HIGH;
int scanButtonCurrent;

unsigned long lastDebounceTime = 0;

void setup(){
  pinMode(MODE_BUTTON, INPUT_PULLUP);
  pinMode(SCAN_BUTTON, INPUT_PULLUP);
  Wire.begin();
  Serial.begin(9600);    // Initialize the Serial monitor for debugging
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  delay(1000);
    Serial2.println("~M00910001."); //Code mode on
    WiFi.mode(WIFI_STA); //Optional
    WiFi.begin(ssid, password);
    Serial.print("\nStarting screen: ");
    while(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
      Serial.print(".");
      delay(100);
    }
    display.clearDisplay();
    display.setTextSize(1);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    //display.drawPixel(10, 10, SSD1306_WHITE);
    display.print(F("Wifi connect: ")); 
    display.display();
    Serial.print("\nConnecting Wifi: ");

    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        display.print(F("."));
        display.display();
        delay(100);
    }
    randomSeed(micros());

    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    display.print(F("\nIP: ")); 
    display.println(WiFi.localIP());
    display.display();
    Serial.println(WiFi.localIP());
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void setScanMode(bool newScanMode) { 
  if(newScanMode == SCANMODE_IN)
  {
    client.publish(modeTopic, "In");
  }
  else
  {
    client.publish(modeTopic, "Out");
  }
  scanMode = newScanMode;
  renderStatusDisplay();
}

void setScanning(bool newScanningState) {
  if(newScanningState == SCAN_ON)
  {
    client.publish(statusTopic, "Reading");
    Serial2.println("~M00220000.");
  }
  else
  {
    client.publish(statusTopic, "Idle");
    Serial2.println("~M00220001.");
  }
  scan = newScanningState;
  renderStatusDisplay();
}

void renderStatusDisplay () {
  display.clearDisplay();
  display.setCursor(0,0);
  display.print(F("ScanMode: "));
  if(scanMode == SCANMODE_IN) {
    display.println(F("In"));
  } else {
    display.println(F("Out"));
  }
  display.print(F("Scanning: "));
  if(scan == SCAN_ON) {
    display.println(F("Active"));
  } else {
    display.println(F("Off"));
  }
  display.display();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    if ((char)payload[i] == '\n') continue;
    sendMessage += ((char)payload[i]);
  }
  if(sendMessage == "READ") {
    setScanning(SCAN_ON);
  } else if (sendMessage == "STOP") {
    setScanning(SCAN_OFF);
  } else if (sendMessage == "SCANMODE_IN") {
    setScanMode(SCANMODE_IN);
  } else if (sendMessage == "SCANMODE_OUT") {
    setScanMode(SCANMODE_OUT);
  } else {
     Serial2.println(sendMessage);
  }
  Serial.println(sendMessage);
  sendMessage = "";
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    //display.setCursor(0,0);
    display.print(F("MQTT Reconnect: "));
    display.display();
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "FoodInventoryBot-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      display.println(F("OK"));
      display.display();
      // Once connected, publish an announcement...
      client.publish(statusTopic, "Connected");
      //client.publish(barcodeTopic, "");
      setScanMode(scanMode);
      setScanning(scan);
      client.publish(commandTopic, " ");
      // ... and resubscribe
      client.subscribe(commandTopic);
    } else {
      display.println(F("Fail"));
      display.display();
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // put your main code here, to run repeatedly:
  unsigned long now = millis();
  while (Serial2.available() > 0) {
    lastMessage = now;
    char receivedChar = Serial2.read();
    if (receivedChar == '\n') {
      client.publish(barcodeTopic, receivedMessage.c_str());
      Serial.println(receivedMessage);  // Print the received message in the Serial monitor
      receivedMessage = "";  // Reset the received message
    } else {
      receivedMessage += receivedChar;  // Append characters to the received message
    }
  }
  if (now - lastMessage > 2000) {
    receivedMessage = ""; //reset after 2 seconds, to avoid jitter
  }
  // Check mode switch button status
  modeButtonCurrent = digitalRead(MODE_BUTTON);
  if(modeButtonCurrent != modeButtonDebounce) {
    modeButtonDebounce = modeButtonCurrent;
    lastDebounceTime = now;
  }
  if ((now - modeButtonDebounce) > DEBOUNCE_TIME) {
    if (modeButtonLast == HIGH && modeButtonCurrent == LOW) {
      Serial.println("Mode button is pressed");
    }
    else if (modeButtonLast == LOW && modeButtonCurrent == HIGH) {
      Serial.println("Mode button is released");
      setScanMode(!scanMode); //Reverse
    }
    modeButtonLast = modeButtonCurrent;
  }
  // Check scanning button status
  scanButtonCurrent = digitalRead(SCAN_BUTTON);
  if(scanButtonCurrent != scanButtonDebounce) {
    scanButtonDebounce = scanButtonCurrent;
    lastDebounceTime = now;
  }
  if ((now - scanButtonDebounce) > DEBOUNCE_TIME) {
    if (scanButtonLast == HIGH && scanButtonCurrent == LOW) {
      Serial.println("Scan button is pressed");
    }
    else if (scanButtonLast == LOW && scanButtonCurrent == HIGH) {
      Serial.println("Scan button is released");
      setScanning(!scan); //Reverse
    }
    scanButtonLast = scanButtonCurrent;
  }
  
}
