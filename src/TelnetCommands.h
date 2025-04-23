#ifndef TELNET_COMMANDS_H
#define TELNET_COMMANDS_H

#include "TelnetServer.h"
#include <LittleFS.h>

class TelnetCommands {
  public:
    static void setupDefaultCommands(TelnetServer &server);

  private:
    // Implementação dos handlers (pode ser movida para o .cpp se preferir)
    static void handleHelp(WiFiClient &client, const String &);
    static void handleListFiles(WiFiClient &client, const String &);
    static void handleChangeDir(WiFiClient &client, const String &cmd);
    static void handleClearScreen(WiFiClient &client, const String &);
    static void handlePrintWorkingDir(WiFiClient &client, const String &);
    static void handleUnknownCommand(WiFiClient &client, const String &);

    // Variável estática para armazenar o diretório atual
    static String currentDirectory;

    // Métodos auxiliares
    static String resolvePath(const String &path);
    static bool isDirectory(const String &path);
};

#endif // TELNET_COMMANDS_H