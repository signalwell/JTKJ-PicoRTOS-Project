
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Exercise 4. Include the libraries necessaries to use the usb-serial-debug, and tinyusb
// Tehtävä 4 . Lisää usb-serial-debugin ja tinyusbin käyttämiseen tarvittavat kirjastot.

#define DEFAULT_STACK_SIZE 2048
#define CDC_ITF_TX 1

// Tehtävä 3: Tilakoneen esittely Add missing states.
// Exercise 3: Definition of the state machine. Add missing states.
enum state
{
    WAITING = 1,
    READ_SENSOR,
    WRITE_SPACE
};
enum state programState = WAITING;

/* typedef enum morseChar {
    dash,
    dot,
    space,
    none
} MorseChar; */

// Global variables for acceleration
char morseString[100];
int morseIndex = 0;
// Acceleration along the x-axis. Values far from 0 indicate the device is turned on its long side.
float ax;
// Acceleration along the y-axis. Values far from 0 indicate the device is turned on its short side.
float ay;
// Acceleration along the z-axis. Values close to 0 indicate the device is turned on its side.
float az;

static void btn_fxn(uint gpio, uint32_t eventMask)
{
    if (gpio == BUTTON1)
    {
        printf("Button 1 pressed\n");
        programState = READ_SENSOR;
    }
    if (gpio == BUTTON2) {
        printf("Button 2 pressed\n");
        programState = WRITE_SPACE;
    }
}

static void btn2_fxn(uint gpio, uint32_t eventMask)
{
    printf("Button 2 pressed\n");
    programState = WRITE_SPACE;
}

static void sensor_task(void *arg)
{
    (void)arg;

    init_veml6030();

    for (;;)
    {

        // Reading accelerometer data
        if (ICM42670_read_sensor_data(&ax, &ay, &az, NULL, NULL, NULL, NULL) < 0)
        {
            // If an error occurs
        }

        // az ~ -1.0 and ax & ay ~ 0 when the device is flat
        /*         if (ax > 0.5 && ax < -0.5) {
                    printf("Device is turned on its long side\n");
                } else if (ay > 0.5 && ay < -0.5) {
                    printf("Device is turned on its short side\n");
                } else if (az < 0.5 && az > -0.5) {
                    printf("Device is turned on its side\n");
                } else {
                    printf("Device is flat\n");
                }
         */

        // programState = DATA_READY;

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int getMorseChar(float ax_, float ay_, float az_)
{
    float threshold = 0.2; // Define a threshold value for determining the Morse code character
    if (ax_ > (1.0 - threshold) || ax_ < (-1.0 + threshold))
    {
        return (int)'-';
    }
    else if (ay_ > (1.0 - threshold) || ay_ < (-1.0 + threshold))
    {
        return (int)'.';
    }
    else
    {
        return 0;
    }
}

static void print_task(void *arg)
{
    (void)arg;

    while (1)
    {
        if (programState == READ_SENSOR)
        {
            int morseChar = getMorseChar(ax, ay, az);
            if (morseChar != 0)
            {
                printf("%c \n", morseChar);
                morseString[morseIndex] = morseChar;
                morseIndex++;
                printf("Full morse: %s\n", morseString);
            }
        }
        else if (programState == WRITE_SPACE)
        {
            morseString[morseIndex] = ' ';
            morseIndex++;
            printf("Full morse: %s\n", morseString);
            if (morseString[morseIndex - 1] == ' ' && morseString[morseIndex - 2] == ' ' && morseString[morseIndex - 3] == ' ')
            {
                morseString[morseIndex - 1] = '\n';
                morseString[morseIndex] = '\0';
                printf("Morse code: %s", morseString);
            }
        }

        else
        {
            printf("Waiting for data...\n");
        }

        programState = WAITING;

        // Do not remove this
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Uncomment the following line to activate the TinyUSB library.
// Poista seuraavan rivin kommentointi aktivoidaksesi TinyUSB-kirjaston.

/*
static void usbTask(void *arg) {
    (void)arg;
    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay.
    }
}*/

int main()
{

    stdio_init_all();

    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/

    init_hat_sdk();
    sleep_ms(300); // Wait some time so initialization of USB and hat is done.

    init_button1();
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    init_button2();
    gpio_set_irq_enabled_with_callback(BUTTON2, GPIO_IRQ_EDGE_FALL, true, btn_fxn);

    init_red_led();

    // Initialize the IMU
    if (init_ICM42670() < 0)
    {
        // If an error occurs
        printf("IMU initialization failed\n");
        return 0;
    }
    if (ICM42670_start_with_default_values() < 0)
    {
        // If an error occurs
        printf("IMU initialization failed (ICM42670_start_with_default_values)\n");
    }

    TaskHandle_t hSensorTask, hPrintTask, hUSB = NULL;

    // Uncomment this xTaskCreate to create the task that enables dual USB communication.
    // Poista tämän xTaskCreate-rivin kommentointi luodaksesi tehtävän,
    // joka mahdollistaa kaksikanavaisen USB-viestinnän.

    /*
    xTaskCreate(usbTask, "usb", 2048, NULL, 3, &hUSB);
    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(hUSB, 1u << 0);
    #endif
    */

    // Create the tasks with xTaskCreate
    BaseType_t result = xTaskCreate(sensor_task,        // (en) Task function
                                    "sensor",           // (en) Name of the task
                                    DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                                    NULL,               // (en) Arguments of the task
                                    2,                  // (en) Priority of this task
                                    &hSensorTask);      // (en) A handle to control the execution of this task

    if (result != pdPASS)
    {
        printf("Sensor task creation failed\n");
        return 0;
    }
    result = xTaskCreate(print_task,         // (en) Task function
                         "print",            // (en) Name of the task
                         DEFAULT_STACK_SIZE, // (en) Size of the stack for this task (in words). Generally 1024 or 2048
                         NULL,               // (en) Arguments of the task
                         2,                  // (en) Priority of this task
                         &hPrintTask);       // (en) A handle to control the execution of this task

    if (result != pdPASS)
    {
        printf("Print Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
