#define TINY_GSM_MODEM_SIM800 // Select your modem: Modem is SIM800L
#define TINY_GSM_DEBUG Serial // Define the serial console for debug prints, if needed
//#define SerialSIM Serial2
#define AT_BAUD_RATE 9600
// Define how you're planning to connect to the internet.
// This is only needed for this example, not in other code.
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define CROWD_COUNTING_PERIOD 150000   // in ms, should be every 150 seconds
#define GENERAL_TIMEOUT 60000          // in ms, should be 60 seconds
#define WAKEUP_TIME 20000              // in ms, should be 20 seconds, intended for ultrasonic polling
#define us_TO_ms_FACTOR 1000ULL          

#include <PubSubClient.h>
#include <algorithm>
#include <set>
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_sleep.h"
#include "time.h"
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
RTC_DATA_ATTR uint32_t g_millisOffset = CROWD_COUNTING_PERIOD + 1;  // to force it at first start

typedef struct gsmTime {
  int dayOfWeek;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  float timezone;
} gsmTime;
gsmTime timeInfo;
const int sleepAfterSec = 19 * 3600 + 00 * 60;   // sleep after 19:00
const int wakeupAfterSec = 7 * 3600 + 00 * 60;   // wakeup after 07:00

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

#define simDTR   21
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

  uint32_t startTime = millis();
  while (!mqtt.connected()) {
    D_println("Reconnecting to MQTT Broker..");
    String clientId = "ESP32Client-capstone";
    
    if (millis() - startTime > GENERAL_TIMEOUT) {
      ESP.restart();
    }
    
    if (mqtt.connect(clientId.c_str(), mqttUsername, mqttPassword)) {
      D_println("Connected.");
    }
  }
}

void connectGPRS() {
  if (modem.isGprsConnected()) {
    D_println("GPRS connected");
    return;
  }

  D_print("Connecting to APN: ");
  D_println(apn);
  
  uint32_t startTime = millis();
  while (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    D_println("GPRS connecting...");
    D_println(" fail");
    if (millis() - startTime > GENERAL_TIMEOUT) {
      ESP.restart();
    }
    delay(100);
  }
  if (modem.isGprsConnected()) {
    D_println("GPRS Connect OK");
  }
}

void sleepEnableSIM(bool enable) {
  digitalWrite(simDTR, (enable ? HIGH : LOW));
  delay(200);
  
  uint32_t startTime = millis();
  while (!modem.sleepEnable(enable)) {
    if (millis() - startTime > GENERAL_TIMEOUT) {
      ESP.restart();
    }
  }
}

int getDayOfWeek(int d, int m, int y) {
  // 0: Sunday, 1: Monday, etc.
  static int t[] = { 0, 3, 2, 5, 0, 3,
                       5, 1, 4, 6, 2, 4 }; 
  y -= m < 3;
  return ( y + y / 4 - y / 100 + 
           y / 400 + t[m - 1] + d) % 7; 
}

void getActualTime() {
  D_println("Getting actual time ...");
  uint32_t startTime = millis();
  while (!modem.getNetworkTime(&(timeInfo.year), &(timeInfo.month), &(timeInfo.day), &(timeInfo.hour), &(timeInfo.minute), &(timeInfo.second), &(timeInfo.timezone))) {
    if (millis() - startTime > GENERAL_TIMEOUT) {
      ESP.restart();
    }
  }
  timeInfo.dayOfWeek = getDayOfWeek(timeInfo.day, timeInfo.month, timeInfo.year);
  
  D_println(String(timeInfo.dayOfWeek) + ", " + String(timeInfo.year) + "-" + String(timeInfo.month) + "-" + String(timeInfo.day) + " " + String(timeInfo.hour) + ":" + String(timeInfo.minute) + ":" + String(timeInfo.second) + "+" + String(timeInfo.timezone));
}

void checkForSleep() {
  int currentTimeInSec = timeInfo.hour * 3600 + timeInfo.minute * 60 + timeInfo.second;
  D_print("Current time sec: ");
  D_println(currentTimeInSec);

  // If it is Monday (1) - Friday (5) and in working hours, don't sleep
  if (timeInfo.dayOfWeek != 0 && timeInfo.dayOfWeek != 6 && currentTimeInSec > wakeupAfterSec && currentTimeInSec < sleepAfterSec) {
    D_println("Not yet the time to sleep");
    return;
  } else {
    /* Algorithm for counting sleep needed
    End - start. If < 0, add 24h. It means the hour <= 23
    Example 1: now is 20:00 (if for some reason the system doesn't sleep on time),
    then needed sleep is 06:30 - 20:00 = -13:30 + 24 = 10h30m
    Example 2: now is 00:00, then needed sleep is
    06:30 - 00:00 = 6h30m
    */
    int sleepDurationInSec = wakeupAfterSec - currentTimeInSec;
    D_print("Wakeup after sec: ");
    D_println(wakeupAfterSec);
    if (sleepDurationInSec < 0) {
      sleepDurationInSec += 24 * 3600;
    }
    D_print("Sleep duration in sec: ");
    D_println(sleepDurationInSec);

    modem.gprsDisconnect();
    sleepEnableSIM(true);
    
    esp_sleep_enable_timer_wakeup(sleepDurationInSec * 1000 * us_TO_ms_FACTOR);
    D_print("Sleeping for ");
    D_print(sleepDurationInSec);
    D_println(" second(s)");
    esp_deep_sleep_start();
  }
}

int checkBus(){
  float duration = 0.0;
  float distance = 0.0;
  
  for (int i = 0; i < 3; i++) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration += pulseIn(echoPin, HIGH);
  }

  distance = (duration / 3) * 0.0344 / 2;
 
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
  
  pinMode(simDTR, OUTPUT);
  sleepEnableSIM(false);
  
  setupUltrasonic();
    
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  if (!modem.isGprsConnected()) {
    D_println("Initializing modem...");
    modem.restart();
    modem.setBaud(AT_BAUD_RATE);

    String modemInfo = modem.getModemInfo();
    D_print("Modem Info: ");
    D_println(modemInfo);
  }

  connectGPRS();

  setupMQTT();

  getActualTime();

  checkForSleep();

  if (g_millisOffset > CROWD_COUNTING_PERIOD) {
    D_println("Entering promiscuous WiFi ...");
    configurePromiscuousWiFi();
    delay(2000);

    esp_wifi_set_channel(curChannel, WIFI_SECOND_CHAN_NONE);
    showAll();

    esp_wifi_stop();

    if (!mqtt.connected())
      reconnectMQTT();
    mqtt.publish(topicCrowd, String(theSet.size()).c_str());

    g_millisOffset = 0;
  }

  theSet.clear();
}

void loop() {
  bool busStatus = checkBus();

  if (busStatus) {
    if (!mqtt.connected())
      reconnectMQTT();
    
    mqtt.publish(topicBus, String(busStatus).c_str());
  }
  
  sleepEnableSIM(true);

  g_millisOffset += millis() + WAKEUP_TIME;
  D_println("Initiating periodic nap ...");
  esp_sleep_enable_timer_wakeup(WAKEUP_TIME * us_TO_ms_FACTOR);
  esp_deep_sleep_start();
}
