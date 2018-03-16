/*
 * Measures and prints frequency and duty cycle of incoming
 * square wave signal.
 */

IntervalTimer measurementTime;
IntervalTimer signalASmooth;
IntervalTimer signalBSmooth;

#define SIGNAL_IN_PIN_A 5
#define SIGNAL_IN_PIN_B 6
#define LED_PIN 13
#define FUNDING_ROUND_A 7
#define FUNDING_ROUND_B 8
#define FREQ_A 950 //Hz
#define FREQ_B 950 //Hz //TO-DO CHANGE THIS
#define WINNING HIGH
#define LOSING LOW
#define MEASUREMENT_INTERVAL 1000000 //microseconds
#define SMOOTH_TIME_A 650 //microseconds
#define SMOOTH_TIME_B 650

volatile bool timeToOut_A;
volatile bool timeToOut_B;
volatile unsigned long count_A;
volatile unsigned long dutyOn_A;
volatile unsigned long dutyOff_A;
volatile unsigned long count_B;
volatile unsigned long dutyOn_B;
volatile unsigned long dutyOff_B;

volatile unsigned long dutyLength_A;
volatile unsigned long dutyLength_B;

/* 
 * Sets up interrupt to measure frequency and timer to measure/update PWM,
 * and initializes module variables and pin modes.
 */
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  Serial.begin(9600);
  Serial.println("Ready");
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_A), measureFreq_A, RISING);
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_B), measureFreq_B, RISING);
  count_A = 0;
  count_B = 0;
  timeToOut_A = false;
  timeToOut_B = false;
  dutyOn_A = 0;
  dutyOff_A = 0;
  dutyOn_B = 0;
  dutyOff_B = 0;
  pinMode(SIGNAL_IN_PIN_A, INPUT);
  pinMode(SIGNAL_IN_PIN_B, INPUT);
  pinMode(FUNDING_ROUND_A, OUTPUT);
  pinMode(FUNDING_ROUND_B, OUTPUT);
  digitalWrite(FUNDING_ROUND_A, WINNING);
  digitalWrite(FUNDING_ROUND_B, WINNING);
  measurementTime.begin(measureOn, MEASUREMENT_INTERVAL);
}

/*
 * Measurement time callback - measures in duty cycles. 
 */
void measureOn() {
  detachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_A));
  detachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_B));
  dutyLength_A = pulseIn(SIGNAL_IN_PIN_A, LOW, 1500);
  if (dutyLength_A < 5)  dutyLength_A = pulseIn(SIGNAL_IN_PIN_A, LOW, 1500);
  dutyLength_B = pulseIn(SIGNAL_IN_PIN_B, LOW, 1500);
  if (dutyLength_B < 5)  dutyLength_B = pulseIn(SIGNAL_IN_PIN_B, LOW, 1500);
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_A), measureFreq_A, RISING);
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_B), measureFreq_B, RISING);
  timeToOut_A = true;
  timeToOut_B = true;
}

/*
 * ISR for frequency measurement interrupt for A. Increments frequency count.
 */
void measureFreq_A() {
  count_A++;
  detachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_A));
  signalASmooth.begin(endSmoothA, SMOOTH_TIME_A);
}


/*
 * ISR for frequency measurement interrupt for B. Increments frequency count.
 */
void measureFreq_B() {
  count_B++;
  detachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_B));
  signalBSmooth.begin(endSmoothB, SMOOTH_TIME_B);
}

//Ends A smoothing
void endSmoothA() {
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_A), measureFreq_A, RISING);
  signalASmooth.end();
}

//Ends B smoothing
void endSmoothB() {
  attachInterrupt(digitalPinToInterrupt(SIGNAL_IN_PIN_B), measureFreq_B, RISING);
  signalBSmooth.end();
}

/*
 * Checks if timeToPrint bool is true. If so, prints current frequency
 * count and calculated PWM value to serial monitor. Resets duty cycle times,
 * frequency count, and timeToPrint bool.
 */
void loop() {
  if (timeToOut_A) {
    unsigned long calcFreq_A = count_A * 1000000/ MEASUREMENT_INTERVAL;
    int PWM_A = (dutyLength_A)*calcFreq_A/10000;
    Serial.print("Current freq A: ");
    Serial.println(calcFreq_A);
    Serial.print("Current PWM A: ");
    Serial.print(PWM_A);
    Serial.println("%");
    Serial.print("Assumed PWM A: ");
    if (PWM_A < 50) Serial.println("25%");
    else Serial.println("75%");
    if (calcFreq_A > 0.9*FREQ_A && calcFreq_A < 1.1*FREQ_A && PWM_A < 100 && PWM_A > 5) {
      if (PWM_A > 60) digitalWrite(FUNDING_ROUND_A, WINNING);
      else digitalWrite(FUNDING_ROUND_A, LOSING);
    } else digitalWrite(FUNDING_ROUND_A, WINNING);
    dutyOn_A = 0;
    dutyOff_A = 0;
    count_A = 0;
    timeToOut_A = false;
  }
  if (timeToOut_B) {
    unsigned long calcFreq_B = count_B * 1000000/ MEASUREMENT_INTERVAL;
    int PWM_B = dutyLength_B*calcFreq_B/10000;
    Serial.print("Current freq B: ");
    Serial.println(calcFreq_B);
    Serial.print("Current PWM B: ");
    Serial.print(PWM_B);
    Serial.println("%");
    Serial.print("Assumed PWM B: ");
    if (PWM_B < 50) Serial.println("25%");
    else Serial.println("75%");
    if (calcFreq_B > 0.9*FREQ_B && calcFreq_B < 1.1*FREQ_B && PWM_B < 100 && PWM_B > 5) {
      if (PWM_B > 60) digitalWrite(FUNDING_ROUND_B, WINNING);
      else digitalWrite(FUNDING_ROUND_B, LOSING);
    } else digitalWrite(FUNDING_ROUND_B, WINNING);
    dutyOn_B = 0;
    dutyOff_B = 0;
    count_B = 0;
    timeToOut_B = false;
  }
}
