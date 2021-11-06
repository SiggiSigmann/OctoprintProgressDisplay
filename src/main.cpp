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

//store rest answere
StaticJsonDocument<1200> doc;

//drawing to display
byte dot = B00000001;
byte e = B11101100;
byte r = B10001000;

//Bxxxxxxx1
// GFABEDC.
byte numbers[]= {B01111110, //0
                B00010010, //1
                B10111100, //2
                B10110110, //3
                B11010010, //4
                B11100110, //5
                B11101110, //6
                B00110010, //7
                B11111110, //8
                B11110110};//9

byte ring[]= {B00000100,
              B00001100,
              B01001100,
              B01101100,
              B01111100,
              B01111110};

//octoprint
const String apikey = APIKEY;
const String host = HOST;

//wifimanager
WiFiManager wifiManager;

//store if bed and hotend is heating
byte heating[2] = {0,0};

//store temperature
float bedTarget = 0;
float bedTemperature = 0;
float toolTarget = 0;
float toolTemperature = 0;

//clients
WiFiClient client;
HTTPClient http;

//timer
#define UPDATETIME 500
unsigned long updateTimer = 0;

//isWificonnected
boolean isWificonnected = 0;

//count for waiting till wifi animation
byte count = 0;
#define BLINKTIMER 100

//prototypes
void updateTemperature();
void updateJob();

//calback
void saveParamsCallback();

//7 Segemnts methods
void printTo7Segment(int vlaue);
void lowlineTo7Segment();
void errorTo7Segment();
int createnumber(int input);
void cleartSegment();

void setup() {
  //Serial.begin(115200);
  //Serial.println("start");

  //pins
  pinMode(STOREPIN, OUTPUT);
  pinMode(SHIFTPIN, OUTPUT);
  pinMode(DATAPIN, OUTPUT);

  //init 7 segments (all on)
  printTo7Segment( 0xFFFF );

  //connect to wifi
  WiFi.mode(WIFI_STA);
  //wifiManager.resetSettings();                                //reset Wifimanager => credentials will be deleted
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setBreakAfterConfig(true);
  wifiManager.setSaveParamsCallback(saveParamsCallback);
  wifiManager.setClass("invert"); // dark theme
  if(wifiManager.autoConnect("OctoPrintProgressDisplay")){
    isWificonnected = 1;
  }

  //write 0
  printTo7Segment(0);
  
  Serial.println(WiFi.localIP());
}

void loop() {
  if(isWificonnected){
    //get data and display it
    if((millis()-updateTimer) > UPDATETIME){
      updateTemperature();
      updateJob();

      updateTimer = millis();
    }

  }else{
    if((millis()-updateTimer) > BLINKTIMER){
      //make donts blink
      printTo7Segment(dot<<(count*8));
      count++;
      if(count > 1){
        count = 0;
      }

      updateTimer = millis();
    }

    //process wifi manager and show waiting animation
    wifiManager.process();
  }
  
}

//calback from Wifimanager
void saveParamsCallback () {
  isWificonnected = 1;
  cleartSegment();
}

//get Termperature from Octoprint via rest call
void updateTemperature(){
  //make an api call for temperature
  if(http.begin(client, host + "/api/printer?apikey=" + apikey)){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){

        //get payload
        String payload = http.getString();
        DeserializationError error = deserializeJson(doc, payload);
        
        //handel and sktip
        if (error) {
          http.end();
          errorTo7Segment();
          delay(100);
          return;
        }
        
        //pars json
        JsonObject root = doc.as<JsonObject>();
        bedTarget = root["temperature"]["bed"]["target"];
        bedTemperature = root["temperature"]["bed"]["actual"];
        toolTarget = root["temperature"]["tool0"]["target"];
        toolTemperature = root["temperature"]["tool0"]["actual"];
  
        //get if heeting is on
        heating[0] = bedTemperature<bedTarget;
        heating[1] = toolTemperature<toolTarget;
      }
        
    }else{
      //print error
      errorTo7Segment();
    }
  }else{
    //reset temperature
    bedTarget = 0;
    bedTemperature = 0;
    toolTarget = 0;
    toolTemperature = 0;
  }
  http.end();
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
        DeserializationError error = deserializeJson(doc, payload);
        
        //check if answere is correct or skip
        if (error) {
          http.end();
          errorTo7Segment();
          delay(100);
          return;
        }
        
        JsonObject root = doc.as<JsonObject>();
  
        //check if printing
        String state = root["state"];
        if(state.equals("Printing")){

          //get progress in percent
          int percentageOfPrint = int(root["progress"]["completion"]);
          
          //check if heating is required
          if((toolTarget==0) |( bedTarget==0) | ((toolTarget-toolTemperature)>2)){
            //create ring for heating
            int bednum = int((6/bedTarget)*bedTemperature);
            int toolnum = int((6/toolTarget)*toolTemperature);
            if((toolnum<1) |( toolnum > 7)){
              toolnum=1;
            }
            if((bednum<1) | (bednum > 7)){
              bednum=1;
            }
            
            //Serial.print(bednum);
            //Serial.println(toolnum);

            //display information ( heating progress)
            printTo7Segment(((ring[toolnum-1])<<8)+(ring[bednum-1]));
          }else{
            //display information ( print progress)
            printTo7Segment(createnumber(percentageOfPrint));
          }
        }else if(state.equals("Cancelling") or state.equals("Operational") ){
          //check if coolingdown
          if(((abs(toolTarget-toolTemperature))>2)| ((abs(bedTarget-bedTemperature))>2)){
            int bednum = int((6/60)*(bedTemperature));
            int toolnum = int((6/215)*toolTemperature);
            if(toolnum<1 | toolnum > 7){
              toolnum=1;
            }

            if(bednum<1 | bednum > 7){
              bednum=1;
            }
            
            //print information
            //Serial.print(bednum);
            //Serial.println(toolnum);
            printTo7Segment(((ring[toolnum-1])<<8)+(ring[bednum-1]));
          }else{
            //print idel status
            lowlineTo7Segment();
          }
        }else{
          //print error
          errorTo7Segment();
        }
      }
    }else{
      //print error
      errorTo7Segment();
    }
  }
  http.end();
}

//print value to 7 segment display
//value must resembel the encoding
void printTo7Segment(int vlaue){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, (vlaue & 0xFF) | heating[0]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, ((vlaue & 0xFF00)>>8)| heating[1]);
  digitalWrite(STOREPIN, HIGH);
}

//print "ER" to display
void errorTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, r| heating[0]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, e| heating[1]);
  digitalWrite(STOREPIN, HIGH);
}

//print "_ _" to diaply
void lowlineTo7Segment(){
  digitalWrite(STOREPIN, LOW);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| heating[0]);
  shiftOut(DATAPIN, SHIFTPIN, MSBFIRST, 0x4| heating[1]);
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
  byte leftDigit = (input / 10)%100;
  byte leftDigitEncoding = numbers[leftDigit]<<8;
  
  //process right digit
  byte rightDigit = input % 10;
  byte rightDigitEncoding = numbers[rightDigit];
  
  //clac value for shifting to display
  return leftDigitEncoding | rightDigitEncoding;
}
