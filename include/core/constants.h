#pragma once

namespace constants {

// ── Mathematical ─────────────────────────────────────────────────────────────
inline constexpr double PI      = 3.14159265358979323846;
inline constexpr double TWO_PI  = 2.0 * PI;
inline constexpr double DEG2RAD = PI / 180.0;
inline constexpr double RAD2DEG = 180.0 / PI;

// ── Earth gravitational parameter ────────────────────────────────────────────
inline constexpr double MU_EARTH  = 398600.4418;   // km³/s²

// ── Earth geometry ───────────────────────────────────────────────────────────
inline constexpr double R_EARTH   = 6378.137;      // km  (equatorial radius)
inline constexpr double J2        = 1.08263e-3;    // oblateness coefficient
inline constexpr double OMEGA_E   = 7.2921150e-5;  // rad/s  (Earth rotation rate)

// ── Physical ─────────────────────────────────────────────────────────────────
inline constexpr double G0        = 9.80665;       // m/s²  (standard gravity)

// ── Standard atmosphere (exponential model) ──────────────────────────────────
inline constexpr double RHO0      = 1.225;         // kg/m³ (sea-level density)
inline constexpr double H_SCALE   = 8.5;           // km    (scale height)
inline constexpr double P0        = 101325.0;      // Pa    (sea-level pressure)
inline constexpr double T0        = 288.15;        // K     (sea-level temperature)

// ── Rendering ────────────────────────────────────────────────────────────────
inline constexpr double GL_SCALE  = 1.0 / R_EARTH; // 1 GL unit = 1 R_E

} // namespace constants
