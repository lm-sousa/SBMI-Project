#include <motor_control.h>

void initMotors () {

    for (int i = 0; i < HISTORY_SIZE; i++) {
        velocity[i] = 0;
    }
    _M1_encoder_counter = 0;
    _M2_encoder_counter = 0;

    TCCR0B = 0; // Stop the Timer
    TCCR0A = (1<<COM0A1) | (0<<COM0A0) | (1<<COM0B1) | (0<<COM0B0) | (0<<WGM01) | (1<<WGM00); // Set the Timer to waveform generation mode 1.
    TIMSK0 = 0; // Disable all interrupt calls
    _initAuxPins();
    setSpeed(0, 0);  // Don't start the motor running

    /* Finish the Phase-Correct PWM configuration and start the Timer with a 1024 prescaler */
    TCCR0B = (0<<FOC0A) | (0<<FOC0B) | (0<<WGM02) | (1<<CS02) | (0<<CS01) | (1<<CS00);
}

void _initAuxPins () {

    /* Motor 1 */
    #ifdef M1_DIRECTION_PIN
    DDRD |= (1<<M1_DRIVE_PIN) | (1<<M1_DIRECTION_PIN);  // Set as output
    PORTD &= ~(1<<M1_DIRECTION_PIN);
    #endif

    #ifdef M1_BRAKE_PIN
        DDRD |= (1<<M1_BRAKE_PIN);  // Set as output
    #endif


    /* Motor 2 */
    #ifdef M2_DIRECTION_PIN
    DDRD |= (1<<M2_DRIVE_PIN) | (1<<M2_DIRECTION_PIN);  // Set as output
    PORTD &= ~(1<<M2_DIRECTION_PIN);
    #endif

    #ifdef M2_BRAKE_PIN
        DDRD |= (1<<M2_BRAKE_PIN);  // Set as output
    #endif
}


void setSpeed (int speed1, int speed2) {
    
    #ifdef M1_DIRECTION_PIN
    OCR0A = 0;

    if ((speed1 < 0 && M1_INVERTED) || (speed1 >= 0 && !M1_INVERTED)) {
        PORTD &= ~(1<<M1_DIRECTION_PIN);
    }
    else {
        PORTD |= (1<<M1_DIRECTION_PIN);
    }
    #endif

    #ifdef M2_DIRECTION_PIN
    OCR0B = 0;

    if ((speed2 < 0 && M2_INVERTED) || (speed2 >= 0 && !M2_INVERTED)) {
        PORTD &= ~(1<<M2_DIRECTION_PIN);
    }
    else {
        PORTD |= (1<<M2_DIRECTION_PIN);
    }
    #endif

    if (speed1 < 0) {
        speed1 = -speed1;
    }
    if (speed2 < 0) {
        speed2 = -speed2;
    }

    OCR0A = (uint8_t)(speed1*255/100);
    OCR0B = (uint8_t)(speed2*255/100);

    #ifdef M1_BRAKE_PIN
    if (speed1 == 0)
        PORTD |= (1<<M1_BRAKE_PIN);
    else
        PORTD &= ~(1<<M1_BRAKE_PIN);
    #endif

    #ifdef M2_BRAKE_PIN
    if (speed2 == 0)
        PORTD |= (1<<M2_BRAKE_PIN);
    else
        PORTD &= ~(1<<M2_BRAKE_PIN);
    #endif
}

void updateOdometry (uint16_t millis) {
    for (int i = HISTORY_SIZE-1; i > 0; i--) {
        velocity[i] = velocity[i-1];
    }

    cli(); // Prevent interrupt call from changing the counter's values
    velocity[0] = (DPC * ((_M1_encoder_counter + _M2_encoder_counter)/2.0))/millis; // Average the velocity of the 2 motors
    _M1_encoder_counter = 0;
    _M2_encoder_counter = 0;
    
    sei(); // Re-enable interrupt calls
}