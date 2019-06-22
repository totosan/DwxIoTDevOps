
//declarations
void initSensor();
float readTemperature();
float readHumidity();
float readPhoto();
bool readMessage(int messageId, char *payload, General *general);
char *getSerializedMessage(General *general);
General *parseTwinMessage(char *message);