#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>

#define PINO_DHT 4
#define TIPO_DHT DHT11
DHT dht(PINO_DHT, TIPO_DHT);

WebServer portalServer(80);
WebServer mainServer(80);

float temperaturaAtual = 0;
float umidadeAtual = 0;

bool finalizeRequested = false;
bool mainServerStarted = false;

String savedSSID, savedPass;

// Controle de tempo para leituras do DHT
unsigned long ultimaMedicao = 0;
const unsigned long intervaloMedicao = 5000; // 5 segundos

// ------------------- Funções -------------------

// Leitura do sensor (não bloqueante)
void medirSensor() {
    unsigned long agora = millis();
    if (agora - ultimaMedicao >= intervaloMedicao) {
        ultimaMedicao = agora;
        float t = dht.readTemperature();
        float u = dht.readHumidity();
        temperaturaAtual = isnan(t) ? 0 : t;
        umidadeAtual = isnan(u) ? 0 : u;
        Serial.printf("Temperatura: %.1f°C, Umidade: %.1f%%\n", temperaturaAtual, umidadeAtual);
    }
}

// ------------------- Páginas HTML -------------------

const char* paginaConfig = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Configurar WiFi</title>
<style>body{font-family:Arial;padding:12px}</style></head>
<body>
<h1>Configurar WiFi</h1>
<form action="/salvar" method="POST">
SSID:<br><input type="text" name="ssid" required><br>
Senha:<br><input type="password" name="senha"><br><br>
<input type="submit" value="Conectar">
</form>
<p>Aguarde a verificação; a página mostrará quando conectado.</p>
</body></html>
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

// ------------------- Handlers -------------------
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

void handleMainRoot() { mainServer.send(200,"text/html",paginaPrincipalHTML); }

void handleDados() {
    String json="{\"temperatura\":"+String(temperaturaAtual)+",\"umidade\":"+String(umidadeAtual)+"}";
    mainServer.send(200,"application/json",json);
}

void handleCadastrar() {
    String p = mainServer.arg("produto");
    mainServer.send(200,"text/html","<p>Produto cadastrado: "+p+"</p><a href='/'>Voltar</a>");
}

// ------------------- Setup -------------------
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

// ------------------- Loop -------------------
void loop() {
    portalServer.handleClient();
    medirSensor();

    if(finalizeRequested && !mainServerStarted){
        Serial.println("Finalizando AP e iniciando servidor principal...");
        portalServer.stop();
        WiFi.softAPdisconnect(true); delay(300);
        mainServer.on("/",HTTP_GET,handleMainRoot);
        mainServer.on("/dados",HTTP_GET,handleDados);
        mainServer.on("/cadastrar",HTTP_POST,handleCadastrar);
        mainServer.begin();
        mainServerStarted=true;
        finalizeRequested=false;
        Serial.println("Servidor principal iniciado. IP: "+WiFi.localIP().toString());
    }

    if(mainServerStarted) mainServer.handleClient();
}
