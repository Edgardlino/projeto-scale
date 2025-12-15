# ⚖️ Balança IoT Inteligente (ESP32 + BLE)

> **Balança de precisão conectada via Web Bluetooth (PWA) com integração direta ao Google Sheets.**

![Status](https://img.shields.io/badge/Status-v1.0%20Estável-success)
![Power](https://img.shields.io/badge/Bateria-Deep%20Sleep-green)
![Tech](https://img.shields.io/badge/Interface-No%20App%20Required-blue)

<p align="center">
  <img src="https://via.placeholder.com/600x300?text=Foto+do+Projeto+Aqui" alt="Foto da Balança">
</p>

## ✨ Destaques
* **Zero Instalação:** Funciona direto no navegador (Chrome/Android/PC) via tecnologia PWA.
* **Bateria de Longa Duração:** Sistema *Smart Power* com Deep Sleep.
* **Nuvem:** Salva histórico de pesagens automaticamente no Google Sheets.
* **Alta Precisão:** Filtros digitais adaptativos para estabilização rápida (10Hz).

---

## 🚀 Como Usar

### 1. Ligar
O sistema possui proteção contra acionamento acidental.
* **Ligar:** Segure o botão por **3 segundos** até o LED acender.
* **Desligar:** Segure por **3 segundos** até o LED piscar (ou aguarde desligamento automático).

### 2. Conectar
1.  Acesse o Web App: **[https://edgardlino.github.io/projeto-scale/]**
2.  Clique em `🔗 CONECTAR BALANÇA`.
3.  O peso aparecerá em tempo real.

---

## 🛠️ Hardware Necessário

* **MCU:** ESP32-C6 SuperMini (Foco em baixo consumo).
* **Sensor:** Célula de Carga + Módulo HX711.
* **Alimentação:** Bateria LiPo 3.7V.

---

## 📚 Documentação Técnica

Quer saber como montamos o hardware, o esquema de ligação, a lógica dos filtros digitais ou como configurar seu próprio Google Sheets?

👉 **[CLIQUE AQUI PARA VER A DOCUMENTAÇÃO COMPLETA (DOCS.md)](DOCS.md)**

---

## 📦 Download & Instalação

Baixe a última versão do firmware na aba **[Releases](../../releases)**.

1.  Configure a IDE do Arduino para **ESP32-C6**.
2.  Instale as bibliotecas `HX711` e `ESP32 BLE Arduino`.
3.  Carregue o arquivo `firmware.ino`.

---

## 📄 Licença
Este projeto é Open Source. Sinta-se livre para contribuir!
