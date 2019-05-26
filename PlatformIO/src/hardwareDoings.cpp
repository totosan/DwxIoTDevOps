#include <Arduino.h>
#include "config.h"

void blinkLED(bool reverse)
{
    digitalWrite(LED_PIN, (reverse) ? HIGH : LOW);
    delay(100);
    digitalWrite(LED_PIN, (reverse) ? LOW : HIGH);
}