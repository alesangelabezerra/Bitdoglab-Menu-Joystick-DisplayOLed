#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

//Definição dos pinos do I2C para o display SSD1306
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

//Definição dos pinos do joystick e botão
const int VRX_PIN = 26;
const int VRY_PIN = 27;
const int SW_PIN = 22;

//Definição dos pinos do LED RGB
const uint BLUE_LED_PIN = 12;
const uint RED_LED_PIN = 13;
const uint GREEN_LED_PIN = 11;

//Variáveis globais
uint8_t ssd1306_buffer[1024]; //Aqui é o Buffer pra usar no display SSD1306
int current_menu_option = 0;  //Essa mostra a Oppção que foi selecionada no menu
bool in_menu = true;          //Vai mostrar se o menu está ativo

void inicializa() {
    stdio_init_all();

    //Inicialização do ADC para o joystick
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    //Inicialização do I2C pra o display SSD1306
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SCL);
    gpio_pull_up(PINO_SDA);

    //Inicialização do display SSD1306
    ssd1306_init(I2C_PORT, 0x3C, 128, 64, ssd1306_buffer);

    //Inicialização dos LEDs RGB
    gpio_set_function(RED_LED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(GREEN_LED_PIN, GPIO_FUNC_PWM);
    gpio_set_function(BLUE_LED_PIN, GPIO_FUNC_PWM);

    uint slice_red = pwm_gpio_to_slice_num(RED_LED_PIN);
    uint slice_green = pwm_gpio_to_slice_num(GREEN_LED_PIN);
    uint slice_blue = pwm_gpio_to_slice_num(BLUE_LED_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_red, &config, true);
    pwm_init(slice_green, &config, true);
    pwm_init(slice_blue, &config, true);

    //Inicialmente desliga o LED RGB
    pwm_set_gpio_level(RED_LED_PIN, 0);
    pwm_set_gpio_level(GREEN_LED_PIN, 0);
    pwm_set_gpio_level(BLUE_LED_PIN, 0);

    //Inicialização do botão do joystick
    gpio_init(SW_PIN);
    gpio_set_dir(SW_PIN, GPIO_IN);
    gpio_pull_up(SW_PIN);
}


//Aqui é a função que mostra o texto no display
void print_texto(const char *msg, uint pos_x, uint pos_y) {
    ssd1306_draw_string(ssd1306_buffer, pos_x, pos_y, msg);
    struct render_area frame_area = {
        start_column: 0,
        end_column: 127,
        start_page: 0,
        end_page: 7
    };
    calculate_render_area_buffer_length(&frame_area);
    render_on_display(ssd1306_buffer, &frame_area);
}

//Aqui é a função que desenha um pixel no display
void ssd1306_draw_pixel(uint8_t *buffer, int x, int y, bool set) {
    if (x >= 0 && x < 128 && y >= 0 && y < 64) {
        int byte_idx = (y / 8) * 128 + x;
        uint8_t bit_mask = 1 << (y % 8);

        if (set) {
            buffer[byte_idx] |= bit_mask;
        } else {
            buffer[byte_idx] &= ~bit_mask;
        }
    }
}

//Aqui é a função que desenha um retângulo vazio pra mostrar qual opção está sendo selecionada pela navegação do Joystick
void ssd1306_draw_empty_square(uint8_t *buffer, int x1, int y1, int x2, int y2) {
    for (int x = x1; x <= x2; x++) {
        ssd1306_draw_pixel(buffer, x, y1, 1); //A linha de cima
        ssd1306_draw_pixel(buffer, x, y2, 1); //A linha de baixo
    }

    for (int y = y1; y <= y2; y++) {
        ssd1306_draw_pixel(buffer, x1, y, 1); //Linha da esquerda
        ssd1306_draw_pixel(buffer, x2, y, 1); //Linha da direita
    }
}

//Aqui é a função que desliga os leds depois que a função é interrompida e volta para a navegação do menu
void desliga_leds() {
    pwm_set_gpio_level(RED_LED_PIN, 0);
    pwm_set_gpio_level(GREEN_LED_PIN, 0);
    pwm_set_gpio_level(BLUE_LED_PIN, 0);
}

//A função para mostrar o menu no display
void display_menu(int selected_option) {
    desliga_leds();
    memset(ssd1306_buffer, 0, 1024);

    //Título do menu
    print_texto("Menu", 52, 2);

    //Opções do menu
    print_texto("Joystick Led", 6, 18);
    print_texto("Buzzer PWM", 6, 31);
    print_texto("PWM Led", 6, 42);

    //Desenha o retângulo ao redor da opção selecionada
    switch (selected_option) {
        case 0:
            ssd1306_draw_empty_square(ssd1306_buffer, 2, 16, 120, 24);
            break;
        case 1:
            ssd1306_draw_empty_square(ssd1306_buffer, 2, 25, 120, 35);
            break;
        case 2:
            ssd1306_draw_empty_square(ssd1306_buffer, 2, 40, 120, 48);
            break;
    }

    //Atualiza o display
    struct render_area frame_area = {
        start_column: 0,
        end_column: 127,
        start_page: 0,
        end_page: 7
    };
    calculate_render_area_buffer_length(&frame_area);
    render_on_display(ssd1306_buffer, &frame_area);
}

//Função pra ler o eixo Y do joystick
int read_joystick_y() {
    adc_select_input(0);
    sleep_us(2);         
    uint adc_y_raw = adc_read();
    
    printf("Valor do eixo Y: %d\n", adc_y_raw);

    //Ajuste da sensibilidade do joystick
    if (adc_y_raw < 1000) {
        return -1;
    } else if (adc_y_raw > 3000) {
        return 1;
    } else {
        return 0;
    }
}

//Aqui é a função que verifica se o botão do joystick foi pressionado
bool is_button_pressed() {
    if (!gpio_get(SW_PIN)) {
        sleep_ms(50);
        if (!gpio_get(SW_PIN)) {
            while (!gpio_get(SW_PIN));
            sleep_ms(50);
            return true;
        }
    }
    return false;
}

//Implementação do programa do joystick led de acordo com o link dado na tarefa
void programa_joystick() {
    uint16_t vrx_value, vry_value;
    while (true) {
        adc_select_input(0);
        sleep_us(2);
        vrx_value = adc_read();

        adc_select_input(1);
        sleep_us(2);
        vry_value = adc_read();

        pwm_set_gpio_level(BLUE_LED_PIN, vrx_value);
        pwm_set_gpio_level(RED_LED_PIN, vry_value);

        if (is_button_pressed()) {
            return;
        }

        sleep_ms(100);
    }
}

//Implementação do programa do buzzer PWM de acordo com o link dado na tarefa
void programa_buzzer() {
    const uint BUZZER_PIN = 21;
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);

    const uint star_wars_notes[] = {
        330, 330, 330, 262, 392, 523, 330, 262,
        392, 523, 330, 659, 659, 659, 698, 523,
        415, 349, 330, 262, 392, 523, 330, 262,
        392, 523, 330, 659, 659, 659, 698, 523,
        415, 349, 330, 523, 494, 440, 392, 330,
        659, 784, 659, 523, 494, 440, 392, 330,
        659, 659, 330, 784, 880, 698, 784, 659,
        523, 494, 440, 392, 659, 784, 659, 523,
        494, 440, 392, 330, 659, 523, 659, 262,
        330, 294, 247, 262, 220, 262, 330, 262,
        330, 294, 247, 262, 330, 392, 523, 440,
        349, 330, 659, 784, 659, 523, 494, 440,
        392, 659, 784, 659, 523, 494, 440, 392
    };

    const uint note_duration[] = {
        500, 500, 500, 350, 150, 300, 500, 350,
        150, 300, 500, 500, 500, 500, 350, 150,
        300, 500, 500, 350, 150, 300, 500, 350,
        150, 300, 650, 500, 150, 300, 500, 350,
        150, 300, 500, 150, 300, 500, 350, 150,
        300, 650, 500, 350, 150, 300, 500, 350,
        150, 300, 500, 500, 500, 500, 350, 150,
        300, 500, 500, 350, 150, 300, 500, 350,
        150, 300, 500, 350, 150, 300, 500, 500,
        350, 150, 300, 500, 500, 350, 150, 300,
    };

    for (int i = 0; i < sizeof(star_wars_notes) / sizeof(star_wars_notes[0]); i++) {
        if (star_wars_notes[i] == 0) {
            sleep_ms(note_duration[i]);
        } else {
            uint32_t clock_freq = clock_get_hz(clk_sys);
            uint32_t top = clock_freq / star_wars_notes[i] - 1;
            pwm_set_wrap(slice_num, top);
            pwm_set_gpio_level(BUZZER_PIN, top / 2);
            sleep_ms(note_duration[i]);
            pwm_set_gpio_level(BUZZER_PIN, 0);
            sleep_ms(50);
        }

        if (is_button_pressed()) {
            break;
        }
    }
}

//Implementação do programa do PWM LED RGB de acordo com o link dado na tarefa
void programa_led_rgb() {
    const uint16_t PERIOD = 2000;
    const float DIVIDER_PWM = 16.0;
    const uint16_t LED_STEP = 100;
    uint16_t led_level = 100;
    uint up_down = 1;

    
    uint slice_blue = pwm_gpio_to_slice_num(BLUE_LED_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, DIVIDER_PWM);
    pwm_config_set_wrap(&config, PERIOD);

    pwm_init(slice_blue, &config, true);

    pwm_set_gpio_level(BLUE_LED_PIN, led_level);
    pwm_set_gpio_level(RED_LED_PIN, 0);
    pwm_set_gpio_level(GREEN_LED_PIN, 0);

    while (true) {
        //Verifica se o botão do joystick foi pressionado para sair do loop
        if (is_button_pressed()) {
            break;
        }

        pwm_set_gpio_level(BLUE_LED_PIN, led_level);

        //Aqui fica vendo se o botão foi pressionado a cada 100 ms e mantendo o tempo total de transição em 1 segundo
        for (int i = 0; i < 10; i++) {
            sleep_ms(100);
            if (is_button_pressed()) {
                goto exit_loop;
            }
        }

        if (up_down) {
            led_level += LED_STEP;
            if (led_level >= PERIOD) {
                up_down = 0;
            }
        } else {
            led_level -= LED_STEP;
            if (led_level <= LED_STEP) {
                up_down = 1;
            }
        }
    }

exit_loop:
    desliga_leds();
}

int main() {
    inicializa();

    display_menu(current_menu_option);

    //Loop principal
    while (true) {
        if (in_menu) {
            int y_direction = read_joystick_y();
    
            if (y_direction == -1) {
                current_menu_option = (current_menu_option - 1 + 3) % 3;
                display_menu(current_menu_option);
                sleep_ms(200);
            } else if (y_direction == 1) {
                current_menu_option = (current_menu_option + 1) % 3;
                display_menu(current_menu_option);
                sleep_ms(200);
            }
    
            if (is_button_pressed()) {
                in_menu = false;
                sleep_ms(200);
            }
        } else {
            switch (current_menu_option) {
                case 0:
                    printf("Executando Programa 01 (Joystick)\n");
                    programa_joystick();
                    break;
                case 1:
                    printf("Executando Programa 02 (Buzzer)\n");
                    programa_buzzer();
                    break;
                case 2:
                    printf("Executando Programa 03 (LED RGB)\n");
                    programa_led_rgb();
                    break;
            }
    
            desliga_leds();
            in_menu = true;
            display_menu(current_menu_option);
            sleep_ms(200);
        }
    }
    

    return 0;
}
