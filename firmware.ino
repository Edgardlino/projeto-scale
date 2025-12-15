/*
 * ---------------------------------------------------------
 * PROJETO: Balança IoT (Versão FINAL - Safe Boot)
 * HARDWARE: ESP32-C6 + HX711 + Botão (Pino 0)
 * LÓGICA: Segurar 3s para Ligar / Segurar 3s para Desligar
 * ---------------------------------------------------------
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include "HX711.h"
#include "driver/gpio.h"

// --- UUIDs ---
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_DATA_UUID         "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_CMD_UUID          "129e7192-36e1-4688-b7f5-ea07361b26a8"

// --- PINAGEM ---
#define LOADCELL_DOUT_PIN 6
#define LOADCELL_SCK_PIN  7
#define HX711_POWER_PIN   3
#define LED_STATUS_PIN    15
#define LED_RGB_PIN       8
#define BATTERY_PIN       2
#define BUTTON_PIN        0   // Botão no Pino 0

// --- OBJETOS ---
HX711 scale;
Preferences preferences;
BLEServer* pServer = NULL;
BLECharacteristic* pCharData = NULL;
BLECharacteristic* pCharCmd = NULL;

// --- VARIÁVEIS ---
bool deviceConnected = false;
bool oldDeviceConnected = false;
float calibrationFactor = 0.0;
float smoothedWeight = 0;
int batteryPct = 0;
unsigned long lastActivityTime = 0;
unsigned long buttonPressStart = 0;

// Configurações
#define FILTER_ALPHA 0.1
#define ZERO_TRACKING 5.0 
#define SLEEP_TIMEOUT_MS 60000 

// --- CALLBACKS BLE ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(LED_STATUS_PIN, HIGH);
      lastActivityTime = millis();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(LED_STATUS_PIN, LOW); delay(100); digitalWrite(LED_STATUS_PIN, HIGH);
      pServer->getAdvertising()->start();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue(); 
      if (value.length() > 0) {
        String cmd = value;
        if (cmd == "TARE") { scale.tare(); smoothedWeight = 0; }
        if (cmd.startsWith("CAL:")) {
          float knownWeight = cmd.substring(4).toFloat();
          if (knownWeight > 0) {
            float raw = scale.get_units(10);
            float newFactor = scale.get_scale() * (raw / knownWeight);
            scale.set_scale(newFactor);
            preferences.putFloat("calFactor", newFactor);
          }
        }
        lastActivityTime = millis();
      }
    }
};

// --- LEITURA BATERIA ---
void lerBateria() {
  uint32_t raw = analogRead(BATTERY_PIN);
  static float smoothBatRaw = 0; 
  if (smoothBatRaw == 0) smoothBatRaw = raw;
  smoothBatRaw = (raw * 0.05) + (smoothBatRaw * 0.95);
  float voltage = (smoothBatRaw / 4095.0) * 3.3 * 2.0 * 1.24; 
  int pct = map((int)(voltage * 100), 300, 415, 0, 100);
  batteryPct = constrain(pct, 0, 100);
}

// --- SONO PROFUNDO ---
void entrarEmSonoProfundo() {
  Serial.println("Entrando em Deep Sleep...");
  
  digitalWrite(HX711_POWER_PIN, LOW);
  gpio_hold_en((gpio_num_t)HX711_POWER_PIN);
  scale.power_down();
  
  // Feedback "Tchau"
  for(int i=0; i<3; i++) {
    digitalWrite(LED_STATUS_PIN, LOW); delay(100);
    digitalWrite(LED_STATUS_PIN, HIGH); delay(100);
  }
  digitalWrite(LED_STATUS_PIN, LOW); 
  gpio_hold_en((gpio_num_t)LED_STATUS_PIN); 
  
  // Configuração de Wakeup
  gpio_set_direction((gpio_num_t)BUTTON_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en((gpio_num_t)BUTTON_PIN);
  gpio_hold_en((gpio_num_t)BUTTON_PIN);
  
  // Wakeup via EXT1 (Melhor para C6)
  esp_sleep_enable_ext1_wakeup(1ULL << BUTTON_PIN, ESP_EXT1_WAKEUP_ANY_LOW);
  
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);

  // 1. Destravar pinos
  gpio_hold_dis((gpio_num_t)LED_STATUS_PIN);
  gpio_hold_dis((gpio_num_t)HX711_POWER_PIN);
  gpio_hold_dis((gpio_num_t)BUTTON_PIN);

  pinMode(LED_STATUS_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // --- LÓGICA DE "LIGAR SEGURO" (3 SEGUNDOS) ---
  // Verifica se acordou pelo botão
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 || wakeup_reason == ESP_SLEEP_WAKEUP_GPIO) {
    // Se acordou pelo botão, vamos conferir se o usuário SEGURUOU
    unsigned long wakeStart = millis();
    bool ligarConfirmado = false;

    // Fica no loop enquanto o botão estiver APERTADO (LOW)
    while (digitalRead(BUTTON_PIN) == LOW) {
      
      // Se segurou por 3000ms (3s)
      if (millis() - wakeStart > 3000) {
        ligarConfirmado = true;
        break; // Sai do loop e liga
      }
      delay(10);
    }

    // Se soltou antes de 3s e não confirmou
    if (!ligarConfirmado) {
      // Volta a dormir imediatamente (falso positivo)
      entrarEmSonoProfundo(); 
    }
  }
  // ------------------------------------------------

  // SE CHEGOU AQUI, LIGOU MESMO!
  digitalWrite(LED_STATUS_PIN, HIGH); // Acende fixo para mostrar que ligou
  
  pinMode(HX711_POWER_PIN, OUTPUT); digitalWrite(HX711_POWER_PIN, HIGH); delay(500);
  pinMode(LED_RGB_PIN, OUTPUT); digitalWrite(LED_RGB_PIN, LOW);
  pinMode(BATTERY_PIN, INPUT); analogReadResolution(12);
  
  preferences.begin("balanca_ble", false);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (scale.wait_ready_timeout(2000)) {
    scale.read_average(10);
    scale.tare();
    calibrationFactor = preferences.getFloat("calFactor", 400.0);
    scale.set_scale(calibrationFactor);
  }

  BLEDevice::init("Projeto Scale"); 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharData = pService->createCharacteristic(CHAR_DATA_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharData->addDescriptor(new BLE2902());
  pCharCmd = pService->createCharacteristic(CHAR_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharCmd->setCallbacks(new MyCallbacks());

  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0); 
  BLEDevice::startAdvertising();
  
  lastActivityTime = millis();
}

void loop() {
  // --- LÓGICA DE DESLIGAR (3 SEGUNDOS) ---
  if (digitalRead(BUTTON_PIN) == LOW) { 
    if (buttonPressStart == 0) buttonPressStart = millis();
    if (millis() - buttonPressStart > 3000) {
      entrarEmSonoProfundo(); // Desliga
    }
  } else {
    buttonPressStart = 0; 
  }

  // Sono por inatividade
  if (!deviceConnected && (millis() - lastActivityTime > SLEEP_TIMEOUT_MS)) {
    entrarEmSonoProfundo();
  }

  if (scale.is_ready()) {
    float rawReading = scale.get_units(5); 
    float diff = abs(rawReading - smoothedWeight);
    float dynamicAlpha;
    if (diff > 1.0) dynamicAlpha = 0.7; 
    else dynamicAlpha = 0.05; 
    
    smoothedWeight = (dynamicAlpha * rawReading) + ((1.0 - dynamicAlpha) * smoothedWeight);
    if (abs(smoothedWeight) < ZERO_TRACKING) smoothedWeight = 0.0;
  }

  if (deviceConnected) {
    lerBateria(); 
    String payload = String(smoothedWeight, 1) + "|" + String(batteryPct);
    pCharData->setValue(payload);
    pCharData->notify(); 
    delay(300); 
    lastActivityTime = millis(); 
  }
  
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); pServer->startAdvertising(); oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}
