#include <Arduino.h>

#ifndef main_h
#define main_h

String pwgen(void);
void setup_wifi_portal(void);
void wait_wifi(void);
String read(const char *);

#endif