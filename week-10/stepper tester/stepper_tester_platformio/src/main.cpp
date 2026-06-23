/*
 * Stepper Motor Tester - ESP32-WROOM-32E + DRV8825
 * Standalone, no WiFi/AP (was drawing too much current).
 *
 * Pinout:
 *   GPIO4  -> DRV8825 DIR
 *   GPIO16 -> DRV8825 STEP
 *   GPIO17 -> DRV8825 EN (active LOW)
 *   GPIO26 -> status LED
 *
 * M0/M1/M2 are wired to an external DIP switch (3V3 or GND), not to the
 * ESP32, so firmware cannot read or set the microstep mode in hardware.
 * The "microstep sanity" phase pauses and prompts over Serial so you can
 * flip the DIP switches by hand before it runs the scaled-up step count.
 *
 * Boot sequence:
 *   1. LED solid ON for 5s
 *   2. Run the full diagnostic test sequence (LED blinks rapidly the whole time)
 *   3. LED solid ON, wait 5s
 *   4. Repeat from step 2
 */

#include <Arduino.h>

#define DIR_PIN 4
#define STEP_PIN 16
#define EN_PIN 17
#define LED_PIN 26

#define DEFAULT_STEPS_PER_REV 200
#define MIN_STEP_HIGH_US 2
#define MIN_RPM 1

#define LED_BUSY_PERIOD_MS 80
#define BOOT_LED_ON_MS 5000UL
#define IDLE_LED_ON_MS 5000UL

void setEnabled(bool en) {
  digitalWrite(EN_PIN, en ? LOW : HIGH);
  Serial.println(en ? "Driver: ENABLED" : "Driver: disabled");
}

// Call frequently from any blocking loop to keep the LED blinking fast
// without needing interrupts.
unsigned long ledFastLast = 0;
bool ledFastState = false;
void serviceFastBlink() {
  unsigned long now = millis();
  if (now - ledFastLast >= LED_BUSY_PERIOD_MS) {
    ledFastLast = now;
    ledFastState = !ledFastState;
    digitalWrite(LED_PIN, ledFastState);
  }
}

// delay() replacement that keeps the fast blink alive during pauses
void pause(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    serviceFastBlink();
  }
}

unsigned long computeStepDelayUs(int rpm, int stepsPerRev) {
  if (rpm < MIN_RPM) rpm = MIN_RPM;
  unsigned long delayUs = 30000000UL / ((unsigned long)rpm * stepsPerRev);
  if (delayUs < MIN_STEP_HIGH_US) delayUs = MIN_STEP_HIGH_US;
  return delayUs;
}

// Constant-speed run, instant start (no ramp) - this doubles as the
// "pull-in" test pattern since it jumps straight to target RPM.
void runFixedSteps(long steps, bool cw, int rpm, int spr) {
  digitalWrite(DIR_PIN, cw ? HIGH : LOW);
  unsigned long d = computeStepDelayUs(rpm, spr);
  for (long i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(d);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(d);
    serviceFastBlink();
  }
}

void runForRevs(float revs, bool cw, int rpm, int spr) {
  long steps = (long)(revs * spr + 0.5f);
  runFixedSteps(steps, cw, rpm, spr);
}

// Linear speed ramp from startRpm to endRpm over durationMs.
void rampSteps(int startRpm, int endRpm, unsigned long durationMs, bool cw, int spr) {
  digitalWrite(DIR_PIN, cw ? HIGH : LOW);
  unsigned long t0 = millis();
  unsigned long elapsed;
  while ((elapsed = millis() - t0) < durationMs) {
    float frac = (float)elapsed / (float)durationMs;
    int rpm = startRpm + (int)((endRpm - startRpm) * frac);
    unsigned long d = computeStepDelayUs(rpm, spr);
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(d);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(d);
    serviceFastBlink();
  }
}

// 1) Clockwise
void testCw(int spr) {
  Serial.println("[test] CW - 1 rev @ 60 RPM");
  runForRevs(1, true, 60, spr);
}

// 2) Anti-clockwise
void testCcw(int spr) {
  Serial.println("[test] CCW - 1 rev @ 60 RPM");
  runForRevs(1, false, 60, spr);
}

// 3) Step - one full rev in single full steps, slow and visible
void testStep(int spr) {
  Serial.println("[test] STEP - 1 rev, single steps, slow (8 RPM)");
  runForRevs(1, true, 8, spr);
}

// 4) RPM - run at a fixed RPM for a set number of revs, then stop
void testRpm(int spr) {
  Serial.println("[test] RPM - 120 RPM for 5 revs");
  runForRevs(5, true, 120, spr);
}

// 5) Stall-speed sweep - ramp RPM upward in steps, a few revs at each
//    level, so you can watch where the shaft mark stops landing true.
void testStallSweep(int spr) {
  const int startRpm = 60;
  const int stepRpm = 60;
  const int maxRpm = 600;
  const float revsPerLevel = 3;
  Serial.println("[test] STALL-SPEED SWEEP");
  for (int rpm = startRpm; rpm <= maxRpm; rpm += stepRpm) {
    Serial.print("  -> ");
    Serial.print(rpm);
    Serial.println(" RPM, 3 revs");
    runForRevs(revsPerLevel, true, rpm, spr);
    pause(800);
  }
}

// 6) Pull-in / max start speed - instant jump to target RPM, no ramp,
//    increasing each attempt, to find the instant-start ceiling.
void testPullIn(int spr) {
  const int startRpm = 60;
  const int stepRpm = 30;
  const int maxRpm = 400;
  const float revsPerAttempt = 2;
  Serial.println("[test] PULL-IN / MAX START SPEED");
  for (int rpm = startRpm; rpm <= maxRpm; rpm += stepRpm) {
    Serial.print("  -> instant start at ");
    Serial.print(rpm);
    Serial.println(" RPM, 2 revs");
    runForRevs(revsPerAttempt, true, rpm, spr);
    pause(800);
  }
}

// 7) Acceleration ceiling - ramp start->target over a shrinking window
//    each pass, to find how hard the motor can be pushed before it stalls.
void testAccelCeiling(int spr) {
  const int startRpm = 30;
  const int targetRpm = 400;
  const long minRampMs = 200;
  long rampMs = 3000;
  Serial.println("[test] ACCELERATION CEILING");
  while (rampMs >= minRampMs) {
    Serial.print("  -> ramp ");
    Serial.print(startRpm);
    Serial.print(" to ");
    Serial.print(targetRpm);
    Serial.print(" RPM over ");
    Serial.print(rampMs);
    Serial.println(" ms");
    rampSteps(startRpm, targetRpm, rampMs, true, spr);
    pause(800);
    rampMs -= 400;
  }
}

// 8) Microstep sanity - run a full-step reference rev, then prompt for a
//    manual DIP switch change, then run the scaled-up 1/16 step rev.
void testMicrostepSanity(int spr) {
  Serial.println("[test] MICROSTEP SANITY - full-step reference, 1 rev @ 30 RPM");
  runForRevs(1, true, 30, spr);
  pause(500);

  Serial.println("[test] MICROSTEP SANITY - set DIP to 1/16 step (M2=1,M1=0,M0=0) now, 3s...");
  pause(3000);

  Serial.println("[test] MICROSTEP SANITY - 1/16 step, 1 rev @ 30 RPM");
  runForRevs(1, true, 30, spr * 16);
}

void runDiagnostics() {
  const int spr = DEFAULT_STEPS_PER_REV; // assumes DIP set to full step

  setEnabled(true);

  testCw(spr);
  pause(500);

  testCcw(spr);
  pause(500);

  testStep(spr);
  pause(500);

  testRpm(spr);
  pause(500);

  testStallSweep(spr);
  pause(500);

  testPullIn(spr);
  pause(500);

  testAccelCeiling(spr);
  pause(500);

  testMicrostepSanity(spr);

  setEnabled(false);
  Serial.println("[test] sequence complete");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nstepper_tester booting");

  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  setEnabled(false);

  digitalWrite(LED_PIN, HIGH);
  delay(BOOT_LED_ON_MS);
}

void loop() {
  runDiagnostics();

  digitalWrite(LED_PIN, HIGH);
  delay(IDLE_LED_ON_MS);
}
