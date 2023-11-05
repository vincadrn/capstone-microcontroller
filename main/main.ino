#define TINY_GSM_MODEM_SIM800 // Select your modem: Modem is SIM800L
#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed
//#define SerialSIM Serial2
#define AT_BAUD_RATE 9600
// Define how you're planning to connect to the internet.
// This is only needed for this example, not in other code.
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GSM_PIN ""  // set GSM PIN, if any

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <algorithm>
#include <set>
#include "esp_wifi.h"
#include "mqtt_client.h"
#include <Wire.h>
#include <TinyGsmClient.h>
#include <HardwareSerial.h>
HardwareSerial SerialSIM (1);


#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialSIM, Serial);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialSIM);
#endif

std::set<String> theSet;
unsigned long startTime = millis();
unsigned long endTime;

// Your GPRS credentials, if any
const char apn[] = "internet"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "";
const char gprsPass[] = "";

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = ""; 

// If possible, make this creds written in ESP NVM instead of hardcoding it like this.
// Will create another file to load and delete NVM if this is going to be implemented.

// MQTT details
const char* broker = "vulture.rmq.cloudamqp.com";                    // Public IP address or domain name
const char* mqttUsername = "hqpkkqwz:hqpkkqwz";  // MQTT username
const char* mqttPassword = "0bPSePKiWVoFBxSZHeQcxNqmYN1bn0Rv";  // MQTT password
int mqttPort = 1883;
const char *ROOT_CERT = "-----BEGIN CERTIFICATE-----\nMIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMTDkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1owTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2VhcmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XCov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpLwYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+DLtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5ysR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZXmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBcSLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2qlPRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TNDTwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEBATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQub3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9EU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26ZtuMA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuGWCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9Ohe8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFCDfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n-----END CERTIFICATE-----";

const char* topicBus = "/track/bus";
const char* topicCrowd = "/track/people";

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
long duration;
int distance;
int adaBus;

void sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { //This is where packets end up after they get sniffed
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf; // Dont know what these 3 lines do
  int len = p->rx_ctrl.sig_len;
  WifiMgmtHdr *wh = (WifiMgmtHdr*)p->payload;
  
  len -= sizeof(WifiMgmtHdr);
  if (len < 0){
    Serial.println("Received 0");
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

//void connectToWiFi() {
//  Serial.print("Connecting to ");
// 
//  WiFi.begin(ssid, password);
//  Serial.println(ssid);
//  while (WiFi.status() != WL_CONNECTED) {
//    Serial.print(".");
//    delay(500);
//  }
//  Serial.print("Connected.");  
//}

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

void setupMQTT() {
  mqtt.setServer(broker, mqttPort);
}

void setupUltrasonic() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void reconnect() {
  Serial.println("Connecting to MQTT Broker...");
  while (!mqtt.connected()) {
      Serial.println("Reconnecting to MQTT Broker..");
      String clientId = "ESP32Client-capstone";
      
      if (mqtt.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
        Serial.println("Connected.");
        // subscribe to topic
        // mqtt.subscribe(topicOutput1);
      }
      
  }
}

int checkBus(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.0344 / 2;
 
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

void showAll() {
  std::for_each(theSet.cbegin(), theSet.cend(), [](String x) {
    Serial.println(x);
  });
  Serial.print("Set: ");
  Serial.println(theSet.size());
}

void setup() {
  // Set console baud rate
  Serial.begin(115200);
  SerialSIM.begin(AT_BAUD_RATE, SERIAL_8N1, 16, 17);
//  SerialPort2.begin(AT_BAUD_RATE, SERIAL_8N1, 16, 17);
  delay(6000);
  Serial.println("starting!");
  setupUltrasonic();
//  // Start I2C communication
//  I2CPower.begin(I2C_SDA, I2C_SCL, 400000ul);
//  
//  // Keep power when running from battery
//  bool isOk = setPowerBoostKeepOn(1);M
//  Serial.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));
//  Serial.println("Wait...");
//
//  // Set GSM module baud rate and UART pins
//  SerialPort2.begin(115200, SERIAL_8N1, 16, 17);
//  delay(6000);
//
//  // Restart takes quite some time
//  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();
  modem.setBaud(AT_BAUD_RATE);
//   modem.init();
//
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem Info: ");
  Serial.println(modemInfo);
//
//  // Unlock your SIM card with a PIN if needed
//  //if ( GSM_PIN && modem.getSimStatus() != 3 ) {
//  //  modem.simUnlock(GSM_PIN);
//  //}
//
  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }
  Serial.print("Connecting to APN: ");
  Serial.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" fail");
    ESP.restart();
  }
  else {
    Serial.println(" OK");
  }
//  
//  if (modem.isGprsConnected()) {
//    Serial.println("GPRS connected");
//  }

  setupMQTT();
}

void loop() {
  configurePromiscuousWiFi();

  if(curChannel > maxCh){ 
    curChannel = 1;
  }
  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  showAll();
  endTime = millis();

  if (endTime - startTime > 4000) {
    theSet.clear();
    startTime = millis();
    Serial.println("Reset!");
  }

//  connectToWiFi();
//  wifiClient.setCACert(ROOT_CERT);

  int busStatus = checkBus();
//  int busStatus = 1;

  if (!mqtt.connected())
    reconnect();
  mqtt.publish(topicCrowd, String(theSet.size()).c_str());
  if (busStatus) mqtt.publish(topicBus, String(busStatus).c_str());
  //    curChannel++;
  delay(2000);

  mqtt.loop();
}
