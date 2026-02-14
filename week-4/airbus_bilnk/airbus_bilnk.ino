#define USER_LED_R 17 // defined the xiao pin to the chip pin 
#define USER_LED_B 25
#define USER_LED_G 16
void setup() {
  pinMode(USER_LED_R, OUTPUT); // intialised the red pin as output , so the power will be provided
  pinMode(USER_LED_G, OUTPUT);
  pinMode(USER_LED_B, OUTPUT);
  digitalWrite(USER_LED_G, HIGH);
  digitalWrite(USER_LED_B, HIGH);
}
void loop() {
  digitalWrite(USER_LED_R, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(USER_LED_R, HIGH);   // turn the LED off by making the voltage LOW
  delay(100);        
  digitalWrite(USER_LED_R, LOW);  // turn the LED on (HIGH is the voltage level)
  delay(100);                      // wait for a second
  digitalWrite(USER_LED_R, HIGH);   // turn the LED off by making the voltage LOW
  delay(1000);                  // wait for a second
}
