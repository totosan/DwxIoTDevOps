#include <ESP8266httpUpdate.h>
#include "generalModel.h"

void HandleUpdate(General *general)
{
    delay(20);
    char *url = (char *)malloc(URL_MAX_LENGTH);
    strcpy(url, general->settings.update_url);
    printf("Downloading from URL: %s\r\n", url);
    t_httpUpdate_return ret = ESPhttpUpdate.update(url);
    //    const char *fingerprint = "55 71 CC 37 53 FF B8 86 0D 80 13 C3 D6 36 C9 7B B7 C0 C4 95";
    //const char* uri = "https://stodevopsiot001.blob.core.windows.net/devopsiot/fw/app.ino.bin";
    //t_httpUpdate_return ret = ESPhttpUpdate.update(uri,"",fingerprint);
    /*     strcpy(general->state.update_state, "UPDATING\0");
    reportState(general); */
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        strcpy(general->state.update_state, "ERROR\0");
        break;
    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        strcpy(general->state.update_state, "NO_UPDATES\0");
        break;
    case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        strcpy(general->state.update_state, "OK\0");
        break;
    }
    //reportState(general);
    free(url);
}