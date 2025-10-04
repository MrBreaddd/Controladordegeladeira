#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// Configurações do Wi-Fi
const char* ssid = "seu ssid";          // Substitua pelo nome da sua rede Wi-Fi
const char* senha = "sua senha";         // Substitua pela senha da sua rede Wi-Fi

// Configurações do DHT11
#define PINO_DHT 4     // Pino conectado ao DHT11 (substitua pelo pino correto)
#define TIPO_DHT DHT11
DHT dht(PINO_DHT, TIPO_DHT);

// Servidor web
WebServer servidor(80);

// Variáveis para armazenar as medições atuais
float temperaturaAtual = 0.0;
float umidadeAtual = 0.0;

// Função para realizar medições
void medirSensor() {
    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();

    if (!isnan(temperatura) && !isnan(umidade)) {
        temperaturaAtual = temperatura;
        umidadeAtual = umidade;

        Serial.print("Temperatura: ");
        Serial.print(temperaturaAtual);
        Serial.print("°C, Umidade: ");
        Serial.print(umidadeAtual);
        Serial.println("%");
    } else {
        Serial.println("Falha ao ler o sensor DHT11.");
    }
}

// Página HTML
void tratarRaiz() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<title>Leituras do DHT11</title>";
    html += "<script>";
    html += "function atualizarDados() {";
    html += "  fetch('/dados').then(response => response.json()).then(data => {";
    html += "    document.getElementById('temperatura').innerHTML = data.temperatura + ' &#8451;';";
    html += "    document.getElementById('umidade').innerHTML = data.umidade + ' %';";
    html += "  });";
    html += "}";    
    html += "setInterval(atualizarDados, 5000);"; // Atualiza a cada 5 segundos
    html += "</script></head><body>";
    html += "<h1>Leituras do Sensor DHT11</h1>";
    html += "<p>Temperatura: <span id='temperatura'>Carregando...</span></p>";
    html += "<p>Umidade: <span id='umidade'>Carregando...</span></p>";
    html += "</body></html>";

    servidor.send(200, "text/html", html);
}

// Endpoint para enviar dados em formato JSON
void tratarDados() {
    String json = "{";
    json += "\"temperatura\": " + String(temperaturaAtual) + ",";
    json += "\"umidade\": " + String(umidadeAtual);
    json += "}";
    servidor.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    dht.begin();

    // Conecta ao Wi-Fi
    WiFi.begin(ssid, senha);
    Serial.println("Conectando ao Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());

    // Configura o servidor
    servidor.on("/", tratarRaiz);
    servidor.on("/dados", tratarDados); // Endpoint para enviar os dados
    servidor.begin();
    Serial.println("Servidor iniciado!");
}

void loop() {
    servidor.handleClient(); // Processa solicitações do cliente
    medirSensor();           // Faz a medição do sensor
    delay(5000);             // Intervalo de 5 segundos entre as medições
}