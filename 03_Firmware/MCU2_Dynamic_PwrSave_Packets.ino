/*
 * =================================================================================
 * PROJECT: OpenWater Hub - ICSP Deep Sleep Modbus Firmware (ATmega128A)
 * DESCRIPTION: Uses a Watchdog Timer (WDT) and a persistent RAM flag to achieve 
 * deep power savings. Includes a unified configuration block for easy editing.
 * UPLOAD: MUST be flashed via ICSP (USBasp) to bypass the standard bootloader!
 * =================================================================================
 */

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h> 

// =========================================================================
// === 1. UNIFIED SYSTEM CONFIGURATION =====================================
// =========================================================================

// ---> EDIT YOUR TARGET SLEEP TIME HERE (IN SECONDS) <---
#define SLEEP_TIME_SECONDS 30  

// Set to 'true' to print a dot (.) every 2 seconds so you know it isn't dead. 
// Set to 'false' for absolute maximum silence.
#define SHOW_SLEEP_HEARTBEAT true 

// Pin controlling the RS485 Transceiver mode (HIGH = Transmit, LOW = Listen/Sleep)
#define RS485_DE_RE 22

// The code automatically calculates how many 2.1-second WDT cycles are needed
const uint8_t TARGET_CYCLES = (SLEEP_TIME_SECONDS / 2);

/* =========================================================================
   =================== THE PRE-BOOT WATCHDOG KILLER ========================
   ========================================================================= */
// HARDWARE HACK: Runs automatically BEFORE the Arduino framework boots.
// Wipes the MCUCSR hardware lock so the Watchdog can actually be turned off.
// Without this, the chip traps itself in a 15-millisecond infinite reboot loop.
void wdt_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void wdt_init(void)
{
    MCUCSR = 0;    
    wdt_disable(); 
}

/* =========================================================================
   ====================== PERSISTENT SURVIVAL MEMORY =======================
   ========================================================================= */
// EXPLANATION: When the Watchdog resets the chip, the battery is still plugged in, 
// so the physical RAM never loses power. The .noinit attribute tells the C++ 
// compiler's startup routine NOT to overwrite this specific memory address with 0. 
// This allows our sleep counter to safely survive the hardware reboot.
uint8_t __attribute__((section(".noinit"))) wdt_cycle_count;

/* =========================================================================
   ====================== DYNAMIC SENSOR CONFIGURATION =====================
   ========================================================================= */

// 1. The Blueprint: Defines all the properties a Sensor needs to function.
struct Sensor {
    uint8_t slaveID;         // The Modbus ID of the sensor (e.g., 0x01)
    uint16_t registerAddr;   // The Modbus Starting Register to read
    float scaleFactor;       // What to divide the raw integer by (10.0, 100.0, etc.)
    uint8_t displayDecimals; // How many decimals to send to the ESP (1 or 2)
    float currentValue;      // [Internal] Stores the latest reading
    bool readSuccess;        // [Internal] Did it read successfully this cycle?
    uint8_t retryCount;      // [Internal] How many times have we tried to read it?
};

// 2. The Sensor List: This is where future users configure the system.
// HOW TO ADD A SENSOR: Add a comma at the end of the last sensor, 
// and insert a new line with the Modbus details. The code automatically 
// detects it, polls it, and adds it to the CSV output string.
Sensor sensors[] = {
    {0x01, 0x0001, 10.0,  1, -1.0, false, 0}, // Index 0: Temperature
    {0x02, 0x0002, 100.0, 2, -1.0, false, 0}, // Index 1: pH
    {0x03, 0x0002, 100.0, 2, -1.0, false, 0}  // Index 2: Dissolved Oxygen

    // --- EXAMPLE OF ADDING A NEW 4TH SENSOR ---
    // ,{0x04, 0x0005, 10.0, 1, -1.0, false, 0} // Index 3: Soil Moisture
};

// Automatically calculates how many sensors are in the array above.
const uint8_t NUM_SENSORS = sizeof(sensors) / sizeof(sensors[0]);

// Maximum attempts per sensor before reporting -1.0 (Error) to save power
const uint8_t MAX_RETRIES = 3; 

// Permanent memory tracker for data loss calculation
uint32_t packet_ID = 0; 

/* =========================================================================
   ========================= POWER MANAGEMENT LOGIC ========================
   ========================================================================= */

void enterDeepSleepWDT() 
{
    // Ensure RS485 transceiver is entirely shut off to prevent battery leakage
    digitalWrite(RS485_DE_RE, LOW); 
    
    // Configure the Watchdog Timer for ~2.1 Seconds (The max for ATmega128A)
    cli();
    wdt_reset();
    WDTCR |= (1<<WDCE) | (1<<WDE);
    // WDP2, WDP1, WDP0 set to 1 = ~2.1 seconds at 5V
    WDTCR = (1<<WDE) | (1<<WDP2) | (1<<WDP1) | (1<<WDP0);
    sei();

    // Go to absolute deepest sleep. The MCU will freeze here until the WDT kills it.
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu(); 
}

/* =========================================================================
   ================================= SETUP =================================
   ========================================================================= */
void setup()
{
    // --- 1. HARDWARE LOCKDOWN ---
    // This MUST run every single reboot to prevent pins from floating.
    Serial.begin(9600);   
    Serial1.begin(9600);  
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW); 
    delay(50); // The critical 50ms needed for hardware to stabilize

    // --- 2. RAM SANITY CHECK (COLD BOOT LOGIC) ---
    // If the battery was physically removed, the RAM is garbage.
    if (wdt_cycle_count > TARGET_CYCLES) {
        wdt_cycle_count = 0;
        Serial.println("\n[SYSTEM] COLD BOOT: Power connected.");
        Serial.print("[SYSTEM] Starting "); 
        Serial.print(SLEEP_TIME_SECONDS);
        Serial.println("-second deep sleep cycle...");
        Serial.flush(); 
    }

    // --- 3. THE SLEEP GATEKEEPER ---
    if (wdt_cycle_count < TARGET_CYCLES) {
        wdt_cycle_count++;        // Increment the persistent memory tracker
        
        if (SHOW_SLEEP_HEARTBEAT) {
            Serial.print(".");    // Print a tiny dot to prove it's alive
            Serial.flush();
        }
        
        enterDeepSleepWDT();      // Chip goes to sleep and restarts
    }

    // --- 4. WAKE UP & INITIALIZE ---
    // If we reach this line, the target sleep time has passed!
    wdt_cycle_count = 0; 
    
    if (SHOW_SLEEP_HEARTBEAT) {
        Serial.println(); // Push to a new line after the dots
    }

    // Retrieve the total packets sent from the permanent EEPROM (Address 0)
    EEPROM.get(0, packet_ID);
    if (packet_ID == 0xFFFFFFFF) packet_ID = 0; 
    
    // Give the sensors 1 second to stabilize before we aggressively poll them
    delay(1000); 
}

/* =========================================================================
   ======================= MODBUS & RS485 CORE LOGIC =======================
   ========================================================================= */
   
uint16_t Modbus_CRC16(uint8_t *buffer, uint8_t length)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) { crc >>= 1; crc ^= 0xA001; }
            else { crc >>= 1; }
        }
    }
    return crc;
}

void RS485_Transmit(uint8_t *data, uint8_t len)
{
    digitalWrite(RS485_DE_RE, HIGH);         
    delayMicroseconds(200);                  
    for (uint8_t i = 0; i < len; i++) { Serial1.write(data[i]); }
    Serial1.flush();                         
    delayMicroseconds(200);                  
    digitalWrite(RS485_DE_RE, LOW);          
}

uint8_t RS485_Receive(uint8_t *buffer, uint8_t len)
{
    uint32_t start = millis();
    uint8_t count = 0;
    
    // 500ms timeout budget. If a sensor is dead, we don't wait forever.
    while ((count < len) && ((millis() - start) < 500)) {
        if (Serial1.available()) { buffer[count++] = Serial1.read(); }
    }
    return count;
}

void RS485_ClearBuffer() { while (Serial1.available()) { Serial1.read(); } }

void Modbus_BuildRequest(uint8_t *frame, uint8_t slave, uint8_t function, uint16_t reg, uint16_t count)
{
    frame[0] = slave; frame[1] = function;
    frame[2] = (reg >> 8) & 0xFF; frame[3] = reg & 0xFF;
    frame[4] = (count >> 8) & 0xFF; frame[5] = count & 0xFF;
    uint16_t crc = Modbus_CRC16(frame, 6);
    frame[6] = crc & 0xFF; frame[7] = (crc >> 8) & 0xFF;
}

bool Modbus_ReadRegister(uint8_t slave, uint16_t reg, uint16_t *value)
{
    uint8_t request[8]; uint8_t response[7];
    RS485_ClearBuffer(); 
    Modbus_BuildRequest(request, slave, 0x03, reg, 0x0001);
    RS485_Transmit(request, 8);

    if (RS485_Receive(response, 7) != 7) return false;
    
    uint16_t receivedCRC = ((uint16_t)response[6] << 8) | response[5];
    uint16_t calculatedCRC = Modbus_CRC16(response, 5);
    if (receivedCRC != calculatedCRC) return false;
    
    if (response[0] != slave || response[1] != 0x03 || response[2] != 0x02) return false;

    *value = ((uint16_t)response[3] << 8) | response[4];
    return true;
}

/* =========================================================================
   ============================== MAIN LOOP ================================
   ========================================================================= */

void loop()
{
    Serial.println("\n--- [ACTIVE] Polling Sensors ---");

    // --- 1. POLL ALL SENSORS ---
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        sensors[i].currentValue = -1.0;
        sensors[i].readSuccess = false;
        sensors[i].retryCount = 0;
        
        while (!sensors[i].readSuccess && sensors[i].retryCount < MAX_RETRIES) {
            uint16_t rawData = 0;
            if (Modbus_ReadRegister(sensors[i].slaveID, sensors[i].registerAddr, &rawData)) {
                sensors[i].currentValue = rawData / sensors[i].scaleFactor;
                sensors[i].readSuccess = true;
            } else {
                sensors[i].retryCount++;
                delay(50); 
            }
        }
    }

    // --- 2. TRANSMIT CSV DATA TO ESP ---
    Serial.println("--- [TRANSMIT] Sending Telemetry to ESP ---");
    
    packet_ID++; 
    EEPROM.put(0, packet_ID); 

    Serial.print(packet_ID);
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        Serial.print(",");
        if (sensors[i].readSuccess) {
            Serial.print(sensors[i].currentValue, sensors[i].displayDecimals);
        } else {
            Serial.print("-1.0"); 
        }
    }
    Serial.println("_"); 

   // --- 3. LISTEN FOR DASHBOARD COMMANDS ---
    Serial.println("--- [LISTENING] Waiting 1.5s for Cloud Commands ---");
    
    // 1. Wipe the initial echo garbage caused by printing the line above
    while (Serial.available() > 0) {
        Serial.read(); 
    }

    // 2. Start the clean 1.5-second listening timer
    uint32_t listenStart = millis();
    
    while (millis() - listenStart < 1500) {
        if (Serial.available() > 0) {  
            String incomingCmd = Serial.readStringUntil('\n'); 
            incomingCmd.trim(); 
            
            
            if (incomingCmd.indexOf("CMD_RESET_PACKET_ID") >= 0) {
                // We only print a response if we actually got the real command
                Serial.println(">>> SUCCESS! Wiping Packet ID to 0...");
                packet_ID = 0;
                EEPROM.put(0, packet_ID); 
                break; 
            }
        }
    }

    // --- 4. GO TO SLEEP ---
    Serial.print("--- [SLEEP] Shutting Down for ");
    Serial.print(SLEEP_TIME_SECONDS);
    Serial.println(" Seconds ---");
    
    Serial.flush(); 
    enterDeepSleepWDT(); 
}