#pragma once
#include <stdint.h>

// Pure input state machine: no delays, GPIO access, or display dependencies.
class ButtonInput {
 public:
  enum Action { NONE, NEXT_VIEW, AUTO, STANDBY, RESUME, CANCEL_HOLD };
  static constexpr uint32_t HOLD_MS = 5000;
  uint32_t holdMs = HOLD_MS;
  bool pressed = false;
  bool edge = false;

  constexpr void begin(bool down, uint32_t now) {
    raw = pressed = down;
    changedAt = pressedAt = now;
    consumed = down; // A button held during boot must first be released.
    pending = secondClick = false;
    edge = false;
  }

  constexpr Action update(bool down, uint32_t now, bool standby) {
    edge = false;
    if (raw != down) { raw = down; changedAt = now; }
    if (raw != pressed && elapsed(now, changedAt) >= (raw ? 40U : 80U)) {
      pressed = raw;
      edge = true;
      if (pressed) {
        pressedAt = changedAt;
        consumed = false;
        secondClick = pending && elapsed(pressedAt, releasedAt) <= 400;
        if (standby) {
          consumed = true;
          pending = secondClick = false;
          return RESUME; // This press cannot also click or enter standby.
        }
      } else if (!consumed) {
        uint32_t duration = elapsed(changedAt, pressedAt);
        // A slow loop may first observe release after the hold threshold.
        if (duration >= holdMs) {
          consumed = true;
          pending = secondClick = false;
          return STANDBY;
        }
        if (duration > 700) {
          pending = secondClick = false;
          return CANCEL_HOLD;
        }
        if (secondClick) {
          pending = secondClick = false;
          return AUTO;
        }
        pending = true;
        releasedAt = changedAt;
      }
    }
    if (pressed && raw && !consumed && elapsed(now, pressedAt) >= holdMs) {
      consumed = true;
      pending = secondClick = false;
      return STANDBY;
    }
    // Once a press becomes a hold, discard any preceding pending click.
    if (pressed && elapsed(now, pressedAt) > 700) pending = secondClick = false;
    if (!pressed && !raw && pending && elapsed(now, releasedAt) > 400) {
      pending = false;
      return NEXT_VIEW;
    }
    return NONE;
  }

  constexpr uint32_t countdown(uint32_t now) const {
    if (!pressed || !raw || consumed) return 0;
    uint32_t held = elapsed(now, pressedAt);
    return held >= holdMs ? 0 : (holdMs - held + 999) / 1000;
  }

 private:
  bool raw = false, consumed = false, pending = false, secondClick = false;
  uint32_t changedAt = 0, pressedAt = 0, releasedAt = 0;
  static constexpr uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
};
