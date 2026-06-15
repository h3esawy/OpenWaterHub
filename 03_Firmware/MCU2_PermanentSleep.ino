/*
 * =================================================================================
 * PROJECT: OpenWater Hub - Permanent Coma Firmware for MCU 2 (ATmega128A)
 * DESCRIPTION: Kills RS485 Transmitter (PD4), builds High-Z walls against LED 
 * circuits (PD2/PD3) and external pull-ups (PE0), and floats SPI pins safely.
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

    // --- 3. HARDWARE-SPECIFIC PIN ANCHORING ---
    
    // PORT A, C, F, G: All unused. Pull them all up safely to prevent floating.
    DDRA = 0x00; PORTA = 0xFF; 
    DDRC = 0x00; PORTC = 0xFF;
    DDRF = 0x00; PORTF = 0xFF;
    DDRG = 0x00; PORTG = 0xFF;

    // PORT B: PB0 (SPI) and PB1 (connected). 
    // Set to INPUT (0) and NO pull-up (0) to float safely on the bus.
    DDRB = 0x00; 
    PORTB = 0xFF & ~((1 << PB0) | (1 << PB1));

    // --- PORT D: THE RS485 & LED FIX ---
    // PD2 (RX + LED to VCC) = INPUT, NO PULL-UP (High-Z Wall blocks LED current)
    // PD3 (TX + LED to VCC) = INPUT, NO PULL-UP (High-Z Wall blocks LED current)
    // PD4 (DE/RE) = OUTPUT LOW (CRITICAL: Forces RS485 chip into low-power receive mode)
    DDRD = (1 << PD4); 
    PORTD = 0xFF & ~((1 << PD2) | (1 << PD3) | (1 << PD4));

    // --- PORT E: THE HARDWARE PULL-UP & SPI FIX ---
    // PE0 (10k to VCC) = INPUT, NO PULL-UP (High-Z Wall blocks 10k current)
    // PE1 (SPI) = INPUT, NO PULL-UP (Float safely on SPI bus)
    DDRE = 0x00; 
    PORTE = 0xFF & ~((1 << PE0) | (1 << PE1));

    // --- 4. ENTER PERMANENT COMA ---
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    cli(); // Disable interrupts so it can never wake up
    sleep_enable();
    sleep_cpu(); 
}

void loop() {
    // Dead.
}