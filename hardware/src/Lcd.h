#pragma once
#include <LiquidCrystal_I2C.h>
#include <vector>
#include <Arduino.h>
#include <pthread.h>

#define LINE_LENGTH 16
#define LINES_PER_SCREEN 2
#define SCROLL_DELAY 3000

typedef struct {
    char line1[LINE_LENGTH + 1];
    char line2[LINE_LENGTH + 1];
} LCDScreen;

class Lcd {
    public:
        Lcd(int address, int columns, int rows);
        ~Lcd();

        void init();

        void write_old(const char *message, int scrollRepeat = 1);

        void write(const char *message, int scrollRepeat = 1);

        void turnOff();
        void displayScreen(const LCDScreen& screen);
        void formatForLcd(const char *message, int *screen_count, LCDScreen (*screens)[50]);

    private:
        LiquidCrystal_I2C _lcd;
        pthread_mutex_t _lcd_mutex = PTHREAD_MUTEX_INITIALIZER;
        
        void _add_line(char *dest, const char *src, int *pos, int text_len);
        
        LCDScreen* _format_for_lcd_old(const char *text, int *screen_count);
        void _format_for_lcd(const char *message, int *screen_count, LCDScreen (*screens)[50]);

        
};

