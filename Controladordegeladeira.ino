#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <HTTPClient.h>
#include "time.h"

#define PINO_DHT 4
#define TIPO_DHT DHT11
DHT dht(PINO_DHT, TIPO_DHT);

WebServer portalServer(80);
WebServer mainServer(80);

float temperaturaAtual = 0;
float temperaturaBuffer = 0;
float umidadeAtual = 0;
float umidadeBuffer = 0;
int contadorMedicoes = 0;

float temperaturaSomaIntervalo[6] = {0};
int temperaturaContagemIntervalo[6] = {0};
// Guarda o último dia (1..31) que foi registrado para reset diário
int ultimoDiaRegistrado = -1;

bool finalizeRequested = false;
bool mainServerStarted = false;

const char *scriptUrl = "https://script.google.com/macros/s/AKfycbzBekL3EUEMCUWtXMaSjFvEfCf8WptoNX-9bC5fnm_tpxOjPiO-k2blqzBMWDy4Ywml/exec";

String savedSSID, savedPass;

// Controle de tempo para leituras do DHT
unsigned long ultimaMedicao = 0;
const unsigned long intervaloMedicao = 10000; // 5 segundos

// ------------------- Funções -------------------

void enviarDados()
{
  if (WiFi.status() == WL_CONNECTED){
    HTTPClient http;

    // Obter hora atual do ESP32
    struct tm timeinfo;
    char buf[32] = "";
    if (getLocalTime(&timeinfo)) {
      strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

    // Montar URL com todos os parâmetros corretamente concatenados
    String url = String(scriptUrl) +
                 "?temperatura=" + String(temperaturaAtual) +
                 "&umidade=" + String(umidadeAtual) +
                 "&acao=add" +
                 "&dataHora=" + String(buf) +
                 "&nome=ESP32" +
                 "&categoria=Sensor" +
                 "&quantidade=1";

    Serial.println("Enviando para: " + url); // debug

    // Iniciar requisição HTTP
    http.begin(url);
    int httpResponseCode = http.GET();

    Serial.print("Código HTTP: ");
    Serial.println(httpResponseCode);

    String resposta = http.getString();
    Serial.print("Resposta do servidor: ");
    Serial.println(resposta);

    if (httpResponseCode > 0){
      Serial.println("Dados enviados com sucesso!");
    }
    else{
      Serial.print("Erro ao enviar dados: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
  else{
    Serial.println("WiFi desconectado, não foi possível enviar os dados.");
  }
}

// Leitura do sensor (não bloqueante)
void medirSensor(){
  unsigned long agora = millis();
  if (agora - ultimaMedicao >= intervaloMedicao){
    ultimaMedicao = agora;
    float t = dht.readTemperature();
    float u = dht.readHumidity();

    // Se leitura inválida, ignora e sai
    if (isnan(t) || isnan(u)){
      return;
    }

    temperaturaBuffer += t;
    umidadeBuffer += u;
    contadorMedicoes++;

    // Quando atingir 10 medições, calcula média e envia
    if (contadorMedicoes >= 12){
      temperaturaAtual = temperaturaBuffer / contadorMedicoes;
      umidadeAtual = umidadeBuffer / contadorMedicoes;
    
      // Determinar qual periodo de 4h estamos (apenas UMA chamada)
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)){
        int periodo = timeinfo.tm_hour / 4; // 0..5
      
        // Se o dia mudou antes de adicionarmos, o reset já terá sido feito no loop()
        temperaturaSomaIntervalo[periodo] += temperaturaAtual;
        temperaturaContagemIntervalo[periodo]++;
      }
    
      Serial.printf("Média das últimas %d medições:\nTemperatura: %.1f°C, Umidade: %.1f%%\n",
                    contadorMedicoes, temperaturaAtual, umidadeAtual);
      enviarDados();
      
      temperaturaBuffer = 0;
      umidadeBuffer = 0;
      contadorMedicoes = 0;
    }
  }
}

// ------------------- Páginas HTML -------------------

const char *paginaConfig = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Configurar WiFi</title>
<style>
  body {
    font-family: Arial, sans-serif;
    background-color: #f3f4f6;
    display: flex;
    justify-content: center;
    align-items: center;
    height: 100vh;
    margin: 0;
  }

  .container {
    background-color: #ffffff;
    border-radius: 20px;
    box-shadow: 0 4px 12px rgba(0,0,0,0.1);
    padding: 30px 40px;
    text-align: center;
    width: 90%;
    max-width: 400px;
  }

  h1 {
    font-size: 28px;
    margin-bottom: 20px;
    color: #333333;
  }

  label {
    font-size: 20px;
    display: block;
    text-align: left;
    margin-bottom: 8px;
    color: #444444;
  }

  input[type="text"],
  input[type="password"] {
    font-size: 18px;
    padding: 10px;
    width: 100%;
    border: 2px solid #ccc;
    border-radius: 10px;
    margin-bottom: 20px;
    box-sizing: border-box;
  }

  input[type="submit"] {
    font-size: 20px;
    padding: 10px 20px;
    background-color: #4CAF50;
    color: white;
    border: none;
    border-radius: 12px;
    cursor: pointer;
    transition: background-color 0.3s;
  }

  input[type="submit"]:hover {
    background-color: #45a049;
  }

  p {
    font-size: 18px;
    margin-top: 20px;
    color: #555555;
  }
</style>
</head>
<body>
  <div class="container">
    <h1>Configurar WiFi</h1>
    <form action="/salvar" method="POST">
      <label for="ssid">SSID:</label>
      <input type="text" id="ssid" name="ssid" required>

      <label for="senha">Senha:</label>
      <input type="password" id="senha" name="senha">

      <input type="submit" value="Conectar">
    </form>
    <p>Aguarde a verificação; a página mostrará quando conectado.</p>
  </div>
</body>
</html>
)rawliteral";

String paginaConectando()
{
  return R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset='utf-8'><title>Conectando...</title></head><body>
<h1>Tentando conectar...</h1><p id='status'>Aguardando...</p>
<script>
function check(){
fetch('/status').then(r=>r.json()).then(j=>{
  if(j.connected){
    document.getElementById('status').innerHTML='Conectado! IP:'+j.ip;
    if(!document.getElementById('finalizarBtn')){
      let btn=document.createElement('button');
      btn.id='finalizarBtn';
      btn.innerText='Desativar AP e abrir interface principal';
      btn.onclick=function(){
        fetch('/finalize',{method:'POST'}).then(()=>{
          document.getElementById('status').innerHTML+='<br>Pedido enviado';
          setTimeout(()=>{window.location='http://'+j.ip;},4000);
        });
      };
      document.body.appendChild(btn);
    }
  }else document.getElementById('status').innerHTML='Ainda não conectado...';
}).catch(e=>document.getElementById('status').innerHTML='Erro');}
setInterval(check,1000); check();
</script>
</body></html>
)rawliteral";
}

// Página principal (HTML completo com cadastro e DHT)
String paginaPrincipalHTML = R"rawliteral(
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
.tab{display:none;max-width:700px;background:#fff;padding:20px;margin:0 auto;
border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}
label{font-weight:bold;display:block;margin-top:10px;}
input,select,button[type=submit]{width:100%;padding:8px;margin-top:5px;
border:1px solid #ccc;border-radius:5px;}
button[type=submit]{background:#28a745;color:#fff;font-weight:bold;cursor:pointer;}
button[type=submit]:hover{background:#218838;}
table{width:100%;border-collapse:collapse;margin-top:20px;}
th,td{padding:10px;text-align:left;border-bottom:1px solid #ccc;}
th{background:#28a745;color:#fff;}
.circle {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  display: inline-block;
  margin: 2px;
}
.green { background: #2ecc71; }
.yellow { background: #f1c40f; }
.red { background: #e74c3c; }
.gray { background: #bdc3c7;}
</style>
</head>

<body>
  <h1>Monitor</h1>
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
        <option>Fruta</option>
        <option>Verdura</option>
        <option>Legume</option>
        <option>Carne</option>
        <option>Bebida</option>
        <option>Outro</option>
      </select>
      <label>Quantidade:</label>
      <input type='number' id='quantidade' min='1' required>
      <button type='submit'>Cadastrar</button>
    </form>

<table id='tabela-alimentos'>
<thead>
<tr>
<th>Data e Hora</th>
<th>Nome</th>
<th>Categoria</th>
<th>Quantidade</th>
<th>Status Térmico</th>
</tr>
</thead>
<tbody></tbody>
</table>
</div>

  <div id='dht' class='tab'>
    <h2>Leituras do Sensor DHT11</h2>
    <p>Temperatura: <span id='temperatura'>Carregando...</span></p>
    <p>Umidade: <span id='umidade'>Carregando...</span></p>
    <div id="monitoramento">
      <h3>Monitoramento:</h3>
      <span id="bolinhas">⚪ ⚪ ⚪ ⚪ ⚪ ⚪</span>
    </div>
  </div>

<script>
// Valor coletado pelo ESP32 e substituido antes do envio
let timestamp = Number(%BUF%);
let buf = new Date(timestamp * 1000).toLocaleString("pt-BR"); 

    // Controle das bolinhas progressivas
    let progresso = 0;
    const MAX_BOLINHAS = 6;
    const TEMPERATURA_MIN = 18;
    const TEMPERATURA_MAX = 25;

    function showTab(tab) {
      document.getElementById('cadastro').style.display = (tab === 'cadastro') ? 'block' : 'none';
      document.getElementById('dht').style.display = (tab === 'dht') ? 'block' : 'none';
      document.getElementById('btnCadastro').classList.toggle('active', tab === 'cadastro');
      document.getElementById('btnDht').classList.toggle('active', tab === 'dht');
    }

    const form = document.getElementById('form-alimento');
    const tabela = document.querySelector('#tabela-alimentos tbody');
    form.addEventListener('submit', e => {
      e.preventDefault();
      const nome = document.getElementById('nome').value.trim();
      const categoria = document.getElementById('categoria').value;
      const quantidade = document.getElementById('quantidade').value;

  if(nome && categoria && quantidade){
    const linha=document.createElement('tr');
    linha.innerHTML=`
      <td>${buf}</td>
      <td>${nome}</td>
      <td>${categoria}</td>
      <td>${quantidade}</td>
      <td class="status-termico"></td>`;
    tabela.appendChild(linha);
    form.reset();

    // ENVIA PARA O GOOGLE SHEETS
    const url = "https://script.google.com/macros/s/AKfycbzgZ5aXzNODIQAeN2m4mBY8730AamtX8PkNbDSJuhEkgEkV5OcKDUcuA3hIJZHhrTyW/exec" +
      `?timestamp=${encodeURIComponent(timestamp)}` +
      `&nome=${encodeURIComponent(nome)}` +
      `&categoria=${encodeURIComponent(categoria)}` +
      `&quantidade=${encodeURIComponent(quantidade)}`;
    fetch(url)
      .then(r => r.text())
      .then(txt => console.log("Resposta do Google:", txt))
      .catch(err => console.error("Erro ao enviar:", err));
  }
});

const scriptURL = "https://script.google.com/macros/s/AKfycbzgZ5aXzNODIQAeN2m4mBY8730AamtX8PkNbDSJuhEkgEkV5OcKDUcuA3hIJZHhrTyW/exec";

function carregarDadosGoogle() {
  fetch(scriptURL + "?acao=listar")
    .then(response => response.json())
    .then(dados => {
      const tabela = document.querySelector("#tabela-alimentos tbody");
      tabela.innerHTML = ""; // limpa tabela atual

      dados.forEach(item => {
        const linha = document.createElement("tr");
        const ts = Number(item["timestamp"]);
        const dataLocal = new Date(ts * 1000).toLocaleString("pt-BR");    

        linha.innerHTML = `
          <td>${dataLocal}</td>
          <td>${item["nome"] || ""}</td>
          <td>${item["categoria"] || ""}</td>
          <td>${item["quantidade"] || ""}</td>
          <td class="status-termico"></td>
        `;
            tabela.appendChild(linha);
          });
        })
        .catch(err => console.error("Erro ao carregar dados:", err));
    }

// Chama a função ao iniciar
window.addEventListener("load", () => {
    carregarDadosGoogle();
    setTimeout(atualizarStatusTermico, 1500);
});

function atualizarDados(){
  fetch('/dados').then(r=>r.json()).then(data=>{
    document.getElementById('temperatura').innerHTML=data.temperatura+' &#8451;';
    document.getElementById('umidade').innerHTML=data.umidade+' %';
  });
}
setInterval(atualizarDados,5000);
atualizarDados();
async function atualizarStatusTermico(){
  const resp = await fetch('/intervalos');
  const medias = await resp.json();

  const linhas = document.querySelectorAll("#tabela-alimentos tbody tr");

  const agora = new Date();
  const periodoAgora = Math.floor(agora.getHours() / 4); // período real do momento

  linhas.forEach(linha => {
    const dataHora = linha.children[0].innerText;
    const cel = linha.querySelector(".status-termico");
    if(!cel || !dataHora) return;
    // conversor robusto DD/MM/YYYY para Date
    function parseData(dataString){
        const partes = dataString.split(/[/ :]/); 
        if (partes.length >= 5){
            const [dia, mes, ano, hora, min, seg] = partes;
            return new Date(`${ano}-${mes}-${dia}T${hora}:${min}:${seg || "00"}`);
        }
        return new Date(dataString);
    }
    const cadastro = parseData(dataHora);
    const periodoCadastro = Math.floor(cadastro.getHours() / 4);
    cel.innerHTML = "";
    
    for (let i = 0; i < 6; i++) {

      let classe = "gray";

        // período antes do cadastro -> sem histórico
      if (i < periodoCadastro) {
        classe = "gray";

        // futuro do dia ainda não chegou
      } else if (i > periodoAgora) {
        classe = "gray";

        // período atual ou passado -> verifica se há média
      } else {
          // medias[i] pode ser null (sem dados) — se for, mantemos gray
          const m = medias[i];

          if (m === null || m === undefined) {
            classe = "gray";
          } else {
              const media = Number(m); // já é número, mas garantimos
              if (media < 4) classe = "green";
              else if (media <= 4.5) classe = "yellow";
              else classe = "red";
          }
      }      
      cel.innerHTML += `<span class="circle ${classe}"></span>`;
    }
  });
}

setInterval(atualizarStatusTermico, 10000);
window.addEventListener("load", () => {
    setTimeout(atualizarStatusTermico, 1500);
});
</script>
</body>
</html>
)rawliteral";

// ------------------- Handlers ------------------- //
void handlePortalRoot() { portalServer.send(200, "text/html", paginaConfig); }

void handleSalvar()
{
  savedSSID = portalServer.arg("ssid");
  savedPass = portalServer.arg("senha");
  Serial.println("Credenciais: " + savedSSID);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  portalServer.send(200, "text/html", paginaConectando());
}

void handleStatus(){
  String json = "{\"connected\":";
  if (WiFi.status() == WL_CONNECTED){
    json += "true,\"ip\":\"" + WiFi.localIP().toString() + "\"";
  }
  else{
    json += "false";}
  json += "}";
  portalServer.send(200, "application/json", json);
}

void handleFinalize(){
  portalServer.send(200, "text/plain", "OK");
  finalizeRequested = true;
}

void handleMainRoot()
{
  time_t now;
  time(&now);

  String html = paginaPrincipalHTML;
  html.replace("%BUF%", String(now)); // envia o timestamp no lugar de data formatada
  mainServer.send(200, "text/html", html);
}

void handleDados()
{
  String json = "{\"temperatura\":" + String(temperaturaAtual) + ",\"umidade\":" + String(umidadeAtual) + "}";
  mainServer.send(200, "application/json", json);
}

void handleIntervalos(){
  String json = "[";
  for (int i = 0; i < 6; i++)  {
    if (temperaturaContagemIntervalo[i] > 0){
      float media = temperaturaSomaIntervalo[i] / temperaturaContagemIntervalo[i];
      json += String(media, 2);
    }
    else{
      json += "null"; // sinaliza explicitamente "sem dados" no JSON
    }

    if (i < 5){
      json += ",";
    }
  }
  json += "]";
  mainServer.send(200, "application/json", json);
}

void handleCadastrar()
{
  String p = mainServer.arg("produto");
  mainServer.send(200, "text/html", "<p>Produto cadastrado: " + p + "</p><a href='/'>Voltar</a>");
}

// ------------------- Setup ------------------- //
void setup()
{
  Serial.begin(115200);

  dht.begin();

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("AP iniciado, IP: " + WiFi.softAPIP().toString());

  portalServer.on("/", HTTP_GET, handlePortalRoot);
  portalServer.on("/salvar", HTTP_POST, handleSalvar);
  portalServer.on("/status", HTTP_GET, handleStatus);
  portalServer.on("/finalize", HTTP_POST, handleFinalize);
  portalServer.begin();
  Serial.println("Portal rodando");
}

// ------------------- Loop ------------------- //
void loop(){
  portalServer.handleClient();
  // --- Reset diário robusto (executa independentemente das medições) ---
  struct tm timeinfoLoop;
  if (getLocalTime(&timeinfoLoop)){
    if (ultimoDiaRegistrado == -1){
      // primeiro boot: inicializa
      ultimoDiaRegistrado = timeinfoLoop.tm_mday;
    } else if (timeinfoLoop.tm_mday != ultimoDiaRegistrado){
      // dia mudou -> reseta buffers
      memset(temperaturaSomaIntervalo, 0, sizeof(temperaturaSomaIntervalo));
      memset(temperaturaContagemIntervalo, 0, sizeof(temperaturaContagemIntervalo));
      ultimoDiaRegistrado = timeinfoLoop.tm_mday;
      Serial.println("Buffers de média resetados (mudança de dia).");
    }
  }

  medirSensor();

  if (finalizeRequested && !mainServerStarted){
    Serial.println("Finalizando AP e iniciando servidor principal...");
    portalServer.stop();
    WiFi.softAPdisconnect(true);
    delay(300);

    configTime(-3 * 3600, 0, "pool.ntp.org");
    Serial.println("Aguardando sincronização...");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo))
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nHora sincronizada!");

    mainServer.on("/", HTTP_GET, handleMainRoot);
    mainServer.on("/dados", HTTP_GET, handleDados);
    mainServer.on("/cadastrar", HTTP_POST, handleCadastrar);

    //(Daniel)
    mainServer.on("/intervalos", HTTP_GET, handleIntervalos);
    //

    mainServer.begin();
    mainServerStarted = true;
    finalizeRequested = false;
    Serial.println("Servidor principal iniciado. IP: " + WiFi.localIP().toString());
  }

  if (mainServerStarted){
    mainServer.handleClient();
  }
}
