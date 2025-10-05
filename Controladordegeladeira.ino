#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

// Configurações do Wi-Fi
const char* ssid = "GERMANE_GSM-2.4G";
const char* senha = "24012004";

// Configurações do DHT11
#define PINO_DHT 4
#define TIPO_DHT DHT11
DHT dht(PINO_DHT, TIPO_DHT);

// Servidor web
WebServer servidor(80);

// Variáveis de leitura
float temperaturaAtual = 0;
float umidadeAtual = 0;

// Função para medir o sensor
void medirSensor() {
    float t = dht.readTemperature();
    float u = dht.readHumidity();
    if (!isnan(t) && !isnan(u)) {
        temperaturaAtual = t;
        umidadeAtual = u;
        Serial.printf("Temperatura: %.1f°C, Umidade: %.1f%%\n", t, u);
    } else {
        Serial.println("Falha ao ler o DHT11.");
    }
}

// Página HTML
void tratarRaiz() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang='pt-BR'>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<title>ESP32 - Cadastro e DHT11</title>
<style>
body{font-family:Arial;background:#f0f2f5;margin:0;padding:20px;}
h1{text-align:center;color:#333;}
.tab-buttons{text-align:center;margin-bottom:20px;}
.tab-buttons button{padding:10px 20px;margin:5px;border:none;border-radius:5px;
background:#28a745;color:#fff;font-weight:bold;cursor:pointer;}
.tab-buttons button.active{background:#218838;}
.tab{display:none;max-width:600px;background:#fff;padding:20px;margin:0 auto;
border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}
label{font-weight:bold;display:block;margin-top:10px;}
input,select,button[type=submit]{width:100%;padding:8px;margin-top:5px;
border:1px solid #ccc;border-radius:5px;}
button[type=submit]{background:#28a745;color:#fff;font-weight:bold;cursor:pointer;}
button[type=submit]:hover{background:#218838;}
table{width:100%;border-collapse:collapse;margin-top:20px;}
th,td{padding:10px;text-align:left;border-bottom:1px solid #ccc;}
th{background:#28a745;color:#fff;}
</style>
</head>
<body>
<h1>Plataforma ESP32</h1>
<div class='tab-buttons'>
<button id='btnCadastro' class='active' onclick="showTab('cadastro')">Cadastro</button>
<button id='btnDht' onclick="showTab('dht')">Medição DHT11</button>
</div>

<div id='cadastro' class='tab' style='display:block;'>
<form id='form-alimento'>
<label>Nome do Alimento:</label>
<input type='text' id='nome' required>
<label>Categoria:</label>
<select id='categoria' required>
<option value=''>Selecione...</option>
<option>Fruta</option><option>Verdura</option><option>Legume</option>
<option>Carne</option><option>Bebida</option><option>Outro</option>
</select>
<label>Quantidade:</label>
<input type='number' id='quantidade' min='1' required>
<button type='submit'>Cadastrar</button>
</form>
<table id='tabela-alimentos'><thead><tr>
<th>Nome</th><th>Categoria</th><th>Quantidade</th>
</tr></thead><tbody></tbody></table>
</div>

<div id='dht' class='tab'>
<h2>Leituras do Sensor DHT11</h2>
<p>Temperatura: <span id='temperatura'>Carregando...</span></p>
<p>Umidade: <span id='umidade'>Carregando...</span></p>
</div>

<script>
function showTab(tab){
document.getElementById('cadastro').style.display=(tab==='cadastro')?'block':'none';
document.getElementById('dht').style.display=(tab==='dht')?'block':'none';
document.getElementById('btnCadastro').classList.toggle('active',tab==='cadastro');
document.getElementById('btnDht').classList.toggle('active',tab==='dht');
}

const form=document.getElementById('form-alimento');
const tabela=document.querySelector('#tabela-alimentos tbody');
form.addEventListener('submit', e=>{
e.preventDefault();
const nome=document.getElementById('nome').value.trim();
const categoria=document.getElementById('categoria').value;
const quantidade=document.getElementById('quantidade').value;
if(nome && categoria && quantidade){
const linha=document.createElement('tr');
linha.innerHTML=`<td>${nome}</td><td>${categoria}</td><td>${quantidade}</td>`;
tabela.appendChild(linha);
form.reset();
}});

function atualizarDados(){
fetch('/dados').then(r=>r.json()).then(data=>{
document.getElementById('temperatura').innerHTML=data.temperatura+' &#8451;';
document.getElementById('umidade').innerHTML=data.umidade+' %';
});
}
setInterval(atualizarDados,5000);
atualizarDados();
</script>
</body>
</html>
)rawliteral";

    servidor.send(200, "text/html", html);
}

// Endpoint JSON
void tratarDados() {
    String json = "{\"temperatura\":" + String(temperaturaAtual) +
                  ",\"umidade\":" + String(umidadeAtual) + "}";
    servidor.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    dht.begin();

    WiFi.begin(ssid, senha);
    Serial.println("Conectando ao Wi-Fi...");
    while (WiFi.status() != WL_CONNECTED) { delay(1000); Serial.print("."); }
    Serial.printf("\nWi-Fi conectado! IP: %s\n", WiFi.localIP().toString().c_str());

    servidor.on("/", tratarRaiz);
    servidor.on("/dados", tratarDados);
    servidor.begin();
    Serial.println("Servidor iniciado!");
}

void loop() {
    servidor.handleClient();
    medirSensor();
    delay(5000);
}
