#include "moon_sky.h"

#include <cmath>
#include <cstdint>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusKm = 6378.14;
// Upper limb on the horizon, after refraction and the Moon's semi-diameter.
constexpr double kRiseAltitudeDeg = -0.833;
constexpr int kRiseStepSeconds = 120;

double rev(double degrees) {
  degrees = std::fmod(degrees, 360.0);
  if (degrees < 0.0) {
    degrees += 360.0;
  }
  return degrees;
}

double sind(double degrees) { return std::sin(degrees * kPi / 180.0); }

double cosd(double degrees) { return std::cos(degrees * kPi / 180.0); }

double atan2d(double y, double x) { return std::atan2(y, x) * 180.0 / kPi; }

double dayNumber(const struct tm &utc) {
  const int year = utc.tm_year + 1900;
  const int month = utc.tm_mon + 1;
  const int day = utc.tm_mday;
  const int whole = 367 * year - 7 * (year + (month + 9) / 12) / 4 +
                    275 * month / 9 + day - 730530;
  const double hours =
      utc.tm_hour + utc.tm_min / 60.0 + utc.tm_sec / 3600.0;
  return whole + hours / 24.0;
}

struct MoonOrbit {
  double distanceEarthRadii;
  double rightAscensionDeg;
  double declinationDeg;
};

MoonOrbit moonOrbit(double day) {
  const double N = rev(125.1228 - 0.0529538083 * day);
  const double inclination = 5.1454;
  const double w = rev(318.0634 + 0.1643573223 * day);
  constexpr double a = 60.2666;
  constexpr double e = 0.054900;
  const double M = rev(115.3654 + 13.0649929509 * day);

  double eccentric = M + (180.0 / kPi) * e * sind(M) * (1.0 + e * cosd(M));
  for (int pass = 0; pass < 5; ++pass) {
    const double next =
        eccentric - (eccentric - (180.0 / kPi) * e * sind(eccentric) - M) /
                        (1.0 - e * cosd(eccentric));
    const double delta = next - eccentric;
    eccentric = next;
    if (std::fabs(delta) < 0.0005) {
      break;
    }
  }

  const double xv = a * (cosd(eccentric) - e);
  const double yv = a * (std::sqrt(1.0 - e * e) * sind(eccentric));
  const double anomaly = atan2d(yv, xv);
  double distance = std::sqrt(xv * xv + yv * yv);
  const double vw = anomaly + w;
  const double xOrbit =
      distance * (cosd(N) * cosd(vw) - sind(N) * sind(vw) * cosd(inclination));
  const double yOrbit =
      distance * (sind(N) * cosd(vw) + cosd(N) * sind(vw) * cosd(inclination));
  const double zOrbit = distance * (sind(vw) * sind(inclination));

  double longitude = atan2d(yOrbit, xOrbit);
  double latitude =
      atan2d(zOrbit, std::sqrt(xOrbit * xOrbit + yOrbit * yOrbit));

  const double sunAnomaly = rev(356.0470 + 0.9856002585 * day);
  const double sunPerihelion = 282.9404 + 4.70935e-5 * day;
  const double sunLongitude = rev(sunAnomaly + sunPerihelion);
  const double moonLongitude = rev(N + w + M);
  const double elongation = rev(moonLongitude - sunLongitude);
  const double latitudeArgument = rev(moonLongitude - N);

  longitude += -1.274 * sind(M - 2 * elongation) + 0.658 * sind(2 * elongation) -
               0.186 * sind(sunAnomaly) -
               0.059 * sind(2 * M - 2 * elongation) -
               0.057 * sind(M - 2 * elongation + sunAnomaly) +
               0.053 * sind(M + 2 * elongation) +
               0.046 * sind(2 * elongation - sunAnomaly) +
               0.041 * sind(M - sunAnomaly) - 0.035 * sind(elongation) -
               0.031 * sind(M + sunAnomaly) -
               0.015 * sind(2 * latitudeArgument - 2 * elongation) +
               0.011 * sind(M - 4 * elongation);
  latitude += -0.173 * sind(latitudeArgument - 2 * elongation) -
              0.055 * sind(M - latitudeArgument - 2 * elongation) -
              0.046 * sind(M + latitudeArgument - 2 * elongation) +
              0.033 * sind(latitudeArgument + 2 * elongation) +
              0.017 * sind(2 * M + latitudeArgument);
  distance += -0.58 * cosd(M - 2 * elongation) - 0.46 * cosd(2 * elongation);

  const double xEcliptic = distance * cosd(longitude) * cosd(latitude);
  const double yEcliptic = distance * sind(longitude) * cosd(latitude);
  const double zEcliptic = distance * sind(latitude);
  const double obliquity = 23.4393 - 3.563e-7 * day;
  const double yEquator =
      yEcliptic * cosd(obliquity) - zEcliptic * sind(obliquity);
  const double zEquator =
      yEcliptic * sind(obliquity) + zEcliptic * cosd(obliquity);

  MoonOrbit orbit;
  orbit.distanceEarthRadii = distance;
  orbit.rightAscensionDeg = atan2d(yEquator, xEcliptic);
  orbit.declinationDeg = atan2d(
      zEquator, std::sqrt(xEcliptic * xEcliptic + yEquator * yEquator));
  return orbit;
}

MoonView moonViewFromOrbit(const MoonOrbit &orbit, double day,
                           double latitudeDeg, double longitudeDeg) {
  const double sunAnomaly = rev(356.0470 + 0.9856002585 * day);
  const double sunPerihelion = 282.9404 + 4.70935e-5 * day;
  const double utHours = (day - std::floor(day)) * 24.0;
  const double lst = rev(sunAnomaly + sunPerihelion + 180.0 + utHours * 15.0 +
                         longitudeDeg);
  const double hourAngle = lst - orbit.rightAscensionDeg;
  const double x = cosd(hourAngle) * cosd(orbit.declinationDeg);
  const double y = sind(hourAngle) * cosd(orbit.declinationDeg);
  const double z = sind(orbit.declinationDeg);
  const double xHorizon = x * sind(latitudeDeg) - z * cosd(latitudeDeg);
  const double zHorizon = x * cosd(latitudeDeg) + z * sind(latitudeDeg);
  const double geometricAltitude =
      atan2d(zHorizon, std::sqrt(xHorizon * xHorizon + y * y));
  const double parallax = std::asin(1.0 / orbit.distanceEarthRadii) * 180.0 / kPi;

  MoonView view;
  view.distanceKm = orbit.distanceEarthRadii * kEarthRadiusKm;
  view.azimuthDeg = rev(atan2d(y, xHorizon) + 180.0);
  view.altitudeDeg = geometricAltitude - parallax * cosd(geometricAltitude);
  return view;
}

struct Equatorial {
  double rightAscensionDeg;
  double declinationDeg;
};

Equatorial sunEquatorial(double day) {
  const double perihelion = 282.9404 + 4.70935e-5 * day;
  const double eccentricity = 0.016709 - 1.151e-9 * day;
  const double anomaly = rev(356.0470 + 0.9856002585 * day);
  const double eccentric =
      anomaly + (180.0 / kPi) * eccentricity * sind(anomaly) *
                    (1.0 + eccentricity * cosd(anomaly));
  const double xv = cosd(eccentric) - eccentricity;
  const double yv =
      std::sqrt(1.0 - eccentricity * eccentricity) * sind(eccentric);
  const double longitude = rev(atan2d(yv, xv) + perihelion);
  const double obliquity = 23.4393 - 3.563e-7 * day;
  const double x = cosd(longitude);
  const double y = sind(longitude) * cosd(obliquity);
  const double z = sind(longitude) * sind(obliquity);
  Equatorial sun;
  sun.rightAscensionDeg = atan2d(y, x);
  sun.declinationDeg = atan2d(z, std::sqrt(x * x + y * y));
  return sun;
}

double brightLimbAt(double day, const MoonOrbit &moon, double latitudeDeg,
                    double longitudeDeg) {
  const Equatorial sun = sunEquatorial(day);
  const double sunAnomaly = rev(356.0470 + 0.9856002585 * day);
  const double sunPerihelion = 282.9404 + 4.70935e-5 * day;
  const double utHours = (day - std::floor(day)) * 24.0;
  const double lst = rev(sunAnomaly + sunPerihelion + 180.0 + utHours * 15.0 +
                         longitudeDeg);
  const double hourAngle = lst - moon.rightAscensionDeg;
  const double raGap = sun.rightAscensionDeg - moon.rightAscensionDeg;
  const double brightFromNorth = atan2d(
      cosd(sun.declinationDeg) * sind(raGap),
      cosd(moon.declinationDeg) * sind(sun.declinationDeg) -
          sind(moon.declinationDeg) * cosd(sun.declinationDeg) * cosd(raGap));
  const double latitudeRad = latitudeDeg * kPi / 180.0;
  const double zenithFromNorth = atan2d(
      sind(hourAngle) * std::cos(latitudeRad),
      std::sin(latitudeRad) * cosd(moon.declinationDeg) -
          std::cos(latitudeRad) * sind(moon.declinationDeg) * cosd(hourAngle));
  return rev(zenithFromNorth - brightFromNorth);
}

MoonView viewAt(time_t utc, double latitudeDeg, double longitudeDeg) {
  struct tm brokenDown;
  gmtime_r(&utc, &brokenDown);
  const double day = dayNumber(brokenDown);
  return moonViewFromOrbit(moonOrbit(day), day, latitudeDeg, longitudeDeg);
}

double altitudeAt(time_t utc, double latitudeDeg, double longitudeDeg) {
  return viewAt(utc, latitudeDeg, longitudeDeg).altitudeDeg;
}

time_t crossingTime(time_t earlier, double earlierAltitude, time_t later,
                    double laterAltitude) {
  const double span = laterAltitude - earlierAltitude;
  const double fraction =
      std::fabs(span) < 1e-6
          ? 0.0
          : (kRiseAltitudeDeg - earlierAltitude) / span;
  const double clamped = fraction < 0.0 ? 0.0 : (fraction > 1.0 ? 1.0 : fraction);
  return earlier + static_cast<time_t>(std::lround(
                       clamped * static_cast<double>(later - earlier)));
}

} // namespace

double moonBrightLimbDeg(time_t utc, double latitudeDeg, double longitudeDeg) {
  struct tm brokenDown;
  if (gmtime_r(&utc, &brokenDown) == nullptr) {
    return 90.0;
  }
  const double day = dayNumber(brokenDown);
  return brightLimbAt(day, moonOrbit(day), latitudeDeg, longitudeDeg);
}

MoonView moonViewAt(time_t utc, double latitudeDeg, double longitudeDeg) {
  return viewAt(utc, latitudeDeg, longitudeDeg);
}

MoonView moonViewForDay(time_t utc, int tzOffsetMinutes, double latitudeDeg,
                        double longitudeDeg) {
  MoonView view = viewAt(utc, latitudeDeg, longitudeDeg);
  const int64_t utc64 = static_cast<int64_t>(utc);
  int64_t secondOfDay = (utc64 + static_cast<int64_t>(tzOffsetMinutes) * 60) % 86400;
  if (secondOfDay < 0) {
    secondOfDay += 86400;
  }
  const time_t dayStart = static_cast<time_t>(utc64 - secondOfDay);
  double previousAltitude = altitudeAt(dayStart, latitudeDeg, longitudeDeg);
  for (int elapsed = kRiseStepSeconds; elapsed <= 86400; elapsed += kRiseStepSeconds) {
    const time_t sample = dayStart + elapsed;
    const double altitude = altitudeAt(sample, latitudeDeg, longitudeDeg);
    const time_t previous = sample - kRiseStepSeconds;
    if (!view.hasRise && previousAltitude < kRiseAltitudeDeg &&
        altitude >= kRiseAltitudeDeg) {
      view.riseUtc =
          crossingTime(previous, previousAltitude, sample, altitude);
      view.hasRise = view.riseUtc < dayStart + 86400;
    }
    if (!view.hasSet && previousAltitude >= kRiseAltitudeDeg &&
        altitude < kRiseAltitudeDeg) {
      view.setUtc = crossingTime(previous, previousAltitude, sample, altitude);
      view.hasSet = view.setUtc < dayStart + 86400;
    }
    previousAltitude = altitude;
    if (view.hasRise && view.hasSet) {
      break;
    }
  }
  return view;
}
