# Sistema de Controle de Ônibus 🚌

Este é um projeto desenvolvido para a placa de desenvolvimento BitDogLab, baseada no microcontrolador Raspberry Pi Pico W.  
O objetivo é simular um painel de controle de lotação de um ônibus urbano, utilizando botões para entrada, saída e reset de passageiros, gerando alertas visuais (LED RGB e matriz WS2812B), sonoros (buzzer) e exibindo dados em tempo real em um display OLED.

---

## 📌 Sobre o Projeto

O Sistema de Controle de Ônibus foi implementado como parte das atividades da 2ª fase da residência tecnológica EmbarcaTech.  
O projeto utiliza o **FreeRTOS**, com múltiplas tarefas que se sincronizam por semáforos de contagem, semáforos binários e mutex, para gerenciar:

- Contagem de passageiros embarcando e desembarcando (botões A e B).  
- Reset assíncrono da contagem (botão de joystick).  
- Indicação de ocupação por LED RGB e matriz WS2812B.  
- Saídas sonoras via buzzer.  
- Exibição de contagem e alertas no display OLED SSD1306.  

---

## 🧪 Como funciona

1. **Entrada de Passageiro (Botão A)**  
   A ISR libera um semáforo binário; a tarefa adiciona ao semáforo de contagem se houver vaga e atualiza o display, ou emite beep curto se estiver lotado.

2. **Saída de Passageiro (Botão B)**  
   A ISR libera outro semáforo binário; a tarefa retira do semáforo de contagem se houver passageiro e atualiza o display.

3. **Reset do Sistema (Joystick)**  
   A ISR libera um semáforo binário; a tarefa esvazia o semáforo de contagem, ajusta a contagem para zero, emite beep duplo e atualiza o display.

4. **LED RGB**  
   A tarefa em loop lê `passageiros` e ajusta a cor (azul, verde, amarelo, vermelho) a cada 10 ms conforme a ocupação.

5. **Matriz WS2812B (PIO)**  
   A tarefa inicializa o PIO e, a cada 200 ms, acende um LED por passageiro na cor definida (verde, amarelo ou vermelho).

6. **Display OLED SSD1306 (I²C)**  
   Todas as atualizações de contagem ocorrem dentro de uma seção protegida por mutex, exibindo “Bus control”, número de passageiros e limite no OLED.

---


## 📁 Utilização

Atendendo aos requisitos de organização da 2ª fase da residência, o arquivo `CMakeLists.txt` está configurado para facilitar a importação do projeto no Visual Studio Code.  
Siga as instruções abaixo:

1. Na barra lateral do VS Code, clique em **Raspberry Pi Pico Project** e depois em **Import Project**.

   ![image](https://github.com/user-attachments/assets/4b1ed8c7-6730-4bfe-ae1f-8a26017d1140)

2. Selecione o diretório do projeto (por exemplo, `bus_control`) e clique em **Import** (utilizando a versão **2.1.1** do Pico SDK).

   ![image](https://github.com/user-attachments/assets/be706372-b918-4ade-847e-12706af0cc99)

3. É necessária a instalação do **FreeRTOS**. No `CMakeLists.txt`, ajuste o caminho de inclusão para apontar para o diretório onde o FreeRTOS está instalado.

   ![image](https://github.com/user-attachments/assets/7271e3aa-405c-4e24-9022-8a4e1ea98f97)

4. Agora, basta **compilar** e **rodar** o projeto, com a placa BitDogLab conectada.

---
