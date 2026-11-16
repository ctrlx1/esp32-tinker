#pragma once

#include <time.h>

struct MoonView {
  double distanceKm = 0;
  double azimuthDeg = 0;
  double altitudeDeg = 0;
  bool hasRise = false;
  bool hasSet = false;
  time_t riseUtc = 0;
  time_t setUtc = 0;
};

// Geocentric distance, plus topocentric azimuth and altitude for one instant.
MoonView moonViewAt(time_t utc, double latitudeDeg, double longitudeDeg);

// Clockwise degrees from straight up to the bright limb. 90 is bright on the right.
double moonBrightLimbDeg(time_t utc, double latitudeDeg, double longitudeDeg);

// Rise and set during the local civil day that contains utc.
// The view's distance, azimuth, and altitude are for utc itself.
MoonView moonViewForDay(time_t utc, int tzOffsetMinutes, double latitudeDeg,
                        double longitudeDeg);
