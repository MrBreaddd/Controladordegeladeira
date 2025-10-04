#include <WiFi.h>
#include <WebServer.h>

WebServer servidor(80);

String ssidRecebido;
String senhaRecebida;

const char* paginaConfig = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Configurar WiFi - ESP32</title>
</head>
<body>
  <h1>Configuração de WiFi</h1>
  <form action="/salvar" method="POST">
    <label for="ssid">SSID:</label><br>
    <input type="text" name="ssid"><br>
    <label for="senha">Senha:</label><br>
    <input type="password" name="senha"><br><br>
    <input type="submit" value="Conectar">
  </form>
</body>
</html>
)rawliteral";

// Página dinâmica que exibe o resultado da conexão
String gerarPaginaResultado(bool conectado, IPAddress ip) {
  String html = "<!DOCTYPE html><html><body>";
  if (conectado) {
    html += "<h1>Conectado com sucesso!</h1>";
    html += "<p>IP do ESP32: ";
    html += ip.toString();
    html += "</p>";
    html += "<p>O Access Point continuará ativo por enquanto.</p>";
  } else {
    html += "<h1>Falha ao conectar.</h1>";
    html += "<p>Verifique SSID e senha e tente novamente.</p>";
  }
  html += "<br><a href='/'>Voltar à configuração</a>";
  html += "</body></html>";
  return html;
}

void tratarRaiz() {
  servidor.send(200, "text/html", paginaConfig);
}

void tratarSalvar() {
  ssidRecebido = servidor.arg("ssid");
  senhaRecebida = servidor.arg("senha");

  Serial.println("Tentando conectar em: " + ssidRecebido);

  WiFi.mode(WIFI_AP_STA); // mantém AP ativo enquanto tenta STA
  WiFi.begin(ssidRecebido.c_str(), senhaRecebida.c_str());

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    servidor.send(200, "text/html", gerarPaginaResultado(true, WiFi.localIP()));
  } else {
    Serial.println("\nFalha na conexão.");
    servidor.send(200, "text/html", gerarPaginaResultado(false, IPAddress(0,0,0,0)));
  }
}

void setup() {
  Serial.begin(115200);

  // Inicia Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.print("AP iniciado. IP: ");
  Serial.println(WiFi.softAPIP());

  servidor.on("/", tratarRaiz);
  servidor.on("/salvar", HTTP_POST, tratarSalvar);

  servidor.begin();
  Serial.println("Servidor web iniciado.");
}

void loop() {
  servidor.handleClient();
}