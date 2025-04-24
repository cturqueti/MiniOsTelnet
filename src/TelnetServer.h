#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#include <LittleFS.h>
#include <LogLibrary.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

class TelnetServer {
  public:
    using CommandHandler = std::function<void(WiFiClient &, const String &)>;

    // Constantes
    static constexpr size_t MAX_BUFFER_SIZE = 512;
    static constexpr uint32_t CLIENT_TIMEOUT_MS = 300000; // 5 minutos

    // Construtor/Destrutor
    TelnetServer(uint16_t port = 23, bool logEnabled = true);
    ~TelnetServer();

    // Controle do servidor
    void begin();
    void stop();
    void update();

    // Gerenciamento de comandos
    void addCommand(const String &command, CommandHandler handler);
    void setDefaultHandler(CommandHandler handler);
    void removeCommand(const String &command);

    // Configuração
    void setWelcomeMessage(const String &message);
    void setPrompt(const String &prompt);
    void enableEcho(bool enable);

    // Informação
    size_t getClientCount() const;
    void disconnectAllClients();

    void logMutexAction(const char *action, const char *function) {
        if (_logEnabled) {
            LOG_DEBUG("[MUTEX] %s em %s", action, function);
        }
    }

  private:
    struct ClientContext {
        WiFiClient client;
        String buffer;
        String currentInput;
        uint32_t lastActivity;
        bool authenticated;
    };

    // Tratamento de clientes
    void handleNewConnections();
    void handleExistingClients();
    void handleClient(ClientContext &context);
    void processBuffer(ClientContext &context);
    void disconnectClient(ClientContext &context, const String &message = "");

    // Processamento de comandos
    bool isTelnetCommand(uint8_t c) const;
    String filterTelnetCommands(const String &input) const;
    void sendTelnetCommand(WiFiClient &client, uint8_t cmd, uint8_t option);

    // Thread/task
    void taskFunction();

    // Variáveis membro
    WiFiServer _server;
    uint16_t _port;
    bool _logEnabled;
    bool _running;
    bool _echoEnabled;
    String _welcomeMessage;
    String _prompt;

    std::vector<ClientContext> _clients;
    std::map<String, CommandHandler> _commandHandlers;
    CommandHandler _defaultHandler;

    mutable std::mutex _mutex;
    TaskHandle_t _taskHandle;
};

#endif // TELNET_SERVER_H