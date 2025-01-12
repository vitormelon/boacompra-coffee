#include <Arduino.h>
// WIFI

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

//EEPROM
#include <EEPROM.h>

//CONFIGURATION
const char NODE_UUID[37]        = "";
const String HASURA_URL         = "";
const String HASURA_FINGERPRINT = "";
const String HASURA_SECRET      = "";


//EEPROM
const int EEPROM_ADDRESS = 0;

// LOGS 
const int LOGS_ENABLED = false; 

// BUTTONS
const int BUTTON_GREEN = 16;
const int BUTTON_RED = 5;

// LEDS
const int LED_GREEN = 12;
const int LED_RED = 13;
const int LED_BLUE = 4;

// POSSIBLE STATES (CAN CHANGE TO ENUM?)
const int STATE_HAVE = 0;
const int STATE_HAVENT = 1;
const int STATE_LOADING = 2;
const int STATE_LOADED = 3;
const int STATE_ERROR = 4;

// ACTUAL STATE (STARTED AS LOADING)
int state = 2;

// STATE MONITOR
int stateChanged = false;

//FUNCTIONS
void updateLED();
void setLEDRed();
void setLEDGreen();
void setLEDBlue();
void setLEDOff();
bool sendMessage();
void saveState(int state);
int initializeState();


void setup() {
  // INITIALIZE SERIAL
  Serial.begin(9600);

  // INITIALIZE EEPROM
  EEPROM.begin(4);
  Serial.print("EEPROM: ");
  Serial.println(EEPROM.read(0));

  // INITIALIZE I/O
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_GREEN,INPUT);
  pinMode(BUTTON_RED,INPUT);
  pinMode(LED_GREEN,OUTPUT);
  pinMode(LED_RED,OUTPUT);
  pinMode(LED_BLUE,OUTPUT);

  // UPDATE LED STATE
  updateLED();

  // CONNECT TO WIFI
  WiFiManager wifiManager;

  wifiManager.setTimeout(180);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  } 

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  delay(10);
  
  // CHANGE STATE AFTER SUCCESSFUL CONNECTION
  state = initializeState();

  Serial.print("state: ");
  Serial.println(state);
  
  // UPDATE LED STATE
  updateLED();
}



void loop() {
  // INITIALIZE STATE MONITOR
  stateChanged = false;
  
  // READ BUTTON STATES
  int button_have_state = digitalRead(BUTTON_GREEN);
  if(button_have_state == HIGH){
    delay(300);
    button_have_state = digitalRead(BUTTON_GREEN);
  }
  
  int button_havent_state = digitalRead(BUTTON_RED);
  if(button_havent_state == HIGH){
    delay(300);
    button_havent_state = digitalRead(BUTTON_RED);
  }

  // LOGS
  if (LOGS_ENABLED) {
    Serial.print("Have: ");
    Serial.println(button_have_state);
    Serial.print("Haven't: ");
    Serial.println(button_havent_state);
  }

  // UPDATE STATE ON BUTTON PRESS
  if(button_have_state == HIGH && state != STATE_HAVE)
  {
    state = STATE_HAVE;
    stateChanged = true;
  } else {
    if(button_havent_state == HIGH && state != STATE_HAVENT)
    {
      state = STATE_HAVENT;
      stateChanged = true;
    }
  }

  if (stateChanged) {
   
    // UPDATE LED STATE
    setLEDBlue();
    
    // SEND MESSAGE TO SNS
    if(!sendMessage()){
      saveState(STATE_LOADED);
      ESP.reset();
    }
   
    saveState(state);
    // UPDATE LED STATE
    updateLED();
  }
}

// MESSAGING FUNCTIONS

bool sendMessage() {
  HTTPClient http;
  
  char hasCoffee[5];
  char message[256];

  http.begin(HASURA_URL, HASURA_FINGERPRINT);
  http.addHeader("Content-Type", "application/json", false, true);
  http.addHeader("x-hasura-admin-secret", HASURA_SECRET);

  if (state == STATE_HAVE) {
    strcpy(hasCoffee, "true");
  } else {
    strcpy(hasCoffee, "false");
  }
  
  snprintf(
    message, 
    sizeof message, 
    "{\"query\":\"mutationMyMutation{insert_events(objects:{event:{hasCoffee:%s},node_id:\\\"%s\\\"}){returning{event_id}}}\"}", 
    hasCoffee, 
    NODE_UUID);

  int httpCode = http.POST(message);

  Serial.print("Mensagem enviada: ");
  Serial.println(message);
  
  Serial.print("Http Code: ");
  Serial.println(httpCode);

  String payload = http.getString();
  Serial.print("Resposta: ");
  Serial.println(payload);
  return true;
}

// LED FUNCTIONS

void updateLED() {
  switch (state) {
    case STATE_HAVE:
      setLEDGreen();
      break;
    case STATE_HAVENT:
      setLEDRed();
      break;
    case STATE_LOADING:
      setLEDBlue();
      break;
    case STATE_LOADED:
      setLEDOff();
      break;
    case STATE_ERROR:
      setLEDOff();
      break;
  }
}

void setLEDOff() {
  digitalWrite(LED_BLUE,LOW);
  digitalWrite(LED_GREEN,LOW);
  digitalWrite(LED_RED,LOW);
}

void setLEDRed() {
  digitalWrite(LED_BLUE,LOW);
  digitalWrite(LED_GREEN,LOW);
  digitalWrite(LED_RED,HIGH);
}

void setLEDBlue() {
  digitalWrite(LED_BLUE,HIGH);
  digitalWrite(LED_GREEN,LOW);
  digitalWrite(LED_RED,LOW);  
}

void setLEDGreen() {
  digitalWrite(LED_BLUE,LOW);
  digitalWrite(LED_GREEN,HIGH);
  digitalWrite(LED_RED,LOW);
}

void saveState(int state){
  EEPROM.write(EEPROM_ADDRESS, state);
  EEPROM.commit();
}

int getState(){
  return EEPROM.read(EEPROM_ADDRESS);
}

int initializeState(){
  int state = getState();
  if(state < 4 && state >= 0){
    return state;
  }
  return STATE_LOADED;
}