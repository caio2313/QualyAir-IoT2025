#include <WiFi.h>
#include <PubSubClient.h> 

// Incluir as bibliotecas reais dos sensores
#include <SoftwareSerial.h>      // Necessário para o SDS011 via Serial
#include <Adafruit_BME680.h>     // Para o BME680
#include <Adafruit_Sensor.h>

// --- 1. CONFIGURAÇÕES DE HARDWARE E PINAGEM ---
// SDS011 (UART2) - Conexões definitivas do seu Fritzing
#define SDS011_RX_PIN 16 // Conectado ao TX do SDS011 (Fio Laranja)
#define SDS011_TX_PIN 17 // Conectado ao RX do SDS011 (Fio Amarelo)

// BME680 (I2C) - Pinos padrão do ESP32
#define I2C_SDA_PIN 21 
#define I2C_SCL_PIN 22 

// Atuador (Relé)
#define ACTUATOR_PIN 23  // GPIO que controla o Módulo Relé (Fio Verde)

// Limites de Acionamento (Baseado em Recomendações de Saúde)
const float LIMIT_PM25_ALERTA = 50.0;    // Se PM2.5 > 50 µg/m³ (Insatisfatório - OMS)
const float LIMIT_UMIDADE_ALERTA = 70.0; // Se Umidade > 70% (Risco de Mofo)
const float LIMIT_IAQ_ALERTA = 150.0;    // IAQ Index do BME680 (Valor a ser calibrado para COV/Qualidade do Ar)

// --- 2. CONFIGURAÇÕES DE REDE/MQTT ---
const char* ssid = "SUA_REDE_WIFI";       // Substituir pela sua rede real
const char* password = "SUA_SENHA_WIFI";   // Substituir pela sua senha real
const char* mqtt_server = "broker.hivemq.com"; 
const char* topic_publish = "qualyair/dados/reais";

// Inicialização de objetos e sensores
WiFiClient espClient;
PubSubClient client(espClient);

// Objetos SDS011
SoftwareSerial sdsSerial(SDS011_RX_PIN, SDS011_TX_PIN);
// Nota: SDS011 library object initialization depends on the specific library used.
// For simplicity here, we assume direct reading from the SoftwareSerial object.

// Objeto BME680
Adafruit_BME680 bme680(I2C_SCL_PIN, I2C_SDA_PIN); // Construtor com pinos I2C

// Estrutura para armazenar dados do SDS011
struct SDS011_Data {
  float pm25;
  float pm10;
};

// --- 3. FUNÇÕES ---

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
  // Lógica de reconexão MQTT (igual ao código de simulação)
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    String clientId = "ESP32_QualyAir_";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado!");
      // client.subscribe("qualyair/comando"); // Assinatura para controle remoto
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos.");
      delay(5000);
    }
  }
}

// *** FUNÇÃO PARA LER SDS011 (Adapte dependendo da biblioteca real que você usar) ***
SDS011_Data read_sds011() {
  SDS011_Data data = {0.0, 0.0};
  // Este é um pseudo-código para leitura, usando a SoftwareSerial
  // O código real envolveria verificar bytes de cabeçalho e checksum da UART
  /*
  if (sdsSerial.available() >= 10) {
    // Implementar lógica de leitura do frame de dados aqui
    // data.pm25 = ...
  }
  */
  // **Atenção: Para testes, use valores fixos até implementar a biblioteca específica.**
  // EXEMPLO DE TESTE:
  data.pm25 = 40.0; // Assume PM2.5 OK
  data.pm10 = 50.0;
  return data;
}
// ********************************************************************************

void setup() {
  pinMode(ACTUATOR_PIN, OUTPUT);
  digitalWrite(ACTUATOR_PIN, LOW); // Inicia desligado

  // Iniciar comunicação Serial para SDS011
  sdsSerial.begin(9600); 

  // Iniciar BME680
  if (!bme680.begin()) {
    Serial.println("Falha ao encontrar o sensor BME680! Verifique as conexões I2C.");
  }
  
  // Configurações do BME680 (Ajuste para otimizar COV/IAQ)
  bme680.setGasHeater(320, 150); // 320C por 150ms

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop(); // Mantém a conexão MQTT viva

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();

    // 1. Leitura dos Sensores REAIS
    
    // Leitura BME680
    bme680.performReading();
    float umidade = bme680.humidity;
    float gas_resistencia = bme680.gas_resistance / 1000.0; // Em Kilo-ohms (proxy para COV)

    // Leitura SDS011
    SDS011_Data sds_data = read_sds011();
    float pm25 = sds_data.pm25;
    
    if (pm25 == 0.0) { // Verifica se a leitura falhou (dependente da implementação do read_sds011)
      Serial.println("Erro na leitura do PM2.5. Pulando ciclo de atuacao.");
      return;
    }
    
    // 2. Lógica de Atuação
    
    // Alerta se: PM2.5 Alto OU Umidade Alta OU COV Alto (usando a resistência como proxy)
    bool alerta_pm25 = (pm25 > LIMIT_PM25_ALERTA);
    bool alerta_umidade = (umidade > LIMIT_UMIDADE_ALERTA);
    // Nota: Resistência baixa -> MAIS gás. Ajuste este limiar após calibrar o sensor BME680.
    bool alerta_cov_gas = (gas_resistencia < 500); // Exemplo de um limite de 500 KOhms

    bool alerta_qualquer = alerta_pm25 || alerta_umidade || alerta_cov_gas;

    String actuator_state = "DESLIGADO";
    String alert_message = "OK";
    
    if (alerta_qualquer) {
        // LIGA o purificador
        digitalWrite(ACTUATOR_PIN, HIGH); 
        actuator_state = "LIGADO";
        
        if (alerta_pm25) alert_message = "PM2.5_ALTO";
        else if (alerta_umidade) alert_message = "UMIDADE_ALTA";
        else if (alerta_cov_gas) alert_message = "GAS_ALTO";
        
        Serial.print("ALERTA: ");
        Serial.print(alert_message);
        Serial.print(" Purificador ");
        Serial.println(actuator_state);
        
    } else if (pm25 < (LIMIT_PM25_ALERTA * 0.5) && umidade < LIMIT_UMIDADE_ALERTA) { 
        // Desliga SÓ SE os principais fatores estiverem em nível seguro
        digitalWrite(ACTUATOR_PIN, LOW);   
        actuator_state = "DESLIGADO";
        Serial.println("Ambiente normal. Purificador DESLIGADO.");
    }
    
    // 3. Envio de Dados via MQTT (JSON Format)
    String payload = "{\"pm25\":" + String(pm25, 2) + 
                     ",\"umidade\":" + String(umidade, 2) + 
                     ",\"cov_kohm\":" + String(gas_resistencia, 2) + 
                     ",\"alerta\":\"" + alert_message + 
                     "\",\"status_purificador\":\"" + actuator_state + "\"}";
    
    client.publish(topic_publish, payload.c_str());
    
    Serial.print("Dados enviados via MQTT: ");
    Serial.println(payload);
  }
}
