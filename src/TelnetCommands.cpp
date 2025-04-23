#include "TelnetCommands.h"

// Inicializa a variável estática
String TelnetCommands::currentDirectory = "/";

void TelnetCommands::setupDefaultCommands(TelnetServer &server) {
    // Configurações básicas
    server.setWelcomeMessage("Bem-vindo ao servidor Telnet\r\n"
                             "Digite 'help' para lista de comandos\r\n\r\n");

    server.setPrompt(currentDirectory + "> ");
    server.enableEcho(false);

    // Comandos básicos
    server.addCommand("help", handleHelp);
    server.addCommand("ls", handleListFiles);
    server.addCommand("cd", handleChangeDir);
    server.addCommand("pwd", handlePrintWorkingDir);
    server.addCommand("clear", handleClearScreen);

    // Handler padrão
    server.setDefaultHandler(handleUnknownCommand);
}

void TelnetCommands::handleHelp(WiFiClient &client, const String &) {
    client.println("Comandos disponíveis:");
    client.println("help          - Mostra esta ajuda");
    client.println("ls [path]     - Lista arquivos no diretório atual ou especificado");
    client.println("cd [path]     - Muda para o diretório especificado");
    client.println("pwd           - Mostra o diretório atual");
    client.println("clear         - Limpa a tela");
    client.println("exit          - Desconecta");
    client.println();
}

// Implementação do ls com suporte a caminhos
void TelnetCommands::handleListFiles(WiFiClient &client, const String &cmd) {
    String path = currentDirectory;

    // Verifica se foi passado um caminho específico
    if (cmd.length() > 2) {
        path = resolvePath(cmd.substring(3)); // Remove "ls "
    }

    if (!LittleFS.exists(path)) {
        client.printf("Diretório não encontrado: %s\r\n", path.c_str());
        return;
    }

    client.printf("Conteúdo de %s:\r\n", path.c_str());
    File dir = LittleFS.open(path);
    File file = dir.openNextFile();

    while (file) {
        if (file.isDirectory()) {
            client.printf("[DIR]  %-20s\r\n", file.name());
        } else {
            client.printf("%-20s %8d bytes\r\n", file.name(), file.size());
        }
        file = dir.openNextFile();
    }
    client.println();
}

// Implementação do cd
void TelnetCommands::handleChangeDir(WiFiClient &client, const String &cmd) {
    if (cmd.length() <= 3) { // Apenas "cd" sem argumentos
        currentDirectory = "/";
        client.println("Diretório atual: /");
        return;
    }

    String newPath = resolvePath(cmd.substring(3)); // Remove "cd "

    if (newPath == "..") {
        int lastSlash = currentDirectory.lastIndexOf('/');
        if (lastSlash > 0) {
            currentDirectory = currentDirectory.substring(0, lastSlash);
        } else {
            currentDirectory = "/";
        }
        client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
        return;
    }

    if (!LittleFS.exists(newPath)) {
        client.printf("Diretório não encontrado: %s\r\n", newPath.c_str());
        return;
    }

    if (!isDirectory(newPath)) {
        client.printf("%s não é um diretório\r\n", newPath.c_str());
        return;
    }

    currentDirectory = newPath;
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
}

// Implementação do pwd
void TelnetCommands::handlePrintWorkingDir(WiFiClient &client, const String &) {
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
}

// Métodos auxiliares
String TelnetCommands::resolvePath(const String &path) {
    if (path.startsWith("/")) {
        return path;
    }

    // Remove espaços extras
    String cleanPath = path;
    cleanPath.trim();

    // Resolve caminho relativo
    if (currentDirectory.endsWith("/")) {
        return currentDirectory + cleanPath;
    }
    return currentDirectory + "/" + cleanPath;
}

bool TelnetCommands::isDirectory(const String &path) {
    File file = LittleFS.open(path);
    bool result = file.isDirectory();
    file.close();
    return result;
}

void TelnetCommands::handleClearScreen(WiFiClient &client, const String &) {

    // Envia sequência ANSI para limpar a tela
    client.write(27);    // ESC
    client.print("[2J"); // Código para limpar tela
    client.write(27);    // ESC
    client.print("[H");  // Código para mover cursor para home
}

void TelnetCommands::handleUnknownCommand(WiFiClient &client, const String &cmd) {
    client.println();
    client.printf("Comando desconhecido: '%s'\r\n", cmd.c_str());
    client.println("Digite 'help' para ver os comandos disponíveis");
    client.println();
}