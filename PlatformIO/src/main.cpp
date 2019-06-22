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
bool onBeep = false;

int armedAlarmCount = 0;
static int maxLightValue = 100;
static int minLightValue = 20;
static int alarmTriggerCount = 6;

char *connectionString;
char *ssid;
char *pass;
char *deviceId;

static int interval = INTERVAL;
static bool hasIoTHub = false;
static bool hasWifi = false;
static int messageCount = 1;

os_timer_t telemtryTimer;
os_timer_t beeperTimer;
bool IsTelemetryEvent;
bool IsDoorAlertEvent;

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
void timerBeeperCallback(void *pArg)
{
    IsDoorAlertEvent = true;
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

    os_timer_setfn(&beeperTimer, timerBeeperCallback, NULL);
    os_timer_arm(&beeperTimer, 1000, true);
} // End of user_init

void initWifi()
{
    // Attempt to connect to Wifi network:
    Serial.printf("Attempting to connect to SSID: %s.\r\n", ssid);

#ifndef newWifiWay
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        // Get Mac Address and show it.
        // WiFi.macAddress(mac) save the mac address into a six length array, but the endian may be different. The huzzah board should
        // start from mac[0] to mac[5], but some other kinds of board run in the oppsite direction.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        Serial.printf("You device with MAC address %02x:%02x:%02x:%02x:%02x:%02x connects to %s failed! Waiting 10 seconds to retry.\r\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ssid);
        WiFi.begin(ssid, pass);
        delay(10000);
    }
    Serial.printf("Connected to wifi %s.\r\n", ssid);
#else
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
#endif
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
    IsDoorAlertEvent = false;
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
    general.state.doorAlerting = false;
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

void CheckDoorAlert()
{

    if (IsDoorAlertEvent == true)
    {
        int lightValue = readPhoto();
        //Serial.printf("Light: %i counter: %i \r\n", lightValue, armedAlarmCount);
        if (lightValue >= maxLightValue && armedAlarmCount <= alarmTriggerCount)
        {
            armedAlarmCount++;
        }
        else if (lightValue < maxLightValue && armedAlarmCount > 0 && armedAlarmCount <= alarmTriggerCount)
        {
            armedAlarmCount = 0;
        }

        if (lightValue < minLightValue && onBeep == true)
        {// stop door alert
            onBeep = false;
            armedAlarmCount = 0;
            general.state.doorAlerting = false;
            reportState(&general, iotHubClientHandle);
            IoTHubClient_LL_DoWork(iotHubClientHandle);
        }

        if (armedAlarmCount > alarmTriggerCount)
        {// arm door alert
            armedAlarmCount = 0;
            onBeep = true;
            general.state.doorAlerting = true;
            reportState(&general, iotHubClientHandle);
            IoTHubClient_LL_DoWork(iotHubClientHandle);
        }

        if (onBeep == true)
        {
            beep();
        }
        IsDoorAlertEvent = false;
    }
}

void loop()
{
    CheckTelemetryIntervallOccured();
    CheckDoorAlert();
    delay(10);
}
