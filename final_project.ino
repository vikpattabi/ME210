#include <Servo.h>

/*---------------Pin Designations--------------------------*/
#define RIGHT_TAPE A2
#define LEFT_TAPE A1
#define FAR_RIGHT_TAPE A4
#define SERVO_CONTROL 4
#define MOTOR1_RPWM A6
#define MOTOR1_LPWM A7
#define MOTOR2_RPWM A9
#define MOTOR2_LPWM A8
#define BUTTON_PIN 12
#define LED_PIN 13

#define HIGH_SPEED 100
#define LOW_SPEED 70

/*---------------Timer Constants--------------------------*/
#define BOT_SETTLE_TIME 1000000
#define BALL_RELEASE_TIME 2000000
#define CRAWL_TIME 1000000

/*---------------State Definitions--------------------------*/
typedef enum {
  CAL_WHITE, CAL_BLACK, CAL_GREY, FORWARD, STOP_WAIT, DEPOSIT_BALL, TURN, BLIND_CRAWL, FINISH
} States_t;

typedef enum {
  RIGHT, LEFT
} Motor_t;

volatile States_t state;

/*---------------Module Variables and Objects--------------------------*/
int tape;
int BLACK_THRESH;
int GREY_THRESH;
int WHITE_THRESH;
int greyCounter;
bool onGrey;
bool crossingSides;
volatile bool botSettled;
IntervalTimer settleDownTimer;
IntervalTimer ballsReleaseTimer;
IntervalTimer crawlTimer;
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
  ballServo.write(90);
  delay(1000); //TO-DO fix this?
  greyCounter = 0;
  onGrey = false;
  botSettled = false;
  crossingSides = false;
  state = CAL_WHITE;
  Serial.begin(9600);
  Serial.println("Type a key to record white threshold.");
}

unsigned char testForKey(void){
  unsigned char KeyEventOccurred;
  KeyEventOccurred = Serial.available();
  while(Serial.available()) Serial.read();
  return KeyEventOccurred;
}

void loop() {
  checkGlobalEvents(); //TO-DO delete this?
  switch (state) {
    case CAL_WHITE:
      calibrateWhite();
      break;
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
    case STOP_WAIT:
      stopAndWait();
      break;
    case DEPOSIT_BALL: //TO-DO combine the three waiting states?
      depositBall(); //TO-DO delete this?
      break;
    case TURN:
      turnToGate();
      if (testForTurnDone()) respToTurnDone();
      break;
    case BLIND_CRAWL:
      crawlForward(); //TO-DO delete this?
    case FINISH:
      finishGame(); //TO-DO delete this
      break;
    default:    // Should never get into an unhandled state
      Serial.println("What is this I do not even...");
  }
}

void checkGlobalEvents(void) {  //TO-DO delete this?

}


void calibrateWhite(void){
  Serial.println("Calib white");
  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  delay(500);
 // if(testForKey()){
    int right = analogRead(RIGHT_TAPE);
    int left = analogRead(LEFT_TAPE);
    WHITE_THRESH = max(right, left) + 20;
    Serial.println(WHITE_THRESH);
    state = CAL_BLACK;
    Serial.println("Calib white done");
//  }
}

void calibrateBlack(void){
  Serial.println("Calib black");
  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  delay(500);
  //if(testForKey()){
    int right = analogRead(RIGHT_TAPE);
    int left = analogRead(LEFT_TAPE);
    BLACK_THRESH = min(right, left) - 20;
    Serial.println(BLACK_THRESH);
    Serial.println("Calib black done");
    state = CAL_GREY;
  //}
}

void calibrateGrey(void){
  Serial.println("Calib grey");
  while(true){
    if(!digitalRead(BUTTON_PIN)){
      break;
    }
    delay(10);
  }
  delay(500);
  int right = analogRead(FAR_RIGHT_TAPE);
  GREY_THRESH = right;
  Serial.println(GREY_THRESH);
  state = FORWARD;
  
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
      analogWrite(MOTOR2_RPWM, speed); 
    }
  } else if(mot == LEFT){
    if(speed > 0){
      analogWrite(MOTOR1_LPWM, speed);
      analogWrite(MOTOR1_RPWM, 0);
    } else {
      analogWrite(MOTOR1_LPWM, 0);
      analogWrite(MOTOR1_RPWM, speed); 
    }
  }
}

void moveForward(void){
  int right = analogRead(RIGHT_TAPE);
  int left = analogRead(LEFT_TAPE);

  if(right < WHITE_THRESH && left < WHITE_THRESH){
    //Both on white, go forward
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, HIGH_SPEED); 
  } else if (right > BLACK_THRESH && left < WHITE_THRESH){
    //Turn right
    runMotor(LEFT, HIGH_SPEED);
    runMotor(RIGHT, LOW_SPEED); 
  } else if (right < WHITE_THRESH && left > BLACK_THRESH){
    //Turn left
    runMotor(RIGHT, HIGH_SPEED);
    runMotor(LEFT, LOW_SPEED); 
  }

  if (testForGrey()){
    respToGrey();
  }
//  if (testForOrthogLine()) { //TO-DO re enable this
//    if (crossingSides) {
//      respToGarage();
//    } else {
//      respToCorner();
//    }
//  }
}

void stopAndWait(void){
  if (botSettled) {
    botSettled = false;
    servoRelease();
    ballsReleaseTimer.begin(ballsDeposited, BALL_RELEASE_TIME);
    state = DEPOSIT_BALL;
  }
}

void depositBall(void){ //TO-DO delete this?
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
  state = TURN;
  setMotorsTurn();
}

void respToGarage(void) {
  state = BLIND_CRAWL;
  setMotorsCrawl;
  crawlTimer.begin(doneCrawling, CRAWL_TIME);
}

bool testForTurnDone(void){
  int right = analogRead(FAR_RIGHT_TAPE);
  if (right > BLACK_THRESH){
    return true;
  }
  return false;
}

void respToTurnDone(void){
  setMotorsForward();
  crossingSides = true;
  state = FORWARD;
}

bool testForGrey(void){
  //tape sensor grey
  int right = analogRead(FAR_RIGHT_TAPE);
  if (right > GREY_THRESH - 30 && right < GREY_THRESH + 30){ //TO-DO is this hystereses right?
    if(!onGrey){
      greyCounter++;
      onGrey = true;  //only increment the first time (what is this?)
      return true;
    }
    return false;
  }

  onGrey = false;
  return false;
}

void respToGrey(void){
  if (greyCounter == 1 || greyCounter == 3){  //hit funding A or B
    setMotorsOff();
    state = STOP_WAIT;
    settleDownTimer.begin(doneSettling, BOT_SETTLE_TIME);
  }
}

void doneSettling(void) {
  settleDownTimer.end();
  botSettled = true;
}

void ballsDeposited(void) {
  ballsReleaseTimer.end();
  state = FORWARD;
  digitalWrite(LED_PIN, HIGH);
}

void doneCrawling(void) {
  crawlTimer.end();
  setMotorsOff();
  state = FINISH;
}

void servoRelease(void) {
  if (greyCounter == 1) {
    ballServo.write(0);
  } else {
    ballServo.write(180);
  }
  delay(1000); //TO-DO no blocking lol
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
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 50);
  analogWrite(MOTOR1_LPWM, 50);
  analogWrite(MOTOR2_LPWM, 0);
}

void setMotorsCrawl(void) {
  analogWrite(MOTOR1_RPWM, 0);
  analogWrite(MOTOR2_RPWM, 0);
  analogWrite(MOTOR1_LPWM, 50);
  analogWrite(MOTOR2_LPWM, 50);
}
