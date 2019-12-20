#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define LORA_SCK     5
#define LORA_MISO    19
#define LORA_MOSI    27
#define LORA_SS      18
#define LORA_RST     14
#define LORA_DI0     26

const char* ssid = "WNET";
const char* password =  "p@ulinh@p3drinh0";
const char* mqttServer = "serproiot.duckdns.org";
const int mqttPort = 1883;
const char* mqttUser = "ha";
const char* mqttPassword = "serprofla";

WiFiClient espClient;
PubSubClient client(espClient);

long lastSendTime = 0;        // TimeStamp da ultima mensagem enviada
String lastMsg;
byte lastIdMsg;
byte lastDestinAddress;
bool lastMsgConfirmed = true;
byte retry = 0;
int interval = 5000;          // Intervalo em ms no envio das mensagens (inicial 5s)

void LoraSendMessage(byte destinAddress, String outgoing, byte retry = 0) {
  if (lastMsgConfirmed || retry) {
    if (lastMsgConfirmed) {
      ++lastIdMsg;
      lastMsg = outgoing;
      lastDestinAddress = destinAddress;
      lastMsgConfirmed = false;
    } else {
      Serial.println("RETRY");
    }

    Serial.print("Enviando ");
    Serial.print(lastIdMsg);
    Serial.print(" - ");
    Serial.println(outgoing);


    LoRa.beginPacket();                   // Inicia o pacote da mensagem
    LoRa.write(destinAddress);            // Adiciona o endereco do destinatário
    LoRa.write(lastIdMsg);              // Contador da mensagem
    LoRa.write(false);                    // ACK
    LoRa.print(outgoing);                 // Vetor da mensagem
    LoRa.endPacket();                     // Finaliza o pacote e envia
  } else {
    Serial.println("Tem menságens não confirmadas");
  }
}

void LoraSendAck(byte sender, byte incomingMsgId) {
  Serial.print("Enviando ACK ");
  Serial.println(incomingMsgId);

  LoRa.beginPacket();                   // Inicia o pacote da mensagem
  LoRa.write(sender);             // Adiciona o endereco do remetente
  LoRa.write(incomingMsgId);                // Contador da mensagem
  LoRa.write(true);                     // ACK
  LoRa.endPacket();                     // Finaliza o pacote e envia
}

// Funcao para receber mensagem
void LoraReceive() {
  Serial.println("Mensagem Lora ");
  // Leu um pacote, vamos decodificar?
  byte sender = LoRa.read();            // Endereco do remetente
  byte incomingMsgId = LoRa.read();     // Id da Mensagem
  bool ack = LoRa.read();

  if (ack) {
    Serial.println("ACK ");
    if (lastIdMsg == incomingMsgId) {
      lastMsgConfirmed = true;
      retry = 0;

      //Publicar no tópico
      String topic = "lora/" + String(sender) + "/state";
      client.publish(topic.c_str() , lastMsg.c_str());

      Serial.print("ACK from ");
      Serial.print(sender);
      Serial.print(" msgId ");
      Serial.println(incomingMsgId);
    }


  } else {
    Serial.println("Mensagem Lora ");
    byte canal = LoRa.read();
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }

    String topic = "lora/" + String(sender) + "/" + canal + "/receive";
    client.publish(topic.c_str() , incoming.c_str());

    LoraSendAck(sender, incomingMsgId);
    Serial.println("Mensagem recebida: " + incoming);
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char* command = strtok(topic, "/");
  command = strtok(0, "/");

  String msg = "";
  for (int i = 0; i < length; i++)
    msg += (char)payload[i];

  LoraSendMessage(atoi(command), msg);
}


void reconnect() {
  reconnectWiFi();
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient", mqttUser, mqttPassword)) {
      Serial.println("connected");
      client.subscribe("lora/+/cmd");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again");
      delay(500);
    }
  }
  client.loop();
}

void reconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED)
    return;
  WiFi.begin(ssid, password); // Conecta na rede WI-FI
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
}

void setup() {
  // inicializacao da serial
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DI0);

  // Inicializa o radio LoRa em 915MHz e checa se esta ok!
  if (!LoRa.begin(915E6)) {
    Serial.println(" Erro ao iniciar modulo LoRa. Verifique a coenxao dos seus pinos!! ");
    while (true);
  }

  Serial.println(" Modulo LoRa Gateway!!!");

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqtt_callback);
}

void loop() {
  /*
    if (millis() - lastSendTime > interval) {
      String mensagem = "Hellora World Gateway!";    // Definicao da mensagem
      LoraSendMessage(0xAA, mensagem);
      lastSendTime = millis();            // Timestamp da ultima mensagem
    }
  */
  if (!lastMsgConfirmed && (retry < 6) && (millis() - lastSendTime > (random(400) + 100) )) {
    LoraSendMessage(lastDestinAddress, lastMsg, retry++);
    lastSendTime = millis();
    if (retry == 6) {
      retry = 0;
      lastMsgConfirmed = true;
    }
  }

  reconnect();

  if (LoRa.parsePacket())
    LoraReceive();
}
