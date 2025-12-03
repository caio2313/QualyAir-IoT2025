#include <WiFi.h>
#include <PubSubClient.h> 
#include "DHT.h"         

// --- 1. CONFIGURAÇÕES DE HARDWARE ---
#define DHTPIN 27            
#define DHTTYPE DHT22        
#define ACTUATOR_PIN 13      
#define GAS_ANALOG_PIN 34    

// Limites de Acionamento (Adaptados para a Simulação)
const int LIMIT_TEMPERATURA = 28;    
const int LIMIT_UMIDADE_ALERTA = 70;  
const int LIMIT_GAS_ANALOG_BAIXO_ALERTA = 700; 

// --- 2. CONFIGURAÇÕES DE REDE/MQTT ---
const char* ssid = "Wokwi-GUEST"; 
const char* password = "";       
const char* mqtt_server = "broker.hivemq.com";
const char* topic_publish = "qualyair/dados/simulacao_final";

// Inicialização de objetos
WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

// --- 3. FUNÇÕES DE CONEXÃO ---

void setup_wifi() {
  Serial.begin(115200);
  Serial.println("\nIniciando conexão Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT (HiveMQ)...");
    String clientId = "ESP32_QualyAir_";
    clientId += String(random(0xffff), HEX);
    // Conexão ao Broker HiveMQ
    if (client.connect("espClient")) { 
      Serial.println("Conectado!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos.");
      delay(5000);
    }
  }
}

// --- 4. SETUP E LOOP PRINCIPAL ---

void setup() {
  pinMode(ACTUATOR_PIN, OUTPUT);
  digitalWrite(ACTUATOR_PIN, LOW); // Garante que o relé comece desligado
  
  dht.begin();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantém a conexão MQTT viva

  // Lógica principal de Leitura e Atuação a cada 5 segundos
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();

    // 1. Leitura dos Sensores
    float t = dht.readTemperature(); 
    float h = dht.readHumidity();    
    int gas_raw = analogRead(GAS_ANALOG_PIN); // Leitura analógica do MQ-2 (0-4095)
    
    if (isnan(t) || isnan(h)) {
      Serial.println("Erro ao ler o sensor DHT!");
      return;
    }

    // 2. Lógica de Atuação (Regra OR: Temp OU Umidade OU Gás)
    
    // Alerta se a leitura analógica cair abaixo do limite (significa muito gás)
    bool alerta_gas = (gas_raw < LIMIT_GAS_ANALOG_BAIXO_ALERTA); 
    bool alerta_temp_poluicao = (t > LIMIT_TEMPERATURA);
    bool alerta_umidade = (h > LIMIT_UMIDADE_ALERTA);

    bool alerta_qualquer = alerta_temp_poluicao || alerta_umidade || alerta_gas;

    String actuator_state = "DESLIGADO";
    String alert_message = "OK";
    
    if (alerta_qualquer) {
        // LIGA o purificador
        digitalWrite(ACTUATOR_PIN, HIGH); 
        actuator_state = "LIGADO";
        
        if (alerta_gas) alert_message = "GAS_ALTO";
        else if (alerta_temp_poluicao) alert_message = "TEMP_PROXY_ALTA";
        else if (alerta_umidade) alert_message = "UMIDADE_ALTA";
        
        Serial.print("ALERTA: ");
        Serial.print(alert_message);
        Serial.print(" Purificador ");
        Serial.println(actuator_state);
        
    } else if (t < (LIMIT_TEMPERATURA - 2) && h < LIMIT_UMIDADE_ALERTA && gas_raw > (LIMIT_GAS_ANALOG_BAIXO_ALERTA)) { 
        // Desliga SÓ SE TODOS os principais fatores estiverem em nível seguro
        digitalWrite(ACTUATOR_PIN, LOW);   
        actuator_state = "DESLIGADO";
        Serial.println("Ambiente normal. Purificador DESLIGADO.");
    }
    
    // 3. Envio de Dados via MQTT (Publicação para HiveMQ)
    String payload = "{\"temp\":" + String(t, 2) + 
                     ",\"umidade\":" + String(h, 2) + 
                     ",\"gas_raw\":" + String(gas_raw) + 
                     ",\"alerta\":\"" + alert_message + 
                     "\",\"status_purificador\":\"" + actuator_state + "\"}";
    
    client.publish(topic_publish, payload.c_str());
    
    Serial.print("Dados enviados via MQTT: ");
    Serial.println(payload);
  }
}
