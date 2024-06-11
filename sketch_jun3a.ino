#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>


Preferences preferences;           
const char* mqttServer = "broker.hivemq.com";   
const int   mqttPort = 1883;                    
const char* mqttUser = "";                      
const char* mqttPassword = "";                           
const char* topico_adicionar = "adicionar/jpmendes";
const char* topico_remover = "remover/jpmendes";
const char* topico_listar = "listar/jpmendes";
const char* topico_enviar = "enviar/jpmendes";

#define LedVerde 32
#define LedVermelho 25
#define tranca 33
#define buzzer 4
#define SS_PIN 5
#define RST_PIN 15
#define botaoVerde 26
#define botaoVermelho 27

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient    espClient;                     
PubSubClient  client(espClient);

const int maxIDs = 10;
String authorizedIDs[maxIDs];

void setup() {
  Serial.begin(115200);

  pinMode(LedVerde, OUTPUT);
  pinMode(LedVermelho, OUTPUT);
  pinMode(tranca, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(botaoVerde,INPUT_PULLUP);
  pinMode(botaoVermelho,INPUT_PULLUP);


  connectToWiFi("galaxy do jp", "mendes123");

  SPI.begin();
  lcd.init();
  lcd.backlight();

  mfrc522.PCD_Init();

  client.setCallback(callback);
  client.subscribe(topico_adicionar); // se inscreve nos topicos do mqtt
  client.subscribe(topico_remover); 
  client.subscribe(topico_listar);
  client.subscribe(topico_enviar);


  preferences.begin("rfid-ids", false); //inicializa a memoria

  for (int i = 0; i < maxIDs; i++) {
    String key = "ID" + String(i);
    authorizedIDs[i] = preferences.getString(key.c_str(), "");
  }
  
}

void loop() {

  if(!client.connected()){
    reconectabroker();                      
  }
  client.loop();

  lcd.home();                // bota o cursor do lcd na posicao inicial
  lcd.print("Aproxime o");
  lcd.setCursor(0,1);        // seta o cursor para a segunda linha
  lcd.print("cartao");

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command.startsWith("add ")) {
      String id = command.substring(4);
      addAuthorizedID(id);
    } else if (command.startsWith("remove ")) {
      String id = command.substring(7);
      removeAuthorizedID(id);
    } else if (command == "list") {
      listAuthorizedIDs();
    } else if (command == "setwifi") {
      Serial.print("Digite o SSID da nova rede: ");
      while (!Serial.available()) {
        delay(10);
      }
      String ssid = Serial.readStringUntil('\n');
      ssid.trim();
      Serial.println(ssid);

      Serial.print("Digite o password da nova rede: ");
      while (!Serial.available()) {
        delay(10);
      }
      String password = Serial.readStringUntil('\n');
      password.trim();
      Serial.println(password);

      setWiFiCredentials(ssid, password);
    }
       
    else {
      Serial.println("Comando não reconhecido. Use 'add <ID>', 'remove <ID>' 'list' ou 'setwifi'");
    }

  }
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
     delay(500);
     return;                 // se nao tiver um cartao para ser lido recomeça o void loop
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    delay(500);
    return;                  //se nao conseguir ler o cartao recomeça o void loop tambem
  }

  String conteudo = "";      // cria uma string

  for (byte i = 0; i < mfrc522.uid.size; i++){
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
    }

  Serial.println();
  conteudo.trim();
  conteudo.toUpperCase();
  client.publish(topico_enviar,conteudo.c_str());

  if(isAuthorizedID(conteudo)){                
      digitalWrite(LedVerde, HIGH);            
      lcd.clear();
      lcd.home();                           
      lcd.print("ID Liberado:");
      lcd.setCursor(0,1);                    
      lcd.print(conteudo);

      digitalWrite(tranca, HIGH);             

      delay(3000);

      digitalWrite(tranca, LOW);               // fecha a tranca
      digitalWrite(LedVerde, LOW);             // e desliga o led
      lcd.clear();
  }else{
    digitalWrite(LedVermelho, HIGH);        // vamos ligar o led vermelho
    delay(800);
    digitalWrite(buzzer, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW);

        lcd.clear();                           
        lcd.home();                            
        lcd.print("ID negado:");            
        lcd.setCursor(0,1);                    
        lcd.print(conteudo);
        delay(3000); 
        
        digitalWrite(LedVermelho, LOW);       // desliga o led vermelho
        lcd.clear();
        lcd.home();
        lcd.print("Adicionar ID?");
        lcd.setCursor(0, 1);
        lcd.print("S:verde N:vermelho");
        while(true){
          if(digitalRead(botaoVerde)==LOW){
            addAuthorizedID(conteudo);
            lcd.clear();
            lcd.home();
            lcd.print("ID adicionada!");
            delay(3000);
            lcd.clear();
            return;
          }
          if(digitalRead(botaoVermelho)==LOW){
            lcd.clear();
            return;
          }
        }
  }


}

// Função para adicionar um novo UID autorizado
void addAuthorizedID(String uid) {
  for (int i = 0; i < maxIDs; i++) {
    if (authorizedIDs[i] == "") {
      authorizedIDs[i] = uid;
      String key = "ID" + String(i);
      preferences.putString(key.c_str(), uid);
      Serial.print("Novo UID autorizado adicionado: ");
      Serial.println(uid);
      return;
    }
  }
  Serial.println("Lista de IDs autorizadas cheia!");
}

void removeAuthorizedID(String uid) {
  for (int i = 0; i < maxIDs; i++) {
    if (authorizedIDs[i] == uid) {
      authorizedIDs[i] = "";
      String key = "ID" + String(i);
      preferences.remove(key.c_str());
      Serial.print("UID autorizado removido: ");
      Serial.println(uid);
      return;
    }
  }
  Serial.println("UID não encontrado na lista de IDs autorizadas!");
}

void listAuthorizedIDs() {
  Serial.println("IDs autorizadas:");
  for (int i = 0; i < maxIDs; i++) {
    if (authorizedIDs[i] != "") {
      Serial.println(authorizedIDs[i]);
    }
  }
}

void reconectabroker()
{
  client.setServer(mqttServer, mqttPort);
  while (!client.connected())
  {
    Serial.println("Conectando ao broker MQTT...");
    if (client.connect("", mqttUser, mqttPassword ))
    {
      Serial.println("Conectado ao broker!");  
        client.subscribe(topico_adicionar);
        client.subscribe(topico_remover); 
        client.subscribe(topico_listar);
        client.subscribe(topico_enviar);            
    }
    else
    {
      Serial.print("Falha na conexao ao broker - Estado: ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic)==topico_adicionar) {
    String id = message;
    addAuthorizedID(id);
  } else if (String(topic) == topico_remover) {
    String id = message;
    removeAuthorizedID(id);
  } else if (String(topic) == topico_listar) {
      listAuthorizedIDs();
    }
}

void setWiFiCredentials(String ssid, String password) {
  Serial.println("Atualizando credenciais WiFi...");
  WiFi.disconnect();
  connectToWiFi(ssid, password);
}

void connectToWiFi(String ssid, String password) {
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao WiFi");
}

bool isAuthorizedID(String id) {
  int count = 10;
  for (int i = 0; i < count; i++) {
    if(authorizedIDs[i]!=""){
      String key = "ID" + String(i);
      String storedID = preferences.getString(key.c_str(), ""); 
      if (storedID == id) {
      return true;  // ID autorizado encontrado
    }
    }
  }
  return false;  // ID não autorizado
}
