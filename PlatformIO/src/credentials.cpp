#include <EEPROM.h>
#include <stdio.h>
#include <stdlib.h>
#include "tokenize.h"
#include "config.h"
#include "credentials.h"
#include "serialReader.h"

extern char *connectionString;
extern char *ssid;
extern char *pass;
extern char *deviceId;

char * GetDeviceId(char *buffer, int cnnStrLen){
    char** conn = getToken(buffer, cnnStrLen, ";");
    deviceId = getToken(conn[1],sizeof(conn[1]),"=")[1];
    printf("This is token for device Id %s\r\n",deviceId);
}

// Read parameters from EEPROM or Serial
void readCredentials()
{
    int ssidAddr = 0;
    int passAddr = ssidAddr + SSID_LEN;
    int connectionStringAddr = passAddr + SSID_LEN;

    // malloc for parameters
    ssid = (char *)malloc(SSID_LEN);
    pass = (char *)malloc(PASS_LEN);
    connectionString = (char *)malloc(CONNECTION_STRING_LEN);
    deviceId = (char *)malloc(DEVICE_ID_LEN);

    // try to read out the credential information, if failed, the length should be 0.
    int ssidLength = EEPROMread(ssidAddr, ssid);
    int passLength = EEPROMread(passAddr, pass);
    int connectionStringLength = EEPROMread(connectionStringAddr, connectionString);

    if (ssidLength > 0 && passLength > 0 && connectionStringLength > 0 && !needEraseEEPROM())
    {
        /*char** conn = getToken(connectionString, connectionStringLength, ";");
        printf("This is token for device Id %s\r\n",conn[1]);
        deviceId = strdup(conn[1]);*/
        GetDeviceId(connectionString, connectionStringLength);
        return;
    }

    // read from Serial and save to EEPROM
    readFromSerial("Input your Wi-Fi SSID: ", ssid, SSID_LEN, 0);
    EEPROMWrite(ssidAddr, ssid, strlen(ssid));

    readFromSerial("Input your Wi-Fi password: ", pass, PASS_LEN, 0);
    EEPROMWrite(passAddr, pass, strlen(pass));

    readFromSerial("Input your Azure IoT hub device connection string: ", connectionString, CONNECTION_STRING_LEN, 0);
    EEPROMWrite(connectionStringAddr, connectionString, strlen(connectionString));
    GetDeviceId(connectionString, connectionStringLength);

}


bool needEraseEEPROM()
{
    char result = 'n';
    readFromSerial("Do you need re-input your credential information?(Auto skip this after 5 seconds)[Y/n]", &result, 2, 5000);
    if (result == 'Y' || result == 'y')
    {
        clearParam();
        return true;
    }
    return false;
}

// reset the EEPROM
void clearParam()
{
    char data[EEPROM_SIZE];
    memset(data, '\0', EEPROM_SIZE);
    EEPROMWrite(0, data, EEPROM_SIZE);
}

#define EEPROM_END 0
#define EEPROM_START 1
void EEPROMWrite(int addr, char *data, int size)
{
    EEPROM.begin(EEPROM_SIZE);
    // write the start marker
    EEPROM.write(addr, EEPROM_START);
    addr++;
    for (int i = 0; i < size; i++)
    {
        EEPROM.write(addr, data[i]);
        addr++;
    }
    EEPROM.write(addr, EEPROM_END);
    EEPROM.commit();
    EEPROM.end();
}

// read bytes from addr util '\0'
// return the length of read out.
int EEPROMread(int addr, char *buf)
{
    EEPROM.begin(EEPROM_SIZE);
    int count = -1;
    char c = EEPROM.read(addr);
    addr++;
    if (c != EEPROM_START)
    {
        return 0;
    }
    while (c != EEPROM_END && count < EEPROM_SIZE)
    {
        c = (char)EEPROM.read(addr);
        count++;
        addr++;
        buf[count] = c;
    }
    EEPROM.end();
    return count;
}
