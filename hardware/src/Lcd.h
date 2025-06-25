#pragma once
#include <LiquidCrystal_I2C.h>
#include <vector>
#include <Arduino.h>

#define LINE_LENGTH 16
#define LINES_PER_SCREEN 2
#define SCROLL_DELAY 3000
#define SCROLL_REPEAT 2

typedef struct {
    char line1[LINE_LENGTH + 1];
    char line2[LINE_LENGTH + 1];
} LCDScreen;

class Lcd {
    public:
        Lcd(int address, int columns, int rows);

        void init();

        void write(const char *message, int scrollRepeat = 1);

        void turnOff();

    private:
        LiquidCrystal_I2C _lcd;
        
        void _add_line(char *dest, const char *src, int *pos, int text_len);
        
        LCDScreen* _format_for_lcd(const char *text, int *screen_count);
};

