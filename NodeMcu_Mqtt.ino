#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

const long TEMPO_RECONEXAO = 3000;
const long TEMPO_ACIONAMENTO = 500;
const long TEMPO_ESPERA_WIFI = 500;
const long TEMPO_ESPERA_CONFIGURACAO = 100;
long TEMPO_INICIAL = 0;

String clientName = "blIot_device";
String versaoFirmware = "V1";
IPAddress ipDispositivo;

#define LED 4
#define CARGA 5
#define CARGA2 14
#define LED_CONF 12

char *server = "test.mosquitto.org";
const char *dirUpdate = "/update.bin";
const int porta = 1883;

char message_buff[100];

void conectarWifi();
bool conectarBroker();
void verificarStatusDoBroker();
void reconectar();
void verificarConexao();
void efetuarSubscribeDeTopicos();
String ipToString();
void delayMultiTask(long tempo);
void efetuarPublish(char *topic, String mensagem);
void inicializar();
void iniciarModoDeConfiguracao(bool estaConectado);
void configurarWifi(bool tipo);
void mostrarDispositivoConectado(bool estaConectado);
void acionarCarga(String msgString, int porta);
void compararTopicos(String msgString, char *topic);
void inicializarAtualizacao(String msgString);
void resetarDispositivo();
void callback(char *topic, byte *payload, unsigned int length);

WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

//conecta a rede wifi
void conectarWifi()
{
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi conectada");
    Serial.println("IP: ");
    ipDispositivo = WiFi.localIP();
    Serial.println(ipDispositivo);
}

void efetuarSubscribeDeTopicos()
{
    client.subscribe("sala/ventilador");
    client.loop();
    client.subscribe("quarto/ventilador");
    client.loop();
    client.subscribe("cozinha/ventilador");
    client.loop();
    client.subscribe("firmware/ventilador");
    client.loop();
    client.subscribe("dispositivo/atualizacao");
    client.loop();
    client.subscribe("dispositivo/ip");
    client.loop();
    client.subscribe("dispositivo/reset");
    client.loop();
}
//conecta ao broker
bool conectarBroker()
{
    if (client.connect((char *)clientName.c_str()))
    {
        Serial.println("Conectado ao  broker MQTT");

        efetuarSubscribeDeTopicos();

        return true;
    }
    else
    { /*reporta a falha e tenta conectar novamente*/
        Serial.println("Falha na conexao do MQTT");
        verificarStatusDoBroker();

        return false;
    }
}

//verifica o status do broker
void verificarStatusDoBroker()
{
    int status = client.state();

    switch (status)
    {
    case -4:
        Serial.println("MQTT_CONNECTION_TIMEOUT");
        Serial.println("Timeout no keepalive time do servidor");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case -3:
        Serial.println("MQTT_CONNECTION_LOST");
        Serial.println("Perda de conexao com o broker");
        Serial.println("Tentando reconexao");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case -2:
        Serial.println("MQTT_CONNECT_FAILED");
        Serial.println("Falha na conexao com a rede");
        Serial.println("Tentando reconexao");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case -1:
        Serial.println("MQTT_DISCONNECTED");
        Serial.println("Cliente desconectado.");
        Serial.println("Tentando reconexao");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case 0:
        Serial.println("MQTT_CONNECTED");
        Serial.println("Cliente conectado com sucesso.");
        break;
    case 1:
        Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
        Serial.println("Falha de protocolo");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case 2:
        Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
        Serial.println("Id Cliente errado.");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case 3:
        Serial.println("MQTT_CONNECT_UNAVAILABLE");
        Serial.println("Servidor nao disponivel.");
        Serial.println("Tentando reconexao");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case 4:
        Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
        Serial.println("Usuario ou senha invalidos.");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    case 5:
        Serial.println("MQTT_CONNECT_UNAUTHORIZED");
        Serial.println("Cliente nao autorizado para conexao.");
        delayMultiTask(TEMPO_RECONEXAO);
        reconectar();
        break;
    }
}

//tenta reconexão na wifi e no broker
void reconectar()
{
    //conectarWifi();

    while (!client.connected())
    {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");

        if (client.connect((char *)clientName.c_str()))
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            efetuarSubscribeDeTopicos();
        }
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }

    if (!conectarBroker())
    {
        verificarStatusDoBroker();
        return;
    }
}

void verificarConexao()
{
    if (!client.connected())
    {
        reconectar();
    }
}

//delay sem travar o device
void delayMultiTask(long tempo)
{
    unsigned long TEMPO_ATUAL = millis();

    if (TEMPO_INICIAL - TEMPO_ATUAL > tempo)
    {
        TEMPO_INICIAL = TEMPO_ATUAL;
    }
}

//efetua o publish em um determinado tópico
void efetuarPublish(char *topic, String mensagem)
{
    int tamanhoMensagem = mensagem.length();

    for (int i = 0; i < tamanhoMensagem; i++)
    {
        message_buff[i] = mensagem[i];
    }

    if (client.publish(topic, message_buff, true))
    {
        Serial.println("Mensagem Publicada");
        Serial.println((String)message_buff);
    }

    delay(1000);
}

//inicializações da função  de setup - OK
void inicializar()
{
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    pinMode(CARGA, OUTPUT);
    digitalWrite(CARGA, LOW);
    pinMode(CARGA2, OUTPUT);
    digitalWrite(CARGA2, LOW);
    Serial.begin(115200);
    delay(10);

    iniciarModoDeConfiguracao(true);
    conectarWifi();
    conectarBroker();

    Serial.print("Conectando a ");
    Serial.print(server);
    Serial.print(" como ");
    Serial.println(clientName);

    efetuarPublish("firmware/ventilador", versaoFirmware);
    efetuarPublish("dispositivo/ip", ipToString(ipDispositivo));
}

//transforma ip para uma string - OK
String ipToString(IPAddress ip)
{
    String s = "";
    for (int i = 0; i < 4; i++)
        s += i ? "." + String(ip[i]) : String(ip[i]);
    return s;
}

//inicia o modo de configuração - OK
void iniciarModoDeConfiguracao(bool estaConectado)
{
    if (!estaConectado)
    {
        configurarWifi(false);
    }
    else
    {
        configurarWifi(true);
    }

    return;
}

//configura a rede wifi - OK
void configurarWifi(bool tipo)
{
    WiFiManager wifiManager;

    if (tipo)
    {
        mostrarDispositivoConectado(false);

        wifiManager.autoConnect("BL-DISPOSITIVO-01", "bl_ioT01");

        Serial.println("Conectado com sucesso!");

        mostrarDispositivoConectado(true);
    }
    else
    {
        mostrarDispositivoConectado(false);

        wifiManager.autoConnect("BL-DISPOSITIVO-01", "bl_ioT01");

        Serial.println("Conectado com sucesso!");

        mostrarDispositivoConectado(true);
    }
}

//mostra se o dispositivo está conectado - OK
void mostrarDispositivoConectado(bool estaConectado)
{
    if (!estaConectado)
    {
        digitalWrite(LED_CONF, HIGH);
        delay(TEMPO_ESPERA_CONFIGURACAO);
    }
    else
    {
        for (int a = 0; a < 3; a++)
        {
            digitalWrite(LED_CONF, HIGH);
            delay(TEMPO_ESPERA_CONFIGURACAO);
            digitalWrite(LED_CONF, LOW);
            delay(TEMPO_ESPERA_CONFIGURACAO);
        }
    }
}

//aciona uma carga - OK
void acionarCarga(String msgString, int porta)
{
    Serial.println("carga ligada");
    digitalWrite(porta, HIGH);
    delay(TEMPO_ACIONAMENTO);
    digitalWrite(porta, LOW);
    Serial.println("carga  desligada");
}

//verifica qual tópico foi recebido - OK
void compararTopicos(String msgString, char *topic)
{
    int comparacaoDeTopico = strcmp(topic, "sala/ventilador");

    if (comparacaoDeTopico == 0)
    {
        acionarCarga(msgString, 4);
    }

    comparacaoDeTopico = strcmp(topic, "quarto/ventilador");

    if (comparacaoDeTopico == 0)
    {
        acionarCarga(msgString, 5);
    }

    comparacaoDeTopico = strcmp(topic, "cozinha/ventilador");

    if (comparacaoDeTopico == 0)
    {
        acionarCarga(msgString, 14);
    }

    comparacaoDeTopico = strcmp(topic, "dispositivo/atualizacao");

    if (comparacaoDeTopico == 0)
    {
        inicializarAtualizacao(msgString);
    }

    comparacaoDeTopico = strcmp(topic, "dispositivo/reset");

    if (comparacaoDeTopico == 0)
    {
        resetarDispositivo();
    }
}

//inicializa a atualização de firmware - OK
void inicializarAtualizacao(String msgString)
{
    digitalWrite(LED_CONF, HIGH);

    t_httpUpdate_return ret = ESPhttpUpdate.update(server, 80, dirUpdate);

    Serial.println("RETORNO DA ATUALIZACAO");
    Serial.println(ret);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        Serial.println("A ATUALIZACAO FALHOU");
        break;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("NAO EXISTEM NOVAS ATUALIZACOES");
        break;

    case HTTP_UPDATE_OK:
        Serial.println("ATUALIZACAO EFETUADA COM SUCESSO!!");

        efetuarPublish("dispositivo/atualizacao", msgString);

        digitalWrite(LED_CONF, LOW);

        ESP.restart();
        break;
    }
}

void resetarDispositivo()
{
    ESP.restart();
}

//recupera os incomings de mensagens
void callback(char *topic, byte *payload, unsigned int length)
{
    int i = 0;

    Serial.println("Mensagem chegou:  topic: " + String(topic));
    Serial.println("Tamanho: " + String(length, DEC));

    for (i = 0; i < length; i++)
    {
        message_buff[i] = payload[i];
    }
    message_buff[i] = '\0';

    String msgString = String(message_buff);

    Serial.println("Payload: " + msgString);

    compararTopicos(msgString, topic);
}

void setup()
{
    inicializar();
}

void loop()
{
    client.loop();
}