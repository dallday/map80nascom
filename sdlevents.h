/*
 * definitions for the sdlevents.c
 *
 */

typedef enum { CONT = 0, RESET = 1, WARMRESET=2, DONE = -1 } sim_action_t;
extern sim_action_t action;

void ui_serve_input(void);
int sdl_initialise(void);

// handle the keyboard stuff
void outPort0Keyboard(int value);
int inPort0Keyboard();

// end of file



