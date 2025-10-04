#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// Configurações do Wi-Fi

const char* ssid = "GERMANE_GSM-2.4G";
const char* senha = "24012004";

/*
const char* ssid = "Padaria Cremosa";          
const char* senha = "abcdefghijklmnopqrstuvxywz1234";
*/

// Configurações do DHT11
#define PINO_DHT 4     // Pino conectado ao DHT11 (substitua pelo pino correto)
#define TIPO_DHT DHT11
DHT dht(PINO_DHT, TIPO_DHT);

//

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
    String html = "<!DOCTYPE html><html lang='pt-BR'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 - Cadastro e DHT11</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#f0f2f5;margin:0;padding:20px;}";
    html += "h1{text-align:center;color:#333;}";
    html += ".tab-buttons{text-align:center;margin-bottom:20px;}";
    html += ".tab-buttons button{padding:10px 20px;margin:5px;border:none;border-radius:5px;";
    html += "background:#28a745;color:#fff;font-weight:bold;cursor:pointer;}";
    html += ".tab-buttons button.active{background:#218838;}";
    html += ".tab{display:none;max-width:600px;background:#fff;padding:20px;margin:0 auto;";
    html += "border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}";
    html += "label{font-weight:bold;display:block;margin-top:10px;}";
    html += "input,select,button[type=submit]{width:100%;padding:8px;margin-top:5px;";
    html += "border:1px solid #ccc;border-radius:5px;}";
    html += "button[type=submit]{background:#28a745;color:#fff;font-weight:bold;cursor:pointer;}";
    html += "button[type=submit]:hover{background:#218838;}";
    html += "table{width:100%;border-collapse:collapse;margin-top:20px;}";
    html += "th,td{padding:10px;text-align:left;border-bottom:1px solid #ccc;}";
    html += "th{background:#28a745;color:#fff;}";
    html += "</style>";
    html += "</head><body>";

    html += "<h1>Plataforma ESP32</h1>";

    // Botões de navegação
    html += "<div class='tab-buttons'>";
    html += "<button id='btnCadastro' class='active' onclick=\"showTab('cadastro')\">Cadastro</button>";
    html += "<button id='btnDht' onclick=\"showTab('dht')\">Medição DHT11</button>";
    html += "</div>";

    // Aba Cadastro
    html += "<div id='cadastro' class='tab' style='display:block;'>";
    html += "<form id='form-alimento'>";
    html += "<label>Nome do Alimento:</label>";
    html += "<input type='text' id='nome' required>";
    html += "<label>Categoria:</label>";
    html += "<select id='categoria' required>";
    html += "<option value=''>Selecione...</option>";
    html += "<option>Fruta</option><option>Verdura</option><option>Legume</option>";
    html += "<option>Carne</option><option>Bebida</option><option>Outro</option>";
    html += "</select>";
    html += "<label>Quantidade:</label>";
    html += "<input type='number' id='quantidade' min='1' required>";
    html += "<button type='submit'>Cadastrar</button>";
    html += "</form>";
    html += "<table id='tabela-alimentos'><thead><tr>";
    html += "<th>Nome</th><th>Categoria</th><th>Quantidade</th>";
    html += "</tr></thead><tbody></tbody></table>";
    html += "</div>";

    // Aba DHT11
    html += "<div id='dht' class='tab'>";
    html += "<h2>Leituras do Sensor DHT11</h2>";
    html += "<p>Temperatura: <span id='temperatura'>Carregando...</span></p>";
    html += "<p>Umidade: <span id='umidade'>Carregando...</span></p>";
    html += "</div>";

    // Script
    html += "<script>";
    html += "function showTab(tab){";
    html += "document.getElementById('cadastro').style.display=(tab==='cadastro')?'block':'none';";
    html += "document.getElementById('dht').style.display=(tab==='dht')?'block':'none';";
    html += "document.getElementById('btnCadastro').classList.toggle('active',tab==='cadastro');";
    html += "document.getElementById('btnDht').classList.toggle('active',tab==='dht');";
    html += "}";

    // Cadastro de alimentos
    html += "const form=document.getElementById('form-alimento');";
    html += "const tabela=document.querySelector('#tabela-alimentos tbody');";
    html += "form.addEventListener('submit',e=>{";
    html += "e.preventDefault();";
    html += "const nome=document.getElementById('nome').value.trim();";
    html += "const categoria=document.getElementById('categoria').value;";
    html += "const quantidade=document.getElementById('quantidade').value;";
    html += "if(nome&&categoria&&quantidade){";
    html += "const linha=document.createElement('tr');";
    html += "linha.innerHTML=`<td>${nome}</td><td>${categoria}</td><td>${quantidade}</td>`;";
    html += "tabela.appendChild(linha);form.reset();}});";

    // Atualização do DHT
    html += "function atualizarDados(){";
    html += "fetch('/dados').then(r=>r.json()).then(data=>{";
    html += "document.getElementById('temperatura').innerHTML=data.temperatura+' &#8451;';";
    html += "document.getElementById('umidade').innerHTML=data.umidade+' %';";
    html += "});}";
    html += "setInterval(atualizarDados,5000);";
    html += "atualizarDados();";
    html += "</script>";

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
