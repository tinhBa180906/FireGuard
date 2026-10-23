#pragma once
#include <stdint.h>

// Independent from Arduino so that the state machine can be tested on a PC.
struct HoldSignal {
  bool tracking = false;
  uint32_t since = 0;
  bool update(bool value, uint32_t now, uint32_t duration) {
    if (!value) { tracking = false; return false; }
    if (!tracking) { tracking = true; since = now; }
    return uint32_t(now - since) >= duration;
  }
};

struct Inputs {
  bool ready = false, smoke = false, flame = false, hot = false;
  bool smokeRaw = false, flameRaw = false;
  bool fault = false, helpPressed = false, resetPressed = false;
  bool armSwitch = false;
};

struct ControlLogic {
  bool fireLatched = false, helpLatched = false;
  bool pump = false, fan = false, pumpUsed = false;
  bool pumpPermitIncident = false;
  bool armOffSeen = false;
  bool resetAccepted = false, resetRejected = false;
  uint32_t pumpSince = 0;
  HoldSignal safeHold;

  void step(const Inputs& x, uint32_t now) {
    resetAccepted = resetRejected = false;
    if (!x.armSwitch) armOffSeen = true;
    const bool fireNow = x.flame || (x.ready && x.smoke && x.hot);
    const bool quiet = x.ready && !x.fault && !x.smokeRaw &&
                       !x.flameRaw && !x.hot && !x.helpPressed;
    const bool safe = safeHold.update(quiet, now, 10000);
    if (x.helpPressed) helpLatched = true;

    if (fireNow && !fireLatched) {
      fireLatched = true;
      // ARM must already be enabled at the start of the incident.
      pumpPermitIncident = x.ready && !x.fault && x.armSwitch && armOffSeen;
    }
    // One water pulse per incident, when a second sensor corroborates flame.
    if (fireLatched && !pumpUsed && pumpPermitIncident && x.ready && !x.fault &&
        x.flame && (x.smoke || x.hot) && x.armSwitch) {
      pumpUsed = true;
      pump = true;
      pumpSince = now;
    }
    // Opening ARM, losing flame, a fault or timeout stops the pulse.
    if (pump && (!x.armSwitch || !x.flameRaw || x.fault ||
                 uint32_t(now - pumpSince) >= 3000)) pump = false;

    if (x.resetPressed) {
      if (safe && !x.armSwitch) {
        fireLatched = helpLatched = pump = pumpUsed = pumpPermitIncident = false;
        resetAccepted = true;
      } else resetRejected = true;
    }
    // Cooling only: never an automatic smoke extraction fan.
    fan = x.ready && x.hot && !x.smokeRaw && !x.flameRaw &&
          !x.fault && !fireLatched && !helpLatched;
  }
};
