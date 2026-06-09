#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>      
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ── Wi-Fi ─────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// ── HiveMQ Cloud ──────────────────────────────────────────────────
const char* MQTT_BROKER   = "efedf4b853574cef8100661792bd568a.s1.eu.hivemq.cloud";
const int   MQTT_PORT     = 8883;
const char* MQTT_USER     = "dragon_user";
const char* MQTT_PASSWORD = "Dragon@1234";

// ── Tópicos MQTT ──────────────────────────────────────────────────
const char* TOPIC_TEMPERATURA = "dragon/telemetry/temperatura";
const char* TOPIC_UMIDADE     = "dragon/telemetry/umidade";
const char* TOPIC_PRESSAO     = "dragon/telemetry/pressao";
const char* TOPIC_OXIGENIO    = "dragon/telemetry/oxigenio";
const char* TOPIC_RADIACAO    = "dragon/telemetry/radiacao";
const char* TOPIC_ACELERACAO  = "dragon/telemetry/aceleracao";
const char* TOPIC_STATUS      = "dragon/telemetry/status";
const char* TOPIC_FULL        = "dragon/telemetry/full";

// ── Intervalos ────────────────────────────────────────────────────
const unsigned long INTERVALO_MS         = 3000;
const unsigned long RECONNECT_INTERVALO  = 10000;

// ── DHT22 — GPIO 15 ───────────────────────────────────────────────
#define DHT_PIN  15
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// ── Potenciômetros ────────────────────────────────────────────────
#define POT_O2_PIN  34
#define POT_RAD_PIN 35

// ── Pinos de alerta visual ────────────────────────────────────────
#define LED_GREEN  25
#define LED_YELLOW 26
#define LED_RED    27
#define BUZZER_PIN 14

// ── Limiares ──────────────────────────────────────────────────────
#define TEMP_WARN_L 17.0
#define TEMP_WARN_H 27.0
#define TEMP_CRIT_L 10.0
#define TEMP_CRIT_H 35.0
#define PRES_WARN_L 980.0
#define PRES_WARN_H 1040.0
#define PRES_CRIT_L 950.0
#define PRES_CRIT_H 1070.0
#define O2_WARN_L 18.0
#define O2_WARN_H 24.0
#define O2_CRIT_L 16.0
#define O2_CRIT_H 26.0
#define RAD_WARN 5.0
#define RAD_CRIT 20.0
#define ACCEL_WARN 2.5
#define ACCEL_CRIT 4.5
#define HUM_WARN_L 25.0
#define HUM_WARN_H 70.0

// ── Clientes ──────────────────────────────────────────────────────
WiFiClientSecure espClient;
PubSubClient     client(espClient);

// ── Objetos de sensor ─────────────────────────────────────────────
Adafruit_BMP085  bmp;
Adafruit_MPU6050 mpu;

bool bmpOk = false;
bool mpuOk = false;

// ── Leitura suavizada ─────────────────────────────────────────────
int lerADC(uint8_t pino) {
  long soma = 0;
  for (int i = 0; i < 10; i++) {
    soma += analogRead(pino);
    delay(2);
  }
  return (int)(soma / 10);
}

// ── Wi-Fi ─────────────────────────────────────────────────────────
void setup_wifi() {
  Serial.println("[WIFI] Conectando...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WIFI] Falha na conexao — continuando offline.");
  }
}

// ── Reconexão MQTT não-bloqueante ─────────────────────────────────
void tentarReconnect() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.print("[MQTT] Conectando...");
  String clientId = "DragonESP32-" + String(millis(), HEX);
  if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
    Serial.println(" OK!");
  } else {
    Serial.printf(" falhou rc=%d\n", client.state());
  }
}

// ── Avalia nível de alerta ────────────────────────────────────────
int avaliarAlerta(float temp, float hum, float pres,
                   float o2, float rad, float accel) {
  if (temp  < TEMP_CRIT_L || temp  > TEMP_CRIT_H) return 2;
  if (pres  < PRES_CRIT_L || pres  > PRES_CRIT_H) return 2;
  if (o2    < O2_CRIT_L   || o2    > O2_CRIT_H)   return 2;
  if (rad   > RAD_CRIT)                            return 2;
  if (accel > ACCEL_CRIT)                          return 2;
  if (temp  < TEMP_WARN_L || temp  > TEMP_WARN_H) return 1;
  if (pres  < PRES_WARN_L || pres  > PRES_WARN_H) return 1;
  if (o2    < O2_WARN_L   || o2    > O2_WARN_H)   return 1;
  if (rad   > RAD_WARN)                            return 1;
  if (accel > ACCEL_WARN)                          return 1;
  if (hum   < HUM_WARN_L  || hum   > HUM_WARN_H)  return 1;
  return 0;
}

// ── Atualiza LEDs e buzzer ────────────────────────────────────────
void atualizarAlertas(int nivel) {
  digitalWrite(LED_GREEN,  nivel == 0 ? HIGH : LOW);
  digitalWrite(LED_YELLOW, nivel == 1 ? HIGH : LOW);
  digitalWrite(LED_RED,    nivel == 2 ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, nivel == 2 ? HIGH : LOW);
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  Serial.println("\n========================================");
  Serial.println("   DRAGON CAPSULE — Sistema Iniciando  ");
  Serial.println("========================================");

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_GREEN,  HIGH);
  digitalWrite(LED_YELLOW, HIGH);
  digitalWrite(LED_RED,    HIGH);
  delay(600);
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED,    LOW);

  dht.begin();
  Serial.println("[INIT] DHT22 OK.");

  bmpOk = bmp.begin();
  if (bmpOk) {
    Serial.println("[INIT] BMP180 OK.");
  } else {
    Serial.println("[WARN] BMP180 nao encontrado — simulacao ativa.");
  }

  mpuOk = mpu.begin();
  if (mpuOk) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("[INIT] MPU6050 OK.");
  } else {
    Serial.println("[WARN] MPU6050 nao encontrado — simulacao ativa.");
  }

  Serial.println("----------------------------------------");
  Serial.println("  DHT22    → GPIO 15  (Temp + Umidade)");
  Serial.println("  BMP180   → I2C 0x77 (Pressao)");
  Serial.println("  MPU-6050 → I2C 0x68 (Aceleracao)");
  Serial.println("  Pot O2   → GPIO 34  (14-28 %)");
  Serial.println("  Pot Rad  → GPIO 35  (0-30 uSv/h)");
  Serial.println("----------------------------------------");

  setup_wifi();

  espClient.setInsecure();
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setBufferSize(1024);

  tentarReconnect();

  Serial.println("[INIT] Setup concluido.\n");
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {
  if (client.connected()) {
    client.loop();
  }

  static unsigned long ultimoEnvio     = 0;
  static unsigned long ultimoReconnect = 0;

  if (!client.connected()) {
    if (millis() - ultimoReconnect >= RECONNECT_INTERVALO) {
      ultimoReconnect = millis();
      tentarReconnect();
    }
  }

  if (millis() - ultimoEnvio < INTERVALO_MS) return;
  ultimoEnvio = millis();

  // ── Leituras ────────────────────────────────────────────────────
  float temperatura = dht.readTemperature();
  float umidade     = dht.readHumidity();
  if (isnan(temperatura) || isnan(umidade)) {
    temperatura = 22.0 + 3.0 * sin(millis() / 15000.0);
    umidade     = 48.0;
  }

  float pressao = bmpOk ? bmp.readPressure() / 100.0
                        : 1013.25 + 5.0 * sin(millis() / 25000.0);

  float oxigenio = 14.0 + (lerADC(POT_O2_PIN)  / 4095.0) * 14.0;
  float radiacao = 0.0  + (lerADC(POT_RAD_PIN) / 4095.0) * 30.0;

  float accelX = 0.01, accelY = 0.0, accelZ = 1.0, accelMag = 1.0;
  if (mpuOk) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    accelX   = a.acceleration.x / 9.81;
    accelY   = a.acceleration.y / 9.81;
    accelZ   = a.acceleration.z / 9.81;
    accelMag = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  }

  // ── Alerta ──────────────────────────────────────────────────────
  int alerta = avaliarAlerta(temperatura, umidade, pressao,
                              oxigenio, radiacao, accelMag);
  atualizarAlertas(alerta);
  const char* alertaStr = (alerta == 0) ? "OK"
                        : (alerta == 1) ? "WARNING" : "CRITICAL";

  // ── Serial ──────────────────────────────────────────────────────
  Serial.println("──────────────────────────────────────────");
  Serial.printf("[DRAGON] Temp: %.1f C | Umid: %.1f%% | Pres: %.1f hPa\n",
                temperatura, umidade, pressao);
  Serial.printf("[DRAGON] O2: %.1f%% | Rad: %.1f uSv/h | Accel: %.2f g\n",
                oxigenio, radiacao, accelMag);
  Serial.printf("[DRAGON] Status: %s | MQTT: %s\n",
                alertaStr, client.connected() ? "ON" : "OFF");

  // ── Publicações MQTT ────────────────────────────────────────────
  if (!client.connected()) {
    Serial.println("[MQTT] Offline — publicacao ignorada.");
    return;
  }

  char buf[128];

  snprintf(buf, sizeof(buf), "{\"temperatura\":%.2f}", temperatura);
  client.publish(TOPIC_TEMPERATURA, buf);

  snprintf(buf, sizeof(buf), "{\"umidade\":%.1f}", umidade);
  client.publish(TOPIC_UMIDADE, buf);

  snprintf(buf, sizeof(buf), "{\"pressao\":%.2f}", pressao);
  client.publish(TOPIC_PRESSAO, buf);

  snprintf(buf, sizeof(buf), "{\"oxigenio\":%.2f}", oxigenio);
  client.publish(TOPIC_OXIGENIO, buf);

  snprintf(buf, sizeof(buf), "{\"radiacao\":%.2f}", radiacao);
  client.publish(TOPIC_RADIACAO, buf);

  snprintf(buf, sizeof(buf),
           "{\"accel_x\":%.3f,\"accel_y\":%.3f,\"accel_z\":%.3f,\"magnitude\":%.3f}",
           accelX, accelY, accelZ, accelMag);
  client.publish(TOPIC_ACELERACAO, buf);

  snprintf(buf, sizeof(buf),
           "{\"nivel\":%d,\"descricao\":\"%s\"}", alerta, alertaStr);
  client.publish(TOPIC_STATUS, buf);

  // ── Payload completo ────────────────────────────────────────────
  StaticJsonDocument<768> doc;
  doc["device_id"] = "dragon-capsule-001";
  doc["mission"]   = "SpaceX-Dragon-ISS";
  doc["ts_ms"]     = millis();

  JsonObject env = doc.createNestedObject("environment");
  env["temperatura_c"] = round(temperatura * 100) / 100.0;
  env["umidade_pct"]   = round(umidade     * 10)  / 10.0;
  env["pressao_hpa"]   = round(pressao     * 100) / 100.0;

  JsonObject life = doc.createNestedObject("life_support");
  life["oxigenio_pct"]  = round(oxigenio * 100) / 100.0;
  life["radiacao_usvh"] = round(radiacao * 100) / 100.0;

  JsonObject mot = doc.createNestedObject("motion");
  mot["accel_x_g"]   = round(accelX   * 1000) / 1000.0;
  mot["accel_y_g"]   = round(accelY   * 1000) / 1000.0;
  mot["accel_z_g"]   = round(accelZ   * 1000) / 1000.0;
  mot["magnitude_g"] = round(accelMag * 1000) / 1000.0;

  JsonObject st = doc.createNestedObject("status");
  st["nivel"]     = alerta;
  st["descricao"] = alertaStr;

  char fullBuf[768];
  serializeJson(doc, fullBuf, sizeof(fullBuf));
  client.publish(TOPIC_FULL, fullBuf);
}