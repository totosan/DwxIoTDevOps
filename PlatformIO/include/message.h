
//declarations
void initSensor();
float readTemperature();
float readHumidity();
float readPhoto();
bool readMessage(int messageId, char *payload);
char *getSerializedMessage(General *general);
General *parseTwinMessage(char *message);