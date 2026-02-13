#define USER_LED_R 17 // defined the xiao pin to the chip pin 
void setup() {
  pinMode(USER_LED_R, OUTPUT); // intialised the red pin as output , so the power will be provided
}
void loop() {
  digitalWrite(USER_LED_R, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(1000);                      // wait for a second
  digitalWrite(USER_LED_R, LOW);   // turn the LED off by making the voltage LOW
  delay(1000);                      // wait for a second
}
