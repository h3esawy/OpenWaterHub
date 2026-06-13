/*
 * =================================================================================
 * PROJECT: OpenWater Hub - Permanent Coma Firmware for MCU 1 (ATmega128A)
 * DESCRIPTION: Ultra-simple power profile. Buzzer is anchored LOW. 
 * ALL other pins are pulled HIGH. This perfectly protects unplugged headers 
 * (ICSP/LoRa) from floating, and draws 0mA against external 10k pull-ups.
 * =================================================================================
 */

#include <avr/sleep.h>
#include <avr/wdt.h>

void setup() {
    // --- 1. KILL THE WATCHDOG ---
    MCUCSR = 0;    
    wdt_disable(); 

    // --- 2. DISABLE ANALOG HARDWARE ---
    ADCSRA = 0;           
    ACSR = (1 << ACD);    

    // --- 3. ULTRA-SIMPLE PIN LOCKDOWN ---
    
    // PORT A: PA3 (Buzzer) OUTPUT LOW (0V). All other pins pulled-up.
    DDRA = (1 << PA3);
    PORTA = 0xFF & ~(1 << PA3); 

    // PORTS B, C, D, E, F, G: All configured as INPUTS with internal PULL-UPS (5V).
    // This safely covers the ICSP headers, LoRa headers, and external button resistors.
    DDRB = 0x00; PORTB = 0xFF; 
    DDRC = 0x00; PORTC = 0xFF;
    DDRD = 0x00; PORTD = 0xFF; 
    DDRE = 0x00; PORTE = 0xFF;
    DDRF = 0x00; PORTF = 0xFF;
    DDRG = 0x00; PORTG = 0xFF;

    // --- 4. ENTER PERMANENT COMA ---
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli(); 
    sleep_enable();
    sleep_cpu(); 
}

void loop() {
    // Dead.
}