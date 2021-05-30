#include<RSCG12864B.h>
#include<stdlib.h>
#include<IRremote.h>
// #include"board.h"
#include<string.h>
const int IR_RECV = 6;
uint16_t cmd;
typedef unsigned char uchar;
struct vertex{
  uchar row;
  uchar col;
  vertex(): row(0), col(0){}
};


class board{
struct site{
    bool isOpen;
    // >=10 for mine
    uchar num;
    // 0 for unmarked, 1 for marked, 2 for unknown
    uchar flag;
    site(): isOpen(false), num(0), flag(0){}
};
private:
    const char* uc2s[10] = {"/","1","2","3","4","5","6","7","8","9"};
    // / -> SPACE
    site** sites;
    uchar safeSite;
    uchar mines;
    uchar marks;
    uchar m;
    uchar n;
    void numset(uchar p, uchar q){
        if(p < 0 || p >= m || q < 0 || q >= n) return;
        sites[p][q].num++;
    }
    
public:
    board(){}
    ~board();
    board(uchar row, uchar col, uchar minenum);
    bool isOpen(uchar p, uchar q);
    // left click
    void open(uchar p, uchar q);
    // right click
    void mark(uchar p, uchar q);
    void draw(uchar p, uchar q) const;
    friend void cmd_handler(board* game);
    void show_safesite(){
      Serial.println("safesites:");
      Serial.println(this->safeSite);
    }
};

/* 连线：
BUSY ---- A3
SDA ---- A4
SCL ---- A5
VDD ---- 5V
GND ---- GND
*/
// let * denote mine, ! denote marked, ? denote uncertain
// numbers denote mines around this block

void print_block(RAYLIDLCD& lcd, unsigned char x, unsigned char y);
void bomb(RAYLIDLCD& lcd);
void win(RAYLIDLCD& lcd);
void draw_cursor();
void cmd_handler(board* game);

const int busyPin = 7;
vertex cursor_co = vertex();
RAYLIDLCD myLCD(busyPin); // 我定义Busy信号到D7（数字口7）

void setup() {

  randomSeed(analogRead(5));
  Serial.begin(9600);
  IrReceiver.begin(IR_RECV, ENABLE_LED_FEEDBACK);
  IrReceiver.enableIRIn();
  myLCD.begin(); 
  myLCD.setBrightness(255);

}

void loop() {
  // initialize
  myLCD.clear();
  cursor_co = vertex();
  Serial.println("test12");
  board *game = new board(8,12,10);
  for(unsigned char x = 0; x < 72; x+=6){
    for(unsigned char y = 0; y < 64; y+=8){
      myLCD.drawRect(x, y, x + 5, y + 7);
      myLCD.drawRectF(x + 2, y + 3, x + 3, y + 4);
    }
  }

  while(1){
    if(IrReceiver.decode()){
        Serial.println("decode");
        cmd = IrReceiver.decodedIRData.command;
        IrReceiver.resume();
        cmd_handler(game);
    }

    // cursor blink
    myLCD.drawRectF(cursor_co.col * 6, cursor_co.row * 8, cursor_co.col * 6 + 5, cursor_co.row * 8 + 7);
    delay(500);
    myLCD.deleteRectF(cursor_co.col * 6, cursor_co.row * 8, cursor_co.col * 6 + 5, cursor_co.row * 8 + 7);
    game->draw(cursor_co.row, cursor_co.col);
    game->show_safesite();
    delay(200);
  }

  game->open(1,1);
  delay(1000);
  myLCD.clear();
  delete game;
}

board::board(uchar row, uchar col, uchar minenum){
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
// this method seems useless
bool board::isOpen(uchar p, uchar q){
    return sites[p][q].isOpen;
}

void board::open(uchar p, uchar q){
    if(p < 0 || p >= m || q < 0 || q >= n) return;
    if(sites[p][q].isOpen || sites[p][q].flag > 0) return;
    if(sites[p][q].num >= 10){
        // printf("bomb!!\n");
        bomb(myLCD); 
        myLCD.print(q * 6, p * 8, "*", SMALL);
        delay(1000);
        return;
    }

    sites[p][q].isOpen = true;
    this->safeSite++;
    myLCD.print(q * 6, p * 8, uc2s[sites[p][q].num],SMALL);
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
        myLCD.print(q * 6, p * 8, uc2s[sites[p][q].num], SMALL);
      }
      else{
        myLCD.print(q * 6, p * 8, "*", SMALL);
      }
      return;
    }
    else if(sites[p][q].flag == 0){
      Serial.println(sites[p][q].num);
      Serial.println(sites[p][q].flag);
      myLCD.drawRect(q * 6, p * 8, q * 6 + 5, p * 8 + 7);
      myLCD.drawRectF(q * 6 + 2, p * 8 + 3, q * 6 + 3, p * 8 + 4);
      return;
    }
    else{
      const char* mark = sites[p][q].flag == 1 ? "!": "?";
      myLCD.print(q * 6, p * 8, mark, SMALL);
    }
}

void print_block(RAYLIDLCD& lcd, unsigned char x, unsigned char y){
  lcd.drawRect(x, y, x + 5, y + 7);
  lcd.drawRectF(x + 2, y + 3, x + 3, y + 4);
}

void bomb(RAYLIDLCD& lcd){
  lcd.print(78,0,"BOMB!!",SMALL);
}

void win(RAYLIDLCD& lcd){
  lcd.print(78,0,"WIN!!", NORMAL);
}

void cmd_handler(board* game) // takes action based on IR code received describing Car MP3 IR codes 
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

void draw_cursor(){
  Serial.println("draw_cursor called");
  uchar p = cursor_co.row;
  uchar q = cursor_co.col;
  myLCD.drawRectF(q * 6, p * 8, q * 6 + 5, p * 8 + 7);
}