#pragma once
#include "ButtonInput.h"

// Compile-time regression tests execute the real state machine with a fake clock.
namespace ButtonChecks {
constexpr bool singleClick() {
  ButtonInput b; b.begin(false, 0);
  b.update(true,100,false); b.update(true,140,false);
  b.update(false,200,false);
  if (b.update(false,280,false) != ButtonInput::NONE) return false;
  return b.update(false,601,false) == ButtonInput::NEXT_VIEW &&
         b.update(false,1000,false) == ButtonInput::NONE;
}
constexpr bool doubleClick() {
  ButtonInput b; b.begin(false,0);
  b.update(true,100,false); b.update(true,140,false);
  b.update(false,200,false); b.update(false,280,false);
  b.update(true,350,false); b.update(true,390,false);
  b.update(false,450,false);
  return b.update(false,530,false) == ButtonInput::AUTO &&
         b.update(false,1000,false) == ButtonInput::NONE;
}
constexpr bool holdAndResume() {
  ButtonInput b; b.begin(false,0);
  b.update(true,100,false); b.update(true,140,false);
  if (b.update(true,5099,false) != ButtonInput::NONE) return false;
  if (b.update(true,5100,false) != ButtonInput::STANDBY) return false;
  if (b.update(true,8000,true) != ButtonInput::NONE) return false;
  b.update(false,8010,true);
  if (b.update(false,8090,true) != ButtonInput::NONE) return false;
  b.update(true,8100,true);
  if (b.update(true,8140,true) != ButtonInput::RESUME) return false;
  // Holding the resume press must not put the device back in standby.
  if (b.update(true,14000,false) != ButtonInput::NONE) return false;
  b.update(false,14010,false);
  return b.update(false,14090,false) == ButtonInput::NONE &&
         b.update(false,15000,false) == ButtonInput::NONE;
}
constexpr bool cancelledHold() {
  ButtonInput b; b.begin(false,0);
  b.update(true,100,false); b.update(true,140,false);
  b.update(false,5090,false); // Release just before five seconds.
  if (b.update(false,5100,false) != ButtonInput::NONE) return false;
  return b.update(false,5170,false) == ButtonInput::CANCEL_HOLD &&
         b.update(false,6000,false) == ButtonInput::NONE;
}
constexpr bool bounceAndBoot() {
  ButtonInput b; b.begin(false,0);
  b.update(true,100,false); b.update(true,140,false);
  b.update(false,1000,false); b.update(true,1020,false);
  if (!b.pressed) return false;
  if (b.update(true,5100,false) != ButtonInput::STANDBY) return false;
  ButtonInput held; held.begin(true,0);
  return held.update(true,10000,false) == ButtonInput::NONE;
}
constexpr bool rollover() {
  ButtonInput b; b.begin(false,0xFFFFFF00U);
  b.update(true,0xFFFFFF10U,false); b.update(true,0xFFFFFF40U,false);
  return b.update(true,0x00001298U,false) == ButtonInput::STANDBY;
}
static_assert(singleClick(), "Single click must change views once, after release");
static_assert(doubleClick(), "Double click must emit AUTO without a page change");
static_assert(holdAndResume(), "Hold, release, resume must work without an arming delay");
static_assert(cancelledHold(), "Release before five seconds must cancel, without clicking");
static_assert(bounceAndBoot(), "Bounce and a held startup button must not emit clicks");
static_assert(rollover(), "Button timing must survive millis rollover");
constexpr bool delayedLoopRelease() {
  ButtonInput b; b.begin(false,0);
  b.update(true,100,false); b.update(true,140,false);
  // No calls during the hold; next sample is release after five seconds.
  b.update(false,5200,false);
  return b.update(false,5280,false) == ButtonInput::STANDBY &&
         b.update(false,6000,true) == ButtonInput::NONE;
}
static_assert(delayedLoopRelease(), "A recorded 5s hold must work despite a slow loop");
}

constexpr bool configurableHoldChecks() {
  ButtonInput b; b.holdMs=2000; b.begin(false,0); b.update(true,100,false); b.update(true,140,false);
  if(b.update(true,2099,false)!=ButtonInput::NONE || b.update(true,2100,false)!=ButtonInput::STANDBY)return false;
  ButtonInput c; c.holdMs=15000; c.begin(false,0); c.update(true,100,false); c.update(true,140,false);
  return c.update(true,5100,false)==ButtonInput::NONE && c.countdown(5100)==10 && c.update(true,15100,false)==ButtonInput::STANDBY;
}
static_assert(configurableHoldChecks(),"Configured button hold must change threshold and countdown");
