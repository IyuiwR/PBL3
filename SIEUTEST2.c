#include <16F877A.h>
#fuses HS,NOWDT,NOPROTECT,NOLVP,NOBROWNOUT
#use delay(clock=8000000)
#use i2c(Master, sda=PIN_C4, scl=PIN_C3, force_sw)
#use rs232(baud=9600, xmit=PIN_C6, rcv=PIN_C7, parity=N, bits=8, stop=1)
#use pwm(CCP1, frequency=1000)

#define LCD_I2C_ADDR 0x27
#define MQ3_DIGITAL PIN_B0
#define MQ3_ANALOG PIN_A1
#define PULSE_SENSOR PIN_A0
#define LED_SIGNAL PIN_D2
#define LED_LOW PIN_D0
#define LED_HIGH PIN_D1
#define BUZZER PIN_D4
#define TRIG PIN_B5
#define ECHO PIN_B4
#define IN1 PIN_C0
#define IN2 PIN_C1
#define LED_PULSE PIN_C5
#define EMERGENCY_BUTTON PIN_B1

#define LCD_CLEAR 0x01
#define LCD_HOME 0x02
#define LCD_ENTRY_MODE 0x06
#define LCD_DISPLAY_ON 0x0C
#define LCD_FUNCTION_SET 0x28
#define BACKLIGHT_ON 0x08
#define BACKLIGHT_OFF 0x00

#define CALIBRATION_FACTOR 5.0

float khoangcach = 0;
unsigned int16 soxung;
int8 current_duty = 90;
int1 motor_running = 0;
int1 emergency_pause = 0;
int1 last_button_state = 1;
int1 last_direction = 0;
int8 last_duty = 90;

char message_buffer[64]; // Kích thu?c d? d? ch?a chu?i dài nh?t

void int_to_string(int16 num, char *str) {
    if (num == 0) { str[0] = '0'; str[1] = '\0'; return; }
    char digits[6]; // Ð? cho s? 65535 (5 ch? s? + \0)
    int8 i = 0;
    int16 temp = num;
    while (temp > 0) { digits[i] = (temp % 10) + '0'; temp /= 10; i++; }
    int8 j;
    for (j = 0; j < i; j++) { str[j] = digits[i - 1 - j]; }
    str[j] = '\0';
}

void send_uart_message(char *msg) {
    int i = 0;
    while (msg[i] != '\0' && i < 63) { // Gi?i h?n 63 ký t? (dành 1 cho null terminator)
        putc(msg[i]);
        i++;
    }
    putc('\r');
    putc('\n');
}

void uart_send_number(int16 num) {
    char digits[6]; // Ð? cho s? 65535 (5 ch? s? + \0)
    int_to_string(num, digits);
    int i = 0;
    while (digits[i] != '\0') {
        putc(digits[i]);
        i++;
    }
}

void send_sensor_data(int16 mq3_value, int16 pulse_value) {
    char buffer[20]; // Tang lên 20 d? ch?a "MQ3:1023,P:1023" (15 ký t? + \0 + d? phòng)
    char num_str[6]; // Ð? cho s? 1023 (4 ch? s? + \0)
    int idx = 0;

    buffer[idx++] = 'M'; buffer[idx++] = 'Q'; buffer[idx++] = '3'; buffer[idx++] = ':';
    int_to_string(mq3_value, num_str);
    int i = 0;
    while (num_str[i] != '\0' && idx < 19) { buffer[idx++] = num_str[i++]; }
    if (idx < 19) buffer[idx++] = ','; if (idx < 19) buffer[idx++] = 'P'; if (idx < 19) buffer[idx++] = ':';
    int_to_string(pulse_value, num_str);
    i = 0;
    while (num_str[i] != '\0' && idx < 19) { buffer[idx++] = num_str[i++]; }
    buffer[idx] = '\0';

    send_uart_message(buffer);
}

void clear_uart_buffer() {
    while (kbhit()) getc();
    delay_ms(200);
}

void lcd_i2c_write_byte(unsigned char data, unsigned char backlight) {
    i2c_start();
    if (i2c_write(LCD_I2C_ADDR << 1)) {
        char err_msg[12] = "I2C Err:NoACK";
        send_uart_message(err_msg);
    }
    i2c_write(data | backlight);
    i2c_stop();
    delay_us(100);
}

void lcd_send_cmd(unsigned char cmd) {
    unsigned char data;
    data = (cmd & 0xF0) | 0x04; lcd_i2c_write_byte(data, BACKLIGHT_ON); delay_us(200);
    data = (cmd & 0xF0); lcd_i2c_write_byte(data, BACKLIGHT_ON); delay_us(200);
    data = ((cmd << 4) & 0xF0) | 0x04; lcd_i2c_write_byte(data, BACKLIGHT_ON); delay_us(200);
    data = ((cmd << 4) & 0xF0); lcd_i2c_write_byte(data, BACKLIGHT_ON); delay_us(200);
    if (cmd == LCD_CLEAR || cmd == LCD_HOME) delay_ms(2);
}

void lcd_send_data(unsigned char data) {
    unsigned char temp;
    temp = (data & 0xF0) | 0x05; lcd_i2c_write_byte(temp, BACKLIGHT_ON); delay_us(200);
    temp = (data & 0xF0) | 0x01; lcd_i2c_write_byte(temp, BACKLIGHT_ON); delay_us(200);
    temp = ((data << 4) & 0xF0) | 0x05; lcd_i2c_write_byte(temp, BACKLIGHT_ON); delay_us(200);
    temp = ((data << 4) & 0xF0) | 0x01; lcd_i2c_write_byte(temp, BACKLIGHT_ON); delay_us(200);
}

void lcd_init() {
    delay_ms(100);
    lcd_i2c_write_byte(0x00, BACKLIGHT_ON); delay_ms(20);
    lcd_i2c_write_byte(0x30 | 0x04, BACKLIGHT_ON); delay_ms(10);
    lcd_i2c_write_byte(0x30, BACKLIGHT_ON); delay_ms(10);
    lcd_i2c_write_byte(0x30 | 0x04, BACKLIGHT_ON); delay_ms(10);
    lcd_i2c_write_byte(0x30, BACKLIGHT_ON); delay_ms(10);
    lcd_i2c_write_byte(0x20 | 0x04, BACKLIGHT_ON); delay_ms(10);
    lcd_i2c_write_byte(0x20, BACKLIGHT_ON); delay_ms(10);

    lcd_send_cmd(LCD_FUNCTION_SET); delay_ms(10);
    lcd_send_cmd(LCD_DISPLAY_ON); delay_ms(10);
    lcd_send_cmd(LCD_CLEAR); delay_ms(10);
    lcd_send_cmd(LCD_ENTRY_MODE); delay_ms(10);
}

void lcd_set_cursor(unsigned char col, unsigned char row) {
    unsigned char addr = (row == 0) ? 0x80 : 0xC0;
    addr += col;
    lcd_send_cmd(addr);
}

void lcd_print(char *str) {
    int i = 0;
    while (str[i] != '\0' && i < 16) {
        lcd_send_data(str[i]);
        i++;
        delay_us(200);
    }
}

void lcd_clear_line(unsigned char row) {
    lcd_set_cursor(0, row);
    char clear_str[17] = "                ";
    lcd_print(clear_str);
    lcd_set_cursor(0, row);
}

void display_distance(float value, unsigned char x, unsigned char y) {
    char str[8]; // Tang lên 8 d? ch?a s? l?n hon (ví d?: "123.4" là 6 ký t? + \0)
    lcd_clear_line(y);
    lcd_set_cursor(x, y);
    if (value == -1 || value > 100) {
        char msg[9] = "D:OutRng";
        lcd_print(msg);
    } else if (value < 2) {
        char msg[6] = "D:Err";
        lcd_print(msg);
    } else {
        int whole = (int)value;
        int decimal = (int)((value - whole) * 10);
        int_to_string(whole, str);
        int len = 0; while (str[len] != '\0') len++;
        str[len] = '.'; str[len + 1] = '0' + decimal; str[len + 2] = '\0';
        char prefix[3] = "D:";
        lcd_print(prefix);
        lcd_print(str);
    }
}

void display_mq3_pulse(int mq3_state, int16 pulse_value, unsigned char x, unsigned char y) {
    char str[6]; // Ð? cho s? 1023 (4 ch? s? + \0)
    lcd_clear_line(y);
    lcd_set_cursor(x, y);
    if (mq3_state == 0) {
        char msg[4] = "M:CO";
        lcd_print(msg);
    } else {
        char msg[4] = "M:KO";
        lcd_print(msg);
    }
    int_to_string(pulse_value, str);
    lcd_set_cursor(x + 8, y);
    char prefix[3] = "P:";
    lcd_print(prefix);
    lcd_print(str);
}

void tao_xung_trig() {
    output_low(TRIG); delay_us(2);
    output_high(TRIG); delay_us(10);
    output_low(TRIG);
}

void thoigian_echo() {
    set_timer1(0);
    while (!input(ECHO) && get_timer1() < 30000);
    if (get_timer1() >= 30000) { soxung = 0; return; }
    set_timer1(0);
    while (input(ECHO) && get_timer1() < 30000);
    soxung = get_timer1();
    if (soxung >= 30000) soxung = 0;
}

float measure_distance() {
    int retries = 10;
    float distance = -1;
    for (int i = 0; i < retries; i++) {
        set_timer1(0);
        while (input(ECHO) && get_timer1() < 30000);
        if (get_timer1() >= 30000) { delay_ms(50); continue; }
        tao_xung_trig();
        thoigian_echo();
        if (soxung != 0) {
            distance = (soxung * 0.5 * 0.0343) / 2 * CALIBRATION_FACTOR;
            break;
        }
        delay_ms(50);
    }
    return distance;
}

int16 read_mq3_analog() {
    set_adc_channel(1); delay_us(20);
    int16 sum = 0;
    for (int i = 0; i < 5; i++) { sum += read_adc(); delay_us(100); }
    return sum / 5;
}

int16 read_pulse_analog() {
    set_adc_channel(0); delay_us(20);
    int16 sum = 0; int count = 0;
    for (int i = 0; i < 5; i++) {
        int16 value = read_adc();
        if (value > 0 && value < 1023) { sum += value; count++; }
        delay_us(100);
    }
    return (count == 0) ? 0 : sum / count;
}

void motor_forward() {
    output_low(IN2); output_high(IN1); delay_ms(10);
    int16 duty_value = (int16)current_duty * 1023 / 100;
    setup_ccp1(CCP_PWM); set_pwm1_duty(duty_value);
    motor_running = 1; last_direction = 0;
    char msg[13] = "Motor:Forward";
    send_uart_message(msg);
}

void motor_reverse() {
    output_low(IN1); output_high(IN2); delay_ms(10);
    int16 duty_value = (int16)current_duty * 1023 / 100;
    setup_ccp1(CCP_PWM); set_pwm1_duty(duty_value);
    motor_running = 1; last_direction = 1;
    char msg[13] = "Motor:Reverse";
    send_uart_message(msg);
}

void motor_stop() {
    output_low(IN1); output_low(IN2);
    set_pwm1_duty(0); setup_ccp1(CCP_OFF);
    motor_running = 0;
    char msg[10] = "Motor:Stop";
    send_uart_message(msg);
}

void set_low_speed() {
    current_duty = 20;
    int16 duty_value = (int16)current_duty * 1023 / 100;
    setup_ccp1(CCP_PWM); set_pwm1_duty(duty_value);
    if (motor_running) {
        if (last_direction == 0) motor_forward();
        else motor_reverse();
    }
    char msg[14] = "Speed:Low(20%)";
    send_uart_message(msg);
}

void set_high_speed() {
    current_duty = 90;
    int16 duty_value = (int16)current_duty * 1023 / 100;
    setup_ccp1(CCP_PWM); set_pwm1_duty(duty_value);
    if (motor_running) {
        if (last_direction == 0) motor_forward();
        else motor_reverse();
    }
    char msg[15] = "Speed:High(90%)";
    send_uart_message(msg);
}

void main() {
    set_tris_a(0b00000011);
    set_tris_b(0b00110011);
    set_tris_d(0b00000000);
    set_tris_c(0b10011000);

    port_b_pullups(TRUE);
    setup_ccp2(CCP_OFF); setup_spi(FALSE);
    setup_adc_ports(A_ANALOG); setup_adc(ADC_CLOCK_DIV_32);
    setup_timer_1(T1_INTERNAL | T1_DIV_BY_4);
    setup_ccp1(CCP_PWM); setup_timer_2(T2_DIV_BY_4, 124, 1);

    output_low(TRIG); output_low(LED_SIGNAL);
    output_low(LED_LOW); output_low(LED_HIGH);
    output_low(BUZZER); output_low(IN1);
    output_low(IN2); output_high(LED_PULSE);

    char start_msg[20] = "BAT DAU CHUONG TRINH";
    send_uart_message(start_msg);

    for (int i = 0; i < 3; i++) {
        output_high(LED_SIGNAL); output_high(BUZZER); delay_ms(100);
        output_low(LED_SIGNAL); output_low(BUZZER); delay_ms(100);
    }

    delay_ms(2000);

    lcd_init();
    char lcd_msg1[14] = "LCD 16x2 Test";
    lcd_set_cursor(0, 0); lcd_print(lcd_msg1);
    char lcd_msg2[9] = "Working!";
    lcd_set_cursor(0, 1); lcd_print(lcd_msg2);
    delay_ms(2000); lcd_send_cmd(LCD_CLEAR);

    motor_stop(); delay_ms(200); clear_uart_buffer();

    unsigned long loop_counter = 0;
    unsigned long update_interval = 20;
    unsigned long uart_interval = 50;
    unsigned long distance_alert_counter = 0; // Ð?m s? vòng l?p d? ki?m tra 3 giây
    int8 current_alert_zone = 0; // 0: Không c?nh báo, 1: 120-100cm, 2: 100-20cm, 3: Du?i 20cm
    int8 last_alert_zone = 0; // Theo dõi vùng c?nh báo tru?c dó d? g?i UART 1 l?n
    int1 alert_triggered = 0; // Ðánh d?u xem c?nh báo dã du?c kích ho?t chua

    while (TRUE) {
        int1 current_button_state = input(EMERGENCY_BUTTON);
        if (current_button_state == 0 && last_button_state == 1) {
            delay_ms(50);
            if (input(EMERGENCY_BUTTON) == 0) {
                emergency_pause = !emergency_pause;
                if (emergency_pause) {
                    if (motor_running) last_duty = current_duty;
                    motor_stop(); output_low(LED_SIGNAL);
                    output_low(LED_LOW); output_low(LED_HIGH);
                    output_low(BUZZER); lcd_send_cmd(LCD_CLEAR);
                    char pause_msg[15] = "Emergency Pause";
                    lcd_set_cursor(0, 0); lcd_print(pause_msg);
                    send_uart_message(pause_msg);
                } else {
                    lcd_send_cmd(LCD_CLEAR);
                    char resume_msg[12] = "System Resume";
                    send_uart_message(resume_msg);
                    if (last_duty != 0) {
                        current_duty = last_duty;
                        if (last_direction == 0) motor_forward();
                        else motor_reverse();
                    }
                }
            }
        }
        last_button_state = current_button_state;

        if (!emergency_pause) {
            if (kbhit()) {
                char command = getc();
                if (command >= 'A' && command <= 'Z') {
                    switch (command) {
                        case 'F': motor_forward(); break;
                        case 'R': motor_reverse(); break;
                        case 'S': motor_stop(); break;
                        case 'L': set_low_speed(); break;
                        case 'H': set_high_speed(); break;
                        default:
                            char err_msg[13] = "Err:InvalidCmd";
                            send_uart_message(err_msg);
                            break;
                    }
                }
                clear_uart_buffer();
            }

            int mq3_digital_state = input(MQ3_DIGITAL);
            delay_us(10);
            int16 mq3_analog_value = read_mq3_analog();
            int16 pulse_value = read_pulse_analog();
            khoangcach = measure_distance();

            if (loop_counter % update_interval == 0) {
                display_mq3_pulse(mq3_digital_state, pulse_value, 0, 0);
                display_distance(khoangcach, 0, 1);
            }

            if (loop_counter % uart_interval == 0) {
                putc('D'); putc('i'); putc('s'); putc('t'); putc(':');
                if (khoangcach == -1 || khoangcach > 100) {
                    char outrange_msg[8] = "OutRng";
                    send_uart_message(outrange_msg);
                } else if (khoangcach < 2) {
                    char error_msg[5] = "Err";
                    send_uart_message(error_msg);
                } else {
                    int whole = (int)khoangcach;
                    int decimal = (int)((khoangcach - whole) * 1);
                    char khoang_str[8]; // Tang lên 8 d? ch?a s? l?n (ví d?: "123.4 cm" = 8 ký t?)
                    int_to_string(whole, khoang_str);
                    int len = 0; while (khoang_str[len] != '\0') len++;
                    khoang_str[len] = '.'; khoang_str[len + 1] = '0' + decimal; khoang_str[len + 2] = '\0';
                    int i = 0; while (khoang_str[i] != '\0') { putc(khoang_str[i]); i++; }
                    putc(' '); putc('c'); putc('m');
                }
                send_sensor_data(mq3_analog_value, pulse_value);
            }

            // Logic c?nh báo kho?ng cách
            if (motor_running && current_duty == 90) {
                // Xác d?nh vùng kho?ng cách hi?n t?i
                if (khoangcach >= 100 && khoangcach <= 120) {
                    current_alert_zone = 1; // Vùng 120-100cm
                } else if (khoangcach >= 20 && khoangcach < 100) {
                    current_alert_zone = 2; // Vùng 100-20cm
                } else if (khoangcach < 20 && khoangcach >= 2) {
                    current_alert_zone = 3; // Vùng du?i 20cm
                } else {
                    current_alert_zone = 0; // Không có c?nh báo
                    distance_alert_counter = 0;
                    alert_triggered = 0;
                }

                // Ð?m th?i gian trong vùng
                if (current_alert_zone == last_alert_zone && current_alert_zone != 0) {
                    distance_alert_counter++;
                } else {
                    distance_alert_counter = 0;
                    alert_triggered = 0;
                }

                // Kích ho?t c?nh báo sau 3 giây (300 vòng l?p v?i delay 10ms)
                if (distance_alert_counter >= 300 && !alert_triggered) {
                    if (current_alert_zone == 1) {
                        // C?nh báo 1 l?n cho vùng 120-100cm
                        char warn_msg[18] = "Warn:Dist120-100cm";
                        send_uart_message(warn_msg);
                        output_high(BUZZER); delay_ms(200);
                        output_low(BUZZER);
                    } else if (current_alert_zone == 2) {
                        // C?nh báo 3 l?n cho vùng 100-20cm
                        char warn_msg[17] = "Warn:Dist100-20cm";
                        send_uart_message(warn_msg);
                        for (int i = 0; i < 3; i++) {
                            output_high(BUZZER); delay_ms(200);
                            output_low(BUZZER); delay_ms(200);
                        }
                    } else if (current_alert_zone == 3) {
                        // C?nh báo liên t?c cho vùng du?i 20cm
                        char warn_msg[13] = "Warn:Dist<20cm";
                        send_uart_message(warn_msg);
                    }
                    alert_triggered = 1;
                }

                // Còi báo liên t?c n?u v?n ? vùng du?i 20cm
                if (current_alert_zone == 3 && alert_triggered) {
                    output_high(BUZZER); delay_ms(100);
                    output_low(BUZZER); delay_ms(100);
                }

                last_alert_zone = current_alert_zone;
            } else {
                // N?u không ch?y m?c High, reset các bi?n c?nh báo
                distance_alert_counter = 0;
                current_alert_zone = 0;
                last_alert_zone = 0;
                alert_triggered = 0;
                output_low(BUZZER);
            }

            // Logic hi?n t?i cho LED và BUZZER (MQ3 và kho?ng cách du?i 20cm khi m?c High)
            if (mq3_digital_state == 0) {
                output_high(LED_SIGNAL); output_high(BUZZER);
                delay_ms(100);
                output_low(LED_SIGNAL);
                output_low(BUZZER);
            } else if (motor_running && current_duty == 90 && khoangcach < 20 && khoangcach >= 2) {
                // Ðã x? lý trong logic c?nh báo ? trên
            } else {
                output_low(LED_SIGNAL);
            }

            if (motor_running && current_duty == 90) {
                output_high(LED_HIGH); output_low(LED_LOW);
            } else if (motor_running && current_duty == 20) {
                output_high(LED_LOW);
                output_low(LED_HIGH);
            } else {
                output_low(LED_HIGH); output_low(LED_LOW);
            }

            if (pulse_value > 700) {
                char warn_msg[11] = "Warn:Pulse!";
                send_uart_message(warn_msg);
            }

            loop_counter++;
            delay_ms(10);
        }
    }
}
