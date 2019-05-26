
//declarations
void initSensor();
float readTemperature();
float readHumidity();
bool readMessage(int messageId, char *payload);
char *getSerializedMessage(General *general);
General *parseTwinMessage(char *message);