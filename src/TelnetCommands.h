#ifndef TELNET_COMMANDS_H
#define TELNET_COMMANDS_H

#include "TelnetServer.h"
#include <LittleFS.h>

class TelnetCommands {
  public:
    static void setupDefaultCommands(TelnetServer &server);

  private:
    // Handlers de comandos
    static void handleHelp(WiFiClient &client, const String &);
    static void handleListFiles(WiFiClient &client, const String &cmd);
    static void handleChangeDir(WiFiClient &client, const String &cmd);
    static void handleClearScreen(WiFiClient &client, const String &);
    static void handlePrintWorkingDir(WiFiClient &client, const String &);
    static void handleTabCompletion(WiFiClient &client, const String &cmd);
    static void handleUnknownCommand(WiFiClient &client, const String &cmd);

    // Variáveis estáticas
    static String currentDirectory;

    // Métodos auxiliares
    static String resolvePath(const String &path);
    static bool isDirectory(const String &path);
    static bool matchesWildcard(const String &name, const String &pattern);
    static void sendColoredEntry(WiFiClient &client, const String &name, bool isDir);
    static String findCommonPrefix(const std::vector<String> &matches);
    static void updatePrompt(TelnetServer &server);
};

#endif // TELNET_COMMANDS_H