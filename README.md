# Projeto: Controle de Joystick e LEDs com RP2040

## Autor
**Levi Silva Freitas**

## Data
**18 de fevereiro de 2025**

## Link de entrega do vídeo
Link: https://youtube.com/shorts/Sr-U8rkz5bg?feature=share

## Descrição do Projeto
Este projeto visa implementar um sistema embarcado utilizando a placa **BitDogLab** com o microcontrolador **RP2040**. O sistema utiliza um **joystick analógico** para controlar a intensidade de **LEDs RGB** e exibir sua posição em um **display OLED SSD1306**.

O código permite:
- Controlar a intensidade dos **LEDs RGB** via **PWM**, baseado na posição do joystick.
- Exibir a posição do **joystick** no **display OLED**, representada por um quadrado móvel.
- Alternar o estado do **LED Verde** e modificar a borda do display ao pressionar o botão do joystick.
- Ativar ou desativar o controle **PWM dos LEDs** ao pressionar o botão **A**.
- Utilizar **interrupções (IRQ)** para detectar eventos de botão com **debouncing** via software.

## Componentes Utilizados
- **LED RGB** – GPIOs **11, 12 e 13** (Verde, Azul, Vermelho).
- **Joystick Analógico** – GPIOs **26 (X) e 27 (Y)**.
- **Botão do Joystick** – GPIO **22**.
- **Botão A** – GPIO **5**.
- **Display OLED SSD1306** via I2C – GPIOs **14 e 15**.

## Estrutura do Código

### 1. Inclusão de Bibliotecas
O código inclui as bibliotecas essenciais para controle de hardware:
```c
#include <stdio.h> // Inclusão da biblioteca padrão de entrada e saída
#include <stdlib.h> // Inclusão da biblioteca padrão do C
#include "pico/stdlib.h" // Inclusão da biblioteca de funções padrão do Pico
#include "hardware/adc.h" // Inclusão da biblioteca de funções do ADC
#include "hardware/i2c.h" // Inclusão da biblioteca de funções do I2C
#include "hardware/pwm.h" // Inclusão da biblioteca de funções do PWM
#include "hardware/gpio.h" // Inclusão da biblioteca de funções do GPIO
#include "inc/font.h" // Inclusão da biblioteca de fontes
#include "inc/ssd1306.h" // Inclusão da biblioteca do display OLED
```

### 2. Definição de Constantes e Pinos
Foram definidas constantes para **pinos GPIO** e configurações de hardware:
```c
// Definições da comunicação I2C
#define I2C_ENDERECO 0x3C // Endereço do display OLED
#define I2C_PORT i2c1 // Barramento I2C utilizado
#define I2C_SDA 14 // Pino SDA do barramento I2C
#define I2C_SCL 15 // Pino SCL do barramento I2C

// Definições de pinos e periféricos
#define LED_VERMELHO 13 // Pino do LED vermelho
#define LED_AZUL 12 // Pino do LED azul
#define LED_VERDE 11 // Pino do LED verde
#define JOYSTICK_Y 27 // Pino do eixo Y do joystick
#define JOYSTICK_X 26 // Pino do eixo X do joystick
#define BOTAO_JOY 22 // Pino do botão do joystick
#define BOTAO_A 5 // Pino do botão A
```

### 3. Inicialização de Componentes
A função `inicializar_componentes()` configura os **LEDs**, **botões**, **joystick** e o **display**:
```c
bool inicializar_componentes() {
    adc_init(); // Inicializa o ADC
    adc_gpio_init(JOYSTICK_X); // Inicializa o pino do eixo X do joystick
    adc_gpio_init(JOYSTICK_Y); // Inicializa o pino do eixo Y do joystick

    gpio_init(BOTAO_JOY); // Inicializa o pino do botão do joystick
    gpio_set_dir(BOTAO_JOY, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BOTAO_JOY); // Habilita o resistor de pull-up interno
    gpio_init(BOTAO_A); // Inicializa o pino do botão A
    gpio_set_dir(BOTAO_A, GPIO_IN); // Configura o pino como entrada
    gpio_pull_up(BOTAO_A); // Habilita o resistor de pull-up interno 
    
    gpio_set_irq_enabled_with_callback(BOTAO_JOY, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio); // Habilita a interrupção no botão do joystick
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &manipulador_irq_gpio); // Habilita a interrupção no botão A
    
    i2c_init(I2C_PORT, 400 * 1000); // Inicializa o barramento I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configura o pino SDA como pino I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configura o pino SCL como pino I2C
    gpio_pull_up(I2C_SDA); // Habilita o resistor de pull-up no pino SDA
    gpio_pull_up(I2C_SCL); // Habilita o resistor de pull-up no pino SCL
    
    ssd1306_init(&display, 128, 64, false, I2C_ENDERECO, I2C_PORT); // Inicializa o display OLED
    ssd1306_config(&display); // Configura o display OLED
    ssd1306_fill(&display, false); // Limpa o display
    ssd1306_send_data(&display); // Atualiza o display
    
    configurar_pwm(LED_AZUL); // Configura o PWM para o LED azul
    configurar_pwm(LED_VERMELHO); // Configura o PWM para o LED vermelho
    configurar_pwm(LED_VERDE); // Configura o PWM para o LED verde
    return true; // Retorna verdadeiro para indicar que a inicialização foi bem-sucedida
}
```

### 4. Controle de LEDs RGB via PWM
Os LEDs são controlados dinamicamente com base na posição do joystick:
```c
void definir_padrao_led(uint16_t x_val, uint16_t y_val) {
    if (leds_pwm_ativados) { // Verifica se os LEDs PWM estão ativados
        pwm_set_gpio_level(LED_AZUL, abs((int)x_val - 2048) * 2); // Define a intensidade do LED azul
        pwm_set_gpio_level(LED_VERMELHO, abs((int)y_val - 2048) * 2); // Define a intensidade do LED vermelho
    } else {
        pwm_set_gpio_level(LED_AZUL, 0); // Desliga o LED azul
        pwm_set_gpio_level(LED_VERMELHO, 0); // Desliga o LED vermelho
    }
}
```

### 5. Representação Gráfica no Display
A função `desenhar_borda()` altera a borda do display:
```c
void desenhar_borda() {
    switch (estilo_borda) { // Verifica o estilo da borda
        case 0:
            ssd1306_rect(&display, 0, 0, 127, 63, true, false); // Desenha a borda simples
            break;
        case 1:
            ssd1306_rect(&display, 0, 0, 127, 63, true, false); // Desenha a borda dupla
            ssd1306_rect(&display, 1, 1, 125, 61, true, false); // Desenha a borda dupla
            break;
    }
}
```

### 6. Tratamento de Interrupções
Os botões **Joystick e A** utilizam interrupções para alternar os LEDs e estilos de borda:
```c
static void manipulador_irq_gpio(uint gpio, uint32_t eventos) {
    uint64_t agora = to_us_since_boot(get_absolute_time()); // Obtém o tempo atual em microssegundos
    if ((agora - antes) < 200000) // Debounce de 200 ms
        return;
    antes = agora; // Atualiza o tempo da última interrupção
    
    if (gpio == BOTAO_JOY) { // Verifica se a interrupção foi no botão do joystick
        printf("Botão do joystick pressionado.\n");
        estado_led_verde = !estado_led_verde; // Alterna o estado do LED verde
        printf("Estado do LED verde: %s\n", estado_led_verde ? "Ligado" : "Desligado"); // Exibe o estado do LED verde
        pwm_set_gpio_level(LED_VERDE, estado_led_verde ? 4095 : 0); // Atualiza o estado do LED verde
        estilo_borda = (estilo_borda + 1) % 2; // Alterna o estilo da borda
        printf("Estilo da borda: %s\n", estilo_borda == 0 ? "Simples" : "Dupla"); // Exibe o estilo da borda
    } else if (gpio == BOTAO_A) { // Verifica se a interrupção foi no botão A
        printf("Botão A pressionado.\n"); 
        leds_pwm_ativados = !leds_pwm_ativados; // Alterna o estado dos LEDs PWM
        printf("PWM dos LEDs: %s\n", leds_pwm_ativados ? "Ativado" : "Desativado"); // Exibe o estado dos LEDs PWM
    }
}
```

## Como Executar o Projeto
1. **Compilar o código** para a plataforma **RP2040**.
2. **Carregar o binário** na placa **BitDogLab**.
3. **Observar o controle dos LEDs RGB e a movimentação no display OLED**.

## Conclusão
Este projeto demonstra o uso de **ADC, PWM, I2C e interrupções** no **RP2040**. Ele possibilita o controle de LEDs e exibição gráfica de forma intuitiva, utilizando **boas práticas de programação para sistemas embarcados**.

## Contato
✉️ **Email:** lsfreitas218@gmail.com
