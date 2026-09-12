#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <tinker/tinker_app.h>

#include "justin_display.h"
#include "justin_project.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 5

MD_Parola Display(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
tinker::TinkerApp<JustinDisplay, JustinProject> app;

void setup() { app.setup(); }

void loop() { app.loop(); }
