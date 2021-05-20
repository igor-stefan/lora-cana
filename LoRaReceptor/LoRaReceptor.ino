/*
* Projeto: receptor - medição local, via LoRa e via mqtt de temperatura e umidade
* Autor: Pedro Bertoleti
*/
#include <LoRa.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "FirebaseESP32.h"
#include "time.h"

/* Definicoes para comunicação com radio LoRa */
#define SCK_LORA           5
#define MISO_LORA          19
#define MOSI_LORA          27
#define RESET_PIN_LORA     14
#define SS_PIN_LORA        18
#define HIGH_GAIN_LORA     20  /* dBm */
#define BAND               915E6  /* 915MHz de frequencia */

/* Definicoes do OLED */
#define OLED_SDA_PIN    4
#define OLED_SCL_PIN    15
#define SCREEN_WIDTH    128 
#define SCREEN_HEIGHT   64  
#define OLED_ADDR       0x3C 
#define OLED_RESET      16

/* Offset de linhas no display OLED */
#define OLED_LINE1     0
#define OLED_LINE2     10
#define OLED_LINE3     20
#define OLED_LINE4     30
#define OLED_LINE5     40
#define OLED_LINE6     50

/* Definicoes gerais */
#define DEBUG_SERIAL_BAUDRATE    115200

/* Credenciais wifi */
#define WIFI_SSID "2G_Casa"
#define WIFI_PASSWORD "240102509"

/* Credenciais Firebase */
#define FIREBASE_HOST "https://monitoramento-de-falhas-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "jm5eREO7HfCBCpF4sGgsMVapvsC5o1bfmN7rD65A"

/* Variaveis e objetos globais */
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
FirebaseData firebaseData;
FirebaseJson updateData;

/* Local prototypes de funções */
void display_init(void);
bool init_comunicacao_lora(void);
void gerar_endereco(char *info, int mes, int dia, int hora, int minuto, int seg, int contagem);

/* typedefs */
typedef struct __attribute__((__packed__))  
{
  float temperatura;
  float umidade;
  float temperatura_min;
  float temperatura_max;
}TDadosLora;

/* Funcao: inicializa comunicacao com o display OLED
 * Parametros: nenhnum
 * Retorno: nenhnum
*/ 
void display_init(void)
{
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) 
    {
        Serial.println("[LoRa Receiver] Falha ao inicializar comunicacao com OLED");        
    }
    else
    {
        Serial.println("[LoRa Receiver] Comunicacao com OLED inicializada com sucesso");
    
        /* Limpa display e configura tamanho de fonte */
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
    }
}

/* Funcao: inicia comunicação com chip LoRa
 * Parametros: nenhum
 * Retorno: true: comunicacao ok
 *          false: falha na comunicacao
*/
bool init_comunicacao_lora(void)
{
    bool status_init = false;
    Serial.println("[LoRa Receiver] Tentando iniciar comunicacao com o radio LoRa...");
    SPI.begin(SCK_LORA, MISO_LORA, MOSI_LORA, SS_PIN_LORA);
    LoRa.setPins(SS_PIN_LORA, RESET_PIN_LORA, LORA_DEFAULT_DIO0_PIN);
    
    if (!LoRa.begin(BAND)) 
    {
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa falhou. Nova tentativa em 1 segundo...");        
        delay(1000);
        status_init = false;
    }
    else
    {
        /* Configura o ganho do receptor LoRa para 20dBm, o maior ganho possível (visando maior alcance possível) */ 
        LoRa.setTxPower(HIGH_GAIN_LORA); 
        Serial.println("[LoRa Receiver] Comunicacao com o radio LoRa ok");
        status_init = true;
    }

    return status_init;
}

/* Funcao: gera endereco de destino dos dados no banco de dados Firebase
 * Parametros: ponteiro para vetor que armazenará informação, (mes, dia, hora, minuto e segundo) 
 * Retorno: nenhum
*/
void gerar_endereco(char *info, int mes, int dia, int hora, int minuto, int seg, int contagem){
  sprintf(info, "%02d/%02d/%d/%02d:%02d:%02d", mes + 1, dia, contagem, hora, minuto, seg);
}

/* Variáveis auxiliares */
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600 * -3;
const int   daylightOffset_sec = 0;
int contador = 0, ano = 0;

/* Funcao de setup */
void setup() 
{
    /* Configuracao da I2C para o display OLED */
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

    /* Display init */
    display_init();

    /* Print message telling to wait */
    display.clearDisplay();    
    display.setCursor(0, OLED_LINE1);
    display.print("Receptor ativo. Aguarde...");
    display.setCursor(0, OLED_LINE2);
    display.print("Iniciando a conexao serial...");
    display.display();
    
    Serial.begin(DEBUG_SERIAL_BAUDRATE);
    while(!Serial);
    display.setCursor(0, OLED_LINE3);
    display.print("CONEXAO SERIAL INICIADA");
    display.setCursor(0, OLED_LINE4);
    display.print("INICIANDO CONEXAO COM LORA");
    display.display();

    /* Tenta, até obter sucesso, comunicacao com o chip LoRa */
    while(init_comunicacao_lora() == false){
      Serial.println("SETUP - FALHA AO CONECTAR COM O CHIP TRANSMISSOR");
      delay(300);
      }
    display.setCursor(0, OLED_LINE5);
    display.print("CONEXAO COM LORA OK");
    display.setCursor(0, OLED_LINE6);
    display.print("INICIANDO CONEXAO WIFI");
    display.display();       

    /* Tenta, até obter sucesso, comunicacao com o Wifi */
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); //Conecta ao wifi
    Serial.print("Conectando ao Wi-Fi"); //Print msg
    while (WiFi.status() != WL_CONNECTED){ //Aguarda enquanto nao conecta.
      Serial.print(".");
      delay(300);
    }
    Serial.println();
    Serial.println(WiFi.localIP()); //printa ip após conectar
    display.clearDisplay();
    display.setCursor(0, OLED_LINE1);
    display.print("WIFI CONECTADO");
    display.setCursor(0, OLED_LINE2);
    display.print("CONFIGURANDO VARIAVEL CONTADOR");
    display.display(); 
    /* Inicia comunicação com o Firebase */
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
    Firebase.reconnectWiFi(true);

    /* Inicia as configurações de hora */
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    if(Firebase.getInt(firebaseData, "/Ultimo/num")){
    if(firebaseData.dataType() == "int"){
      Serial.print("CONTADOR INICIANDO NO ULTIMO VALOR = ");
      Serial.println(firebaseData.intData());
      contador = firebaseData.intData();
    }
  }else {
    Serial.print("OCORREU UM ERRO => ");
    Serial.println(firebaseData.errorReason());
    display.setCursor(0, OLED_LINE3);
    display.print("OCORREU UM ERRO!");
    display.display();
  }
    display.setCursor(0, OLED_LINE3);
    display.print("CONTADOR CONFIGURADO = ");
    display.setCursor(0, OLED_LINE4);
    display.print(contador);
    display.setCursor(0, OLED_LINE5);
    display.print("SETUP COMPLETO!");
    display.display();
}

/* Programa principal */
void loop() 
{
    struct tm timeinfo;
    char byte_recebido;
    int packet_size = 0;
    int lora_rssi = 0;
    TDadosLora dados_lora;
    char * ptInformaraoRecebida = NULL;
    String endereco = "/Geral/falha", yyear = String(ano);
    char info[60];
  
    /* Verifica se chegou alguma informação do tamanho esperado */
    packet_size = LoRa.parsePacket();
    
    if(packet_size == sizeof(TDadosLora)){ //verifica se há dados para serem lidos
        Serial.println("[LoRa Receiver] Há dados a serem lidos");
        
        /* Recebe os dados conforme protocolo */               
        ptInformaraoRecebida = (char *)&dados_lora; 
        /* o ponteiro ptInformaraoRecebida passará a apontar para o endereço de dados_lora
        o endereço de dados_lora, que é uma struct definida pelo usuario TDadosLora, será interpretado como char
        devido ao cast (char *) antes da atribuição. o tamanho em bytes do ponteiro ptInformaraoRecebida é do mesmo tamanho
        da struct dados_lora*/
        while(LoRa.available()){ //enquanto estiver dados disponiveis
            byte_recebido = (char)LoRa.read(); //lê byte a byte
            *ptInformaraoRecebida = byte_recebido; //o valor do byte lido é atribuido ao byte do ponteiro
            ptInformaraoRecebida++; //o ponteiro é incrementado pra receber o proximo valor
        }

        /* Escreve RSSI de recepção e informação recebida */
        lora_rssi = LoRa.packetRssi(); //Received Signal Strength Indication (RSSI) medido em dBm
        if(dados_lora.temperatura < 50 && dados_lora.temperatura > -1 && !isnan(dados_lora.temperatura)
        && dados_lora.umidade < 100 && dados_lora.umidade > -1 && !isnan(dados_lora.umidade)){ 
          //testa se os dados são válidos
          display.clearDisplay();
          int tentativas = 0;
           while(!getLocalTime(&timeinfo) && tentativas++ < 10){ //tenta sincronizar o horário
            Serial.println("Falha na sincronização com a hora");
            display.setCursor(0, OLED_LINE1);
            display.print("TENTANDO SINCRONIZAR O HORARIO");
            display.display();
            delay(300);
            display.clearDisplay();
           }
           if(tentativas < 10){ //se o horario e os dados estao ok, enviar para o firebase.
            contador++;     
            display.setCursor(0, OLED_LINE1); //mostra no display os dados
            display.print("Temp: ");
            display.println(dados_lora.temperatura);
    
            display.setCursor(0, OLED_LINE2);
            display.print("Umid: ");
            display.println(dados_lora.umidade);
    
            display.setCursor(0, OLED_LINE3);
            display.println("Temp min/max:");
            display.setCursor(0, OLED_LINE4);
            display.print(dados_lora.temperatura_min);
            display.print("/");
            display.print(dados_lora.temperatura_max);
    
            display.setCursor(0, OLED_LINE5);
            display.println("ENDERECO FIREBASE:");
            display.setCursor(0, OLED_LINE6);
    
            gerar_endereco(info, timeinfo.tm_mon,timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, contador);
            endereco = info; //atribui o endereço armazenado no vetor de char "info" pela funcao gerar_endereço à string endereço
            Serial.print("INFO = ");
            Serial.println(info);
            if(ano !=  timeinfo.tm_year + 1900){ //verifica se o ano armazenado é diferente do ano atual
              ano = timeinfo.tm_year + 1900; 
              yyear = String(ano); 
            }
            
            display.print(endereco); //mostra endereço do firebase no oled
            display.display();     
            Firebase.setFloat(firebaseData, yyear + "/Temperatura/" + endereco + "/dado", dados_lora.temperatura);
            Firebase.setFloat(firebaseData, yyear + "/Umidade/" + endereco + "/dado", dados_lora.umidade);
            updateData.set("num", contador);
            Firebase.updateNode(firebaseData, "/Ultimo", updateData);
           } //finaliza envio dos dados
        }else{ //se os dados foram invalidos
          display.setCursor(0, OLED_LINE1); //mostra no display
          display.print("DADOS INVALIDOS");
          display.setCursor(0, OLED_LINE3);
          display.print("ESPERANDO PROXIMO PACOTE");
          display.display();
          Serial.println("DADOS INVÁLIDOS!!"); //mostra na serial
          Serial.println("ESPERANDO PRÓXIMO PACOTE"); //fim do loop, aguarda proximo pacote
        }
    }
}
