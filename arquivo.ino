// --------------------------------------------------------------------------
// Projeto: Monitor de Distância – Edge Computing + FIWARE
// Dispositivo: sensor001
// Grupo ZettaWorks:
// Victor Altieri RM: 565288
// Pedro Henrique Silva Gregolini RM:563342
// Rafael Falaguasta RM: 561714
// --------------------------------------------------------------------------

#include <WiFi.h>
#include <PubSubClient.h>

// ------------------------ CONFIGURAÇÕES EDITÁVEIS --------------------------

const char* SSID         = "Wokwi-GUEST";
const char* PASSWORD     = "";

const char* BROKER_MQTT  = "20.220.33.84";
const int   BROKER_PORT  = 1883;

// Tópicos MQTT para FIWARE
const char* TOPICO_SUBSCRIBE = "/TEF/sensor001/cmd";     // Comandos
const char* TOPICO_PUBLISH_1 = "/TEF/sensor001/attrs";   // Estado (on/off)
const char* TOPICO_PUBLISH_2 = "/TEF/sensor001/attrs/d"; // Distância medida

const char* ID_MQTT     = "fiware_sensor001";
const char* topicPrefix = "sensor001";   // prefixo usado nos comandos

// LED de alerta local (EDGE) – LED onboard do ESP32
const int LED_PIN = 2;

// ---------------------------- SENSOR ULTRASSÔNICO --------------------------

const int trigPin = 5;
const int echoPin = 18;

// ----------------------------- VARIÁVEIS GLOBAIS ---------------------------

WiFiClient espClient;
PubSubClient MQTT(espClient);

char EstadoSaida = '0';   // '1' = on, '0' = off

// ---------------------------- INICIALIZAÇÕES --------------------------------

void initSerial() {
  Serial.begin(115200);
  delay(10);
}

void initWiFi() {
  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

void InitHardware() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Piscar LED para indicar inicialização (sem usar digitalWrite aninhado)
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(150);
    digitalWrite(LED_PIN, LOW);
    delay(150);
  }
}

// --------------------------- MQTT – CALLBACK -------------------------------

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg;

  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Comando recebido: ");
  Serial.println(msg);

  String onTopic  = String(topicPrefix) + "@on|";
  String offTopic = String(topicPrefix) + "@off|";

  if (msg.equals(onTopic)) {
    digitalWrite(LED_PIN, HIGH);
    EstadoSaida = '1';
  }

  if (msg.equals(offTopic)) {
    digitalWrite(LED_PIN, LOW);
    EstadoSaida = '0';
  }
}

// ------------------------ ULTRASSÔNICO – FUNÇÃO EDGE -----------------------

float medirDistancia() {
  // Gera pulso no TRIG
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Mede o tempo de ida e volta do som no ECHO
  long duracao = pulseIn(echoPin, HIGH, 30000); // timeout 30 ms

  if (duracao == 0) {
    return -1; // leitura inválida
  }

  // Converte tempo em distância (cm)
  float distancia = (duracao / 2.0) * 0.0343;
  return distancia;
}

void handleDistance() {
  float distancia = medirDistancia();

  if (distancia < 0) {
    Serial.println("Falha na leitura do sensor ultrassônico");
    return;
  }

  Serial.print("Distância: ");
  Serial.print(distancia);
  Serial.println(" cm");

  // ------------------ LÓGICA EDGE (alerta local) ------------------
  // Se estiver muito perto (< 25 cm), acende LED
  if (distancia < 75.0) {
    digitalWrite(LED_PIN, HIGH);
    EstadoSaida = '1';
  } else {
    digitalWrite(LED_PIN, LOW);
    EstadoSaida = '0';
  }

  // ---------------------- PUBLICAÇÃO NO FIWARE --------------------

  char mensagem[16];
  dtostrf(distancia, 0, 2, mensagem);  // float -> string com 2 casas decimais

  MQTT.publish(TOPICO_PUBLISH_2, mensagem);

  delay(1000);
}

// --------------------------- FUNÇÕES DE SUPORTE ----------------------------

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("Conectando ao Broker MQTT... ");

    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
    } else {
      Serial.println("Falha. Tentando novamente em 2s...");
      delay(2000);
    }
  }
}

void VerificaConexoesWiFIEMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();
  }

  if (!MQTT.connected()) {
    reconnectMQTT();
  }
}

void EnviaEstadoOutputMQTT() {
  if (EstadoSaida == '1') {
    MQTT.publish(TOPICO_PUBLISH_1, "s|on");
  } else {
    MQTT.publish(TOPICO_PUBLISH_1, "s|off");
  }

  delay(500);
}

// ------------------------------ SETUP & LOOP -------------------------------

void setup() {
  initSerial();
  InitHardware();
  initWiFi();
  initMQTT();
  delay(2000);

  // Publica estado inicial
  MQTT.publish(TOPICO_PUBLISH_1, "s|off");
}

void loop() {
  VerificaConexoesWiFIEMQTT();
  EnviaEstadoOutputMQTT();
  handleDistance();
  MQTT.loop();
}
