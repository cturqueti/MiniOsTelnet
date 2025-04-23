#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

#define CLIENT_TIMEOUT_MS 300000 // 5 minutos
#define MAX_BUFFER_SIZE 256

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

    struct ClientContext {
        WiFiClient client;
        String buffer;
        uint32_t lastActivity;
        bool authenticated; // Adicione este membro
    };

    TelnetServer(uint16_t port = 23, bool logEnabled = true);
    ~TelnetServer();

    void begin();
    void stop();
    void update();

    void addCommand(const String &command, CommandHandler handler);
    void setDefaultHandler(CommandHandler handler);
    void setTabHandler(CommandHandler handler); // Novo método
    void setWelcomeMessage(const String &message);
    void setPrompt(const String &prompt);

    size_t getClientCount() const;
    void disconnectAllClients();

    void handleNewConnections();
    void handleExistingClients();
    void removeCommand(const String &command);
    void enableEcho(bool enable);
    void disconnectClient(ClientContext &context, const String &message);
    void sendTelnetCommand(WiFiClient &client, uint8_t cmd, uint8_t option);

  private:
    void handleClient(ClientContext &context);
    void processBuffer(ClientContext &context);
    void taskFunction();
    bool isTelnetCommand(uint8_t c) const;
    String filterTelnetCommands(const String &input) const;

    WiFiServer _server;
    uint16_t _port;
    bool _logEnabled;
    bool _echoEnabled = false;
    bool _running;
    String _welcomeMessage;
    String _prompt;

    std::vector<ClientContext> _clients;
    std::map<String, CommandHandler> _commandHandlers;
    CommandHandler _defaultHandler;
    CommandHandler _tabHandler; // Novo membro

    mutable std::mutex _mutex;
    TaskHandle_t _taskHandle;
};

#endif // TELNET_SERVER_H