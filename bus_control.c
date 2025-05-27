#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"
#include "stdio.h"
#include "bus_control.pio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

#define BOTAO_A 5 
#define BOTAO_B 6 
#define BOTAO_J 22 

#define LED_RED 13
#define LED_GREEN  11
#define LED_BLUE 12

#define BUZZER_A_PIN 10
#define BUZZER_B_PIN 21

#define NUM_PIXELS 25 
#define MATRIX_PIN 7 

ssd1306_t ssd; // Variável para armazenar o display OLED

uint32_t last_time = 0; // Variável para armazenar o último tempo de interrupção
uint8_t passageiros = 0; // Contador de passageiros

#define PASS_MAX 10 // Limite máximo de passageiros

SemaphoreHandle_t xPassageirosSem; // Semáforo de contagem para controlar o número de passageiros
SemaphoreHandle_t xSemBotaoA; // Semáforo binário para o botão A
SemaphoreHandle_t xSemBotaoB;  // Semáforo binário para o botão B
SemaphoreHandle_t xButtonSem; // Semáforo binário para o botão do joystick
SemaphoreHandle_t xDisplayMutex; // Mutex para proteger o acesso ao display

void init_gpios();
void gpio_irq_handler(uint gpio, uint32_t events);
void draw_screen();
void pwm_setup_gpio(uint gpio, uint freq);
uint32_t matrix_rgb(double r, double g, double b);
void vLEDTask(void *params);
void vTaskEntrada(void *params);
void vTaskSaida(void *params);
void vTaskReset(void *params);
void vTaskMatrizLED(void *params);

int main()
{
    stdio_init_all();

    init_gpios();

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(&ssd);
    draw_screen();
    ssd1306_send_data(&ssd);

    bool cor = true;

    gpio_set_irq_enabled_with_callback(BOTAO_J, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    xDisplayMutex = xSemaphoreCreateMutex();
    xPassageirosSem = xSemaphoreCreateCounting(PASS_MAX, 0);
    xSemBotaoA = xSemaphoreCreateBinary();
    xSemBotaoB = xSemaphoreCreateBinary();
    xButtonSem = xSemaphoreCreateBinary();

    xTaskCreate(vLEDTask, "LED Task", 256, NULL, 1, NULL);
    xTaskCreate(vTaskEntrada, "Entrada Task", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskSaida, "Saida Task", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskReset, "Reset Task", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vTaskMatrizLED, "Matriz LED", 512, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}

// Inicia os pinos GPIO
void init_gpios() {
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_init(BOTAO_J);
    gpio_set_dir(BOTAO_J, GPIO_IN);
    gpio_pull_up(BOTAO_J);

    gpio_init(LED_RED);
    gpio_set_dir(LED_RED, GPIO_OUT);

    gpio_init(LED_GREEN);
    gpio_set_dir(LED_GREEN, GPIO_OUT);

    gpio_init(LED_BLUE);
    gpio_set_dir(LED_BLUE, GPIO_OUT);

    gpio_init(BUZZER_A_PIN);
    gpio_set_dir(BUZZER_A_PIN, GPIO_OUT);

    gpio_init(BUZZER_B_PIN);
    gpio_set_dir(BUZZER_B_PIN, GPIO_OUT);
}

// Função de callback para tratamento de interrupção dos botões
void gpio_irq_handler(uint gpio, uint32_t events) { 
    uint32_t current_time = to_us_since_boot(get_absolute_time()); // Pega o tempo atual em ms
    if (current_time - last_time > 250000) { // Debouncing de 250ms
        last_time = current_time;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (gpio == BOTAO_A) { // Verifica se o botão A foi pressionado 
            printf("Botão A pressionado!\n");
            xSemaphoreGiveFromISR(xSemBotaoA, NULL);
        } else if (gpio == BOTAO_B) { 
            printf("Botão B pressionado!\n");
            xSemaphoreGiveFromISR(xSemBotaoB, NULL);
        } else if (gpio == BOTAO_J) { 
            printf("Botão do joystick pressionado!\n");
            xSemaphoreGiveFromISR(xButtonSem, NULL);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// Desenha a tela OLED com as informações do controle de passageiros
void draw_screen() {
    char buffer[32];

    ssd1306_fill(&ssd, 0); // Limpa a tela
    ssd1306_rect(&ssd, 0, 0, 128, 64, true, false); 
    ssd1306_draw_string(&ssd, "Bus control", 21, 2);
    ssd1306_rect(&ssd, 0, 0, 128, 12, true, false);
    
    ssd1306_draw_string(&ssd, "Passageiros: ", 2, 16);
    sprintf(buffer, "%d", passageiros);
    ssd1306_draw_string(&ssd, buffer, 102, 16);
    ssd1306_draw_string(&ssd, "Limite: ", 2, 26);
    sprintf(buffer, "%d", PASS_MAX);
    ssd1306_draw_string(&ssd, buffer, 62, 26);
}

// Tarefa que controla os LEDs RGB
void vLEDTask(void *params) {
    while (true) {
            if (passageiros == 0) {
                // Nenhum passageiro, LED azul
                gpio_put(LED_RED,   0);
                gpio_put(LED_GREEN, 0);
                gpio_put(LED_BLUE,  1);
            } else if (passageiros <= PASS_MAX - 2) {
                // Passageiros abaixo do limite, LED verde
                gpio_put(LED_RED,   0);
                gpio_put(LED_GREEN, 1);
                gpio_put(LED_BLUE,  0);
            } else if (passageiros == PASS_MAX - 1) {
                // Uma vaga restante, LED amarelo
                gpio_put(LED_RED,   1);
                gpio_put(LED_GREEN, 1);
                gpio_put(LED_BLUE,  0);
            } else {
                // Lotado, LED vermelho
                gpio_put(LED_RED,   1);
                gpio_put(LED_GREEN, 0);
                gpio_put(LED_BLUE,  0);
            }
        vTaskDelay(pdMS_TO_TICKS(10));
        
    }
}

// Tarefa que controla a entrada de passageiros
void vTaskEntrada(void *params) {
    while (true) {
        if (xSemaphoreTake(xSemBotaoA, portMAX_DELAY) == pdTRUE) { // Aguarda o botão A ser pressionado
            if (xSemaphoreGive(xPassageirosSem) == pdTRUE) { // Tenta adicionar um passageiro ao semáforo
                passageiros = uxSemaphoreGetCount(xPassageirosSem);
                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY)) { // Protege o acesso ao display e atualiza a tela
                    draw_screen();
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xDisplayMutex);
                }
            } else {
                pwm_setup_gpio(BUZZER_A_PIN, 1000); // Emite um som de aviso
                vTaskDelay(pdMS_TO_TICKS(150)); // Aguarda 100ms
                pwm_setup_gpio(BUZZER_A_PIN, 0); // Desliga o buzzer
                gpio_put(BUZZER_A_PIN, 0);
            }
        } 
    }
}

// Tarefa que controla a saída de passageiros
void vTaskSaida(void *params) {
    while (true) {
        if (xSemaphoreTake(xSemBotaoB, portMAX_DELAY) == pdTRUE) { // Aguarda o botão B ser pressionado
            if (xSemaphoreTake(xPassageirosSem, 0) == pdTRUE) { // Tenta retirar um passageiro do semáforo
                passageiros = uxSemaphoreGetCount(xPassageirosSem);
                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY)) { // Protege o acesso ao display e atualiza a tela
                    draw_screen();
                    ssd1306_send_data(&ssd);
                    xSemaphoreGive(xDisplayMutex);
                }
            }
        }
    }
}

// Tarefa que reseta o contador de passageiros 
void vTaskReset(void *params) {
    while (true) {
        if (xSemaphoreTake(xButtonSem, portMAX_DELAY) == pdTRUE) {
            // Esvazia o semáforo até ficar com 0 (todos passageiros saem)
            while (uxSemaphoreGetCount(xPassageirosSem) > 0) {
                xSemaphoreTake(xPassageirosSem, 0);
            }

            passageiros = uxSemaphoreGetCount(xPassageirosSem);

            // Aviso sonoro
            pwm_setup_gpio(BUZZER_A_PIN, 1000); // Emite um som de aviso
            vTaskDelay(pdMS_TO_TICKS(150)); // Aguarda 100ms
            pwm_setup_gpio(BUZZER_A_PIN, 1250); // Emite um som de aviso
            vTaskDelay(pdMS_TO_TICKS(150)); // Aguarda 100ms
            pwm_setup_gpio(BUZZER_A_PIN, 0); // Desliga o buzzer
            gpio_put(BUZZER_A_PIN, 0);

            // Atualiza o display
            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY)) {
                draw_screen();
                ssd1306_send_data(&ssd);
                xSemaphoreGive(xDisplayMutex);
            }
            vTaskDelay(pdTICKS_TO_MS(1000));
        }
    }
}

// Tarefa que controla a matriz de LEDs
void vTaskMatrizLED(void *params) {
    PIO pio = pio0; 
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, MATRIX_PIN);

    while (true) {
        // Define a cor com base no número de passageiros
        double r = 0, g = 0, b = 0;

        if (passageiros < 9) {
            // Verde
            r = 0; g = 25; b = 0;
        } else if (passageiros == 9) {
            // Amarelo (r + g)
            r = 25; g = 25; b = 0;
        } else {
            // Vermelho
            r = 25; g = 0; b = 0;
        }

        // Desenha os LEDs (um por passageiro)
        for (int i = 0; i < NUM_PIXELS; i++) {
            if (i < passageiros) {
                uint32_t valor_led = matrix_rgb(r, g, b);
                pio_sm_put_blocking(pio, sm, valor_led);
            } else {
                pio_sm_put_blocking(pio, sm, 0); // apagado
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200)); // atualiza a cada 200 ms
    }
}

// Rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double r, double g, double b) {
    unsigned char R, G, B;
    R = r;
    G = g;
    B = b;
    return (G << 24) | (R << 16) | (B << 8);
}


// Função para configurar o PWM
void pwm_setup_gpio(uint gpio, uint freq) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);  // Define o pino como saída PWM
    uint slice_num = pwm_gpio_to_slice_num(gpio);  // Obtém o slice do PWM

    if (freq == 0) {
        pwm_set_enabled(slice_num, false);  // Desabilita o PWM
        gpio_put(gpio, 0);  // Desliga o pino
    } else {
        uint32_t clock_div = 4; // Define o divisor do clock
        uint32_t wrap = (clock_get_hz(clk_sys) / (clock_div * freq)) - 1; // Calcula o valor de wrap

        // Configurações do PWM (clock_div, wrap e duty cycle) e habilita o PWM
        pwm_set_clkdiv(slice_num, clock_div); 
        pwm_set_wrap(slice_num, wrap);  
        pwm_set_gpio_level(gpio, wrap / 5);  
        pwm_set_enabled(slice_num, true);  
    }
}

