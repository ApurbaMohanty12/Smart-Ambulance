/*
  ESP32 Receiver — ESP-NOW + Serial JSON Output
  ===============================================
  Receives data from ambulance sender via ESP-NOW.
  Controls traffic LEDs + sensor LEDs locally.
  Prints JSON over Serial so the Python bridge can
  forward it to the web dashboard via WebSocket.

  JSON format sent every update:
  {"temp":36.5,"gas":2,"vib":0,"emergency":1}
*/

#include <WiFi.h>
#include <esp_now.h>

// ── Traffic Signal LEDs ──
#define RED_LED  18
#define YEL_LED  19
#define GRN_LED  23

// ── Sensor Alert LEDs ──
#define GAS_LED  33
#define TEMP_LED 26
#define VIB_LED  25

// ── Emergency green duration (ms) ──
#define GREEN_DURATION 30000

typedef struct {
  float temp;
  int   gas;
  int   vib;
  int   emergency;
} Data;

Data data;

unsigned long greenStart    = 0;
bool          emergencyActive = false;

// ── ESP-NOW receive callback ──
void onReceive(const esp_now_recv_info_t *info,
               const uint8_t *incomingData,
               int len) {

  memcpy(&data, incomingData, sizeof(data));

  // ── Sensor LEDs ──
  digitalWrite(GAS_LED,  data.gas  > 2);
  digitalWrite(TEMP_LED, data.temp > 10);
  digitalWrite(VIB_LED,  data.vib  == 1);

  // ── Traffic LEDs ──
  if (data.emergency == 1) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GRN_LED, HIGH);
    greenStart     = millis();
    emergencyActive = true;
    Serial.println(">> AMBULANCE DETECTED — GREEN");
  }

  // ── Print JSON for dashboard bridge ──
  // Format: {"temp":XX.X,"gas":X,"vib":X,"emergency":X}
  Serial.print("{");
  Serial.print("\"temp\":");    Serial.print(data.temp, 1);
  Serial.print(",\"gas\":");    Serial.print(data.gas);
  Serial.print(",\"vib\":");    Serial.print(data.vib);
  Serial.print(",\"emergency\":"); Serial.print(data.emergency);
  Serial.println("}");
}

void setup() {
  Serial.begin(115200);

  // ── Pin modes ──
  pinMode(RED_LED,  OUTPUT);
  pinMode(YEL_LED,  OUTPUT);
  pinMode(GRN_LED,  OUTPUT);
  pinMode(GAS_LED,  OUTPUT);
  pinMode(TEMP_LED, OUTPUT);
  pinMode(VIB_LED,  OUTPUT);

  // Default: RED on
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YEL_LED, LOW);
  digitalWrite(GRN_LED, LOW);

  // ── ESP-NOW init ──
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: esp_now_init() failed");
    return;
  }

  esp_now_register_recv_cb(onReceive);

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Ready — waiting for ESP-NOW packets...");
}

void loop() {
  // Restore RED after green timer expires
  if (emergencyActive && millis() - greenStart > GREEN_DURATION) {
    digitalWrite(GRN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    emergencyActive = false;
    Serial.println(">> Timer expired — back to RED");

    // Send updated state to dashboard
    Serial.print("{");
    Serial.print("\"temp\":");    Serial.print(data.temp, 1);
    Serial.print(",\"gas\":");    Serial.print(data.gas);
    Serial.print(",\"vib\":");    Serial.print(data.vib);
    Serial.print(",\"emergency\":0");   // emergency cleared
    Serial.println("}");
  }
}
