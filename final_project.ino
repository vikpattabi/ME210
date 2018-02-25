#define RIGHT_TAPE A3
#define LEFT_TAPE A4
#define FAR_RIGHT_TAPE A5
#define LIMIT_SWITCH 10

/*---------------State Definitions--------------------------*/
typedef enum {
  CAL_WHITE, CAL_BLACK, FORWARD, STOP_WAIT, DEPOSIT_BALL, TURN, FINISH
} States_t;

States_t state;

int tape;
int BLACK_THRESH;
int GREY_THRESH;
int WHITE_THRESH;
int GREY_COUNTER;
bool onGrey;

void setup() {
  pinMode(RIGHT_TAPE, INPUT);
  pinMode(LEFT_TAPE, INPUT);

  onGreyTrigger = false;
  GREY_COUNTER = 0;
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
  checkGlobalEvents();

  switch (state) {
    case CAL_WHITE:
      calibrateWhite();
      break;
    case CAL_BLACK:
      calibrateBlack();
      break;
    case FORWARD:
      moveForward();
      break;
    case STOP_WAIT:
      stopAndWait();
      break;
    case DEPOSIT_BALL:
      depositBall();
      break;
    case TURN:
      turnToGate();
      break;
    case FINISH:
      finishGame();
      break;
    default:    // Should never get into an unhandled state
      Serial.println("What is this I do not even...");
  }
}

void checkGlobalEvents(void) {
  if (state == FORWARD && testForGrey()) respToGrey();
  if (state == FORWARD && testForCorner()) respToCorner();
  if (state == TURN && testForTurnDone()) respToTurnDone();
  if(state == FORWARD && testForFrontWall()) respToFrontWall();
}


void calibrateWhite(void){
  if(testForKey()){
    int white = (analogRead(RIGHT_TAPE) + analogRead(LEFT_TAPE))/2.0;
    WHITE_THRESH = white + 50;
    Serial.println(WHITE_THRESH);
    state = CAL_BLACK;
  }
}

void calibrateBlack(void){
  if(testForKey()){
    int black = (analogRead(RIGHT_TAPE) + analogRead(LEFT_TAPE))/2.0;
    BLACK_THRESH = black - 50;
    Serial.println(BLACK_THRESH);
    state = FORWARD;
  }
}

void moveForward(void){
  int right = analogRead(RIGHT_TAPE);
  int left = analogRead(LEFT_TAPE);

  //write to motors - forward

  if (right > BLACK_THRESH && left > BLACK_THRESH){
    Serial.println("We fucked up.");
  } else if (right < WHITE_THRESH && left > BLACK_THRESH){
    Serial.println("Turning left.");
    //turn motor left adjust
  } else if (right > BLACK_THRESH && left < WHITE_THRESH){
    Serial.println("Turning right.");
    //turn motor right adjust
  } else {
    Serial.println("We are moving forward.");
    //motors power forward
  }
}

void stopAndWait(void){
  //motors off
  //timer on
  //timer done = drop ball
}

void depositBall(void){
  //open servo for ball
  //wait time?
  //timer over, state = FORWARD
}

void turnToGate(void){
  //turn motors until some time or sensor HIT
}

void finishGame(void){
  //turn off motors and CELEBRATE
}

/////////////////////////////////////////////////////////////

bool testForCorner(void){
  //tape sensor black
  int far_right = analogRead(FAR_RIGHT_TAPE);
  if (far_right > BLACK_THRESH){
    return true;
  }
  return false;
}

void respToCorner(void){
  if (state == FORWARD){
    state = TURN;  //change to TURN the first time it hits from moving forward
  }
}

bool testForTurnDone(void){
  int far_right = analogRead(FAR_RIGHT_TAPE);
  if (far_right > BLACK_THRESH){
    return true;
  }
  return false;
}

void respToTurnDone(void){
  state = FORWARD
}

bool testForGrey(void){
  //tape sensor grey
  int right = analogRead(RIGHT_TAPE);
  if (right > GREY_THRESH - 50 || right < GREY_THRESH + 50 && onGrey == false){
    GREY_COUNTER++; 
    onGrey = true;  //only increment the first time
    return true;
  }
  else{
    onGrey = false;
  }
  return false;
}

void respToGrey(void){
  if (GREY_COUNTER == 1 || GREY_COUNTER == 3){  //hit funding A or B
    state = STOP_WAIT;
  }
}

bool testForFrontWall(void){
  int limitSwitch = digitalRead(LIMIT_SWITCH);
  return limitSwitch;
}

void respToFrontWall(void){
  state = FINISH;
}

