// The project keeps the firmware implementation in main.cpp inside an
// anonymous namespace. Export Arduino's required global entry points here.
#define setup neonClockSetupInternal
#define loop neonClockLoopInternal
#include "main.cpp"
#undef setup
#undef loop

void setup() { neonClockSetupInternal(); }
void loop() { neonClockLoopInternal(); }
