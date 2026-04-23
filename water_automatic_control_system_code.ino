//2026/04/11 <ByDushan/> - V13: Anti-Electrolysis Pulse (Pin 7), 5-Min Override

#include <Servo.h>

// --- Pin Definitions ---
const int servoPin = 9;
const int valveLedPin = 2; 

const int redPin = 6;
const int greenPin = 5;
const int bluePin = 3;

const int t1HighPin = A0; 
const int t1LowPin = A1;  
const int t2HighPin = A2; 
const int t2LowPin = A3;  

// Override Button & Indicator Pins
const int buttonPin = 8;
const int overrideLedPin = 10;

// NEW: Anti-Electrolysis Power Pin
const int sensorPowerPin = 7; 

// --- Settings ---
Servo valveServo;
const int valveOpenPos = 0;   // Open is 0
const int valveClosePos = 105; // Close is 105 to avoid valve loose movements
int currentValvePos = 0; 
const int servoSpeedDelay = 15; 

unsigned long previousMillis = 0;
const long checkInterval = 10000;        
const long switchDisplayInterval = 5000; 

const int noiseThreshold = 200; 

// --- RGB Brightness ---
const int LED_OFF = 255;
const int LED_ON = 200; 

// --- 5-Minute Override Variables ---
bool overrideEnabled = false;       
bool isOverridingNow = false;       
unsigned long overrideStartTime = 0; 
const unsigned long OVERRIDE_TIME = 300000; 
bool buttonState = HIGH;            
bool lastButtonState = HIGH;        

void setup() {
  Serial.begin(9600);
  
  pinMode(valveLedPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  pinMode(overrideLedPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); 
  
  pinMode(t1HighPin, INPUT_PULLUP);
  pinMode(t1LowPin, INPUT_PULLUP);
  pinMode(t2HighPin, INPUT_PULLUP);
  pinMode(t2LowPin, INPUT_PULLUP);
  
  // NEW: Setup the sensor power pin and make sure it starts OFF
  pinMode(sensorPowerPin, OUTPUT);
  digitalWrite(sensorPowerPin, LOW);
  
  setRGB(LED_OFF, LED_OFF, LED_OFF); 
  digitalWrite(overrideLedPin, LOW); 
  
  Serial.println("System Starting...");
  checkTanksAndUpdateValve(); 
}

void loop() {
  unsigned long currentMillis = millis();
  unsigned long timePassed = currentMillis - previousMillis;

  // --- BUTTON TOGGLE LOGIC ---
  buttonState = digitalRead(buttonPin);
  if (lastButtonState == HIGH && buttonState == LOW) {
    overrideEnabled = !overrideEnabled;
    digitalWrite(overrideLedPin, overrideEnabled ? HIGH : LOW);
    
    if (overrideEnabled) {
      Serial.println("OVERRIDE: Armed! Will pump 5 extra mins when full.");
    } else {
      Serial.println("OVERRIDE: Disabled.");
      isOverridingNow = false; 
    }
    delay(200); 
  }
  lastButtonState = buttonState;

  // --- MAIN DISPLAY & CHECK LOGIC ---
  if (timePassed < switchDisplayInterval) {
    displayTankLevel(t1HighPin, t1LowPin);
  } 
  else if (timePassed >= switchDisplayInterval && timePassed < checkInterval) {
    displayTankLevel(t2HighPin, t2LowPin);
  } 
  else if (timePassed >= checkInterval) {
    printDiagnostics();
    checkTanksAndUpdateValve();
    
    setRGB(LED_OFF, LED_OFF, LED_OFF); 
    delay(1000);                       
    previousMillis = millis(); 
  }
}

// --- Functions ---

void setRGB(int targetR, int targetG, int targetB) {
  analogWrite(redPin, targetR);
  analogWrite(greenPin, targetG);
  analogWrite(bluePin, targetB);
}

void moveValveSlowly(int targetPos) {
  if (currentValvePos == targetPos) return; 

  valveServo.write(currentValvePos); 
  valveServo.attach(servoPin);       

  if (currentValvePos < targetPos) {
    for (int pos = currentValvePos; pos <= targetPos; pos++) {
      valveServo.write(pos);
      delay(servoSpeedDelay); 
    }
  } else {
    for (int pos = currentValvePos; pos >= targetPos; pos--) {
      valveServo.write(pos);
      delay(servoSpeedDelay);
    }
  }
  currentValvePos = targetPos; 
  
  delay(150);          
  valveServo.detach(); 
}

// NEW: Updated to pulse the power only when reading!
bool isWetDebounced(int pin) {
  digitalWrite(sensorPowerPin, HIGH); // Send 5V into the water
  delay(5);                           // Wait 5ms to let the transistor fully turn on
  
  for (int i = 0; i < 10; i++) {
    if (analogRead(pin) > noiseThreshold) {
      digitalWrite(sensorPowerPin, LOW); // Kill power immediately if dry
      return false; 
    }
    delay(100); 
  }
  
  digitalWrite(sensorPowerPin, LOW); // Kill power after confirming wet
  return true; 
}

void checkTanksAndUpdateValve() {
  bool t1IsFull = isWetDebounced(t1HighPin);
  bool t2IsFull = isWetDebounced(t2HighPin);
  
  if (t1IsFull && t2IsFull) {
    if (overrideEnabled) {
      
      if (!isOverridingNow) {
        isOverridingNow = true;
        overrideStartTime = millis();
        Serial.println("ACTION: Tanks Full -> 5-Min Override Started!");
      }
      
      if (millis() - overrideStartTime >= OVERRIDE_TIME) {
        Serial.println("ACTION: 5-Min Override Complete. Valve CLOSED.");
        digitalWrite(valveLedPin, LOW); 
        moveValveSlowly(valveClosePos); 
        
        overrideEnabled = false;
        isOverridingNow = false;
        digitalWrite(overrideLedPin, LOW);
      } else {
        digitalWrite(valveLedPin, HIGH); 
        moveValveSlowly(valveOpenPos); 
        Serial.println("ACTION: 5-Min Override Actively Pumping...");
      }
      
    } else {
      digitalWrite(valveLedPin, LOW); 
      moveValveSlowly(valveClosePos); 
      isOverridingNow = false; 
      Serial.println("ACTION: All Tanks Full. Valve CLOSED.");
    }
    
  } else {
    digitalWrite(valveLedPin, HIGH); 
    moveValveSlowly(valveOpenPos);  
    isOverridingNow = false; 
    Serial.println("ACTION: System needs water. Valve OPEN.");
  }
}

void displayTankLevel(int highSensorPin, int lowSensorPin) {
  if (isOverridingNow) {
    setRGB(LED_OFF, LED_ON, LED_OFF); 
    return; 
  }

  bool isTopWet = isWetDebounced(highSensorPin);
  bool isMidWet = isWetDebounced(lowSensorPin);
  
  if (isTopWet) {
    setRGB(LED_OFF, LED_ON, LED_OFF); // GREEN (Full)
  } 
  else if (isMidWet) {
    setRGB(LED_ON, LED_ON, LED_OFF);  // YELLOW (Half)
  } 
  else {
    setRGB(LED_ON, LED_OFF, LED_OFF); // RED (Empty)
  }
}

void printDiagnostics() {
  Serial.println("\n--- Sensor Status ---");
  Serial.print("T1 Top (A0): "); Serial.println(analogRead(t1HighPin) > noiseThreshold ? "DRY" : "WET");
  Serial.print("T1 Mid (A1): "); Serial.println(analogRead(t1LowPin) > noiseThreshold ? "DRY" : "WET");
  Serial.print("T2 Top (A2): "); Serial.println(analogRead(t2HighPin) > noiseThreshold ? "DRY" : "WET");
  Serial.print("T2 Mid (A3): "); Serial.println(analogRead(t2LowPin) > noiseThreshold ? "DRY" : "WET");
}
