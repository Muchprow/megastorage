#include "interface.h"
#include <stdio.h>
#include <stdlib.h>

void gotoxy(int x, int y) {
    COORD coord = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void set_color(int text, int bg) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), text | (bg << 4));
}

void draw_window(int x, int y, int w, int h, const char *title) {
    gotoxy(x, y); printf("┌");
    for(int i=0; i<w-2; i++) printf("─");
    printf("┐");

    for(int i=1; i<h-1; i++) {
        gotoxy(x, y+i); printf("│");
        gotoxy(x+w-1, y+i); printf("│");
    }

    gotoxy(x, y+h-1); printf("└");
    for(int i=0; i<w-2; i++) printf("─");
    printf("┘");

    if (title) {
        gotoxy(x + (w - (int)strlen(title)) / 2, y);
        set_color(14, 1);
        printf(" %s ", title);
    }
}

void draw_button(int x, int y, int w, const char *text, int active) {
    gotoxy(x, y);
    if (active) set_color(0, 7);
    else set_color(15, 8);

    printf(" [");
    int padding = (w - 4 - (int)strlen(text)) / 2;
    for(int i=0; i<padding; i++) printf(" ");
    printf("%s", text);
    for(int i=0; i<(w - 4 - (int)strlen(text) - padding); i++) printf(" ");
    printf("] ");
}

int get_mouse_click(int *click_x, int *click_y) {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD numRead;
    INPUT_RECORD inputRec;

    SetConsoleMode(hInput, ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

    while (ReadConsoleInput(hInput, &inputRec, 1, &numRead)) {
        if (inputRec.EventType == MOUSE_EVENT) {
            MOUSE_EVENT_RECORD mer = inputRec.Event.MouseEvent;
            if (mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) {
                *click_x = mer.dwMousePosition.X;
                *click_y = mer.dwMousePosition.Y;
                return 1;
            }
        }
    }
    return 0;
}
