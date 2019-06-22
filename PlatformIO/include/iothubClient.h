//declarations
static void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback);
void sendMessage(IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle, char *buffer, bool temperatureAlert);
void start();
void stop();
IOTHUBMESSAGE_DISPOSITION_RESULT receiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback);
int deviceMethodCallback(const char *methodName,const unsigned char *payload,size_t size,unsigned char **response,size_t *response_size,void *userContextCallback);
static void reportedStateCallback(int status_code, void *userContextCallback);
void reportState(General *oldGeneral, IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle);
void twinCallback(DEVICE_TWIN_UPDATE_STATE updateState,const unsigned char *payLoad,size_t size,void *userContextCallback);