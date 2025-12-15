# ⚖️ Projeto Scale IoT - Balança Inteligente BLE

Uma balança de precisão conectada, de baixo consumo energético, que opera via **Web Bluetooth** diretamente no navegador e armazena histórico de pesagens no **Google Sheets**.

![Status do Projeto](https://img.shields.io/badge/Status-Concluído-success)
![Hardware](https://img.shields.io/badge/Hardware-ESP32--C6-blueviolet)
![Interface](https://img.shields.io/badge/Interface-PWA%20%2F%20Web%20Bluetooth-blue)

---

## 📖 1. Glossário e Siglas

Para facilitar o entendimento da documentação, abaixo estão listadas as siglas e termos técnicos utilizados neste projeto:

1.  **IoT (Internet of Things):** Internet das Coisas. Rede de objetos físicos incorporados com sensores e software para trocar dados.
2.  **BLE (Bluetooth Low Energy):** Tecnologia de Bluetooth de baixa energia, ideal para dispositivos a bateria que enviam pequenas quantidades de dados.
3.  **GATT (Generic Attribute Profile):** Protocolo usado pelo BLE para definir como os dados são organizados e transferidos (Serviços e Características).
4.  **ESP32:** Família de microcontroladores de baixo custo e baixo consumo com Wi-Fi e Bluetooth integrados.
5.  **ADC (Analog-to-Digital Converter):** Conversor Analógico-Digital. Usado aqui para ler a voltagem da bateria.
6.  **GPIO (General Purpose Input/Output):** Pinos de entrada e saída de uso geral no microcontrolador.
7.  **PWA (Progressive Web App):** Aplicação web que se comporta como um aplicativo nativo no celular (pode ser instalada, funciona offline/bluetooth).
8.  **OTA (Over-The-Air):** Método de distribuição de novas atualizações de software/firmware sem a necessidade de cabos (via Wi-Fi). *Nota: O projeto foca em BLE, mas o hardware suporta OTA.*
9.  **NVS (Non-Volatile Storage):** Memória não volátil do ESP32 usada para salvar o fator de calibração mesmo sem energia.

---

## 🛠️ 2. Hardware e Esquema de Ligação

O projeto utiliza o **ESP32-C6 SuperMini** por sua eficiência energética e o **HX711** para amplificação de sinal.

### Destaques Técnicos do HX711 (Baseado no Datasheet)
* **Amplificador de Baixo Ruído (PGA):** Utilizamos o Canal A com ganho de **128**. Isso permite ler variações de tensão minúsculas ($\pm20mV$ Full Scale) provenientes da célula de carga.
* **Taxa de Amostragem (Data Rate):** Configurado para **10Hz**. Embora mais lento que 80Hz, o datasheet confirma que este modo reduz o ruído de entrada de 90nV(Nanovolt) para **50nV**, essencial para precisão.
* **Settling Time:** O conversor precisa de **400ms** para estabilizar os dados após ligar. O firmware trata isso com um delay inicial de segurança.

### Lista de Componentes (BOM)
* 1x Placa de Desenvolvimento ESP32-C6 SuperMini.
* 1x Célula de Carga (capacidade conforme necessidade, ex: 5kg, 20kg).
* 1x Módulo Amplificador HX711 (Configurado para **10Hz** para maior estabilidade).
* 1x Bateria LiPo 3.7V/4.2V.
* 2x Resistores de 100kΩ (para o divisor de tensão da bateria).
* 1x Botão Táctil (Push Button) para Ligar/Desligar.

### Diagrama de Pinos (Pinout)

A ligação foi projetada para garantir segurança no Deep Sleep e evitar travamentos nos pinos de boot.

| Componente | Pino do Componente | Pino ESP32 (GPIO) | Função |
| :--- | :--- | :--- | :--- |
| **HX711** | DT (Data) | **GPIO 6** | Comunicação de dados do sensor |
| **HX711** | SCK (Clock) | **GPIO 7** | Clock de comunicação |
| **HX711** | VCC | **GPIO 3** | Alimentação controlada (Economia de energia) |
| **HX711** | GND | GND | Aterramento |
| **Bateria** | Positivo (+) | **BAT** (Pad) | Alimentação da placa |
| **Monitor Bat** | Divisor (Meio) | **GPIO 2** | Leitura de nível de bateria (ADC) |
| **Botão Power** | Perna 1 | **GPIO 0** | Ligar/Desligar (Hold 3s) |
| **LED Status** | Anodo (+) | **GPIO 15** | Indicador de conexão BLE |

> **Nota sobre a Bateria:** O divisor de tensão (2 resistores de 100kΩ) conecta o positivo da bateria ao GND. O ponto central (entre os resistores) vai ao **GPIO 2**.
> **Nota sobre o Botão:** A outra perna do botão deve ser ligada ao **GND**.

---

## ⚙️ 3. Funcionalidades do Firmware

O código foi desenvolvido em C++ (Arduino IDE) com foco em **eficiência energética extrema**.

* **Smart Power Control (Botão Seguro):**
    * **Ligar:** Segure o botão por 3 segundos até o LED acender. (Evita ligar acidentalmente com toques rápidos).
    * **Desligar:** Segure o botão por 3 segundos até o LED piscar.
* **Deep Sleep Inteligente:**
    * **Inatividade:** Se não conectar em 60s, desliga automaticamente.
    * **Desconexão:** Se perder o Bluetooth, aguarda reconexão brevemente e desliga.
* **Gestão de Energia do Sensor:** O pino VCC do HX711 é alimentado pelo **GPIO 3**. Durante o sono profundo, o ESP32 corta a energia do sensor, zerando o consumo da célula de carga.
* **Interface BLE:** Atua como servidor GATT, enviando dados e recebendo comandos (`TARE`, `CAL`).

---

## 🧠 4. Lógica dos Filtros Digitais

Um dos maiores desafios em balanças IoT é a instabilidade das leituras (ruído elétrico) e o "drift" (variação lenta). Este projeto implementa dois algoritmos distintos para garantir leituras sólidas:

### 1. Filtro Adaptativo de Peso (Smart Smoothing)
Em vez de usar uma média fixa, o firmware analisa a **taxa de variação** do peso em tempo real para decidir como filtrar:

* **Cenário A (Mudança Brusca):** Se a diferença entre a leitura atual e a anterior for **> 1.0g** (ex: colocou um objeto), o filtro abre (Alpha 0.7).
    * *Resultado:* Resposta instantânea na tela.
* **Cenário B (Estabilidade):** Se a variação for pequena (ex: ruído do sensor ou vento), o filtro fecha drasticamente (Alpha 0.05).
    * *Resultado:* O número "trava" na tela e ignora oscilações, eliminando o efeito de "ficar trocando o último dígito".

### 2. Filtro Passa-Baixa na Bateria
A leitura do ADC do ESP32 é naturalmente ruidosa. Para evitar que a porcentagem da bateria fique pulando (ex: 85% ↔ 84%), aplicamos um filtro de **Média Móvel Exponencial (EMA)**.

* **Fórmula:** `Valor_Final = (Leitura_Nova * 0.05) + (Valor_Antigo * 0.95)`
* Isso significa que cada nova leitura afeta apenas 5% do resultado final, criando uma "inércia" que estabiliza a visualização da carga.

---

## 💻 5. Interface Web (Front-End)

A interface é uma página HTML única hospedada no **GitHub Pages**. Ela utiliza a **Web Bluetooth API** para conectar diretamente ao ESP32 sem instalar aplicativos.

### Recursos:
* Visualização de peso em tempo real.
* Indicador de bateria e status de carregamento (ícone ⚡ acima de 4.18V).
* Botão de **Tara** (Zerar).
* Menu de **Calibração** (Salva na memória NVS do ESP32).
* **Nuvem:** Botão para salvar a pesagem atual no Google Sheets.
* **Histórico:** Modal que busca e exibe as últimas 15 pesagens salvas.

---

## 🚀 6. Como Configurar

### Passo A: Google Sheets (Back-End)
1.  Crie uma nova planilha no Google Sheets.
2.  Vá em `Extensões` > `Apps Script`.
3.  Cole o código do script (`doGet`) fornecido no projeto.
4.  Clique em **Implantar** > **Nova implantação**.
5.  **Tipo:** App da Web.
6.  **Acesso:** "Qualquer pessoa" (Anyone).
7.  Copie a URL gerada (`.../exec`).

### Passo B: Front-End
1.  No arquivo `index.html`, localize a constante:
    `const GOOGLE_SCRIPT_URL = "SUA_URL_DO_GOOGLE_APPS_SCRIPT_AQUI";`
2.  Cole a URL gerada no Passo A.
3.  Suba o arquivo para o GitHub e ative o **GitHub Pages** nas configurações do repositório.

### Passo C: Firmware
1.  Instale a biblioteca `HX711` e `ESP32 BLE Arduino`.
2.  Selecione a placa `ESP32C6 Dev Module` (Board Manager v3.0+).
3.  **Importante:** Desative a opção *USB CDC On Boot* para economizar bateria e acelerar o boot.
4.  Carregue o código no ESP32.

---

## 📱 7. Como Usar

1.  **Ligar:** Segure o botão Power (Pino 0) por **3 segundos**. O LED acenderá fixamente.
2.  Abra o site (GitHub Pages) no seu celular (Chrome/Android ou Bluefy/iOS).
3.  Clique em **🔗 CONECTAR BALANÇA**.
4.  Selecione **"Projeto Scale"** na lista.
5.  O peso aparecerá na tela.
6.  Para salvar, clique em **☁️ SALVAR NA NUVEM**.
7.  Para ver os dados anteriores, clique em **📜 VER HISTÓRICO**.
8.  **Desligar:** Segure o botão Power por 3 segundos novamente (ou aguarde o tempo limite).

---

## ❓ 8. Solução de Problemas (Troubleshooting)

### A balança não liga / LED não acende
* Certifique-se de **segurar o botão por 3 segundos**. Toques rápidos são ignorados propositalmente para evitar acionamento acidental.

### A balança não aparece na lista de Bluetooth
* **Bateria/Sono:** O dispositivo entra em *Deep Sleep* após 60s sem conexão. Ligue-o novamente.
* **Navegador:** Certifique-se de usar **Google Chrome** (Android/PC) ou **Bluefy** (iOS). O Safari padrão não suporta Web Bluetooth.
* **Permissões:** No Android, é **obrigatório** ativar a **Localização (GPS)** para escanear dispositivos Bluetooth (exigência do sistema operacional).

### O peso fica oscilando ou "caindo" sozinho (Drift)
* **Configuração RATE (HX711):** Verifique se o pino 15 (RATE) do chip HX711 está aterrado (GND). Em módulos comerciais, isso é o padrão. Se o módulo foi modificado para 80Hz (pino levantado ou ligado ao VCC), a leitura será instável.
* **Acomodação:** O código inclui um atraso de 500ms no início para respeitar o "Settling Time" de 400ms exigido pelo datasheet em modo 10Hz. Não remova este delay.

### Erro ao "Salvar na Nuvem"
* Verifique se o **Google Apps Script** foi implantado com permissão de acesso para **"Qualquer pessoa" (Anyone)**.
* Se você editou o script, lembre-se de criar uma **"Nova Versão"** na hora de implantar.
* Confirme se o link gerado (`.../exec`) foi copiado corretamente para a constante `GOOGLE_SCRIPT_URL` no arquivo `index.html`.

## * Nota: Em alguns módulos genéricos, pode ser necessário soldar o pino E- ao GND para corrigir flutuações, conforme erro de design conhecido.

---

## 🎛️ 9. Como Ajustar os Filtros (Ajuste Fino)

Se você sentir que a balança está lenta demais ou sensível demais, você pode ajustar os parâmetros diretamente no código `firmware.ino`.

### Ajuste de Responsividade do Peso
Procure a função `void loop()` e localize a lógica do **Filtro Adaptativo**:

    // Cenário: Mudança Brusca (Colocou peso)
    // Aumente para 0.8 ou 0.9 para ficar MAIS RÁPIDO (mas menos estável)
    // Diminua para 0.5 para ficar MAIS LENTO
    dynamicAlpha = 0.7; 

    // Cenário: Estabilidade (Repouso)
    // Diminua para 0.02 para travar mais o número (menos oscilação)
    // Aumente para 0.1 se o peso estiver demorando para estabilizar o último dígito
    dynamicAlpha = 0.05; 


### Ajuste da Bateria
Procure a função `void lerBateria()`:

    // O valor 0.05 (5%) define a velocidade da atualização.
    // Se a bateria estiver mudando muito rápido na tela, diminua para 0.01.
    // Se estiver demorando muito para atualizar, aumente para 0.10.
    smoothBatRaw = (raw * 0.05) + (smoothBatRaw * 0.95);

---

## 💡 10. Perguntas Frequentes (FAQ)

### Por que o HX711 é alimentado pelo pino GPIO 3 e não pelo 3.3V?
Esta é uma decisão de design para **economia extrema de energia**.
Se ligássemos o sensor no 3.3V direto, ele consumiria bateria (cerca de 1.5mA) o tempo todo, mesmo quando a balança estivesse desligada (Deep Sleep).
Ao ligar no GPIO 3, podemos usar o comando `digitalWrite(3, LOW)` para cortar totalmente a energia do sensor quando a balança vai dormir, garantindo que a bateria dure meses em vez de dias.

### Por que usar Bluetooth (BLE) em vez de Wi-Fi?
O Wi-Fi consome muita energia (cerca de 80mA a 150mA) apenas para manter a conexão. O BLE consome uma fração disso (cerca de 10mA).
Como o objetivo é um dispositivo portátil a bateria, o BLE permite enviar os dados para o celular gastando o mínimo possível. O celular (que tem bateria grande e internet) faz o trabalho pesado de enviar para a nuvem.

### O que significa a taxa de 10Hz ou 80Hz no HX711?
O chip HX711 possui um pino de controle chamado **RATE (Pino 15)**.
* **Nível Lógico 1 (DVDD):** 80 amostras por segundo. Rápido, mas com maior ruído (90nV).
* **Nível Lógico 0 (GND):** 10 amostras por segundo. Lento, mas com menor ruído (50nV) e melhor rejeição de interferências de 50/60Hz da rede elétrica.
* **Neste projeto:** O pino RATE deve estar conectado ao **GND** (padrão na maioria dos módulos comerciais) para garantir a estabilidade do filtro digital.

### Como funciona a calibração?
O valor de calibração é um "Fator de Conversão" que transforma os dados elétricos brutos do sensor em gramas.
Quando você calibra pelo site, o ESP32 calcula esse número e o salva na memória **NVS (Non-Volatile Storage)**. Isso significa que você pode desligar a balança, acabar a bateria ou reiniciar, que ela continuará calibrada para sempre (ou até você calibrar de novo).

---

## 📄 Licença

Este projeto é de código aberto. Sinta-se livre para modificar e melhorar.
