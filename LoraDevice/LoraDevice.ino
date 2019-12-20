// Comunicação LoRa com Arduino
// Definicao das bibliotecas a serem utilizadas no projeto
#include <SPI.h>
#include <LoRa.h>
#include "DHT.h"

#define LORA_SCK     5
#define LORA_MISO    19
#define LORA_MOSI    27
#define LORA_SS      18
#define LORA_RST     14
#define LORA_DI0     26

#define localAddress 0xAA     // Endereco deste dispositivo LoRa

DHT dht(13, DHT22);

long lastSendTime = 0;        // TimeStamp da ultima mensagem enviada
String lastMsg;
byte lastIdMsg;
bool lastMsgConfirmed = true;
byte retry = 0;
int interval = 5000;          // Intervalo em ms no envio das mensagens (inicial 5s)

void LoraSendMessage(String outgoing, byte canal, byte retry = 0) {
  if (lastMsgConfirmed || retry) {
    if (lastMsgConfirmed) {
      ++lastIdMsg;
      lastMsg = outgoing;
      lastMsgConfirmed = false;
    } else {
      Serial.println("RETRY");
    }

    Serial.print("Enviando ");
    Serial.print(lastIdMsg);
    Serial.print(" - ");
    Serial.println(outgoing);

    lastMsg = outgoing;
    lastMsgConfirmed = false;

    LoRa.beginPacket();                   // Inicia o pacote da mensagem
    LoRa.write(localAddress);             // Adiciona o endereco do remetente
    LoRa.write(lastIdMsg);              // Contador da mensagem
    LoRa.write(false);                    // ACK
    LoRa.write(canal);              // Contador da mensagem
    LoRa.print(outgoing);                 // Vetor da mensagem
    LoRa.endPacket();                     // Finaliza o pacote e envia
  }
}

void LoraSendAck(byte incomingMsgId) {
  LoRa.beginPacket();                   // Inicia o pacote da mensagem
  LoRa.write(localAddress);             // Adiciona o endereco do remetente
  LoRa.write(incomingMsgId);            // Contador da mensagem
  LoRa.write(true);                     // ACK
  LoRa.endPacket();                     // Finaliza o pacote e envia
}

void LoraReceive() {
  // Leu um pacote, vamos decodificar?
  Serial.println("Lora Received");
  byte recipient = LoRa.read();          // Endereco de quem ta recebendo
  if (recipient != localAddress) {
    return; //Não é pra mim
  }

  byte incomingMsgId = LoRa.read();     // Id da Mensagem
  bool ack = LoRa.read();               // Se é uma ack

  if (ack) {
    if (lastIdMsg == incomingMsgId) {
      lastMsgConfirmed = true;
      Serial.println("ACK Received " + String(incomingMsgId));
    }
  } else {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    //Trabalhar a msg
    if (incomingMsgId % 2)
      digitalWrite(BUILTIN_LED, HIGH);
    else
      digitalWrite(BUILTIN_LED, LOW);

    Serial.println("Mensagem recebida: " + incoming);

    //Enviar o ack
    LoraSendAck(incomingMsgId);
    Serial.print("Enviando ACK ");
    Serial.println(lastIdMsg);
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

  Serial.println(" Modulo LoRa Device!!!");

  pinMode(BUILTIN_LED, OUTPUT);
}

void loop() {
  // verifica se temos o intervalo de tempo para enviar uma mensagem
  if (millis() - lastSendTime > interval) {
    //String mensagem = "Hellora World Device " + String(localAddress)  ;    // Definicao da mensagem
    Serial.println("Enviando temperatura!");
    LoraSendMessage(String(dht.readTemperature()), 1);
    lastSendTime = millis();            // Timestamp da ultima mensagem
  }

  if (!lastMsgConfirmed && (retry < 6) && (millis() - lastSendTime > (random(400) + 100))) {
    LoraSendMessage(lastMsg, retry++);
    lastSendTime = millis();
    if (retry == 6) {
      retry=0;
      lastMsgConfirmed = true;
    }
  }


  if (LoRa.parsePacket())
    LoraReceive();
}
