# WiFiLib

Biblioteca para gerenciamento de conexões WiFi em dispositivos ESP32/ESP8266 com suporte a configurações estáticas e DHCP.  

![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange?style=plastic&logo=platformio)  
![Licença](https://img.shields.io/badge/licen%C3%A7a-Apache%202.0-blue.svg?style=plastic&logo=apache)  
![Versão](https://img.shields.io/badge/Vers%C3%A3o-1.0.0-green.svg?style=plastic&logo=github)  


## 📦 Recursos Principais
- ✅ Conexão WiFi com suporte a STA (station) mode

- ⚙️ Configuração via JSON armazenada em LittleFS/SPIFFS

- 🔌 Suporte a IP estático e DHCP

- 📊 Sistema de log configurável

- 🛠 Padrão Singleton para acesso global

- 🔄 Tratamento completo de eventos WiFi

- 📶 Monitoramento de força do sinal

- 🔄 Reconexão automática (em desenvolvimento)

## 🚀 Instalação
### Via PlatformIO
Adicione no seu platformio.ini:

```ini
Copy
lib_deps =
    https://github.com/seu-usuario/MiniOsWiFiLib.git
```
### Via Arduino IDE
Baixe o ZIP da biblioteca

Em Sketch > Include Library > Add .ZIP Library... selecione o arquivo baixado

## Dependências
- ArduinoJson (v6.18.0 ou superior)

- LittleFS (para ESP8266) / LittleFS para ESP32

- LogLibrary (opcional)

## 🛠 Configuração Inicial
Crie um arquivo wifi_config.json no sistema de arquivos LittleFS/SPIFFS:

```json
{
    "ssid": "sua_rede_wifi",
    "password": "sua_senha",
    "dhcp": false,
    "ip": [192, 168, 1, 100],
    "gateway": [192, 168, 1, 1],
    "subnet": [255, 255, 255, 0]
}
```
Use o seguinte código para carregar e iniciar:

```cpp
#include <WiFiLib.h>

void setup() {
    WiFiLib& wifi = WiFiLib::getInstance();
    WiFiItems config = wifi.loadConfig("/wifi_config.json");
    
    if(wifi.isConfigLoaded()) {
        wifi.begin(config, WiFiLib::WiFiLog::ENABLE);
    } else {
        // Tratar erro de carregamento
    }
}

void loop() {
    // Seu código principal
}
```

## 📚 Documentação da API
### Métodos Principais
|Método	|Descrição|
|---|---|
|begin(WiFiItems wifi)	|Inicia conexão WiFi com configurações padrão|
|begin(WiFiItems wifi, WiFiLog log)	|Inicia conexão com controle de log (ENABLE/DISABLE)|
|loadConfig(std::string path)	|Carrega configurações do arquivo JSON especificado|

### Estrutura WiFiItems
|Campo	|Tipo	|Descrição|
|---|---|---|
|ssid	|std::string	|Nome da rede WiFi|
|password	|std::string	|Senha da rede|
|dhcpFlag	|bool	|Usar DHCP (true) ou IP estático (false)|
|ip	|std::vector<uint8_t>	|Endereço IP (4 bytes)|
|gateway	|std::vector<uint8_t>	|Gateway (4 bytes)|
|subnet	|std::vector<uint8_t>	|Máscara de sub-rede (4 bytes)|
|configLoaded	|bool	|Flag de configuração carregada|
|connectionStatus	|int	|Status atual da conexão|
|power	|int	|Força do sinal em dBm|

### Métodos de Verificação
|Método	|Retorno	|Descrição|
|---|---|---|
|isConfigLoaded()	|bool	|Verifica se a configuração foi carregada|
|isDhcp()	|bool	|Verifica se está usando DHCP|
|isSsid()	|bool	|Verifica se SSID foi configurado|

## 💡 Exemplos
### Exemplo Básico
```cpp
#include <WiFiLib.h>

void setup() {
    Serial.begin(115200);
    
    WiFiLib& wifi = WiFiLib::getInstance();
    WiFiItems config = wifi.loadConfig("/wifi_config.json");
    
    if(wifi.isConfigLoaded()) {
        wifi.begin(config, WiFiLib::WiFiLog::ENABLE);
        
        while(WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        
        Serial.println("\nConectado!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
}

void loop() {}
```
### Exemplo com Tratamento de Eventos
```cpp
#include <WiFiLib.h>

void WiFiEvent(WiFiEvent_t event) {
    switch(event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Conectado ao AP!");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("Desconectado. Tentando reconectar...");
            break;
    }
}

void setup() {
    Serial.begin(115200);
    
    WiFiLib& wifi = WiFiLib::getInstance();
    WiFi.onEvent(WiFiEvent);
    
    WiFiItems config = wifi.loadConfig("/wifi_config.json");
    wifi.begin(config);
}

void loop() {}
```
## 🐛 Solução de Problemas
### Problemas Comuns
1. Falha ao carregar configuração

  - Verifique se o arquivo JSON existe no sistema de arquivos

  - Valide a sintaxe do JSON

  - Confira as permissões do arquivo

2. Falha na conexão WiFi

  - Verifique SSID e senha

  - Confira se a rede está disponível

  - No modo IP estático, valide os endereços

3. Problemas com LittleFS

  - Certifique-se de que o sistema de arquivos foi inicializado

```cpp

if(!LittleFS.begin()) {
    Serial.println("Falha ao montar LittleFS");
    return;
}
```
## 🤝 Contribuição
Contribuições são bem-vindas! Siga estes passos:

1. Faça um fork do projeto

2. Crie sua branch (git checkout -b feature/nova-feature)

3. Commit suas mudanças (git commit -m 'Adiciona nova feature')

4. Push para a branch (git push origin feature/nova-feature)

5. Abra um Pull Request

## 📜 Licença
Copyright 2025 cturqueti

Licenciado sob a Apache License, Versão 2.0 (a "Licença");
você não pode usar este arquivo exceto em conformidade com a Licença.
Você pode obter uma cópia da Licença em:

http://www.apache.org/licenses/LICENSE-2.0

A menos que exigido por lei aplicável ou acordado por escrito, o software
distribuído sob a Licença é distribuído "COMO ESTÁ",
SEM GARANTIAS OU CONDIÇÕES DE QUALQUER TIPO, expressas ou implícitas.
Consulte a Licença para o idioma específico que rege as permissões e
limitações sob a Licença.

Consulte o arquivo [LICENSE](LICENSE) para o texto completo da licença e
[NOTICE](NOTICE) para informações sobre atribuições e histórico de modificações.

## ✉️ Contato
Carlos Augusto D'Orazio Turqueti - @cturqueti - carlosturqueti@gmail.com

Link do Projeto: https://github.com/cturqueti/MiniOsWiFiLib