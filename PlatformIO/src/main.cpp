// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// Please use an Arduino IDE 1.6.8 or greater
#define newWifiWay
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

#include <AzureIoTHub.h>
#include <AzureIoTProtocol_MQTT.h>
#include <AzureIoTUtility.h>
#include "generalModel.h"
#include "config.h"
#include "credentials.h"
#include "serialReader.h"
#include "message.h"
#include "iothubClient.h"
#include "update.h"
#include "hardwareDoings.h"

static char *SW_VERSION = "1.4";

bool messagePending = false;
bool messageSending = true;
bool updatePending = false;
bool stateReporting = false;
bool stateSent = false;

char *connectionString;
char *ssid;
char *pass;
char *deviceId;

static int interval = INTERVAL;
static bool hasIoTHub = false;
static bool hasWifi = false;
static int messageCount = 1;

os_timer_t telemtryTimer;
bool IsTelemetryEvent;

// declaration
void timerCallback(void *pArg);
void user_init(void);
void initWifi();
void initTime();

extern void reportState(General *general, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle);
extern void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert);

void timerSendCallback(void *pArg)
{
    IsTelemetryEvent = true;
}

void user_init(void)
{
    /*
  os_timer_setfn - Define a function to be called when the timer fires
https://www.switchdoc.com/2015/10/iot-esp8266-timer-tutorial-arduino-ide/
void os_timer_setfn(
      os_timer_t *pTimer,
      os_timer_func_t *pFunction,
      void *pArg)
*/
    os_timer_setfn(&telemtryTimer, timerSendCallback, NULL);
    os_timer_arm(&telemtryTimer, interval, true);

} // End of user_init

void initWifi()
{
    // Attempt to connect to Wifi network:
    Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

    delay(10);
    WiFi.mode(WIFI_AP);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        hasWifi = false;
    }
    hasWifi = true;

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println(" > IoT Hub");
}

void initTime()
{
    time_t epochTime;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    while (true)
    {
        epochTime = time(NULL);

        if (epochTime == 0)
        {
            Serial.println("Fetching NTP epoch time failed! Waiting 2 seconds to retry.");
            delay(2000);
        }
        else
        {
            Serial.printf("Fetched NTP epoch time is: %lu.\r\n", epochTime);
            break;
        }
    }
}

static IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

static General general;

void setup()
{
    pinMode(LED_PIN, OUTPUT);

    initSerial();
    delay(2000);
    readCredentials();

    initWifi();
    initTime();
    initSensor();

    IsTelemetryEvent = false;
    user_init();

    /*
     * AzureIotHub library remove AzureIoTHubClient class in 1.0.34, so we remove the code below to avoid
     *    compile error
    */

    // initIoThubClient();
    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
    if (iotHubClientHandle == NULL)
    {
        Serial.printf("Failed on IoTHubClient_CreateFromConnectionString. %s\r\n", connectionString);
        while (1)
            ;
    }

    memset(&general, 0, sizeof(General));
    general.settings.desired_interval = interval;
    general.state.reported_interval = interval;
    general.state.temperature = 0.0f;
    general.state.humidity = 0.0f;
    strcpy(general.state.version, SW_VERSION);
    *general.state.update_state = {'\0'};
    *general.settings.update_url = {'\0'};

    IoTHubClient_LL_SetOption(iotHubClientHandle, "dwx_sample", "Updating_firmware_DevOps");
    IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, receiveMessageCallback, &general);
    IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, deviceMethodCallback, &general);
    IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, twinCallback, &general);

    reportState(&general, iotHubClientHandle);
}

void CheckTelemetryIntervallOccured()
{
    if (IsTelemetryEvent == true)
    {
        if (!updatePending)
        {
            if (!messagePending && messageSending)
            {
                Serial.println("Message loop");
                char messagePayload[MESSAGE_MAX_LEN];
                bool temperatureAlert = readMessage(messageCount, messagePayload, &general);
                if (!temperatureAlert)
                {
                    sendMessage(iotHubClientHandle, messagePayload, temperatureAlert);
                    reportState(&general, iotHubClientHandle);
                    messageCount++;
                }
                Serial.println("Waiting for delay");
            }
        }
        else // updatePending
        {
            if (!stateReporting && !stateSent)
            {
                strcpy(general.state.update_state, "DOWNLOADING");
                reportState(&general, iotHubClientHandle);
                stateSent = true;
            }
            if (!stateReporting)
            {
                HandleUpdate(&general);
                updatePending = false;
            }
        }
        {
            IoTHubClient_LL_DoWork(iotHubClientHandle);
        }

        IsTelemetryEvent = false;
    }
}

void loop()
{
    CheckTelemetryIntervallOccured();
    delay(10);
}
