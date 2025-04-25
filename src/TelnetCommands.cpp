#include "TelnetCommands.h"

Preferences TelnetCommands::preferences;

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
    server.addCommand("nvs", handleNvsCommand);
    server.addCommand("ifconfig", handleIfConfigCommand);
    server.addCommand("ping", handlePingCommand);
    server.addCommand("clear", handleClearScreen);
    server.addCommand("exit", handleExitCommand);

    // Handler padrão
    server.setDefaultHandler(handleUnknownCommand);
}

void TelnetCommands::handleHelp(WiFiClient &client, const String &) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
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
    client.println(String(styleText("nvs -[command] [namespace] [data]", "white", true, false) + "    - " +
                          styleText("Exibe conteúdo de arquivos", "cyan", false, false)));
    client.println(String(styleText("ifconfig", "white", true, false) + "     - " +
                          styleText("Exibe informações de rede", "cyan", false, false)));
    client.println(String(styleText("ping <hostname/IP> [tentativas]", "white", true, false) + " - " +
                          styleText("Testa a conectividade de rede com outro dispositivo", "cyan", false, false)));
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
        client.println(styleText("Diretório não encontrado: ", "red", true) + path);
        return;
    }

    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    client.println(styleText("Conteúdo de ", "yellow", true) + path + ":");
    File dir = LittleFS.open(path);
    File file = dir.openNextFile();

    while (file) {
        if (file.isDirectory()) {
            client.printf("%s\r\n", styleText("[DIR]  " + String(file.name()), "blue", true).c_str());
        } else {
            String line = String(file.name()) + " " + String(file.size()) + " bytes";
            client.printf("%s\r\n", styleText(line, "white").c_str());
        }
        file = dir.openNextFile();
    }
    client.println();
}

// Implementação do cd
void TelnetCommands::handleChangeDir(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    if (activeServer == nullptr) {
        if (_log)
            LOG_ERROR("[CD] Erro: servidor não inicializado");
        client.println(styleText("Erro interno do servidor", "red", true));
        return;
    }

    if (cmd.length() >= 5 && cmd.substring(3, 5) == "-h") { // "cd -h"
        helpChangeDir(client);
        return;
    }

    if (cmd.length() <= 3) { // Apenas "cd" sem argumentos
        changeToRootDir();
        updatePrompt(*activeServer);
        if (activeServer->isEchoEnabled()) {
            client.println();
        }
        client.println(styleText("Diretório atual: /", "green"));
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
        client.println(styleText("Diretório atual: " + currentDirectory, "green"));
        return;
    }

    String newPath = resolvePath(target);

    if (!LittleFS.exists(newPath)) {
        client.println(styleText("Diretório não encontrado: " + newPath, "red", true));
        return;
    }

    if (!isDirectory(newPath)) {
        client.println(styleText(newPath + " não é um diretório", "red", true));
        return;
    }

    changeToDirectory(newPath);
    // currentDirectory = newPath;

    if (_log == true) {
        LOG_INFO("[CD] Mudou para: %s\n", currentDirectory.c_str());
    }
    updatePrompt(*activeServer);
    client.println(styleText("Diretório atual: " + currentDirectory, "green"));
    client.clear();
}

// Implementação do cat
void TelnetCommands::handleCatCommand(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    if (cmd.length() <= 4) { // Apenas "cat" sem argumentos
        client.println(styleText("Uso: cat <arquivo>", "yellow", true));
        client.println("Exibe o conteúdo de um arquivo");
        return;
    }

    // Correção: primeiro extrai a substring, depois aplica trim()
    String pathArg = cmd.substring(4);
    pathArg.trim();
    String filePath = resolvePath(pathArg);

    // Verifica se o usuário pediu versão (cat -v)
    if (pathArg == "-v") {
        client.println(styleText("cat versão 1.0", "cyan"));
        client.println("Suporta exibição de arquivos texto e binários");
        return;
    }

    // Verifica se o usuário pediu ajuda (cat -h)
    if (pathArg == "-h") {
        helpCatCommand(client);
        return;
    }

    if (!LittleFS.exists(filePath)) {
        client.println(styleText("Arquivo não encontrado: " + filePath, "red", true));
        return;
    }

    if (isDirectory(filePath)) {
        client.println(styleText(filePath + " é um diretório, não um arquivo", "red", true));
        return;
    }

    File file = LittleFS.open(filePath, "r");
    if (!file) {
        client.println(styleText("Erro ao abrir o arquivo: " + filePath, "red", true));
        return;
    }

    client.println(styleText("Conteúdo de " + filePath + ":", "green"));
    client.println(
        styleText("--------------------------------------------------", "white", false, true)); // itálico branco

    // Lê e envia o arquivo em chunks para evitar sobrecarregar a memória
    const size_t bufferSize = 256;
    uint8_t buffer[bufferSize];
    size_t bytesRead;

    while ((bytesRead = file.read(buffer, bufferSize)) > 0) {
        client.write(buffer, bytesRead);
    }

    client.println(styleText("\r\n--------------------------------------------------", "white", false, true));
    client.println(styleText("Fim do arquivo (" + String(file.size()) + " bytes)", "green"));

    file.close();
}

/**
 * @brief Comando nvs: manipula o armazenamento de chaves-valor no NVS (Non-Volatile Storage)
 *
 * Comandos:
 *  nvs -h          - Ajuda
 *  nvs -rd <namespace> <chave> - Leia o valor de uma chave
 *  nvs -s <namespace> <chave=valor> - Defina o valor de uma chave
 *  nvs -x <namespace> - Apague todo o namespace
 */
void TelnetCommands::handleNvsCommand(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    std::vector<String> args = splitCommand(cmd, ' ');

    const String &operation = args[1];

    if (_log == true) {
        LOG_DEBUG("[NVS] Command '%s'", args[0].c_str());
        LOG_DEBUG("[NVS] Function '%s'", operation.c_str());
        LOG_DEBUG("[NVS] Arguments '%s'", args[2].c_str());
        LOG_DEBUG("[NVS] Arguments '%s'", args[3].c_str());
    }

    if (operation == "-h" || operation.length() == 0) {
        helpNvsCommand(client);
        return;
    }

    else if (operation == "-rd") {
        if (args.size() < 4) {
            client.println(styleText("Uso: nvsget -rd <namespace> <key>", "yellow", true));
            client.println("Le um valor de uma chave no NVS");
            return;
        }

        String nameSpace = args[2];
        String key;
        if (_log == true) {
            LOG_INFO("nvsget -rd %s\n", nameSpace.c_str());
        }
        // Remover o prefixo "-r" e depois dividir os argumentos restantes em namespace e chave

        String remainingArgs = args[3];

        int spaceIndex = remainingArgs.indexOf(" ");
        if (spaceIndex != -1) {
            nameSpace = remainingArgs.substring(0, spaceIndex);
            key = remainingArgs.substring(spaceIndex + 1);
        } else {
            client.println(styleText("Erro: ", "red", true) + "Faltando chave ou namespace.");
            return;
        }

        // Abrir o namespace e tentar ler o valor da chave
        preferences.begin(nameSpace.c_str(), true);

        if (!preferences.isKey(key.c_str())) {
            client.printf("Chave '%s' não encontrada no namespace '%s'.\r\n", key.c_str(), nameSpace.c_str());
            preferences.end();
            return;
        }

        String value = preferences.getString(key.c_str(), "[vazio]");
        client.printf("Valor de '%s' no namespace '%s': %s\r\n", styleText(key, "green", true, false).c_str(),
                      styleText(nameSpace, "green", true, false).c_str(),
                      styleText(value, "green", true, false).c_str());
        preferences.end();
        return;
    }

    else if (operation == "-s") { // Formato: nvs -s <namespace> <chave=valor>
        String nameSpace = args[2];
        if (args.size() < 4) {
            client.println(styleText("Erro: ", "red", true) +
                           "Formato inválido. Use: nvs -s <namespace> <chave=valor>");
            return;
        }

        std::vector<String> temp = splitCommand(args[3], '=');
        if (temp.size() != 2) {
            client.println(styleText("Erro: ", "red", true) +
                           "Formato inválido. Use: nvs -s <namespace> <chave=valor>");
            return;
        }
        String key = temp[0];
        String value = temp[1];

        if (_log) {
            LOG_INFO("nvs -s %s %s=%s\n", nameSpace.c_str(), key.c_str(), value.c_str());
        }

        // Abre o namespace NVS
        preferences.begin(nameSpace.c_str(), false);

        // Verifica se a chave já existe e pede confirmação para sobrescrever
        if (preferences.isKey(key.c_str())) {

            client.printf("%s A chave '%s' já existe no namespace '%s'.\r\n"
                          "Valor atual: %s\r\n"
                          "%s\r\n",
                          styleText("AVISO: ", "yellow", true).c_str(), styleText(key, "green").c_str(),
                          styleText(nameSpace, "green").c_str(),
                          styleText(preferences.getString(key.c_str(), ""), "yellow").c_str(),
                          styleText("Deseja sobrescrever? (S/N)", "yellow", true).c_str());
            // client.println(buffer);
            client.clear();

            unsigned long startTime = millis();
            while (millis() - startTime < 5000) { // Timeout de 5 segundos
                if (client.available()) {
                    char response = client.read();
                    if (response != 's' && response != 'S') {
                        client.println(styleText("Operação cancelada.", "yellow", true));
                        preferences.end();
                        return;
                    } else {
                        preferences.putString(key.c_str(), value.c_str());
                        preferences.end();

                        client.printf("%s Chave '%s' definida como '%s' no namespace '%s'.\r\n",
                                      styleText("SUCESSO: ", "green", true).c_str(), styleText(key, "green").c_str(),
                                      styleText(value, "green").c_str(), styleText(nameSpace, "green").c_str());
                        return;
                    }
                }
                delay(50);
            }

            client.println(styleText("Tempo limite atingido. Operação cancelada.", "yellow", true));
            preferences.end();
            return;
        } else { // ADICIONE ESTE BLOCO PARA NOVAS CHAVES
            preferences.putString(key.c_str(), value.c_str());
            preferences.end();
            client.printf("%s Chave '%s' criada com valor '%s' no namespace '%s'.\r\n",
                          styleText("SUCESSO: ", "green", true).c_str(), styleText(key, "green").c_str(),
                          styleText(value, "green").c_str(), styleText(nameSpace, "green").c_str());
            return;
        }
    }

    else if (operation == "-x") { // Apagar namespace
        if (args.size() < 3) {
            client.println(styleText("Erro: ", "red", true) + "Especifique um namespace para apagar.");
            return;
        }

        String nameSpace = args[2];
        if (nameSpace.length() == 0) {
            client.println(styleText("Erro: ", "red", true) + "Especifique um namespace para apagar.");
            return;
        }

        client.print(styleText("AVISO: ", "yellow", true));
        client.printf("Tem certeza que deseja apagar todo o namespace '%s'? (S/N)\r\n",
                      styleText(nameSpace, "yellow", true).c_str());

        client.clear();

        unsigned long startTime = millis();
        while (millis() - startTime < 5000) {
            if (client.available()) {
                char response = client.read();
                if (response == 's' || response == 'S') {
                    preferences.begin(nameSpace.c_str(), false);
                    if (preferences.clear()) { // Apaga todas as chaves do namespace
                        preferences.end();
                        client.println(styleText("SUCESSO: ", "green", true) + "Namespace '" + nameSpace +
                                       "' apagado completamente.");
                    } else {
                        client.println(styleText("ERRO: ", "red", true) + "Falha ao apagar o namespace '" + nameSpace +
                                       "'");
                    }
                    return;
                } else {
                    client.println(styleText("Operação cancelada.", "yellow", true));
                    return;
                }
            }
            delay(50);
        }
        client.println(styleText("Tempo limite atingido. Operação cancelada.", "yellow", true));
        return;
    }

    else if (operation.startsWith("-")) {
        if (_log) {
            LOG_INFO("[NVS] Opção inválida: %s", operation.c_str());
        }
        client.printf("Opção inválida: %s\r\n", styleText(operation, "yellow", true).c_str());
        return;
    }
    if (_log) {
        LOG_INFO("[NVS] Não foi selecionado uma opção verifique o help -h");
    }
    client.println(styleText("Não foi selecionado uma opção verifique o help -h ", "yellow", true));
}

void TelnetCommands::handleIfConfigCommand(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    String args = cmd.substring(8); // remove "ifconfig"
    args.trim();

    if (args == "-h") {
        helpIfConfigCommand(client);
        return;
    }

    int wifiStatus = WiFi.status();

    // Cabeçalho
    client.println(styleText("\nInformações de Rede:", "yellow", true));
    client.println(styleText("====================", "yellow"));

    // Status da conexão WiFi
    String statusText;
    switch (wifiStatus) {
    case WL_NO_SHIELD:
        statusText = "No shield";
        break;
    case WL_IDLE_STATUS:
        statusText = "Idle";
        break;
    case WL_NO_SSID_AVAIL:
        statusText = "No SSID available";
        break;
    case WL_SCAN_COMPLETED:
        statusText = "Scan completed";
        break;
    case WL_CONNECTED:
        statusText = "Connected";
        break;
    case WL_CONNECT_FAILED:
        statusText = "Connection failed";
        break;
    case WL_CONNECTION_LOST:
        statusText = "Connection lost";
        break;
    case WL_DISCONNECTED:
        statusText = "Disconnected";
        break;
    default:
        statusText = "Unknown status";
    }
    client.printf("%s: %s\r\n", styleText("Status WiFi", "cyan", true).c_str(),
                  styleText(statusText, wifiStatus == WL_CONNECTED ? "green" : "red", true).c_str());

    // Informações quando conectado
    if (wifiStatus == WL_CONNECTED) {
        client.println(styleText("\nConfiguração IP:", "cyan", true));
        client.printf("  %-10s: %s\r\n", "IP", styleText(WiFi.localIP().toString(), "green").c_str());
        client.printf("  %-10s: %s\r\n", "Gateway", styleText(WiFi.gatewayIP().toString(), "green").c_str());
        client.printf("  %-10s: %s\r\n", "Subnet", styleText(WiFi.subnetMask().toString(), "green").c_str());
        client.printf("  %-10s: %s\r\n", "mDNS", styleText(activeServer->getMdnsHostname(), "green").c_str());

        client.println(styleText("\nInformações WiFi:", "cyan", true));
        client.printf("  %-15s: %s\r\n", "SSID", styleText(WiFi.SSID(), "green").c_str());
        client.printf("  %-15s: %s\r\n", "BSSID", styleText(WiFi.BSSIDstr(), "green").c_str());
        client.printf("  %-15s: %d\r\n", "Canal", WiFi.channel());
        client.printf("  %-15s: %d dBm\r\n", "RSSI", WiFi.RSSI());
    }

    // Informações da interface (mesmo desconectado)
    client.println(styleText("\nInformações da Interface:", "cyan", true));
    client.printf("  %-15s: %s\r\n", "MAC Address", styleText(WiFi.macAddress(), "green").c_str());

    if (WiFi.getMode() & WIFI_AP) {
        client.printf("  %-15s: %s\r\n", "AP IP", styleText(WiFi.softAPIP().toString(), "green").c_str());
    }

    client.println();
}

void TelnetCommands::handlePingCommand(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    String args = cmd.substring(5); // remove "ping "
    args.trim();

    // Mostrar ajuda se solicitado
    if (args == "-h") {
        helpPingCommand(client);
        return;
    }

    // Verificar se há argumentos suficientes
    if (args.length() == 0) {
        client.println(styleText("Uso: ping <hostname/IP> [tentativas]", "red", true));
        client.println(styleText("Use 'ping -h' para ajuda", "yellow"));
        return;
    }

    // Extrair hostname e número de tentativas
    int spacePos = args.indexOf(' ');
    String hostname = args;
    int attempts = 4; // padrão: 4 tentativas

    if (spacePos != -1) {
        hostname = args.substring(0, spacePos);
        String attemptsStr = args.substring(spacePos + 1);
        attempts = attemptsStr.toInt();
        if (attempts < 1 || attempts > 10) {
            client.println(styleText("Número de tentativas deve estar entre 1 e 10", "red", true));
            return;
        }
    }

    client.println(styleText("Pingando " + hostname + " (" + String(attempts) + " tentativas)...", "cyan", true));
    client.println();

    // Executar o ping
    bool success = pingHost(hostname, attempts, client);

    client.println();
    if (success) {
        client.println(styleText("Ping concluído com sucesso!", "green", true));
    } else {
        client.println(styleText("Falha no ping para " + hostname, "red", true));
    }
}

// Implementação do pwd
void TelnetCommands::handlePrintWorkingDir(WiFiClient &client, const String &) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
}

// Implementação do exit
void TelnetCommands::handleExitCommand(WiFiClient &client, const String &cmd) {
    if (activeServer->isEchoEnabled()) {
        client.println();
    }
    String pathArg = cmd.substring(5);
    pathArg.trim();
    // Verifica se foi solicitada ajuda
    if (cmd.length() > 5 && pathArg == "-h") {
        helpExitCommand(client);
        return;
    }

    // Confirmação (opcional)
    client.println(styleText("Tem certeza que deseja sair? (S/N)", "yellow", true));
    client.clear();

    // Aguarda resposta (opcional)
    unsigned long startTime = millis();
    while (millis() - startTime < 5000) { // Timeout de 5 segundos
        if (client.available()) {
            char response = client.read();
            if (response == 's' || response == 'S') {
                client.println(styleText("Desconectando... Adeus!", "magenta", true));
                if (_log) {
                    LOG_INFO("[TELNET] Cliente desconectado via comando exit");
                }
                delay(100); // Tempo para enviar a mensagem
                client.stop();
                return;
            } else {
                client.println(styleText("Cancelado.", "cyan", true));
                return;
            }
        }
        delay(50);
    }

    client.println(styleText("Timeout. Comando cancelado.", "red", true));
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
    client.println(styleText("Comando desconhecido: '" + cmd + "'", "red", true));
    client.println(styleText("Digite 'help' para ver os comandos disponíveis", "yellow", true));
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

    client.println(styleText("Opções disponíveis:", "yellow", true));

    if (_log == true) {
        LOG_INFO("[CD] Buscando: %s\n", basePath.c_str());
    }

    // Listar diretórios no caminho base que começam com o padrão parcial
    File root = LittleFS.open(currentDirectory);
    if (!root) {
        client.println(styleText("  Nenhum diretório encontrado", "red", true));
        return;
    }

    if (!root.isDirectory()) {
        client.println(styleText("  Caminho atual não é um diretório", "red", true));
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
                client.println(styleText("  " + dirName + "/", "cyan", true));
                // client.printf("  %s/\r\n");
                found = true;
            }
        }
        file = root.openNextFile();
    }

    if (!found) {
        client.println(styleText("  Nenhum diretório correspondente encontrado", "red", true));
    }

    root.close();
    return;
}

void TelnetCommands::helpChangeDir(WiFiClient &client) {
    client.println(styleText("Ajuda do comando cd:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "cd [diretório]");

    client.println(styleText("Opções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "          Mostra esta ajuda");
    client.println(styleText("  ..", "cyan", true) + "          Volta para o diretório pai");
    client.println(styleText("  /", "cyan", true) + "           Vai para o diretório raiz");
    client.println(styleText("  [nome]?", "cyan", true) + "     Lista diretórios que começam com [nome]");

    client.println(styleText("Exemplos:", "green", true));
    client.println(styleText("  cd /", "white") + "        Vai para o diretório raiz");
    client.println(styleText("  cd dir", "white") + "      Entra no diretório 'dir'");
    client.println(styleText("  cd ..", "white") + "       Volta um diretório");
    client.println(styleText("  cd abc?", "white") + "     Lista diretórios começando com 'abc'");
}

void TelnetCommands::helpCatCommand(WiFiClient &client) {
    client.println(styleText("Ajuda do comando cat:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "cat <arquivo>");

    client.println(styleText("Opções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "          Mostra esta ajuda");
    client.println(styleText("  -v", "cyan", true) + "          Mostra a versão do cat");
    client.println(styleText("  [arquivo]", "cyan", true) + "   Exibe o conteúdo do arquivo especificado");

    client.println(styleText("Exemplos:", "green", true));
    client.println(styleText("  cat /dados/config.txt", "white") + "  Exibe o arquivo config.txt");
    client.println(styleText("  cat teste.log", "white") + "          Exibe o arquivo teste.log no diretório atual");
}

void TelnetCommands::helpNvsCommand(WiFiClient &client) {
    client.println(styleText("Ajuda do comando nvs:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "cat -<comando> <namespace> <chave>=<valor>");

    client.println(styleText("Opções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "          Mostra esta ajuda");
    client.println(styleText("  -rd [namespace] [chave]", "cyan", true) +
                   "          Mostra o valor da chave armazenada no NVS");
    client.println(styleText("  -s [namespace] [chave]=[valor]", "cyan", true) +
                   "          Armazena o valor na chave do NVS");

    client.println(styleText("Exemplos:", "green", true));
    client.println(styleText("  nvs -rd default wifi", "white") +
                   "            Exibe o valor de wifi no namespace default da NVS");
    client.println(styleText("  nvs -s default wifi_ssid=teste", "white") +
                   "          Salva o valor de wifi_ssid=teste no namespace default da NVS");

    return;
}

void TelnetCommands::helpIfConfigCommand(WiFiClient &client) {
    client.println(styleText("Ajuda do comando ifconfig:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "ifconfig");
    client.println("Exibe informações detalhadas da interface de rede");

    client.println(styleText("\nInformações exibidas:", "cyan", true));
    client.println("  - Status da conexão WiFi");
    client.println("  - Configuração IP (quando conectado)");
    client.println("  - Informações da rede WiFi");
    client.println("  - Endereço MAC");
    client.println("  - Configuração do AP (se ativo)");

    client.println(styleText("\nOpções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "  Mostra esta ajuda");
}

void TelnetCommands::helpPingCommand(WiFiClient &client) {
    client.println(styleText("Ajuda do comando ping:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "ping <hostname/IP> [tentativas]");

    client.println(styleText("Descrição:", "green", true));
    client.println("Testa a conectividade de rede com outro dispositivo");

    client.println(styleText("Parâmetros:", "green", true));
    client.println(styleText("  hostname/IP", "cyan", true) + "  Nome ou endereço IP do host de destino");
    client.println(styleText("  tentativas", "cyan", true) + "   Número de tentativas (1-10, padrão=4)");

    client.println(styleText("Opções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "          Mostra esta ajuda");

    client.println(styleText("Exemplos:", "green", true));
    client.println(styleText("  ping 192.168.1.1", "white") + "       Ping para o endereço IP");
    client.println(styleText("  ping google.com", "white") + "        Ping para um hostname");
    client.println(styleText("  ping 192.168.1.1 6", "white") + "    Ping com 6 tentativas");
}

void TelnetCommands::helpExitCommand(WiFiClient &client) {
    client.println(styleText("Ajuda do comando exit:", "yellow", true));
    client.println(styleText("Uso: ", "green", true) + "exit");
    client.println("Encerra a sessão Telnet atual");

    client.println(styleText("Opções:", "green", true));
    client.println(styleText("  -h", "cyan", true) + "  Mostra esta ajuda");
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

bool TelnetCommands::pingHost(const String &hostname, int maxAttempts, WiFiClient &client) {
    IPAddress ip;

    // Resolver o hostname para IP se necessário
    if (!WiFi.hostByName(hostname.c_str(), ip)) {
        client.println(styleText("Não foi possível resolver o hostname: " + hostname, "red", true));
        return false;
    }

    client.printf("Pingando %s [%s] com %d bytes de dados:\r\n", styleText(hostname, "cyan").c_str(),
                  styleText(ip.toString(), "cyan").c_str(), 32);

    bool anySuccess = false;
    int packetSize = 32;
    int totalTime = 0;
    int successCount = 0;

    for (int i = 0; i < maxAttempts; i++) {
        unsigned long startTime = millis();
        bool reply = Ping.ping(ip, 1); // 1 tentativa por ping

        if (reply) {
            unsigned long duration = millis() - startTime;
            client.printf("Resposta de %s: bytes=%d tempo=%lums TTL=%d\r\n", styleText(ip.toString(), "green").c_str(),
                          packetSize, duration,
                          128); // TTL fixo (não temos essa informação)
            totalTime += duration;
            successCount++;
            anySuccess = true;
        } else {
            client.println(styleText("Tempo limite esgotado.", "red"));
        }

        delay(1000); // Intervalo entre pings
    }

    // Estatísticas
    if (successCount > 0) {
        client.println();
        client.println(styleText("Estatísticas do Ping:", "yellow", true));
        client.printf("    Pacotes: Enviados = %d, Recebidos = %d, Perdidos = %d (%.0f%% de perda)\r\n", maxAttempts,
                      successCount, maxAttempts - successCount, ((maxAttempts - successCount) * 100.0 / maxAttempts));

        if (successCount > 0) {
            client.printf("Tempo aproximado de ida e volta em ms:\r\n"
                          "    Mínimo = %dms, Máximo = %dms, Média = %dms\r\n",
                          totalTime / successCount, // Simplificação
                          totalTime / successCount, totalTime / successCount);
        }
    }

    return anySuccess;
}

String TelnetCommands::getCommand(const String &cmd) {
    String trimmedCmd = cmd;
    trimmedCmd.trim();
    int spaceIndex = trimmedCmd.indexOf(' ');
    return (spaceIndex == -1) ? trimmedCmd : trimmedCmd.substring(0, spaceIndex);
}

std::vector<String> TelnetCommands::splitCommand(const String &cmd, char delimiter) {
    std::vector<String> parts;
    int start = 0;
    int end = cmd.indexOf(delimiter);

    while (end != -1) {
        String part = cmd.substring(start, end);
        part.trim();
        if (part.length() > 0) {
            parts.push_back(part);
        }
        start = end + 1;
        end = cmd.indexOf(delimiter, start);
    }

    // Adiciona a última parte
    String lastPart = cmd.substring(start);
    lastPart.trim();
    if (lastPart.length() > 0) {
        parts.push_back(lastPart);
    }

    return parts;
}