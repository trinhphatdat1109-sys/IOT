#include <WiFi.h>
#include <PubSubClient.h>

// ========= WiFi =========
const char* WIFI_SSID     = "Sailor Moon";
const char* WIFI_PASS     = "sailorxinchao";

// ========= MQTT Cloud =========
// Khuyến nghị dùng EMQX/HiveMQ Cloud (điền host/port/user/pass)
const char* MQTT_HOST     = "YOUR_BROKER_HOST"; // ví dụ: xxxxx.s1.eu.hivemq.cloud
const int   MQTT_PORT     = 1883;               // 1883 (no TLS) hoặc 8883 (TLS)*
const char* MQTT_USER     = "YOUR_USER";        // nếu broker yêu cầu
const char* MQTT_PASS     = "YOUR_PASS";        // nếu broker yêu cầu
const char* MQTT_CLIENTID = "ESP32WaterPump";

// *Nếu dùng TLS 8883 bạn sẽ cần WiFiClientSecure + CA (mình sẽ viết giúp nếu bạn chọn broker)

// ========= Topics =========
const char* T_MODE_CMD    = "waterpump/control";   // "OFF" | "ON" | "AUTO"
const char* T_LEVEL_CM    = "waterpump/mucNuoc";
const char* T_STATUS      = "waterpump/status";    // ONLINE/OFFLINE + RUN/STOP
const char* T_MODE        = "waterpump/mode";      // echo chế độ hiện tại

// ========= Pins =========
#define RELAY_PIN  26   // LED test (HIGH = ON)
#define LED_PIN    2
#define TRIG_PIN   23
#define ECHO_PIN   22

// ========= Tank & SR04 =========
const float H_TANK_CM         = 20.0f; // chiều cao bồn (cm)
const float OFFSET_CM         = 0.0f;  // bù cảm biến -> miệng bồn (cm), đo thực tế rồi sửa
const float AUTO_ON_LEVEL     = 5.0f;  // bật khi < 5 cm
const float AUTO_OFF_LEVEL    = 18.0f; // tắt khi >= 18 cm

// ========= Globals =========
WiFiClient espClient;
PubSubClient client(espClient);
String mode = "OFF";
bool pumpOn = false;
int  mucNuoc = 0;
unsigned long lastPub = 0;

void setPump(bool on) {
  pumpOn = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  digitalWrite(LED_PIN,   on ? HIGH : LOW);
}

float readOnceCm() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long d = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (d == 0) return NAN;
  return (d / 2.0f) * 0.0343f; // cm
}

float median3(float a, float b, float c) {
  float x[3] = {a,b,c};
  for (int i=0;i<3;i++) if (isnan(x[i])) x[i] = 1e9;
  if (x[0]>x[1]) std::swap(x[0],x[1]);
  if (x[1]>x[2]) std::swap(x[1],x[2]);
  if (x[0]>x[1]) std::swap(x[0],x[1]);
  return (x[1] > 1e8) ? NAN : x[1];
}

void readWaterLevel() {
  float d1 = readOnceCm(); delay(10);
  float d2 = readOnceCm(); delay(10);
  float d3 = readOnceCm();
  float ds = median3(d1,d2,d3);   // sensor -> water
  if (isnan(ds)) return;
  float d_rim = ds - OFFSET_CM;   // từ miệng bồn
  float level = H_TANK_CM - d_rim;
  if (level < 0) level = 0;
  if (level > H_TANK_CM) level = H_TANK_CM;
  mucNuoc = (int)roundf(level);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi connecting to %s", WIFI_SSID);
  for (int i=0;i<80 && WiFi.status()!=WL_CONNECTED;i++){ delay(250); Serial.print("."); }
  Serial.println();
  if (WiFi.status()==WL_CONNECTED) {
    Serial.print("WiFi OK, IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  payload[len] = '\0';
  String cmd = String((char*)payload);
  if (String(topic) == T_MODE_CMD) {
    if (cmd=="OFF" || cmd=="ON" || cmd=="AUTO") {
      mode = cmd;
      client.publish(T_MODE, mode.c_str(), true);
      Serial.printf("Mode -> %s\n", mode.c_str());
    }
  }
}

void mqttEnsure() {
  while (!client.connected()) {
    Serial.print("MQTT connect...");
    // LWT OFFLINE
    if (client.connect(MQTT_CLIENTID, MQTT_USER, MQTT_PASS, T_STATUS, 1, true, "OFFLINE")) {
      Serial.println("OK");
      client.subscribe(T_MODE_CMD, 1);
      client.publish(T_STATUS, "ONLINE", true);
      client.publish(T_MODE, mode.c_str(), true);
    } else {
      Serial.printf("fail rc=%d\n", client.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  setPump(false);

  setupWiFi();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(mqttCallback);
}

void loop() {
  if (!client.connected()) mqttEnsure();
  client.loop();

  readWaterLevel();

  if (mode=="AUTO") {
    static bool autoState=false;
    if (mucNuoc < AUTO_ON_LEVEL)  autoState = true;
    if (mucNuoc >= AUTO_OFF_LEVEL) autoState = false;
    setPump(autoState);
  } else if (mode=="ON") {
    setPump(true);
  } else {
    setPump(false);
  }

  unsigned long now = millis();
  if (now - lastPub > 2000) {
    lastPub = now;
    char buf[16]; snprintf(buf,sizeof(buf),"%d",mucNuoc);
    client.publish(T_LEVEL_CM, buf);
    client.publish(T_STATUS, pumpOn ? "ON" : "OFF", true);
    client.publish(T_MODE, mode.c_str(), true);
    Serial.printf("Level: %d cm | Pump: %s | Mode: %s\n",
      mucNuoc, pumpOn?"ON":"OFF", mode.c_str());
  }

  delay(30);
}
