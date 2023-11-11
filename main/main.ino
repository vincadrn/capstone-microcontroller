#define TINY_GSM_MODEM_SIM800 // Select your modem: Modem is SIM800L
#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed
//#define SerialSIM Serial2
#define AT_BAUD_RATE 9600
// Define how you're planning to connect to the internet.
// This is only needed for this example, not in other code.
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define GENERAL_TIMEOUT 60000   // in ms

#include <PubSubClient.h>
#include <algorithm>
#include <set>
#include "esp_wifi.h"
#include "mqtt_client.h"
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

// This block of codes is used for
// enabling/suppressing serial debug
// Should be DISABLED in production
// Uncomment line below to enable debug
#define DEBUG
#ifdef DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__)
#define D_print(...)    Serial.print(__VA_ARGS__)
#define D_write(...)    Serial.write(__VA_ARGS__)
#define D_println(...)  Serial.println(__VA_ARGS__)
#else
#define D_SerialBegin(...)
#define D_print(...)
#define D_write(...)
#define D_println(...)
#endif

std::set<String> theSet;
uint32_t startTime = millis();
uint32_t endTime;

// GPRS creds
const char apn[] = "internet";  // by.u, Indosat
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
const char* broker = "vulture.rmq.cloudamqp.com";                    // Public IP address or domain name
const char* mqttUsername = "hqpkkqwz:hqpkkqwz";                      // MQTT username
const char* mqttPassword = "0bPSePKiWVoFBxSZHeQcxNqmYN1bn0Rv";       // MQTT password
int mqttPort = 1883;

const char* topicBus = "/track/bus";
const char* topicCrowd = "/track/people";

TinyGsmClient client(modem);
PubSubClient mqtt(client);

#define echoPin  18 
#define trigPin   5

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

int curChannel = 1;
long duration;
int distance;

void sniffer(void* buf, wifi_promiscuous_pkt_type_t type) { //This is where packets end up after they get sniffed
  wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf; // Dont know what these 3 lines do
  int len = p->rx_ctrl.sig_len;
  WifiMgmtHdr *wh = (WifiMgmtHdr*)p->payload;
  
  len -= sizeof(WifiMgmtHdr);
  if (len < 0){
    D_println("Received 0");
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

void setupMQTT() {
  mqtt.setServer(broker, mqttPort);
}

void setupUltrasonic() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void reconnectMQTT() {
  D_println("Connecting to MQTT Broker...");
  while (!mqtt.connected()) {
      D_println("Reconnecting to MQTT Broker..");
      String clientId = "ESP32Client-capstone";
      
      if (mqtt.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
        D_println("Connected.");
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
 
  D_print("Distance: ");
  D_print(distance); // Print the output in serial monitor
  D_println(" cm");

  bool busArrived = false;
  if(distance > 400){
    D_println("Tidak ada bus");
    busArrived = false;
  } else{
    D_println("Ada bus");
    busArrived = true;
  }

  return busArrived;
}

void showAll() {
  std::for_each(theSet.cbegin(), theSet.cend(), [](String x) {
    D_println(x);
  });
  D_print("Set: ");
  D_println(theSet.size());
}

void setup() {
  // Set console baud rate
  D_SerialBegin(115200);
  SerialSIM.begin(AT_BAUD_RATE, SERIAL_8N1, 16, 17);
  delay(6000);
  D_println("starting!");
  setupUltrasonic();
  
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  D_println("Initializing modem...");
  modem.restart();
  modem.setBaud(AT_BAUD_RATE);

  String modemInfo = modem.getModemInfo();
  D_print("Modem Info: ");
  D_println(modemInfo);
  
  if (modem.isNetworkConnected()) {
    D_println("Network connected");
  }
  D_print("Connecting to APN: ");
  D_println(apn);

  startTime = millis();
  while (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    D_println(" fail");
    if (millis() - startTime > GENERAL_TIMEOUT) {
      ESP.restart();
    }
  }
  D_println("GPRS Connect OK");
  
  if (modem.isGprsConnected()) {
    D_println("GPRS connected");
  }

  setupMQTT();
}

void loop() {
  configurePromiscuousWiFi();

  esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
  showAll();
  endTime = millis();

  if (endTime - startTime > 4000) {
    theSet.clear();
    startTime = millis();
    D_println("Reset!");
  }

  bool busStatus = checkBus();

  if (!mqtt.connected())
    reconnectMQTT();
  mqtt.publish(topicCrowd, String(theSet.size()).c_str());
  if (busStatus)
    mqtt.publish(topicBus, String(busStatus).c_str());
  delay(10000);

  mqtt.loop();
}
