// Linux native main() — POSIX entry point for the linux_repeater firmware.
// Provides global Arduino-compatible objects (Serial, LinuxFS) and
// runs the standard Arduino setup() / loop() cycle.
#include "Arduino.h"
#include "LinuxFS.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

// ---------------------------------------------------------------------------
// Global instances declared extern in Arduino.h / LinuxFS.h
// ---------------------------------------------------------------------------
HardwareSerial Serial;
fs::FS LinuxFS;

// ---------------------------------------------------------------------------
// Forward declarations (defined in examples/simple_repeater/main.cpp)
// ---------------------------------------------------------------------------
void setup();
void loop();

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Parse --fsdir / -d  (filesystem root for identity/config storage)
    const char* fsdir = "/var/lib/meshcore";
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--fsdir") == 0 || strcmp(argv[i], "-d") == 0)
                && i + 1 < argc) {
            fsdir = argv[++i];
        }
    }
    LinuxFS.setRoot(fsdir);

    // Ignore SIGPIPE (can occur when a pipe reader exits)
    signal(SIGPIPE, SIG_IGN);

    setup();

    while (true) {
        loop();
        usleep(1000);  // 1 ms yield — prevents busy-spin
    }
    return 0;
}
