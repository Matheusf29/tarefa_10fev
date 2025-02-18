//Incluindo as bibliotecas
#include "hardware/pio.h"  
#include "hardware/i2c.h" 
#include "pico/stdlib.h" 
#include "ssd1306.h"   
#include <stdlib.h> 
#include <stdio.h>
#include "font.h"  
#include "hardware/adc.h" 
#include "hardware/pwm.h"
#include "math.h"

// Definições as portas utilizadas 
#define I2C_PORT i2c1    // Porta I2C
#define I2C_SDA 14      
#define I2C_SCL 15     
#define ENDERECO 0x3C   // Endereço do display SSD1306

#define JOYSTICK_X_PIN 26  // GPIO para eixo X
#define JOYSTICK_Y_PIN 27  // GPIO para eixo Y
#define JOYSTICK_PB 22 // Botão do Joystick
#define BOTAO_A 5      
#define azul 12              //RGB
#define verde 11            //RGB
#define vermelho 13        //RGB

volatile uint32_t last_time = 0; 
bool led_green = false, led_blue_red = true, cor = true, quad = true;
ssd1306_t ssd;  
uint16_t eixo_x, eixo_y, level_r, level_b;
int centro = 2047, xquad = 60, yquad = 28;    
uint slice_r, slice_b;

// Função para configurar o PWM na GPIO especificada (pin 22)
void configure_pwm(uint pin, uint *slice, uint16_t level) {

    // Inicializa o slice e o canal
    gpio_set_function(pin, GPIO_FUNC_PWM);
    *slice = pwm_gpio_to_slice_num(pin);
    pwm_set_clkdiv(*slice, 16); // 16 MHz
    pwm_set_wrap(*slice, 4095);
    pwm_set_gpio_level(pin, level);
    // Ativa o PWM
    pwm_set_enabled(*slice, true);
}

//Inicializar os pinos do led RGB
void iniciar_rgb() {
    gpio_init(verde);
    gpio_set_dir(verde, GPIO_OUT);
    gpio_put(verde, 0);
    //vermelho e azul como saida pwm
    configure_pwm(vermelho, &slice_r, level_r);
    configure_pwm(azul, &slice_b, level_b);
}

// Inicializando e configurando o display SSD1306 
void config_display() {
    // Inicializa a comunicação I2C com velocidade de 400 kHz
    i2c_init(I2C_PORT, 400000); 

    // Configurando os pino para função I2C
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);

    // Ativando o resistor pull-up
    gpio_pull_up(I2C_SDA);   
    gpio_pull_up(I2C_SCL); 

    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT); // Inicializa o display SSD1306
    ssd1306_config(&ssd);           // Configura o display
    ssd1306_fill(&ssd, false);      // Limpa
    //ssd1306_send_data(&ssd);        // Atualiza o display
    //ssd1306_fill(&ssd, false);      // Limpa
    ssd1306_rect(&ssd, 1, 1, 122, 58, cor, !cor); 
    ssd1306_send_data(&ssd);        // Atualiza o display
}

void botaoA(bool *led_blue_red){
    *led_blue_red = !(*led_blue_red);
    pwm_set_enabled(slice_r, *led_blue_red);
    pwm_set_enabled(slice_b, *led_blue_red);
}

void botaoJoy(bool *led_green){
    *led_green = !(*led_green);
    gpio_put(verde, *led_green); 
}

// Callback chamado quando ocorre interrupção em algum botão
void botao_callback(uint gpio, uint32_t eventos) {
    // Obtém o tempo atual em us
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) // 200 ms
    {
        //printf("led verde = %d\n", led_green);
        last_time = current_time; // Atualiza o tempo do último evento
        if (gpio == BOTAO_A) { //  Botão A foi pressionado
            //printf("leds= %d\n", led_blue_red);
            botaoA(&led_blue_red);
        }
        else if (gpio == JOYSTICK_PB) { //  Botão A foi pressionado
            botaoJoy(&led_green); 
            cor = !cor; //Alterar a borda
        }
}}

// Função para inicializar os botões
void iniciar() {
    // Inicializa os pinos
    gpio_init(BOTAO_A);
    // Configurando como entrada
    gpio_set_dir(BOTAO_A, GPIO_IN); 
    // Resistor de pull-up
    gpio_pull_up(BOTAO_A);
    // Interrupções para os botões
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, botao_callback);
    
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN); 

    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);

    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_FALL, true, botao_callback); 
}

/* Função para mapear valores do ADC para o display (0-4095 -> 0-120 ou 0-56)
valores max do display corrigidos levando em conta as dimensões do quadrado*/
int mapear(int valor, int in_max, int out_min) {
    return (valor) * ( - out_min) / (in_max) + out_min;
}
  

void display(){
    // Exibição inicial no display OLED
    ssd1306_fill(&ssd, !cor);   // Limpa o display
    ssd1306_rect(&ssd, 2, 2, 125, 61, cor, !cor); // Desenha a borda que vai alterar de acordo com a variavel cor
    
    // Correção para o display
    xquad = mapear(eixo_x, 4095, 120); // Inverte o eixo X 
    yquad = mapear(eixo_y, 4095, 56);  // Inverte o eixo Y 
    
    ssd1306_rect(&ssd, yquad, xquad, 8, 8, quad, !quad); // Desenha o quadrado
    ssd1306_send_data(&ssd); // Atualizar o display
}

void ler_joy(uint16_t *eixo_x, uint16_t *eixo_y){
    // Leitura do valor do eixo X do joystick
    adc_select_input(0); // Seleciona o canal ADC para o eixo X
    sleep_us(20);                     // Pequeno delay para estabilidade
    *eixo_x = adc_read();         // Lê o valor do eixo X (0-4095)
    // Leitura do valor do eixo Y do joystick
    adc_select_input(1); // Seleciona o canal ADC para o eixo Y
    sleep_us(20);                     // Pequeno delay para estabilidade
    *eixo_y = adc_read();         // Lê o valor do eixo Y (0-4095)
}

void led_rb(){
    if ((eixo_x <= centro + 100) && (eixo_x >= centro - 100)) {
        pwm_set_gpio_level(vermelho, 0);
    } else if(eixo_x < centro - 100) {
        pwm_set_gpio_level(vermelho, 4096 - eixo_x); // Ajusta o brilho do LED Vermelho com o valor do eixo X
    }else pwm_set_gpio_level(vermelho, eixo_x);

    if((eixo_y <= centro + 100) && (eixo_y >= centro - 100)) {
        pwm_set_gpio_level(azul, 0);
    } else if(eixo_y < centro - 100) {
        pwm_set_gpio_level(azul, 4096 - eixo_y); // Ajusta o brilho do LED Vermelho com o valor do eixo X
}   else pwm_set_gpio_level(azul, eixo_y);
}

void main() {

    stdio_init_all();
    iniciar();   
    iniciar_rgb();   
    config_display();      
    //printf("ola\n");
    sleep_ms(10); 

// Loop infinito 
while (true) {
    ler_joy(&eixo_x, &eixo_y);
    //printf("x = %d, y = %d\n", xquad, yquad); 
    display();
    led_rb();
    sleep_ms(10);
} return;
}