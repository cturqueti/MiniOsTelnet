#include "TelnetCommands.h"
#include <vector>

static TelnetServer *activeServer = nullptr;

// Inicializa a variável estática
String TelnetCommands::currentDirectory = "/";

void TelnetCommands::setupDefaultCommands(TelnetServer &server) {
    // Configurações básicas
    server.setWelcomeMessage("\033[1;32mBem-vindo ao servidor Telnet\033[0m\r\n"
                             "Digite 'help' para lista de comandos\r\n\r\n");

    activeServer = &server;
    updatePrompt(server);
    // server.enableEcho(false);
    server.setTabHandler(handleTabCompletion); // Configura o handler de TAB

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
    String pattern = "*";

    // Extrai caminho e padrão do comando
    if (cmd.length() > 2) {
        String args = cmd.substring(3);
        int spacePos = args.indexOf(' ');

        if (spacePos == -1) {
            // ls <pattern> ou ls <path>
            if (args.indexOf('*') != -1 || args.indexOf('?') != -1) {
                pattern = args;
            } else {
                path = resolvePath(args);
            }
        } else {
            // ls <path> <pattern>
            path = resolvePath(args.substring(0, spacePos));
            pattern = args.substring(spacePos + 1);
        }
    }

    if (!LittleFS.exists(path)) {
        client.printf("\033[1;31mDiretório não encontrado: %s\033[0m\r\n", path.c_str());
        return;
    }

    client.printf("\033[1;36mConteúdo de %s:\033[0m\r\n", path.c_str());
    File dir = LittleFS.open(path);
    File file = dir.openNextFile();
    int count = 0;

    while (file) {
        String fileName = file.name();
        // Remove o caminho completo se necessário
        if (fileName.startsWith(path)) {
            fileName = fileName.substring(path.length() + (path.endsWith("/") ? 0 : 1));
        }

        if (matchesWildcard(fileName, pattern)) {
            sendColoredEntry(client, fileName, file.isDirectory());
            count++;
        }
        file = dir.openNextFile();
    }

    client.printf("\r\n\033[1;33mTotal: %d itens\033[0m\r\n", count);
}

// Implementação do cd
void TelnetCommands::handleChangeDir(WiFiClient &client, const String &cmd) {
    if (cmd.length() <= 3) { // Apenas "cd" sem argumentos
        currentDirectory = "/";
        client.println("Diretório atual: /");
        return;
    }

    String newPath = cmd.substring(3);
    newPath.trim();

    // Trata cd ..
    if (newPath == "..") {
        int lastSlash = currentDirectory.lastIndexOf('/');
        if (lastSlash > 0) {
            currentDirectory = currentDirectory.substring(0, lastSlash);
        } else {
            currentDirectory = "/";
        }
        client.printf("\033[1;32mDiretório atual: %s\033[0m\r\n", currentDirectory.c_str());
        updatePrompt(*activeServer);
        return;
    }

    // Resolve caminho completo
    String fullPath = resolvePath(newPath);

    if (!LittleFS.exists(fullPath)) {
        client.printf("\033[1;31mDiretório não encontrado: %s\033[0m\r\n", fullPath.c_str());
        return;
    }

    if (!isDirectory(fullPath)) {
        client.printf("\033[1;31m%s não é um diretório\033[0m\r\n", fullPath.c_str());
        return;
    }

    currentDirectory = fullPath;
    client.printf("\033[1;32mDiretório atual: %s\033[0m\r\n", currentDirectory.c_str());
    updatePrompt(*activeServer);
}

// Implementação do autocompletar com TAB
void TelnetCommands::handleTabCompletion(WiFiClient &client, const String &partialCmd) {
    if (!activeServer)
        return;
    if (partialCmd.isEmpty())
        return;

    // Divide o comando em partes
    int lastSpace = partialCmd.lastIndexOf(' ');
    String path = currentDirectory;
    String pattern;
    String prefix;

    if (lastSpace == -1) {
        // Completa comandos
        std::vector<String> matches;
        for (const auto &cmd : {"help", "ls", "cd", "pwd", "clear"}) {
            if (strncmp(cmd, partialCmd.c_str(), partialCmd.length()) == 0) {
                matches.push_back(cmd);
            }
        }

        if (matches.size() == 1) {
            client.print(matches[0].substring(partialCmd.length()));
        } else if (matches.size() > 1) {
            client.print("\r\n");
            for (const auto &m : matches) {
                client.printf("%s ", m.c_str());
            }
            client.print("\r\n" + currentDirectory + "> " + partialCmd);
        }
        return;
    }

    // Completa arquivos/diretórios
    prefix = partialCmd.substring(0, lastSpace + 1);
    pattern = partialCmd.substring(lastSpace + 1);

    // Se o padrão contém caminho, ajusta
    int lastSlash = pattern.lastIndexOf('/');
    if (lastSlash != -1) {
        path = resolvePath(pattern.substring(0, lastSlash));
        pattern = pattern.substring(lastSlash + 1);
    }

    if (!LittleFS.exists(path))
        return;

    File dir = LittleFS.open(path);
    File file = dir.openNextFile();
    std::vector<String> matches;

    while (file) {
        String fileName = file.name();
        if (fileName.startsWith(path)) {
            fileName = fileName.substring(path.length() + (path.endsWith("/") ? 0 : 1));
        }

        if (fileName.startsWith(pattern)) {
            matches.push_back(fileName);
        }
        file = dir.openNextFile();
    }

    if (matches.empty())
        return;

    if (matches.size() == 1) {
        client.print(matches[0].substring(pattern.length()));
    } else {
        String commonPrefix = findCommonPrefix(matches);
        if (commonPrefix.length() > pattern.length()) {
            client.print(commonPrefix.substring(pattern.length()));
        } else {
            client.print("\r\n");
            for (const auto &m : matches) {
                sendColoredEntry(client, m, isDirectory(resolvePath(path + "/" + m)));
            }
            client.print("\r\n" + currentDirectory + "> " + partialCmd);
        }
    }
}

// Implementação do pwd
void TelnetCommands::handlePrintWorkingDir(WiFiClient &client, const String &) {
    client.printf("Diretório atual: %s\r\n", currentDirectory.c_str());
}

// Métodos auxiliares
String TelnetCommands::resolvePath(const String &path) {
    if (path.startsWith("/"))
        return path;

    String cleanPath = path;
    cleanPath.trim();

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

bool TelnetCommands::matchesWildcard(const String &name, const String &pattern) {
    if (pattern == "*")
        return true;

    // Implementação simples de wildcards (pode ser melhorada)
    int patternPos = 0;
    int namePos = 0;
    int patternLen = pattern.length();
    int nameLen = name.length();

    while (patternPos < patternLen && namePos < nameLen) {
        if (pattern[patternPos] == '*') {
            // Avança até encontrar o próximo caractere do padrão
            patternPos++;
            if (patternPos >= patternLen)
                return true;

            while (namePos < nameLen) {
                if (name[namePos] == pattern[patternPos]) {
                    break;
                }
                namePos++;
            }
            if (namePos >= nameLen)
                return false;
        } else if (pattern[patternPos] == '?' || pattern[patternPos] == name[namePos]) {
            patternPos++;
            namePos++;
        } else {
            return false;
        }
    }

    return (patternPos == patternLen) && (namePos == nameLen);
}

void TelnetCommands::sendColoredEntry(WiFiClient &client, const String &name, bool isDir) {
    if (isDir) {
        client.printf("\033[1;34m[DIR]  %-30s\033[0m\r\n", name.c_str());
    } else {
        // Diferentes cores para diferentes extensões
        if (name.endsWith(".txt") || name.endsWith(".md")) {
            client.printf("\033[1;32m%-30s\033[0m\r\n", name.c_str());
        } else if (name.endsWith(".cpp") || name.endsWith(".h") || name.endsWith(".ino")) {
            client.printf("\033[1;33m%-30s\033[0m\r\n", name.c_str());
        } else {
            client.printf("%-30s\r\n", name.c_str());
        }
    }
}

String TelnetCommands::findCommonPrefix(const std::vector<String> &matches) {
    if (matches.empty())
        return "";

    String prefix = matches[0];
    for (size_t i = 1; i < matches.size(); i++) {
        const String &current = matches[i];
        size_t j = 0;
        while (j < prefix.length() && j < current.length() && prefix[j] == current[j]) {
            j++;
        }
        prefix = prefix.substring(0, j);
        if (prefix.isEmpty())
            break;
    }
    return prefix;
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
    // Atualiza o prompt com o diretório atual (pode ser chamado após mudar de diretório)
    server.setPrompt("\033[1;34m" + currentDirectory + "> \033[0m");
}