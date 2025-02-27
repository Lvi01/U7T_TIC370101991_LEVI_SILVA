// Autor: Levi Silva Freitas
// Data: 26/02/2025

// Inclusão de bibliotecas necessárias
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "inc/font.h"
#include "inc/ssd1306.h"

// Definições da comunicação I2C
#define I2C_ENDERECO 0x3C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15

// Definições de pinos e periféricos
#define LED_VERMELHO 13
#define LED_AZUL 12
#define LED_VERDE 11
#define SENSOR_TEMP 26
#define SENSOR_CORRENTE 27
#define BOTAO_TEMP 5
#define BOTAO_CORRENTE 6

#define BUZZER 21
#define BUZZER_PWM_SLICE pwm_gpio_to_slice_num(BUZZER) 
#define BUZZER_PWM_CHANNEL pwm_gpio_to_channel(BUZZER)

// Variáveis globais
static volatile uint64_t Debounc_antes = 0; // Tempo da última interrupção
static volatile uint64_t Alarme_antes = 0;
static volatile bool Alarme = false; 
static volatile bool mostrar_temp = false;
static volatile bool mostrar_corrente = false;
ssd1306_t ssd;

// Protótipos de funções
bool inicializar_componentes();
static void manipulador_irq_gpio(uint gpio, uint32_t eventos);
void monitorar_variaveis();
void configurar_pwm(uint gpio);
void atualizar_led_rgb(uint16_t temp, uint16_t corrente);
void atualizar_ssd(uint16_t temp, uint16_t corrente);
void alarme();

int main() {
    stdio_init_all();
    if (!inicializar_componentes()) {
        printf("Erro ao inicializar componentes.\n");
        return 1;
    }

    while (true) {
        monitorar_variaveis();
        sleep_ms(200);
        const uint16_t frequencias[] = {1000, 1500}; // Alternância de tons
        const uint16_t duracao = 300; // Tempo de cada tom (ms)
        uint8_t i = 0;

        while (Alarme) { // Enquanto o alarme estiver ativo
            pwm_set_clkdiv(BUZZER_PWM_SLICE, 125.0f); // Ajuste do clock
            pwm_set_wrap(BUZZER_PWM_SLICE, 125000 / frequencias[i]); // Define frequência
            pwm_set_chan_level(BUZZER_PWM_SLICE, BUZZER_PWM_CHANNEL, 125000 / frequencias[i] / 2); // 50% Duty Cycle
            sleep_ms(duracao);
            i = !i; // Alterna entre os tons
        }
        pwm_set_chan_level(BUZZER_PWM_SLICE, BUZZER_PWM_CHANNEL, 0); // Desliga o buzzer ao sair do loop

    }
    return 0;
}

bool inicializar_componentes() {
    adc_init();
    adc_gpio_init(SENSOR_TEMP);
    adc_gpio_init(SENSOR_CORRENTE);
    
    gpio_init(BOTAO_TEMP);
    gpio_set_dir(BOTAO_TEMP, GPIO_IN);
    gpio_pull_up(BOTAO_TEMP);
    gpio_set_irq_enabled_with_callback(BOTAO_TEMP, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio);
    
    gpio_init(BOTAO_CORRENTE);
    gpio_set_dir(BOTAO_CORRENTE, GPIO_IN);
    gpio_pull_up(BOTAO_CORRENTE);
    gpio_set_irq_enabled_with_callback(BOTAO_CORRENTE, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio);
    
    // I2C inicialização e configuração do display OLED SSD1306 128x64 pixels com endereço 0x3C e 400 KHz
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, I2C_ENDERECO, I2C_PORT); // Inicializa o display OLED
    i2c_init(I2C_PORT, 400 * 1000); // Inicializa o I2C com 400 KHz

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                    // Pull up the data line
    gpio_pull_up(I2C_SCL);                   // Pull up the clock line
    ssd1306_config(&ssd);                   // Configura o display
    ssd1306_send_data(&ssd);               // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_send_data(&ssd);  // Atualiza o display
    
    configurar_pwm(LED_VERDE);
    configurar_pwm(LED_AZUL);
    configurar_pwm(LED_VERMELHO);
    configurar_pwm(BUZZER);
    
    return true;
}

static void manipulador_irq_gpio(uint gpio, uint32_t eventos) {
    uint64_t agora = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos
    if ((agora - Debounc_antes) < 200000) // Debounce de 200 ms
        return;
    Debounc_antes = agora; // Atualiza o tempo da última interrupção

    if(!Alarme){

        if (gpio == BOTAO_TEMP) {
            mostrar_temp = !mostrar_temp;
            mostrar_corrente = false;
            printf("\nBotao A pressionado\n");
            printf("Mudando o display para mostrar temperatura\n\n");
        } else if (gpio == BOTAO_CORRENTE) {
            mostrar_corrente = !mostrar_corrente;
            mostrar_temp = false;
            printf("\nBotao B pressionado\n");
            printf("Mudando o display para mostrar corrente\n\n");
        }
    }
    else{
        alarme();
    }
}

void monitorar_variaveis() {
    if(Alarme)
        return;
    adc_select_input(0);
    uint16_t temp_val = adc_read();
    printf("Temperatura ADC: %d\n", temp_val);
    adc_select_input(1);
    uint16_t corrente_val = adc_read();
    printf("Corrente ADC: %d\n", corrente_val);
    
    atualizar_led_rgb(temp_val, corrente_val);
    atualizar_ssd(temp_val, corrente_val);
}
void configurar_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice, 4095);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(gpio), 0);
    pwm_set_enabled(slice, true);
}

void atualizar_led_rgb(uint16_t temp, uint16_t corrente) {
    static uint64_t tempo_inicio_alarme = 0; // Variável para armazenar o tempo inicial da detecção
    uint64_t tempo_atual = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual

    // Verifica se a temperatura ou corrente está fora do limite crítico
    if (temp > 4000 || temp < 20 || corrente > 4000 || corrente < 20) {
        if (tempo_inicio_alarme == 0) {
            tempo_inicio_alarme = tempo_atual; // Armazena o tempo inicial apenas na primeira detecção
        }

        if ((tempo_atual - tempo_inicio_alarme) > 5000000) { // Se passaram 5 segundos fora do limite
            printf("Alarme disparado\n");
            alarme();
        }

        // Define LEDs para vermelho indicando perigo
        pwm_set_gpio_level(LED_VERDE, 0);
        pwm_set_gpio_level(LED_AZUL, 0);
        pwm_set_gpio_level(LED_VERMELHO, 4095);
        ssd1306_draw_string(&ssd, "PERIGO", 10, 30);
    } 
    else { 
        tempo_inicio_alarme = 0; // Reseta o contador caso os valores voltem ao normal

        // Define LEDs para amarelo se valores estiverem fora da faixa ideal
        if (temp > 3000 || temp < 1000 || corrente > 3000 || corrente < 1000) {
            pwm_set_gpio_level(LED_VERDE, 4095);
            pwm_set_gpio_level(LED_AZUL, 0);
            pwm_set_gpio_level(LED_VERMELHO, 4095);
        } 
        else { // Define LEDs para verde indicando funcionamento normal
            pwm_set_gpio_level(LED_VERDE, 4095);
            pwm_set_gpio_level(LED_AZUL, 0);
            pwm_set_gpio_level(LED_VERMELHO, 0);
        }
    }
}


void atualizar_ssd(uint16_t temp, uint16_t corrente) {
    char temp_str[5];
    char corrente_str[5];

    sprintf(temp_str, "%d", temp);
    sprintf(corrente_str, "%d", corrente);

    ssd1306_fill(&ssd, false);
    if(!Alarme){
        if (mostrar_temp) {
            ssd1306_draw_string(&ssd, "Temperatura ADC", 3, 10);
            ssd1306_draw_string(&ssd, temp_str, 50, 20);
        }
        else if(mostrar_corrente){
            ssd1306_draw_string(&ssd, "Corrente ADC", 18, 10);
            ssd1306_draw_string(&ssd, corrente_str, 50, 20);
        }
        ssd1306_send_data(&ssd);
    }
    else{
        ssd1306_draw_string(&ssd, "ALARME", 10, 10);
        ssd1306_draw_string(&ssd, "DISPARADO", 10, 20);
        ssd1306_draw_string(&ssd, "Aperte algum", 10, 30);
        ssd1306_draw_string(&ssd, "botao para", 10, 40);
        ssd1306_draw_string(&ssd, "desativar", 10, 50);
        ssd1306_send_data(&ssd);
    }
}

void alarme() {
    Alarme = !Alarme;

    if (Alarme) {
        pwm_set_gpio_level(LED_VERDE, 0);
        pwm_set_gpio_level(LED_AZUL, 0);
        pwm_set_gpio_level(LED_VERMELHO, 4095);
        printf("Alarme ativado!\n");
    } else {
        printf("Alarme desativado!\n");
        pwm_set_gpio_level(BUZZER, 0);
    }
}
