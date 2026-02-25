/*
 * QPAD-XIAO GAME CARTRIDGE v3.0
 * Target: Seeed XIAO RP2040 + 0.96" SSD1306 OLED 128x64
 *
 * CONTROLS:
 *   D0  = UP
 *   D1  = LEFT
 *   D7  = RIGHT
 *   D8  = DOWN
 *   D9  = MODE  (back to menu / pause)
 *   D10 = ACTION (select / fire / jump / rotate)
 *
 * GAMES:
 *   1.  Breakout
 *   2.  Bounce Classic
 *   3.  Snake (Wall)
 *   4.  Snake (Wrap)
 *   5.  Tetris
 *   6.  Flappy Bird
 *   7.  Asteroids
 *   8.  Pac-Man
 *   9.  Space Shooter
 *   10. TicTacToe vs AI
 *   11. Pong / Air Hockey
 *
 * Libraries: Adafruit SSD1306, Adafruit GFX
 */
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
struct Ghost;                 // line ~25
void pmMoveGhost(Ghost& g);   // line ~26

// ─────────────────────────────────────────────────────────
//  OLED
// ─────────────────────────────────────────────────────────
#define SW 128
#define SH  64
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SW, SH, &Wire, -1);

// ─────────────────────────────────────────────────────────
//  BUTTONS
// ─────────────────────────────────────────────────────────
#define PIN_UP     D0
#define PIN_LEFT   D1
#define PIN_RIGHT  D7
#define PIN_DOWN   D8
#define PIN_MODE   D9
#define PIN_ACT    D10

struct Btn { uint8_t pin; bool held, pressed; };
Btn bU={PIN_UP,0,0},bL={PIN_LEFT,0,0},bR={PIN_RIGHT,0,0};
Btn bD={PIN_DOWN,0,0},bM={PIN_MODE,0,0},bA={PIN_ACT,0,0};
Btn* BTNS[]={&bU,&bL,&bR,&bD,&bM,&bA};

void readBtns(){
  for(auto b:BTNS){
    bool c=(digitalRead(b->pin)==LOW);
    b->pressed=c&&!b->held; b->held=c;
  }
}

// ─────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────
void cx(const char* t,int y,int s=1){
  display.setTextSize(s); display.setTextColor(SSD1306_WHITE);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds(t,0,0,&x1,&y1,&w,&h);
  display.setCursor((SW-(int)w)/2,y); display.print(t);
}
void line(int x0,int y0,int x1,int y1){ display.drawLine(x0,y0,x1,y1,SSD1306_WHITE); }

// ═════════════════════════════════════════════════════════
//  APP STATE / MENU
// ═════════════════════════════════════════════════════════
enum App {
  A_MENU,
  A_BREAKOUT, A_BOUNCE,
  A_SNAKE_W, A_SNAKE_WRAP,
  A_TETRIS, A_FLAPPY,
  A_ASTEROIDS, A_PACMAN,
  A_SHOOTER, A_TICTACTOE, A_PONG
};
App appState=A_MENU;

const char* GAMES[]={"Breakout","Bounce Classic","Snake Wall","Snake Wrap",
                     "Tetris","Flappy Bird","Asteroids","Pac-Man",
                     "Space Shooter","TicTacToe AI","Pong"};
const int NGAMES=11;
int menuCur=0;
#define MENU_VIS 4   // visible items at once
int menuScroll=0;

bool anyBtn(){ return bU.pressed||bD.pressed||bL.pressed||bR.pressed||bA.pressed||bM.pressed; }

void menuDraw(){
  display.clearDisplay();
  cx("QPAD GAMES",1,1);
  display.drawLine(0,10,SW,10,SSD1306_WHITE);
  for(int i=0;i<MENU_VIS&&(i+menuScroll)<NGAMES;i++){
    int idx=i+menuScroll, y=13+i*13;
    bool sel=(idx==menuCur);
    if(sel){ display.fillRect(0,y-1,SW,11,SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    else display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1); display.setCursor(4,y+1);
    display.print(idx+1); display.print(F(". ")); display.print(GAMES[idx]);
  }
  display.setTextColor(SSD1306_WHITE);
  // scroll arrows
  if(menuScroll>0)            { display.setCursor(120,13); display.print('^'); }
  if(menuScroll+MENU_VIS<NGAMES){ display.setCursor(120,52); display.print('v'); }
  display.display();
}

void menuUpdate(){
  if(bU.pressed){ menuCur=(menuCur-1+NGAMES)%NGAMES; if(menuCur<menuScroll) menuScroll=menuCur; }
  if(bD.pressed){ menuCur=(menuCur+1)%NGAMES; if(menuCur>=menuScroll+MENU_VIS) menuScroll=menuCur-MENU_VIS+1; }
  if(bA.pressed){
    App map[]={A_BREAKOUT,A_BOUNCE,A_SNAKE_W,A_SNAKE_WRAP,
               A_TETRIS,A_FLAPPY,A_ASTEROIDS,A_PACMAN,
               A_SHOOTER,A_TICTACTOE,A_PONG};
    appState=map[menuCur];
  }
}

// ═════════════════════════════════════════════════════════
//  1. BREAKOUT
// ═════════════════════════════════════════════════════════
#define BR_PW 28
#define BR_PH  3
#define BR_PY 59
#define BR_PS  3
#define BR_BR  2
#define BR_BC  8
#define BR_BRW 4
#define BR_BW 14
#define BR_BH  4
#define BR_BP  2
#define BR_X0  4
#define BR_Y0  9

enum {BR_TTL,BR_PLY,BR_PAU,BR_DED,BR_WIN} brSt=BR_TTL;
float brBx,brBy,brDx,brDy; int brPx,brSc,brLv,brLi; bool brOp;
bool brK[BR_BRW][BR_BC];

void brReset(){ for(int r=0;r<BR_BRW;r++) for(int c=0;c<BR_BC;c++) brK[r][c]=true; }
void brBall(){ brBx=brPx+BR_PW/2.0f; brBy=BR_PY-BR_BR-1;
  float s=1.8f+brLv*0.2f; brDx=(random(0,2)?s:-s); brDy=-s; brOp=true; }
void brInit(){ brPx=(SW-BR_PW)/2; brSc=0; brLi=3; brLv=0; brReset(); brBall(); brSt=BR_PLY; }
bool brClear(){ for(int r=0;r<BR_BRW;r++) for(int c=0;c<BR_BC;c++) if(brK[r][c]) return false; return true; }

void brScene(){
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0); display.print(F("L")); display.print(brLi);
  display.print(F(" S:")); display.print(brSc);
  char b[8]; sprintf(b,"Lv%d",brLv+1); display.setCursor(SW-24,0); display.print(b);
  for(int r=0;r<BR_BRW;r++) for(int c=0;c<BR_BC;c++){
    if(!brK[r][c]) continue;
    int bx=BR_X0+c*(BR_BW+BR_BP), by=BR_Y0+r*(BR_BH+BR_BP);
    if(r<2) display.drawRect(bx,by,BR_BW,BR_BH,SSD1306_WHITE);
    else    display.fillRect(bx,by,BR_BW,BR_BH,SSD1306_WHITE);
  }
  display.fillRoundRect(brPx,BR_PY,BR_PW,BR_PH,1,SSD1306_WHITE);
  display.fillCircle((int)brBx,(int)brBy,BR_BR,SSD1306_WHITE);
}

void updateBreakout(){
  if(bM.pressed){appState=A_MENU;brSt=BR_TTL;return;}
  if(brSt==BR_TTL){cx("BREAKOUT",10,2);cx("ACT=start",44);if(bA.pressed)brInit();return;}
  if(brSt==BR_DED){cx("GAME OVER",8,2);char b[20];sprintf(b,"Score:%d",brSc);cx(b,38);cx("ACT=retry",54);if(bA.pressed)brInit();return;}
  if(brSt==BR_WIN){cx("YOU WIN!",8,2);char b[24];sprintf(b,"Sc:%d Lv%d",brSc,brLv+1);cx(b,34);cx("ACT=next",54);if(bA.pressed){brLv++;brReset();brBall();brSt=BR_PLY;}return;}
  if(brSt==BR_PAU){brScene();display.fillRect(24,22,80,22,SSD1306_BLACK);display.drawRect(24,22,80,22,SSD1306_WHITE);cx("PAUSED",25);cx("ACT=resume",34);if(bA.pressed)brSt=BR_PLY;return;}
  if(bL.held)brPx-=BR_PS; if(bR.held)brPx+=BR_PS; brPx=constrain(brPx,0,SW-BR_PW);
  if(brOp){brBx=brPx+BR_PW/2.0f;if(bA.pressed)brOp=false;brScene();cx("ACT=launch",48);return;}
  if(bA.pressed){brSt=BR_PAU;return;}
  brBx+=brDx; brBy+=brDy;
  if(brBx-BR_BR<=0){brBx=BR_BR;brDx=fabs(brDx);}
  if(brBx+BR_BR>=SW){brBx=SW-BR_BR;brDx=-fabs(brDx);}
  if(brBy-BR_BR<=0){brBy=BR_BR;brDy=fabs(brDy);}
  if(brDy>0&&brBy+BR_BR>=BR_PY&&brBy-BR_BR<=BR_PY+BR_PH&&brBx>=brPx-BR_BR&&brBx<=brPx+BR_PW+BR_BR){
    brBy=BR_PY-BR_BR;brDy=-fabs(brDy);
    float r=(brBx-(brPx+BR_PW/2.0f))/(BR_PW/2.0f); brDx=r*3.0f;
    if(fabs(brDx)<0.7f) brDx=(brDx>=0?0.7f:-0.7f);
  }
  if(brBy>SH+4){brLi--;if(brLi<=0){brSt=BR_DED;return;}brBall();return;}
  for(int r=0;r<BR_BRW;r++) for(int c=0;c<BR_BC;c++){
    if(!brK[r][c]) continue;
    int bx=BR_X0+c*(BR_BW+BR_BP),by=BR_Y0+r*(BR_BH+BR_BP);
    if(brBx+BR_BR>bx&&brBx-BR_BR<bx+BR_BW&&brBy+BR_BR>by&&brBy-BR_BR<by+BR_BH){
      brK[r][c]=false; brSc+=10*(BR_BRW-r);
      float oL=(brBx+BR_BR)-bx,oR=(bx+BR_BW)-(brBx-BR_BR),oT=(brBy+BR_BR)-by,oB=(by+BR_BH)-(brBy-BR_BR);
      if(min(oL,oR)<min(oT,oB)) brDx=-brDx; else brDy=-brDy;
      float sp=sqrt(brDx*brDx+brDy*brDy); if(sp<4.5f){brDx*=1.015f;brDy*=1.015f;}
      if(brClear()){brSt=BR_WIN;return;} goto brDone;
    }
  }
  brDone: brScene();
}

// ═════════════════════════════════════════════════════════
//  2. BOUNCE CLASSIC
// ═════════════════════════════════════════════════════════
#define BC_G   0.35f
#define BC_J  -4.5f
#define BC_MS  2.0f
#define BC_BR  4
#define NP 10
#define NR  8
#define NS  4

struct Plat{int x,y,w;}; struct Ring{int x,y;bool g;}; struct Spk{int x,y;};
Plat bcP[NP]; Ring bcR[NR]; Spk bcS[NS];
float bcBx,bcBy,bcVx,bcVy,bcCam; int bcSc,bcLi,bcLv,bcRL; bool bcGnd,bcPau;
enum{BC_TTL,BC_PLY,BC_DED,BC_WIN} bcSt=BC_TTL;

void bcGen(){
  int lw=600+bcLv*100; bcP[0]={0,58,lw};
  randomSeed(bcLv*1234+42);
  for(int i=1;i<NP;i++){bcP[i]={60+i*55+(int)random(0,20),30+(int)random(0,18),20+(int)random(0,20)};}
  bcRL=NR;
  for(int i=0;i<NR;i++){int pi=1+(i%(NP-1));bcR[i]={bcP[pi].x+bcP[pi].w/2+(int)random(-8,8),bcP[pi].y-12,false};}
  for(int i=0;i<NS;i++) bcS[i]={80+i*120+(int)random(0,40),52};
}
void bcSpawn(){bcBx=12+bcCam;bcBy=40;bcVx=0;bcVy=0;bcGnd=false;}
void bcInit(){bcLi=3;bcSc=0;bcLv=0;bcCam=0;bcGen();bcSpawn();bcPau=false;bcSt=BC_PLY;}
void bcNext(){bcLv++;bcCam=0;bcGen();bcSpawn();bcSt=BC_PLY;}

void updateBounce(){
  if(bM.pressed){appState=A_MENU;bcSt=BC_TTL;return;}
  if(bcSt==BC_TTL){cx("BOUNCE",4,2);cx("CLASSIC",22,2);cx("D0=jump D1/D7=move",46);cx("ACT=start",56);if(bA.pressed)bcInit();return;}
  if(bcSt==BC_DED){cx("OUCH!",10,2);char b[24];sprintf(b,"Lives:%d Sc:%d",bcLi,bcSc);cx(b,36);cx("ACT=continue",52);
    if(bA.pressed){if(bcLi<=0){bcSt=BC_TTL;bcSc=0;}else{bcSpawn();bcSt=BC_PLY;}} return;}
  if(bcSt==BC_WIN){cx("LEVEL CLEAR!",6,2);char b[20];sprintf(b,"Score:%d",bcSc);cx(b,38);cx("ACT=next",54);if(bA.pressed)bcNext();return;}
  if(bA.pressed)bcPau=!bcPau;
  if(!bcPau){
    bcVx=0; if(bL.held)bcVx=-BC_MS; if(bR.held)bcVx=BC_MS;
    if(bU.pressed&&bcGnd){bcVy=BC_J;bcGnd=false;}
    bcVy+=BC_G; if(bcVy>6)bcVy=6;
    bcBx+=bcVx; bcBy+=bcVy; bcGnd=false;
    for(int i=0;i<NP;i++){
      auto&p=bcP[i];
      if(bcVy>=0&&bcBx+BC_BR>p.x&&bcBx-BC_BR<p.x+p.w&&bcBy+BC_BR>=p.y&&bcBy+BC_BR<=p.y+8){
        bcBy=p.y-BC_BR;bcVy=0;bcGnd=true;
      }
    }
    if(bcBx-BC_BR<0)bcBx=BC_BR;
    float tc=bcBx-SW/3.0f; if(tc>bcCam)bcCam=tc; if(bcCam<0)bcCam=0;
    if(bcBy>SH+20){bcLi--;bcSt=BC_DED;return;}
    for(auto&s:bcS){float dx=bcBx-s.x,dy=bcBy-s.y;if(sqrt(dx*dx+dy*dy)<BC_BR+3){bcLi--;bcSt=BC_DED;return;}}
    for(auto&r:bcR){if(r.g)continue;float dx=bcBx-r.x,dy=bcBy-r.y;if(sqrt(dx*dx+dy*dy)<BC_BR+5){r.g=true;bcSc+=10;bcRL--;}}
    if(bcBx>590+bcLv*100||bcRL<=0){bcSc+=bcLi*50;bcSt=BC_WIN;}
  }
  // Draw
  int cam=(int)bcCam;
  for(int i=0;i<NP;i++){auto&p=bcP[i];int sx=p.x-cam;if(sx+p.w<0||sx>SW)continue;
    if(i==0)display.fillRect(sx,p.y,p.w,SH-p.y,SSD1306_WHITE);
    else display.fillRect(sx,p.y,p.w,4,SSD1306_WHITE);}
  for(auto&s:bcS){int sx=s.x-cam;if(sx<-4||sx>SW+4)continue;display.drawTriangle(sx,s.y+6,sx-4,s.y+6,sx,s.y,SSD1306_WHITE);}
  for(auto&r:bcR){if(r.g)continue;int sx=r.x-cam;if(sx<-8||sx>SW+8)continue;display.drawCircle(sx,r.y,4,SSD1306_WHITE);display.drawCircle(sx,r.y,2,SSD1306_WHITE);}
  int bsx=(int)bcBx-cam;display.fillCircle(bsx,(int)bcBy,BC_BR,SSD1306_WHITE);display.drawPixel(bsx-1,(int)bcBy-2,SSD1306_BLACK);
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);display.setCursor(0,0);
  display.print(F("L"));display.print(bcLi);display.print(F(" R:"));display.print(bcRL);
  display.setCursor(SW-36,0);display.print(F("S:"));display.print(bcSc);
  if(bcPau){display.fillRect(24,22,80,22,SSD1306_BLACK);display.drawRect(24,22,80,22,SSD1306_WHITE);cx("PAUSED",25);cx("ACT=resume",33);}
}

// ═════════════════════════════════════════════════════════
//  3 & 4. SNAKE
// ═════════════════════════════════════════════════════════
#define SN_CELL  4
#define SN_COLS  (SW/SN_CELL)   // 32
#define SN_ROWS  ((SH-8)/SN_CELL) // 14
#define SN_MAX   200

int snX[SN_MAX],snY[SN_MAX],snLen,snDx,snDy,snFx,snFy,snSc,snSpd;
bool snWrap,snDead,snPau;
unsigned long snLast;

void snFood(){
  do{snFx=random(0,SN_COLS);snFy=random(0,SN_ROWS);}
  while([&](){for(int i=0;i<snLen;i++)if(snX[i]==snFx&&snY[i]==snFy)return true;return false;}());
}
void snInit(bool wrap){
  snWrap=wrap;snLen=4;snDx=1;snDy=0;snSc=0;snPau=false;snDead=false;
  for(int i=0;i<snLen;i++){snX[i]=snLen-1-i;snY[i]=SN_ROWS/2;}
  snFood();snSpd=150;snLast=millis();
}

void updateSnake(bool wrap){
  if(bM.pressed){appState=A_MENU;snDead=false;return;}
  if(snDead){cx("GAME OVER",8,2);char b[20];sprintf(b,"Score:%d",snSc);cx(b,38);cx("ACT=retry",54);if(bA.pressed)snInit(wrap);return;}
  if(bA.pressed)snPau=!snPau;
  if(!snPau){
    if(bU.pressed&&snDy==0){snDx=0;snDy=-1;}
    if(bD.pressed&&snDy==0){snDx=0;snDy=1;}
    if(bL.pressed&&snDx==0){snDx=-1;snDy=0;}
    if(bR.pressed&&snDx==0){snDx=1;snDy=0;}
    unsigned long now=millis();
    if(now-snLast>=(unsigned long)snSpd){
      snLast=now;
      int nx=snX[0]+snDx, ny=snY[0]+snDy;
      if(wrap){nx=(nx+SN_COLS)%SN_COLS;ny=(ny+SN_ROWS)%SN_ROWS;}
      else if(nx<0||nx>=SN_COLS||ny<0||ny>=SN_ROWS){snDead=true;return;}
      for(int i=1;i<snLen;i++) if(snX[i]==nx&&snY[i]==ny){snDead=true;return;}
      bool ate=(nx==snFx&&ny==snFy);
      for(int i=snLen-1;i>0;i--){snX[i]=snX[i-1];snY[i]=snY[i-1];}
      snX[0]=nx;snY[0]=ny;
      if(ate){if(snLen<SN_MAX)snLen++;snSc+=10;if(snSpd>60)snSpd-=3;snFood();}
    }
  }
  // Draw
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(snWrap?F("Snake Wrap"):F("Snake Wall"));
  display.setCursor(SW-36,0);display.print(F("S:"));display.print(snSc);
  display.drawLine(0,8,SW,8,SSD1306_WHITE);
  int oy=9;
  for(int i=0;i<snLen;i++) display.fillRect(snX[i]*SN_CELL,oy+snY[i]*SN_CELL,SN_CELL-1,SN_CELL-1,SSD1306_WHITE);
  display.fillRect(snFx*SN_CELL+1,oy+snFy*SN_CELL+1,SN_CELL-2,SN_CELL-2,SSD1306_WHITE);
  if(snPau){cx("PAUSED",28);cx("ACT=resume",38);}
}

// ═════════════════════════════════════════════════════════
//  5. TETRIS
// ═════════════════════════════════════════════════════════
#define TC 10
#define TR 16
#define TS  4
uint8_t tetBoard[TR][TC];
int tetSc,tetLv,tetLines;
bool tetOver,tetPau;
int tetPx,tetPy,tetPt,tetPr;
unsigned long tetLast;
int tetNextT;

// Pieces: 7 types, 4 rotations each, 4 cells (row,col offsets)
const int8_t PIECES[7][4][4][2]={
  {{{0,0},{0,1},{0,2},{0,3}},{{0,0},{1,0},{2,0},{3,0}},{{0,0},{0,1},{0,2},{0,3}},{{0,0},{1,0},{2,0},{3,0}}}, // I
  {{{0,0},{0,1},{1,0},{1,1}},{{0,0},{0,1},{1,0},{1,1}},{{0,0},{0,1},{1,0},{1,1}},{{0,0},{0,1},{1,0},{1,1}}}, // O
  {{{0,1},{1,0},{1,1},{1,2}},{{0,0},{1,0},{1,1},{2,0}},{{1,0},{1,1},{1,2},{2,1}},{{0,1},{1,0},{1,1},{2,1}}}, // T
  {{{0,0},{1,0},{1,1},{1,2}},{{0,0},{0,1},{1,0},{2,0}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,0},{2,1}}}, // L
  {{{0,2},{1,0},{1,1},{1,2}},{{0,0},{1,0},{2,0},{2,1}},{{1,0},{1,1},{1,2},{2,0}},{{0,0},{0,1},{1,1},{2,1}}}, // J
  {{{0,1},{0,2},{1,0},{1,1}},{{0,0},{1,0},{1,1},{2,1}},{{0,1},{0,2},{1,0},{1,1}},{{0,0},{1,0},{1,1},{2,1}}}, // S
  {{{0,0},{0,1},{1,1},{1,2}},{{0,1},{1,0},{1,1},{2,0}},{{0,0},{0,1},{1,1},{1,2}},{{0,1},{1,0},{1,1},{2,0}}}  // Z
};

bool tetFits(int t,int r,int px,int py){
  for(int i=0;i<4;i++){
    int rr=py+PIECES[t][r][i][0], cc=px+PIECES[t][r][i][1];
    if(rr<0||rr>=TR||cc<0||cc>=TC) return false;
    if(tetBoard[rr][cc]) return false;
  }
  return true;
}
void tetPlace(int t,int r,int px,int py){
  for(int i=0;i<4;i++) tetBoard[py+PIECES[t][r][i][0]][px+PIECES[t][r][i][1]]=t+1;
}
int tetClear(){
  int n=0;
  for(int r=TR-1;r>=0;r--){
    bool full=true; for(int c=0;c<TC;c++) if(!tetBoard[r][c]){full=false;break;}
    if(full){n++;for(int rr=r;rr>0;rr--) for(int c=0;c<TC;c++) tetBoard[rr][c]=tetBoard[rr-1][c];
      for(int c=0;c<TC;c++) tetBoard[0][c]=0; r++;}
  }
  return n;
}
void tetSpawn(){
  tetPt=tetNextT; tetNextT=random(0,7); tetPr=0; tetPx=TC/2-2; tetPy=0;
  if(!tetFits(tetPt,tetPr,tetPx,tetPy)) tetOver=true;
}
void tetInit(){
  memset(tetBoard,0,sizeof(tetBoard));
  tetSc=0;tetLv=0;tetLines=0;tetOver=false;tetPau=false;
  tetNextT=random(0,7); tetLast=millis();
  tetSpawn();
}
int tetSpeed(){ return max(80,500-tetLv*40); }

void updateTetris(){
  if(bM.pressed){appState=A_MENU;tetOver=false;return;}
  static bool tetStarted=false;
  if(!tetStarted||tetOver){
    if(tetOver){cx("GAME OVER",8,2);char b[20];sprintf(b,"Sc:%d",tetSc);cx(b,38);cx("ACT=retry",54);if(bA.pressed){tetInit();tetStarted=true;}}
    else{cx("TETRIS",10,2);cx("ACT=start",44);if(bA.pressed){tetInit();tetStarted=true;}}
    if(!bA.pressed&&!tetOver) return;
    return;
  }
  if(bA.pressed)tetPau=!tetPau;
  if(!tetPau){
    static bool prevU=false,prevR=false,prevL=false,prevD=false;
    if(bR.pressed&&tetFits(tetPt,tetPr,tetPx+1,tetPy)) tetPx++;
    if(bL.pressed&&tetFits(tetPt,tetPr,tetPx-1,tetPy)) tetPx--;
    if(bU.pressed){ int nr=(tetPr+1)%4; if(tetFits(tetPt,nr,tetPx,tetPy)) tetPr=nr; }
    if(bD.held&&tetFits(tetPt,tetPr,tetPx,tetPy+1)) tetPy++;
    unsigned long now=millis();
    if(now-tetLast>=(unsigned long)tetSpeed()){
      tetLast=now;
      if(tetFits(tetPt,tetPr,tetPx,tetPy+1)) tetPy++;
      else{
        tetPlace(tetPt,tetPr,tetPx,tetPy);
        int cl=tetClear(); tetLines+=cl;
        int pts[]={0,40,100,300,1200}; tetSc+=pts[min(cl,4)]*(tetLv+1);
        tetLv=tetLines/10; tetSpawn();
      }
    }
  }
  // Draw board
  int ox=0,oy=0,bx=44;
  display.drawRect(bx,oy,TC*TS+2,SH,SSD1306_WHITE);
  for(int r=0;r<TR;r++) for(int c=0;c<TC;c++){
    if(tetBoard[r][c]) display.fillRect(bx+1+c*TS,oy+r*TS,TS-1,TS-1,SSD1306_WHITE);
  }
  // Current piece
  for(int i=0;i<4;i++){
    int rr=tetPy+PIECES[tetPt][tetPr][i][0], cc=tetPx+PIECES[tetPt][tetPr][i][1];
    display.fillRect(bx+1+cc*TS,oy+rr*TS,TS-1,TS-1,SSD1306_WHITE);
  }
  // Side panel
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(F("Sc"));
  display.setCursor(0,10);display.print(tetSc);
  display.setCursor(0,24);display.print(F("Lv"));
  display.setCursor(0,34);display.print(tetLv);
  display.setCursor(0,46);display.print(F("Nx"));
  // Next piece preview
  for(int i=0;i<4;i++){
    int rr=PIECES[tetNextT][0][i][0], cc=PIECES[tetNextT][0][i][1];
    display.fillRect(2+cc*TS,56+rr*TS,TS-1,TS-1,SSD1306_WHITE);
  }
  if(tetPau){display.fillRect(20,20,88,24,SSD1306_BLACK);display.drawRect(20,20,88,24,SSD1306_WHITE);cx("PAUSED",23);cx("ACT=resume",32);}
}

// ═════════════════════════════════════════════════════════
//  6. FLAPPY BIRD
// ═════════════════════════════════════════════════════════
#define FB_PIPES 3
#define FB_GAP   18
#define FB_PW     8
#define FB_SPD    2

float fbBy,fbVy; int fbSc; bool fbDead,fbStarted;
struct FbPipe{int x,gap;};
FbPipe fbP[FB_PIPES];

void fbInitPipe(int i,int prevX){
  fbP[i].x=prevX+50+random(0,20);
  fbP[i].gap=8+random(0,SH-FB_GAP-16);
}
void fbInit(){
  fbBy=SH/2; fbVy=0; fbSc=0; fbDead=false; fbStarted=false;
  for(int i=0;i<FB_PIPES;i++) fbInitPipe(i,SW+i*60);
}

void updateFlappy(){
  if(bM.pressed){appState=A_MENU;fbDead=false;return;}
  if(!fbStarted){cx("FLAPPY BIRD",4,2);cx("ACT=flap",36);cx("D9=menu",50);if(bA.pressed){fbInit();fbStarted=true;} return;}
  if(fbDead){cx("DEAD!",10,2);char b[20];sprintf(b,"Score:%d",fbSc);cx(b,36);cx("ACT=retry",52);if(bA.pressed)fbInit();return;}
  if(bA.pressed||bU.pressed) fbVy=-3.2f;
  fbVy+=0.4f; if(fbVy>5)fbVy=5;
  fbBy+=fbVy;
  if(fbBy<2){fbBy=2;fbVy=0;}
  if(fbBy>SH-4){fbDead=true;return;}
  for(int i=0;i<FB_PIPES;i++){
    fbP[i].x-=FB_SPD;
    if(fbP[i].x+FB_PW<0){fbInitPipe(i,fbP[(i+FB_PIPES-1)%FB_PIPES].x);fbSc++;}
    // collision
    if(14>fbP[i].x&&6<fbP[i].x+FB_PW){
      if((int)fbBy<fbP[i].gap||(int)fbBy>fbP[i].gap+FB_GAP) fbDead=true;
    }
  }
  // Draw
  display.fillCircle(14,(int)fbBy,4,SSD1306_WHITE);
  for(int i=0;i<FB_PIPES;i++){
    display.fillRect(fbP[i].x,0,FB_PW,fbP[i].gap,SSD1306_WHITE);
    display.fillRect(fbP[i].x,fbP[i].gap+FB_GAP,FB_PW,SH-(fbP[i].gap+FB_GAP),SSD1306_WHITE);
  }
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(fbSc);
}

// ═════════════════════════════════════════════════════════
//  7. ASTEROIDS
// ═════════════════════════════════════════════════════════
#define AS_MAXAST 8
#define AS_MAXBUL 4

struct AstShip{float x,y,ang,vx,vy;};
struct Asteroid{float x,y,vx,vy,r;bool alive;};
struct Bullet{float x,y,vx,vy;bool alive;int life;};

AstShip asShip;
Asteroid asAst[AS_MAXAST];
Bullet asBul[AS_MAXBUL];
int asSc,asLi,asWave; bool asOver,asPau;

void asSpawnWave(){
  for(int i=0;i<min(asWave+2,AS_MAXAST);i++){
    float ang=random(0,628)/100.0f;
    float spd=0.5f+random(0,10)/10.0f;
    float r=8+random(0,6);
    float sx,sy;
    do{sx=random(0,SW);sy=random(0,SH);}
    while(sqrt((sx-asShip.x)*(sx-asShip.x)+(sy-asShip.y)*(sy-asShip.y))<30);
    asAst[i]={sx,sy,cos(ang)*spd,sin(ang)*spd,r,true};
  }
  for(int i=min(asWave+2,AS_MAXAST);i<AS_MAXAST;i++) asAst[i].alive=false;
}
void asInit(){
  asShip={SW/2.0f,SH/2.0f,0,0,0};
  for(auto&b:asBul) b.alive=false;
  asSc=0;asLi=3;asWave=0;asOver=false;asPau=false;
  asSpawnWave();
}
bool asAnyAlive(){ for(auto&a:asAst) if(a.alive) return true; return false; }

void updateAsteroids(){
  if(bM.pressed){appState=A_MENU;asOver=false;return;}
  if(asOver){cx("ASTEROIDS",4,2);cx("GAME OVER",24,2);char b[16];sprintf(b,"Sc:%d",asSc);cx(b,46);cx("ACT=start",56);if(bA.pressed)asInit();return;}
  if(!asAst[0].alive&&!asAnyAlive()){asWave++;asSpawnWave();}
  if(bA.pressed)asPau=!asPau;
  if(asPau){cx("PAUSED",24);cx("ACT=resume",36);return;}
  // Input
  if(bL.held) asShip.ang-=0.12f;
  if(bR.held) asShip.ang+=0.12f;
  if(bU.held){ asShip.vx+=cos(asShip.ang)*0.3f; asShip.vy+=sin(asShip.ang)*0.3f; }
  float spd=sqrt(asShip.vx*asShip.vx+asShip.vy*asShip.vy);
  if(spd>4){ asShip.vx=asShip.vx/spd*4; asShip.vy=asShip.vy/spd*4; }
  asShip.vx*=0.98f; asShip.vy*=0.98f;
  asShip.x+=asShip.vx; asShip.y+=asShip.vy;
  asShip.x=fmod(asShip.x+SW,SW); asShip.y=fmod(asShip.y+SH,SH);
  // Shoot
  static bool prevA=false;
  if(bD.pressed){
    for(auto&b:asBul) if(!b.alive){
      b={asShip.x,asShip.y,cos(asShip.ang)*5,sin(asShip.ang)*5,true,30}; break;
    }
  }
  // Bullets
  for(auto&b:asBul){
    if(!b.alive) continue;
    b.x+=b.vx; b.y+=b.vy; b.life--;
    if(b.life<=0||b.x<0||b.x>SW||b.y<0||b.y>SH) b.alive=false;
  }
  // Asteroids
  for(auto&a:asAst){
    if(!a.alive) continue;
    a.x+=a.vx; a.y+=a.vy;
    a.x=fmod(a.x+SW,SW); a.y=fmod(a.y+SH,SH);
    // Ship collision
    float dx=asShip.x-a.x,dy=asShip.y-a.y;
    if(sqrt(dx*dx+dy*dy)<a.r+3){ asLi--; if(asLi<=0){asOver=true;return;} asShip={SW/2.0f,SH/2.0f,0,0,0}; }
    // Bullet collision
    for(auto&b:asBul){
      if(!b.alive) continue;
      float bx=b.x-a.x,by=b.y-a.y;
      if(sqrt(bx*bx+by*by)<a.r){ b.alive=false; a.alive=false; asSc+=10; break; }
    }
  }
  // Draw ship
  float tx=asShip.x+cos(asShip.ang)*8, ty=asShip.y+sin(asShip.ang)*8;
  float lx=asShip.x+cos(asShip.ang+2.5f)*5, ly=asShip.y+sin(asShip.ang+2.5f)*5;
  float rx=asShip.x+cos(asShip.ang-2.5f)*5, ry=asShip.y+sin(asShip.ang-2.5f)*5;
  display.drawLine((int)tx,(int)ty,(int)lx,(int)ly,SSD1306_WHITE);
  display.drawLine((int)tx,(int)ty,(int)rx,(int)ry,SSD1306_WHITE);
  display.drawLine((int)lx,(int)ly,(int)rx,(int)ry,SSD1306_WHITE);
  // Draw asteroids
  for(auto&a:asAst) if(a.alive) display.drawCircle((int)a.x,(int)a.y,(int)a.r,SSD1306_WHITE);
  // Draw bullets
  for(auto&b:asBul) if(b.alive) display.fillCircle((int)b.x,(int)b.y,1,SSD1306_WHITE);
  // HUD
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(F("L"));display.print(asLi);
  display.setCursor(SW-30,0);display.print(F("S:"));display.print(asSc);
  // Controls hint
  display.setCursor(0,56);display.print(F("L/R=rot U=thru D=fire"));
}

// ═════════════════════════════════════════════════════════
//  8. PAC-MAN (simplified)
// ═════════════════════════════════════════════════════════
#define PM_CELL 4
#define PM_COLS 16
#define PM_ROWS 12

// Maze: 0=dot, 1=wall, 2=empty, 3=power
const uint8_t PM_MAZE_TMPL[PM_ROWS][PM_COLS]={
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,3,1,1,0,1,0,1,1,0,1,0,1,1,3,1},
  {1,0,1,1,0,1,0,0,0,0,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,1,1,1,1,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,1,0,1,1,2,2,1,1,0,1,1,0,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,3,1,1,0,1,0,0,0,0,1,0,1,1,3,1},
  {1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,1},
  {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
};
uint8_t pmMaze[PM_ROWS][PM_COLS];
float pmPx,pmPy; int pmDx,pmDy,pmNDx,pmNDy,pmSc,pmLi,pmDots; bool pmPow,pmOver;
unsigned long pmPowEnd;
struct Ghost{float x,y;int dx,dy;};
Ghost pmG[2];

void pmInitMaze(){ for(int r=0;r<PM_ROWS;r++) for(int c=0;c<PM_COLS;c++){pmMaze[r][c]=PM_MAZE_TMPL[r][c];if(pmMaze[r][c]==0||pmMaze[r][c]==3)pmDots++;}}
void pmInitGhost(int i){ pmG[i]={(float)(7+i),4.0f,1,0}; }
void pmInit(){
  pmDots=0; pmInitMaze();
  pmPx=1;pmPy=1;pmDx=1;pmDy=0;pmNDx=1;pmNDy=0;
  pmSc=0;pmLi=3;pmPow=false;pmOver=false;
  pmInitGhost(0);pmInitGhost(1);
}

bool pmWall(int x,int y){ if(x<0||x>=PM_COLS||y<0||y>=PM_ROWS) return true; return pmMaze[y][x]==1; }

void pmMoveGhost(Ghost&g){
  // Simple: try current dir, else random turn
  int nx=g.x+g.dx, ny=g.y+g.dy;
  if(!pmWall(nx,ny)){g.x=nx;g.y=ny;}
  else{
    int dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    int r=random(0,4);
    for(int i=0;i<4;i++){
      int d=(r+i)%4;
      if(!pmWall(g.x+dirs[d][0],g.y+dirs[d][1])){g.dx=dirs[d][0];g.dy=dirs[d][1];break;}
    }
  }
}

void updatePacman(){
  if(bM.pressed){appState=A_MENU;pmOver=false;return;}
  static bool pmStarted=false;
  if(!pmStarted||pmOver){
    if(pmOver){cx("PAC-MAN",4,2);char b[20];sprintf(b,"Score:%d",pmSc);cx(b,28);cx(pmLi<=0?"GAME OVER":"YOU WIN!",40,1);cx("ACT=start",54);}
    else{cx("PAC-MAN",10,2);cx("ACT=start",44);}
    if(bA.pressed){pmInit();pmStarted=true;pmOver=false;} return;
  }
  // Input (next dir)
  if(bU.pressed){pmNDx=0;pmNDy=-1;} if(bD.pressed){pmNDx=0;pmNDy=1;}
  if(bL.pressed){pmNDx=-1;pmNDy=0;} if(bR.pressed){pmNDx=1;pmNDy=0;}
  static unsigned long pmLast=0;
  unsigned long now=millis();
  if(now-pmLast>200){
    pmLast=now;
    // Try to turn
    if(!pmWall(pmPx+pmNDx,pmPy+pmNDy)){pmDx=pmNDx;pmDy=pmNDy;}
    int nx=pmPx+pmDx,ny=pmPy+pmDy;
    if(!pmWall(nx,ny)){pmPx=nx;pmPy=ny;}
    // Eat dot
    int cx2=(int)pmPx,cy2=(int)pmPy;
    if(pmMaze[cy2][cx2]==0){pmMaze[cy2][cx2]=2;pmSc+=10;pmDots--;}
    else if(pmMaze[cy2][cx2]==3){pmMaze[cy2][cx2]=2;pmSc+=50;pmDots--;pmPow=true;pmPowEnd=millis()+5000;}
    if(pmPow&&millis()>pmPowEnd) pmPow=false;
    if(pmDots<=0){pmOver=true;return;}
    // Move ghosts every other tick
    static int gTick=0; if(++gTick>=2){gTick=0;for(auto&g:pmG)pmMoveGhost(g);}
    // Ghost collision
    for(auto&g:pmG){
      if(abs(g.x-pmPx)<1&&abs(g.y-pmPy)<1){
        if(pmPow){pmSc+=200;pmInitGhost(&g-pmG);}
        else{pmLi--;if(pmLi<=0){pmOver=true;return;}pmPx=1;pmPy=1;}
      }
    }
  }
  // Draw
  for(int r=0;r<PM_ROWS;r++) for(int c=0;c<PM_COLS;c++){
    int px=c*PM_CELL,py=r*PM_CELL;
    if(pmMaze[r][c]==1) display.fillRect(px,py,PM_CELL,PM_CELL,SSD1306_WHITE);
    else if(pmMaze[r][c]==0) display.fillCircle(px+PM_CELL/2,py+PM_CELL/2,1,SSD1306_WHITE);
    else if(pmMaze[r][c]==3) display.fillCircle(px+PM_CELL/2,py+PM_CELL/2,2,SSD1306_WHITE);
  }
  // Pac-Man
  int ppx=(int)pmPx*PM_CELL+PM_CELL/2, ppy=(int)pmPy*PM_CELL+PM_CELL/2;
  display.fillCircle(ppx,ppy,PM_CELL/2-1,SSD1306_WHITE);
  // Ghosts
  for(auto&g:pmG){
    int gx=(int)g.x*PM_CELL,gy=(int)g.y*PM_CELL;
    if(pmPow) display.drawRect(gx,gy,PM_CELL,PM_CELL,SSD1306_WHITE);
    else      display.fillRect(gx,gy,PM_CELL,PM_CELL,SSD1306_WHITE);
  }
  // HUD overlay
  display.fillRect(0,0,SW,8,SSD1306_BLACK);
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(F("L"));display.print(pmLi);
  display.setCursor(SW-36,0);display.print(F("S:"));display.print(pmSc);
  if(pmPow){display.setCursor(50,0);display.print(F("POW!"));}
}

// ═════════════════════════════════════════════════════════
//  9. SPACE SHOOTER
// ═════════════════════════════════════════════════════════
#define SH_MAXE  8
#define SH_MAXB  6
#define SH_MAXEB 4

struct ShPlayer{int x,y;};
struct ShEnemy{float x,y;bool alive;int type;};
struct ShBullet{float x,y;bool alive;bool enemy;};

ShPlayer shP;
ShEnemy  shE[SH_MAXE];
ShBullet shB[SH_MAXB+SH_MAXEB];
int shSc,shLi,shWave; bool shOver;
unsigned long shShootCD,shESpawn,shEShoot;

void shSpawnEnemy(){
  for(auto&e:shE) if(!e.alive){
    e={((float)random(10,SW-10)),(0.0f),true,random(0,3)};break;
  }
}
void shInit(){
  shP={SW/2,SH-8}; shSc=0;shLi=3;shWave=0;shOver=false;
  for(auto&e:shE) e.alive=false;
  for(auto&b:shB) b.alive=false;
  shShootCD=0;shESpawn=millis();shEShoot=millis();
}

void updateShooter(){
  if(bM.pressed){appState=A_MENU;shOver=false;return;}
  static bool shStarted=false;
  if(!shStarted){cx("SPACE",4,2);cx("SHOOTER",20,2);cx("ACT=start",46);if(bA.pressed){shInit();shStarted=true;}return;}
  if(shOver){cx("GAME OVER",8,2);char b[20];sprintf(b,"Score:%d",shSc);cx(b,38);cx("ACT=retry",54);if(bA.pressed)shInit();return;}

  if(bL.held)shP.x-=2; if(bR.held)shP.x+=2;
  shP.x=constrain(shP.x,4,SW-4);
  unsigned long now=millis();
  if(bA.held&&now>shShootCD){
    shShootCD=now+200;
    for(auto&b:shB) if(!b.alive&&!b.enemy){b={((float)shP.x),((float)shP.y),true,false};break;}
  }
  // Spawn enemies
  if(now>shESpawn){shESpawn=now+max(400,1500-shWave*100);shSpawnEnemy();}
  // Enemy shoot
  if(now>shEShoot){
    shEShoot=now+max(600,2000-shWave*150);
    for(auto&e:shE) if(e.alive){
      for(auto&b:shB) if(!b.alive&&b.enemy){b={e.x,e.y,true,true};break;}
      break;
    }
  }
  // Move bullets
  for(auto&b:shB){
    if(!b.alive) continue;
    b.y+=b.enemy?1.5f:-3.0f;
    if(b.y<0||b.y>SH)b.alive=false;
  }
  // Move enemies
  for(int i=0;i<SH_MAXE;i++){
    if(!shE[i].alive) continue;
    shE[i].y+=0.5f+(shWave*0.05f);
    shE[i].x+=sin(millis()/400.0f+i)*0.8f;
    if(shE[i].y>SH){shE[i].alive=false;shLi--;if(shLi<=0){shOver=true;return;}}
    // Player hit
    if(abs(shE[i].x-shP.x)<6&&abs(shE[i].y-shP.y)<6){shLi--;shE[i].alive=false;if(shLi<=0){shOver=true;return;}}
    // Bullet hit
    for(auto&b:shB){
      if(!b.alive||b.enemy) continue;
      if(abs(b.x-shE[i].x)<5&&abs(b.y-shE[i].y)<5){b.alive=false;shE[i].alive=false;shSc+=10*(i%3+1);shWave=shSc/100;break;}
    }
  }
  // Enemy bullet hits player
  for(auto&b:shB){
    if(!b.alive||!b.enemy) continue;
    if(abs(b.x-shP.x)<4&&abs(b.y-shP.y)<4){b.alive=false;shLi--;if(shLi<=0){shOver=true;return;}}
  }
  // Draw stars (simple scanline)
  for(int i=0;i<8;i++) display.drawPixel((i*17+now/50)%SW,(i*13+now/80)%SH,SSD1306_WHITE);
  // Draw enemies
  for(auto&e:shE) if(e.alive){
    int ex=(int)e.x,ey=(int)e.y;
    display.drawLine(ex-4,ey,ex+4,ey,SSD1306_WHITE);
    display.drawLine(ex,ey-3,ex,ey+3,SSD1306_WHITE);
    display.fillCircle(ex,ey,2,SSD1306_WHITE);
  }
  // Draw bullets
  for(auto&b:shB) if(b.alive) display.fillRect((int)b.x-1,(int)b.y-2,2,4,SSD1306_WHITE);
  // Draw player ship
  display.fillTriangle(shP.x,shP.y-5,shP.x-4,shP.y+3,shP.x+4,shP.y+3,SSD1306_WHITE);
  // HUD
  display.fillRect(0,0,SW,7,SSD1306_BLACK);
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);display.print(F("L"));display.print(shLi);
  display.setCursor(SW-36,0);display.print(F("S:"));display.print(shSc);
}

// ═════════════════════════════════════════════════════════
//  10. TICTACTOE vs AI (Minimax)
// ═════════════════════════════════════════════════════════
int ttB[9]; // 0=empty,1=X,2=O
int ttCur; bool ttOver; char ttMsg[24];

int ttCheck(int b[9]){
  int w[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
  for(auto&l:w){ if(b[l[0]]&&b[l[0]]==b[l[1]]&&b[l[1]]==b[l[2]]) return b[l[0]]; }
  return 0;
}
bool ttFull(int b[9]){ for(int i=0;i<9;i++) if(!b[i]) return false; return true; }

int minimax(int b[9],bool isAI){
  int w=ttCheck(b);
  if(w==2) return 10;
  if(w==1) return -10;
  if(ttFull(b)) return 0;
  int best=isAI?-100:100;
  for(int i=0;i<9;i++){
    if(b[i]) continue;
    b[i]=isAI?2:1;
    int sc=minimax(b,!isAI);
    b[i]=0;
    if(isAI&&sc>best) best=sc;
    if(!isAI&&sc<best) best=sc;
  }
  return best;
}
void ttAIMove(){
  int best=-100,bm=-1;
  for(int i=0;i<9;i++){
    if(ttB[i]) continue;
    ttB[i]=2; int sc=minimax(ttB,false); ttB[i]=0;
    if(sc>best){best=sc;bm=i;}
  }
  if(bm>=0) ttB[bm]=2;
}
void ttInit(){ memset(ttB,0,sizeof(ttB)); ttCur=0; ttOver=false; strcpy(ttMsg,"Your turn (X)"); }

void updateTicTacToe(){
  if(bM.pressed){appState=A_MENU;ttOver=false;return;}
  static bool ttStarted=false;
  if(!ttStarted){cx("TICTACTOE",4,2);cx("vs AI",24,2);cx("ACT=start",48);if(bA.pressed){ttInit();ttStarted=true;}return;}
  // Draw grid
  int ox=32,oy=2,cs=19;
  for(int i=1;i<3;i++){
    display.drawLine(ox+i*cs,oy,ox+i*cs,oy+3*cs,SSD1306_WHITE);
    display.drawLine(ox,oy+i*cs,ox+3*cs,oy+i*cs,SSD1306_WHITE);
  }
  for(int i=0;i<9;i++){
    int r=i/3,c=i%3,cx2=ox+c*cs+cs/2,cy2=oy+r*cs+cs/2;
    if(ttB[i]==1){display.drawLine(cx2-5,cy2-5,cx2+5,cy2+5,SSD1306_WHITE);display.drawLine(cx2+5,cy2-5,cx2-5,cy2+5,SSD1306_WHITE);}
    else if(ttB[i]==2) display.drawCircle(cx2,cy2,6,SSD1306_WHITE);
    if(i==ttCur&&!ttOver){display.drawRect(ox+c*cs+1,oy+r*cs+1,cs-2,cs-2,SSD1306_WHITE);}
  }
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,24);
  char lns[24]; strncpy(lns,ttMsg,23);
  // word wrap at 16 chars
  for(int i=0;i<16&&lns[i];i++) display.print(lns[i]);
  if(ttOver){display.setCursor(0,48);display.print(F("ACT=new game"));}
  if(ttOver){if(bA.pressed)ttInit();return;}
  if(bL.pressed) ttCur=(ttCur+8)%9;
  if(bR.pressed) ttCur=(ttCur+1)%9;
  if(bU.pressed) ttCur=(ttCur+6)%9;
  if(bD.pressed) ttCur=(ttCur+3)%9;
  if(bA.pressed&&!ttB[ttCur]){
    ttB[ttCur]=1;
    int w=ttCheck(ttB);
    if(w){strcpy(ttMsg,"You win!");ttOver=true;return;}
    if(ttFull(ttB)){strcpy(ttMsg,"Draw!");ttOver=true;return;}
    ttAIMove();
    w=ttCheck(ttB);
    if(w){strcpy(ttMsg,"AI wins!");ttOver=true;}
    else if(ttFull(ttB)){strcpy(ttMsg,"Draw!");ttOver=true;}
    else strcpy(ttMsg,"Your turn");
  }
}

// ═════════════════════════════════════════════════════════
//  11. PONG / AIR HOCKEY
// ═════════════════════════════════════════════════════════
#define PN_PH 16
#define PN_PW  4
#define PN_BR  3
#define PN_SPD 3

float pnBx,pnBy,pnDx,pnDy;
int pnP1y,pnP2y,pnSc1,pnSc2; bool pnOver,pnPau;

void pnInit(){
  pnBx=SW/2;pnBy=SH/2;
  float ang=(random(0,4)*1.5707f)+0.3f;
  pnDx=cos(ang)*2.5f;pnDy=sin(ang)*2.5f;
  pnP1y=SH/2-PN_PH/2;pnP2y=SH/2-PN_PH/2;
  pnSc1=0;pnSc2=0;pnOver=false;pnPau=false;
}
void pnResetBall(){
  pnBx=SW/2;pnBy=SH/2;
  float ang=(random(0,4)*1.5707f)+0.3f;
  pnDx=cos(ang)*2.5f;pnDy=sin(ang)*2.5f;
}

void updatePong(){
  if(bM.pressed){appState=A_MENU;pnOver=false;return;}
  static bool pnStarted=false;
  if(!pnStarted){cx("PONG",4,2);cx("AIR HOCKEY",22,2);cx("U/D=you  AI=right",46);cx("ACT=start",56);if(bA.pressed){pnInit();pnStarted=true;}return;}
  if(pnOver){cx("GAME OVER",8,2);char b[24];sprintf(b,"You %d - %d AI",pnSc1,pnSc2);cx(b,36);cx("ACT=retry",54);if(bA.pressed)pnInit();return;}
  if(bA.pressed)pnPau=!pnPau;
  if(pnPau){
    // Draw partial
    display.drawLine(SW/2,0,SW/2,SH,SSD1306_WHITE);
    display.fillRect(0,pnP1y,PN_PW,PN_PH,SSD1306_WHITE);
    display.fillRect(SW-PN_PW,pnP2y,PN_PW,PN_PH,SSD1306_WHITE);
    display.fillCircle((int)pnBx,(int)pnBy,PN_BR,SSD1306_WHITE);
    cx("PAUSED",24);cx("ACT=resume",34);
    return;
  }
  // Player 1
  if(bU.held)pnP1y-=PN_SPD; if(bD.held)pnP1y+=PN_SPD;
  pnP1y=constrain(pnP1y,0,SH-PN_PH);
  // AI player 2 (simple tracking)
  int aiTarget=pnBy-PN_PH/2;
  if(pnP2y<aiTarget)pnP2y+=2; else pnP2y-=2;
  pnP2y=constrain(pnP2y,0,SH-PN_PH);
  // Ball
  pnBx+=pnDx;pnBy+=pnDy;
  if(pnBy-PN_BR<=0){pnBy=PN_BR;pnDy=fabs(pnDy);}
  if(pnBy+PN_BR>=SH){pnBy=SH-PN_BR;pnDy=-fabs(pnDy);}
  // Paddle bounces
  if(pnBx-PN_BR<=PN_PW&&pnBy>pnP1y&&pnBy<pnP1y+PN_PH){
    pnBx=PN_PW+PN_BR;pnDx=fabs(pnDx)*1.05f;
    float rel=(pnBy-(pnP1y+PN_PH/2.0f))/(PN_PH/2.0f);pnDy=rel*3.0f;
    if(fabs(pnDx)>5)pnDx=5;
  }
  if(pnBx+PN_BR>=SW-PN_PW&&pnBy>pnP2y&&pnBy<pnP2y+PN_PH){
    pnBx=SW-PN_PW-PN_BR;pnDx=-fabs(pnDx)*1.05f;
    float rel=(pnBy-(pnP2y+PN_PH/2.0f))/(PN_PH/2.0f);pnDy=rel*3.0f;
    if(fabs(pnDx)>5)pnDx=-5;
  }
  // Scoring
  if(pnBx<0){pnSc2++;if(pnSc2>=7)pnOver=true;else pnResetBall();}
  if(pnBx>SW){pnSc1++;if(pnSc1>=7)pnOver=true;else pnResetBall();}
  // Draw
  display.drawLine(SW/2,0,SW/2,SH,SSD1306_WHITE);
  // Dashed center
  for(int y=0;y<SH;y+=6) display.drawPixel(SW/2,y,SSD1306_BLACK);
  display.fillRect(0,pnP1y,PN_PW,PN_PH,SSD1306_WHITE);
  display.fillRect(SW-PN_PW,pnP2y,PN_PW,PN_PH,SSD1306_WHITE);
  display.fillCircle((int)pnBx,(int)pnBy,PN_BR,SSD1306_WHITE);
  display.setTextSize(1);display.setTextColor(SSD1306_WHITE);
  display.setCursor(SW/2-16,0);display.print(pnSc1);display.print(F("-"));display.print(pnSc2);
}

// ═════════════════════════════════════════════════════════
//  SETUP / LOOP
// ═════════════════════════════════════════════════════════
void setup(){
  Serial.begin(115200);
  for(auto b:BTNS) pinMode(b->pin,INPUT_PULLUP);
  randomSeed(analogRead(A0));
  Wire.setSDA(6); Wire.setSCL(7); Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR)){
    pinMode(LED_BUILTIN,OUTPUT);
    while(true){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(300);}
  }
  appState=A_MENU;
}

void loop(){
  readBtns();
  // MODE button always returns to menu (except when already there)
  if(bM.pressed&&appState!=A_MENU){appState=A_MENU;return;}
  if(appState==A_MENU){menuUpdate();menuDraw();return;}
  display.clearDisplay();
  switch(appState){
    case A_BREAKOUT:  updateBreakout();  break;
    case A_BOUNCE:    updateBounce();    break;
    case A_SNAKE_W:   updateSnake(false);break;
    case A_SNAKE_WRAP:updateSnake(true); break;
    case A_TETRIS:    updateTetris();    break;
    case A_FLAPPY:    updateFlappy();    break;
    case A_ASTEROIDS: updateAsteroids(); break;
    case A_PACMAN:    updatePacman();    break;
    case A_SHOOTER:   updateShooter();   break;
    case A_TICTACTOE: updateTicTacToe(); break;
    case A_PONG:      updatePong();      break;
    default: break;
  }
  display.display();
  delay(16);
}
