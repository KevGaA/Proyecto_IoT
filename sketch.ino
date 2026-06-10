#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include "DHT.h"
#include <ArduinoJson.h>

// RED WiFi y MQTT
const char* ssid = "iPhone de Lucas";          
const char* password = "12345678";  
const char* mqtt_server = "172.20.10.5"; 

WiFiClient espClient;
PubSubClient client(espClient);

// CONF DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// CONF HC-SR04
const int trigPin = 5;
const int echoPin = 18;

// CONF GY906
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// CONF MAX4466
const int micPin = 34;
const int ventanaMuestreo = 50;
const int ledPin = 2; 
const int umbralRuido = 1500; 
const int ruidoElectricoFondo = 0; 

//GLOBALES
float humedad = 0;
float tempAmbiente = 0;
float tempObjeto = 0;
float distancia = 0;
int amplitud = 0;

// TEMPORIZADORES
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
  Serial.println("\nWiFi conectado");
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

  // DISTANCIA Y RUIDO
  if (tiempoActual - ultimoEnvioRapido >= intervaloRapido) {
    ultimoEnvioRapido = tiempoActual;

    // LEE HC-SR04
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duracion = pulseIn(echoPin, HIGH);
    distancia = (duracion * 0.0343) / 2;

    // LEE MAX4466 con filtro de ruido eléctrico
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
    
    // CALCULO DE AMPLITUD CON FILTRO DE RUIDO
    int amplitudCruda = maximo - minimo;
    if (amplitudCruda < ruidoElectricoFondo) {
      amplitud = 0; 
    } else {
      amplitud = amplitudCruda; 
    }

    // ACTUADOR LED
    if (amplitud > umbralRuido) {
      digitalWrite(ledPin, HIGH); 
    } else {
      digitalWrite(ledPin, LOW);  
    }

  
    client.publish("smarthome/equipo06/distancia", String(distancia).c_str());
    client.publish("smarthome/equipo06/ruido", String(amplitud).c_str());

    // TRIGGER DE CÁMARA SI SE DETECTA MOVIMIENTO CERCA
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

  // TEMPERATURA Y AMBIENTE
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