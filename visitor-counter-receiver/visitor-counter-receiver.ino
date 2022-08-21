#include <SPI.h>
#include <ezTime.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

#include "arduino_secrets.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
 
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET 4       // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define detectorPin_in 4
#define detectorPin_out 5

int countin = 0;
int countout = 0;

int val_in;
int val_out;

int in;
int out;
int num;
int MQTT_status = 0;

int sofa = 0;
int equipment = 0;
int desk = 0;
int blind = 0;

int total_device_num = 1; // change here for multi-device work
int device_num = 1;

const char* mqtt_server = "mqtt.cetools.org";

WiFiClient unoClient;
PubSubClient client(unoClient);

char msg[50];
Timezone GB;

uint8_t today;

//const char* ssid     = SECRET_SSID;
//const char* password = SECRET_PASS;
//const char* mqttuser = SECRET_MQTTUSER;
//const char* mqttpass = SECRET_MQTTPASS;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  setupOLED();
  showOLED();
  pinMode(detectorPin_in, INPUT);   
  pinMode(detectorPin_out, INPUT);  
  Serial.print("detectorPin succeed!");

  setupWifi();
  syncDate();
  // start MQTT server
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);

  today = weekday();

  Serial.println("Setup finished!");
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();
  val_in = digitalRead(detectorPin_in);
  val_out = digitalRead(detectorPin_out);

  if (today != weekday()){
    countin = 0;
    countout = 0;

    in = 0;
    out = 0;
    num = 0;
    today = weekday();   
  }

  if (val_in == 1){
    Serial.print("val_in: HIGH  ");
  } else {
    Serial.print("val_in: LOW  ");
  }
  if (val_out == 1){
    Serial.println("val_out: HIGH");
  } else {
    Serial.println("val_out: LOW");
  }
  
  if(val_in == 1 and val_out == 0){
    unsigned long Time = millis(); 
    int flag = 1;
    while(digitalRead(detectorPin_out) == 0){
      if (millis() - Time >= 500){
        flag = 0;
        break;
      }
    }
    if (flag == 1) {
      in = countin++;
    }
    delay(10);
  }
  
  if (val_in == 0 and val_out == 1){
    unsigned long Time = millis(); 
    int flag = 1;
    while(digitalRead(detectorPin_in) == 0){
      if (millis() - Time >= 500){
        flag = 0;
        break;
      }
    }
    if (flag == 1) {
      out = countout++;
    }
    delay(10);
  }
  
  if(MQTT_status == 0){
    num = in - out;
    if (num < 0) {
      countin = 0;
      countout = 0;
  
      in = 0;
      out = 0;
      num = 0;
    }
  }

  
  
  Serial.print("in_people: ");
  Serial.println(in);
  Serial.print("out_people: ");
  Serial.println(out);
  Serial.print("num: ");
  Serial.println(num);

  showOLED();
  publishMQTT();
  //client.loop();
}

void setupWifi(){
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupOLED(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initialize with the I2C addr 0x3C (128x64)
  delay(2000);
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  display.print("Visitor");
  display.setCursor(20, 40);
  display.print("Counter");
  display.display();
  delay(3000);
  Serial.println("setupOLED finished!");
}

void showOLED(){
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  if (num > 9) {
    display.print("Visitor:");
  } else {
    display.print("Visitor: ");
  }
  display.setTextSize(2);
  //display.setCursor(50, 15);
  display.print(num);

  display.setTextSize(1);
  display.setCursor(0, 30);
  display.print("Sofa: ");
  display.print(sofa);

  display.setTextSize(1);
  display.setCursor(70, 30);
  display.print("Equip: ");
  display.print(equipment);

  display.setTextSize(1);
  display.setCursor(0, 42);
  display.print("Desk: ");
  display.print(desk);

  display.setTextSize(1);
  display.setCursor(70, 42);
  display.print("Blind: ");
  display.print(blind);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("IN: ");
  display.print(in);

  display.setTextSize(1);
  display.setCursor(70, 55);
  display.print("OUT: ");
  display.print(out);

  display.display();
}

void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {    // while not (!) connected....
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ArduinoUnoWiFiRev2Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttuser, mqttpass)) {
      Serial.println("connected");
      // ... and subscribe to messages on broker
      client.subscribe("student/CASA0022/ucfnzc1/visitorNumber");
      client.subscribe("student/CASA0022/ucfnzc1/changeSignal");
      client.subscribe("student/CASA0022/ucfnzc1/Sofa");
      client.subscribe("student/CASA0022/ucfnzc1/Equipment");
      client.subscribe("student/CASA0022/ucfnzc1/Desk");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishMQTT() {
 // try to reconnect if not
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  String str1 = "student/CASA0022/ucfnzc1/in_";
  String str2 = String(device_num);
  String str = str1 + str2;

  snprintf (msg, 50, "%.0i", in);
  Serial.print("Publish message for in_number_device_" + String(device_num) + ": ");
  Serial.println(msg);
  client.publish(str.c_str(), msg);

  str1 = "student/CASA0022/ucfnzc1/out_";
  str2 = String(device_num);
  str = str1 + str2;

  snprintf (msg, 50, "%.0i", out);
  Serial.print("Publish message for out_number_device_" + String(device_num) + ": ");
  Serial.println(msg);
  client.publish(str.c_str(), msg); 
}


// Execute code when a message is recieved from the MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  if (strcmp(topic,"student/CASA0022/ucfnzc1/visitorNumber")==0){
    num = 0;
    for (int i = 0; i < length; i++) {
      num *= 10;
      num += int(char(payload[i]) - '0');  
    }
    MQTT_status = 1;
  }
  if (strcmp(topic,"student/CASA0022/ucfnzc1/changeSignal")==0){
    if (char(payload[0]) - '0' == 1){
      if (total_device_num > 1) {
        countout = 0;
        countin = int(num / total_device_num);
        int mod = num % total_device_num;
        if (device_num <= mod){
          countin += 1;
        }
      }
      else {
        countout = 0;
        countin = num;
      }
      in = countin;
      out = 0;
    }
  }
  if (strcmp(topic,"student/CASA0022/ucfnzc1/Sofa")==0){
    sofa = 0;
    for (int i = 0; i < length; i++) {
      sofa *= 10;
      sofa += int(char(payload[i]) - '0');  
    }
  }  
  if (strcmp(topic,"student/CASA0022/ucfnzc1/Equipment")==0){
    equipment = 0;
    for (int i = 0; i < length; i++) {
      equipment *= 10;
      equipment += int(char(payload[i]) - '0');  
    }
  }
  if (strcmp(topic,"student/CASA0022/ucfnzc1/Desk")==0){
    desk = 0;
    for (int i = 0; i < length; i++) {
      desk *= 10;
      desk += int(char(payload[i]) - '0');  
    }
  }
  blind = max(0, num - sofa - equipment - desk);
  Serial.println();
}
