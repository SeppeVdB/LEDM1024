/*
 * Code for LEDM 1024 by Seppe Van den Bergh 2022
 * 
 * This 32x32 LED screen is able to play John Conway's Game of Life, Snake, and Pong.
 * The external controller is needed to play Snake and Pong.
 * The turning knob on the side of the screen is used to adjust the frame rate during
 * the different applications. Therefore you can increase or decrease the difficulty
 * in Snake and Pong.
 * 
 * Enjoy!
 * 
 */

#include <SPI.h>
#include <EEPROM.h>
#include "images.h"

#define latch_pin 8
#define blank_pin 7
#define data_pin 11
#define clock_pin 13

#define buttons_1_pin A1
#define buttons_2_pin A2
#define buttons_3_pin A3
#define pot_pin A4
#define buttons_panel_pin A5
#define controller_check_pin 2


byte cathode[128];
byte anode[8];

byte button_values_4[15] = {8,  25,  41,  54,  65,  75,  85,  93,  100, 107, 114, 119, 124, 129, 134};
byte button_values_2[3] = {48, 112, 142};
uint16_t buttons_pressed = 0; // | control panel (4) | controller check (1) | left buttons (4) | middle buttons (4) | right buttons (2) | 0 |
char button_symbols[14] = {'R', 'L', 'X', 'O', 'L', 'D', 'R', 'U', 'S', 'X', 'O', 'T', 'u', 'd'};
byte pot_value;

//          controller               side panel
//  ╱¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯╲     │¯¯¯¯¯¯¯¯¯¯│
// │    U         T       u  │    │  X    O  │
// │  L   R     S   O        │    │          │
// │    D         X       d  │    │  L    R  │
//  ╲_______________________╱     │__________│


// game of life variables
byte cathode_next[128];
byte cathode_past[128];
byte num_neigh = 0;
int num_empty = 0;

// snake game variables
byte food[2];
byte dir;
byte body[132];
byte head[2];
int snake_length = 0;
byte food_onoff = 1;
int score = 0;
int high_score = 0;

// pong game variables
byte paddles[2] = {17, 17};
float ball[2] = {15.0, 19.0};   // [0-31, 8-30]
float ball_prev[2] = {15.0, 19.0};
byte ball_byte[2] = {15, 19};
float angle = 0.0;
byte forward = 0;
byte upward = 0;
byte b = 14;
byte scores[2] = {0, 0};
byte start_dir = 1;

int shift_out;
byte anodelevel = 0;

byte rand_byte;

int i, j, k;  // for behind the scenes stuff
int m, n, o; // for applications
boolean play = true;
boolean game_over = false;
boolean menu = true;

byte screen_num = 0;
const int* menu_screens[3];
void setup_menu_screens(){
  menu_screens[0] = game_of_life_ss;
  menu_screens[1] = snake_ss;
  menu_screens[2] = pong_ss;
}
void (*applications[3])() {
  game_of_life,
  snake,
  pong
};

//////////////////////////////////////│¯¯¯¯¯¯¯│//////////////////////////////////////
//////////////////////////////////////│ Setup │//////////////////////////////////////
//////////////////////////////////////│_______│//////////////////////////////////////

void setup(){

  SPI.setBitOrder(LSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV2);

  noInterrupts();

  TCCR1A = B00000000;
  TCCR1B = B00001011;     //bit 3 to activate interrupts, bit 0 and 1 to divide clock by 64: 16MHz/64=250kHz
  TIMSK1 = B00000010;     //bit 1 to call interrupt on OCR1A match
  OCR1A = 30;     //to calculate multiplex frequency: f = 250kHz/(OCR1A+1)
                  //with OCR1A=30: f = 250000/31 ≈ 8kHz

  for(i=7; i>-1; i--){
    bitSet(anode[i], i);
  }
  for(i=0; i<128; i++){
    cathode[i] = 0;
  }

  EEPROM.get(0, high_score);
  if(high_score < 2){
    high_score = 23;
    EEPROM.put(0, high_score);
  }
  
  pinMode(latch_pin, OUTPUT);     //Latch
  pinMode(data_pin, OUTPUT);      //MOSI DATA
  pinMode(clock_pin, OUTPUT);     //SPI Clock
  pinMode(blank_pin, OUTPUT);     //Output Enable  important to do this last, so LEDs do not flash on boot up
  SPI.begin();      //start up the SPI library
  interrupts();     //start multiplexing

  randomSeed(analogRead(0));

  setup_menu_screens();
}

//////////////////////////////////////│¯¯¯¯¯¯│//////////////////////////////////////
//////////////////////////////////////│ Loop │//////////////////////////////////////
//////////////////////////////////////│______│//////////////////////////////////////

void loop(){
  while(menu){
    buttonRead();
    displayFile(menu_screens[screen_num]);

    if(screen_num == 1){
      EEPROM.get(0, high_score);
      displayNumber(high_score, 24, 3);
      delay(50);
    }
    
    switch(whichButton()){
      case 'L':
        if(screen_num == 0) screen_num = 2;
        else screen_num --;
        scroll(menu_screens[screen_num], 'L', 20);
        break;
      case 'R':
        if(screen_num == 2) screen_num = 0;
        else screen_num ++;
        scroll(menu_screens[screen_num], 'R', 20);
        break;
      case 'D':
      case 'd':
      case 'X':
        if(screen_num > 0 and !controllerCheck()){
          showError();
          delay(500);
          break;
        }
        play = true;
        game_over = false;
        applications[screen_num]();
        break;
    }
  }
}

//////////////////////////////////////│¯¯¯¯¯¯¯¯¯¯¯│//////////////////////////////////////
//////////////////////////////////////│ Functions │//////////////////////////////////////
//////////////////////////////////////│___________│//////////////////////////////////////

void LED(byte row, byte column, boolean state){
  row = constrain(row, 0, 31);
  column = constrain(column, 0, 31);
  byte whichbyte = constrain((row*32+column)/8, 0, 127);
  byte whichbit = column%8;

  bitWrite(cathode[whichbyte], 7-whichbit, state);
}

void displayFile(uint16_t image[64]){
  for(i=0; i<32; i++){
    cathode[4*i] = highByte(image[i*2]);
    cathode[4*i+1] = lowByte(image[i*2]);
    cathode[4*i+2] = highByte(image[i*2+1]);
    cathode[4*i+3] = lowByte(image[i*2+1]);
  }
}

void scroll(uint16_t image[64], char side, int delay_time){
  if(side == 'L' || side == 'W'){
    for(i=0; i<32; i++){
      for(j=0; j<32; j++){
        cathode[j*4+3] = cathode[j*4+3] >> 1;
        bitWrite(cathode[j*4+3], 7, bitRead(cathode[j*4+2], 0));
        cathode[j*4+2] = cathode[j*4+2] >> 1;
        bitWrite(cathode[j*4+2], 7, bitRead(cathode[j*4+1], 0));
        cathode[j*4+1] = cathode[j*4+1] >> 1;
        bitWrite(cathode[j*4+1], 7, bitRead(cathode[j*4], 0));
        cathode[j*4] = cathode[j*4] >> 1;
        bitWrite(cathode[j*4], 7, bitRead(image[j*2+1-i/16], i-16*(i/16)));
      }
      delay(delay_time);
    }
  }else if(side == 'R' || side == 'E'){
    for(i=0; i<32; i++){
      for(j=0; j<32; j++){
        cathode[j*4] = cathode[j*4] << 1;
        bitWrite(cathode[j*4], 0, bitRead(cathode[j*4+1], 7));
        cathode[j*4+1] = cathode[j*4+1] << 1;
        bitWrite(cathode[j*4+1], 0, bitRead(cathode[j*4+2], 7));
        cathode[j*4+2] = cathode[j*4+2] << 1;
        bitWrite(cathode[j*4+2], 0, bitRead(cathode[j*4+3], 7));
        cathode[j*4+3] = cathode[j*4+3] << 1;
        bitWrite(cathode[j*4+3], 0, bitRead(image[j*2+i/16], 15-i+16*(i/16)));
      }
      delay(delay_time);
    }
  }else if(side == 'U' || side == 'N'){
    for(i=31; i>-1; i--){
      for(j=31; j>0; j--){
        cathode[j*4] = cathode[j*4-4];
        cathode[j*4+1] = cathode[j*4-3];
        cathode[j*4+2] = cathode[j*4-2];
        cathode[j*4+3] = cathode[j*4-1];
      }
      cathode[0] = highByte(image[i*2]);
      cathode[1] = lowByte(image[i*2]);
      cathode[2] = highByte(image[i*2+1]);
      cathode[3] = lowByte(image[i*2+1]);
      delay(delay_time);
    }
  }else if(side == 'D' || side == 'S'){
    for(i=0; i<32; i++){
      for(j=0; j<31; j++){
        cathode[j*4] = cathode[j*4+4];
        cathode[j*4+1] = cathode[j*4+5];
        cathode[j*4+2] = cathode[j*4+6];
        cathode[j*4+3] = cathode[j*4+7];
      }
      cathode[124] = highByte(image[i*2]);
      cathode[125] = lowByte(image[i*2]);
      cathode[126] = highByte(image[i*2+1]);
      cathode[127] = lowByte(image[i*2+1]);
      delay(delay_time);
    }
  }
}

boolean isEqual(byte array_1[128], byte array_2[128]){
  for(i=0; i<128; i++) if(array_1[i] != array_2[i]) return false;
  return true;
}

void clearScreen(){
  memset(cathode, 0, sizeof(cathode));
}

uint16_t buttonRead(){
  byte buttons_panel = map(analogRead(buttons_panel_pin), 0, 1023, 0, 255);
  byte controller = digitalRead(controller_check_pin);
  pot_value = map(analogRead(pot_pin), 0, 1023, 0, 255);

  byte index_1 = 7;
  for(i=0; i<3; i++){
    if(buttons_panel < button_values_4[index_1]){
      bitClear(buttons_pressed, 15-i);
      index_1 -= 1 << (2-i);
    } else{
      bitSet(buttons_pressed, 15-i);
      index_1 += 1 << (2-i);
    }
  }
  bitWrite(buttons_pressed, 12, !(buttons_panel < button_values_4[index_1]));

  if(controller == LOW){
    bitSet(buttons_pressed, 1);

    byte buttons_1 = map(analogRead(buttons_1_pin), 0, 1023, 0, 255);
    byte buttons_2 = map(analogRead(buttons_2_pin), 0, 1023, 0, 255);
    byte buttons_3 = map(analogRead(buttons_3_pin), 0, 1023, 0, 255);

    index_1 = 7;
    byte index_2 = 7;
    for(i=0; i<3; i++){
      if(buttons_1 < button_values_4[index_1]){
        bitClear(buttons_pressed, 11-i);
        index_1 -= 1 << (2-i);
      } else{
        bitSet(buttons_pressed, 11-i);
        index_1 += 1 << (2-i);
      }
      if(buttons_2 < button_values_4[index_2]){
        bitClear(buttons_pressed, 7-i);
        index_2 -= 1 << (2-i);
      } else{
        bitSet(buttons_pressed, 7-i);
        index_2 += 1 << (2-i);
      }
    }
    bitWrite(buttons_pressed, 8, !(buttons_1 < button_values_4[index_1]));
    bitWrite(buttons_pressed, 4, !(buttons_2 < button_values_4[index_2]));
    if(buttons_3 < button_values_2[1]){
      bitClear(buttons_pressed, 3);
      bitWrite(buttons_pressed, 2, !(buttons_3 < button_values_2[0]));
    } else{
      bitSet(buttons_pressed, 3);
      bitWrite(buttons_pressed, 2, !(buttons_3 < button_values_2[2]));
    }
  } else buttons_pressed &= 0b1111000000000000;
}

boolean controllerCheck(){
  return bitRead(buttons_pressed, 1);
}

boolean buttonCheck(char button){
  switch(button){
    case 'd':   // down controller (right side)
      return bitRead(buttons_pressed, 2);
    case 'u':   // up controller (right side)
      return bitRead(buttons_pressed, 3);
    case 'T':   // triangle controller
      return bitRead(buttons_pressed, 4);
    case 'O':   // circle controller or panel
      return bitRead(buttons_pressed, 5) or bitRead(buttons_pressed, 12);
    case 'X':   // cross controller or panel
      return bitRead(buttons_pressed, 6) or bitRead(buttons_pressed, 13);
    case 'S':   // square controller
      return bitRead(buttons_pressed, 7);
    case 'U':   // up controller (left side)
      return bitRead(buttons_pressed, 8);
    case 'R':   // right controller or panel
      return bitRead(buttons_pressed, 9) or bitRead(buttons_pressed, 15);
    case 'D':   // down controller (left side)
      return bitRead(buttons_pressed, 10);
    case 'L':   // left controller or panel
      return bitRead(buttons_pressed, 11) or bitRead(buttons_pressed, 14);
  }
}

char whichButton(){
  for(i=0; i<14; i++){
    if(bitRead(buttons_pressed << i, 15)) return button_symbols[i];
  }
  return 'E';
}

void displayDigit(byte num, byte row, byte column){
  row = constrain(row, 0, 27);
  column = constrain(column, 0, 29);
  byte whichbyte_1 = (row*32+column)/8;
  byte whichbyte_2 = (row*32+column+1)/8;
  byte whichbyte_3 = (row*32+column+2)/8;
  byte whichbit_1 = 7 - column%8;
  byte whichbit_2 = 7 - (column+1)%8;
  byte whichbit_3 = 7 - (column+2)%8;
  
  if(num == 0){
    bitSet(cathode[whichbyte_2], whichbit_2);
    bitSet(cathode[whichbyte_2+16], whichbit_2);
    for(i=0; i<5; i++){
      bitSet(cathode[whichbyte_1+4*i], whichbit_1);
      bitSet(cathode[whichbyte_3+4*i], whichbit_3);
    }
  }else if(num == 1){
    for(i=0; i<5; i++){
      bitSet(cathode[whichbyte_1+4*i], whichbit_1);
    }
  }else if(num == 2){
    for(i=0; i<3; i++){
      bitSet(cathode[whichbyte_1+8*i], whichbit_1);
      bitSet(cathode[whichbyte_2+8*i], whichbit_2);
      bitSet(cathode[whichbyte_3+8*i], whichbit_3);
    }
    bitSet(cathode[whichbyte_1+12], whichbit_1);
    bitSet(cathode[whichbyte_3+4], whichbit_3);
  }else if(num == 3){
    for(i=0; i<3; i++){
      bitSet(cathode[whichbyte_1+8*i], whichbit_1);
      bitSet(cathode[whichbyte_2+8*i], whichbit_2);
      bitSet(cathode[whichbyte_3+8*i], whichbit_3);
    }
    bitSet(cathode[whichbyte_3+4], whichbit_3);
    bitSet(cathode[whichbyte_3+12], whichbit_3);
  }else if(num == 4){
    bitSet(cathode[whichbyte_2+8], whichbit_2);
    for(i=0; i<5; i++){
      bitSet(cathode[whichbyte_3+4*i], whichbit_3);
      if(i<3) bitSet(cathode[whichbyte_1+4*i], whichbit_1);
    }
  }else if(num == 5){
    for(i=0; i<3; i++){
      bitSet(cathode[whichbyte_1+8*i], whichbit_1);
      bitSet(cathode[whichbyte_2+8*i], whichbit_2);
      bitSet(cathode[whichbyte_3+8*i], whichbit_3);
    }
    bitSet(cathode[whichbyte_1+4], whichbit_1);
    bitSet(cathode[whichbyte_3+12], whichbit_3);
  }else if(num == 6){
    for(i=0; i<3; i++){
      bitSet(cathode[whichbyte_1+8*i], whichbit_1);
      bitSet(cathode[whichbyte_2+8*i], whichbit_2);
      bitSet(cathode[whichbyte_3+8*i], whichbit_3);
    }
    bitSet(cathode[whichbyte_1+4], whichbit_1);
    bitSet(cathode[whichbyte_3+12], whichbit_3);
    bitSet(cathode[whichbyte_1+12], whichbit_1);
  }else if(num == 7){
    bitSet(cathode[whichbyte_1], whichbit_1);
    bitSet(cathode[whichbyte_2], whichbit_2);
    bitSet(cathode[whichbyte_3], whichbit_3);
    bitSet(cathode[whichbyte_3+4], whichbit_3);
    bitSet(cathode[whichbyte_2+8], whichbit_2);
    bitSet(cathode[whichbyte_1+12], whichbit_1);
    bitSet(cathode[whichbyte_1+16], whichbit_1);
  }else if(num == 8){
    for(i=0; i<5; i++){
      bitSet(cathode[whichbyte_1+4*i], whichbit_1);
      bitSet(cathode[whichbyte_3+4*i], whichbit_3);
      if(i%2 == 0 || i == 0) bitSet(cathode[whichbyte_2+4*i], whichbit_2);
    }
  }else if(num == 9){
    for(i=0; i<3; i++){
      bitSet(cathode[whichbyte_1+8*i], whichbit_1);
      bitSet(cathode[whichbyte_2+8*i], whichbit_2);
      bitSet(cathode[whichbyte_3+8*i], whichbit_3);
    }
    bitSet(cathode[whichbyte_1+4], whichbit_1);
    bitSet(cathode[whichbyte_3+4], whichbit_3);
    bitSet(cathode[whichbyte_3+12], whichbit_3);
  }else if(num==10){
    for(i=0; i<5; i++){
      bitClear(cathode[whichbyte_1+4*i], whichbit_1);
      bitClear(cathode[whichbyte_2+4*i], whichbit_2);
      bitClear(cathode[whichbyte_3+4*i], whichbit_3);
    }
  }
}

void displayNumber(int num, byte row, byte column){
  byte current_column = column;
  String num_str = String(num);
  for(j=0; j<num_str.length(); j++){
    displayDigit(num_str.charAt(j) - '0', row, current_column);
    if(num_str.charAt(j) == '1') current_column += 2;
    else current_column += 4;
  }
}

void countdown(byte row, byte column){
  for(m=15; m>=0; m--){
    displayDigit(10, row, column);
    displayDigit(m/4, row, column+(m/4==1));
    buttonRead();
      switch(whichButton()){
        case 'O':
          play = false;
          m = -1;
          break;
        case 'T':
          pauseScreen();
          break;
      }
    delay(250);
  }
  displayDigit(10, row, column);
}

void showError(){
  for(i=0; i<11; i++){
    for(j=0; j<11; j++){
      if((j>0 and j<10 and (i==1 or i==9)) or (i>0 and i<10 and (j==1 or j==9)) or ((i==j or i==10-j) and j>2 and j<8)) LED(i+21, j+21, 1);
      else LED(i+21, j+21, 0);
    }
  }
}

void pauseScreen(){
  memcpy(cathode_past, cathode, sizeof(cathode));
  for(i=33; i<92; i+=4){
    cathode[i] = 0;
    cathode[i+1] = 0;
    if(i>40 and i<83){
      cathode[i] = highByte(play_logo[(i-41)/4]);
      cathode[i+1] = lowByte(play_logo[(i-41)/4]);
    }
  }
  delay(500);
  buttonRead();
  while(whichButton() == 'E'){
    buttonRead();
  }
  memcpy(cathode, cathode_past, sizeof(cathode));
  delay(500);
}

//////////////////////////////////////│¯¯¯¯¯¯¯¯¯¯¯│//////////////////////////////////////
//////////////////////////////////////│ Interrupt │//////////////////////////////////////
//////////////////////////////////////│___________│//////////////////////////////////////

ISR(TIMER1_COMPA_vect){
    PORTD |= 1<<blank_pin;    //turn all the LEDs off

    SPCR |= _BV(DORD);    //set bitorder to LSBFIRST

    for(shift_out=anodelevel*4+64; shift_out<anodelevel*4+68; shift_out++){
        SPI.transfer(cathode[shift_out]);
    }

    SPCR &= ~(_BV(DORD));   //set bitorder to MSBFIRST

    for(shift_out=anodelevel*4+3; shift_out>=anodelevel*4; shift_out--){
        SPI.transfer(cathode[shift_out]);
    }

    if(anodelevel<8){
        SPI.transfer(0);
        SPI.transfer(anode[anodelevel]);
    }else{
        SPI.transfer(anode[anodelevel-8]);
        SPI.transfer(0);
    }

    PORTB |= 1<<0;      //Latch pin HIGH
    PORTB &= 0<<0;      //Latch pin LOW
    PORTD &= 0<<blank_pin;      //Blank pin LOW to turn on the LEDs with the new data

    anodelevel ++;
    if(anodelevel == 16) anodelevel = 0;
}

/////////////////////////////////////│¯¯¯¯¯¯¯¯¯¯¯¯¯¯│/////////////////////////////////////
/////////////////////////////////////│ Applications │/////////////////////////////////////
/////////////////////////////////////│______________│/////////////////////////////////////

void game_of_life(){
  while(play){
    for(i=0; i<128; i++){
      cathode[i] = lowByte(random(256));
    }
    memcpy(cathode_past, cathode, sizeof(cathode));
    game_over = false;
    while(play and !game_over){
      num_empty = 0;
      for(m=0; m<32; m++){
        for(n=0; n<32; n++){
          num_neigh = number_neighbours(m, n);
          if(bitRead(cathode[(m*32+n)/8], 7-n%8)){
            if(num_neigh == 2 || num_neigh == 3) bitSet(cathode_next[(m*32+n)/8], 7-n%8);
          }else{
            if(num_neigh == 3) bitSet(cathode_next[(m*32+n)/8], 7-n%8);
            num_empty ++;
          }
        }
      }
      if(num_empty == 1024 || isEqual(cathode, cathode_next) || isEqual(cathode_next, cathode_past)) break;
      memcpy(cathode_past, cathode, sizeof(cathode));
      memcpy(cathode, cathode_next, sizeof(cathode));
      memset(cathode_next, 0, sizeof(cathode_next));
  
      buttonRead();
      switch(whichButton()){
        case 'O':
          play = false;
          break;
        case 'S':
          game_over = true;
          break;
      }
      
      delay(pot_value/3);
    }
  }
}

byte number_neighbours(int row, int column){
  byte output = 0;
  if(column < 31) output += bitRead(cathode[((row)*32+(column+1))/8], 7-(column+1)%8);
  if(column > 0) output += bitRead(cathode[((row)*32+(column-1))/8], 7-(column-1)%8);
  if(row < 31) output += bitRead(cathode[((row+1)*32+(column))/8], 7-(column)%8);
  if(row < 31 && column < 31) output += bitRead(cathode[((row+1)*32+(column+1))/8], 7-(column+1)%8);
  if(row < 31 && column > 0) output += bitRead(cathode[((row+1)*32+(column-1))/8], 7-(column-1)%8);
  if(row > 0) output += bitRead(cathode[((row-1)*32+(column))/8], 7-(column)%8);
  if(row > 0 && column < 31) output += bitRead(cathode[((row-1)*32+(column+1))/8], 7-(column+1)%8);
  if(row > 0 && column > 0) output += bitRead(cathode[((row-1)*32+(column-1))/8], 7-(column-1)%8);
  return output;
}

void snake(){
  displayFile(snake_ps);
  delay(500);
  snake_length = 2;
  score = 0;
  head[0] = 10;    // starting row
  head[1] = 5;    // starting column
  body[0] = 10;   // set bits 0-3 to 1
  dir = 2;
  
  food[0] = random(8, 30);
  food[1] = random(2, 26);
  
  make_snake();
  LED(food[0], food[1], 1);
  displayDigit(0, 1, 1);

  countdown(1, 20);

  while(play and !game_over){
    buttonRead();
    
    for(m=0; m<4; m++){
      if(bitRead(buttons_pressed, 11-m) and m != dir+2 and dir != m+2){
        dir = m;
        break;
      }
    }

    snake_step();
    make_snake();
    if(head[0] == food[0] and head[1] == food[1]){
      snake_length ++;
      score += 1+4*(pot_value<5);
      food[0] = random(8, 30);
      food[1] = random(2, 26);
      LED(food[0], food[1], 1);
      food_onoff = 8;
      for(m=4; m<24; m++) if((m+1)%4 != 0) cathode[m] = 0;
      displayNumber(score, 1, 1);
    }

    food_onoff ++;
    LED(food[0], food[1], bitRead(food_onoff, 3));
    
    if(head[0] < 8 or head[0] > 29 or head[1] < 2 or head[1] > 25) game_over = true;
    buttonRead();
    switch(whichButton()){
      case 'O':
        play = false;
        break;
      case 'S':
        game_over = true;
        break;
      case 'T':
        pauseScreen();
        break;
    }
    if(!controllerCheck()){
      play = false;
      game_over = false;
    }
    delay(pot_value/2+20);
  }
  if(game_over){
    for(n=0; n<4; n++){
      for(m=4; m<24; m++) if((m+1)%4 != 0) cathode[m] = 0;
      delay(250);
      displayNumber(score, 1, 1);
      delay(250);
      buttonRead();
      if(buttonCheck('O')){
        play = false;
        break;
      }
    }
    EEPROM.get(0, high_score);
    if(score > high_score) EEPROM.put(0, score);
    game_over = false;
    if(play) snake();
  }
}

void make_snake(){
  byte current_dir;
  byte current_pos[2] = {head[0], head[1]};
  for(m=0; m<snake_length; m++){
    LED(current_pos[0], current_pos[1], 1);
    if(head[0] == current_pos[0] and head[1] == current_pos[1] and m>0) game_over = true;
    current_dir = bitRead(body[m/4], m%4*2) + bitRead(body[m/4], m%4*2+1)*2;
    if(current_dir == 0) current_pos[1] --;
    if(current_dir == 1) current_pos[0] ++;
    if(current_dir == 2) current_pos[1] ++;
    if(current_dir == 3) current_pos[0] --;
  }
  LED(current_pos[0], current_pos[1], 0);
}

void snake_step(){
  for(m=(snake_length-1)/4; m>-1; m--){
    if(m < 131){
      bitWrite(body[m+1], 0, bitRead(body[m], 6));
      bitWrite(body[m+1], 1, bitRead(body[m], 7));
    }
    body[m] = body[m] << 2;
  }
  bitWrite(body[0], 0, bitRead((dir+2)%4, 0));
  bitWrite(body[0], 1, bitRead((dir+2)%4, 1));
  if(dir == 0) head[1] --;
  if(dir == 1) head[0] ++;
  if(dir == 2) head[1] ++;
  if(dir == 3) head[0] --;
}

void pong(){
  scores[0] = 0; scores[1] = 0;
  while(play){
    clearScreen();
    for(m=0; m<4; m++){ // draw top and bottom walls
      cathode[m+28] = 255;
      cathode[m+124] = 255;
    }
    pong_draw();
    displayDigit(10, 1, 1);
    displayDigit(10, 1, 28);
    displayDigit(scores[0], 1, 1);
    displayDigit(scores[1], 1, 28);
    delay(1000);
    countdown(1, 15);
    angle = (float)random(26)/22.0;
    forward = random(2);
    upward = random(2);
    
    ball[0] = 15.0; ball[1] = 19.0;
    ball_prev[0] = 15.0; ball_prev[1] = 19.0;
    ball_byte[0] = 15; ball_byte[1] = 19;
    
    while(ball_byte[0] > 0 && ball_byte[0] < 31 && play){
      buttonRead();
      switch(whichButton()){
        case 'O':
          play = false;
          break;
        case 'T':
          pauseScreen();
          break;
      }
      if(!controllerCheck()){
        play = false;
      }
      if(buttonCheck('U')) paddles[0] = constrain(paddles[0]-1, 8, 26);
      else if(buttonCheck('D')) paddles[0] = constrain(paddles[0]+1, 8, 26);
      if(buttonCheck('u')) paddles[1] = constrain(paddles[1]-1, 8, 26);
      else if(buttonCheck('d')) paddles[1] = constrain(paddles[1]+1, 8, 26);
      
      pong_draw();
      ball_prev[0] = ball[0]; ball_prev[1] = ball[1];
      pong_next_ball_pos();   // calculate next ball position (without looking at walls or paddles)
      ball_byte[0] = (byte)(ball[0]+0.5); ball_byte[1] = (byte)(ball[1]+0.5);
      
      if(ball_byte[1] < 8 || ball_byte[1] > 30) upward = 1-upward;  // check if ball hit top or bottom walls
      if((ball_byte[0] < 2 && ball_byte[1] >= paddles[0] && ball_byte[1] < paddles[0]+5) || (ball_byte[0] > 29 && ball_byte[1] >= paddles[1] && ball_byte[1] < paddles[1]+5)){  // check if ball hit paddles
        forward = 1-forward;
        angle = (float)random(8, 26)/22.0;
      }
      pong_next_ball_pos();   // calculate next ball position (including walls and/or paddles)
      
      delay(pot_value/2+20);
    }
    if(ball_byte[0] < 1) scores[1]++;
    else if(ball_byte[0] > 30) scores[0]++;

    ball[0] = 15.0; ball[1] = 19.0;
    paddles[0] = 17; paddles[1] = 17;
    pong_draw();

    if(scores[0] == 3 || scores[1] == 3){
      for(n=0; n<4; n++){   // blink winning score
        if(scores[0] == 3) displayDigit(10, 1, 1);
        else displayDigit(10, 1, 28);
        delay(250);
        if(scores[0] == 3) displayDigit(3, 1, 1);
        else displayDigit(3, 1, 28);
        delay(250);
        buttonRead();
        if(buttonCheck('O')){
          play = false;
          break;
        }
      }
      if(play) pong();
    }
  }
}

void pong_draw(){
  LED((byte)(ball_prev[1]+0.5), (byte)(ball_prev[0]+0.5), false);   // clear previous ball
  for(m=8; m<31; m++){  // clear first and last two columns + draw middle line
    bitClear(cathode[m*4], 6);
    bitClear(cathode[m*4], 7);
    bitClear(cathode[m*4+3], 1);
    bitClear(cathode[m*4+3], 0);
    if(m%4 == 0) bitSet(cathode[m*4+5+(m%8)/4], (m%8)/4*7);   // draw next ball
  }
  LED((byte)(ball[1]+0.5), (byte)(ball[0]+0.5), true);
  for(m=0; m<5; m++){   // draw paddles
    bitSet(cathode[(paddles[0]+m)*4], 6);
    bitSet(cathode[(paddles[1]+m)*4+3], 1);
  }
  
}

void pong_next_ball_pos(){
  ball[0] = ball_prev[0]+0.5*cos(angle)*(forward*2-1);
  ball[1] = ball_prev[1]+0.5*sin(angle)*(upward*2-1);
}
