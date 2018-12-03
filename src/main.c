/*
 *  main.c
 *
 *  Created on: Oct 29, 2018
 *      Author: Luís Sousa, Leonor Santos
 */

#ifndef F_CPU
#define F_CPU 16000000
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include <motor_control.h>
#include <BTcomms.h>
#include <IRArray.h>


/*******************************/
/*                             */
/*    Generic configuration    */
/*                             */
/*******************************/

//#define DEBUG
//#define DEMO

#define INITIAL_STATE 0
#define BREAKDOWN_ENTRY_STATE 0
#define ODOMETRY_TIME_RESOLUTION 1000

uint8_t state = 0;                  // Current state of the state-machine
uint8_t nstate = 0;                 // Next state of the state-machine
volatile uint8_t istate = 0;        // Variable to prevent race-conditions to the state-machine's 'state'.
volatile uint8_t i_flag = FALSE;    // Flag to tell the main-loop if a request is made by a interrupt to change the current state-machine's state
uint16_t odometryTime = 0;
uint8_t mcucr_copy;
volatile char lastReceivedChar = 0;
volatile uint8_t newCharFlag = FALSE;

/**
 * @brief If a Inturrupt is called and has no ISR associated, this function is called, instead of a system reset occuring
 * 
 */
ISR(BADISR_vect) {
    istate = BREAKDOWN_ENTRY_STATE;
    i_flag = TRUE;
}

/**
 * @brief Setups all External Interruptions
 * 
 */
void initExternalInterrupts() {
    EICRA |= (1<<ISC11) | (1<<ISC10) | (1<<ISC01) | (1<<ISC00); // Both external Interrupt calls at RE
    EIMSK |= (1 << M1_ENCODER_INTERRUPT_PIN) | (1 << M2_ENCODER_INTERRUPT_PIN); // Enable the external interrupt calls
    EIFR |= (1<<INTF1) | (1<<INTF0);    // Clear Flag Register by writting '1's to the corresponding positions
}

/**
 * @brief M1_ENCODER_INTERRUPT_PIN Interrupt Call
 * 
 */
ISR(INT0_vect) {
    _M1_encoder_counter += 1;
}

/**
 * @brief M2_ENCODER_INTERRUPT_PIN Interrupt Call
 * 
 */
ISR(INT1_vect) {
    _M2_encoder_counter += 1;
}

/**
 * @brief Saves important variables to EEPROM
 * 
 */
void emergencySaveToEEPROM() {
    // TODO: Save important variables to EEPROM
}

/**
 * @brief Enables a 0.5 second Watchdog Timer that trigger both the Interrupt Call followed by a System Reset
 * 
 */
void initWatchdogTimer() {
    wdt_reset();                    // 'Pet the Dog'
    WDTCSR |= (1<<WDCE) | (1<<WDE); // Unlock prescaler changes for 4 clock cycles
    /* Set Timeout to 0.5 seconds, clear Interrupt flag and enable both Interrupt and system reset modes */
    WDTCSR = (1<<WDIF) | (1<<WDE) | (1<<WDIE) | (1<<WDP2) | (1<<WDP0);
}

/**
 * @brief Disables the Watchdog Timer. Both the Interrupt and System Reset Enable bits are also disabled.
 * 
 */
void disableWatchdogTimer() {
    wdt_reset();            // 'Pet the Dog'
    MCUSR &= ~(1<<WDRF);    // Datasheet Recomendation to prevent time-out loops
    /* Write logical one to WDCE and WDE */
    /* Keep old prescaler setting to prevent unintentional time-out */
    WDTCSR |= (1<<WDCE) | (1<<WDE);
    /* Turn off WDT */
    WDTCSR = 0x00;
}

/**
 * @brief Interrupt Service Routine for when the Watchdog Timer is triggered
 * 
 */
ISR(WDT_vect) {
    emergencySaveToEEPROM();
}

/**
 * @brief Callback function to handle the byte received by the UART interface
 * 
 * @param rxChar byte received by the UART interface
 */
void handleReceivedByte(char rxChar) {
    lastReceivedChar = rxChar;
    newCharFlag = TRUE;
}

/**
 * @brief Initializes Timer1 to trigger a interrupt every 10ms (TIMER1_COMPA_vect)
 * 
 */
void init_TC1_10ms() {
    TCCR1B = 0;                         // STOP the timer
    TIFR1 = (7<<TOV1) | (1<<ICF1);      // Clear flag register
    TCCR1A = 0;                         // Set mode...
    TCCR1B = (1<<WGM12);                // ...to CTC
    OCR1A = 625;                        // OCR1A
    TIMSK1 = (1<<OCIE1A);               // Enable interrupt on compare with OCR1A
    TCCR1B |= 4;                        // Set the TP to 256, therefore also starting the timer
}

/**
 * @brief Handles the countdown of all application timers.
 * 
 * Supposedly, it is triggered every 10ms.
 * 
 */
ISR(TIMER1_COMPA_vect) {
    if (!odometryTime) {
        odometryTime -= 10;
    }
}

/**
 * @brief Condenses all hardware configurations to one function call
 * 
 */
void initHardware() {
    cli(); // Disable all interrupts during hardware configuration

    initExternalInterrupts();
    initMotors();
    initWatchdogTimer();
    setReceiveCallback(&handleReceivedByte);
    initBTComms();
    init_TC1_10ms();
    initArrayHardware();

    /* Activate Interrupts */
    sei();
}


/**
 * @brief Disables unused peripherals to save power.
 * 
 */
void disableUnusedHardware() {
    // TODO: Disable unused peripherals to save power.
}

int main() {
    mcucr_copy = MCUCR^0b00001111;
    cli();
    disableWatchdogTimer();

    /* Check the reason for the reset */
    switch(mcucr_copy) {
        case PORF:  // Power-On Reset Flag
            // TODO: Check if the siganture is present. If not, prime the EEPROM
            break;
        case EXTRF: // External Reset Flag
            // TODO: Same as PORF???? IDK... yet.
            break;
        case BORF:  // Brown-out Reset Flag
            emergencySaveToEEPROM();
            break;
        case WDRF:  // Watchdog System Reset Flag
            // TODO: Either go into Breakdown mode or log the reset to the EEPROM
            // If needed, read saved data from EEPROM
            break;
        default:
            state = BREAKDOWN_ENTRY_STATE;
            break;
    }
    MCUCR = 0; // Clear the Reset Flag Register

    /* Hardware and Timer Initializations */
    initHardware();
    disableUnusedHardware();
    odometryTime = ODOMETRY_TIME_RESOLUTION; // Initialize counter

    /* Main Program Loop */
    while(1) {
        wdt_reset();    // 'Pet the Dog'

        if (i_flag) {
            state = istate;
            i_flag = FALSE;
        }

        if (!odometryTime) {
            updateOdometry(ODOMETRY_TIME_RESOLUTION);   // Update Odometry values
            odometryTime = ODOMETRY_TIME_RESOLUTION;    // Reset the time variable
        }
        
        /* State Machine */
        switch(state) {
            case 0:
                setSpeed(100,100);
                break;
            case 1:
                break;
            default:
                nstate = BREAKDOWN_ENTRY_STATE;
                break;
        }

        state = nstate; // Update 'state' variable
    }
    return 0;
}