/******************************************************************************
*
* FILENAME: sdl_if.h
*
* DESCRIPTION:  This contains function declartions for SDL interface functions
*
* AUTHORS:  bberlin
*
* CHANGE LOG
*
*  VER     DATE     CHANGED BY   DESCRIPTION OF CHANGE
* ------  --------  ----------   ------------------------------------
* 0.1     07/18/04  bberlin      Creation
* 0.2     03/09/05  bberlin      Added function declaration for debugger and
*                                  GUI video functions
* 0.4.0   05/31/06  bberlin      Added 'pc_toggle_fullscreen' function
******************************************************************************/
#ifndef sdl_if_h
#define sdl_if_h

#include <SDL.h>
#include "atari_ntsc.h"
#include "video.h"
#include "../core/boom6502.h"

#define VID_STYLE

#define MAX_PC_DEVICE 8
#define MAX_PC_PART 32
#define MAX_PC_KEY SDLK_LAST

typedef struct {
	SDL_Surface *screen;
	SDL_Rect screen_rect;
	SDL_Surface *surface;
	SDL_Rect surface_rect;
	SDL_Surface *line;
	SDL_AudioSpec audio_d;
	SDL_AudioSpec audio_o;
	SDL_Color color_palette[256];
	atari_ntsc_t *ntsc_emu;
	unsigned int color_rgb[256];
	void *video_start;
	void *line_pixel_ptr;
} t_sdl_if;

int pc_init (void);
int pc_exit (void);
int pc_game_init (void);
int pc_open_joystick (int index);
int pc_detect_and_open_joysticks (void);
int pc_detect_joysticks (void);
const char * pc_get_joystick_name ( int index );
int pc_get_bytes_per_pixel (void);
UINT8 * pc_get_scanline_ptr (void);
void pc_set_scanline_ptr ( void *ptr );
void * pc_get_current_line_ptr (void);
int pc_poll_events (void);
int pc_poll_debug (void);
void pc_delay (int ms);
int pc_get_ticks (void);
int pc_get_video_scale (void);
int pc_get_modes (int bpp, int flags, SDL_Rect ***p_modes);
int pc_lock_audio (void);
int pc_unlock_audio (void);
int pc_set_palette ( t_atari_video *p_video, SDL_Color *palette, unsigned int *rgb );
SDL_Surface * pc_set_gui_video (void);
SDL_Surface * pc_set_game_video (void);
SDL_Surface * pc_set_debugger_video (void);
SDL_Surface * pc_toggle_fullscreen (void);
SDL_Surface * pc_toggle_gui_fullscreen (SDL_Surface *gui_screen);
int pc_save_screenshot (char *file);
int pc_convert_bitmap ( char *old_file, char *new_file );

void pc_set_ui_defaults ( void );
void pc_set_controller_defaults ( void );
void pc_set_keyboard_defaults ( void );
int pc_check_for_lr_keys ( int key_value );
int pc_interpret_key ( char *string );
char * pc_interpret_key_value ( int value, char *string );
int pc_interpret_mod_key ( char *string );
char * pc_interpret_mod_key_value ( int value, char *string );

void pc_blit_ntsc_filtered_screen_with_inputs(atari_ntsc_t *ntsc_emu,SDL_Surface *surface, SDL_Rect surface_rect, SDL_Surface *screen, SDL_Rect screen_rect);
#endif
