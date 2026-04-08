#pragma once

// Aggregates all runtime-tweakable state so modules don't need to share globals.
struct UIState {
    // ── Simulation controls ───────────────────────────────────────────────────
    bool   playing   = true;
    double timeScale = 60.0;   // real-time multiplier
    double simTime   = 0.0;    // current mission elapsed time (s)
    double simMax    = 86400.0; // simulation horizon (s)

    // ── Display toggles ───────────────────────────────────────────────────────
    bool showOrbit         = true;
    bool showTrail         = true;
    bool showGroundTrack   = false;
    bool showAxes          = true;
    bool showSigmaEllipses = false;

    // ── Camera ────────────────────────────────────────────────────────────────
    float camYaw    =  30.0f;
    float camPitch  =  20.0f;
    float camRadius =   4.5f;

    // ── Slider drag state ─────────────────────────────────────────────────────
    bool sliderDrag = false;
};
