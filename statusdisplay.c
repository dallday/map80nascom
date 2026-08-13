#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "options.h"
#include "statusdisplay.h"

// --- Global state definitions ---

StatusScreenContext status_ctx;
StatusScreenChars status_screen_chars;
StatusScreenColors status_screen_fg_colors;
StatusScreenColors status_screen_bg_colors;
StatusCursorState status_cursor;

// change so we can mofify the font being used :) 
//uint8_t* status_font = nascom_font_raw;
//uint8_t STATUS_CHAR_W=8;
//uint8_t STATUS_CHAR_H=16;
//uint8_t STATUS_FONT_BYTES_CHAR=16;
uint8_t* status_font = map80VFCcharRom1;
uint8_t STATUS_CHAR_W=8;
uint8_t STATUS_CHAR_H=10;
uint8_t STATUS_FONT_BYTES_CHAR=16;


// --- status_ function implementations ---

int status_create_screen(int pixel_scale) {
    status_ctx.window = SDL_CreateWindow(
        "Map80Nascom status screen",
        STATUS_DISPLAY_XPOS,
        STATUS_DISPLAY_YPOS,
        STATUS_TOTAL_W * pixel_scale, STATUS_TOTAL_H * pixel_scale,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!status_ctx.window) {
        fprintf(stderr, "SDL_CreateWindow status screen failed: %s\n", SDL_GetError());
        return 1;
    }

    status_ctx.renderer = SDL_CreateRenderer(status_ctx.window, -1, SDL_RENDERER_ACCELERATED);
    if (!status_ctx.renderer) {
        fprintf(stderr, "SDL_CreateRenderer status screen failed: %s\n", SDL_GetError());
        return 1;
    }

    // The texture we draw the character display into at native resolution;
    // SDL stretches it to the window size when we copy it to the renderer.
    status_ctx.texture = SDL_CreateTexture(
        status_ctx.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        STATUS_TOTAL_W, STATUS_TOTAL_H
    );
    if (!status_ctx.texture) {
        fprintf(stderr, "SDL_CreateTexture status screen failed: %s\n", SDL_GetError());
        return 1;
    }
    // Use nearest-neighbor scaling to keep the pixel-art look crisp.
    SDL_SetTextureScaleMode(status_ctx.texture, SDL_ScaleModeNearest);

    status_ctx.window_id = SDL_GetWindowID(status_ctx.window);

    SDL_ShowCursor(SDL_ENABLE);
 //   SDL_SetWindowGrab(status_ctx.window, SDL_TRUE);

    // Initialize the screen buffers: all spaces, STATUS_COLOR_BACKGROUND, black foreground.
    memset(status_screen_chars, ' ', sizeof(status_screen_chars));
    for (int r = 0; r < STATUS_ROWS; r++) {
        for (int c = 0; c < STATUS_COLS; c++) {
            status_screen_fg_colors[r][c] = STATUS_COLOR_BLACK;
            status_screen_bg_colors[r][c] = STATUS_COLOR_BACKGROUND;
        }
    }

    status_cursor.in_screen = 0;
    status_cursor.last_x = 0;
    status_cursor.last_y = 0;
    status_cursor.hover_col = -1;
    status_cursor.hover_row = -1;

    return 0;
}

void status_destroy_screen(void) {
    if (status_ctx.texture) SDL_DestroyTexture(status_ctx.texture);
    if (status_ctx.renderer) SDL_DestroyRenderer(status_ctx.renderer);
    if (status_ctx.window) SDL_DestroyWindow(status_ctx.window);
}

void status_handle_events(SDL_Event event) {
    //SDL_Event event;
//    char title[128];

    //while (SDL_PollEvent(&event)) {
 //       if (event.type == SDL_QUIT) *running = 0;
//        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) *running = 0;

        if (event.type == SDL_WINDOWEVENT && event.window.windowID == status_ctx.window_id) {
            if (event.window.event == SDL_WINDOWEVENT_ENTER) status_cursor.in_screen = 1;
            else if (event.window.event == SDL_WINDOWEVENT_LEAVE) status_cursor.in_screen = 0;
        }

        if (event.type == SDL_MOUSEMOTION && event.motion.windowID == status_ctx.window_id) {
            status_cursor.last_x = event.motion.x;
            status_cursor.last_y = event.motion.y;

            int w, h;
            SDL_GetWindowSize(status_ctx.window, &w, &h);
            int in_active = status_window_pos_to_cell(w, h, status_cursor.last_x, status_cursor.last_y,
                                                       &status_cursor.hover_col, &status_cursor.hover_row);
            if (!in_active) { status_cursor.hover_col = -1; status_cursor.hover_row = -1; }

//            snprintf(title, sizeof(title),
//                     "status_screen - cursor: (%d, %d) cell: (col %d, row %d)",
//                     status_cursor.last_x, status_cursor.last_y,
//                     status_cursor.hover_col, status_cursor.hover_row);
//            SDL_SetWindowTitle(status_ctx.window, title);

           // printf("Cursor at (%d, %d) -> cell (col %d, row %d)\n",
             //      status_cursor.last_x, status_cursor.last_y,
               //    status_cursor.hover_col, status_cursor.hover_row);
        }

        // Still filtered to status_screen's window ID, even though it's
        // the only window now -- keeps this safe if more windows are ever added.
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.windowID == status_ctx.window_id) {
            status_on_mouse_button_down(event.button.x, event.button.y, event.button.button);
        }
    //}
}

void status_draw_char_argb(uint32_t *pixels, int pitch_pixels, int col, int row,
                            unsigned char c, uint32_t fg_color, uint32_t bg_color) {

// change so we can mofify the font being used :) 
//    uint8_t* status_font = nascom_font_raw;
    const uint8_t *glyph = &status_font[(int)c * STATUS_FONT_BYTES_CHAR];
    int base_x = STATUS_BORDER_LEFT + col * STATUS_CHAR_W;
    int base_y = STATUS_BORDER_TOP + row * STATUS_CHAR_H;

    for (int y = 0; y < STATUS_CHAR_H; y++) {
        uint8_t bits = glyph[y];
        uint32_t *row_ptr = pixels + (base_y + y) * pitch_pixels + base_x;
        for (int x = 0; x < STATUS_CHAR_W; x++) {
            row_ptr[x] = (bits & (0x80 >> x)) ? fg_color : bg_color;
        }
    }
}

void status_show_char(unsigned char ch, int col, int row, uint32_t fg_color, uint32_t bg_color) {
    if (row < 0 || row >= STATUS_ROWS || col < 0 || col >= STATUS_COLS) return;
    status_screen_chars[row][col] = ch;
    status_screen_fg_colors[row][col] = fg_color;
    status_screen_bg_colors[row][col] = bg_color;
}

void status_show_string(const char *str, int col, int row, uint32_t fg_color, uint32_t bg_color) {
    for (int i = 0; str[i] != '\0' && (col + i) < STATUS_COLS; i++) {
        status_show_char((unsigned char)str[i], col + i, row, fg_color, bg_color);
    }
}

void status_show_chars(const unsigned char *chars, int count, int col, int row,
                        const uint32_t *fg_colors, uint32_t default_fg_color, uint32_t bg_color) {
    for (int i = 0; i < count && (col + i) < STATUS_COLS; i++) {
        uint32_t fg = fg_colors ? fg_colors[i] : default_fg_color;
        status_show_char(chars[i], col + i, row, fg, bg_color);
    }
}

int status_window_pos_to_cell(int win_w, int win_h, int px, int py, int *col, int *row) {
    float scale_x = (float)win_w / STATUS_TOTAL_W;
    float scale_y = (float)win_h / STATUS_TOTAL_H;

    float active_x = px - STATUS_BORDER_LEFT * scale_x;
    float active_y = py - STATUS_BORDER_TOP * scale_y;

    float cell_w = STATUS_CHAR_W * scale_x;
    float cell_h = STATUS_CHAR_H * scale_y;

    if (active_x < 0 || active_y < 0) return 0;

    int c = (int)(active_x / cell_w);
    int r = (int)(active_y / cell_h);
    if (c < 0 || c >= STATUS_COLS || r < 0 || r >= STATUS_ROWS) return 0;

    *col = c;
    *row = r;
    return 1;
}

void status_refresh_screen(void) {
    // --- Build the native-resolution ARGB8888 frame ---
    void *pixels;
    int pitch;
    SDL_LockTexture(status_ctx.texture, NULL, &pixels, &pitch);
    int pitch_pixels = pitch / 4;
    uint32_t *buf = (uint32_t *)pixels;

    // Border color first (the margin around the active character area).
    for (int y = 0; y < STATUS_TOTAL_H; y++) {
        uint32_t *row_ptr = buf + y * pitch_pixels;
        for (int x = 0; x < STATUS_TOTAL_W; x++) row_ptr[x] = STATUS_COLOR_BORDER;
    }

    // Draw every character cell: its own background fills the whole 8x16
    // block, its own foreground draws on top wherever the glyph has a bit
    // set. The cell currently under the cursor gets its foreground and
    // background swapped, as a simple inverted-video highlight.
    for (int r = 0; r < STATUS_ROWS; r++) {
        for (int c = 0; c < STATUS_COLS; c++) {
            uint32_t fg = status_screen_fg_colors[r][c];
            uint32_t bg = status_screen_bg_colors[r][c];

            int is_hovered = status_cursor.in_screen &&
                              r == status_cursor.hover_row && c == status_cursor.hover_col;
            if (is_hovered) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

            status_draw_char_argb(buf, pitch_pixels, c, r, status_screen_chars[r][c], fg, bg);
        }
    }

    SDL_UnlockTexture(status_ctx.texture);


    // --- Render status_screen: stretch the native buffer to fill the
    // window edge-to-edge (no aspect-preserving letterbox/offset). ---
    int win_w, win_h;
    SDL_GetWindowSize(status_ctx.window, &win_w, &win_h);
    SDL_RenderClear(status_ctx.renderer);
    SDL_Rect full_dst = { 0, 0, win_w, win_h };
    SDL_RenderCopy(status_ctx.renderer, status_ctx.texture, NULL, &full_dst);


    // Highlight the grid cell the cursor is currently over.
    // Highlight the grid cell the cursor is currently over.
    //
    // IMPORTANT: each edge of the highlighted cell is computed independently
    // in floating point and rounded only at the very end. Rounding a single
    // shared cell_w/cell_h first and then multiplying by hover_col/hover_row
    // (the previous approach) throws away a fraction of a pixel per cell,
    // and that loss accumulates linearly with the column/row index and with
    // how far the window has been stretched -- which is exactly what caused
    // the highlight to drift left/up as the window got larger or the cursor
    // moved further right/down.
    if (status_cursor.in_screen && status_cursor.hover_col >= 0 && status_cursor.hover_col < STATUS_COLS &&
        status_cursor.hover_row >= 0 && status_cursor.hover_row < STATUS_ROWS) {
        float scale_x = (float)win_w / STATUS_TOTAL_W;
        float scale_y = (float)win_h / STATUS_TOTAL_H;

        float x0 = (STATUS_BORDER_LEFT + status_cursor.hover_col * STATUS_CHAR_W) * scale_x;
        float x1 = (STATUS_BORDER_LEFT + (status_cursor.hover_col + 1) * STATUS_CHAR_W) * scale_x;
        float y0 = (STATUS_BORDER_TOP + status_cursor.hover_row * STATUS_CHAR_H) * scale_y;
        float y1 = (STATUS_BORDER_TOP + (status_cursor.hover_row + 1) * STATUS_CHAR_H) * scale_y;

        int ix0 = (int)(x0 + 0.5f);
        int iy0 = (int)(y0 + 0.5f);
        int ix1 = (int)(x1 + 0.5f);
        int iy1 = (int)(y1 + 0.5f);

        SDL_Rect hl = { ix0, iy0, ix1 - ix0, iy1 - iy0 };
        SDL_SetRenderDrawColor(status_ctx.renderer, 0, 0, 0, 100);
        SDL_SetRenderDrawBlendMode(status_ctx.renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderDrawRect(status_ctx.renderer, &hl);
    }
    SDL_RenderPresent(status_ctx.renderer);
}

void status_on_mouse_button_down(int x, int y, Uint8 button) {
    int w, h;
    SDL_GetWindowSize(status_ctx.window, &w, &h);

    int col, row;
    int in_active = status_window_pos_to_cell(w, h, x, y, &col, &row);

    const char *button_name =
        (button == SDL_BUTTON_LEFT)  ? "left"  :
        (button == SDL_BUTTON_RIGHT) ? "right" :
        (button == SDL_BUTTON_MIDDLE)? "middle": "other";

    if (in_active) {
        printf("Mouse %s button pressed at (%d, %d) -> cell (col %d, row %d)\n",
               button_name, x, y, col, row);
    } else {
        printf("Mouse %s button pressed at (%d, %d) -> on border (no cell)\n",
               button_name, x, y);
    }
}

/*
 * This is the main routine for adding characters to the screen
 * it checks if the position is out of bounds and ignores it if it is.
 */
void status_display_set_char(char ch, unsigned int col, unsigned int row, 
          uint32_t charcolour, uint32_t bg_color){
    if (row < 0 || row >= STATUS_ROWS || col < 0 || col >= STATUS_COLS) return;
    status_screen_chars[row][col] = ch;
    status_screen_fg_colors[row][col] = charcolour;
    status_screen_bg_colors [row][col] = bg_color;
              
}

/*
 * writes the same character from col row for a numberof characters
 */
void status_display_clear(char ch, unsigned int col, unsigned int row, unsigned int numberof,
          uint32_t charcolour, uint32_t bg_color){

    for (int pos1=0;pos1<numberof;pos1++){
        status_display_set_char(ch,col+pos1,row,charcolour,bg_color);
    }
    
}

          
/*
 * display a string on the screen using character position value x col and y row 
 * 
 */

void status_display_show_chars(const char *str, unsigned int col, unsigned int row) {
    for (int i = 0; str[i] != '\0' && (col + i) < STATUS_COLS; i++) {
        status_display_set_char( (unsigned char)str[i], col + i, row,  STATUS_COLOR_BLACK, STATUS_COLOR_BACKGROUND);
    }
}


/*
 * display a string on the screen using character position value x and y 
 *   and allow setting of  colour
 * TODO sort out background color set
 */
void status_display_show_chars_full(char * stringdata, unsigned int col , unsigned int row, 
        uint32_t charcolour, uint32_t bg_color){
                for (int i = 0; stringdata[i] != '\0' && (col + i) < STATUS_COLS; i++) {
        status_display_set_char( (unsigned char)stringdata[i], col + i , row, charcolour, bg_color);
    }
}
            


void status_display_change_size(int sizefactor){

    // since we build the display at 2x actual pixel size 
    // scaling down if actually asking for scaling of 1
    SDL_SetWindowSize(status_ctx.window, STATUS_TOTAL_W*sizefactor*1, STATUS_TOTAL_H*sizefactor*1);
    
}

void status_display_position(int x, int y){

    SDL_SetWindowPosition(status_ctx.window, x, y);

}
void status_get_display_position(int* x, int* y){

    SDL_GetWindowPosition(status_ctx.window, x, y);

}

// get the current size of the status window on the screen
// updates the integers pointed to by w and h
void status_GetWindowSize(int* w, int* h){

    SDL_GetWindowSize(status_ctx.window,w,h);
    
    if (w==NULL){
        *w=STATUS_TOTAL_W;
    }
    if (h==NULL){
        *h=STATUS_TOTAL_H;
    }
}

// end of code