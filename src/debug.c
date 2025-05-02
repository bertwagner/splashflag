#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_LENGTH 16
#define LINES_PER_SCREEN 2

typedef struct {
    char line1[LINE_LENGTH + 1];
    char line2[LINE_LENGTH + 1];
} LCDScreen;

// Helper to copy a line without splitting words unless too long
void add_line(char *dest, const char *src, int *pos, int text_len) {
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

LCDScreen* format_for_lcd(const char *text, int *screen_count) {
    int text_len = strlen(text);
    int pos = 0;
    int capacity = 4;
    int screen_idx = 0;

    LCDScreen *screens = malloc(capacity * sizeof(LCDScreen));
    if (!screens) return NULL;

    while (pos < text_len) {
        if (screen_idx >= capacity) {
            capacity *= 2;
            screens = realloc(screens, capacity * sizeof(LCDScreen));
            if (!screens) return NULL;
        }

        add_line(screens[screen_idx].line1, text, &pos, text_len);
        add_line(screens[screen_idx].line2, text, &pos, text_len);
        screen_idx++;
    }

    *screen_count = screen_idx;
    return screens;
}

int main() {
    const char *text = "This is a long sentence that must be split into multiple screens without breaking words.";
    int screen_count;
    LCDScreen *screens = format_for_lcd(text, &screen_count);

    for (int i = 0; i < screen_count; i++) {
        printf("Screen %d:\n", i + 1);
        printf("Line 1: '%s'\n", screens[i].line1);
        printf("Line 2: '%s'\n", screens[i].line2);
    }

    free(screens);
    return 0;
}