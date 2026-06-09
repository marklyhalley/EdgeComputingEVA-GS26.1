# 🚀 E.V.A. — Dragon Capsule Telemetry System
### Edge Computing & Computer Systems · Global Solution 2026 · FIAP

---

## 📡 Descrição da Solução

Sistema de telemetria embarcada para monitoramento ambiental contínuo da cápsula **SpaceX Dragon** durante o transporte dos consórcios fúngicos do projeto **E.V.A. (Ecosystem Vitality Assistant)** da Terra até Marte.

Antes que qualquer fungo possa regenerar o solo marciano, ele precisa sobreviver à viagem de aproximadamente 7 meses. Este sistema garante isso: um **ESP32** coleta dados de 6 sensores a cada 3 segundos, avalia os parâmetros localmente e publica os dados via **MQTT sobre TLS** no broker **HiveMQ Cloud**. Quando qualquer parâmetro sai da faixa segura, o sistema aciona alertas visuais (LEDs) e sonoros (buzzer) **imediatamente, sem depender de conexão externa**.

O **Node-RED** recebe as mensagens do broker, processa os fluxos de telemetria e exibe os dados em um dashboard dedicado para monitoramento da equipe em Terra.

---

## 🔗 Links

| | |
|---|---|
| **Simulação Wokwi** | https://wokwi.com/projects/465809120774624257 |
| **Repositório GitHub** | https://github.com/marklyhalley/EdgeComputingEVA-GS26.1 |
| **Broker MQTT** | efedf4b853574cef8100661792bd568a.s1.eu.hivemq.cloud |

---

## 🏗️ Arquitetura da Solução

```
┌─────────────────────────────────────────────────────────────────┐
│                     DRAGON CAPSULE (Wokwi)                      │
│                                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌───────────────┐  │
│  │  DHT22   │  │  BMP180  │  │ MPU-6050 │  │  Pot O₂ / Rad │  │
│  │GPIO 15   │  │  I2C     │  │  I2C     │  │  GPIO 34 / 35 │  │
│  │Temp+Umid │  │ Pressão  │  │Aceleração│  │  O₂ / Radiação│  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬────────┘  │
│       └─────────────┴─────────────┴────────────────┘           │
│                            ESP32                                 │
│                    (Processamento Local)                         │
│                    Avalia limiares                               │
│                    LEDs + Buzzer                                  │
│                    GPIO 25/26/27/14                              │
└───────────────────────────┬─────────────────────────────────────┘
                            │ MQTT over TLS (porta 8883)
                            ▼
                  ┌──────────────────┐
                  │  HiveMQ Cloud    │
                  │  (Broker MQTT)   │
                  └────────┬─────────┘
                           │
                  ┌────────▼─────────┐
                  │    Node-RED      │
                  │  (Processamento) │
                  └────────┬─────────┘
                           │
                  ┌────────▼─────────┐
                  │ Node-RED         │
                  │ Dashboard        │
                  └──────────────────┘
```

---

## 🔧 Hardware — Componentes

| Componente | Interface | GPIO | Parâmetro Medido | Faixa |
|---|---|---|---|---|
| **DHT22** | Digital | GPIO 15 | Temperatura | -40 a 80 °C |
| **DHT22** | Digital | GPIO 15 | Umidade Relativa | 0 a 100 % |
| **BMP180** | I2C (0x77) | SDA/SCL | Pressão Atmosférica | 300 a 1100 hPa |
| **MPU-6050** | I2C (0x68) | SDA/SCL | Aceleração (3 eixos) | ±8 G |
| **Potenciômetro O₂** | Analógico | GPIO 34 | Concentração de O₂ | 14 a 28 % |
| **Potenciômetro Rad** | Analógico | GPIO 35 | Radiação Ionizante | 0 a 30 µSv/h |
| **LED Verde** | Digital | GPIO 25 | Status NOMINAL | — |
| **LED Amarelo** | Digital | GPIO 26 | Status ATENÇÃO | — |
| **LED Vermelho** | Digital | GPIO 27 | Status CRÍTICO | — |
| **Buzzer** | Digital | GPIO 14 | Alerta Sonoro (CRÍTICO) | — |

> Os GPIOs 34 e 35 são input-only no ESP32. A leitura ADC usa média de 10 amostras para suavizar ruído.

---

## 📶 Configuração MQTT

| Parâmetro | Valor |
|---|---|
| **Broker** | `efedf4b853574cef8100661792bd568a.s1.eu.hivemq.cloud` |
| **Porta** | `8883` (MQTT over TLS) |
| **Usuário** | `dragon_user` |
| **Senha** | `Dragon@1234` |
| **Protocolo** | MQTT v5 |
| **QoS** | 1 |
| **Intervalo** | 3 segundos |
| **Client ID** | `DragonESP32-[hex único]` |

### Tópicos publicados

| Tópico | Conteúdo | Exemplo de Payload |
|---|---|---|
| `dragon/telemetry/temperatura` | Temperatura do ambiente | `{"temperatura": 22.50}` |
| `dragon/telemetry/umidade` | Umidade relativa | `{"umidade": 48.0}` |
| `dragon/telemetry/pressao` | Pressão atmosférica | `{"pressao": 1013.25}` |
| `dragon/telemetry/oxigenio` | Concentração de O₂ | `{"oxigenio": 21.00}` |
| `dragon/telemetry/radiacao` | Radiação ionizante | `{"radiacao": 4.80}` |
| `dragon/telemetry/aceleracao` | Aceleração (3 eixos + magnitude) | `{"accel_x_g": 0.01, ...}` |
| `dragon/telemetry/status` | Nível de alerta atual | `{"nivel": 0, "descricao": "OK"}` |
| `dragon/telemetry/full` | **Payload completo** (todos os dados) | ver abaixo |

### Payload completo — `dragon/telemetry/full`

```json
{
  "device_id": "dragon-capsule-001",
  "mission": "SpaceX-Dragon-ISS",
  "ts_ms": 45231,
  "environment": {
    "temperatura_c": 22.50,
    "umidade_pct": 48.0,
    "pressao_hpa": 1013.25
  },
  "life_support": {
    "oxigenio_pct": 21.00,
    "radiacao_usvh": 4.80
  },
  "motion": {
    "accel_x_g": 0.010,
    "accel_y_g": 0.000,
    "accel_z_g": 1.000,
    "magnitude_g": 1.002
  },
  "status": {
    "nivel": 0,
    "descricao": "OK"
  }
}
```

---

## 🚨 Sistema de Alertas

O ESP32 avalia os parâmetros localmente a cada leitura — a decisão de alerta não depende de conexão com o broker.

| Parâmetro | NOMINAL | ATENÇÃO | CRÍTICO |
|---|---|---|---|
| Temperatura | 17–27 °C | 10–17 °C ou 27–35 °C | < 10 °C ou > 35 °C |
| Umidade | 25–70 % | < 25 % ou > 70 % | — |
| Pressão | 980–1040 hPa | 950–980 ou 1040–1070 hPa | < 950 ou > 1070 hPa |
| Oxigênio | 18–24 % | 16–18 % ou 24–26 % | < 16 % ou > 26 % |
| Radiação | < 5 µSv/h | 5–20 µSv/h | > 20 µSv/h |
| Aceleração | < 2.5 G | 2.5–4.5 G | > 4.5 G |

**Resposta por nível:**

| Nível | LED | Buzzer | Publicação MQTT `status` |
|---|---|---|---|
| 0 — NOMINAL | 🟢 Verde | OFF | `{"nivel": 0, "descricao": "OK"}` |
| 1 — ATENÇÃO | 🟡 Amarelo | OFF | `{"nivel": 1, "descricao": "WARNING"}` |
| 2 — CRÍTICO | 🔴 Vermelho | ON | `{"nivel": 2, "descricao": "CRITICAL"}` |

---

## 📚 Bibliotecas Utilizadas

```
DHT sensor library
Adafruit BMP085 Library
Adafruit MPU6050
Adafruit Unified Sensor
ArduinoJson
PubSubClient
```

Todas disponíveis no Library Manager do Wokwi via `libraries.txt`.

---

## ▶️ Como Executar a Simulação

1. Acesse o projeto no Wokwi: https://wokwi.com/projects/465809120774624257
2. Clique em **▶ Start Simulation**
3. O ESP32 conectará automaticamente ao WiFi (rede `Wokwi-GUEST`)
4. Em seguida, conectará ao broker HiveMQ Cloud via MQTT/TLS
5. O Serial Monitor exibirá as leituras a cada 3 segundos
6. **Interagindo com os sensores:**
   - Gire o **Potenciômetro O₂** (GPIO 34) para simular variação de oxigênio
   - Gire o **Potenciômetro Radiação** (GPIO 35) para simular picos de radiação
   - Observe os LEDs mudando conforme os limiares são ultrapassados
7. Os dados chegam ao broker e podem ser monitorados no **Node-RED Dashboard**

---

## 🔄 Funcionamento do Firmware

```
setup()
  ├── Serial.begin(115200)
  ├── Configura pinos dos LEDs e buzzer
  ├── Inicializa DHT22, BMP180 e MPU6050
  │     └── Fallback para simulação se sensor não encontrado
  ├── Conecta ao WiFi (Wokwi-GUEST)
  ├── Configura MQTT (setInsecure + setBufferSize 1024)
  └── Primeira tentativa de conexão ao broker

loop() — a cada 3 segundos
  ├── Mantém conexão MQTT ativa (client.loop())
  ├── Reconexão não-bloqueante se desconectado
  ├── Lê DHT22 (temperatura + umidade)
  ├── Lê BMP180 (pressão) ou usa onda seno como fallback
  ├── Lê MPU6050 (aceleração XYZ) ou usa valores fixos
  ├── Lê potenciômetros (O₂ e radiação) — média 10 amostras ADC
  ├── Avalia nível de alerta (0/1/2) para todos os parâmetros
  ├── Atualiza LEDs e buzzer conforme nível
  ├── Publica 7 tópicos individuais + 1 payload completo (TOPIC_FULL)
  └── Imprime resumo no Serial Monitor
```

---

## 👥 Integrantes

| Nome | RM |
|---|---|
| Camile Vitória Silva | RM 566649 |
| Gustavo Almeida Ferreira | RM 566980 |
| Lucas de Oliveira Miranda Caetano | RM 568036 |
| Marco Túlio Longo Conte | RM 568373 |
| Sofia Souza Rodrigues | RM 566708 |

**Turma:** 1ESPS · **FIAP · Engenharia de Software · 1º Ano · 2026**
