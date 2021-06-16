/* connections
BUSY ---- A3
SDA ---- A4
SCL ---- A5
VDD ---- 5V
GND ---- GND
*/
#include<RSCG12864B.h>
#include<stdlib.h>
#include<IRremote.h>
#include<string.h>
#define _ROW_ 8
#define _COLUMN_ 15
typedef unsigned char uchar;

uint16_t cmd;
// cmd: to store IRremote signal
uchar winState = 0;
// winState: 0 for still playing, 1 for win, 2 for lose
const uchar gameMode[3][3] = {{8, 12, 8}, {8, 15, 15}, {8, 15, 25}};
const char* modeMines[3]={"8", "15", "25"};
// modeMines is just used for myLCD.print(), whose output type should be const char*.
struct vertex{
  uchar row;
  uchar col;
  vertex(): row(0), col(0){}
  vertex(uchar p, uchar q): row(p), col(q){}
};
struct stack{
    vertex elem[_ROW_ * _COLUMN_ + 1];
    uchar top;
    stack(): top(0){}
};
const uchar TONE_PIN = 5;
const uchar IR_RECV = 6;
const uchar busyPin = 7;
const uchar LED_RED = 12;
const uchar LED_BLUE = 13;
vertex cursor_co = vertex();
RAYLIDLCD myLCD(busyPin);
// bind busy signal with D7

class board{
struct site{
    bool isOpen;
    uchar num;
    // >=10 for mine
    uchar flag;
    // flag: 0 for unmarked, 1 for marked, 2 for unknown
    site(): isOpen(false), num(0), flag(0){}
};
private:
    const char* uc2s[10] = {"/","1","2","3","4","5","6","7","8","9"};
    // uc2s: unsigned char to string. Used for myLCD.print()
    site** sites;
    uchar safeSite;
    uchar mines;
    uchar marks;
    // marks: we don't use it in our game
    uchar m;
    uchar n;
    stack dfs;
    void numset(uchar p, uchar q){
        if(p < 0 || p >= m || q < 0 || q >= n) return;
        sites[p][q].num++;
    }
public:
    board(){}
    ~board();
    board(uchar row, uchar col, uchar minenum);

    // open(): left click
    void open(uchar p, uchar q);

    // mark(): right click
    void mark(uchar p, uchar q);
    void draw(uchar p, uchar q) const;
    friend void cmd_handler(board* game);
    void show_safesite(){
      Serial.println("safesites:");
      Serial.println(this->safeSite);
    }
};

void bomb(RAYLIDLCD& lcd);
void win(RAYLIDLCD& lcd);
void cmd_handler(board* game);

// the built-in function tone() conflicts with IRremote.h
// so we define Ring() to replace tone();
void Ring(){
  for(uchar i = 0; i < 50; i++){
    digitalWrite(TONE_PIN, HIGH);
    delayMicroseconds(100);
    digitalWrite(TONE_PIN, LOW);
    delayMicroseconds(100);
  }
}

void setup() {
  randomSeed(analogRead(5));
  Serial.begin(9600);
  IrReceiver.begin(IR_RECV, false);
  IrReceiver.enableIRIn();
  myLCD.begin(); 
  myLCD.setBrightness(255);
  pinMode(TONE_PIN, OUTPUT);
}

void loop() {
  myLCD.clear();

  // stage 1 -- menu:
  // the LCD screen should be like this:
  // ###########################
  // ##      Minesweeper      ##
  // ##         Easy      #   ##
  // ##        Normal         ##
  // ##         Hard          ##
  // ###########################

  myLCD.print(20, 2, "Minesweeper", VLARGE);
  myLCD.print(50, 20, "Easy", LARGE);
  myLCD.print(45, 30, "Normal", LARGE);
  myLCD.print(50, 40, "Hard", LARGE);
  uchar mode = 0;
  while(1){
    if(IrReceiver.decode()){
        cmd = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        Serial.println("IR data received");
        Serial.println(cmd);
        Ring();
        if(cmd == 0x18){
          mode = (mode + 2) % 3;
        }else if(cmd == 0x52){
          mode = (mode + 1) % 3;
        }
        if(cmd == 0x16){
          // next stage
          break;
        }
        Serial.println(mode);
    }
    myLCD.drawRectF(100, mode * 10 + 22, 105, mode * 10 + 28);
    delay(500);
    myLCD.deleteRectF(100, mode * 10 + 22, 105, mode * 10 + 28);
  }

  // stage 2 -- game:
  myLCD.clear();
  // board initialization
  winState = 0;
  cursor_co = vertex();
  cursor_co = vertex();
  Serial.println("test12");
  board *game = new board(gameMode[mode][0], gameMode[mode][1], gameMode[mode][2]);
  for(uchar x = 0; x < 5 * gameMode[mode][1]; x+=5){
    for(uchar y = 0; y < 8 * gameMode[mode][0]; y+=8){
      myLCD.drawRect(x, y, x + 4, y + 7);
      myLCD.drawRectF(x + 2, y + 3, x + 2, y + 4);
    }
  }
  myLCD.print(79, 0, "mines:", SMALL);
  myLCD.print(79, 10, modeMines[mode], SMALL);
  // playing...
  while(winState == 0){
    if(IrReceiver.decode()){
        Serial.println("IR data received");
        Ring();
        cmd = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        cmd_handler(game);
    }
    // cursor blink
    myLCD.drawRectF(cursor_co.col * 5, cursor_co.row * 8, cursor_co.col * 5 + 4, cursor_co.row * 8 + 7);
    delay(400);
    myLCD.deleteRectF(cursor_co.col * 5, cursor_co.row * 8, cursor_co.col * 5 + 4, cursor_co.row * 8 + 7);
    game->draw(cursor_co.row, cursor_co.col);
    game->show_safesite();
    delay(300);
  }

  // stage 3 -- game over:
  for(uchar i = 0; i < 3; i++){
    digitalWrite(LED_RED - winState + 2, HIGH);
    delay(300);
    digitalWrite(LED_RED - winState + 2, LOW);
    delay(300);
  }
  myLCD.print(78, 20, "Press 0",SMALL);
  myLCD.print(78, 30, "to play", SMALL);
  myLCD.print(78, 40, "again", SMALL);
  while(1){
    if(IrReceiver.decode()){
        Serial.println("IR data received");
        cmd = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        Ring();
        if(cmd == 0x16){
            break;
        }
    }
    delay(500);
  }
  delay(1000);
  myLCD.clear();
  delete game;
}

board::board(uchar row, uchar col, uchar minenum){
    this->dfs = stack();
    this->mines = minenum;
    this->marks = 0;
    this->safeSite = 0;
    this->m = row;
    this->n = col;
    // sites.resize(row);
    sites = (site**)malloc(sizeof(site*) * row);
    for(uchar i = 0; i < row; i++){
        // sites[i].resize(col);
        sites[i] = (site*)malloc(sizeof(site) * col);
        memset(sites[i], 0, sizeof(site) * col);
    }

    // srand((unsigned)time(0));
    for(uchar i = 0; i < minenum; ){
        uchar p = random(row)%row;
        uchar q = random(col)%col;
        // set a mine on sites[p][q].
        if(sites[p][q].num < 10){
            sites[p][q].num = 10;
            i++;
            numset(p - 1, q - 1);
            numset(p ,q - 1);
            numset(p + 1, q - 1);
            numset(p + 1, q);
            numset(p + 1, q + 1);
            numset(p, q + 1);
            numset(p - 1, q + 1);
            numset(p - 1, q);
        }
    }
}

board::~board(){
    for(uchar i = 0; i < m; i++){
        free(sites[i]);
    }
    free(sites);
}

/* void board::open(uchar p, uchar q){
    if(p < 0 || p >= m || q < 0 || q >= n) return;
    if(sites[p][q].isOpen || sites[p][q].flag > 0) return;
    if(sites[p][q].num >= 10){
        // printf("bomb!!\n");
        bomb(myLCD); 
        myLCD.print(q * 5, p * 8, "*", SMALL);
        delay(1000);
        return;
    }

    sites[p][q].isOpen = true;
    this->safeSite++;
    myLCD.print(q * 5, p * 8, uc2s[sites[p][q].num],SMALL);
    //draw(p, q);
    // recursively open 8 neighbors
    if(sites[p][q].num == 0){
        open(p - 1, q - 1);
        open(p ,q - 1);
        open(p + 1, q - 1);
        open(p + 1, q);
        open(p + 1, q + 1);
        open(p, q + 1);
        open(p - 1, q + 1);
        open(p - 1, q);
        return;
    }
    if(safeSite + mines == m * n){
        win(myLCD);
    }
    return;
} */

void board::open(uchar p, uchar q){
    if(p < 0 || p >= m || q < 0 || q >= n) return;
    if(sites[p][q].isOpen || sites[p][q].flag > 0) return;
    dfs.elem[++dfs.top] = vertex(p, q);
    sites[p][q].isOpen = true;
    if(sites[p][q].num >= 10){
      bomb(myLCD);
      myLCD.print(78,0,"BOMB!!",SMALL);
      winState = 2;
      return;
    }
    while(dfs.top > 0){
        uchar i = dfs.elem[dfs.top].row;
        uchar j = dfs.elem[dfs.top].col;
        dfs.top--;
        this->safeSite++;
        myLCD.print(j * 5, i * 8, uc2s[sites[i][j].num],SMALL);
        // if sites[i][j].num == 0, push neighbors to stack.
        if(sites[i][j].num == 0){
            if(i > 0 && j > 0 && !(sites[i - 1][j - 1].isOpen || sites[i - 1][j - 1].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i - 1, j - 1);
                sites[i - 1][j - 1].isOpen = true; 
            }   
            if(j > 0 && !(sites[i][j - 1].isOpen || sites[i][j - 1].flag > 0)) {
                dfs.elem[++dfs.top] = vertex(i, j - 1);
                sites[i][j - 1].isOpen = true; 
            }
            if(i < m - 1 && j > 0 && !(sites[i + 1][j - 1].isOpen || sites[i + 1][j - 1].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i + 1, j - 1);
                sites[i + 1][j - 1].isOpen = true; 
            }
            if(i < m - 1 && !(sites[i + 1][j].isOpen || sites[i + 1][j].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i + 1, j);
                sites[i + 1][j].isOpen = true; 
            }
            if(i < m - 1 && j < n - 1 && !(sites[i + 1][j + 1].isOpen || sites[i + 1][j + 1].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i + 1, j + 1);
                sites[i + 1][j + 1].isOpen = true; 
            } 
            if(j < n - 1 && !(sites[i][j + 1].isOpen || sites[i][j + 1].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i, j + 1);
                sites[i][j + 1].isOpen = true; 
            } 
            if(i > 0 && j < n - 1 && !(sites[i - 1][j + 1].isOpen || sites[i - 1][j + 1].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i - 1, j + 1);
                sites[i - 1][j + 1].isOpen = true; 
            } 
            if(i > 0 && !(sites[i - 1][j].isOpen || sites[i - 1][j].flag > 0)){
                dfs.elem[++dfs.top] = vertex(i - 1, j);
                sites[i - 1][j].isOpen = true; 
            } 
        }
    }
    if(safeSite + mines == m * n){
      win(myLCD);
    }
    return;
}

void board::mark(uchar p, uchar q){
    if(sites[p][q].isOpen) return;
    this->marks += (sites[p][q].flag == 0 ? 1: 0);
    this->marks -= (sites[p][q].flag == 1 ? 1: 0);
    sites[p][q].flag = (sites[p][q].flag + 1) % 3;
    return;
}

void board::draw(uchar p, uchar q) const{
    if(sites[p][q].isOpen){
      if(sites[p][q].num < 10){
        myLCD.print(q * 5, p * 8, uc2s[sites[p][q].num], SMALL);
      }
      else{
        myLCD.print(q * 5, p * 8, "*", SMALL);
      }
      return;
    }
    else if(sites[p][q].flag == 0){
      Serial.println(sites[p][q].num);
      Serial.println(sites[p][q].flag);
      myLCD.drawRect(q * 5, p * 8, q * 5 + 4, p * 8 + 7);
      myLCD.drawRectF(q * 5 + 2, p * 8 + 3, q * 5 + 2, p * 8 + 4);
      return;
    }
    else{
      const char* mark = sites[p][q].flag == 1 ? "!": "?";
      myLCD.print(q * 5, p * 8, mark, SMALL);
    }
}


void bomb(RAYLIDLCD& lcd){
  lcd.deleteRectF(78, 0, 110, 20);
  lcd.print(78,0,"BOMB!!",SMALL);
  winState = 2;
}

void win(RAYLIDLCD& lcd){
  lcd.deleteRectF(78, 0, 110, 20);
  lcd.print(78,0,"WIN!!", NORMAL);
  winState = 1;
}

// takes action based on IR code received
void cmd_handler(board* game)
{
  switch(cmd){
  case 0x16:  
    game->open(cursor_co.row, cursor_co.col); 
    break;
  case 0x19:  
    game->mark(cursor_co.row, cursor_co.col);
    break;
  case 0x18:  
    if(cursor_co.row > 0){
      cursor_co.row--;
    }
    break;
  case 0x8:  
    if(cursor_co.col > 0){
      cursor_co.col--;
    }
    break;
  case 0x5A:  
    if(cursor_co.col < game->n - 1){
      cursor_co.col++;
    }
    break;
  case 0x52:  
    if(cursor_co.row < game->m - 1){
      cursor_co.row++;
    }
    break;
  default:
    break;
  }
}
