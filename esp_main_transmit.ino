#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define REQUEST_CHARACTERISTIC_UUID "c0de1234-5678-9abc-def0-123456789abc"

RF24 radio(4, 5); // CE, CSN
const byte address[6] = "00001"; // Адрес для NRF24
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 500;
const int maxPacketSize = 30;

TinyGPSPlus gps;

BLECharacteristic *pCharacteristic;
BLECharacteristic *pRequestCharacteristic;
BLEServer *pServer = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::string userData = "";
std::string cargoID = "N/A";
std::string cargoAddress = "N/A";
std::string fullName = "N/A";
std::string price = "N/A";
std::string weight = "N/A";
std::string description = "N/A";

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

class MyDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = std::string(pCharacteristic->getValue().c_str());
        if (value.length() > 0) {
            Serial.println("Data received via BLE (bytearray): ");
            for (char c : userData) {
                Serial.print(c, HEX);
                Serial.print(" ");
            }
            Serial.println();
        }
    }
};

std::string getCombinedData() {
    std::string combinedData = "{";

    // GPS данные
    if (gps.location.isValid()) {
        combinedData += "\"GPS_Latitude\": \"" + std::to_string(gps.location.lat()) + "\", ";
        combinedData += "\"GPS_Longitude\": \"" + std::to_string(gps.location.lng()) + "\", ";
        combinedData += "\"Date\": \"" + std::to_string(gps.date.month()) + "/" + std::to_string(gps.date.day()) + "/" + std::to_string(gps.date.year()) + "\", ";
        combinedData += "\"Time\": \"" + std::to_string(gps.time.hour()) + ":" + std::to_string(gps.time.minute()) + ":" + std::to_string(gps.time.second()) + "\", ";
    } else {
        combinedData += "\"GPS_Latitude\": \"0.0\", \"GPS_Longitude\": \"0.0\", \"Date\": \"N/A\", \"Time\": \"N/A\", ";
    }

    // Данные пользователя
    if (!userData.empty()) {
        combinedData += userData.substr(1, userData.length() - 2) + ", ";
    } else {
        combinedData += "\"CargoID\": \"" + cargoID + "\", ";
        combinedData += "\"Longitude\": \"N/A\", \"Latitude\": \"N/A\", ";
        combinedData += "\"Address\": \"" + cargoAddress + "\", "; // Обновлено
        combinedData += "\"FullName\": \"" + fullName + "\", ";
        combinedData += "\"Price\": \"" + price + "\", ";
        combinedData += "\"Weight\": \"" + weight + "\", ";
        combinedData += "\"Description\": \"" + description + "\", ";
    }

    // Убираем последний запятый символ и закрываем JSON
    if (combinedData.back() == ' ') {
        combinedData.pop_back();  // Убираем последний пробел
    }
    if (combinedData.back() == ',') {
        combinedData.pop_back();  // Убираем последнюю запятую
    }

    combinedData += "}";
    Serial.println("Combined data: " + String(combinedData.c_str()));
    return combinedData;
}

std::string getUserData() {
    if (!userData.empty()) {
        return userData;
    } else {
        return "{\"CargoID\": \"" + cargoID + "\", \"Longitude\": \"N/A\", \"Latitude\": \"N/A\"}";
    }
}

void sendBLEData(BLECharacteristic *pCharacteristic, std::string data) {
    // Преобразуем std::string в String
    String dataString = String(data.c_str());

    Serial.println("Sending data via BLE: " + dataString);

    // Устанавливаем значение характеристики с преобразованной строкой
    pCharacteristic->setValue(dataString.c_str());
    pCharacteristic->notify();
}

class MyRequestCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string value = std::string(pCharacteristic->getValue().c_str());
        if (value.length() > 0 && value == "get") {
            Serial.println("Received 'get' request");
            pCharacteristic->setValue("");  // Очистка запроса
            if (deviceConnected) {
                std::string userDataToSend = getUserData();
                sendBLEData(pCharacteristic, userDataToSend);
                Serial.println("Sent user data to request: " + String(userDataToSend.c_str()));
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, 16, 17);  // Настройка GPS модуля

    BLEDevice::init("ESP32_BLE");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setCallbacks(new MyDataCallbacks());

    pRequestCharacteristic = pService->createCharacteristic(
                        REQUEST_CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_WRITE
                      );
    pRequestCharacteristic->setCallbacks(new MyRequestCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("BLE сервер запущен, ожидается подключение...");

    if (!radio.begin()) {
        Serial.println("NRF24L01 не отвечает!");
        while (1);
    }
    radio.openWritingPipe(address);
    radio.setPALevel(RF24_PA_HIGH);
    radio.stopListening();
}

void updateGPSData() {
    while (Serial2.available() > 0) {
        gps.encode(Serial2.read());
    }
}

void sendNRFData() {
    std::string combinedData = getCombinedData();
    int jsonLength = combinedData.length();
    char packetBuffer[maxPacketSize + 1];

    for (int i = 0; i < jsonLength; i += maxPacketSize) {
        std::string packet = combinedData.substr(i, maxPacketSize);
        strncpy(packetBuffer, packet.c_str(), maxPacketSize);
        packetBuffer[maxPacketSize] = '\0';

        radio.write(packetBuffer, maxPacketSize + 1);
        Serial.print("Sent via NRF24: ");
        Serial.println(packetBuffer);
    }
}

void loop() {
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    updateGPSData();

    if (millis() - lastSendTime >= sendInterval) {
        lastSendTime = millis();
        sendNRFData();
    }

    delay(10);
}