//Deklarasi Library
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <CTBot.h>
#include <RTClib.h>

//Deklarasi variabel datetime, dan data message
char dataHari[7][12] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"}; 
int tanggal, bulan, tahun; String hari, waktu; 

//Deklarasi variabel yang berkaitan dengan sensor
const float GAMMA = 0.7; const float RL10 = 50;
int analogValue; float voltage, resistance, temp, ntu;
String statusSuhu, statusKekeruhan;

//Deklarasi variabel yang berkaitan dengan bot telegram
String sendMsg, msg1, msg2; bool viewTombol;

//Inisialisasi variabel bot telegram
#define BOTtoken "5799908359:AAGhASHV0A9JYhKQ8nxEC75p47IktiN6fww"
#define INrespYes1 "INrespYes1"
#define INrespYes2 "INrespYes2"
#define INrespNo1 "INrespNo1"
#define INrespYes3 "INrespYes3"
#define INrespYes4 "INrespYes4"
#define INrespNo2 "INrespNo2"
#define KODEBOT "FISHERY2022"

//Deklarasi pin device
#define DHT_PIN 27 //Pin DHT
#define DHTTYPE DHT22
#define HEATER_PIN 2 //Pin LED Kuning Sebagai Heater
#define COOLER_PIN 4 //Pin LED Hijau Sebagai Cooler
#define LDR_PIN 36 //Pin LDR Sebagai Kekeruhan
#define DANGER_INDICATOR_PIN 18 //Pin LED Merah

//inisialisasi konstruktor
RTC_DS3231 rtc; CTBot myBot; DHT dhtSensor(DHT_PIN, DHTTYPE);
CTBotReplyKeyboard myKbd, submainKbd, sub1Kbd, sub2Kbd;
CTBotInlineKeyboard In1Kbd1, In1Kbd2, In2Kbd1, In2Kbd2, InNULL;

//Inisialisasi variabel thingsboard
#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define THINGSBOARD_SERVER "thingsboard.cloud"
#define CLIENT_ID "teamciot4_1"
#define USERNAME_DEVICE "smartfisheryair"
#define TOPIC "v1/devices/me/rpc/request/+";
// #define TOPIC "v1/devices/me/attributes/smartfisheryiot4";
#define SERIAL_DEBUG_BAUD 115200
WiFiClient wifiClient;
PubSubClient client(wifiClient);
StaticJsonDocument<256> Data;
int status = WL_IDLE_STATUS;
uint8_t newMACAddress[] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0x66};
char Payload[128], msgPayload[128];

void InitWiFi() {
  Serial.print("Menyambungkan ke: ");
  Serial.print(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println();
  Serial.print("Terhubung ke Local IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
      }
      Serial.println("\nBerhasil tersambung ke Access Point..");
    }
    if (client.connect(CLIENT_ID, USERNAME_DEVICE, "")) {
      Serial.println("Menyambungkan ke Node ThingsBoard ...[SUKSES]\n");
      client.subscribe(TOPIC);
    } 
  }
}

void connectBot() {
  myBot.setTelegramToken(BOTtoken);
  myBot.wifiConnect(WIFI_SSID, WIFI_PASSWORD); 
  myBot.setMaxConnectionRetries(5);
  Serial.println("Mencoba menghubungkan ke: fisheryiot_bot"); 

  if(myBot.testConnection()){
    Serial.println("Bot telegram berhasil tersambung dan sedang mengirim data...."); 
  } else{ 
    Serial.print("Bot telegram gagal tersambung\nMenyambungkan kembali"); 
    while (!myBot.testConnection()){ 
      Serial.print("."); delay(500);
    }
    Serial.println();
  }
}

void RTCinit() {
  rtc.begin(); //Memulai komunikasi serial dengan RTC
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //Pengaturan DateTime
  rtc.adjust(DateTime(2022, 10, 29, 9, 48, 0)); //TAHUN, BULAN, TANGGAL, JAM, MENIT, DETIK --> Untuk Kalibrasi Waktu
}

void DTnow() {
  DateTime now = rtc.now(); //Membuat objek baru: now untuk menampung method RTC
  hari = dataHari[now.dayOfTheWeek()]; //Hari
  tanggal = now.day(), DEC; bulan = now.month(), DEC; tahun = now.year(), DEC; //DD-MM-YYYY
  waktu = String() + hari + ", " + tanggal + "-" + bulan + "-" + tahun; //waktu
}

//Subscribe Switch Control
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(TOPIC);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    msgPayload[i] = char(payload[i]);
  } do_action(msgPayload);
}

void do_action(const char* dataTopic) {
  DeserializationError error = deserializeJson(Data, dataTopic);
  if (error) {
    Serial.print(F("Gagal mendapatkan data Topic dengan kode: "));
    Serial.println(error.f_str());
  }
  else{
    String Control1 = Data ["HeaterControl"];
    if (Control1 == "true") {
      Serial.println("Status Heater: Menyala !!");
      digitalWrite(HEATER_PIN, HIGH); digitalWrite(COOLER_PIN, LOW); delay(10000);
      Data["Heater"] = "1";
      kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
    } else if (Control1 == "false") {
      Serial.println("Status Heater: Mati !!");
      digitalWrite(HEATER_PIN, LOW);
      Data["Heater"] = "0";
      kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
    }
    String Control2 = Data ["CoolerControl"];
    if (Control2 == "true") {
      Serial.println("Status Heater: Menyala !!");
      digitalWrite(COOLER_PIN, HIGH); digitalWrite(HEATER_PIN, LOW); delay(10000);
      Data["Cooler"] = "1";
      kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
    } else if (Control2 == "false") {
      Serial.println("Status Heater: Mati !!");
      digitalWrite(COOLER_PIN, LOW);
      Data["Cooler"] = "0";
      kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
    }
  }
}

//Perintah untuk mengirimkan data ke thingsboard dalam bentuk JSON
void kirimDataThingsBoard(){
  serializeJson(Data, Payload);
  client.publish("v1/devices/me/telemetry/smartfisheryiot4", Payload);
  client.publish("v1/devices/me/attributes/smartfisheryiot4", Payload);
}

//Bagian yang mengatur suhu dan kekeruhan air
void SuhuKekeruhan() {
  temp =  dhtSensor.readTemperature();
  if(isnan(temp)){
    Serial.println("Gagal membaca data sensor!!"); return;
  }
  analogValue = analogRead(LDR_PIN);
  voltage = analogValue * 5 / 4095.0;
  resistance = 2000 * voltage / (1 - voltage / 5);
  ntu = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));

  //Batasan Suhu & Kekeruhan Air
  if(temp > 100){ temp = '100'; } 
  if (ntu < 0){ ntu = '0'; }
  else if (ntu > 900){ ntu = '900'; }

  Data["Suhu Air"] = String(temp);
  Data["Kekeruhan Air"] = String(ntu);
}

//Bagian yang mengatur indikator suhu dan kekeruhan air
void Indikator() {
  DTnow(); //Memanggil method DTnow
  SuhuKekeruhan(); //Memanggil method SuhuKekeruhan

  //Indikator & Status Suhu Air
  if(temp < 4){
    statusSuhu = "Sangat Dingin";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  } else if(temp >= 4 && temp <= 15){
    statusSuhu = "Dingin";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  } else if(temp > 15 && temp < 23){
    statusSuhu = "Lumayan Dingin";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(temp >= 23 && temp <= 32){
    statusSuhu = "Normal";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(temp > 32 && temp <= 37){
    statusSuhu = "Hangat";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(temp > 37 && temp <= 60){
    statusSuhu = "Panas";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  } else if(temp > 60){
    statusSuhu = "Sangat Panas";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  } else {
    statusSuhu = "Sangat Tidak Aman";
    Serial.println("Suhu air pada kolam saat ini : " + String(temp) + "°C\nStatus Suhu Air: " + String(statusSuhu) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  }

  //Indikator & Status Kekeruhan Air
  if(ntu >= 0 && ntu < 15){
    statusKekeruhan = "Tidak Keruh";
    Serial.println("Kekeruhan air pada kolam saat ini : " + String(ntu) + "\nStatus Kekeruhan Air: " + String(statusKekeruhan) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(ntu >= 15 && ntu < 35){
    statusKekeruhan = "Lumayan Keruh";
    Serial.println("Kekeruhan air pada kolam saat ini : " + String(ntu) + "\nStatus Kekeruhan Air: " + String(statusKekeruhan) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(ntu >= 35 && ntu <= 50){
    statusKekeruhan = "Keruh";
    Serial.println("Kekeruhan air pada kolam saat ini : " + String(ntu) + "\nStatus Kekeruhan Air: " + String(statusKekeruhan) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, LOW);
  } else if(ntu > 50 && ntu <= 100){
    statusKekeruhan = "Sangat Keruh";
    Serial.println("Kekeruhan air pada kolam saat ini : " + String(ntu) + "\nStatus Kekeruhan Air: " + String(statusKekeruhan) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  } else {
    statusKekeruhan = "Sangat Tidak Aman";
    Serial.println("Kekeruhan air pada kolam saat ini : " + String(ntu) + "\nStatus Kekeruhan Air: " + String(statusKekeruhan) + "\nWaktu saat ini : " + String(waktu) + "\n");
    digitalWrite(DANGER_INDICATOR_PIN, HIGH);
  }
}

//Bagian yang mengatur visualisasi tombol di bot telegram
void ButtonBot() {
  //Button Menu Utama
  myKbd.addButton("🌦️ Monitoring suhu air kolam");
  myKbd.addButton("🚰 Monitoring kekeruhan air kolam");
  myKbd.addRow(); myKbd.addButton("♨️ Pengaturan suhu air kolam");
  sub1Kbd.enableResize();

  //Button Respon Menu Pengaturan air kolam
  submainKbd.addButton("⛲ Cooler air"); submainKbd.addButton("⛲ Heater air");
  submainKbd.addRow(); submainKbd.addButton("⬅️ Kembali");
  submainKbd.enableResize();
  
  //Button Respon Sub Menu Cooler
  sub1Kbd.addButton("1️⃣ Cooler ON"); sub1Kbd.addButton("2️⃣ Cooler OFF");
  sub1Kbd.addRow(); sub1Kbd.addButton("↩️ Kembali");
  sub1Kbd.enableResize();

  //Button Respon Sub Menu Heater
  sub2Kbd.addButton("1️⃣ Heater ON"); sub2Kbd.addButton("2️⃣ Heater OFF");
  sub2Kbd.addRow(); sub2Kbd.addButton("↩️ Kembali");
  sub2Kbd.enableResize();

  viewTombol = false; //Tombol -> default : hidden

  //[Sub Menu 1] : Inline Button Respon Sub Menu Cooler ON
  In1Kbd1.addButton("✅ Ya", INrespYes1, CTBotKeyboardButtonQuery);
  In1Kbd1.addButton("❌ Tidak", INrespNo1, CTBotKeyboardButtonQuery);
  //Inline Button Respon Sub Menu Cooler OFF
  In1Kbd2.addButton("✅ Ya", INrespYes2, CTBotKeyboardButtonQuery);
  In1Kbd2.addButton("❌ Tidak", INrespNo1, CTBotKeyboardButtonQuery);

  //[Sub Menu 2] : Inline Button Respon Sub Menu Heater ON
  In2Kbd1.addButton("✅ Ya", INrespYes3, CTBotKeyboardButtonQuery);
  In2Kbd1.addButton("❌ Tidak", INrespNo2, CTBotKeyboardButtonQuery);
  //Inline Button Respon Sub Menu Heater OFF
  In2Kbd2.addButton("✅ Ya", INrespYes4, CTBotKeyboardButtonQuery);
  In2Kbd2.addButton("❌ Tidak", INrespNo2, CTBotKeyboardButtonQuery);
}

//Bagian yang mengatur dan mengelola perintah yang diberikan oleh user melalui telegram
void botTelegram() {
  if (!client.connected()){ reconnect(); } //Reconnect Jika Tidak Tersambung ke AP
  Indikator(); //Memanggil method Indikator
  kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
  connectBot(); //Memanggil method connectBot
  TBMessage msg; //Constructor TBMessage
  
  if(myBot.getNewMessage(msg)){  
    if(msg.text.equalsIgnoreCase("/start")){ //Start Bot
      msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nSelamat datang di Layanan BOT FISHERY IOT.";
      msg2 = "\n\n🔐 Silahkan isi kode rahasia 👇👇\n.................................. *(11 Characters)";
      sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg);
    } 
    else if(msg.text.equalsIgnoreCase(KODEBOT)){ //Menu Utama
      msg1 = "🔓 Kode yang anda masukkan benar";
      myBot.sendMessage(msg.sender.id, msg1);
      main_menu:
      msg2 = "\n--------------------------------------------------------------\n 📝 MENU UTAMA \n--------------------------------------------------------------\nSilahkan pilih menu dibawah ini 👇👇";
      viewTombol = true; myBot.sendMessage(msg.sender.id, msg2, myKbd);
    }
    else if(msg.text.equalsIgnoreCase("🌦️ Monitoring suhu air kolam")){ //Hasil Monitoring suhu air kolam
      msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring suhu terkini :\n\n--------------------------------------------------------------\n 🌦️ MONITORING SUHU \n--------------------------------------------------------------\n";
      msg2 = "💦 Suhu air pada kolam : " + String(temp) + "°C\n✍️ Status suhu : " + String(statusSuhu) + "\n⏰ Waktu : " + String(waktu) + "\n--------------------------------------------------------------"; 
      sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg);
    } 
    else if(msg.text.equalsIgnoreCase("🚰 Monitoring kekeruhan air kolam")){ //Hasil Monitoring kekeruhan air kolam
      msg1 = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\nBerikut hasil monitoring kekeruhan terkini :\n\n--------------------------------------------------------------\n 🚰 MONITORING KEKERUHAN \n--------------------------------------------------------------\n";
      msg2 = "💦 Kekeruhan air pada kolam : " + String(ntu) + "\n✍️ Status kekeruhan : " + String(statusKekeruhan) + "\n⏰ Waktu : " + String(waktu) + "\n--------------------------------------------------------------"; 
      sendMsg = msg1 + msg2; myBot.sendMessage(msg.sender.id, sendMsg);
    }
    else if(msg.text.equalsIgnoreCase("♨️ Pengaturan suhu air kolam")){ //Sub Menu Pengaturan suhu air kolam
      sub_menu1:
      sendMsg = "--------------------------------------------------------------\n ♨️ PENGATURAN SUHU AIR \n--------------------------------------------------------------\nBerikut pilihan sub menu yang dapat anda akses :";
      viewTombol = true; myBot.sendMessage(msg.sender.id, sendMsg, submainKbd);          
    }
    else if(msg.text.equalsIgnoreCase("⬅️ Kembali")){ //Opsi Kembali ke Menu Utama
      sendMsg = "✅ Berhasil kembali ke menu utama";
      myBot.sendMessage(msg.sender.id, sendMsg); goto main_menu;
    }
    else if(msg.text.equalsIgnoreCase("⛲ Cooler air")){ //Sub menu cooler air
      sub_menu2:
      sendMsg = "--------------------------------------------------------------\n ⛲ PENGATURAN COOLER AIR \n--------------------------------------------------------------\nBerikut pilihan sub menu yang dapat anda akses :";
      viewTombol = true; myBot.sendMessage(msg.sender.id, sendMsg, sub1Kbd);        
    }
    else if(msg.text.equalsIgnoreCase("⛲ Heater air")){ //Sub menu heater air
      sub_menu3:
      sendMsg = "--------------------------------------------------------------\n ⛲ PENGATURAN HEATER AIR \n--------------------------------------------------------------\nBerikut pilihan sub menu yang dapat anda akses :";
      viewTombol = true; myBot.sendMessage(msg.sender.id, sendMsg, sub2Kbd);          
    }
    else if(msg.text.equalsIgnoreCase("1️⃣ Cooler ON")){ //Opsi Sub Menu Cooler On
      sendMsg = "Anda yakin memilih opsi Cooler ON?";
      myBot.sendMessage(msg.sender.id, sendMsg, In1Kbd1); 
    }
    else if(msg.text.equalsIgnoreCase("2️⃣ Cooler OFF")){ //Opsi Sub Menu Cooler Off
      sendMsg = "Anda yakin memilih opsi Cooler OFF?";
      myBot.sendMessage(msg.sender.id, sendMsg, In1Kbd2); 
    }
    else if(msg.text.equalsIgnoreCase("1️⃣ Heater ON")){ //Opsi Sub Menu Heater On
      sendMsg = "Anda yakin memilih opsi Heater ON?";
      myBot.sendMessage(msg.sender.id, sendMsg, In2Kbd1); 
    }
    else if(msg.text.equalsIgnoreCase("2️⃣ Heater OFF")){ //Opsi Sub Menu Heater Off
      sendMsg = "Anda yakin memilih opsi Heater OFF?";
      myBot.sendMessage(msg.sender.id, sendMsg, In2Kbd2); 
    }
    else if(msg.text.equalsIgnoreCase("↩️ Kembali")){ //Opsi Kembali ke Menu Pengaturan Suhu air
      sendMsg = "✅ Berhasil kembali ke menu pengaturan suhu air";
      myBot.sendMessage(msg.sender.id, sendMsg); goto sub_menu1;
    }
    else if(msg.messageType == CTBotMessageQuery){ //Respon Dari Semua Opsi
      if(msg.callbackQueryData.equals(INrespYes1)){ //Respon Opsi Sub Menu Cooler On
        Data["Cooler"] = "1";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "Cooler berhasil anda nyalakan !! 📢";
        myBot.sendMessage(msg.sender.id, sendMsg);
        Serial.println("\nStatus Cooler: Menyala !!");
        digitalWrite(COOLER_PIN, HIGH); digitalWrite(HEATER_PIN, LOW); delay(10000);
      }
      else if(msg.callbackQueryData.equals(INrespYes2)){ //Respon Opsi Sub Menu Cooler Off
        Data["Cooler"] = "0";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "Cooler berhasil anda matikan !! 📢";
        myBot.sendMessage(msg.sender.id, sendMsg);
        Serial.println("\nStatus Cooler: Mati !!");
        digitalWrite(COOLER_PIN, LOW);
      }
      else if(msg.callbackQueryData.equals(INrespNo1)){ //Respon Opsi Tidak -> sub_menu2
        Data["Cooler"] = "0";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "❌ Perintah telah dibatalkan!!!";
        myBot.sendMessage(msg.sender.id, sendMsg);
        digitalWrite(COOLER_PIN, LOW); delay(2000); goto sub_menu2;
      }
      else if(msg.callbackQueryData.equals(INrespYes3)){ //Respon Opsi Sub Menu Heater On
        Data["Heater"] = "1";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "Heater berhasil anda nyalakan !! 📢";
        myBot.sendMessage(msg.sender.id, sendMsg);
        Serial.println("\nStatus Heater: Menyala !!");
        digitalWrite(HEATER_PIN, HIGH); digitalWrite(COOLER_PIN, LOW); delay(10000);
      }
      else if(msg.callbackQueryData.equals(INrespYes4)){ //Respon Opsi Sub Menu Heater Off
        Data["Heater"] = "0";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "Heater berhasil anda matikan !! 📢";
        myBot.sendMessage(msg.sender.id, sendMsg);
        Serial.println("\nStatus Heater: Mati !!");
        digitalWrite(HEATER_PIN, LOW);
      }
      else if(msg.callbackQueryData.equals(INrespNo2)){ //Respon Opsi Tidak -> sub_menu3
        Data["Heater"] = "0";
        kirimDataThingsBoard(); //Memanggil method kirimDataThingsBoard
        sendMsg = "❌ Perintah telah dibatalkan!!!";
        myBot.sendMessage(msg.sender.id, sendMsg);
        digitalWrite(HEATER_PIN, LOW); delay(2000); goto sub_menu3;
      }
    }
    else{ //Control Error jika perintah tidak sesuai
      sendMsg = "🙋🏻‍♂️ Hai @" + msg.sender.username + " 👋👋\n\n❌ Gagal mengakses, coba lagi";
      myBot.sendMessage(msg.sender.id, sendMsg);
    } 
  }
}

//Bagian untuk menampilkan kode max address di serial monitor
void macAddressPrint(){
  Serial.print("[OLD] ESP32 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);
  Serial.print("[NEW] ESP32 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
}

//Method Utama yang hanya dapat dijalankan sekali
void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  dhtSensor.begin();
  InitWiFi(); //Memanggil method InitWiFi
  client.setServer(THINGSBOARD_SERVER, 1883);
  client.setCallback(callback);
  pinMode(HEATER_PIN, OUTPUT);
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(DANGER_INDICATOR_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, LOW);
  digitalWrite(COOLER_PIN, LOW);
  digitalWrite(DANGER_INDICATOR_PIN, LOW);
  ButtonBot(); //Memanggil method Tombol Custom pada Bot Telegram
  RTCinit(); //Memanggil method RTCinit
  macAddressPrint(); //Memanggil method macAddress
}

//Method yang dapat dijalankan terus-menerus
void loop() {
  botTelegram(); //Memanggil method opsi menu Bot Telegram
  client.loop();
  delay(500); //Tunda setengah detik
}
