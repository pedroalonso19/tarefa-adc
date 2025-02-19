#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"

// Bibliotecas para configuração do display
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Definições para o I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define DISPLAY_ADDRESS 0x3C

// Definição dos pinos ADC para o joystick
const int X_AXIS_PIN = 26; const int X_AXIS_CHANNEL = 0; // Pino e canal ADC para o eixo X
const int Y_AXIS_PIN = 27; const int Y_AXIS_CHANNEL = 1; // Pino e canal ADC para o eixo Y

// Definição dos pinos PWM
const float PWM_DIVIDER = 16.0; // Divisor do clock PWM
const uint16_t PWM_WRAP = 2048; // Valor máximo de contagem do PWM
uint16_t red_led_level, blue_led_level = 100;
uint red_led_slice, blue_led_slice;

// Definição dos pinos dos LEDs RGB
#define RED_LED_PIN 13
#define GREEN_LED_PIN 11
#define BLUE_LED_PIN 12

// Definição dos pinos dos botões
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define JOYSTICK_BUTTON_PIN 22 // Botão do joystick

// Variável para debounce dos botões
static volatile uint32_t last_interrupt_time = 0; 

// Variável para controlar o estado do LED
static volatile bool is_led_active = true; 

// Estrutura para o display OLED
ssd1306_t display; 

// Enumeração para os estados dos botões
typedef enum {
    BUTTON_PRESSED,
    BUTTON_RELEASED
} ButtonState;

// Inicialização dos LEDs e botões
void initialize_peripherals() {
    gpio_init(GREEN_LED_PIN);
    gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);
    gpio_put(GREEN_LED_PIN, 0);

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);

    gpio_init(JOYSTICK_BUTTON_PIN);
    gpio_set_dir(JOYSTICK_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON_PIN);
}

// Configuração do ADC
void setup_adc() {
    adc_init();
    adc_gpio_init(X_AXIS_PIN); // Configura o pino do eixo X
    adc_gpio_init(Y_AXIS_PIN); // Configura o pino do eixo Y
}

// Configuração do PWM
void setup_pwm() {
    gpio_set_function(RED_LED_PIN, GPIO_FUNC_PWM); 
    red_led_slice = pwm_gpio_to_slice_num(RED_LED_PIN);   
    pwm_set_clkdiv(red_led_slice, PWM_DIVIDER);            
    pwm_set_wrap(red_led_slice, PWM_WRAP);  
    pwm_set_gpio_level(RED_LED_PIN, red_led_level);            
    pwm_set_enabled(red_led_slice, true); 
    
    gpio_set_function(BLUE_LED_PIN, GPIO_FUNC_PWM); 
    blue_led_slice = pwm_gpio_to_slice_num(BLUE_LED_PIN);   
    pwm_set_clkdiv(blue_led_slice, PWM_DIVIDER);            
    pwm_set_wrap(blue_led_slice, PWM_WRAP);
    pwm_set_gpio_level(BLUE_LED_PIN, blue_led_level);              
    pwm_set_enabled(blue_led_slice, true); 
}

// Manipulador de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    if (current_time - last_interrupt_time > 200000) {
        last_interrupt_time = current_time;
        
        if (gpio == JOYSTICK_BUTTON_PIN) {
            gpio_put(GREEN_LED_PIN, !gpio_get(GREEN_LED_PIN)); // Alterna o estado do LED verde
            is_led_active = !is_led_active; // Alterna o estado da borda do display
            printf("Estado do LED Verde e da borda do display alternados.\n");
        } else if (gpio == BUTTON_A_PIN) {
            is_led_active = !is_led_active; // Alterna o estado dos LEDs azul e vermelho
            if (!is_led_active) {
                pwm_set_gpio_level(RED_LED_PIN, 0); 
                pwm_set_gpio_level(BLUE_LED_PIN, 0);
            }
            printf("Estado dos LEDs Azul e Vermelho alternados.\n");
        }
    }
}

// Função principal
int main() {
    // Inicializa o sistema
    stdio_init_all();
    initialize_peripherals();
    setup_adc();
    setup_pwm();

    printf("Sistema inicializado.\n");

    // Configura interrupções para os botões
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // Configuração do I2C para o display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&display, 128, 64, false, DISPLAY_ADDRESS, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_send_data(&display);

    // Limpa o display
    ssd1306_fill(&display, false);
    ssd1306_send_data(&display);

    int x_value = 0, y_value = 0; // Valores lidos do joystick
    int dx = 0, dy = 0; // Posição do quadrado no display
    const int SQUARE_SIZE = 8; // Tamanho do quadrado

    while (true) {
        // Leitura dos valores do joystick
        adc_select_input(X_AXIS_CHANNEL);
        sleep_us(10);
        x_value = adc_read();

        adc_select_input(Y_AXIS_CHANNEL);
        sleep_us(10);
        y_value = adc_read();

        printf("x_value = %i, y_value = %i\n", x_value, y_value);

        // Controle da intensidade dos LEDs
        if (is_led_active) {
            if (x_value > 2048) {
                pwm_set_gpio_level(BLUE_LED_PIN, x_value - 2048);
            } else if (x_value < 2048) {
                pwm_set_gpio_level(BLUE_LED_PIN, 2048 - x_value);
            }

            if (y_value > 2048) {
                pwm_set_gpio_level(RED_LED_PIN, y_value - 2048); 
            } else if (y_value < 2048) {
                pwm_set_gpio_level(RED_LED_PIN, 2048 - y_value);
            }
        }
        
        // Calcula a posição do quadrado no display
        dx = 60 - (x_value * 60 / 4096) - (SQUARE_SIZE / 2);
        dy = y_value * 120 / 4096;

        printf("dx = %i, dy = %i\n", dx, dy);

        // Atualiza o display
        ssd1306_fill(&display, false);

        if (!is_led_active) {
            ssd1306_rect(&display, 2, 2, 124, 60, 1, 0);
        }
        ssd1306_rect(&display, 3, 3, 122, 58, 1, 0);

        if (dx < 0) {
            ssd1306_rect(&display, -dx, dy, SQUARE_SIZE, SQUARE_SIZE, 1, 1);
        } else {
            ssd1306_rect(&display, dx, dy, SQUARE_SIZE, SQUARE_SIZE, 1, 1);
        }

        ssd1306_send_data(&display);

        sleep_ms(50);
    }
    return 0;
}