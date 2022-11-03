#include "AiEsp32RotaryEncoder.h"
#include <Wire.h>
#include "DS1307.h"
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <StreamUtils.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define ROTARY_ENCODER_A_PIN 25
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 33
#define ROTARY_ENCODER_VCC_PIN 27

// #define ROTARY_ENCODER_STEPS 1
// #define ROTARY_ENCODER_STEPS 2
#define ROTARY_ENCODER_STEPS 4

#define servo 15

#define trig 4
#define echo 5

#define topicpub "v1/devices/me/telemetry/smartfisheryiot4"
#define subtopic "v1/devices/me/rpc/request/+"

const char* ssid = "Wokwi-GUEST";
const char* password =  "";
const char* mqttServer = "thingsboard.cloud";
const int mqttPort = 1883;
const char* clientID = "teamciot4_2";
const char* mqttUser = "smartfisherypakan";
const char* mqttPassword = "";


AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);
DS1307 jam;//define a object of DS1307 class
Servo serv;
LiquidCrystal_I2C lcd (0x27, 20, 4);

WiFiClient espClient;
PubSubClient client(espClient);

char msg[300];
unsigned long interval;
volatile int limits = 0;
volatile bool mode_display = true;

bool online = false;


bool alarm_en = false;
int sp_pakan = 10;
int distance;

int h1 = 15;
int m1 = 30;
int s1 = 0;

int h2 = 15;
int m2 = 31;
int s2 = 0;

int h3 = 15;
int m3 = 32;
int s3 = 0;

int old_level = 0;
int last_pos = 0;

bool servo_move_ok = true;

void load_settings()
{
  Serial.println("LOAD SETTINGS");
  const size_t CAPACITY = JSON_OBJECT_SIZE(255);
  StaticJsonDocument<CAPACITY> doc;
  EepromStream eepromStream(0, EEPROM.length());

  DeserializationError err = deserializeJson(doc, eepromStream);
  serializeJsonPretty(doc, Serial);

  if (err)
  {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.f_str());
  }
  else
  {
    h1 = doc["sch1"]["h1"];
    m1 = doc["sch1"]["m1"];
    s1 = doc["sch1"]["s1"];
    Serial.println("Waktu 1 : " + String(h1) + String (m1) + String(s1));
    h2 = doc["sch2"]["h2"];
    m2 = doc["sch2"]["m2"];
    s2 = doc["sch2"]["s2"];
    Serial.println("Waktu 2 : " + String(h2) + String (m2) + String(s2));
    h3 = doc["sch3"]["h3"];
    m3 = doc["sch3"]["m3"];
    s3 = doc["sch3"]["s3"];
    Serial.println("Waktu 3 : " + String(h3) + String (m3) + String(s3));
    sp_pakan = doc["pakan"]["sp_pakan"];
    Serial.println("SP_Pakan : " + String(sp_pakan));
  }


}

void save_settings()
{
  Serial.println("SAVE SETTINGS");
  const size_t CAPACITY = JSON_OBJECT_SIZE(255);
  StaticJsonDocument<CAPACITY> doc;
  JsonObject sch1  = doc.createNestedObject("sch1");
  JsonObject sch2  = doc.createNestedObject("sch2");
  JsonObject sch3  = doc.createNestedObject("sch3");
  JsonObject pakan = doc.createNestedObject("sp_pakan");
  sch1["h1"] = 17;
  sch1["m1"] = 30;
  sch1["s1"] = 30;
  sch2["h2"] = h2;
  sch2["m2"] = m2;
  sch2["s2"] = s2;
  sch3["h3"] = h3;
  sch3["m3"] = m3;
  sch3["s3"] = s3;
  pakan["sp_pakan"] = sp_pakan;

  EepromStream eepromStream(0, EEPROM.length());
  serializeJson(doc, eepromStream);
  eepromStream.flush();
  serializeJsonPretty(doc, Serial);


}

void rotary_onButtonClick()
{
  static unsigned long lastTimePressed = 0;
  //ignore multiple press in that time milliseconds
  if (millis() - lastTimePressed < 500)
  {
    return;
  }
  lastTimePressed = millis();
  Serial.print("button pressed ");
  Serial.print(millis());
  Serial.println(" milliseconds after restart");
  mode_display = !mode_display;
  load_settings();
  lcd.clear();
}

void IRAM_ATTR readEncoderISR()
{
  rotaryEncoder.readEncoder_ISR();
}

void rotary_loop()
{
  if (rotaryEncoder.encoderChanged())
  {
    Serial.print("Value: ");
    // Serial.println(rotaryEncoder.readEncoder());
    limits++;
    Serial.println(limits);
  }
  if (rotaryEncoder.isEncoderButtonClicked())
  {
    rotary_onButtonClick();
  }
}



void setup_wifi() {
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi.." + String(ssid));
  }
  Serial.println("Connected to the WiFi network");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(subtopic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    msg[i] = (char)payload[i];
  }
  do_action(msg);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect(clientID, mqttUser, mqttPassword )) {
      Serial.println("connected");
      client.subscribe(subtopic);
    }
    else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);

  if (online)
  {
    setup_wifi();
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
  }


  jam.begin();
  jam.fillByYMD(2022, 10, 26); //Jan 19,2013
  jam.fillByHMS(15, 30, 50); //15:28 30"
  jam.fillDayOfWeek(WED);//Saturday
  jam.setTime();//write time to the RTC chip

  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  bool circleValues = false;
  rotaryEncoder.setBoundaries(0, 1000, circleValues);
  //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(250);

  serv.attach(servo);
  serv.write(0);
  lcd.init();
  lcd.backlight();
}
void loop() {
  // printTime();

  if (WiFi.status() != WL_CONNECTED && online == true) {
    setup_wifi();
  }
  if (WiFi.status() == WL_CONNECTED && !client.connected() && online == true) {
    reconnect();
  }
  rotary_loop();
  jam.getTime();
  display_show(mode_display);

  distance = dist_measurement();
  distance = map(distance, 0, 300, 0, 100);

  if (millis() >= interval + 1000 && online == true)
  {
    interval = millis();
    kirim();
  }

  if (jam.hour == h1 && jam.minute == m1 && jam.second == s1 )
  {
    Serial.print(String(jam.hour) + ":" + String(jam.minute) + ":" + String(jam.second));
    Serial.println("");
    Serial.println("ALARM 1 ON");
    alarm_en = true;
    limits = 0;
    save_settings();
  }

  if (jam.hour == h2 && jam.minute == m2 && jam.second == s2 )
  {
    Serial.print(String(jam.hour) + ":" + String(jam.minute) + ":" + String(jam.second));
    Serial.println("");
    Serial.println("ALARM 2 ON");
    alarm_en = true;
    limits = 0;
    save_settings();
  }

  if (jam.hour == h3 && jam.minute == m3 && jam.second == s3 )
  {
    Serial.print(String(jam.hour) + ":" + String(jam.minute) + ":" + String(jam.second));
    Serial.println("");
    Serial.println("ALARM 3 ON");
    alarm_en = true;
    limits = 0;
  }

  if (alarm_en)
  {
    // serv.write(90);

    if (servo_move_ok)
    {
      servo_move(90);
      servo_move_ok = !servo_move_ok;
    }
    if (limits == sp_pakan)
    {
      // serv.write(0);
      servo_move(0);
      limits = 0;
      alarm_en = false;
      servo_move_ok = true;
      Serial.println("FEEDING FINISHED!");
    }
  }

  client.loop();
}

void display_show(bool mode)
{
  String waktu = " " + String(jam.hour) + ":" + String(jam.minute) + ":" + String(jam.second) + " ";
  String alarm_1_set = String(h1) + ":" + String(m1) + ":" + String(s1);
  int panjang = waktu.length();

  String alarm_2_set = String(h2) + ":" + String(m2) + ":" + String(s2);
  String alarm_3_set = String(h3) + ":" + String(m3) + ":" + String(s3);

  String level = String(distance) + " ";
  int panjang_dist = level.length();

  if (old_level != panjang_dist)
  {
    old_level = panjang_dist;
    lcd.setCursor(0, 2);
    lcd.print("                    ");
  }

  lcd.setCursor(((20 - panjang) / 2), 0);
  lcd.print(waktu);

  if (mode)
  {
    lcd.setCursor(0, 1);
    lcd.print("Alarm 1 : ");
    lcd.print(alarm_1_set);
    lcd.setCursor(0, 2);
    lcd.print("Alarm 2 : ");
    lcd.print(alarm_2_set);
    lcd.setCursor(0, 3);
    lcd.print("Alarm 3 : ");
    lcd.print(alarm_3_set);
  }
  else
  {
    lcd.setCursor(4, 1);
    lcd.print("Level Pakan");
    lcd.setCursor(((20 - panjang_dist) / 2), 2);
    // String old_level = level;
    lcd.print(level);
    lcd.setCursor(3, 3);
    lcd.print("Percent  (%)");
  }

}


int dist_measurement()
{
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  int time = pulseIn(echo, HIGH);
  float dist = (0.034 * time) / 2;

  return int(dist);
}

void servo_move(int angle)
{
  if (angle == 0)
  {
    for (int i = last_pos ; i >= angle ; i--)
    {
      serv.write(i);
      last_pos = i;
      delay(10);
    }

  }
  else
  {
    for (int i = 0; i <= angle ; i++)
    {
      serv.write(i);
      last_pos = i;
      delay(10);

    }
  }
}

void do_action(const char* miseji) {

  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, miseji);

  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.f_str());
  }
  else
  {
    Serial.println(miseji);
    String method = doc ["method"];
    if (method == "setValue") {
      Serial.print("Set Point Pakan : ");
      sp_pakan = doc["params"];
      Serial.println(sp_pakan);
      save_settings();
    }
    if (method == "getValue") {
      Serial.print("Servo Move : ");
      String parameter = doc["params"];
      if (parameter == "true")
      {
        Serial.println(" OPEN");
        servo_move(90);
      }
      else
      {
        Serial.println(" CLOSED");
        servo_move(90);
      }
    }
  }
}

void kirim() {
  const size_t CAPACITY = JSON_OBJECT_SIZE(2);
  StaticJsonDocument<CAPACITY> doc;
  doc["level_pakan"] = distance;
  // doc["humidity"] = humidity;

  char JSONmessageBuffer[200];
  serializeJson(doc, JSONmessageBuffer, sizeof(JSONmessageBuffer));
  Serial.println(JSONmessageBuffer);
  client.publish(topicpub, JSONmessageBuffer);
  Serial.println("Data Sent!");
}

/*Function: Display time on the serial monitor*/
// void printTime() {
//   jam.getTime();
//   Serial.print(jam.hour, DEC);
//   Serial.print(":");
//   Serial.print(jam.minute, DEC);
//   Serial.print(":");
//   Serial.print(jam.second, DEC);
//   Serial.print("	");
//   Serial.print(jam.month, DEC);
//   Serial.print("/");
//   Serial.print(jam.dayOfMonth, DEC);
//   Serial.print("/");
//   Serial.print(jam.year + 2000, DEC);
//   Serial.print(" ");
//   Serial.print(jam.dayOfMonth);
//   Serial.print(" *");
//   switch (jam.dayOfWeek) { // Friendly printout the weekday
//     case MON:
//       Serial.print("MON");
//       break;
//     case TUE:
//       Serial.print("TUE");
//       break;
//     case WED:
//       Serial.print("WED");
//       break;
//     case THU:
//       Serial.print("THU");
//       break;
//     case FRI:
//       Serial.print("FRI");
//       break;
//     case SAT:
//       Serial.print("SAT");
//       break;
//     case SUN:
//       Serial.print("SUN");
//       break;
//   }
//   Serial.println(" ");
// }