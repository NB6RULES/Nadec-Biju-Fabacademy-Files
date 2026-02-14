// Pin definitions
#define LED1 D0    // D0 → first LED (this one gonna shine first)
#define LED2 D1    // D1 → second LED
#define LED3 D2    // D2 → third LED
#define BUTTON D3  // D3 → button input (main trigger)

// Variables
int currentLED = 0;           
// tracks which LED is active rn (0,1,2) and 3 = all off 😴

bool lastButtonState = HIGH;  
// previous button state (HIGH = not pressed, chill state)

bool buttonState = HIGH;      
// current confirmed button state after debounce

unsigned long lastDebounceTime = 0;
// last time the button changed (used to filter noise)

unsigned long debounceDelay = 50; 
// 50ms cooldown so button doesn’t spam fake presses

void setup() {
  Serial.begin(9600);
  // start serial → for printing debug stuff to console

  // Set LED pins as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  // LEDs → output because we control them

  // Set button pin as input with internal pull-up
  pinMode(BUTTON, INPUT_PULLUP);
  // default = HIGH, press = LOW (inverted logic but stable af)

  // Turn off all LEDs initially
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  // clean start, no random glowing nonsense

  Serial.println("System ready. Press button to cycle LEDs.");
  // just letting you know it’s alive 😎
}

void loop() {
  // Read the button state
  bool reading = digitalRead(BUTTON);
  // grab live button value

  // Check if button state changed (debounce trigger)
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
    // state changed → start debounce timer
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // if stable for 50ms → legit press

    if (reading != buttonState) {
      buttonState = reading;
      // update official button state

      // If button was just pressed (HIGH → LOW)
      if (buttonState == LOW) {
        // button press detected 🔘

        // Turn off all LEDs first (reset everything)
        digitalWrite(LED1, LOW);
        digitalWrite(LED2, LOW);
        digitalWrite(LED3, LOW);

        // Move to next LED
        currentLED++;
        // go to next stage in the cycle

        if (currentLED > 3) {
          currentLED = 0; 
          // loop back → infinite cycle 🔁
        }

        // Turn on the selected LED
        switch(currentLED) {
          case 0:
            digitalWrite(LED1, HIGH);
            Serial.println("LED 1 ON");
            // LED1 glow mode ✨
            break;

          case 1:
            digitalWrite(LED2, HIGH);
            Serial.println("LED 2 ON");
            // LED2 turn 🔥
            break;

          case 2:
            digitalWrite(LED3, HIGH);
            Serial.println("LED 3 ON");
            // LED3 flex 😎
            break;

          case 3:
            Serial.println("All LEDs OFF");
            // everyone off → nap time 😴
            break;
        }
      }
    }
  }

  lastButtonState = reading;
  // store current reading for next loop
}
