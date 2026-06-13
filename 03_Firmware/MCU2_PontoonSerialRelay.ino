/*
 * ===================================================================================
 * Pontoon Serial Relay / Bridge with RS485 Control
 * ===================================================================================
 *
 * AUTHORS:
 * Hany A. Elesawy and Ahmed I. Ahmed
 *
 * PURPOSE:
 * This program runs on the microcontroller located on the pontoon. Its function
 * is to act as a transparent bridge between the user's control interface
 * (via Bluetooth on Serial0) and the fish (via a half-duplex RS485 on Serial1).
 *
 * HARDWARE CONNECTIONS:
 * - Serial0 (pins 0, 1): Connected to a Bluetooth / nRF Module Serial Module (e.g., HC-05 or nRF240L).
 * - Serial1 (pins 18, 19): Connected to the RX/TX pins of an RS485 Transceiver.
 * - Digital Pin 22: Connected to the DE/RE (Driver Enable/Receiver Enable)
 * pins of the RS485 Transceiver.
 *
 * METHODOLOGY:
 * The code is non-blocking. It defaults to RS485 "receive" mode to constantly
 * listen for messages from the fish. When data arrives from the user, it
 * switches to "transmit" mode, sends all the data at once, waits for the
 * transmission to complete, and immediately switches back to "receive" mode.
 * This ensures a fast and reliable half-duplex communication link.
 *
 */

// --- Hardware Configuration ---
// Define the pin connected to the DE & RE pins of the RS485 transceiver.
#define RS485_CTRL_PIN 22

/************************ RS485 Direction Control ************************/

/**
 * @brief Enables the RS485 transmitter and disables the receiver.
 */
void enableTransmit() {
  // A brief delay can help ensure the transceiver chip has time to switch states.
  delayMicroseconds(100);
  digitalWrite(RS485_CTRL_PIN, HIGH);
}

/**
 * @brief Enables the RS485 receiver and disables the transmitter.
 */
void enableReceive() {
  delayMicroseconds(100);
  digitalWrite(RS485_CTRL_PIN, LOW);
}

/************************ Setup & Main Loop ************************/

/**
 * @brief Initializes the serial ports and the RS485 control pin.
 */
void setup() {
  // Configure the RS485 control pin as an output.
  pinMode(RS485_CTRL_PIN, OUTPUT);
  // Start in receive mode by default to listen for the fish.
  enableReceive();

  // Start Serial0 for communication with the user's Bluetooth device.
  // This baud rate must match the configuration of your Bluetooth module.
  Serial.begin(115200);

  // Start Serial1 for communication with the fish via the RS485 transceiver.
  // This baud rate MUST match the baud rate set in the fish's code.
  Serial1.begin(115200);

  // Send a confirmation message to the user's console to show the relay is active.
  Serial.println("Pontoon RS485 Relay Initialized. Bridge is active.");

  delay(1000);
}

/**
 * @brief The main execution loop for the serial relay.
 */
void loop() {
  // --- Relay 1: User (Bluetooth on Serial0) TO Fish (RS485 on Serial1) ---

  // Check if there is any data from the user waiting to be sent.
  if (Serial.available()>0) {
    // 1. Switch the RS485 module to Transmit mode.
    enableTransmit();

    // 2. Forward all available bytes at once.
    // Using a while loop is efficient. If multiple bytes arrive in a burst,
    // they are all sent in one continuous transmission.
    while (Serial.available()>0) {
      char v = Serial.read();
      delay(100);
      Serial.print("Pontoon Sent: ");
      Serial.println(v);
      Serial1.print(v);

    }

    // 3. Wait for the transmission to complete.
    // This is a crucial step to ensure all data has left the buffer
    // before we switch the direction back.

    // 4. Switch the RS485 module back to Receive mode.
    enableReceive();
  }

  // --- Relay 2: Fish (RS485 on Serial1) TO User (Bluetooth on Serial0) ---

  // Check if there is any data from the fish waiting to be read.
  // Since the default state is "receive," we can just read the data directly.
  while (Serial1.available()>0) {
    char data = Serial1.read();
    Serial.print(data);
  }
}