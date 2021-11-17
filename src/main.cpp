#include <Arduino.h>

//wifi
#include <ESP8266WiFi.h>

//wifimanager
#include <WiFiManager.h>

//Rest client
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

//SSID PWD OctoprintIP KEY
#include "secrets.h"

//connection to shiftregister
#define SHIFTPIN 13
#define STOREPIN 15
#define DATAPIN 2

#define DEBUG

//store rest answere
StaticJsonDocument<1200> apiAnswere;

//drawing to display
byte dotCode = B00000001;
byte eCode = B11101100;
byte rCode = B10001000;

//Bxxxxxxx0
// GFABEDC.
byte numbersCodes[]= {B01111110, //0
                      B00010010, //1
                      B10111100, //2
                      B10110110, //3
                      B11010010, //4
                      B11100110, //5
                      B11101110, //6
                      B00110010, //7
                      B11111110, //8
                      B11110110};//9

byte ringCodes[]=  {B00000100,
                    B00001100,
                    B01001100,
                    B01101100,
                    B01111100,
                    B01111110};

#define BED 0
#define HOTEND 1

//octoprint
const String apikey = APIKEY;
const String host = HOST;

//wifimanager
WiFiManager wifiManager;

//clients
WiFiClient client;
HTTPClient http;

//timer
#define UPDATETIME 500
#define BLINKTIMER 100
unsigned long updateTimer = 0;

//isWificonnected
boolean isWificonnected = 0;

//count for waiting till wifi animation
byte count = 0;

//printing information
boolean isError = false;
String jobState = "";
int percentageOfPrint = 0;
byte heating[2] = {0,0}; //store if bed and hotend is heating
float bedTargetTemperature = 0;
float bedIsTemperature = 0;
float toolTargetTemperature = 0;
float toolIsTemperature = 0;






/**********************************************************************************************
 * 7 segment display
 **********************************************************************************************/
//value must resembel the encoding
void printTo7Segment(int vlaue){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, (vlaue & 0xFF) | heating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, ((vlaue & 0xFF00)>>8)| heating[HOTEND]);
  digitalWrite(STOREPIN, HIGH);
}

//print "ER" to display
void errorTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, rCode| heating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, eCode| heating[HOTEND]);
  digitalWrite(STOREPIN, HIGH);
}

//print "_ _" to diaply
void lowlineTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| heating[BED]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| heating[HOTEND]);
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
  int leftDigit = (input / 10)%100;
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

void printHeatingProgress(){
  int bedProgress = int((6/bedTargetTemperature)*(bedIsTemperature));
  int toolProgress = int((6/toolTargetTemperature)*toolIsTemperature);
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

void printToDisplay(){
  if(isError){
    errorTo7Segment();
  }else{
    if(jobState.equals("Printing")){
      if((toolTargetTemperature==0) || (bedTargetTemperature==0) || ((toolTargetTemperature-toolIsTemperature)>2)){
        //printer heats up
        printHeatingProgress();
      }else{
        //display information (print progress)
        printTo7Segment(createnumber(percentageOfPrint));
      }
    }else if(jobState.equals("Cancelling") or jobState.equals("Operational") ){
      if(((abs(toolTargetTemperature-toolIsTemperature))>2)| ((abs(bedTargetTemperature-bedIsTemperature))>2)){
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
  if(http.begin(client, host + "/api/printer?apikey=" + apikey)){

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
          isError = true;
          return;
        }
        
        //pars json
        JsonObject root = apiAnswere.as<JsonObject>();
        bedTargetTemperature = root["temperature"]["bed"]["target"];
        bedIsTemperature = root["temperature"]["bed"]["actual"];
        toolTargetTemperature = root["temperature"]["tool0"]["target"];
        toolIsTemperature = root["temperature"]["tool0"]["actual"];
      }
    }else{
      //print error
      isError = true;
    }
  }else{
    //reset temperature
    bedTargetTemperature = 0;
    bedIsTemperature = 0;
    toolTargetTemperature = 0;
    toolIsTemperature = 0;
  }
  http.end();
}

void updateTemperature(){
  getTemperature();

  //get if heeting is on
  heating[0] = bedIsTemperature<bedTargetTemperature;
  heating[1] = toolIsTemperature<toolTargetTemperature;
}

//get job percentage
void updateJob(){
  //get jobs
  if(http.begin(client, host + "/api/job?apikey=" + apikey)){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){
        String payload = http.getString();
        DeserializationError error = deserializeJson(apiAnswere, payload);
        
        //check if answere is correct or skip
        if (error) {
          http.end();
          isError = true;
          jobState = "";
          return;
        }
        
        JsonObject root = apiAnswere.as<JsonObject>();
  
        //check if printing
        jobState = String(root["state"]);

        if(jobState.equals("Printing")){
          //get progress in percent
          percentageOfPrint = int(root["progress"]["completion"]);
        }
      }
    }else{
      //print error
      isError = true;
    }
  }
  http.end();
}







/**********************************************************************************************
 * loop 
 **********************************************************************************************/
void wifiConnectingAnimation(){
  if((millis()-updateTimer) > BLINKTIMER){
    //make donts blink
    printTo7Segment(dotCode<<(count*8));
    count++;
    if(count > 1){
      count = 0;
    }

    updateTimer = millis();
  }
}

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