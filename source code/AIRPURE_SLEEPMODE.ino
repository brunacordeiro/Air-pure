/*Bibliotecas*/
#include <PubSubClient.h>
#include <DHT.h>
#include <WiFi.h>
#include <SPI.h>
#include <Adafruit_CCS811.h>

/*Definir os pinos dos sensor*/
#define dhtPin 4 //Sensor de temperatura e umidade - DHT11.
#define RXD2 16 //Sensor de CO2 - MH-Z14A.
#define TXD2 17 //Sensor de CO2 - MH-Z14A.

#define uS_TO_S_FACTOR 1000000  /* Fator de conversao de microsegundos para segundos */
#define TIME_TO_SLEEP  60        /* Tempo de sleep do ESP32 em segundos */

/*Configuração de sensores.*/
//DHT11 - Temperatura e Umidade.
#define dhtType DHT11 //Tipo do sensor DHT.
DHT dht(dhtPin, dhtType); //Objeto sensor de temperatura e umidade
//CSS811 - TVOC
Adafruit_CCS811 ccs; //Objeto sensor de TVOC.

//Definir variaveis globais.
float temp; //Temperatura em graus celsius.
float umid; //Umidade relativa.
float eco2; //Equivalente de Dióxido de carbono.
float voc; //Total de compostos organicos voláteis.
float valorCO2; //Dióxido de carbono. 

/*Configurações de rede e conexão MQTT ThingSpeak*/
char ssid[] = "Antonielli"; //nome da rede. PACO Internet
char pass[] = "12345678"; //senha da rede. SEM SENHA
char mqttUserName[] = "ufgsaudeambiental"; //nome de usuário do MQTT
char mqttPass[] = "0QIMS6VELRQUUC0A"; //chave de acesso do MQTT.
char writeAPIKey[] = "WDPPXX2EI7II24E0"; //chave de escrita, canal Thingspeak.
long channelID = 1160801; //Identificação do canal Thingspeak - Pessoal.

/*Definir identificação de cliente, randomico.*/
static const char alphanum[] = "0123456789""ABCDEFGHIJKLMNOPQRSTUVWXYZ""abcdefghijklmnopqrstuvwxyz";

WiFiClient client; //Inicializar cliente wifi

//Inicializar a biblioteca pubsubclient e definir o broker MQTT thingspeak.
PubSubClient mqttClient(client);
const char* server = "mqtt.thingspeak.com";

unsigned long lastConnectionTime = 0; //Tempo da última conexão.
const unsigned long postingInterval = 20000L; //Tempo de postagem, 20 segundos.

void setup() {
  Serial.begin(115200); //Iniciar porta serial - USB.
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); //Iniciar porta serial - UART.
  int status = WL_IDLE_STATUS; //Estado da conexão wifi.

  //Inicializar sensor CCS811.
  if(!ccs.begin()){
    Serial.println("Falha ao iniciar o CCS811! Checar conexão dos fios.");
    while(1);
  }
  
  dht.begin(); //Inicializar DHT11.
  
  pinMode(dhtPin, INPUT); //Configurar modo dos pinos.
  
  /*Conectar a rede wifi*/
  while(status != WL_CONNECTED){
    status = WiFi.begin(ssid, pass); //Conectar a rede WiFi WPA/WPA2.
    delay(5000);
    }
  Serial.print("Conectado ao WiFi: "); //Imprimir nome da rede conectada.
  Serial.println(ssid);
  
  mqttClient.setServer(server, 1883); //Configurar Broker MQTT - ThingSpeak.


  if(!mqttClient.connected()){
    reconnect();
  }

  mqttClient.loop(); //Manter conexão MQTT.
  
  mqttpublish();

  WiFi.mode(WIFI_OFF); //Desliga o WiFi antes de entrar em modo SLEEP.

  Serial.println("Sleep Mode!");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Entra no modo Sleep.
  esp_deep_sleep_start();

}

//Como nesta configuracao o loop nao e usado, passamos todas as instrucoes para o setup().
void loop() {

}


//Leitura da concentração de gás - MH-Z14A.
float leituraGas(){

  const byte comando[9] =  {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79}; //Comando de leitura da concentração de gás.
  byte resposta[9]; //Armazena a resposta do comando de leitura.

  //Escrever comando de leitura.
  for(int i = 0; i<9; i++){
    Serial2.write(comando[i]);
    }

  delay(30);

  //Ler retorno do comando de leitura.
  if(Serial2.available()){
    for(int i=0; i<9; i++){
      resposta[i] = Serial2.read();
      }
    int alto = (int)resposta[2];
    int baixo = (int)resposta[3];

    float CO2 = ((alto*256)+baixo); //Concentração de CO2 em ppm, referência datasheet.

    return CO2;
  }
}

//Conectar ao Broker MQTT.
void reconnect(){
  char clientID[9]; //Identificação do cliente.

  //Gerar ID do cliente.
  while(!mqttClient.connected()){
    Serial.print("Tentando conexão MQTT...");
    for(int i=0; i<8; i++){
      clientID[i] = alphanum[random(100)];
      }
  clientID[8] = '\0';

  //Iniciar conexão MQTT.
  if(mqttClient.connect(clientID, mqttUserName, mqttPass)){
    Serial.println("Conectado.");
    }else{
      Serial.print("Failed, rc= ");
      /*Verificar o porque ocorreu a falha.*/
      //Ver em: https://pubsubclient.knolleary.net/api.html#state explicação do código da falha.
      Serial.print(mqttClient.state());
      Serial.println("Tentar novamente em 5 segundos.");
      delay(5000);
      }
   }
 }

 //Publicar dados ThingSpeak.
 void mqttpublish(){
 //Leitura dos valores.
  //DHT11 - Temperatura e Umidade.
  temp = dht.readTemperature(); //Ler temperatura - DHT11.
  umid = dht.readHumidity(); //Ler umidade - DHT11.
  
  //Verifica se a leitura não um número.
  if(isnan(umid) || isnan(temp)){
  Serial.println("Erro de leitura DHT11!");
  return;
  }

  //CCS811 - TVOC
   if(ccs.available()){
    if(!ccs.readData()){
      eco2 = ccs.geteCO2(); //Ler eCO2 - CCS811.
      voc = ccs.getTVOC(); //Ler TVOC - CCS811.
    }else{
      Serial.println("Erro de leitura CCS811!");
      while(1);
    }
  }

  valorCO2 = leituraGas(); //Concentração de CO2 - MH-Z14A.
  
  //String de dados para enviar a Thingspeak.
  String dados = String("field1=" + String(temp, 2) + "&field2=" + String(umid, 2) + "&field3=" +String(eco2, 2)+ "&field4=" +String(voc, 2)+ "&field5=" + String(valorCO2));
  int tamanho = dados.length();
  char msgBuffer[tamanho];
  dados.toCharArray(msgBuffer,tamanho+1);
  Serial.println(msgBuffer);

  //Cria uma String de tópico e publica os dados na Thingspeak.
  String topicString = "channels/" + String(channelID) + "/publish/"+String(writeAPIKey);
  tamanho = topicString.length();
  char topicBuffer[tamanho];
  topicString.toCharArray(topicBuffer, tamanho+1);

  mqttClient.publish(topicBuffer, msgBuffer); //Publicar dados.

  lastConnectionTime = millis();
  }
