#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "DHT.h"
#include <ArduinoJson.h>

// --- Configuración WiFi y MQTT ---
const char* ssid = "iPhone de Lucas";          
const char* password = "12345678";  
const char* mqtt_server = "172.20.10.5"; 

WiFiClient espClient;
PubSubClient client(espClient);

// --- Configuración DHT11 ---
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Configuración HC-SR04 ---
const int trigPin = 5;
const int echoPin = 18;

// --- Configuración GY-906 ---
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// --- Configuración MAX4466 y LED ---
const int micPin = 34;
const int ventanaMuestreo = 50;
const int ledPin = 2; // Pin del LED (el pin 2 suele ser el LED integrado de la placa)
const int umbralRuido = 1500; // Valor 'x' a superar para encender el LED (Ajustable)
const int ruidoElectricoFondo = 500; // Todo lo que esté por debajo de esto se considerará silencio

// --- Variables Globales de Sensores ---
float humedad = 0;
float tempAmbiente = 0;
float tempObjeto = 0;
float distancia = 0;
int amplitud = 0;

// --- Temporizadores Asíncronos ---
unsigned long ultimoEnvioRapido = 0;
unsigned long ultimoEnvioLento = 0;
unsigned long ultimoDisparoCamara = 0; 
const long intervaloRapido = 300;  
const long intervaloLento = 2000;   
const long cooldownCamara = 5000;      

void setup_wifi() {
  delay(10);
  Serial.println("\nConectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    String clientId = "ESP32Client-Eq06-";
    clientId += String(random(0, 1000));
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado al broker mosquitto");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);

  dht.begin();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  // LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  if (!mlx.begin()) {
    Serial.println("Error conectando con GY-906!");
    while (1); 
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long tiempoActual = millis();

  // ==========================================
  // 1. TAREAS RÁPIDAS (Distancia y Ruido) - Cada 300ms
  // ==========================================
  if (tiempoActual - ultimoEnvioRapido >= intervaloRapido) {
    ultimoEnvioRapido = tiempoActual;

    // Lectura HC-SR04 (Distancia)
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duracion = pulseIn(echoPin, HIGH);
    distancia = (duracion * 0.0343) / 2;

    // Lectura MAX4466 (Ruido)
    unsigned long inicioMic = millis();
    int maximo = 0;
    int minimo = 4095;
    while (millis() - inicioMic < ventanaMuestreo) {
      int lectura = analogRead(micPin);
      if (lectura < 4095) { 
        if (lectura > maximo) maximo = lectura;
        if (lectura < minimo) minimo = lectura;
      }
    }
    
    // Cálculo de amplitud con filtro de ruido eléctrico
    int amplitudCruda = maximo - minimo;
    if (amplitudCruda < ruidoElectricoFondo) {
      amplitud = 0; // Silencio absoluto
    } else {
      amplitud = amplitudCruda; // Sonido real detectado
    }

    // --- LÓGICA DEL ACTUADOR (LED) ---
    if (amplitud > umbralRuido) {
      digitalWrite(ledPin, HIGH); // Enciende el LED si hay ruido fuerte
    } else {
      digitalWrite(ledPin, LOW);  // Lo apaga si el ruido baja
    }

    // Publicación inmediata de variables rápidas
    client.publish("smarthome/equipo06/distancia", String(distancia).c_str());
    client.publish("smarthome/equipo06/ruido", String(amplitud).c_str());

    // ==========================================
    // GATILLO AUTOMÁTICO DE LA CÁMARA
    // ==========================================
    if (distancia > 0 && distancia < 80) {
      if (tiempoActual - ultimoDisparoCamara >= cooldownCamara) {
        ultimoDisparoCamara = tiempoActual; 
        client.publish("smarthome/equipo06/trigger_automatico", "DISPARAR");
        Serial.print("¡Persona detectada a ");
        Serial.print(distancia);
        Serial.println(" cm! Orden de foto enviada a Node-RED.");
      }
    }
  }

  // ==========================================
  // 2. TAREAS LENTAS (Temperaturas e Informe JSON) - Cada 2000ms
  // ==========================================
  if (tiempoActual - ultimoEnvioLento >= intervaloLento) {
    ultimoEnvioLento = tiempoActual;

    humedad = dht.readHumidity();
    tempAmbiente = dht.readTemperature();
    tempObjeto = mlx.readObjectTempC();

    if (!isnan(humedad) && !isnan(tempAmbiente)) {
      client.publish("smarthome/equipo06/temperatura", String(tempAmbiente).c_str());
      client.publish("smarthome/equipo06/humedad", String(humedad).c_str());
    }
    if (!isnan(tempObjeto)) {
      client.publish("smarthome/equipo06/temp_objeto", String(tempObjeto).c_str());
    }

    StaticJsonDocument<256> doc;
    doc["equipo"] = "equipo06";
    if (!isnan(tempAmbiente)) doc["temperatura"] = tempAmbiente;
    if (!isnan(humedad)) doc["humedad"] = humedad;
    doc["ruido"] = amplitud;
    doc["distancia"] = distancia;
    doc["temp_objeto"] = tempObjeto;

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);
    client.publish("smarthome/equipo06/datos_json", jsonBuffer);
    
    Serial.print("JSON Enviado: ");
    Serial.println(jsonBuffer);
  }
}