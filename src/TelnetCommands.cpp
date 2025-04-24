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
    server.addCommand("cat", handleCatCommand);
    server.addCommand("clear", handleClearScreen);
    server.addCommand("exit", handleExitCommand);

    // Handler padrão
    server.setDefaultHandler(handleUnknownCommand);
}

void TelnetCommands::handleHelp(WiFiClient &client, const String &) {
    client.println(styleText("Comandos disponíveis:", "yellow", true, false));
    client.println();

    // Lista de comandos formatados
    client.println(String(styleText("help", "white", true, false) + "          - " +
                          styleText("Mostra esta ajuda", "cyan", false, false)));
    client.println(String(styleText("ls [path]", "white", true, false) + "     - " +
                          styleText("Lista arquivos no diretório atual ou especificado", "cyan", false, false)));
    client.println(String(styleText("cd [path]", "white", true, false) + "     - " +
                          styleText("Muda para o diretório especificado", "cyan", false, false)));
    client.println(String(styleText("pwd", "white", true, false) + "           - " +
                          styleText("Mostra o diretório atual", "cyan", false, false)));
    client.println(String(styleText("cat [file]", "white", true, false) + "    - " +
                          styleText("Exibe conteúdo de arquivos", "cyan", false, false)));
    client.println(String(styleText("clear", "white", true, false) + "         - " +
                          styleText("Limpa a tela", "cyan", false, false)));
    client.println(String(styleText("exit", "white", true, false) + "          - " +
                          styleText("Encerra a sessão Telnet", "cyan", false, false)));
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

    if (cmd.length() >= 5 && cmd.substring(3, 5) == "-h") { // "cd -h"
        helpChangeDir(client);
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

// Implementação do cat
void TelnetCommands::handleCatCommand(WiFiClient &client, const String &cmd) {
    if (cmd.length() <= 4) { // Apenas "cat" sem argumentos
        client.println("Uso: cat <arquivo>");
        client.println("Exibe o conteúdo de um arquivo");
        return;
    }

    // Correção: primeiro extrai a substring, depois aplica trim()
    String pathArg = cmd.substring(4);
    pathArg.trim();
    String filePath = resolvePath(pathArg);

    // Verifica se o usuário pediu versão (cat -v)
    if (pathArg == "-v") {
        client.println("cat versão 1.0");
        client.println("Suporta exibição de arquivos texto e binários");
        return;
    }

    // Verifica se o usuário pediu ajuda (cat -h)
    if (pathArg == "-h") {
        helpCatCommand(client);
    }

    if (!LittleFS.exists(filePath)) {
        client.printf("Arquivo não encontrado: %s\r\n", filePath.c_str());
        return;
    }

    if (isDirectory(filePath)) {
        client.printf("%s é um diretório, não um arquivo\r\n", filePath.c_str());
        return;
    }

    File file = LittleFS.open(filePath, "r");
    if (!file) {
        client.printf("Erro ao abrir o arquivo: %s\r\n", filePath.c_str());
        return;
    }

    client.printf("Conteúdo de %s:\r\n", filePath.c_str());
    client.println("--------------------------------------------------");

    // Lê e envia o arquivo em chunks para evitar sobrecarregar a memória
    const size_t bufferSize = 256;
    uint8_t buffer[bufferSize];
    size_t bytesRead;

    while ((bytesRead = file.read(buffer, bufferSize)) > 0) {
        client.write(buffer, bytesRead);
    }

    client.println("\r\n--------------------------------------------------");
    client.printf("Fim do arquivo (%d bytes)\r\n", file.size());

    file.close();
}

// Implementação do pwd
void TelnetCommands::handlePrintWorkingDir(WiFiClient &client, const String &) {
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
}

// Implementação do exit
void TelnetCommands::handleExitCommand(WiFiClient &client, const String &cmd) {

    String pathArg = cmd.substring(5);
    pathArg.trim();
    // Verifica se foi solicitada ajuda
    if (cmd.length() > 5 && pathArg == "-h") {
        helpExitCommand(client);
        return;
    }

    // Confirmação (opcional)
    client.println("Tem certeza que deseja sair? (s/n)");
    client.clear();

    // Aguarda resposta (opcional)
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // Timeout de 5 segundos
        if (client.available()) {
            char response = client.read();
            if (response == 's' || response == 'S') {
                client.println("Desconectando... Adeus!");
                if (_log) {
                    LOG_INFO("[TELNET] Cliente desconectado via comando exit");
                }
                delay(100); // Tempo para enviar a mensagem
                client.stop();
                return;
            } else {
                client.println("Cancelado.");
                return;
            }
        }
        delay(50);
    }

    client.println("Timeout. Comando cancelado.");
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

void TelnetCommands::helpChangeDir(WiFiClient &client) {
    client.println("Ajuda do comando cd:");
    client.println("Uso: cd [diretório]");
    client.println("Opções:");
    client.println("  -h          Mostra esta ajuda");
    client.println("  ..          Volta para o diretório pai");
    client.println("  /           Vai para o diretório raiz");
    client.println("  [nome]?     Lista diretórios que começam com [nome]");
    client.println("Exemplos:");
    client.println("  cd /        Vai para o diretório raiz");
    client.println("  cd dir      Entra no diretório 'dir'");
    client.println("  cd ..       Volta um diretório");
    client.println("  cd abc?     Lista diretórios começando com 'abc'");
}

void TelnetCommands::helpCatCommand(WiFiClient &client) {
    client.println("Ajuda do comando cat:");
    client.println("Uso: cat <arquivo>");
    client.println("Opções:");
    client.println("  -h          Mostra esta ajuda");
    client.println("  -v          Mostra a versão do cat");
    client.println("  [arquivo]   Exibe o conteúdo do arquivo especificado");
    client.println("Exemplos:");
    client.println("  cat /dados/config.txt  Exibe o arquivo config.txt");
    client.println("  cat teste.log          Exibe o arquivo teste.log no diretório atual");
}

void TelnetCommands::helpExitCommand(WiFiClient &client) {
    client.println("Ajuda do comando exit:");
    client.println("Uso: exit");
    client.println("Encerra a sessão Telnet atual");
    client.println("Opções:");
    client.println("  -h  Mostra esta ajuda");
}

String TelnetCommands::styleText(const String &text, const String &colorName, bool bold, bool italic) {
    String colorCode = getAnsiColorCode(colorName);
    String styleCode = "";

    if (bold)
        styleCode += "\033[1m";
    if (italic)
        styleCode += "\033[3m";

    return styleCode + colorCode + text + "\033[0m";
}

String TelnetCommands::getAnsiColorCode(const String &colorName) {
    if (colorName == "black")
        return "\033[30m";
    if (colorName == "red")
        return "\033[31m";
    if (colorName == "green")
        return "\033[32m";
    if (colorName == "yellow")
        return "\033[33m";
    if (colorName == "blue")
        return "\033[34m";
    if (colorName == "magenta")
        return "\033[35m";
    if (colorName == "cyan")
        return "\033[36m";
    if (colorName == "white")
        return "\033[37m";
    return ""; // default sem cor
}