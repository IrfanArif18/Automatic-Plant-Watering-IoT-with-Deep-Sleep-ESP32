// ===== Libraries =====
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ThingerESP32.h>

// ===== Pin Assignments =====
#define sensorPin 32
#define relayPin  33

// ===== Moisture and Pump Variable and Parameters =====
int moisture, pump = 0;
#define DRY   25
#define MOIST 50

// ===== Timer Parameters =====
#define S_TO_uS 1000000
#define M_TO_S  60
#define H_TO_S  3600

// ===== WatchDog Variables =====
#define WATCHDOG_TIMEOUT_S 10
hw_timer_t * watchDogTimer = NULL;

// ===== WiFi Connections and NTP Client =====
const char* WIFI_SSID = "I believe Wi can Fi";
const char* WIFI_PASS = "11333355555577777777";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// ===== Thinger =====
#define USERNAME          "IrfanArif"
#define DEVICE_ID         "ESP32_Embed"
#define DEVICE_CREDENTIAL "crzZM6TlJg#37Bcd"
ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

// ===== Timer Variables =====
String formattedTime;
int currentHour, currentMinute, sleepHour = 0, sleepMinute = 0;
unsigned long sleepTime = 0;

// ===== Modular Functions Declaration =====
void IRAM_ATTR watchDogInterrupt();
void watchDogRefresh();
void watchDogInit();

void connectWiFi();
void NTPClientInit();

int calcMoisture();
void drivePump();

void getCurrentTime();
void sleep();

// ===== Setup =====
void setup() {
  delay(5000);
  Serial.begin(9600);
  pinMode(sensorPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);

  connectWiFi();
  NTPClientInit();

  thing["moisture"] >> outputValue(moisture);
  thing["pump"] >> outputValue(pump);
}

// ===== Loop =====
void loop() {
  thing.handle();
  moisture = calcMoisture();
  drivePump();
  sleep();
  delay(5000);
}

// ===== Modular Functions Definition =====
void IRAM_ATTR watchDogInterrupt() {
  Serial.println("Rebooting");
  delay(2000);
  ESP.restart();
}

void watchDogRefresh() {
  timerWrite(watchDogTimer, 0);
}

void watchDogInit() {
  watchDogTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(watchDogTimer, &watchDogInterrupt, true);
  timerAlarmWrite(watchDogTimer, WATCHDOG_TIMEOUT_S * S_TO_uS, false);
  timerAlarmEnable(watchDogTimer);
}

void connectWiFi() {
  long startTime, loopTime;
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  watchDogInit();
  watchDogRefresh();
  
  startTime = millis();
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print(". ");
    delay(500);
    loopTime = millis() - startTime;

    if(loopTime > 9000) {
      Serial.println("");
      Serial.println("Connection failed");
      delay(1000);
    }
  }

  timerAlarmDisable(watchDogTimer);
  Serial.println("");
  Serial.println("Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
}

void NTPClientInit() {
  timeClient.begin();
  timeClient.setTimeOffset(7 * 3600); // GMT + 7
}

int calcMoisture() {
  return map(analogRead(sensorPin), 0, 4095, 100, 0);
}

void drivePump() {
  Serial.print("Moisture level is: ");
  Serial.print(moisture);
  Serial.println("/100");
  
  if(moisture <= DRY) {
    Serial.println("Soil is dry and needs water");
    Serial.print("Driving pump . . . ");
    digitalWrite(relayPin, LOW);
    pump = 1;
    delay(5000);
    digitalWrite(relayPin, HIGH);
    Serial.println("Done");
  } else if(moisture > DRY && moisture <= MOIST) {
    Serial.println("Soil is quite moist and needs some water");
    Serial.print("Driving pump . . . ");
    digitalWrite(relayPin, LOW);
    pump = 1;
    delay(2000);
    digitalWrite(relayPin, HIGH);
    Serial.println("Done");
  } else {
    Serial.println("Soil is already wet");
    Serial.println("No need to drive pump");
    digitalWrite(relayPin, HIGH);
    pump = 0;
  }
  Serial.println("");
}

void getCurrentTime() {
  timeClient.update();
  formattedTime = timeClient.getFormattedTime();
  currentHour = timeClient.getHours();
  currentMinute = timeClient.getMinutes();
}

void sleep() {
  getCurrentTime();

  Serial.print("Currently, it's ");
  Serial.println(formattedTime);

  if(currentHour >= 6 && currentHour < 13) {
    sleepHour = 13 - currentHour;
    Serial.println("Next operating hour is at 13:00");
  } else if(currentHour >= 13 && currentHour < 20) {
    sleepHour = 20 - currentHour;
    Serial.println("Next operating hour is at 20:00");
  } else if(currentHour >= 20 && currentHour < 24) {
    sleepHour = 24 - currentHour + 6;
    Serial.println("Next operating hour is at 6:00");
  } else if(currentHour < 6) {
    sleepHour = 6 - currentHour;
    Serial.println("Next operating hour is at 6:00");
  }

  if(currentMinute > 0) {
      sleepHour -= 1;
      sleepMinute = 59 - currentMinute;
  }

  sleepTime = sleepHour * H_TO_S;
  sleepTime += sleepMinute * M_TO_S;
  sleepTime = sleepTime * S_TO_uS;

  if(sleepTime > 0) {
    Serial.print("Going to sleep for ");
    Serial.print(sleepHour);
    Serial.print(" hour(s) and ");
    Serial.print(sleepMinute);
    Serial.println(" minute(s)");
    Serial.println("==================================================");
    esp_sleep_enable_timer_wakeup(sleepTime);
    Serial.flush();
    delay(3000);
    esp_deep_sleep_start();
  }
}
