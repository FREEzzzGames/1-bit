#include <Arduino.h>
// Arduino entry point declarations must be parsed before renaming the
// implementation functions in main.cpp.
#define setup neonClockSetupInternal
#define loop neonClockLoopInternal
#include "main.cpp"
#undef setup
#undef loop

void setup() { neonClockSetupInternal(); }
void loop() { neonClockLoopInternal(); }
