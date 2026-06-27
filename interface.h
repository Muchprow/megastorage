#ifndef INTERFACE_H
#define INTERFACE_H

#include <windows.h>

void gotoxy(int x, int y);
void set_color(int text, int bg);
void draw_window(int x, int y, int w, int h, const char *title);
void draw_button(int x, int y, int w, const char *text, int active);
int get_mouse_click(int *click_x, int *click_y);

#endif
