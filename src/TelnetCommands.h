#ifndef TELNET_COMMANDS_H
#define TELNET_COMMANDS_H

#include "TelnetServer.h"
#include <ESP32Ping.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFiLib.h>

class TelnetCommands {
  public:
    static void setupDefaultCommands(TelnetServer &server);

  private:
    static bool _log;
    static Preferences preferences;

    // Implementação dos handlers (pode ser movida para o .cpp se preferir)
    static void handleHelp(WiFiClient &client, const String &);
    static void handleListFiles(WiFiClient &client, const String &);
    static void handleChangeDir(WiFiClient &client, const String &cmd);
    static void handleClearScreen(WiFiClient &client, const String &);
    static void handlePrintWorkingDir(WiFiClient &client, const String &);
    static void handleCatCommand(WiFiClient &client, const String &cmd);
    static void handleNvsCommand(WiFiClient &client, const String &cmd);
    static void handleIfConfigCommand(WiFiClient &client, const String &cmd);
    static void handlePingCommand(WiFiClient &client, const String &cmd);
    static void handleUnknownCommand(WiFiClient &client, const String &);
    static void handleExitCommand(WiFiClient &client, const String &cmd);

    // helps
    static void helpChangeDir(WiFiClient &client);
    static void helpCatCommand(WiFiClient &client);
    static void helpNvsCommand(WiFiClient &client);
    static void helpIfConfigCommand(WiFiClient &client);
    static void helpPingCommand(WiFiClient &client);
    static void helpExitCommand(WiFiClient &client);

    // Variável estática para armazenar o diretório atual
    static String currentDirectory;

    // Métodos auxiliares
    static String resolvePath(const String &path);
    static bool isDirectory(const String &path);

    static void updatePrompt(TelnetServer &server);

    static void changeToParentDir();
    static void changeToDirectory(const String &path);
    static void changeToRootDir();

    static void findPartialPath(WiFiClient &client, const String &target);

    static String styleText(const String &text, const String &colorName, bool bold = false, bool italic = false);
    static String getAnsiColorCode(const String &colorName);

    static bool pingHost(const String &hostname, int maxAttempts, WiFiClient &client);

    static String getCommand(const String &cmd);
    static std::vector<String> splitCommand(const String &cmd, char delimiter = ' ');
};

#endif // TELNET_COMMANDS_H