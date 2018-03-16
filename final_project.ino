/*---------------Libraries to Include--------------------------*/
#include <Servo.h>
#include <RunningAverage.h>
#include <Wire.h>
#include <I2CEncoder.h>

/*---------------Pin Designations and Constants--------------------------*/
#define RIGHT_TAPE A2
#define LEFT_TAPE A1
#define RIGHT_TAPE_BACK A3
#define LEFT_TAPE_BACK A0
#define FAR_RIGHT_TAPE A9
#define BALL_SERVO 0
#define MOTOR1_RPWM A6
#define MOTOR1_LPWM A7
#define MOTOR2_RPWM 4
#define MOTOR2_LPWM A8
#define BUTTON_PIN 12
#define LED_PIN 13
#define LEFT_BEACON_SERVO 5
#define RIGHT_BEACON_SERVO 8
#define LEFT_BEACON_PIN 1
#define RIGHT_BEACON_PIN 2
#define HAMMER_SERVO 7
#define LIMIT 6

/*---------------Actuator constants--------------------------*/
#define SERVO_CENTER 60
#define HIGH_SPEED 120  //210 max
#define LOW_SPEED  100

#define HIGH_SPEED_BACK 80  //210 max
#define LOW_SPEED_BACK  60

/*---------------Beacon Tracker Constants--------------------------*/
#define Y_START_TO_A 41.16 //5.2
#define Y_START_TO_MIDDLE 58.97 //7.45
#define Y_START_TO_B 71.5 //9.79
#define X_SENSOR_TO_BEACON 12
#define ROTATION_VAL 3 //TODO - pick this.

/*---------------Timer Constants--------------------------*/
#define BALL_RELEASE_TIME 1500000
#define BOT_HAMMER_TIME 3000000
#define GREY_READ_TIME 2000
#define RELOAD_TIME 1500000
#define GAME_END_TIME 129000000
#define IGNORE_TIME 500000
#define TIME_TO_DRIVE 1000000

const float pi = 3.14;

/*---------------State Definitions--------------------------*/
typedef enum {
  CAL_BLACK, CAL_GREY, FORWARD, REVERSE, STOP_WAIT, FINISH, STOP_WAIT_HAMMER, BOOTY_BUMP, BOOTY_BUMP_RETURN, DRIVE_TO_BOX, WAITING_TO_START
} States_t;

char* stateNames[11] = {"CAL_BLACK", "CAL_GREY", "FORWARD", "REVERSE","STOP_WAIT", "FINISH","STOP_WAIT_HAMMER","BOOTY_BUMP", "BOOTY_BUMP_RETURN", "DRIVE_TO_BOX", "WAITING_TO_START" };

volatile States_t state;

/*---------------Motor Definitions--------------------------*/
typedef enum {
  RIGHT, LEFT
} Motor_t;

/*---------------Game State Definitions--------------------------*/
typedef enum {
  LOSING, WINNING
} beacon_states_t;

char* frStates[2] = {"LOSING, WINNING"};

/*---------------Module Variables and Objects--------------------------*/
int tape;
//These values can be reset during calibration
int BLACK_THRESH = 750;
int GREY_THRESH = 580;
int BLACK_BACK_THRESH = 730;
int greyCounter;
bool onGrey;
bool CALIBRATE_TOGGLE = true;
int targetGreyCounter;
bool readingFromBeacons = false;
bool reload_toggled = false;
int BOOTY_BUMP_DIST = 5.5;
volatile bool checkTape;
volatile bool timerElapsed = false;

//Default definition for start time - changed when the robot enters the startup garage initially
unsigned long START_TIME = 1000000000000;

int frA = LOSING;
int frB = LOSING;

IntervalTimer settleDownTimer;
IntervalTimer ballsReleaseTimer;
IntervalTimer hammerTime;
IntervalTimer readGreyTimer;
IntervalTimer reloadTimer;
IntervalTimer ignoreTape;
Servo ballServo;
Servo servoA; //for funding Round A
Servo servoB; //for funding Round B
Servo hammerServo;
I2CEncoder encoder;
RunningAverage greyHistory(10);

void setup() {
  pinMode(RIGHT_TAPE, INPUT);
  pinMode(LEFT_TAPE, INPUT);
  pinMode(MOTOR1_RPWM, OUTPUT);
  pinMode(MOTOR1_LPWM, OUTPUT);
  pinMode(MOTOR2_RPWM, OUTPUT);
  pinMode(MOTOR2_LPWM, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEFT_BEACON_PIN, INPUT);
  pinMode(RIGHT_BEACON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN,HIGH);
  pinMode(LIMIT, INPUT_PULLUP);
  //digitalWrite(LED_PIN, HIGH);
  ballServo.attach(BALL_SERVO);
  ballServo.write(SERVO_CENTER);
  servoA.attach(RIGHT_BEACON_SERVO);
  servoB.attach(LEFT_BEACON_SERVO);
  servoA.write(SERVO_CENTER);
  hammerServo.attach(HAMMER_SERVO);
  hammerServo.write(10);
  greyHistory.clear();
  greyCounter = 0;
  onGrey = false;
  targetGreyCounter = 3; //AIM FOR FUNDING B AT START
  state = CAL_BLACK;
  Serial.begin(9600);
  Serial.println("Type a key to record black threshold.");

  //Initialize the encoder
  Wire.begin();
  encoder.init((2.0/3.0)*MOTOR_393_SPEED_ROTATIONS, MOTOR_393_TIME_DELTA);
  readGreyTimer.begin(testForGrey, GREY_READ_TIME);
  checkTape = true;
}

void doneIgnoring(){
  checkTape = true;
  ignoreTape.end();
}

unsigned char testForKey(void){
  unsigned char KeyEventOccurred;
  KeyEventOccurred = Serial.available();
  while(Serial.available()) Serial.read();
  return KeyEventOccurred;
}

void loop() {
  if(millis() > START_TIME + 129000){
    //End the game
    runMotor(LEFT, 0);
    runMotor(RIGHT, 0);
    changeState(FINISH);
  }
  trackBeacon();
  //if (readingFromBeacons && (state == REVERSE || state == FORWARD)){
    //checkBeaconValue();
  //}
  switch (state) {
    case CAL_BLACK:
      calibrateBlack();
      break;
    case CAL_GREY:
      calibrateGrey();
      break;
    case DRIVE_TO_BOX:
      driveToBox();
      break;
    case WAITING_TO_START:
      waitToStart();
      break;
    case FORWARD:
      //digitalWrite(LED_PIN, LOW);
      moveForward();
      break;
    case REVERSE:
      moveReverse();
      break;
    case STOP_WAIT:
      stopAndWait();
      break;
    case STOP_WAIT_HAMMER:
      stopAndWaitHammer();
      break;
    case BOOTY_BUMP:
      bootyBump();
      break;
    case BOOTY_BUMP_RETURN:
      bootyBumpReturn();
      break;
    case FINISH:
      finishGame();
      break;
    default:    // Should never get into an unhandled state
      Serial.println("What is this I do not even...");
  }
}

/*---------------Calibration--------------------------*/

void calibrateBlack(void){
  Serial.println("Calib black");
  // Only calibrate if the toggle is on.
  if (CALIBRATE_TOGGLE){
    while(true){
      if(!digitalRead(BUTTON_PIN)){
        break;
      }
      delay(10);
    }
    int right = analogRead(RIGHT_TAPE);
    int left = analogRead(LEFT_TAPE);
    BLACK_THRESH = min(right, left) - 75;
    Serial.println(BLACK_THRESH);
    Serial.println("Calib black done");

    //Calibrate rear tape sensors
    while(true){
      if(!digitalRead(BUTTON_PIN)){
        break;
      }
      delay(10);
    }

    right = analogRead(RIGHT_TAPE_BACK);
    left = analogRead(LEFT_TAPE_BACK);
    BLACK_BACK_THRESH = min(right, left) - 75;
    Serial.println(BLACK_BACK_THRESH);
    Serial.println("Calib rear black done");
  }
  changeState(CAL_GREY);
}

void calibrateGrey(void){
  Serial.println("Calib grey");
  if(CALIBRATE_TOGGLE){
    while(true){
      if(!digitalRead(BUTTON_PIN)){
        break;
      }
      delay(10);
    }
    int right = analogRead(FAR_RIGHT_TAPE);
    Serial.println(right);
    GREY_THRESH = right;
  }

  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  Serial.println("Calib grey done");
  changeState(DRIVE_TO_BOX);
}

void waitToStart(){
  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  changeState(FORWARD);
  START_TIME = millis();
}

/*---------------Global Checks--------------------------*/

//This code was never used or tested - consider it more an outline of the logic for reacting to changes in the beacon state.
void checkBeaconValue(){
  //ONLY LOGIC IMPLEMENTED, NOT FINAL CODE
  int roundAStatus = digitalRead(RIGHT_BEACON_PIN);
  int roundBStatus = digitalRead(LEFT_BEACON_PIN);

  Serial.print("Round B: ");
  Serial.println(roundBStatus);
  Serial.print("Round A: ");
  Serial.println(roundAStatus);
  if (roundAStatus != frA){
    frA = roundAStatus;
    Serial.println("BEACON READING CHANGED");
    Serial.print("Found round A: ");
    Serial.println(frA);
  }

  if (roundBStatus != frB){
    frB = roundBStatus;
    Serial.println();
    Serial.println("BEACON READING CHANGED");
    Serial.print("Found round B: ");
    Serial.println(frB);
    Serial.println();
  }

  if(roundAStatus == 0 && roundBStatus == 1){
    //move to it and hammer
    Serial.println("Losing A, Winning B");
    if(state == FORWARD){
      if(greyCounter >= 1){ //we already passed it!
        changeState(REVERSE);
      }
    }
    else if(state == REVERSE){
      if(greyCounter <= 1){  //won't ever get here because we shouldnt go past it
        changeState(FORWARD);
      }
    }
    targetGreyCounter = 1;
  }
  else if(roundBStatus == 0 && roundAStatus == 1){
    //move to it and hammer
    Serial.println("Losing B, Winning A");

    if(state == FORWARD){
      if(greyCounter > 3){  //won't ever get here because we shouldnt go past it
        changeState(REVERSE);
      }
    }
    else if(state == REVERSE){
      if(greyCounter <= 3){  //won't ever get here because we shouldnt go past it
        changeState(FORWARD);
      }
    }
    targetGreyCounter = 3;

  }
  else if(roundAStatus == 0 && roundBStatus == 0){
    //do something different for this state?
    Serial.println("what to do if we are losing both?");
    //go to closer one!
  }
}

//Driving into the startup garage when in the calibration phase
void driveToBox(){
  int right = analogRead(RIGHT_TAPE);
  int left = analogRead(LEFT_TAPE);
  if((right < BLACK_THRESH && left < BLACK_THRESH) ){
    //Both on white, go forward
    runMotor(RIGHT, 60);
    runMotor(LEFT, 60);
  } else if (right > BLACK_THRESH && left < BLACK_THRESH){
    //Turn right
    runMotor(LEFT, 60);
    runMotor(RIGHT, 30);
  } else if (right < BLACK_THRESH && left > BLACK_THRESH){
    //Turn left
    runMotor(RIGHT, 60);
    runMotor(LEFT, 30);
  } else {      // we dont know what we're on so move forward slowly
    //Serial.println("we are in an unknown state");
    runMotor(RIGHT, 60);
    runMotor(LEFT, 60);
  }

  double distance = encoder.getPosition();
  double distance_inches = map(distance,0,10,0,79.162); //Hard Coded translation of encoder distance to inches
  if(distance_inches > 22){
    runMotor(LEFT, 0);
    runMotor(RIGHT, 0);
    changeState(WAITING_TO_START);
  }
}

//Debugging method for changing states
void changeState(States_t newState){
  state = newState;
  Serial.println(stateNames[newState]);
}

//Sends locations values to the beacon tracking servos based on the encoder reading
void trackBeacon(){ //distance in inches!
  //for use in potentiometer
  double distance = encoder.getPosition();
  double distance_inches = map(distance,0,10,0,79.162); //Hard coded translation of encoder distance to inches (based on field measurements)
  double A_angle;

  double conversion = 180/pi;

  if (distance_inches <= Y_START_TO_A){  //hasnt reached A yet
    A_angle = 180 - atan(X_SENSOR_TO_BEACON/(Y_START_TO_A - distance_inches))*conversion;
  }
  else{
    A_angle = atan(X_SENSOR_TO_BEACON/(distance_inches - Y_START_TO_A))*conversion;  //after past A
  }
  servoA.write(A_angle);

  double B_angle;
  if (distance_inches <= Y_START_TO_B){  //hasnt reached B yet
    B_angle = 180 - atan(X_SENSOR_TO_BEACON/(Y_START_TO_B - distance_inches))*conversion;
  }
  else{
    B_angle = atan(X_SENSOR_TO_BEACON/(distance_inches - Y_START_TO_B))*conversion;  //after past B
  }
  servoB.write(B_angle);
}


/*---------------Motor Functions--------------------------*/

void runMotor(Motor_t mot, int speed){
  if(mot == RIGHT){
    if(speed > 0){
      analogWrite(MOTOR2_LPWM, speed);
      analogWrite(MOTOR2_RPWM, 0);
    } else {
      analogWrite(MOTOR2_LPWM, 0);
      analogWrite(MOTOR2_RPWM, -speed);
    }
  } else if(mot == LEFT){
    if(speed > 0){
      analogWrite(MOTOR1_LPWM, speed);
      analogWrite(MOTOR1_RPWM, 0);
    } else {
      analogWrite(MOTOR1_LPWM, 0);
      analogWrite(MOTOR1_RPWM, -speed);
    }
  }
}

void moveForward(void){
  int right = analogRead(RIGHT_TAPE);
  int left = analogRead(LEFT_TAPE);
  if((right < BLACK_THRESH && left < BLACK_THRESH) ){
    //Both on white, go forward
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, HIGH_SPEED);
  } else if (right > BLACK_THRESH && left < BLACK_THRESH){
    //Turn right
    runMotor(LEFT, HIGH_SPEED);
    runMotor(RIGHT, LOW_SPEED);
  } else if (right < BLACK_THRESH && left > BLACK_THRESH){
    //Turn left
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, LOW_SPEED);
  } else {      // we dont know what we're on so move forward slowly
    //Serial.println("we are in an unknown state");
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, HIGH_SPEED);
  }
}

void moveReverse(void){
  if(!digitalRead(LIMIT)){
    changeState(FORWARD);
    targetGreyCounter = 1;
  }
  int right = analogRead(RIGHT_TAPE_BACK);
  int left = analogRead(LEFT_TAPE_BACK);
  if((right < BLACK_BACK_THRESH && left < BLACK_BACK_THRESH) ){
    //Both on white, go forward
    runMotor(RIGHT, -HIGH_SPEED_BACK);
    runMotor(LEFT, -HIGH_SPEED_BACK);
  } else if (right > BLACK_BACK_THRESH && left < BLACK_BACK_THRESH){
    runMotor(LEFT, -HIGH_SPEED_BACK);
    runMotor(RIGHT, -LOW_SPEED_BACK);
  } else if (right < BLACK_BACK_THRESH && left > BLACK_BACK_THRESH){
    runMotor(RIGHT, -HIGH_SPEED_BACK);
    runMotor(LEFT, -LOW_SPEED_BACK);
  } else {      // we dont know what we're on so move forward slowly
    runMotor(RIGHT, -HIGH_SPEED_BACK);
    runMotor(LEFT, -HIGH_SPEED_BACK);
  }

  if(readingFromBeacons){
    testForStartupGarage();
  }
}

/*---------------Timer and Waiting Functions--------------------------*/

void testForGrey(void){
  if(!checkTape) return;
  //tape sensor grey
  if (state == FORWARD || state == REVERSE){
    int right = analogRead(FAR_RIGHT_TAPE);
    greyHistory.addValue(right);

    bool pastTenGrey = true;
    for (int i = 0; i < 10; i ++){
      int curr = greyHistory.getElement(i);
      if (!(curr > GREY_THRESH - 75 && curr < GREY_THRESH + 75)){
        pastTenGrey = false;
      }
    }

    if (pastTenGrey){ //Use rolling list of previous grey values to avoid erroneously detecting grey
      if(!onGrey){
        if(greyCounter == -1){
          greyCounter = 3;
        } else {
          if (state == FORWARD){
            greyCounter++;
          }
          else if (state == REVERSE){
            greyCounter --;
          }
        }
        onGrey = true;
        respToGrey();
        return;
      }
      return;
    }
    onGrey = false;
    return;
  }
}

void respToGrey(void){
  Serial.println(greyCounter);
  double distance = encoder.getPosition();
  Serial.print("distanceFromStart: ");
  Serial.println(distance);
  if (greyCounter == 2 && targetGreyCounter == 2){    //UNLOADING BALLS INTO MIDDLE
    changeState(STOP_WAIT);
    ballsReleaseTimer.begin(ballsDeposited, BALL_RELEASE_TIME);
    runMotor(RIGHT,0);
    runMotor(LEFT,0);
    targetGreyCounter = 3;
  }
  else if (greyCounter == 1 && targetGreyCounter == 1){ //PUSH DOWN A
    if (!readingFromBeacons){
      changeState(STOP_WAIT_HAMMER);
      hammerTime.begin(doneHammering, BOT_HAMMER_TIME);
      targetGreyCounter = 0;

      Serial.println("Now reading from beacons");
      readingFromBeacons = true;
    } else{  //first Time!!
      changeState(STOP_WAIT);
      ballsReleaseTimer.begin(ballsDeposited, BALL_RELEASE_TIME);
      targetGreyCounter = 0;
    }
    runMotor(RIGHT,0);
    runMotor(LEFT,0);
  }
  //reverse
  else if (greyCounter == 3 &&  targetGreyCounter == 3){ // Drop 4 in B
    changeState(STOP_WAIT);
    ballsReleaseTimer.begin(ballsDeposited, BALL_RELEASE_TIME);
    targetGreyCounter = 1;
    runMotor(RIGHT,0);
    runMotor(LEFT,0);
  }
}

void stopAndWaitHammer(void){
  useHammer();
}

void useHammer(){
  hammerServo.write(178);
}

void stopAndWait(void){
    dropBall();
}

void dropBall(void) {
  ballServo.write(SERVO_CENTER - 35);
}

void doneHammering(void){
  hammerTime.end();
  hammerServo.write(15);
  delay(200);
  changeState(REVERSE);
  if(greyCounter == 3){
    greyCounter = -1;
  }
}

void ballsDeposited(void) {
  ballsReleaseTimer.end();
  changeState(REVERSE);
  ballServo.write(SERVO_CENTER + 5);
  if(greyCounter == 3){
    greyCounter = -1;
  }
}

void testForStartupGarage(){
  double distance = encoder.getPosition();
  double distance_inches = map(distance,0,10,0,79.162); //Hard coded translation of encoder distance to inches
  if(distance_inches < BOOTY_BUMP_DIST && distance_inches > 0.0){
    runMotor(RIGHT, 0);
    runMotor(LEFT, -70);
    changeState(BOOTY_BUMP);
  }  else if (distance_inches <= 0.0){
    Serial.println("Encoder error");
  }
}

void bootyBump(){
  greyCounter = 0;
  double distance = encoder.getPosition();
  double distance_inches = map(distance,0,10,0,79.162); //Hard coded translation of encoder distance to inches
  if(distance_inches <= BOOTY_BUMP_DIST - ROTATION_VAL){
    changeState(BOOTY_BUMP_RETURN);
    runMotor(RIGHT, 0);
    runMotor(LEFT, 70);
    reload_toggled = true;
  }
}

void bootyBumpReturn(){
  double distance = encoder.getPosition();
  double distance_inches = map(distance,0,10,0,79.162); //Hard coded translation of encoder distance to inches
  if(distance_inches >= BOOTY_BUMP_DIST - 0.5 && reload_toggled){
    reloadTimer.begin(doneReloading, RELOAD_TIME);
    runMotor(RIGHT,0);
    runMotor(LEFT,0);
    targetGreyCounter = 1;
    reload_toggled = false;
    BOOTY_BUMP_DIST = BOOTY_BUMP_DIST - 0.1;
  }
}

void doneReloading(){
  changeState(FORWARD);
  reloadTimer.end();
  ignoreTape.begin(doneIgnoring, IGNORE_TIME);
  checkTape = false;
}

void finishGame(void){
  //CELEBRATE!!!
  runMotor(RIGHT,0);
  runMotor(LEFT,0);
}
