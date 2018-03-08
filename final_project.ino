#include <Servo.h>

/*---------------Pin Designations and Constants--------------------------*/
#define RIGHT_TAPE A2
#define LEFT_TAPE A1
#define RIGHT_TAPE_BACK A3
#define LEFT_TAPE_BACK A0
#define FAR_RIGHT_TAPE A4
#define SERVO_CONTROL A5
#define MOTOR1_RPWM A6
#define MOTOR1_LPWM A7
#define MOTOR2_RPWM A9
#define MOTOR2_LPWM A8
#define BUTTON_PIN 12
#define LED_PIN 13

#define SERVO_CENTER 80
#define HIGH_SPEED 90  //210 max
#define LOW_SPEED 60

/*---------------Timer Constants--------------------------*/
#define BALL_RELEASE_TIME 2000000
#define CRAWL_TIME 250000
#define BOT_HAMMER_TIME 500000

/*---------------State Definitions--------------------------*/
typedef enum {
  CAL_BLACK, CAL_GREY, FORWARD, REVERSE, STOP_WAIT, TURN, BLIND_CRAWL, FINISH, STOP_WAIT_HAMMER
} States_t;

char* stateNames[9] = {"CAL_BLACK", "CAL_GREY", "FORWARD", "REVERSE","STOP_WAIT", "TURN", "BLIND_CRAWL", "FINISH","STOP_WAIT_HAMMER"};

typedef enum {
  RIGHT, LEFT
} Motor_t;

volatile States_t state;

/*---------------Module Variables and Objects--------------------------*/
int tape;
int BLACK_THRESH = 800;
int GREY_THRESH = 530;
int greyCounter;
bool onGrey;
bool crossingSides;
volatile bool botSettled;
bool CALIBRATE = false;
IntervalTimer settleDownTimer;
IntervalTimer ballsReleaseTimer;
IntervalTimer crawlTimer;
IntervalTimer hammerTime;
Servo ballServo;

void setup() {
  pinMode(RIGHT_TAPE, INPUT);
  pinMode(LEFT_TAPE, INPUT);
  pinMode(MOTOR1_RPWM, OUTPUT);
  pinMode(MOTOR1_LPWM, OUTPUT);
  pinMode(MOTOR2_RPWM, OUTPUT);
  pinMode(MOTOR2_LPWM, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  //digitalWrite(LED_PIN, HIGH);
  ballServo.attach(SERVO_CONTROL);
  ballServo.write(SERVO_CENTER);
  delay(1000); //TO-DO fix this?
  greyCounter = 0;
  onGrey = false;
  botSettled = false;
  crossingSides = false;
  state = CAL_BLACK;
  Serial.begin(9600);
  Serial.println("Type a key to record black threshold.");
  pinMode(13, OUTPUT);
  digitalWrite(13,HIGH);
}

unsigned char testForKey(void){
  unsigned char KeyEventOccurred;
  KeyEventOccurred = Serial.available();
  while(Serial.available()) Serial.read();
  return KeyEventOccurred;
}

void loop() {
  switch (state) {
    case CAL_BLACK:
      calibrateBlack();
      break;
    case CAL_GREY:
      calibrateGrey();
      break;
    case FORWARD:
      //digitalWrite(LED_PIN, LOW);
      moveForward();
      break;
    case REVERSE:
      moveBackward();
      break;
    case STOP_WAIT:
      stopAndWait();
      break;
    case TURN:
      turnToGate();
      break;
    case BLIND_CRAWL:
      crawlForward(); //TO-DO delete this?
      break;
    case FINISH:
      setMotorsOff();
      finishGame(); //TO-DO delete this
      break;
    case STOP_WAIT_HAMMER:
      stopAndWaitHammer();
      break;
    default:    // Should never get into an unhandled state
      Serial.println("What is this I do not even...");
  }
}

void changeState(States_t newState){
  state = newState;
  Serial.println(stateNames[newState]);
}

void calibrateBlack(void){
  Serial.println("Calib black");
// hacked way to avoid calibration
  if (CALIBRATE){
    while(true){
      if(!digitalRead(BUTTON_PIN)){
        break;
      }
      delay(10);
    }
    delay(500);

    int right = analogRead(RIGHT_TAPE);
    int left = analogRead(LEFT_TAPE);
    BLACK_THRESH = min(right, left) - 75;
  }
  Serial.println(BLACK_THRESH);
  Serial.println("Calib black done");
  changeState(CAL_GREY);
}

void calibrateGrey(void){
  Serial.println("Calib grey");
// hacked way to avoid calibration
  if(CALIBRATE){
    while(true){
      if(!digitalRead(BUTTON_PIN)){
        break;
      }
      delay(10);
    }
  }
  delay(500);
  int right = analogRead(FAR_RIGHT_TAPE);
  Serial.println(GREY_THRESH);
//  changeState(FORWARD);
  changeState(REVERSE);
  
  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  Serial.println("Calib grey done");
  delay(2000); //TO-DO remove this
}

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

  if (testForGrey()){
    respToGrey();
  }
  if (testForOrthogLine()) { //TO-DO re enable this
    if (crossingSides) {
      respToGarage();
    } else {
      respToCorner();
    }
  }
}

void moveReverse(void){
  int right = analogRead(RIGHT_TAPE_BACK);
  int left = analogRead(LEFT_TAPE_BACK);
  if((right < BLACK_THRESH && left < BLACK_THRESH) ){
    //Both on white, go forward
    runMotor(RIGHT, -HIGH_SPEED);
    runMotor(LEFT, -HIGH_SPEED); 
  } else if (right > BLACK_THRESH && left < BLACK_THRESH){
    //Turn right
    Serial.println("Left motor high reverse");
    runMotor(LEFT, -HIGH_SPEED);
    runMotor(RIGHT, -LOW_SPEED); 
  } else if (right < BLACK_THRESH && left > BLACK_THRESH){
    //Turn left
    Serial.println("Right motor high reverse");
    runMotor(RIGHT, -HIGH_SPEED);
    runMotor(LEFT, -LOW_SPEED); 
  } else {      // we dont know what we're on so move forward slowly
    //Serial.println("we are in an unknown state");
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, HIGH_SPEED); 
  }

 //respond to reload area
}

void stopAndWaitHammer(void){
}


void stopAndWait(void){
    dropBall();
//    changeState(DEPOSIT_BALL);
}

void turnToGate(void){
  if (testForTurnDone()) respToTurnDone();
}

void crawlForward(void){ //TO-DO delete this
}

void finishGame(void){ //TO-DO delete this?
}

/////////////////////////////////////////////////////////////

bool testForOrthogLine(void){
  //tape sensor black
  int far_right = analogRead(FAR_RIGHT_TAPE);
  if (far_right > BLACK_THRESH){
    return true;
  }
  return false;
}

void respToCorner(void){
  changeState(TURN);
  setMotorsTurn();
  delay(400);
}

void respToGarage(void) {
  changeState(BLIND_CRAWL);
  setMotorsCrawl;
  crawlTimer.begin(doneCrawling, CRAWL_TIME);
}

bool testForTurnDone(void){
  int left = analogRead(LEFT_TAPE);
  if (left > BLACK_THRESH){
    Serial.println("TURN DONE");
    return true;
  }
  return false;
}

void respToTurnDone(void){
  setMotorsForward();
  crossingSides = true;
  changeState(FORWARD);
}

bool testForGrey(void){
  //tape sensor grey
  int right = analogRead(FAR_RIGHT_TAPE);
  if (right > GREY_THRESH - 65 && right < GREY_THRESH + 65){ //TO-DO is this hystereses right?
    Serial.print("FOUND GREY LINE, GREY COUNTER = ");
    Serial.println(greyCounter);
    Serial.print(" Threshold is: ");
    Serial.println(right);

    if(!onGrey){
      greyCounter++;
      onGrey = true;
      return true;
    }
    return false;
  }

  onGrey = false;
  return false;
}

void respToGrey(void){
//  if (greyCounter == 1 || greyCounter == 3){  //hit funding A or B
  if (greyCounter == 2){  //hit funding A or B
    setMotorsOff();
    changeState(STOP_WAIT);
    ballsReleaseTimer.begin(ballsDeposited, BALL_RELEASE_TIME);

  }
  else if (greyCounter == 1){
    setMotorsOff();
    changeState(STOP_WAIT_HAMMER);
    hammerTime.begin(doneHammering, BOT_HAMMER_TIME);
  }
  //reverse
  else if (greyCounter == 3){
    setMotorsOff();
    changeState(STOP_WAIT_HAMMER);
    hammerTime.begin(doneHammering, BOT_HAMMER_TIME);
  }
}

void doneHammering(void){
  hammerTime.end();
  //Change state again?
  if (greyCounter == 3){
    changeState(REVERSE);
  }else{
    changeState(FORWARD);
  }
}

void ballsDeposited(void) {
  ballsReleaseTimer.end();
  changeState(FORWARD);
  digitalWrite(LED_PIN, HIGH);
}

void doneCrawling(void) {
  crawlTimer.end();
  setMotorsOff();
  changeState(FINISH);
}

void dropBall(void) {
  if (greyCounter == 2) {
    ballServo.write(SERVO_CENTER+25);
  }
//  } else {
//    ballServo.write(SERVO_CENTER+30);
//  }
}

//TODO: Redefine this with the powerMotor function.
void setMotorsForward(void) {
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 0);
  analogWrite(MOTOR1_LPWM, 50);
  analogWrite(MOTOR2_LPWM, 50);
}

void setMotorsOff(void) {
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 0);
  analogWrite(MOTOR1_LPWM, 0);
  analogWrite(MOTOR2_LPWM, 0);
}

void setMotorsTurn(void) {
//  runMotor(RIGHT, -25);
//  runMotor(LEFT, 25);
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 25);
  analogWrite(MOTOR1_LPWM, 25);
  analogWrite(MOTOR2_LPWM, 0);
}

void setMotorsCrawl(void) {
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 0);
  analogWrite(MOTOR1_LPWM, 50);
  analogWrite(MOTOR2_LPWM, 50);
}
