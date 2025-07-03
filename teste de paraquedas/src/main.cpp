//by wilson simonal

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Preferences.h>

// Credenciais do WiFi
const char* ssid = "PRD_coleta_paraquedas";
const char* password = "1234567890";

WebServer server(80);

String statusMsg = "Coleta desligada";
String statusBMP = "não encontrado";
String statusMPU = "não encontrado";
String csvData = "Altura;AccX;AccY;AccZ\n";

// Criação do objeto BMP280
Adafruit_BMP280 bmp;
float altitude_inicial = 0;
float altitude_atual = 0;
float altitude_maxima = -10000;
float altitude_minima = 10000;

// variável para controlar se deve zerar a altitude
bool zerar_altitude = true;

// Criação do objeto MPU6050
Adafruit_MPU6050 mpu;

// variável global para armazenar o maior valor de aceleração no eixo Y
float max_accel_y = -10000;
float ultimo_max_accel_y = 0;

#define NOME_ARQUIVO_BASE "/coleta_de_dados_"
#define EXTENSAO ".csv"

Preferences prefs;
uint32_t coletaAtual = 0;

// Função para piscar o LED duas vezes
void piscarLED() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(32, HIGH);
    delay(200);
    digitalWrite(32, LOW);
    delay(200);
  }
  
  // Se a coleta estiver ativa, mantém o LED ligado após as piscadas
  if (statusMsg == "Coleta ligada") {
    digitalWrite(32, HIGH);
  }
}

// Variáveis para lançamento por altitude
float altitude_ejecao = 100.0; // Altitude padrão em metros
bool ejetar_acima = true; // true = acima, false = abaixo
bool lancamento_altitude_ativo = false;
String status_lancamento = "Desativado"; // Status atual do lançamento
float altitude_inicial_lancamento = 0; // Altitude inicial de referência para lançamento
bool zerar_altitude_lancamento = true; // Flag para zerar altitude de referência

// Lista todos os arquivos na SPIFFS e exibe na Serial
void listarArquivosSPIFFS() {
  Serial.println("Arquivos disponíveis na SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.println(String("Link: ") + file.name());
    file = root.openNextFile();
  }
}

// Salva o conteúdo do csvData em um novo arquivo sequencial
void salvarCSVEmArquivo() {
  String nomeArquivo = NOME_ARQUIVO_BASE + String(coletaAtual) + EXTENSAO;
  File file = SPIFFS.open(nomeArquivo, FILE_WRITE);
  if (file) {
    file.print(csvData);
    file.close();
    Serial.println("CSV salvo em: " + nomeArquivo);
    coletaAtual++;
    prefs.putUInt("num", coletaAtual);
  } else {
    Serial.println("Falha ao salvar CSV!");
  }
}

// Função para obter uma string HTML com as linhas da tabela de arquivos CSV salvos
String gerarTabelaArquivosSPIFFS() {
  String linhas = "";
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String nome = String(file.name());
    if (nome.endsWith(".csv")) {
      // Link de download
      linhas += "<tr><td>" + nome + "</td><td><a href='" + nome + "' download style='color:#1976d2;text-decoration:underline;'>Baixar</a></td>";
      linhas += "<td><form method='POST' action='/excluir?file=" + nome + "&tab=downloads' style='margin:0;'><button type='submit' style='background:#e53935;color:#fff;border:none;padding:6px 12px;border-radius:4px;cursor:pointer;'>Excluir</button></form></td></tr>";
    }
    file = root.openNextFile();
  }
  return linhas;
}

// função que trata a página principal
void tratarRaiz() {
  // Pega a aba atual da URL (padrão é telemetria)
  String abaAtual = "telemetria";
  if (server.hasArg("tab")) {
    abaAtual = server.arg("tab");
  }
  
  String corBotao;
  String textoBotao;
  if (statusMsg == "Coleta ligada") {
    corBotao = "background-color:yellow;color:black;";
    textoBotao = "Parar Coleta";
  } else {
    corBotao = "background-color:green;color:white;";
    textoBotao = "Iniciar Coleta";
  }
  String html = "<html><head><title>Página de Controle</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<style>body{font-family:sans-serif;background:#eaf1fb;margin:0;padding:0;}";
  html += ".container{width:100vw;min-height:100vh;max-width:100vw;margin:0;background:#fff;padding:0;border-radius:0;box-shadow:none;display:flex;flex-direction:column;}";
  html += ".logo-top{display:block;margin:24px auto 8px auto;max-width:120px;width:60vw;height:auto;}";
  html += ".bar{display:flex;background:#fff;color:#1976d2;padding:0;margin:0;width:100vw;box-shadow:0 2px 8px #1976d233;border-bottom:1px solid #e0e0e0;}";
  html += ".bar button{flex:1;padding:10px 0;background:#f8f9fa;border:2px solid #e0e0e0;color:#1976d2;font-size:1em;cursor:pointer;transition:all 0.3s ease;min-width:80px;letter-spacing:1px;font-weight:500;margin:4px 2px;border-radius:8px;}";
  html += ".bar button:hover{background:#e3f2fd;border-color:#1976d2;transform:translateY(-1px);box-shadow:0 2px 4px rgba(25,118,210,0.2);}";
  html += ".bar button.active{background:linear-gradient(90deg,#1976d2 0%,#42a5f5 100%);color:#fff;border:2px solid #1976d2;box-shadow:0 4px 8px rgba(25,118,210,0.3);transform:translateY(-2px);}";
  html += ".tab-content{margin-top:20px;padding:16px;}";
  html += ".status-box{background:#e3f2fd;padding:10px 10px;border-radius:8px;margin-bottom:18px;display:flex;gap:10px;justify-content:center;font-size:1em;color:#1976d2;}";
  html += "h2{color:#1976d2;font-size:1.2em;}table{width:100%;margin-bottom:20px;border-radius:8px;overflow:hidden;box-shadow:0 1px 4px #1976d233;}th,td{padding:8px 6px;text-align:center;}th{background:#1976d2;color:#fff;}tr:nth-child(even){background:#f5faff;}tr:nth-child(odd){background:#e3f2fd;}button[type='submit']{padding:10px 18px;font-size:1em;border:none;border-radius:6px;cursor:pointer;transition:0.2s;background:linear-gradient(90deg,#1976d2 0%,#42a5f5 100%);color:#fff;font-weight:500;box-shadow:0 2px 8px #1976d233;}button[type='submit']:hover{filter:brightness(0.95);background:#1565c0;}form{margin-bottom:0;}p{font-size:1em;color:#1976d2;}";
  html += "@media (max-width:600px){.container{padding:0;}.tab-content{padding:8px;}h2{font-size:1em;}.bar button{font-size:0.95em;padding:8px 0;}}";
  html += "</style>";
  html += "<script>function showTab(tab){document.getElementById('downloads').style.display=tab=='downloads'?'block':'none';document.getElementById('telemetria').style.display=tab=='telemetria'?'block':'none';document.getElementById('lancamento').style.display=tab=='lancamento'?'block':'none';document.getElementById('btnDownloads').classList.toggle('active',tab=='downloads');document.getElementById('btnTelemetria').classList.toggle('active',tab=='telemetria');document.getElementById('btnLancamento').classList.toggle('active',tab=='lancamento');}</script>";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<img src='/logo.png' alt='Logo' class='logo-top'>";
  html += "<div class='bar'>";
  html += "<button id='btnTelemetria' onclick=\"showTab('telemetria')\">Telemetria</button>";
  html += "<button id='btnLancamento' onclick=\"showTab('lancamento')\">Lancamento</button>";
  html += "<button id='btnDownloads' onclick=\"showTab('downloads')\">Downloads</button>";
  html += "</div>";
  // Conteúdo das abas
  html += "<div class='tab-content' id='downloads' style='display:none;'>";
  html += "<h2>Tabela de Dados</h2>";
  html += "<form method='POST' action='/excluir_todos?tab=downloads' style='margin-bottom:16px;'><button type='submit' style='background:#b71c1c;color:#fff;border:none;padding:10px 22px;border-radius:6px;cursor:pointer;font-weight:600;font-size:1em;'>Excluir Todos</button></form>";
  html += "<table border='1'><tr><th>Coleta</th><th>link csv</th><th>Excluir</th></tr>";
  html += gerarTabelaArquivosSPIFFS();
  html += "</table><hr>";
  html += "</div>";
  html += "<div class='tab-content' id='lancamento' style='display:none;'>";
  html += "<h2>Sistema de Lancamento</h2>";
  
  // Botões de ação
  html += "<div style='display:flex;flex-direction:column;gap:15px;max-width:400px;'>";
  
  // Status do lançamento por altitude
  if (lancamento_altitude_ativo) {
    // Configuração de lançamento por altitude (só aparece quando ativo)
    html += "<div style='background:#f5f5f5;padding:15px;border-radius:8px;margin-bottom:20px;'>";
    html += "<h3 style='color:#1976d2;margin-top:0;'>Configuracao de Altitude</h3>";
    html += "<form method='POST' action='/configurar_altitude?tab=lancamento' style='display:flex;flex-direction:column;gap:15px;'>";
    html += "<div style='display:flex;align-items:center;gap:10px;'>";
    html += "<label style='font-weight:500;color:#333;'>Altitude de Ejecao:</label>";
    html += "<input type='number' name='altitude' value='" + String(altitude_ejecao, 1) + "' step='0.1' min='0' max='1000' style='padding:8px;border:2px solid #ddd;border-radius:4px;font-size:1em;width:100px;' required>";
    html += "<span style='color:#666;'>metros</span>";
    html += "</div>";
    html += "<div style='display:flex;flex-direction:column;gap:8px;'>";
    html += "<label style='font-weight:500;color:#333;'>Condicao de Ejecao:</label>";
    html += "<label style='display:flex;align-items:center;gap:8px;cursor:pointer;'>";
    html += "<input type='radio' name='condicao' value='acima'" + String(ejetar_acima ? " checked" : "") + " style='transform:scale(1.2);'>";
    html += "<span>Ejetar quando ACIMA da altitude definida</span>";
    html += "</label>";
    html += "<label style='display:flex;align-items:center;gap:8px;cursor:pointer;'>";
    html += "<input type='radio' name='condicao' value='abaixo'" + String(!ejetar_acima ? " checked" : "") + " style='transform:scale(1.2);'>";
    html += "<span>Ejetar quando ABAIXO da altitude definida</span>";
    html += "</label>";
    html += "</div>";
    html += "<button type='submit' style='background:#2196f3;color:#fff;border:none;padding:10px 20px;border-radius:6px;cursor:pointer;font-weight:600;'>Salvar Configuracao</button>";
    html += "</form>";
    html += "</div>";
    
    html += "<div style='background:#e8f5e8;padding:12px;border-radius:8px;color:#2e7d32;border:2px solid #4caf50;'>";
    html += "<p style='margin:0;font-weight:600;'>Lancamento por Altitude ATIVO</p>";
    html += "<p style='margin:5px 0 0 0;font-size:0.9em;'>Altitude: " + String(altitude_ejecao, 1) + "m | Condicao: " + String(ejetar_acima ? "ACIMA" : "ABAIXO") + "</p>";
    html += "</div>";
    html += "<form method='POST' action='/desativar_altitude?tab=lancamento'>";
    html += "<button type='submit' style='background:#f44336;color:#fff;border:none;padding:15px 25px;border-radius:8px;cursor:pointer;font-weight:600;font-size:1.1em;width:100%;'>Desativar Lancamento por Altitude</button>";
    html += "</form>";
  } else {
    html += "<form method='POST' action='/ativar_altitude?tab=lancamento'>";
    html += "<button type='submit' style='background:linear-gradient(90deg,#ff9800 0%,#ff5722 100%);color:#fff;border:none;padding:15px 25px;border-radius:8px;cursor:pointer;font-weight:600;font-size:1.1em;width:100%;'>Ativar Lancamento por Altitude</button>";
    html += "</form>";
  }
  
  // Status geral do sistema de lançamento
  html += "<div style='background:#f0f0f0;padding:10px;border-radius:6px;margin:15px 0;text-align:center;'>";
  html += "<strong>Status do Sistema: </strong>";
  if (status_lancamento == "Lancado - Altitude") {
    html += "<span style='color:#4caf50;font-weight:600;'>Lancado - " + String(ejetar_acima ? "Acima" : "Abaixo") + " " + String(altitude_ejecao, 1) + "m</span>";
  } else if (status_lancamento == "Lancado - Manual") {
    html += "<span style='color:#2196f3;font-weight:600;'>Lancado - Manual</span>";
  } else if (status_lancamento == "Ativo - Monitorando") {
    html += "<span style='color:#ff9800;font-weight:600;'>Ativo - " + String(ejetar_acima ? "Acima" : "Abaixo") + " " + String(altitude_ejecao, 1) + "m</span>";
  } else if (status_lancamento == "Iniciando...") {
    html += "<span style='color:#2196f3;font-weight:600;'>Iniciando...</span>";
  } else if (status_lancamento == "Configurado - Pronto para ativar") {
    html += "<span style='color:#9c27b0;font-weight:600;'>Configurado - " + String(ejetar_acima ? "Acima" : "Abaixo") + " " + String(altitude_ejecao, 1) + "m</span>";
  } else {
    html += "<span style='color:#666;font-weight:600;'>Desativado</span>";
  }
  html += "</div>";
  
  // Mostrar altitude atual (sempre visível)
  html += "<div style='background:#e3f2fd;padding:10px;border-radius:6px;margin:10px 0;text-align:center;'>";
  if (statusMsg == "Coleta ligada") {
    // Durante coleta, mostra altitude relativa à coleta
    html += "<strong>Altitude Atual: </strong><span style='color:#1976d2;font-weight:600;'>" + String(altitude_atual, 2) + " m</span>";
    html += "<br><small style='color:#666;'>(Relativa ao inicio da coleta)</small>";
  } else {
    // Fora da coleta, mostra altitude absoluta
    html += "<strong>Altitude Atual: </strong><span style='color:#1976d2;font-weight:600;'>" + String(altitude_atual, 2) + " m</span>";
    html += "<br><small style='color:#666;'>(Altitude absoluta)</small>";
  }
  html += "</div>";
  
  // Mostrar altitude relativa do lançamento quando ativo e já foi zerado
  if (lancamento_altitude_ativo && !zerar_altitude_lancamento) {
    // Para cálculo correto, usamos a altitude absoluta quando coleta está desligada
    // ou a altitude relativa da coleta quando está ligada, mas sempre comparamos com a referência do lançamento
    float altitude_atual_absoluta = altitude_atual;
    if (statusMsg == "Coleta ligada") {
      // Se coleta está ligada, altitude_atual é relativa, então somamos com a inicial da coleta
      altitude_atual_absoluta = altitude_atual + altitude_inicial;
    }
    float altitude_relativa = altitude_atual_absoluta - altitude_inicial_lancamento;
    html += "<div style='background:#fff3e0;padding:10px;border-radius:6px;margin:10px 0;text-align:center;'>";
    html += "<strong>Altitude Relativa Lancamento: </strong><span style='color:#f57c00;font-weight:600;'>" + String(altitude_relativa, 2) + " m</span>";
    html += "<br><small style='color:#666;'>Referencia: " + String(altitude_inicial_lancamento, 2) + " m</small>";
    html += "</div>";
  }
  
  html += "<form method='POST' action='/lancamento_manual?tab=lancamento'>";
  html += "<button type='submit' style='background:linear-gradient(90deg,#4caf50 0%,#2e7d32 100%);color:#fff;border:none;padding:15px 25px;border-radius:8px;cursor:pointer;font-weight:600;font-size:1.1em;width:100%;'>Lancamento Manual</button>";
  html += "</form>";
  html += "</div>";
  html += "</div>";
  html += "<div class='tab-content' id='telemetria'>";
  html += "<div class='status-box'>";
  html += "<b>BMP280 (0x76):</b> " + statusBMP + " &nbsp; <b>MPU (0x68):</b> " + statusMPU;
  html += "</div>";
  html += "<h2>Iniciar coleta</h2>";
  html += "<form action='/botao?tab=telemetria' method='POST'>";
  if (statusMsg == "Coleta ligada") {
    html += "<button type='submit' style='background:linear-gradient(90deg,#fffde4 0%,#fff200 100%);color:#333;font-weight:600;'>Parar Coleta</button>";
  } else {
    html += "<button type='submit' style='background:linear-gradient(90deg,#43e97b 0%,#38f9d7 100%);color:#fff;font-weight:600;'>Iniciar Coleta</button>";
  }
  html += "</form>";
  html += "<p>Status: <b>" + statusMsg + "</b></p>";
  if (statusMsg == "Coleta desligada") {
    html += "<div style='margin-top:18px;background:#e3f2fd;padding:10px 14px;border-radius:8px;color:#1976d2;'><b>Altura maxima:</b> " + String(altitude_maxima, 2) + " m &nbsp; <b>Altura minima:</b> " + String(altitude_minima, 2) + " m</div>";
    html += "<div style='margin-top:12px;background:#ffe0b2;padding:10px 14px;border-radius:8px;color:#e65100;'><b>Maior aceleracao Y:</b> " + String(ultimo_max_accel_y, 4) + " m/s² &nbsp; (" + String(ultimo_max_accel_y/9.80665, 4) + " g)</div>";
  } else if (statusMsg == "Coleta ligada") {
    float g_atual = max_accel_y / 9.80665;
   // html += "<div style='margin-top:12px;background:#fffde7;padding:10px 14px;border-radius:8px;color:#f9a825;'><b>Aceleração Y atual (máx):</b> " + String(max_accel_y, 4) + " m/s² &nbsp; (" + String(g_atual, 4) + " g)</div>";
  }
  html += "</div>";
  html += "<script>showTab('" + abaAtual + "');</script>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}



// função que trata o botão
void tratarBotao() {
  String tab = "telemetria";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  if (statusMsg == "Coleta ligada") {
    statusMsg = "Coleta desligada";
    digitalWrite(32, LOW); // Desliga o LED
    Serial.print("Maior valor de aceleração Y: ");
    Serial.println(max_accel_y, 4);
    ultimo_max_accel_y = max_accel_y;
    salvarCSVEmArquivo(); // Salva o CSV ao parar a coleta
  } else {
    statusMsg = "Coleta ligada";
    digitalWrite(32, HIGH); // Liga o LED
    max_accel_y = -10000; // Zera o valor ao iniciar nova coleta
  }
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para tratar exclusão de arquivo
void tratarExcluir() {
  String tab = "downloads";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Arquivo não especificado");
    return;
  }
  String nomeArquivo = server.arg("file");
  if (!nomeArquivo.startsWith("/")) {
    nomeArquivo = "/" + nomeArquivo;
  }
  if (SPIFFS.exists(nomeArquivo)) {
    SPIFFS.remove(nomeArquivo);
    server.sendHeader("Location", "/?tab=" + tab); // Redireciona para a página principal
    server.send(303);
  } else {
    server.send(404, "text/plain", "Arquivo não encontrado");
  }
}

// Função para excluir todos os arquivos .csv e .txt e zerar o contador
void tratarExcluirTodos() {
  String tab = "downloads";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  int apagados = 0;
  while (file) {
    String nome = String(file.name());
    if (nome.endsWith(".csv") || nome.endsWith(".txt")) {
      if (!nome.startsWith("/")) {
        nome = "/" + nome;
      }
      SPIFFS.remove(nome);
      apagados++;
    }
    file = root.openNextFile();
  }
  coletaAtual = 0;
  prefs.putUInt("num", 0);
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para configurar altitude de ejeção
void tratarConfigurarAltitude() {
  String tab = "lancamento";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  if (server.hasArg("altitude") && server.hasArg("condicao")) {
    altitude_ejecao = server.arg("altitude").toFloat();
    ejetar_acima = (server.arg("condicao") == "acima");
    
    // Desativa temporariamente para esconder o quadro de configuração
    lancamento_altitude_ativo = false;
    status_lancamento = "Configurado - Pronto para ativar";
    
    Serial.println("Configuracao de altitude atualizada:");
    Serial.println("Altitude: " + String(altitude_ejecao, 1) + "m");
    Serial.println("Condicao: " + String(ejetar_acima ? "ACIMA" : "ABAIXO"));
  }
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para ativar lançamento por altitude
void tratarAtivarAltitude() {
  String tab = "lancamento";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  lancamento_altitude_ativo = true;
  zerar_altitude_lancamento = true; // Força o zero da altitude de referência
  status_lancamento = "Iniciando...";
  Serial.println("Lancamento por altitude ATIVADO!");
  Serial.println("Altitude: " + String(altitude_ejecao, 1) + "m");
  Serial.println("Condicao: " + String(ejetar_acima ? "ACIMA" : "ABAIXO"));
  Serial.println("Zerando altitude de referencia...");
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para desativar lançamento por altitude
void tratarDesativarAltitude() {
  String tab = "lancamento";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  lancamento_altitude_ativo = false;
  status_lancamento = "Desativado";
  Serial.println("Lancamento por altitude DESATIVADO!");
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para tratar lançamento por altitude (mantida para compatibilidade)
void tratarLancamentoAltitude() {
  String tab = "lancamento";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  Serial.println("Lançamento por altitude ativado!");
  // Aqui você pode adicionar a lógica para lançamento por altitude
  // Por exemplo: definir uma altitude de referência e monitorar
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// Função para tratar lançamento manual
void tratarLancamentoManual() {
  String tab = "lancamento";
  if (server.hasArg("tab")) {
    tab = server.arg("tab");
  }
  
  Serial.println("LANCAMENTO MANUAL ATIVADO!");
  status_lancamento = "Lancado - Manual";
  piscarLED(); // Pisca o LED duas vezes
  Serial.println("LED piscou - Lancamento manual executado");
  // Aqui você pode adicionar a lógica para lançamento manual
  // Por exemplo: ativar um pino de saída para acionar o mecanismo
  server.sendHeader("Location", "/?tab=" + tab);
  server.send(303);
}

// função da task do servidor web
void taskServidorWeb(void *pvParameters) {
  while (true) {
    server.handleClient();
    vTaskDelay(10 / portTICK_PERIOD_MS); // Pequeno delay para liberar o scheduler
  }
}

// função da task para ler sensores
void taskLeituraSensores(void *pvParameters) {
  sensors_event_t acc, gyro, temp;
  while (true) {
    // Leitura básica dos sensores (sempre ativa para monitoramento de lançamento)
    float pressure = bmp.readPressure();
    float temperature = bmp.readTemperature();
    float altitude = bmp.readAltitude(1013.25);
    
    if (statusMsg == "Coleta ligada") {
      // BMP280 - Configuração para coleta de dados
      if (zerar_altitude) {
        float soma_altitude = 0;
        for (int i = 0; i < 50; i++) {
          soma_altitude += bmp.readAltitude(1013.25);
          vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        altitude_inicial = soma_altitude / 50.0;
        zerar_altitude = false;
        altitude_maxima = -10000;
        altitude_minima = 10000;
        csvData = "Altura;AccX;AccY;AccZ\n"; // Limpa CSV ao iniciar coleta
      }
      
      altitude_atual = altitude - altitude_inicial;
      if (altitude_atual > altitude_maxima) altitude_maxima = altitude_atual;
      if (altitude_atual < altitude_minima) altitude_minima = altitude_atual;
      
      // MPU6050 - Coleta de dados
      mpu.getEvent(&acc, &gyro, &temp);
      float y_invertido = acc.acceleration.y * -1;
      if (y_invertido > max_accel_y) max_accel_y = y_invertido;
      
      // Monta a linha completa antes de salvar
      String linhaCSV = String(altitude_atual, 4) + ";" + String(acc.acceleration.x, 4) + ";" + String(y_invertido, 4) + ";" + String(acc.acceleration.z, 4) + "\n";
      // Só salva se todos os valores são válidos (não NaN)
      if (!isnan(altitude_atual) && !isnan(acc.acceleration.x) && !isnan(y_invertido) && !isnan(acc.acceleration.z)) {
        csvData += linhaCSV;
        Serial.print(altitude_atual); Serial.print("\t");
        Serial.print(acc.acceleration.x); Serial.print("\t");
        Serial.print(y_invertido); Serial.print("\t");
        Serial.print(acc.acceleration.z); Serial.print("\t");
        Serial.println();
      }
    } else {
      // Quando coleta está desligada, ainda precisamos da altitude para lançamento
      zerar_altitude = true;
      // Usa altitude absoluta para monitoramento de lançamento
      altitude_atual = altitude;
    }
    
    // Verificar condições de lançamento por altitude (INDEPENDENTE da coleta)
    if (lancamento_altitude_ativo) {
      // Se acabou de ativar, zera a altitude de referência para lançamento
      if (zerar_altitude_lancamento) {
        float soma_altitude = 0;
        for (int i = 0; i < 50; i++) {
          soma_altitude += bmp.readAltitude(1013.25);
          vTaskDelay(5 / portTICK_PERIOD_MS);
        }
        altitude_inicial_lancamento = soma_altitude / 50.0;
        zerar_altitude_lancamento = false;
        status_lancamento = "Ativo - Monitorando";
        Serial.println("Altitude inicial de lancamento zerada: " + String(altitude_inicial_lancamento, 2) + "m");
      }
      
      // Calcula altitude relativa (atual - inicial do lançamento)
      // Esta é a altitude relativa usada para comparação com o threshold de ejeção
      float altitude_relativa = altitude - altitude_inicial_lancamento;
      
      bool condicao_atendida = false;
      if (ejetar_acima && altitude_relativa >= altitude_ejecao) {
        condicao_atendida = true;
      } else if (!ejetar_acima && altitude_relativa <= altitude_ejecao) {
        condicao_atendida = true;
      }
      
      if (condicao_atendida && status_lancamento == "Ativo - Monitorando") {
        Serial.println("CONDICAO DE EJECAO ATENDIDA!");
        Serial.println("Altitude relativa: " + String(altitude_relativa, 2) + "m");
        Serial.println("Altitude de ejecao: " + String(altitude_ejecao, 1) + "m");
        Serial.println("Condicao: " + String(ejetar_acima ? "ACIMA" : "ABAIXO"));
        
        status_lancamento = "Lancado - Altitude";
        piscarLED(); // Pisca o LED duas vezes
        Serial.println("LED piscou - Lancamento por altitude executado");
        
        // Aqui você deve adicionar o código para acionar o mecanismo de ejeção
        // Por exemplo: digitalWrite(PINO_EJECAO, HIGH);
        lancamento_altitude_ativo = false; // Desativa após ejeção
      }
    }
    
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

// função para escanear dispositivos I2C
void escanearI2C() {
  Wire.begin();
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      if (address == 0x76) statusBMP = "ativos";
      if (address == 0x68) statusMPU = "ativos";
    }
  }
}

// função de configuração
void setup() {
  Serial.begin(115200);
  pinMode(32, OUTPUT); // Configura o pino do LED
  digitalWrite(32, LOW); // Garante que o LED inicie desligado

  // Inicializa SPIFFS para servir arquivos estáticos
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    while (1);
  }
  prefs.begin("coleta", false);
  coletaAtual = prefs.getUInt("num", 0);

  listarArquivosSPIFFS();

  Wire.begin();
  escanearI2C(); // Atualiza statusBMP e statusMPU antes de iniciar o servidor
  if (!bmp.begin(0x76)) {  // Endereço I2C pode ser 0x76 ou 0x77 dependendo do módulo
    Serial.println("Falha ao encontrar o sensor BMP280!");
    while (1);
  }
  // Configura oversampling de 8x e filtro digital X4
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X8,   // Temperatura
    Adafruit_BMP280::SAMPLING_X8,   // Pressão
    Adafruit_BMP280::FILTER_X4      // Filtro digital X4
  );
  Serial.println("BMP280 iniciado com sucesso!");
  altitude_inicial = bmp.readAltitude(1013.25); // Zera a altitude ao ligar

  if (!mpu.begin()) {
    Serial.println("MPU6050 não encontrado. Verifique as conexões!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  // Inicia o modo Access Point
  WiFi.softAP(ssid, password);
  Serial.println("\nWiFi AP iniciado!");
  Serial.print("IP do AP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", tratarRaiz);
  server.on("/botao", HTTP_POST, tratarBotao);
  server.on("/excluir", HTTP_POST, tratarExcluir);
  server.on("/excluir_todos", HTTP_POST, tratarExcluirTodos);
  server.on("/configurar_altitude", HTTP_POST, tratarConfigurarAltitude);
  server.on("/ativar_altitude", HTTP_POST, tratarAtivarAltitude);
  server.on("/desativar_altitude", HTTP_POST, tratarDesativarAltitude);
  server.on("/lancamento_altitude", HTTP_POST, tratarLancamentoAltitude);
  server.on("/lancamento_manual", HTTP_POST, tratarLancamentoManual);
  // Serve arquivos .csv do SPIFFS
  server.serveStatic("/", SPIFFS, "/");
  server.serveStatic("/logo.png", SPIFFS, "/logo.png");
  server.begin();

  // Cria a task do servidor web no núcleo 0
  xTaskCreatePinnedToCore(
    taskServidorWeb,    // Função da task
    "ServidorWeb",      // Nome da task
    4096,               // Tamanho da stack
    NULL,               // Parâmetro
    1,                  // Prioridade
    NULL,               // Handle da task
    0                   // Núcleo (0 ou 1)
  );
  // Cria a task de leitura de sensores no núcleo 1
  xTaskCreatePinnedToCore(
    taskLeituraSensores,    // Função da task
    "LeituraSensores",     // Nome da task
    4096,                  // Tamanho da stack
    NULL,                  // Parâmetro
    2,                     // Prioridade
    NULL,                  // Handle da task
    1                      // Núcleo 1
  );
}

// função principal de repetição
void loop() {
  // Pode ser usado para outras tarefas, se necessário
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
