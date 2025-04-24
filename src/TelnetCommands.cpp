#include "TelnetCommands.h"

// Inicializa a variável estática
String TelnetCommands::currentDirectory = "/";

static TelnetServer *activeServer = nullptr;
bool TelnetCommands::_log = true;

void TelnetCommands::setupDefaultCommands(TelnetServer &server) {
    // Configurações básicas
    server.setWelcomeMessage("Bem-vindo ao servidor Telnet\r\n"
                             "Digite 'help' para lista de comandos\r\n\r\n");

    activeServer = &server;
    updatePrompt(*activeServer);
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
    if (activeServer == nullptr) {
        if (_log)
            LOG_ERROR("[CD] Erro: servidor não inicializado");
        client.println("Erro interno do servidor");
        return;
    }

    if (cmd.length() <= 3) { // Apenas "cd" sem argumentos
        changeToRootDir();
        updatePrompt(*activeServer);
        client.println("Diretório atual: /");
        return;
    }

    // String target = resolvePath(cmd.substring(3)); // Remove "cd "
    String target = cmd.substring(3);
    if (target.endsWith("?")) {
        findPartialPath(client, target);
        return;
    }

    if (target == "..") {
        changeToParentDir();
        updatePrompt(*activeServer);
        client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
        return;
    }

    String newPath = resolvePath(target);

    if (!LittleFS.exists(newPath)) {
        client.printf("Diretório não encontrado: %s\r\n", newPath.c_str());
        return;
    }

    if (!isDirectory(newPath)) {
        client.printf("%s não é um diretório\r\n", newPath.c_str());
        return;
    }

    changeToDirectory(newPath);
    // currentDirectory = newPath;

    if (_log == true) {
        LOG_INFO("[CD] Mudou para: %s\n", currentDirectory.c_str());
    }
    updatePrompt(*activeServer);
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
    client.clear();
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

void TelnetCommands::updatePrompt(TelnetServer &server) {
    // if (_log == true) {
    //     LOG_DEBUG("[CD] Checkpoint 1");
    // }
    // Atualiza o prompt com o diretório atual (pode ser chamado após mudar de diretório)
    server.setPrompt("\033[1;34m" + currentDirectory + "> \033[0m");
}

void TelnetCommands::changeToParentDir() {
    int lastSlash = currentDirectory.lastIndexOf('/');
    if (lastSlash > 0) {
        currentDirectory = currentDirectory.substring(0, lastSlash);
    } else {
        currentDirectory = "/";
    }

    if (currentDirectory.length() == 0) {
        currentDirectory = "/";
    }

    if (_log)
        LOG_INFO("[CD] Mudou para: %s\n", currentDirectory.c_str());
}

void TelnetCommands::changeToDirectory(const String &path) {
    currentDirectory = path;
    if (_log)
        LOG_INFO("[CD] Mudou para: %s\n", currentDirectory.c_str());
}

void TelnetCommands::changeToRootDir() {
    currentDirectory = "/";
    if (_log)
        LOG_INFO("[CD] Mudou para: /\n");
}

void TelnetCommands::findPartialPath(WiFiClient &client, const String &target) {
    String partial = target.substring(0, target.length() - 1);
    String basePath = resolvePath(partial);

    client.println("Opções disponíveis:");

    if (_log == true) {
        LOG_INFO("[CD] Buscando: %s\n", basePath.c_str());
    }

    // Listar diretórios no caminho base que começam com o padrão parcial
    File root = LittleFS.open(currentDirectory);
    if (!root) {
        client.println("  Nenhum diretório encontrado");
        return;
    }

    if (!root.isDirectory()) {
        client.println("  Caminho atual não é um diretório");
        root.close();
        return;
    }

    File file = root.openNextFile();
    bool found = false;
    while (file) {
        if (file.isDirectory()) {
            String fileName = file.name();
            // Extrai apenas o nome do diretório (última parte do caminho)
            int lastSlash = fileName.lastIndexOf('/');
            String dirName = lastSlash >= 0 ? fileName.substring(lastSlash + 1) : fileName;

            // Se partial não está vazio, verifica se o diretório começa com o padrão
            if (partial.length() == 0 || dirName.startsWith(partial)) {
                client.printf("  %s/\r\n", dirName.c_str());
                found = true;
            }
        }
        file = root.openNextFile();
    }

    if (!found) {
        client.println("  Nenhum diretório correspondente encontrado");
    }

    root.close();
    return;
}