#include <WiFi.h>
#include <FirebaseESP32.h>
#include <time.h>

// Wi-Fi credentials
//const char* ssid = "Mobitel K";
//const char* password = "44444444";

const char* ssid = "Galaxy A13 00F4";
const char* password = "elfv3871";

// Firebase credentials
#define FIREBASE_HOST "https://aquasmatters-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "BHJpj0D088Di3l6du8fcZrPnitmY9s7JEZ1QoXvI"

FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

// Sensor pins
#define TRIG_PIN 13
#define ECHO_PIN 12
#define TURBIDITY_PIN 35
#define RELAY_PIN 23
#define FLOW_PIN 4  
#define TDS_PIN 34

// Flow sensor variables
const float CALIBRATION_FACTOR = 7.5;
volatile int pulseCount = 0;
float flowRate = 0;
unsigned long lastTime = 0;
unsigned long lastPulseTime = 0;

// Global variables
float waterLevel;
int turbidity;
float tds;
bool valveStatus;

// Interrupt for flow sensor
void IRAM_ATTR pulseCounter() {
    unsigned long currentTime = millis();
    if (currentTime - lastPulseTime > 10) {
        pulseCount++;
        lastPulseTime = currentTime;
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to Wi-Fi");

    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(FLOW_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
    
    digitalWrite(RELAY_PIN, LOW); // Ensure valve is closed initially
    lastTime = millis();
}

void loop() {
    waterLevel = readWaterLevel();
    turbidity = readTurbidity();
    tds = readTDS();
    flowRate = readFlowRate();

    Serial.print("Water Level: "); Serial.println(waterLevel);
    Serial.print("Turbidity: "); Serial.println(turbidity);
    Serial.print("TDS: "); Serial.println(tds);
    Serial.print("Flow Rate: "); Serial.println(flowRate);
    
  valveStatus = getValveStatus();  // Get the valve status from Firebase
  
  if (valveStatus) {
    digitalWrite(RELAY_PIN, HIGH);  // Turn the relay ON (valve open)
  } else {
  digitalWrite(RELAY_PIN, LOW);   // Turn the relay OFF (valve closed)
  }
    //digitalWrite(RELAY_PIN, valveStatus ? HIGH : LOW);
    
    sendSensorData(waterLevel, turbidity, tds, flowRate);
    delay(5000);
}

int readWaterLevel() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    unsigned long duration = pulseIn(ECHO_PIN, HIGH);
    return duration == 0 ? -1 : 100 - map(duration / 29 / 2, 0, 100, 0, 100);
}

int readTurbidity() {
    return map(analogRead(TURBIDITY_PIN), 0, 4095, 0, 100);
}

float readTDS() {
    int rawADC = 0;
    for (int i = 0; i < 10; i++) {
        rawADC += analogRead(TDS_PIN);
        delay(10);
    }
    rawADC /= 10;
    float voltage = rawADC * (3.3 / 4095.0);
    return (133.42 * pow(voltage, 3)) - (255.86 * pow(voltage, 2)) + (857.39 * voltage);
}

float readFlowRate() {
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= 1000) {
        flowRate = (pulseCount / CALIBRATION_FACTOR) / ((currentTime - lastTime) / 1000.0);
        pulseCount = 0;
        lastTime = currentTime;
    }
    return flowRate;
}

bool getValveStatus() {
  // Attempt to read the valve status from Firebase
  if (Firebase.getBool(firebaseData, "/control/valveStatus")) {
    // If the read operation is successful, get the valve status
    bool valveStatus = firebaseData.boolData();

    // Print the valve status to the Serial Monitor
    if (valveStatus) {
      Serial.println("Valve status: OPEN");
    } else {
      Serial.println("Valve status: CLOSED");
    }

    // Return the valve status
    return valveStatus;
  } else {
    // If the read operation fails, print an error message and return a default value (e.g., false)
    Serial.println("Failed to read valve status from Firebase");
    Serial.println(firebaseData.errorReason());  // Print the error reason
    return false;  // Default to closed if reading fails
  }
}

void sendSensorData(float waterLevel, int turbidity, float tds, float flowRate) {
    FirebaseJson json;
    json.set("waterLevel", waterLevel);
    json.set("turbidity", turbidity);
    json.set("tds", tds);
    json.set("flowRate", flowRate);
    json.set("timestamp", getCurrentTimestamp());
    Firebase.setJSON(firebaseData, "/sensorData", json);
}

String getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm *timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    return String(buffer);
}
