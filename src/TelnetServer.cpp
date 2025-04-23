#include "TelnetServer.h"
#include <Arduino.h>

// Construtor
TelnetServer::TelnetServer(uint16_t port, bool logEnabled)
    : _server(port), _port(port), _logEnabled(logEnabled), _running(false),
      _welcomeMessage("Bem-vindo ao servidor Telnet\r\n> ") {}

// Destrutor
TelnetServer::~TelnetServer() { stop(); }

// Inicia o servidor
void TelnetServer::begin() {
    if (_running)
        return;

    if (!LittleFS.begin() && _logEnabled) {
        LOG_ERROR("[TELNET] Erro ao montar LittleFS");
        return;
    }

    _server.begin();
    _running = true;

    xTaskCreatePinnedToCore([](void *param) { static_cast<TelnetServer *>(param)->taskFunction(); }, "TelnetServerTask",
                            8192, this, 1, &_taskHandle, 1);

    if (_logEnabled) {
        LOG_INFO("[TELNET] Servidor iniciado na porta %d", _port);
    }
}

// Para o servidor
void TelnetServer::stop() {
    if (!_running)
        return;

    _running = false;

    if (_taskHandle != NULL) {
        vTaskDelete(_taskHandle);
        _taskHandle = NULL;
    }

    disconnectAllClients();
    _server.stop();

    if (_logEnabled) {
        LOG_WARN("[TELNET] Servidor parado");
    }
}

// Atualiza o estado do servidor
void TelnetServer::update() {
    std::lock_guard<std::mutex> lock(_mutex);
    handleNewConnections();
    handleExistingClients();
}

// Manipula novas conexões
void TelnetServer::handleNewConnections() {
    if (WiFiClient client = _server.available()) {
        std::lock_guard<std::mutex> lock(_mutex);
        _clients.push_back({client, "", millis(), true}); // Adiciona authenticated

        if (_echoEnabled) {
            client.print("Echo enabled\r\n");
        }
        client.print(_welcomeMessage);
        client.print(_prompt);
    }
}

// Manipula clientes existentes
void TelnetServer::handleExistingClients() {
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto it = _clients.begin(); it != _clients.end();) {
        if (millis() - it->lastActivity > CLIENT_TIMEOUT_MS) {
            disconnectClient(*it, "Tempo de inatividade excedido");
            it = _clients.erase(it);
        } else {
            handleClient(*it);
            ++it;
        }
    }
}

// Manipula um cliente específico
void TelnetServer::handleClient(ClientContext &context) {
    while (context.client.available()) {
        char c = context.client.read();
        context.lastActivity = millis();

        if (c == '\t' && _tabHandler) { // Trata pressionamento de TAB
            _tabHandler(context.client, context.buffer);
        } else if (c == '\n' || c == '\r') {
            if (!context.buffer.isEmpty()) {
                processBuffer(context);
                context.buffer = "";
            }
        } else if (context.buffer.length() < MAX_BUFFER_SIZE) {
            context.buffer += c;
        }
    }
}

// Processa o buffer de comando
void TelnetServer::processBuffer(ClientContext &context) {
    if (_logEnabled) {
        LOG_DEBUG("[TELNET] Comando recebido de %s: %s", context.client.remoteIP().toString().c_str(),
                  context.buffer.c_str());
    }

    String command = filterTelnetCommands(context.buffer);
    command.trim();

    // Verifica comandos registrados
    bool commandHandled = false;
    for (const auto &handler : _commandHandlers) {
        if (command.equalsIgnoreCase(handler.first) || command.startsWith(handler.first + " ")) {
            handler.second(context.client, command);
            commandHandled = true;
            break;
        }
    }

    // Handler padrão
    if (!commandHandled && _defaultHandler) {
        _defaultHandler(context.client, command);
    }

    context.client.print(_prompt);
}

// Adiciona um novo comando
void TelnetServer::addCommand(const String &command, CommandHandler handler) {
    std::lock_guard<std::mutex> lock(_mutex);
    _commandHandlers[command] = handler;
}

// Remove um comando
void TelnetServer::removeCommand(const String &command) {
    std::lock_guard<std::mutex> lock(_mutex);
    _commandHandlers.erase(command);
}

// Define o handler padrão
void TelnetServer::setDefaultHandler(CommandHandler handler) {
    std::lock_guard<std::mutex> lock(_mutex);
    _defaultHandler = handler;
}

void TelnetServer::setTabHandler(CommandHandler handler) {
    std::lock_guard<std::mutex> lock(_mutex);
    _tabHandler = handler;
}

// Define a mensagem de boas-vindas
void TelnetServer::setWelcomeMessage(const String &message) {
    std::lock_guard<std::mutex> lock(_mutex);
    _welcomeMessage = message;
}

// Define o prompt
void TelnetServer::setPrompt(const String &prompt) {
    std::lock_guard<std::mutex> lock(_mutex);
    _prompt = prompt;
}

// Habilita/desabilita eco
void TelnetServer::enableEcho(bool enable) { _echoEnabled = enable; }

// Número de clientes conectados
size_t TelnetServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _clients.size();
}

// Desconecta todos os clientes
void TelnetServer::disconnectAllClients() {
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto &client : _clients) {
        disconnectClient(client, "Servidor sendo desligado...");
    }
    _clients.clear();
}

// Desconecta um cliente específico
void TelnetServer::disconnectClient(ClientContext &context, const String &message) {
    if (context.client.connected()) {
        context.client.println(message);
        context.client.stop();
    }
}

// Filtra comandos Telnet
String TelnetServer::filterTelnetCommands(const String &input) const {
    String filtered;
    bool inTelnetCommand = false;
    uint8_t telnetCmdLength = 0;

    for (size_t i = 0; i < input.length(); i++) {
        uint8_t c = input[i];

        if (inTelnetCommand) {
            telnetCmdLength++;
            // Comandos Telnet têm 2 ou 3 bytes
            if ((telnetCmdLength >= 2 && (c < 0xF0 || c > 0xFF)) || telnetCmdLength >= 3) {
                inTelnetCommand = false;
            }
            continue;
        }

        if (isTelnetCommand(c)) {
            inTelnetCommand = true;
            telnetCmdLength = 1;
            continue;
        }

        if (c >= 32 && c <= 126) { // Caracteres imprimíveis
            filtered += (char)c;
        }
    }

    return filtered;
}

// Verifica se é um comando Telnet
bool TelnetServer::isTelnetCommand(uint8_t c) const {
    return (c == 0xFF); // IAC (Interpret As Command)
}

// Envia comando Telnet
void TelnetServer::sendTelnetCommand(WiFiClient &client, uint8_t cmd, uint8_t option) {
    client.write(0xFF); // IAC
    client.write(cmd);
    client.write(option);
}

// Função da task
void TelnetServer::taskFunction() {
    while (_running) {
        update();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}