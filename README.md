# QualyAir-IoT2025
Trabalho Objetos Inteligentes Conectados - Mackenzie
#  QualyAir: Sistema Autônomo de Monitoramento e Purificação do Ar (IoT)

##  Visão Geral
Este projeto implementa um sistema de Internet das Coisas (IoT) de baixo custo, alinhado ao ODS 3 (Saúde e Bem-Estar), que monitora a qualidade do ar interno (partículas PM2.5 e Umidade) e aciona um purificador de ar de forma autônoma.

O sistema utiliza a conectividade Wi-Fi nativa do ESP32 e o protocolo MQTT para comunicação em tempo real, permitindo o monitoramento remoto e a ação corretiva imediata.

##  Hardware Utilizado (iii)
| Componente | Plataforma | Função |
| :--- | :--- | :--- |
| **Plataforma Principal** | ESP32 DevKitC V4 | Processamento, lógica e conectividade TCP/IP/Wi-Fi. |
| **Sensor Primário** | Sensor de Partículas SDS011 | Medição de PM2.5 (Gatilho de Acionamento). |
| **Sensor Secundário** | Sensor BME680 | Medição de Umidade (Gatilho Secundário) e COV. |
| **Atuador de Controle** | Módulo Relé de 5V | Isola o circuito e chaveia o purificador. |
| **Atuador Final** | Ventilador + Filtro HEPA | Purificação física do ar. |

##  Como Funciona 
O ESP32 lê os sensores. Se o nível de PM2.5 **OU** o nível de Umidade ultrapassar o limiar de segurança (lógica 'OU'), o ESP32 envia um sinal ALTO ao GPIO 23, que ativa o Módulo Relé. O sistema se comunica continuamente com a nuvem via MQTT, publicando dados e status.

##  Módulo de Comunicação e Protocolo
* **Módulo:** O módulo de comunicação é o chip **Wi-Fi integrado** do ESP32 (TCP/IP).
* **Protocolo:** **MQTT (Message Queuing Telemetry Transport)**, utilizado para garantir uma comunicação leve, eficiente e assíncrona com o Broker (servidor).
* **Fluxo IoT:** O ESP32 atua como **Publisher** (enviando leituras e status) e **Subscriber** (pronto para receber comandos remotos, como "DESLIGAR_MANUAL").

---

## 🔧 Guia de Instalação e Uso 

1.  **Pré-requisitos:** Instalar o Arduino IDE e as placas ESP32. Instalar as bibliotecas: `PubSubClient`, `DHT sensor library`, e bibliotecas específicas para `SDS011` e `BME680`.
2.  **Configuração de Rede:** Preencha as credenciais de sua rede Wi-Fi e o endereço do Broker MQTT no arquivo `qualvair_firmware/secrets.h`.
3.  **Upload:** Carregue o código `qualyAir_firmware.ino` no ESP32.
4.  **Uso:** O sistema iniciará o ciclo autônomo. Monitore os dados e o status no seu dashboard MQTT.
