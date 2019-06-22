#include <ESP8266WiFi.h>
#include <AzureIoTHub.h>
#include "hardwareDoings.h"
#include "generalModel.h"
#include "message.h"
#include "iothubClient.h"

extern bool messagePending;
extern bool messageSending;
extern bool updatePending;
extern bool stateReporting;
extern bool stateSent;
static WiFiClientSecure sslClient; // for ESP8266

const char *onSuccess = "\"Successfully invoke device method\"";
const char *notFound = "\"No method found\"";

/*
 * The new version of AzureIoTHub library change the AzureIoTHubClient signature.
 * As a temporary solution, we will test the definition of AzureIoTHubVersion, which is only defined
 *    in the new AzureIoTHub library version. Once we totally deprecate the last version, we can take
 *    the #ifdef out.
 *
 * AzureIotHub library remove AzureIoTHubClient class in 1.0.34, so we remove the code below to avoid
 *    compile error
*/

void start()
{
    Serial.println("Start sending temperature and humidity data.");
    messageSending = true;
}

void stop()
{
    Serial.println("Stop sending temperature and humidity data.");
    messageSending = false;
}

static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    if (IOTHUB_CLIENT_CONFIRMATION_OK == result)
    {
        General *oldGeneral = (General *)userContextCallback;
        if (NULL == oldGeneral)
        {
            Serial.println("userContextCallback is NULL");
        }

        Serial.println("Message sent to Azure IoT Hub");
        blinkLED(LED_REVERSE);
    }
    else
    {
        Serial.println("Failed to send message to Azure IoT Hub");
    }
    messagePending = false;
}

void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert)
{
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char *)buffer, strlen(buffer));
    if (messageHandle == NULL)
    {
        Serial.println("Unable to create a new IoTHubMessage.");
    }
    else
    {
        MAP_HANDLE properties = IoTHubMessage_Properties(messageHandle);
        Map_Add(properties, "temperatureAlert", temperatureAlert ? "true" : "false");
        Serial.printf("Sending message: %s.\r\n", buffer);
        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, NULL) != IOTHUB_CLIENT_OK)
        {
            Serial.println("Failed to hand over the message to IoTHubClient.");
        }
        else
        {
            messagePending = true;
            Serial.println("IoTHubClient accepted the message for delivery.");
        }

        IoTHubMessage_Destroy(messageHandle);
    }
}

IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char *buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        Serial.println("Unable to IoTHubMessage_GetByteArray.");
        result = IOTHUBMESSAGE_REJECTED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char *temp = (char *)malloc(size + 1);

        if (temp == NULL)
        {
            return IOTHUBMESSAGE_ABANDONED;
        }

        strncpy(temp, (const char *)buffer, size);
        temp[size] = '\0';
        Serial.printf("Receive C2D message: %s.\r\n", temp);
        free(temp);
        blinkLED(LED_REVERSE);
    }
    return IOTHUBMESSAGE_ACCEPTED;
}

int deviceMethodCallback(
    const char *methodName,
    const unsigned char *payload,
    size_t size,
    unsigned char **response,
    size_t *response_size,
    void *userContextCallback)
{
    General *oldGeneral = (General *)userContextCallback;
    if (NULL == oldGeneral)
    {
        printf("userContextCallback is NULL\n");
        return 0;
    }

    Serial.printf("Try to invoke method %s.\r\n", methodName);
    const char *responseMessage = onSuccess;
    int result = 200;

    if (strcmp(methodName, "start") == 0)
    {
        start();
    }
    else if (strcmp(methodName, "stop") == 0)
    {
        stop();
    }
    else
    {
        Serial.printf("No method %s found.\r\n", methodName);
        responseMessage = notFound;
        result = 404;
    }

    *response_size = strlen(responseMessage);
    *response = (unsigned char *)malloc(*response_size);
    strncpy((char *)(*response), responseMessage, *response_size);

    //reportState(oldGeneral, iothubclient);

    return result;
}

/*
*   Device Twin Handling
*/

static void reportedStateCallback(int status_code, void *userContextCallback)
{
    (void)userContextCallback;
    printf("Device Twin reported properties update completed with result: %d\r\n", status_code);
    stateReporting = false;
}

void reportState(General *oldGeneral, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle)
{
    stateReporting = true;
    oldGeneral->state.reported_interval = oldGeneral->settings.desired_interval;
    printf("reporting state\r\n");
    char *reportedProperties = getSerializedMessage(oldGeneral);
    int size = strlen(reportedProperties);
    for (int i = 0; i < size; i++)
    {
        printf("%c", reportedProperties[i]);
    }
    printf("\r\n");

    (void)IoTHubClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char *)reportedProperties, strlen(reportedProperties), reportedStateCallback, NULL);
}

void twinCallback(
    DEVICE_TWIN_UPDATE_STATE updateState,
    const unsigned char *payLoad,
    size_t size,
    void *userContextCallback)
{
    printf("Device Twin update received (state=%s, size=%u): \r\n", ENUM_TO_STRING(DEVICE_TWIN_UPDATE_STATE, updateState), size);
    for (size_t n = 0; n < size; n++)
    {
        printf("%c", payLoad[n]);
    }
    printf("\r\n");

    char *temp = (char *)malloc(size + 1);
    for (size_t i = 0; i < size; i++)
    {
        temp[i] = (char)(payLoad[i]);
    }
    temp[size] = '\0';

    General *oldGeneral = (General *)userContextCallback;
    if (NULL == oldGeneral)
    {
        printf("general is null\n");
        return;
    }

    General *newGeneral = parseTwinMessage(temp);

    printf("assigning desired states\n");
    printf("\tversion: %s\t\t-->: %s\r\n", oldGeneral->state.version, newGeneral->state.version);
    printf("\tinterval: %i\t\t-->: %i\r\n", oldGeneral->settings.desired_interval, newGeneral->settings.desired_interval);
    if (NULL != oldGeneral->settings.update_url && strlen(newGeneral->settings.update_url) > 1)
    {
        printf("\turl: %s\t\t-->: %s\r\n", oldGeneral->settings.update_url, newGeneral->settings.update_url);
    }
    if (oldGeneral->settings.desired_interval != newGeneral->state.reported_interval)
    {
        if (NULL == newGeneral->settings.desired_interval || newGeneral->settings.desired_interval < 500)
        {
            newGeneral->settings.desired_interval = 500;
            printf("Interval invalid (%i)! Setting to 500ms.\n", newGeneral->settings.desired_interval);
        }
        oldGeneral->settings.desired_interval = newGeneral->settings.desired_interval;
        printf("set interval: %i\n", oldGeneral->settings.desired_interval);
    }

    if (strcmp(oldGeneral->state.version, newGeneral->state.version) < 0)
    {
        strcpy(oldGeneral->state.version, newGeneral->state.version);
        printf("should trigger update with version: %s sizeof %d strlen %d\n", oldGeneral->state.version, sizeof(oldGeneral->state.version), strlen(oldGeneral->state.version));
        updatePending = true;
    }

    if (oldGeneral->settings.update_url != '\0' || strcmp(oldGeneral->settings.update_url, newGeneral->settings.update_url) != 0)
    {
        strcpy(oldGeneral->settings.update_url, newGeneral->settings.update_url);
        printf("set update url: %s\n", oldGeneral->settings.update_url);
    }
    //reportState(oldGeneral);
    free(temp);
    free(newGeneral);
}
