// declarations
char * GetDeviceId(char *buffer, int cnnStrLen);
void readCredentials();
bool needEraseEEPROM();
void clearParam();
void EEPROMWrite(int addr, char *data, int size);
int EEPROMread(int addr, char *buf);