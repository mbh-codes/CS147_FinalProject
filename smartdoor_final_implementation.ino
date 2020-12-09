#include "WiFiEsp.h"
#include <Wire.h>
#include "rgb_lcd.h"
#include <Servo.h>

// Emulate Serial1 on pins 9/8 if not present
#ifndef HAVE_HWSERIAL1
#include <SoftwareSerial.h>
SoftwareSerial Serial1(9,8); // TX, RX
#endif

const int buttonPin = 2; //the number of the pushbutton pin
const int servoPin = 3; // the number of the servo pin
const int buzzerPin = 4; // the number of the buzzer pin
const int touchPin = 5; // the number of the touch pin

const int blueRightPin = 10; // the number of the blue right LED pin
const int blueLeftPin = 11; // the number of the blue left LED pin
const int redRightPin = 12; // the number of the red right LED pin
const int redLeftPin = 13; // the number of the red left LED pin

// Wifi variables
char ssid[] = "Network SSID"; // the network SSID (name)
char pass[] = "Password"; // network password
int status = WL_IDLE_STATUS; // the Wifi radio's status
char server[] = "SERVER IP ADDRESS"; // the server IP ADDRESS
char get_request[200];
WiFiEspClient client; // the ethernet client object

// LED variables
unsigned long ledPreviousMillis = 0;
int ledTimeCount = 0;
unsigned long ledMaxTime = 10; // 10 is one second
unsigned long ledInterval = 500;
unsigned long ledState = 0;
bool ledTimer = false;

/// LCD RGB Backlight variables
const int lockedR = 255;
const int lockedG = 0;
const int lockedB = 0;
const int ringR = 255;
const int ringG = 255;
const int ringB = 0;
const int openR = 0;
const int openG = 0;
const int openB = 255;
rgb_lcd lcd;
const String lockedText = "DOOR: LOCKED";
const String ringedText = "DOOR: RINGED";
const String unlockedText = "DOOR: UNLOCKED";

/// Servo variables
Servo myservo;
const int lockedAngle = 90;
const int unlockedAngle = 0;

// Connection variables
const int NOTCONNECTED = 0;
const int CONNECTED = 1;
int connectionStatus = NOTCONNECTED;

/// Logic variables
const int LOCKED = 0;
const int LOCKED_RINGED = 1;
const int UNLOCKED = 2;
const int UNLOCKED_RINGED = 3;
int doorStatus = LOCKED;

const long blinkingTime = 5000;
const long blinkingInterval = 500;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // communication with the host computer
  Serial1.begin(115200); // communication with the ESP8266
  
  pinMode(buttonPin, INPUT);     // pin 2
  myservo.attach(servoPin);      // pin 3
  pinMode(buzzerPin, OUTPUT);    // pin 4
  pinMode(touchPin, INPUT);      // pin 5
  pinMode(blueRightPin, OUTPUT); // pin 10
  pinMode(blueLeftPin, OUTPUT);  // pin 11
  pinMode(redRightPin, OUTPUT);  // pin 12
  pinMode(redLeftPin, OUTPUT);   // pin 13
  lcd.begin(16, 2);
  lcd.setRGB(lockedR, lockedG, lockedB);
  lcd.print(lockedText);
  myservo.write(lockedAngle);

  WiFi.init(&Serial1); // initialize ESP module
  // check for the of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); // don't continue
    while (true);
  }
  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED ) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }
  Serial.println("You're  connected to the network");
  printWifiStatus();
  trackState();
  delay(1000);
}

void loop() {
  switch (doorStatus) {
    case LOCKED:
      doorLocked();
      break;
    case LOCKED_RINGED:
      doorLockedRinged();
      break;
    case UNLOCKED:
      doorUnlocked();
      break;
    case UNLOCKED_RINGED:
      doorUnlockedRinged();
      break;
  }
}

void checkMasterButton(bool locked) {
  // DOOR IS ALREADY LOCKED, CHECKING MASTER KEY
  int touched = digitalRead(touchPin);
  if (touched && locked) {
      Serial.println(unlockedText);
      doorStatus = UNLOCKED;
      updateBackLight();
      startLedTimer();
      updateLock();
      turnOffLed();
      trackState();
      delay(250);
  } else if (touched && !locked){
      Serial.println(lockedText);
      doorStatus = LOCKED;
      updateBackLight();
      startLedTimer();
      updateLock();
      turnOffLed();
      trackState();
      delay(250);
  }
}

void checkSlaveButton(bool locked) {
  int ringed = digitalRead(buttonPin);
  if (ringed && locked) {
    Serial.println(ringedText);
    doorStatus = LOCKED_RINGED;
    updateBackLight();
    startLedTimer();
    trackState();
    delay(250);
  } else if (ringed && !locked) {
    Serial.println(ringedText);
    doorStatus = UNLOCKED_RINGED;
    updateBackLight();
    startLedTimer();
    trackState();
    delay(250);
  }
}

void doorLocked() {
  checkMasterButton(true);
  checkSlaveButton(true);
  if (doorStatus == LOCKED) {
    //Do nothing
    handleLedTimer();
  }
}

void doorLockedRinged() {
  checkMasterButton(true);
  checkSlaveButton(true);
  if (doorStatus == LOCKED_RINGED) {
    // handle the ring
    handleLedTimer();
  }
}

void doorUnlocked() {
  checkMasterButton(false);
  checkSlaveButton(false);
  if (doorStatus == UNLOCKED) {
    //handle the unlocked
    handleLedTimer();
  }
}

void doorUnlockedRinged() {
  checkMasterButton(false);
  checkSlaveButton(false);
  if (doorStatus == UNLOCKED_RINGED) {
    //handle the unlocked
    handleLedTimer();
  }
}

void handleLedTimer() {
  checkLedTimer();
  unsigned long current = millis();
  if (current - ledPreviousMillis >= ledInterval && ledTimer) {
    ledTimeCount += 1;
    ledPreviousMillis = current;
    switch (doorStatus) {
        case LOCKED:
          redLights();
          break;
        case LOCKED_RINGED:
          policeStrob();
          break;
        case UNLOCKED:
          blueLights();
          break;
        case UNLOCKED_RINGED:
          blueLights();
          break;
    }
  }
}

void updateBackLight() {
  lcd.setCursor(0,0);
  lcd.clear();
  switch (doorStatus) {
    case LOCKED:
      lcd.print(lockedText);
      lcd.setRGB(lockedR, lockedG, lockedB);
      break;
    case LOCKED_RINGED:
      lcd.print(lockedText);
      lcd.setCursor(0,1);
      lcd.print(ringedText);
      lcd.setRGB(ringR, ringG, ringB);
      break;
    case UNLOCKED:
      lcd.print(unlockedText);
      lcd.setRGB(openR, openG, openB);
      break;
    case UNLOCKED_RINGED:
      lcd.print(unlockedText);
      lcd.setRGB(openR, openG, openB);
      lcd.setCursor(0,1);
      lcd.print(ringedText);
      break;
  }
}

void updateLock() {
  switch (doorStatus) {
    case LOCKED:
      myservo.write(lockedAngle);
      Serial.print("locking door");
      break;
    case UNLOCKED:
      myservo.write(unlockedAngle);
      Serial.println("unlocking door");
      break;
  }
}

void turnOffLed() {
  digitalWrite(buzzerPin, LOW);
  digitalWrite(blueRightPin, LOW); 
  digitalWrite(blueLeftPin, LOW); 
  digitalWrite(redLeftPin, LOW);  
  digitalWrite(redRightPin, LOW); 
  ledState = 0;
}

void policeStrob() {
  if (ledState) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(blueRightPin, HIGH); 
    digitalWrite(blueLeftPin, HIGH); 
    digitalWrite(redLeftPin, LOW);  
    digitalWrite(redRightPin, LOW); 
    ledState = 0;
  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(blueRightPin, LOW); 
    digitalWrite(blueLeftPin, LOW); 
    digitalWrite(redLeftPin, HIGH);
    digitalWrite(redRightPin, HIGH);  
    ledState = 1;
  }
}

void bothLights() {
  if (ledState) {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(blueRightPin, LOW);
    digitalWrite(blueLeftPin, LOW);
    digitalWrite(redLeftPin, LOW);
    digitalWrite(redRightPin, LOW);
    ledState = 0;
  } else {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(blueRightPin, HIGH);
    digitalWrite(blueLeftPin, HIGH);
    digitalWrite(redLeftPin, HIGH);
    digitalWrite(redRightPin, HIGH);
    ledState = 1;
  }
}

void redLights() {
  if (ledState) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(blueRightPin, LOW);
    digitalWrite(blueLeftPin, LOW);
    digitalWrite(redLeftPin, HIGH);
    digitalWrite(redRightPin, HIGH);
    ledState = 0;
  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(blueRightPin, LOW);
    digitalWrite(blueLeftPin, LOW);
    digitalWrite(redLeftPin, LOW);
    digitalWrite(redRightPin, LOW);
    ledState = 1;
  }
}

void blueLights() {
  if (ledState) {
    digitalWrite(buzzerPin, HIGH);
    digitalWrite(blueRightPin, HIGH);
    digitalWrite(blueLeftPin, HIGH);
    digitalWrite(redLeftPin, LOW);
    digitalWrite(redRightPin, LOW);
    ledState = 0;
  } else {
    digitalWrite(buzzerPin, LOW);
    digitalWrite(blueRightPin, LOW);
    digitalWrite(blueLeftPin, LOW);
    digitalWrite(redLeftPin, LOW);
    digitalWrite(redRightPin, LOW);
    ledState = 1;
  }
}

void startLedTimer() {
  ledTimer = true;
  ledTimeCount = 0;
}

void checkLedTimer() {
  unsigned long current = millis();
  if (ledTimeCount >= ledMaxTime) {
    ledTimer = false;
    turnOffLed();
  }
}

void printWifiStatus() {
  // print the SSID of the network you're attached to 
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP(); 
  Serial.print("IP Address: "); 
  Serial.println(ip);
  // print the received signal strength 
  long rssi = WiFi.RSSI(); 
  Serial.print("Signal strength (RSSI):"); 
  Serial.print(rssi);
  Serial.println(" dBm"); 
}

void trackState() {
  // Will make an api call to track current start
  int locked = 1;
  int ringed = 0;
  switch (doorStatus) {
    case LOCKED:
      locked = 1;
      ringed = 0;
      break;
    case LOCKED_RINGED:
      locked = 1;
      ringed = 1;
      break;
    case UNLOCKED:
      locked = 0;
      ringed = 0;
      break;
    case UNLOCKED_RINGED:
      locked = 0;
      ringed = 1;
      break;
  }
  Serial.println();
  if(!client.connected()) {
    Serial.println("Starting connection to server...");
    client.connect(server, 5000);    
  }
  Serial.println("Connected to Server");
  Serial.println();
  Serial.println("Tracking State");
  Serial.print("LOCKED: ");
  Serial.println(locked);
  Serial.print("RINGED: ");
  Serial.println(ringed);
  sprintf(get_request, "GET /update/?LOCKED=%d&RINGED=%d HTTP/1.1\r\nHOST: 18.188.140.205\r\nConnection: close\r\n\r\n", locked, ringed);
  client.print(get_request);
  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
}
