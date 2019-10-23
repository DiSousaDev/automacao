#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SimpleDHT.h>
//Led da Placa
#define led 2
//Pino onde está o Relê lampada 1
#define RELE1_PIN 5
//Pino onde está o Relê lampada 2
#define RELE2_PIN 4
//Pino onde está o DHT11
#define DHT_PIN 15
//Pino do Sensor de Presença
#define PIN_SENSOR 27
//Pino do Buzzer
#define PIN_BUZZER 19
//Intervalo entre as checagens de novas mensagens
#define INTERVAL 1000
//Token do seu bot. Troque pela que o BotFather te mostrar
#define BOT_TOKEN "842075575:AAHlDdCNxseTF2M9lHoHQ3YDv01KB9KEIVo"
#define CORE_UM 1
#define CORE_ZERO 0
//Troque pelo ssid e senha da sua rede WiFi
#define SSID "IFPR-Alunos"
#define PASSWORD "#alunoifpr"
//#define SSID "AgneMoveis"
//#define PASSWORD "Agne36219846"
//#define SSID "Eventos"
//#define PASSWORD "$Th3534leventos$"

//Comandos aceitos
const String LIGHT_ON_QUARTO = "ligar a luz do quarto";
const String LIGHT_OFF_QUARTO = "desligar a luz do quarto";
const String LIGHT_ON_SALA = "ligar a luz da sala";
const String LIGHT_OFF_SALA = "desligar a luz da sala";
const String ALARME_ON = "ativar alarme";
const String ALARME_OFF = "desativar alarme";
const String CLIMATE = "clima";
const String STATS = "status";
const String START = "/start";

//Objeto que realiza a leitura da temperatura e umidade
SimpleDHT11 dht;
//Estado do relê
int relayStatus = HIGH;
//Cliente para conexões seguras
WiFiClientSecure client;
//Objeto com os métodos para comunicarmos pelo Telegram
UniversalTelegramBot bot(BOT_TOKEN, client);
//Tempo em que foi feita a última checagem
uint32_t lastCheckTime = 0;
//Quantidade de usuários que podem interagir com o bot
#define SENDER_ID_COUNT 2
//Ids dos usuários que podem interagir com o bot.
//É possível verificar seu id pelo monitor serial ao enviar uma mensagem para o bot
String validSenderIds[SENDER_ID_COUNT] = {"575489178", "123456789"};
//sensor de presença desativado
int presencaStatus = LOW;


void setup() {
  Serial.begin(115200);
  pinMode(led, OUTPUT);
  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(RELE1_PIN, relayStatus);
  digitalWrite(RELE2_PIN, relayStatus);
  digitalWrite(PIN_SENSOR, presencaStatus);
  digitalWrite(PIN_BUZZER, presencaStatus);

  //criando tarefa e definindo o core
  TaskHandle_t handle_setupWiFi;
  xTaskCreatePinnedToCore(
    setupWiFi
    ,  "setupWiFi"   // A name just for humans
    ,  10000  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  10  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  &handle_setupWiFi
    ,  CORE_UM);
}
TaskHandle_t handle_setupWiFi;
void setupWiFi(void *pvParameters) {
  Serial.print("Conectando a Rede Wireless: ");
  Serial.println(SSID);
  //Inicia em modo station e se conecta à rede WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  //Enquanto não estiver conectado à rede
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  //Se chegou aqui está conectado
  Serial.println();
  Serial.println("Conexao bem sucedida!");
  vTaskDelete (handle_setupWiFi);
}


void loop() {
  //Tempo agora desde o boot
  uint32_t now = millis();
  //Se o tempo passado desde a última checagem for maior que o intervalo determinado
  if (now - lastCheckTime > INTERVAL) {
    //Coloca o tempo de útlima checagem como agora e checa por mensagens
    lastCheckTime = now;
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    handleNewMessages(numNewMessages);
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) { //para cada mensagem nova
    String chatId = String(bot.messages[i].chat_id); //id do chat
    String senderId = String(bot.messages[i].from_id); //id do contato
    Serial.println("senderId: " + senderId); //mostra no monitor serial o id de quem mandou a mensagem
    boolean validSender = validateSender(senderId); //verifica se é o id de um remetente da lista de remetentes válidos
    if (!validSender) { //se não for um remetente válido
      bot.sendMessage(chatId, "Desculpe mas você não tem permissão", "HTML"); //envia mensagem que não possui permissão e retorna sem fazer mais nada
      continue; //continua para a próxima iteração do for (vai para próxima mensgem, não executa o código abaixo)
    }
    String text = bot.messages[i].text; //texto que chegou
    if (text.equalsIgnoreCase(START)) {
      handleStart(chatId, bot.messages[i].from_name); //mostra as opções
    } else if (text.equalsIgnoreCase(LIGHT_ON_QUARTO)) {
      handleLightOnQuarto(chatId); //liga o relê do quarto
    } else if (text.equalsIgnoreCase(LIGHT_OFF_QUARTO)) {
      handleLightOffQuarto(chatId); //desliga o relê do quarto
    } else if (text.equalsIgnoreCase(LIGHT_ON_SALA)) {
      handleLightOnSala(chatId); //liga o relê da sala
    } else if (text.equalsIgnoreCase(LIGHT_OFF_SALA)) {
      handleLightOffSala(chatId); //desliga o relê da sala
    } else if (text.equalsIgnoreCase(ALARME_ON)) {
      handleAlarmeON(chatId); //ativa o alarme
    } else if (text.equalsIgnoreCase(ALARME_OFF)) {
      handleAlarmeOFF(chatId); //desliga o alarme
    } else if (text.equalsIgnoreCase(CLIMATE)) {
      handleClimate(chatId); //envia mensagem com a temperatura e umidade
    } else if (text.equalsIgnoreCase(STATS)) {
      handleStatus(chatId); //envia mensagem com o estado do relê, temperatura e umidade
    } else {
      handleNotFound(chatId); //mostra mensagem que a opção não é válida e mostra as opções
    }
  }//for
}
boolean validateSender(String senderId) {
  //Para cada id de usuário que pode interagir com este bot
  for (int i = 0; i < SENDER_ID_COUNT; i++) {
    //Se o id do remetente faz parte do array retornamos que é válido
    if (senderId == validSenderIds[i]) {
      return true;
    }
  }
  //Se chegou aqui significa que verificou todos os ids e não encontrou no array
  return false;
}

void handleStart(String chatId, String fromName) {
  //Mostra Olá e o nome do contato seguido das mensagens válidas
  String message = "<b>Olá " + fromName + ".</b>\n";
  message += getCommands();
  bot.sendMessage(chatId, message, "HTML");
}

String getCommands() {
  //String com a lista de mensagens que são válidas e explicação sobre o que faz
  String message = "Os comandos disponíveis são:\n\n";
  message += "<b>" + LIGHT_ON_QUARTO + "</b>: Para ligar a luz do quarto\n";
  message += "<b>" + LIGHT_OFF_QUARTO + "</b>: Para desligar a luz do quarto\n";
  message += "<b>" + LIGHT_ON_SALA + "</b>: Para ligar a luz da sala\n";
  message += "<b>" + LIGHT_OFF_SALA + "</b>: Para desligar a luz da sala\n";
  message += "<b>" + ALARME_ON + "</b>: Para ativar o alarme.\n";
  message += "<b>" + ALARME_OFF + "</b>: Para desligar o alarme.\n";
  message += "<b>" + CLIMATE + "</b>: Para verificar o clima\n";
  message += "<b>" + STATS + "</b>: Para verificar o estado da luz e a temperatura";
  return message;
}
xTaskCreatePinnedToCore(
    handleAlarmeON
    ,  "handleAlarmeON"   // A name just for humans
    ,  10000  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  (void*)&chatId
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL
    ,  CORE_ZERO);
}

void handleAlarmeON(void *param_chatId) {
  //ATIVA O ALARME
  String chatId = *((String)param_chatId);
  bot.sendMessage(chatId, "O monitoramento está <b>ativo</b>", "HTML");
    delay(1000);
    int presencaStatus = digitalRead(PIN_SENSOR);
    if (presencaStatus == HIGH) {
      digitalWrite(led, HIGH);
      digitalWrite(PIN_BUZZER, HIGH);
      Serial.println("MOVIMENTO DETECTADO");
    } else {
      digitalWrite(led, LOW);
      digitalWrite(PIN_BUZZER, LOW);
      Serial.println("NENHUM MOVIMENTO DETECTADO");
    }
}
void handleAlarmeOFF(String chatId) {
  int presencaStatus = LOW;
  digitalWrite(PIN_BUZZER, LOW);
}

void handleLightOnQuarto(String chatId) {
  //Liga o relê do quarto e envia mensagem confirmando a operação
  relayStatus = LOW; //A lógica do nosso relê é invertida
  digitalWrite(RELE1_PIN, relayStatus);
  bot.sendMessage(chatId, "A luz está do quarto está <b>acesa</b>", "HTML");
}
void handleLightOffQuarto(String chatId) {
  //Desliga o relê do quarto e envia mensagem confirmando a operação
  relayStatus = HIGH; //A lógica do nosso relê é invertida
  digitalWrite(RELE1_PIN, relayStatus);
  bot.sendMessage(chatId, "A luz do quarto está <b>apagada</b>", "HTML");
}
void handleLightOnSala(String chatId) {
  //Liga o relê da sala e envia mensagem confirmando a operação
  relayStatus = LOW; //A lógica do nosso relê é invertida
  digitalWrite(RELE2_PIN, relayStatus);
  bot.sendMessage(chatId, "A luz da Sala está <b>acesa</b>", "HTML");
}
void handleLightOffSala(String chatId) {
  //Desliga o relê da sala e envia mensagem confirmando a operação
  relayStatus = HIGH; //A lógica do nosso relê é invertida
  digitalWrite(RELE2_PIN, relayStatus);
  bot.sendMessage(chatId, "A luz da Sala está <b>apagada</b>", "HTML");
}
void handleClimate(String chatId) {
  //Envia mensagem com o valor da temperatura e da umidade
  bot.sendMessage(chatId, getClimateMessage(), "");
}
String getClimateMessage() {
  //Faz a leitura da temperatura e da umidade
  float temperature, humidity;
  int status = dht.read2(DHT_PIN, &temperature, &humidity, NULL);

  //Se foi bem sucedido
  if (status == SimpleDHTErrSuccess)  {
    //Retorna uma string com os valores
    String message = "";
    message += "A temperatura é de " + String(temperature) + " °C e ";
    message += "a umidade é de " + String(humidity) + "%\n";
    return message;
  }
  //Se não foi bem sucedido retorna um mensagem de erro
  return "Erro ao ler temperatura e umidade";
}
String getAlarmeStatus() {
  //Faz verificação se o alarme está ativo.
  String message = "";
  if (presencaStatus == HIGH) {
    message += "O Alarme está Ativo.";
  } else {
    message += "O Alarme está desativado.";
  }
  return message;
}

void handleStatus(String chatId) {
  String message = "";
  //Verifica se o relê está ligado ou desligado e gera a mensagem de acordo
  if (RELE1_PIN, relayStatus == LOW) { //A lógica do nosso relê é invertida
    message += "A luz do quarto está acesa\n";
  } else {
    message += "A luz do quarto está apagada\n";
  }
  if (RELE2_PIN, relayStatus == LOW) { //A lógica do nosso relê é invertida
    message += "A luz da sala está acesa\n";
  } else {
    message += "A luz da sala está apagada\n";
  }

  //Adiciona à mensagem o valor da temperatura e umidade
  message += getClimateMessage();
  //Adiciona à mensagem o Status do alarme.
  message += getAlarmeStatus();
  //Envia a mensagem para o contato
  bot.sendMessage(chatId, message, "");
}
void handleNotFound(String chatId) {
  //Envia mensagem dizendo que o comando não foi encontrado e mostra opções de comando válidos
  String message = "Comando não encontrado\n";
  message += getCommands();
  bot.sendMessage(chatId, message, "HTML");
}
