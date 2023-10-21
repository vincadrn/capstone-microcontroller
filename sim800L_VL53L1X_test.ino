#define TINY_GSM_MODEM_SIM800 // Select your modem: Modem is SIM800L
#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed
#define GSM_PIN ""  // set GSM PIN, if any

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <set>
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <Wire.h>
#include <VL53L1X.h>
#include <TinyGsmClient.h>
#include <HardwareSerial.h>
HardwareSerial SerialAT (2);
VL53L1X sensor;

//#define DUMP_AT_COMMANDS
#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed
#define GSM_PIN ""  // set GSM PIN, if any
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

std::set<String> theSet;
unsigned long startTime = millis();
unsigned long endTime;

String maclist[64][3]; 
int listcount = 0;

// Your GPRS credentials, if any
const char apn[] = ""; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "";
const char gprsPass[] = "";

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 

const char *ssid = "CASCADE_HOUSE01 2.4G";
const char *password = "Mandorgoweng8601";

// MQTT details
const char* broker = "vulture.rmq.cloudamqp.com";                    // Public IP address or domain name
const char* mqttUsername = "hqpkkqwz:hqpkkqwz";  // MQTT username
const char* mqttPassword = "0bPSePKiWVoFBxSZHeQcxNqmYN1bn0Rv";  // MQTT password
int mqttPort = 8883;
const char *ROOT_CERT = "-----BEGIN CERTIFICATE-----\nMIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMTDkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1owTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XCov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpLwYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+DLtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5ysR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZXmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBcSLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2qlPRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TNDTwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEBATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQub3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9EU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26ZtuMA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuGWCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9Ohe8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFCDfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n-----END CERTIFICATE-----";

const char* topicOutput1 = "/swa/commands";
const char* topicCrowd = "/swa/test";

WiFiClientSecure wifiClient;
TinyGsmClient client(modem);
PubSubClient mqtt(client);

#define echoPin  18 
#define trigPin   5
#define I2C_SDA  21
#define I2C_SCL  22

TwoWire I2CPower = TwoWire(0); // I2C for SIM800 (to keep it running when powered from battery)
#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

String defaultTTL = "60"; // Maximum time (Apx seconds) elapsed before device is consirded offline

const wifi_promiscuous_filter_t filt={ //Idk what this does
    .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA
};

typedef struct { // or this
  uint8_t mac[6];
} __attribute__((packed)) MacAddr;

typedef struct { // still dont know much about this
  int16_t fctl;
  int16_t duration;
  MacAddr da;
  MacAddr sa;
  MacAddr bssid;
  int16_t seqctl;
  unsigned char payload[];
} __attribute__((packed)) WifiMgmtHdr;

#define maxCh 13 //max Channel -> US = 11, EU = 13, Japan = 14

int curChannel = 1;
int distance;
int adaBus;

void sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { //This is where packets end up after they get sniffed
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf; // Dont know what these 3 lines do
  int len = p->rx_ctrl.sig_len;
  WifiMgmtHdr *wh = (WifiMgmtHdr*)p->payload;
  
  len -= sizeof(WifiMgmtHdr);
  if (len < 0){
    Serial.println("Receuved 0");
    return;
  }

  String packet;
  String mac;
  int fctl = ntohs(wh->fctl);

  for(int i=8;i<=8+6+1;i++){ // This reads the first couple of bytes of the packet. This is where you can read the whole packet replaceing the "8+6+1" with "p->rx_ctrl.sig_len"
     packet += String(p->payload[i],HEX);
  }
  for(int i=4;i<=15;i++){ // This removes the 'nibble' bits from the stat and end of the data we want. So we only get the mac address.
    mac += packet[i];
  }
  mac.toUpperCase();

  theSet.emplace(mac);
  
  int added = 0;
  for(int i=0;i<=63;i++){ // checks if the MAC address has been added before
    if(mac == maclist[i][0]){
      maclist[i][1] = defaultTTL;
      if(maclist[i][2] == "OFFLINE"){
        maclist[i][2] = "0";
      }
      added = 1;
    }
  }
  
  if(added == 0){ // If its new. add it to the array.
    maclist[listcount][0] = mac;
    maclist[listcount][1] = defaultTTL;
    //Serial.println(mac);
    listcount ++;
    if(listcount >= 64){
      Serial.println("Too many addresses");
      listcount = 0;
    }
  }
}

void configurePromiscuousWiFi() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
}

void connectToWiFi() {
  Serial.print("Connecting to ");
 
  WiFi.begin(ssid, password);
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("Connected.");  
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Callback - ");
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}

void setupTOF(){
  Wire.setClock(400000); // use 400 kHz I2C
  sensor.setTimeout(500);
  if (!sensor.init()){
    Serial.println("Failed to detect and initialize sensor!");
    while (1);
  }
  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(15000);
  sensor.startContinuous(15);
}

void setupMQTT() {
  mqtt.setServer(broker, mqttPort);
  mqtt.setCallback(mqttCallback); // set the callback function
}

void reconnect() {
  Serial.println("Connecting to MQTT Broker...");
  while (!mqtt.connected()) {
      Serial.println("Reconnecting to MQTT Broker..");
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      
      if (mqtt.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
        Serial.println("Connected.");
        // subscribe to topic
        mqtt.subscribe(topicOutput1);
      }
      
  }
}

int checkBus(){
  
  distance = sensor.read();
  delay(50);
 
  Serial.print("Distance: ");
  Serial.print(distance); // Print the output in serial monitor
  Serial.println(" cm");

  if(distance > 400){
    Serial.println("Tidak ada bus");
    adaBus = 0;
  }else{
    Serial.println("Ada bus");
    adaBus = 1;
  }

  return adaBus;
}

void setup() {
  // Set console baud rate
  Serial.begin(115200);
  delay(10);
  Serial.println("starting!");
  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);
  
  // Keep power when running from battery
  bool isOk = setPowerBoostKeepOn(1);
  Serial.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
  Serial.println("Wait...");

  Wire.begin();
  setupTOF();

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, 16, 17);
  delay(6000);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();
  // modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);

  // Unlock your SIM card with a PIN if needed
  //if ( GSM_PIN && modem.getSimStatus() != 3 ) {
  //  modem.simUnlock(GSM_PIN);
  //}

  Serial.print("Connecting to APN: ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" fail");
    ESP.restart();
  }
  else {
    Serial.println(" OK");
  }
  
  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  }

  setupMQTT();
}

void purge(){ // This manages the TTL
  for(int i=0;i<=63;i++){
    if(!(maclist[i][0] == "")){
      int ttl = (maclist[i][1].toInt());
      ttl --;
      if(ttl <= 0){
        //Serial.println("OFFLINE: " + maclist[i][0]);
        maclist[i][2] = "OFFLINE";
        maclist[i][1] = defaultTTL;
      }else{
        maclist[i][1] = String(ttl);
      }
    }
  }
}

void updatetime(){ // This updates the time the device has been online for
  for(int i=0;i<=63;i++){
    if(!(maclist[i][0] == "")){
      if(maclist[i][2] == "")maclist[i][2] = "0";
      if(!(maclist[i][2] == "OFFLINE")){
          int timehere = (maclist[i][2].toInt());
          timehere ++;
          maclist[i][2] = String(timehere);
      }
      
      //Serial.println(maclist[i][0] + " : " + maclist[i][2]);
      
    }
  }
}

//void showpeople(){ // This checks if the MAC is in the reckonized list and then displays it on the OLED and/or prints it to serial.
//  String forScreen = "";
//  for(int i=0;i<=63;i++){
//    String tmp1 = maclist[i][0];
//    if(!(tmp1 == "")){
//      for(int j=0;j<=9;j++){
//        String tmp2 = KnownMac[j][1];
//        if(tmp1 == tmp2){
//          forScreen += (KnownMac[j][0] + " : " + maclist[i][2] + "\n");
//          Serial.print(KnownMac[j][0] + " : " + tmp1 + " : " + maclist[i][2] + "\n -- \n");
//        }
//      }
//    }
//  }
//  update_screen_text(forScreen);
//}

void showAll() {
  bool isEmpty = false;
  for(int i = 0; i < 64; i++) {
    for(int j = 0; j < 3; j++) {
      if (maclist[i][0] == "") isEmpty = true;
      Serial.print(maclist[i][j]);
      Serial.print(" | ");
    }
    if (isEmpty) break;
    Serial.println("");
  }
  Serial.println();
  Serial.print("Set: ");
  Serial.println(theSet.size());
}

void loop() {
  configurePromiscuousWiFi();

  if(curChannel > maxCh){ 
    curChannel = 1;
  }
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  updatetime();
  purge();
  //showpeople();
  showAll();
  endTime = millis();

  if (endTime - startTime > 4000) {
    theSet.clear();
    startTime = millis();
    Serial.println("Reset!");
  }

  connectToWiFi();
  wifiClient.setCACert(ROOT_CERT);

  int busStatus = checkBus();

  if (!mqtt.connected())
    reconnect();
  mqtt.publish(topicCrowd, String(theSet.size()).c_str());
  //    curChannel++;
  delay(2000);

  mqtt.loop();
}