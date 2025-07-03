# Sistema de Lançamento de Paraquedas ESP32

## Descrição
Sistema de controle para lançamento de paraquedas baseado em ESP32 com sensores BMP280 e MPU6050. O sistema oferece interface web para monitoramento e controle.

## Funcionalidades

### 📊 Telemetria
- Coleta de dados de altitude (BMP280)
- Coleta de dados de aceleração (MPU6050)
- Armazenamento em arquivos CSV
- Monitoramento em tempo real

### 🚀 Sistema de Lançamento
- **Lançamento Manual**: Ativação imediata via interface web
- **Lançamento por Altitude**: Configurável para ejeção acima ou abaixo de altitude específica
- Indicação visual via LED
- Monitoramento contínuo de condições

### 🌐 Interface Web
- Controle via WiFi (Access Point)
- Interface responsiva para mobile
- Três abas: Telemetria, Lançamento, Downloads
- Download de arquivos CSV coletados

## Hardware Necessário

- **ESP32** (qualquer modelo)
- **BMP280** - Sensor de pressão/altitude (I2C: 0x76)
- **MPU6050** - Sensor de aceleração/giroscópio (I2C: 0x68)
- **LED** - Conectado ao pino 32
- **Mecanismo de ejeção** (a ser implementado)

## Conexões

### I2C (sensores)
- **SDA**: GPIO 21
- **SCL**: GPIO 22
- **VCC**: 3.3V
- **GND**: GND

### LED
- **Pino 32**: LED + resistor (220Ω) para GND

## Configuração

### WiFi Access Point
- **SSID**: `PRD_coleta_paraquedas`
- **Senha**: `1234567890`
- **IP**: 192.168.4.1

### Uso
1. Conecte-se ao WiFi do ESP32
2. Acesse `http://192.168.4.1` no navegador
3. Configure parâmetros de lançamento na aba "Lançamento"
4. Inicie coleta de dados na aba "Telemetria"
5. Baixe dados coletados na aba "Downloads"

## Recursos Técnicos

- **Multitasking**: Utiliza FreeRTOS para processamento paralelo
- **Armazenamento**: Sistema de arquivos SPIFFS
- **Memória**: Preferences para persistência de dados
- **Sensores**: Configuração otimizada com filtros digitais

## Autor
Wilson Simonal

## Licença
Projeto desenvolvido para fins educacionais e de pesquisa.
