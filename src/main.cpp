#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

//connection and tcp stuff
ESP8266WiFiMulti WiFiMulti;
StaticJsonDocument<1200> doc;

//connection to shiftregister
int shiftPin = 13;
int storePin = 15;
int dataPin = 2;

//drawing to display
int dot = B00000001;
int e = B11101100;
int r = B10001000;
//Bxxxxxxx1
// GFABEDC.
int numbers[]= {B01111110, //0
                B00010010, //1
                B10111100, //2
                B10110110, //3
                B11010010, //4
                B11100110, //5
                B11101110, //6
                B00110010, //7
                B11111110, //8
                B11110110};//9

int ring[]= {B00000100,
              B00001100,
              B01001100,
              B01101100,
              B01111100,
              B01111110};

//octoprint
const String apikey = APIKEY;
const String host = HOST;

//prototypes
void printTo7Segment(int vlaue);
void lowlineTo7Segment();
void errorTo7Segment();
int createnumber(int input);


void setup() {
  //pins
  pinMode(storePin, OUTPUT);
  pinMode(shiftPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(12, INPUT);
  pinMode(14, OUTPUT);
  Serial.begin(115200);
  Serial.println("start");

  //init 7 segments
  printTo7Segment( 0xFFFF );

  //connect to wifi (dots of the displays will blink)
  WiFi.begin(SSID, PASSWORD);
  byte count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    printTo7Segment(dot<<(count*8));
    
    count++;
    if(count > 1){
      count = 0;
    }
    delay(100);
  }

  //write 0
  printTo7Segment(0);
  
  Serial.println(WiFi.localIP());
}

int heating[2] = {0,0};

void loop() {
  float bedTarget = 0;
  float bedTemperature = 0;
  float toolTarget = 0;
  float toolTemperature = 0;
  
  //create clients
  WiFiClient client;
  HTTPClient http;
  
  //make an api call for temperature
  if(http.begin(client, host + "/api/printer?apikey=" + apikey)){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){

        //get payload
        String payload = http.getString();
        DeserializationError error = deserializeJson(doc, payload);
        
        //handel error and retry
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
    http.end();
  }
  
  //get jobs
  if(http.begin(client, host + "/api/job?apikey=" + apikey)){

    //process answere
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK){
        String payload = http.getString();
        DeserializationError error = deserializeJson(doc, payload);
        
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
            
            Serial.print(bednum);
            Serial.println(toolnum);

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
            Serial.print(bednum);
            Serial.println(toolnum);
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
    http.end();
  }
  delay(500);
}

//print value to 7 segment display
void printTo7Segment(int vlaue){
  digitalWrite(storePin, LOW);
  shiftOut(dataPin, shiftPin, MSBFIRST, (vlaue & 0xFF) | heating[0]);
  shiftOut(dataPin, shiftPin, MSBFIRST, ((vlaue & 0xFF00)>>8)| heating[1]);
  digitalWrite(storePin, HIGH);
}

//print "ER" to display
void errorTo7Segment(){
  digitalWrite(storePin, LOW);
  shiftOut(dataPin, shiftPin, MSBFIRST, r| heating[0]);
  shiftOut(dataPin, shiftPin, MSBFIRST, e| heating[1]);
  digitalWrite(storePin, HIGH);
}

//print "__" to diaply
void lowlineTo7Segment(){
  digitalWrite(storePin, LOW);
  shiftOut(dataPin, shiftPin, MSBFIRST, 0x4| heating[0]);
  shiftOut(dataPin, shiftPin, MSBFIRST, 0x4| heating[1]);
  digitalWrite(storePin, HIGH);
}

//create number to print
int createnumber(int input){
  int zehn = (input / 10)%100;
  int zehnZahl = numbers[zehn]<<8;
  
  int einz = input % 10;
  int einzZahl = numbers[einz];
  
  return zehnZahl | einzZahl;
}
