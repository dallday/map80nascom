#ifndef STATUS_SCREEN_H
#define STATUS_SCREEN_H

#include <SDL2/SDL.h>
#include <stdint.h>

// --- All configuration constants, prefixed STATUS_ ---

#define STATUS_COLS 48
#define STATUS_ROWS 24

// Native pixel resolution of the whole character display, before it gets
// stretched up to whatever size the window currently is.
#define STATUS_NATIVE_W (STATUS_COLS * STATUS_CHAR_W)   // 384
#define STATUS_NATIVE_H (STATUS_ROWS * STATUS_CHAR_H)   // 384

// Border around the active character-display area, in native pixels.
#define STATUS_BORDER_LEFT   24
#define STATUS_BORDER_RIGHT  24
#define STATUS_BORDER_TOP    8
#define STATUS_BORDER_BOTTOM 8

// Total native texture size including the border.
#define STATUS_TOTAL_W (STATUS_NATIVE_W + STATUS_BORDER_LEFT + STATUS_BORDER_RIGHT)   // 432
#define STATUS_TOTAL_H (STATUS_NATIVE_H + STATUS_BORDER_TOP + STATUS_BORDER_BOTTOM)   // 400

// Default pixel scale factor for status_screen's starting size. This is
// just the initial value of a runtime variable (see pixel_scale in main),
// so it can be changed programmatically rather than being a fixed literal.
//#define STATUS_PIXEL_SCALE_DEFAULT 1

#define STATUS_COLOR_WHITE  0xFFFFFFFFu
#define STATUS_COLOR_BLACK  0xFF000000u


// Some handy ARGB8888 colors (0xAARRGGBB) for character foregrounds/backgrounds.
#define STATUS_COLOR_RED     0xFFCC2222u
#define STATUS_COLOR_GREEN   0xFF229933u
#define STATUS_COLOR_BLUE    0xFF2244CCu
#define STATUS_COLOR_MAGENTA 0xFFAA22AAu
#define STATUS_COLOR_ORANGE  0xFFDD8800u
#define STATUS_COLOR_LIGHTBLUE    0xFF8080FF
#define STATUS_COLOR_YELLOW  0xFFC8C800

#define STATUS_COLOR_BACKGROUND 0xFFE0E0E0
#define STATUS_COLOR_BORDER     0xFFE0E0E0


// --- Types ---

typedef unsigned char StatusScreenChars[STATUS_ROWS][STATUS_COLS];
typedef uint32_t StatusScreenColors[STATUS_ROWS][STATUS_COLS];

// Everything needed to own and drive status_screen: the window itself,
// its renderer, its pixel-buffer texture, and its window ID (used to
// filter events so only this window's input is ever acted on).
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    Uint32 window_id;
} StatusScreenContext;

// Live cursor/mouse state for status_screen, updated by status_handle_events.
typedef struct {
    int in_screen;   // 1 while the cursor is inside status_screen, else 0
    int last_x, last_y;      // last known cursor position in window pixels
    int hover_col, hover_row; // last known hovered character cell, or -1/-1
} StatusCursorState;

// --- Global state ---
// status_screen only ever needs one instance of each of these, so they're
// globals rather than being threaded through every function call.

extern StatusScreenContext status_ctx;
extern StatusScreenChars status_screen_chars;      // character at each cell
extern StatusScreenColors status_screen_fg_colors; // foreground color at each cell
extern StatusScreenColors status_screen_bg_colors; // background color at each cell
extern StatusCursorState status_cursor;
extern uint8_t* status_font;
extern uint8_t STATUS_CHAR_W;
extern uint8_t STATUS_CHAR_H;
extern uint8_t STATUS_FONT_BYTES_CHAR;



// --- The NASCOM 2 font: 256 characters, 16 bytes each. ---
// Each byte is one row of 8 pixels (bit 7 = leftmost pixel, bit 0 = rightmost).
// Defined in font.c.
extern uint8_t nascom_font_raw[];
// VFC font definition
extern uint8_t map80VFCcharRom1[];


// --- Function declarations (defined in sdl_window.c) ---

// Creates status_screen (window, renderer, texture) and initializes the
// global screen buffers to blank/white. Returns 0 on success, non-zero
// on failure (an error is printed to stderr).
int status_create_screen(int pixel_scale);

// Destroys everything created by status_create_screen.
void status_destroy_screen(void);

// hndle 1 event for status window
void status_handle_events(SDL_Event event);

// Writes one character's glyph directly into an ARGB8888 pixel buffer at
// native resolution. Coordinates are in "active area" cells (0..COLS-1,
// 0..ROWS-1); the border offset is added internally. Paints bg_color into
// the whole 8x16 cell first, then fg_color wherever the glyph has a set bit.
void status_draw_char_argb(uint32_t *pixels, int pitch_pixels, int col, int row,
                            unsigned char c, uint32_t fg_color, uint32_t bg_color);

// Show a single character at (col, row) with the given foreground and
// background colors. Silently clips if col/row is out of bounds.
void status_show_char(unsigned char ch, int col, int row, uint32_t fg_color, uint32_t bg_color);

// Show a null-terminated string starting at (col, row), all one foreground
// and background color. Stops at the end of the row rather than wrapping.
void status_show_string(const char *str, int col, int row, uint32_t fg_color, uint32_t bg_color);

// Show an array of characters starting at (col, row). fg_colors is a
// parallel array (one color per character); pass fg_colors=NULL to use
// default_fg_color for all of them instead. bg_color applies to every
// character in the array.
void status_show_chars(const unsigned char *chars, int count, int col, int row,
                        const uint32_t *fg_colors, uint32_t default_fg_color, uint32_t bg_color);

// Converts a mouse position in window pixels to an (col, row) cell in the
// active character area, accounting for the current window scale and the
// border offset. Returns 1 and fills *col/*row if the point is inside the
// active area, or returns 0 (cursor is over the border) otherwise.
int status_window_pos_to_cell(int win_w, int win_h, int px, int py, int *col, int *row);

// Builds and presents one frame of status_screen (the character display),
// using the global screen buffers and cursor state.
void status_refresh_screen(void);

// Called whenever a mouse button is pressed while status_screen has focus.
// Reports which character cell (if any) was clicked, and which button.
void status_on_mouse_button_down(int x, int y, Uint8 button);

extern int  statusdisplayxpos;
extern int  statusdisplayypos;
extern StatusScreenContext status_ctx;
extern StatusCursorState status_cursor;

void status_display_position(int x, int y);

// get the current size of the status window on the screen
// updates the integers pointed to by w and h
void status_GetWindowSize(int* w, int* h);

void status_display_change_size(int sizefactor);


void status_display_show_char_full(char ch, unsigned int col, unsigned int row, 
          uint32_t charcolour, uint32_t bg_colour);

void status_display_show_chars_full(char * stringdata, unsigned int col, unsigned int row, 
          uint32_t charcolour, uint32_t bg_colour);
          
void status_display_show_chars(const char *str, int col, int row);

void status_display_show_chars_full(char * stringdata, unsigned int col , unsigned int row, 
        uint32_t charcolour, uint32_t bg_colour);
        
void status_set_char(unsigned char ch, int col, int row,  uint32_t fg_color, uint32_t bg_color);


void status_display_change_size(int sizefactor);
void status_display_position(int x, int y);
void status_get_display_position(int* x, int* y);
// get the current size of the status window on the screen
// updates the integers pointed to by w and h
void status_GetWindowSize(int* w, int* h);


#endif // STATUS_SCREEN_H
