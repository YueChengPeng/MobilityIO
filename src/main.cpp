

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "I2CScanner.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

Adafruit_MPU6050 mpu;
I2CScanner scanner;
Servo myservo; // create servo object to control a servo

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledInitialized = false;

int servoPos = 0; // variable to store the servo
bool servoInitialized = false;
int servoOrder = 0;

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic_IO1 = NULL;
BLECharacteristic *pCharacteristic_IO2 = NULL;
BLECharacteristic *pCharacteristic_IO3 = NULL;
BLECharacteristic *pCharacteristic_IO4 = NULL;

BLECharacteristic *pCharacteristic_Order = NULL;
BLECharacteristic *characteristicsIOArray[4]; // four io characteristics
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;
char orderData[10];
int pinConfigGroup[4][2] = {{4, 5}, {0, 1}, {2, 3}, {8, 9}};          // 4 components, 2 pins each
int pinConfigGroupA[4][2] = {{A4, A5}, {A0, A1}, {A2, A3}, {A8, A9}}; // 4 components, 2 pins each
int pinConfigGroupD[4][2] = {{D4, D5}, {D0, D1}, {D2, D3}, {D8, D9}}; // 4 components, 2 pins each
bool motionInitialized = false;
std::array<std::string, 4> ioDataArr;

bool motionSetup();

#define SERVICE_UUID "18a3c5f7-4a60-4021-acbb-5721434d9239"
#define CHARACTERISTIC_IO1_UUID "bff7f0c9-5fbf-4b63-8d83-b8e077176fbe"
#define CHARACTERISTIC_IO2_UUID "e66f28fc-d85b-4e29-adad-20e5409723c8"
#define CHARACTERISTIC_IO3_UUID "06e5b626-82bb-4348-b898-d17949b23011"
#define CHARACTERISTIC_IO4_UUID "abbb094b-1889-4419-a611-064feeb4e09c"

#define CHARACTERISTIC_ORDER_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" // web app (manually input) to ESP32, specify the order of components (e.g. A1d3E2C4)

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string value = pCharacteristic->getValue();

        if (value.length() > 0)
        {
            Serial.println("New value written:");
            // Serial.println(value.length());
            // for (int i = 0; i < value.length(); i++)
            //     Serial.print(value[i]);
            // Serial.println();
            if (pCharacteristic->getUUID().toString() == CHARACTERISTIC_ORDER_UUID)
            {
                strcpy(orderData, value.c_str());
                Serial.println(orderData);
            }
            else if (pCharacteristic->getUUID().toString() == CHARACTERISTIC_IO1_UUID)
            {
                Serial.print("IO1 Data read:");
                Serial.println(value.c_str());
                ioDataArr[0] = value;
            }
            else if (pCharacteristic->getUUID().toString() == CHARACTERISTIC_IO2_UUID)
            {
                Serial.print("IO2 Data read:");
                Serial.println(value.c_str());
                ioDataArr[1] = value;
            }
            else if (pCharacteristic->getUUID().toString() == CHARACTERISTIC_IO3_UUID)
            {
                Serial.print("IO3 Data read:");
                Serial.println(value.c_str());
                ioDataArr[2] = value;
            }
            else if (pCharacteristic->getUUID().toString() == CHARACTERISTIC_IO4_UUID)
            {
                Serial.print("IO4 Data read:");
                Serial.println(value.c_str());
                ioDataArr[3] = value;
            }
        }
    }
};

void setup()
{
    memset(orderData, 0, sizeof(orderData));
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // scanner.Init();
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu.setMotionDetectionThreshold(1);
    mpu.setMotionDetectionDuration(20);
    mpu.setInterruptPinLatch(true); // Keep it latched.  Will turn off when reinitialized.
    mpu.setInterruptPinPolarity(true);
    mpu.setMotionInterrupt(true);

    // Allow allocation of all timers
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    myservo.setPeriodHertz(50); // standard 50 hz servo

    BLEDevice::init("ModularIO"); // Give it a name
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // cannot write like BLECharacteristic *pCharacteristic_IO1. Or later pCharacteristic_IO1->setValue(combined.c_str()); will reboot the chip
    pCharacteristic_IO1 = pService->createCharacteristic(
        CHARACTERISTIC_IO1_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_IO2 = pService->createCharacteristic(
        CHARACTERISTIC_IO2_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_IO3 = pService->createCharacteristic(
        CHARACTERISTIC_IO3_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_IO4 = pService->createCharacteristic(
        CHARACTERISTIC_IO4_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_Order = pService->createCharacteristic(
        CHARACTERISTIC_ORDER_UUID,
        BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_NOTIFY);

    pCharacteristic_Order->setCallbacks(new MyCharacteristicCallbacks()); // don't put this line before the pCharacteristic_Order is defined! after definition, can the pCharacteristic_Order be set callback functions
    pCharacteristic_IO1->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristic_IO2->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristic_IO3->setCallbacks(new MyCharacteristicCallbacks());
    pCharacteristic_IO4->setCallbacks(new MyCharacteristicCallbacks());

    characteristicsIOArray[0] = pCharacteristic_IO1;
    characteristicsIOArray[1] = pCharacteristic_IO2;
    characteristicsIOArray[2] = pCharacteristic_IO3;
    characteristicsIOArray[3] = pCharacteristic_IO4;

    pCharacteristic_IO1->addDescriptor(new BLE2902());
    pCharacteristic_IO2->addDescriptor(new BLE2902());
    pCharacteristic_IO3->addDescriptor(new BLE2902());
    pCharacteristic_IO4->addDescriptor(new BLE2902());

    pCharacteristic_Order->addDescriptor(new BLE2902());
    pCharacteristic_IO1->setValue("");
    pCharacteristic_IO2->setValue("");
    pCharacteristic_IO3->setValue("");
    pCharacteristic_IO4->setValue("");

    pCharacteristic_Order->setValue("Default String");
    pService->addCharacteristic(pCharacteristic_Order);

    pService->start();
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop()
{
    if (deviceConnected)
    {
        if (orderData[0] != '0')
        {
            // Serial.print("MPU check: ");
            // Serial.println(scanner.Check(0x68));

            // if (orderData[0] != 'm' && motionInitialized && !scanner.Check(0x68))
            // {
            //     motionInitialized = false;
            // }

            if (orderData[0] != 'm') // not motion sensor
            {
                // Serial.println("motionInitialized set to false!");
                motionInitialized = false;
            }
            if (orderData[0] != 'o') // not oled
            {
                oledInitialized = false;
            }
            // if (orderData[0] != 'm' && orderData[0] != 'o') // not I2C interface
            // {
            //     // Wire.end();
            // }
            if (orderData[servoOrder] != 's') // servo initialized then removed
            {
                servoInitialized = false;
            }

            // Serial.println(orderData);
            for (int i = 0; orderData[i] != '\0'; i++)
            {
                int value1;
                int value2;
                int value3;
                int frequency;
                long duration, distance;

                String combined;
                switch (orderData[i])
                {
                case 'j': // joystick
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }
                    pinMode(pinConfigGroupA[i][0], INPUT);
                    pinMode(pinConfigGroupA[i][1], INPUT);
                    value1 = analogRead(pinConfigGroupA[i][0]);
                    value2 = analogRead(pinConfigGroupA[i][1]);
                    combined = String(value1) + " " + String(value2);
                    characteristicsIOArray[i]->setValue(combined.c_str());
                    characteristicsIOArray[i]->notify();
                    break;
                case 'r': // rotary knob
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }
                    pinMode(pinConfigGroupA[i][0], INPUT);
                    pinMode(pinConfigGroupA[i][1], INPUT);
                    value1 = analogRead(pinConfigGroupA[i][0]);
                    value2 = analogRead(pinConfigGroupA[i][1]);
                    combined = String(value1) + " " + String(value2);
                    characteristicsIOArray[i]->setValue(combined.c_str());
                    characteristicsIOArray[i]->notify();
                    break;
                case 'm': // motion sensor
                    if (!motionInitialized)
                    {
                        // initialize the i2c pin
                        Wire.end();
                        pinMode(pinConfigGroupD[0][0], OUTPUT);
                        pinMode(pinConfigGroupD[0][1], OUTPUT);
                        Wire.begin();
                        characteristicsIOArray[i]->setValue("0 0 0");
                        characteristicsIOArray[i]->notify();
                        if (mpu.begin())
                            motionInitialized = true;
                    }
                    // if (mpu.getMotionInterruptStatus())
                    // {
                    /* Get new sensor events with the readings */

                    if (motionInitialized)
                    {
                        sensors_event_t a, g, temp;
                        mpu.getEvent(&a, &g, &temp);

                        /* Print out the values */
                        // Serial.print("AccelX:");
                        value1 = a.acceleration.x;
                        value2 = a.acceleration.y;
                        value3 = a.acceleration.z - 9; // calibration, set all initial values to 0
                        combined = String(value1) + " " + String(value2) + " " + String(value3);
                        Serial.println(combined);
                        characteristicsIOArray[i]->setValue(combined.c_str());
                        characteristicsIOArray[i]->notify();
                    }
                    else
                    {
                        Serial.println("MPU6050 Not Initialized!");
                        characteristicsIOArray[i]->setValue("0 0 0");
                        characteristicsIOArray[i]->notify();
                    }
                    break;
                case 'z': // buzzer
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }

                    pinMode(pinConfigGroupD[i][0], OUTPUT);
                    pinMode(pinConfigGroupD[i][1], OUTPUT);

                    frequency = atoi(ioDataArr[i].c_str()); // Convert string to integer
                    tone(pinConfigGroupD[i][0], frequency);
                    tone(pinConfigGroupD[i][1], frequency);

                    break;
                case 'v': // vibration motor
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }

                    pinMode(pinConfigGroupD[i][0], OUTPUT);
                    if (atoi(ioDataArr[i].c_str()))
                    {
                        // turn on motor
                        digitalWrite(pinConfigGroupD[i][0], HIGH);
                    }
                    else
                    {
                        // turn off motor
                        digitalWrite(pinConfigGroupD[i][0], LOW);
                    }
                    break;
                case 'b': // button
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }

                    pinMode(pinConfigGroupA[i][0], INPUT);
                    pinMode(pinConfigGroupA[i][1], INPUT);
                    value1 = digitalRead(pinConfigGroupD[i][0]);
                    combined = String(value1);
                    characteristicsIOArray[i]->setValue(combined.c_str());
                    characteristicsIOArray[i]->notify();
                    break;
                case 'o': // oled
                    if (!oledInitialized)
                    {
                        // initialize the i2c pin
                        Wire.end();
                        pinMode(pinConfigGroupD[0][0], OUTPUT);
                        pinMode(pinConfigGroupD[0][1], OUTPUT);
                        Wire.begin();

                        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
                        {
                            Serial.println(F("SSD1306 allocation failed"));
                        }
                        else
                        {
                            Serial.println(F("SSD1306 allocation success"));
                            oledInitialized = true;
                            display.display();
                            delay(2000); // Pause for 2 seconds
                            display.clearDisplay();
                        }
                    }
                    if (oledInitialized)
                    {
                        display.clearDisplay();
                        display.setTextSize(4);              // Normal 1:1 pixel scale
                        display.setTextColor(SSD1306_WHITE); // Draw white text
                        display.setCursor(0, 10);            // Start at top-left corner
                        // Serial.println(ioDataArr[i].c_str());
                        display.println(ioDataArr[i].c_str());
                        display.display();
                    }
                    break;
                case 'u': // ultrasonic sensor
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }

                    pinMode(pinConfigGroupD[i][0], OUTPUT);
                    pinMode(pinConfigGroupD[i][1], INPUT);
                    digitalWrite(pinConfigGroupD[i][0], LOW);
                    delayMicroseconds(2);
                    digitalWrite(pinConfigGroupD[i][0], HIGH);
                    delayMicroseconds(10);
                    digitalWrite(pinConfigGroupD[i][0], LOW);
                    duration = pulseIn(pinConfigGroupD[i][1], HIGH);
                    distance = (duration / 2) / 29.1;
                    Serial.println(distance);
                    combined = String(distance);
                    characteristicsIOArray[i]->setValue(combined.c_str());
                    characteristicsIOArray[i]->notify();
                    break;
                case 's': // servo
                    // if the i2c is occupied by other components, end it
                    if (i == 0)
                    {
                        Wire.end();
                    }

                    if (!servoInitialized)
                    {
                        myservo.attach(pinConfigGroup[i][1], 500, 2400); // the pin number should be GPIO number, so 5 is actually D4
                        servoInitialized = true;
                        servoOrder = i;
                        Serial.println("Servo attached");
                    }
                    if (ioDataArr[i] != "")
                    {
                        if (servoPos != atoi(ioDataArr[i].c_str()))
                        {
                            Serial.print("Servo moving to:");
                            Serial.println(ioDataArr[i].c_str());

                            servoPos = atoi(ioDataArr[i].c_str());
                            myservo.write(servoPos);
                            delay(15);
                        }
                    }
                    break;
                }
            }
        }
    }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(500);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // advertise again
        Serial.println("Start advertising");
        memset(orderData, 0, sizeof(orderData)); // clear the orderData
        motionInitialized = false;               // reset the motion sensor
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    delay(300);
}

bool motionSetup()
{
    // Try to initialize the mpu6050
    if (!mpu.begin())
    {
        Serial.println("Failed to find MPU6050 chip");
        // while (1)
        // {
        //     if (mpu.begin())
        //         break;
        //     delay(100);
        //     Serial.println("Failed to find MPU6050 chip");
        // }
        return false;
    }
    // Serial.println("MPU6050 Found!");
    // setupt motion detection
    return true;
}
