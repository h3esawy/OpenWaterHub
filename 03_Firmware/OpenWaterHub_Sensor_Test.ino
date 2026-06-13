#define RS485_DE_RE 22

void setup() 
{
    Serial.begin(115200);
    Serial1.begin(9600);
    pinMode(RS485_DE_RE, OUTPUT);
    digitalWrite(RS485_DE_RE, LOW);

    Serial.println("RS485 Modbus Communication Initialized");
}

uint16_t Modbus_CRC16(uint8_t *buffer, uint8_t length) 
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < length; i++) 
    {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) 
        {
            if (crc & 0x0001) 
            {
                crc >>= 1;
                crc ^= 0xA001;
            } 
            else 
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

void RS485_Transmit(uint8_t *data, uint8_t len) 
{
    digitalWrite(RS485_DE_RE, HIGH);
    delay(1);

    Serial.print("TX: ");
    for (uint8_t i = 0; i < len; i++) 
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
        Serial1.write(data[i]);
    }
    Serial.println();

    Serial1.flush();
    digitalWrite(RS485_DE_RE, LOW);
}

uint8_t RS485_Receive(uint8_t *buffer, uint8_t len, uint16_t timeout) 
{
    uint32_t start = millis();
    uint8_t count = 0;

    while (count < len && (millis() - start) < timeout) 
    {
        if (Serial1.available()) 
        {
            buffer[count++] = Serial1.read();
        }
    }

    Serial.print("RX: ");
    for (uint8_t i = 0; i < count; i++) 
    {
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    return count;
}

bool ReadSensor(uint8_t id, float divisor, const char* label, const char* unit)
{
    uint8_t request[] = { id, 0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00 };

    // Temperature uses register address 0x0001 instead of 0x0002
    if (id == 0x03) request[3] = 0x01;

    uint16_t crc = Modbus_CRC16(request, 6);
    request[6] = crc & 0xFF;
    request[7] = (crc >> 8) & 0xFF;

    RS485_Transmit(request, 8);

    uint8_t response[7];
    if (RS485_Receive(response, 7, 500) == 7)
    {
        uint16_t received_crc = (response[6] << 8) | response[5];
        uint16_t calculated_crc = Modbus_CRC16(response, 5);

        if (received_crc == calculated_crc)
        {
            uint16_t raw_value = (response[3] << 8) | response[4];
            float value = raw_value / divisor;

            Serial.print(label);
            Serial.print(": ");
            Serial.print(value, 2);
            Serial.print(" ");
            Serial.println(unit);
            return true;
        }
        else
        {
            Serial.print(label);
            Serial.println(": CRC Error");
        }
    }
    else
    {
        Serial.print(label);
        Serial.println(": No Response");
    }
    return false;
}

void loop() 
{
    Serial.println("\n--- Reading Sensors ---");

    // Temperature sensor (°C)
    ReadSensor(0x03, 10.0, "Temperature", "°C");
    delay(1000); // 1-second delay between readings

    // ORP sensor (mV)
    ReadSensor(0x02, 10.0, "ORP", "mV");
    delay(1000);

    // pH sensor
    ReadSensor(0x06, 100.0, "pH", "pH");
    delay(1000);

    // Turbidity sensor (NTU)
    ReadSensor(0x01, 1.0, "Turbidity", "NTU");
    delay(1000);

    Serial.println("-----------------------------");
    delay(3000); // Wait 3 seconds before starting next cycle
}