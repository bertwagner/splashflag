#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "Lcd.h"

Lcd::Lcd(int address, int columns, int rows) : _lcd(address, columns, rows) //constructor initialization list
{       
}

Lcd::~Lcd() {
    pthread_mutex_destroy(&_lcd_mutex);
}

void Lcd::init()
{
    pthread_mutex_init(&_lcd_mutex, NULL);
    
    pthread_mutex_lock(&_lcd_mutex);
    _lcd.init();
    _lcd.clear();
    pthread_mutex_unlock(&_lcd_mutex);        
}

void Lcd::displayScreen(const LCDScreen& screen) {
    pthread_mutex_lock(&_lcd_mutex);
    _lcd.backlight();
    _lcd.clear();
    _lcd.setCursor(0,0);
    _lcd.print(screen.line1);
    _lcd.setCursor(0,1);
    _lcd.print(screen.line2);
    pthread_mutex_unlock(&_lcd_mutex);
}

void Lcd::formatForLcd(const char *message, int *screen_count, LCDScreen (*screens)[50]) {
    _format_for_lcd(message, screen_count, screens);
}

void Lcd::write(const char *message, int scrollRepeat)
{
    pthread_mutex_lock(&_lcd_mutex);
    _lcd.backlight(); 

    int screen_count;
    LCDScreen screens[50];
    //Serial.printf("Preparing to display messag: %s\n", message);
    _format_for_lcd(message, &screen_count, &screens);

    //Serial.printf("Screen count: %d\n", screen_count);
    //Serial.printf("screens0: %s\n", screens[0].line1);

    for (int s = 0; s < scrollRepeat; s++) {
        for (int i = 0; i < screen_count; i++) {
        _lcd.clear(); 

        _lcd.setCursor(0,0);
        _lcd.print(screens[i].line1);

        _lcd.setCursor(0,1);
        _lcd.print(screens[i].line2);
        
        delay(SCROLL_DELAY);
        }
    }
    pthread_mutex_unlock(&_lcd_mutex);

}

void Lcd::_format_for_lcd(const char *message, int *screen_count, LCDScreen (*screens)[50]) {
    int text_len = strlen(message);
    int pos = 0;
    int screen_idx = 0;

    while (pos < text_len) {

      _add_line((*screens)[screen_idx].line1, message, &pos, text_len);
      _add_line((*screens)[screen_idx].line2, message, &pos, text_len);
      screen_idx++;
  }

  *screen_count = screen_idx;

}

void Lcd::write_old(const char *message, int scrollRepeat) 
{
    _lcd.backlight(); 

    int screen_count;
    LCDScreen *screens = _format_for_lcd_old(message, &screen_count);

    for (int s = 0; s < scrollRepeat; s++) {
        for (int i = 0; i < screen_count; i++) {
        _lcd.clear(); 

        _lcd.setCursor(0,0);
        _lcd.print(screens[i].line1);

        _lcd.setCursor(0,1);
        _lcd.print(screens[i].line2);
        
        delay(SCROLL_DELAY);
        }
    }

    free(screens);

    // _lcd.clear();
    //_lcd.noBacklight();
}

void Lcd::turnOff()
{
    pthread_mutex_lock(&_lcd_mutex);
    _lcd.clear();
    _lcd.noBacklight();
    pthread_mutex_unlock(&_lcd_mutex);
}

void Lcd::_add_line(char *dest, const char *src, int *pos, int text_len) {
  dest[0] = '\0';
  int line_len = 0;

  while (*pos < text_len) {
      // Skip any leading spaces
      while (*pos < text_len && isspace(src[*pos])) (*pos)++;

      int word_start = *pos;
      while (*pos < text_len && !isspace(src[*pos])) (*pos)++;
      int word_len = *pos - word_start;

      if (word_len == 0)
          break;

      // Word longer than LINE_LENGTH, needs to be split
      if (word_len > LINE_LENGTH) {
          int available = LINE_LENGTH - line_len;
          if (available <= 0)
              break;

          strncat(dest, src + word_start, available);
          line_len += available;
          // rewind to unsplit rest of the word
          *pos = word_start + available;
          break;
      }

      // Check if word + space fits
      int extra = (line_len > 0 ? 1 : 0) + word_len;
      if (line_len + extra > LINE_LENGTH) {
          // rewind to start of word
          *pos = word_start;
          break;
      }

      // Add space if not first word
      if (line_len > 0) {
          strcat(dest, " ");
          line_len++;
      }

      strncat(dest, src + word_start, word_len);
      line_len += word_len;
  }
}

LCDScreen* Lcd::_format_for_lcd_old(const char *text, int *screen_count) {
  int text_len = strlen(text);
  int pos = 0;
  int capacity = 4;
  int screen_idx = 0;

  LCDScreen *screens = (LCDScreen *)malloc(capacity * sizeof(LCDScreen));
  if (!screens) return NULL;

  while (pos < text_len) {
      if (screen_idx >= capacity) {
          capacity *= 2;
          screens = (LCDScreen *)realloc(screens, capacity * sizeof(LCDScreen));
          if (!screens) return NULL;
      }

      _add_line(screens[screen_idx].line1, text, &pos, text_len);
      _add_line(screens[screen_idx].line2, text, &pos, text_len);
      screen_idx++;
  }

  *screen_count = screen_idx;
  return screens;
}


