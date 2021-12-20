/* This code works with MAX30102 + 128x32 OLED i2c + Buzzer and Arduino UNO
   It's displays the Average BPM on the screen, with an animation and a buzzer sound
   everytime a heart pulse is detected
   It's a modified version of the HeartRate library example
   Refer to www.surtrtech.com for more details or SurtrTech YouTube channel
*/

#include <Adafruit_GFX.h>       //OLED libraries
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "MAX30105.h"           //MAX3010x library
#include "heartRate.h"          //Heart rate calculating algorithm
#include "spo2_algorithm.h"     //SpO2 calculating algorithm

MAX30105 particleSensor;

const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#ifndef STASSID
#define STASSID "G09-AP"
#define STAPSK  "12345678"
#endif

//OLED
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, i2c Addr, Reset share with 8266 reset);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

static const unsigned char PROGMEM logo2_bmp[] =
{ 0x03, 0xC0, 0xF0, 0x06, 0x71, 0x8C, 0x0C, 0x1B, 0x06, 0x18, 0x0E, 0x02, 0x10, 0x0C, 0x03, 0x10,    //Logo2 and Logo3 are two bmp pictures that display on the OLED if called
  0x04, 0x01, 0x10, 0x04, 0x01, 0x10, 0x40, 0x01, 0x10, 0x40, 0x01, 0x10, 0xC0, 0x03, 0x08, 0x88,
  0x02, 0x08, 0xB8, 0x04, 0xFF, 0x37, 0x08, 0x01, 0x30, 0x18, 0x01, 0x90, 0x30, 0x00, 0xC0, 0x60,
  0x00, 0x60, 0xC0, 0x00, 0x31, 0x80, 0x00, 0x1B, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x04, 0x00,
};

static const unsigned char PROGMEM logo3_bmp[] =
{ 0x01, 0xF0, 0x0F, 0x80, 0x06, 0x1C, 0x38, 0x60, 0x18, 0x06, 0x60, 0x18, 0x10, 0x01, 0x80, 0x08,
  0x20, 0x01, 0x80, 0x04, 0x40, 0x00, 0x00, 0x02, 0x40, 0x00, 0x00, 0x02, 0xC0, 0x00, 0x08, 0x03,
  0x80, 0x00, 0x08, 0x01, 0x80, 0x00, 0x18, 0x01, 0x80, 0x00, 0x1C, 0x01, 0x80, 0x00, 0x14, 0x00,
  0x80, 0x00, 0x14, 0x00, 0x80, 0x00, 0x14, 0x00, 0x40, 0x10, 0x12, 0x00, 0x40, 0x10, 0x12, 0x00,
  0x7E, 0x1F, 0x23, 0xFE, 0x03, 0x31, 0xA0, 0x04, 0x01, 0xA0, 0xA0, 0x0C, 0x00, 0xA0, 0xA0, 0x08,
  0x00, 0x60, 0xE0, 0x10, 0x00, 0x20, 0x60, 0x20, 0x06, 0x00, 0x40, 0x60, 0x03, 0x00, 0x40, 0xC0,
  0x01, 0x80, 0x01, 0x80, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x30, 0x0C, 0x00,
  0x00, 0x08, 0x10, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x03, 0xC0, 0x00, 0x00, 0x01, 0x80, 0x00
};

const char* ssid = STASSID;
const char* password = STAPSK;
const char* apiKey = "EY7JA5FN9AFDDTO2";
const char* resource = "/update?api_key=";
const char* serverUrl = "api.thingspeak.com";
//dOut ledUsb(16, 1); //-- set this pin is activeLow

ESP8266WebServer server(80);

void handleRoot() 
{
  //ledUsb.on();
  String message = "\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) 
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
//    if (server.argName(i)=="led" && server.arg(i)=="on") ledUsb.on();
//    else if (server.argName(i)=="led" && server.arg(i)=="off") ledUsb.off();
  }
  server.send(200, "text/plain", message);
  //ledUsb.off();
}

void handleNotFound() {
  //ledUsb.on();
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) 
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //ledUsb.off();
}


void setup() {
//  ESP.wdtDisable();
//  ESP.wdtFeed();
  Serial.begin(115200);
//  for(int i=0; i<10; i++)
//  {
//    ledUsb.toggle();
//    delay(100);    
//  }
  Wire.begin(2,0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("ESP8266 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);

  server.on("/inline", []() {
    server.send(200, "text/plain", "this works as well");
  });

  server.on("/gif", []() {
    static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = millis() % 256;
    gif_colored[17] = millis() % 256;
    gif_colored[18] = millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
  });

  server.onNotFound(handleNotFound);

  /////////////////////////////////////////////////////////
  // Hook examples

  server.addHook([](const String & method, const String & url, WiFiClient * client, ESP8266WebServer::ContentTypeFunction contentType) {
    (void)method;      // GET, PUT, ...
    (void)url;         // example: /root/myfile.html
    (void)client;      // the webserver tcp client connection
    (void)contentType; // contentType(".html") => "text/html"
    Serial.printf("A useless web hook has passed\n");
    Serial.printf("(this hook is in 0x%08x area (401x=IRAM 402x=FLASH))\n", esp_get_program_counter());
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient*, ESP8266WebServer::ContentTypeFunction) {
    if (url.startsWith("/fail")) {
      Serial.printf("An always failing web hook has been triggered\n");
      return ESP8266WebServer::CLIENT_MUST_STOP;
    }
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  server.addHook([](const String&, const String & url, WiFiClient * client, ESP8266WebServer::ContentTypeFunction) {
    if (url.startsWith("/dump")) {
      Serial.printf("The dumper web hook is on the run\n");

      // Here the request is not interpreted, so we cannot for sure
      // swallow the exact amount matching the full request+content,
      // hence the tcp connection cannot be handled anymore by the
      // webserver.
#ifdef STREAMSEND_API
      // we are lucky
      client->sendAll(Serial, 500);
#else
      auto last = millis();
      while ((millis() - last) < 500) {
        char buf[32];
        size_t len = client->read((uint8_t*)buf, sizeof(buf));
        if (len > 0) {
          Serial.printf("(<%d> chars)", (int)len);
          Serial.write(buf, len);
          last = millis();
        }
      }
#endif
      // Two choices: return MUST STOP and webserver will close it
      //                       (we already have the example with '/fail' hook)
      // or                  IS GIVEN and webserver will forget it
      // trying with IS GIVEN and storing it on a dumb WiFiClient.
      // check the client connection: it should not immediately be closed
      // (make another '/dump' one to close the first)
      Serial.printf("\nTelling server to forget this connection\n");
      static WiFiClient forgetme = *client; // stop previous one if present and transfer client refcounter
      return ESP8266WebServer::CLIENT_IS_GIVEN;
    }
    return ESP8266WebServer::CLIENT_REQUEST_CAN_CONTINUE;
  });

  // Hook examples
  /////////////////////////////////////////////////////////

  server.begin();
  Serial.println("HTTP server started");

  // Initialize sensor
  particleSensor.begin(Wire, I2C_SPEED_FAST); //Use default I2C port, 400kHz speed
  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(1); //Turn off Green LED

  //OLED Setup
//  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
//    Serial.println(F("SSD1306 allocation failed"));
//    for (;;);
//  }

  //OLED diplay 1st line
//  display.clearDisplay();
//  display.setTextSize(1);
//  display.setTextColor(WHITE);
//  display.setCursor(0, 10);
//  display.println("IERG4230 IoT MAX30102"); // Display static text
//  display.display();
  delay(500);
}

void loop() {
  //ESP.wdtFeed();
  server.handleClient();
  MDNS.update();
  long irValue = particleSensor.getIR();    //Reading the IR value it will permit us to know if there's a finger on the sensor or not
  
  //Also detecting a heartbeat
  if (irValue > 7000)
  {                                         //If a finger is detected
    //display.clearDisplay();                       //Clear the display
    //OLED diplay 1st line
//    display.setCursor(0, 10);
//    display.println("IERG4230 IoT MAX30102"); // Display static text
    //OLED diplay 3rd line
//    display.setCursor(50, 30);
//    display.println("BPM:"); // Display static text
//    display.setCursor(74, 30);
//    display.println(beatAvg);
    //OLED diplay 5th line
    //display.setCursor(50, 50);
//    display.println("SpO2:"); // Display static text
//    display.setCursor(80, 50);
//    display.println(beatAvg);
    
    //display.drawBitmap(5, 30, logo2_bmp, 24, 21, WHITE);       //Draw the first bmp picture (little heart)
//    display.setTextSize(2);                                 //Near it display the average BPM you can display the BPM if you want
//    display.setTextColor(WHITE);
//    display.setCursor(50, 0);
//    display.println("BPM");
//    display.setCursor(50, 18);
//    display.println(beatAvg);
    //display.display();

    if (checkForBeat(irValue) == true)                        //If a heart beat is detected
    {
//        display.clearDisplay();     //Clear the display
//        display.setCursor(0, 10);   //OLED diplay 1st line
//        display.println("IERG4230 IoT MAX30102"); // Display static text
//        display.setCursor(50, 30);   //OLED diplay 3rd line
//        display.println("BPM:");    // Display static text
//        display.setCursor(74, 30);
//        display.println(beatAvg);
        //OLED diplay 5th line
        //display.setCursor(50, 50);
//        display.println("SpO2:");   // Display static text
//        display.setCursor(80, 50);
//        display.println(beatAvg);
//        display.drawBitmap(0, 25, logo3_bmp, 32, 32, WHITE);    //Draw the second picture (bigger heart)
//        display.display();
//      tone(3, 1000);                                       //And tone the buzzer for a 100ms you can reduce it it will be better
//      delay(100);
//      noTone(3);                                          //Deactivate the buzzer to have the effect of a "bip"
      //We sensed a beat!
//      Serial.print("Time=");
//      Serial.println(millis());
      long delta = millis() - lastBeat;                   //Measure duration between two beats
      lastBeat = millis();

      beatsPerMinute = 60 / (delta / 1000.0);             //Calculating the BPM

      if (beatsPerMinute < 255 && beatsPerMinute > 20)    //To calculate the average we strore some values (4) then do some math to calculate the average
      {
        rates[rateSpot++] = (byte)beatsPerMinute;         //Store this reading in the array
        rateSpot %= RATE_SIZE; //Wrap variable

        //Take average of readings
        beatAvg = 0;
        for (byte x = 0 ; x < RATE_SIZE ; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
      WiFiClient client;
      if (client.connect(serverUrl, 80)) {
        Serial.println("connected"); 
       }else {
        Serial.println("connection failed");
        return; 
       }
       Serial.print("Request resource: ");
       Serial.println(resource);
//       client.print(String("GET ") + resource + apiKey + "&field1=" + 
//       beatsPerMinute + "&field2=" + beatAvg);
       client.print(String("GET ") + resource + apiKey + "&field1=" + 
       beatsPerMinute + "&field2=" + beatAvg + " HTTP/1.1\r\n" + "Host: " + 
       serverUrl + "\r\n" + "Connection: close\r\n\r\n");

       int timeout = 3 * 10;
       while (!!!client.available() && (timeout-- > 0)) {
        delay(100); 
       }

       if (!client.available()) {
        Serial.println("No response, going back to sleep"); 
       }
       while(client.available()) {
        Serial.write(client.read());
       }
       Serial.println("\nclosing connection");
       client.stop();
    }

    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.println(beatAvg);
  }
  
  if (irValue < 7000)
  { //If no finger is detected it inform the user and put the average BPM to 0 or it will be stored for the next measure
    beatAvg = 0;
//    display.clearDisplay();   //Clear the display
//    display.setCursor(0, 10);
//    display.println("IERG4230 IoT MAX30102"); // Display static text
//
//    display.setCursor(0, 30);
//    display.println("Please place your finger!"); // Display static text
//    display.display();
    
//    display.clearDisplay();
//    display.setTextSize(1);
//    display.setTextColor(WHITE);
//    display.setCursor(30, 5);
//    display.println("Please Place ");
//    display.setCursor(30, 15);
//    display.println("your finger ");
//    display.display();
//    noTone(3);
  }
}
