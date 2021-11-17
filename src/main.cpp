#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "secrets.h"        //SSID PWD OctoprintIP KEY
#include "displayConsts.h"  //display encodings to draw to 7 Segments

//connection to shiftregister
#define SHIFTPIN 13
#define STOREPIN 15
#define DATAPIN 2
#define STOPPIN 12

//wifi and connection
WiFiManager wifiManager;
WiFiClient client;
HTTPClient http;

StaticJsonDocument<1200> apiAnswere; //store rest answere

//softwaretimer
#define UPDATETIME 500
#define BLINKTIMER 100
unsigned long updateTimer = 0;

boolean isWificonnected = 0;    //stored for connecteionanimation
boolean showDot = false;

//printing information
boolean displayError = false;
String jobState = "";
int printProgress = 0;
byte isHeating[2] = {0,0}; //store if bed and hotend is heating
#define BED 0
#define HOTEND 1
float bedTargetTemperature = 0;   //temperatures
float bedCurrentTemperature = 0;
float toolTargetTemperature = 0;
float toolCurrentTemperature = 0;






/**********************************************************************************************
 * 7 segment display
 **********************************************************************************************/
//value must resembel the encoding
void printTo7Segment(int vlaue){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, (vlaue & 0xFF) | isHeating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, ((vlaue & 0xFF00)>>8)| isHeating[HOTEND]);
  digitalWrite(STOREPIN, HIGH);
}

//print "ER" to display
void errorTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, rCode| isHeating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, eCode| isHeating[HOTEND]);
  digitalWrite(STOREPIN, HIGH);
}

//print "_ _" to diaply
void lowlineTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| isHeating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| isHeating[HOTEND]);
  digitalWrite(STOREPIN, HIGH);
}

//print "_ _" to diaply
void cleartSegment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0);
  digitalWrite(STOREPIN, HIGH);
}

//create number to print
int createnumber(int input){
  //process left Digit
  int leftDigit = (input / 10)%10;
  int leftDigitEncoding = numbersCodes[leftDigit]<<8;
  
  //process right digit
  int rightDigit = input % 10;
  int rightDigitEncoding = numbersCodes[rightDigit];
  
  //clac value for shifting to display
  return leftDigitEncoding | rightDigitEncoding;
}









/**********************************************************************************************
 * update display
 **********************************************************************************************/
//print heating progress as two circals
void printHeatingProgress(){
  //calc circelparts
  int bedProgress = int((6/bedTargetTemperature)*(bedCurrentTemperature));
  int toolProgress = int((6/toolTargetTemperature)*toolCurrentTemperature);

  //correct output
  if(bedProgress<1 | bedProgress > 7){
    bedProgress=1;
  }

  if(bedProgress<1 | toolProgress > 7){
    toolProgress=1;
  }
  
  //print information
  //Serial.print(bednum);
  //Serial.println(toolnum);
  printTo7Segment(((ringCodes[toolProgress-1])<<8)+(ringCodes[bedProgress-1]));
}

//print informations to display
void printToDisplay(){
  if(displayError){
    errorTo7Segment();
  }else{
    if(jobState.equals("Printing")){
      if((toolTargetTemperature==0) || (bedTargetTemperature==0) || ((toolTargetTemperature-toolCurrentTemperature)>2)){
        //printer heats up
        printHeatingProgress();
      }else{
        //display information (print progress)
        printTo7Segment(createnumber(printProgress));
      }
    }else if(jobState.equals("Cancelling") or jobState.equals("Operational") ){
      if(((abs(toolTargetTemperature-toolCurrentTemperature))>2)| ((abs(bedTargetTemperature-bedCurrentTemperature))>2)){
        //printer is cooling down
        printHeatingProgress();
      }else{
        //print idel status
        lowlineTo7Segment();
      }
    }else{
      errorTo7Segment();
    }
  }
}





/**********************************************************************************************
 * api / update methods
 **********************************************************************************************/
//get Termperature from Octoprint via rest call
void getTemperature(){
  //make an api call for temperature
  if(http.begin(client, String(HOST) + "/api/printer?apikey=" + String(APIKEY))){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){
        //get payload
        String payload = http.getString();
        DeserializationError error = deserializeJson(apiAnswere, payload);
        
        //handel and sktip
        if (error) {
          http.end();
          displayError = true;
          return;
        }
        
        //pars json
        JsonObject answere = apiAnswere.as<JsonObject>();
        bedTargetTemperature = answere["temperature"]["bed"]["target"];
        bedCurrentTemperature = answere["temperature"]["bed"]["actual"];
        toolTargetTemperature = answere["temperature"]["tool0"]["target"];
        toolCurrentTemperature = answere["temperature"]["tool0"]["actual"];
      }
    }else{
      //print error
      displayError = true;
    }
  }else{
    //reset temperature
    bedTargetTemperature = 0;
    bedCurrentTemperature = 0;
    toolTargetTemperature = 0;
    toolCurrentTemperature = 0;
  }
  http.end();
}

//update Temperature and calc if heating is on
void updateTemperature(){
  getTemperature();

  //get if heeting is on
  isHeating[0] = bedCurrentTemperature < bedTargetTemperature;
  isHeating[1] = toolCurrentTemperature < toolTargetTemperature;
}

//get job percentage
void updateJob(){
  //get jobs
  if(http.begin(client, String(HOST) + "/api/job?apikey=" + String(APIKEY))){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){
        String payload = http.getString();
        DeserializationError error = deserializeJson(apiAnswere, payload);
        
        //check if answere is correct or skip
        if (error) {
          http.end();
          displayError = true;
          jobState = "";
          return;
        }
        
        JsonObject answere = apiAnswere.as<JsonObject>();
  
        //check if printing
        jobState = String(answere["state"]);

        if(jobState.equals("Printing")){
          //get progress in percent
          printProgress = int(answere["progress"]["completion"]);
        }
      }
    }else{
      //print error
      displayError = true;
    }
  }
  http.end();
}







/**********************************************************************************************
 * loop 
 **********************************************************************************************/
//send command to stop job
void sendStopCommand(){
  /*POST /api/job HTTP/1.1
  Host: example.com
  Content-Type: application/json
  X-Api-Key: abcdef...
  {
    "command": "start"
  }*/

  //get jobs
  if(http.begin(client, String(HOST) + "/api/job")){
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", String(APIKEY));
    String httpRequestData = "{\n\"command\": \"start\"}";           
    // Send HTTP POST request
    int httpCode = http.POST(httpRequestData);

    Serial.print("Send: ");
    Serial.println(httpCode);

    //process answere
    if (httpCode != 0) {
     
    }else{
      //print error
      displayError = true;
    }
  }
  http.end();
}





/**********************************************************************************************
 * loop 
 **********************************************************************************************/
//display wifi connection animation (dot flashing)
void wifiConnectingAnimation(){
  if((millis()-updateTimer) > BLINKTIMER){
    //make donts blink
    printTo7Segment(dotCode<<(showDot*8));
    showDot = !showDot;

    updateTimer = millis();
  }
}

//display status information
void updateDisplay(){
  //get data and display it
  if((millis()-updateTimer) > UPDATETIME){
    updateTemperature();
    updateJob();

    printToDisplay();

    updateTimer = millis();
  }
}

void loop() {
  Serial.println(isWificonnected);

  if(isWificonnected){
    updateDisplay();

    if(digitalRead(STOPPIN)){
      sendStopCommand();
    }
  }else{
    wifiConnectingAnimation();

    //process wifi manager and show waiting animation
    wifiManager.process();
  }
}






/**********************************************************************************************
 * wifimanager callback
 **********************************************************************************************/
void saveParamsCallback () {
  isWificonnected = 1;
  Serial.println(WiFi.localIP());
  cleartSegment();
}






/**********************************************************************************************
 * setup methods 
 **********************************************************************************************/
void setupSerial(){
  Serial.begin(115200);
  while (!Serial) {
    // wait for serial port to connect. Needed for native USB
  }
  Serial.println("start");
}

void setupPins(){
  pinMode(STOREPIN, OUTPUT);
  pinMode(SHIFTPIN, OUTPUT);
  pinMode(DATAPIN, OUTPUT);
  pinMode(STOPPIN, OUTPUT);
}

void setupDisplay(){
  printTo7Segment(0xFFFF);    //all on
}

void setupWifi(){
  WiFi.mode(WIFI_STA);
  //wifiManager.resetSettings();                                //reset Wifimanager => credentials will be deleted
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setBreakAfterConfig(true);
  wifiManager.setSaveParamsCallback(saveParamsCallback);
  wifiManager.setClass("invert"); // dark theme
  if(wifiManager.autoConnect("OctoPrintProgressDisplay")){
    isWificonnected = 1;
    Serial.println(WiFi.localIP());
  }
}

void setup() {
  setupSerial();
  setupPins();
  setupDisplay();
  setupWifi();
}