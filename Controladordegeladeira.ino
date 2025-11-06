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

// Armazenamento de das medias(Daniel)
float temperaturaSomaIntervalo[6] = {0};
int temperaturaContagemIntervalo[6] = {0};
//

bool finalizeRequested = false;
bool mainServerStarted = false;

const char* scriptUrl = "https://script.google.com/macros/s/AKfycbzvI1MD2EVyt0Ghqzj2UNYmdOiTAhQKQK2J67MAmQG54BcDI8Feo6nEysiUbWituZPX/exec";

String savedSSID, savedPass;

// Controle de tempo para leituras do DHT
unsigned long ultimaMedicao = 0;
const unsigned long intervaloMedicao = 5000; // 5 segundos

// ------------------- Funções ------------------- //

void enviarDados() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(scriptUrl) +
                 "?temperatura=" + String(temperaturaAtual) +
                 "&umidade=" + String(umidadeAtual);

    http.begin(url);
    int httpResponseCode = http.GET();

    Serial.print("Código HTTP: ");
    Serial.println(httpResponseCode);

    String resposta = http.getString();
    Serial.print("Resposta do servidor: ");
    Serial.println(resposta);


    if (httpResponseCode > 0) {
      Serial.println("Dados enviados com sucesso!");
    } else {
      Serial.print("Erro ao enviar dados: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi desconectado, não foi possível enviar os dados.");
  }
}

// Leitura do sensor (não bloqueante)
void medirSensor() {
    unsigned long agora = millis();
    if (agora - ultimaMedicao >= intervaloMedicao) {
        ultimaMedicao = agora;
        float t = dht.readTemperature();
        float u = dht.readHumidity();
        
        // Se leitura inválida, ignora e sai
        if (isnan(t) || isnan(u)) return;

        temperaturaBuffer += t;
        umidadeBuffer += u;
        contadorMedicoes++;

        // Quando atingir 10 medições, calcula média e envia
        if (contadorMedicoes >= 12) {
            temperaturaAtual = temperaturaBuffer / contadorMedicoes;
            umidadeAtual = umidadeBuffer / contadorMedicoes;
            
            //Deteminar qual periodo de 4h estamos(Daniel)
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
              int periodo = timeinfo.tm_hour / 4; // 0..5

              temperaturaSomaIntervalo[periodo] += temperaturaAtual;
              temperaturaContagemIntervalo[periodo]++;
            }

            if (getLocalTime(&timeinfo)) {
                if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec < 5) {
                    memset(temperaturaSomaIntervalo, 0, sizeof(temperaturaSomaIntervalo));
                    memset(temperaturaContagemIntervalo, 0 , sizeof(temperaturaContagemIntervalo));
                    Serial.println("Buffers de média resetados para novo dia.");
                }
            }
            //
            
            Serial.printf("Média das últimas 10 medições:\nTemperatura: %.1f°C, Umidade: %.1f%%\n",
                          temperaturaAtual, umidadeAtual);
            enviarDados();

            temperaturaBuffer = 0;
            umidadeBuffer = 0;
            contadorMedicoes = 0;
        }
    }
}

// ------------------- Páginas HTML ------------------- //

const char* paginaConfig = R"rawliteral(
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

String paginaConectando() {
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
.gray { background: #bdc3c7}
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
<option>Fruta</option><option>Verdura</option><option>Legume</option>
<option>Carne</option><option>Bebida</option><option>Outro</option>
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
</div>

<script>
// Valor coletado pelo ESP32 e substituido antes do envio
let buf = %BUF%; 

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
    linha.innerHTML=`
      <td>${buf}</td>
      <td>${nome}</td>
      <td>${categoria}</td>
      <td>${quantidade}</td>
      <td class="status-termico"></td>`;
    tabela.appendChild(linha);
    form.reset();

    // ENVIA PARA O GOOGLE SHEETS
    const url = "https://script.google.com/macros/s/AKfycbxP_y9Tyglv1GjYUlPTYyaUSs9jOzsEhyANuUAspPoCRVqsxQJw0t7OA68oAnCMgSWw/exec" +
      `?dataHora=${encodeURIComponent(buf)}` +
      `&nome=${encodeURIComponent(nome)}` +
      `&categoria=${encodeURIComponent(categoria)}` +
      `&quantidade=${encodeURIComponent(quantidade)}`;
    fetch(url)
      .then(r => r.text())
      .then(txt => console.log("Resposta do Google:", txt))
      .catch(err => console.error("Erro ao enviar:", err));
  }
});

const scriptURL = "https://script.google.com/macros/s/AKfycbxP_y9Tyglv1GjYUlPTYyaUSs9jOzsEhyANuUAspPoCRVqsxQJw0t7OA68oAnCMgSWw/exec";

function carregarDadosGoogle() {
  fetch(scriptURL + "?acao=listar")
    .then(response => response.json())
    .then(dados => {
      const tabela = document.querySelector("#tabela-alimentos tbody");
      tabela.innerHTML = ""; // limpa tabela atual

      dados.forEach(item => {
        const linha = document.createElement("tr");
        linha.innerHTML = `
        <td>${item["dataHora"] || ""}</td>
        <td>${item["nome"] || ""}</td>
        <td>${item["categoria"] || ""}</td>
        <td>${item["quantidade"] || ""}</td>
        `;
        tabela.appendChild(linha);
      });
    })
    .catch(err => console.error("Erro ao carregar dados:", err));
}

// Chama a função ao iniciar
window.addEventListener("load", carregarDadosGoogle);

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

    linhas.forEach(linha => {
        const dataHora = linha.children[0].innerText;
        const cel = linha.querySelector(".status-termico");

        if(!cel || !dataHora) return;

        const cadastro = new Date(dataHora);
        const periodoCadastro = Math.floor(cadastro.getHours() / 4);

        cel.innerHTML = "";

        for (let i = 0; i < 6; i++) {
            let classe = "gray"; // padrão = sem histórico

            if(i >= periodoCadastro) {
                const m = medias[i] || 0;
                if(m < 4) classe = "green";
                else if(m <= 4.5) classe = "yellow";
                else classe = "red";
            }

            cel.innerHTML += `<span class="circle ${classe}"></span>`;
        }
    });
}

setInterval(atualizarStatusTermico, 10000);
window.addEventListener("load", atualizarStatusTermico);
</script>
</body>
</html>
)rawliteral";

// ------------------- Handlers ------------------- //
void handlePortalRoot() { portalServer.send(200,"text/html",paginaConfig); }

void handleSalvar() {
    savedSSID = portalServer.arg("ssid");
    savedPass = portalServer.arg("senha");
    Serial.println("Credenciais: "+savedSSID);
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    portalServer.send(200,"text/html",paginaConectando());
}

void handleStatus() {
    String json="{\"connected\":";
    if(WiFi.status()==WL_CONNECTED){ json+="true,\"ip\":\""+WiFi.localIP().toString()+"\""; }
    else json+="false";
    json+="}";
    portalServer.send(200,"application/json",json);
}

void handleFinalize() { portalServer.send(200,"text/plain","OK"); finalizeRequested=true; }

void handleMainRoot() {
  struct tm timeinfo;
  char buf[32] = "";
  if (getLocalTime(&timeinfo)) {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }

  String html = paginaPrincipalHTML;
  html.replace("%BUF%", String("\"") + buf + "\""); // substitui marcador pelo valor real da hora
  mainServer.send(200, "text/html", html);
}

void handleDados() {
    String json="{\"temperatura\":"+String(temperaturaAtual)+",\"umidade\":"+String(umidadeAtual)+"}";
    mainServer.send(200,"application/json",json);
}

//(Daniel)
void handleIntervalos() {
  String json = "[";
  for (int i = 0; i < 6; i++) {
    float media = 0;
    if (temperaturaContagemIntervalo[i] > 0) {
      media = temperaturaSomaIntervalo[i] / temperaturaContagemIntervalo[i];
    }
    json += String(media, 2);
    if (i < 5) json += ",";
  }
  json += "]";
  mainServer.send(200, "application/json", json);
}
//

void handleCadastrar() {
    String p = mainServer.arg("produto");
    mainServer.send(200,"text/html","<p>Produto cadastrado: "+p+"</p><a href='/'>Voltar</a>");
}

// ------------------- Setup ------------------- //
void setup() {
    Serial.begin(115200);

    dht.begin();

    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_Config","12345678");
    Serial.println("AP iniciado, IP: "+WiFi.softAPIP().toString());

    portalServer.on("/",HTTP_GET,handlePortalRoot);
    portalServer.on("/salvar",HTTP_POST,handleSalvar);
    portalServer.on("/status",HTTP_GET,handleStatus);
    portalServer.on("/finalize",HTTP_POST,handleFinalize);
    portalServer.begin();
    Serial.println("Portal rodando");
}

// ------------------- Loop ------------------- //
void loop() {
    portalServer.handleClient();
    medirSensor();

    if(finalizeRequested && !mainServerStarted){
        Serial.println("Finalizando AP e iniciando servidor principal...");
        portalServer.stop();
        WiFi.softAPdisconnect(true);
        delay(300);

      configTime(-3 * 3600, 0, "pool.ntp.org");
      Serial.println("Aguardando sincronização...");
      struct tm timeinfo;
      while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nHora sincronizada!");

        mainServer.on("/",HTTP_GET,handleMainRoot);
        mainServer.on("/dados",HTTP_GET,handleDados);
        mainServer.on("/cadastrar",HTTP_POST,handleCadastrar);
        
        //(Daniel)
        mainServer.on("/intervalos", HTTP_GET, handleIntervalos);
        //

        mainServer.begin();
        mainServerStarted=true;
        finalizeRequested=false;
        Serial.println("Servidor principal iniciado. IP: "+WiFi.localIP().toString());
    }

    if(mainServerStarted){ mainServer.handleClient();
    }
}
