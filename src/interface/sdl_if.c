/******************************************************************************
*
* FILENAME: sdl_if.c
*
* DESCRIPTION:  This intefaces the sdl driver (Simple Direct Media Layer)
*
* AUTHORS:  bberlin
*
* CHANGE LOG
*
*  VER     DATE     CHANGED BY   DESCRIPTION OF CHANGE
* ------  --------  ----------   ------------------------------------
* 0.1     07/18/04  bberlin      Creation
* 0.2     03/09/06  bberlin      Added GUI video setup
*                                Fixed D-Pad and analog axis centering issue
*                                Added debugger video setup to put in window
* 0.2.1   03/13/06  bberlin      Found problem where I was doing axis_idle[0]
*                                  instead of axis_idle[1] causing centering
*                                  issue on vertical axis
* 0.2.2   03/15/06  bberlin      Added and implemented deadzone parameter.
*                                  Also changed so that we don't cut off 5200
*                                  range in deadzone.
* 0.2.3   03/16/06  bberlin      Added ALT-F4 exits program to 'pc_poll_events'
* 0.2.3   03/19/06  bberlin      Added Pokey_sound_init to 'pc_init'
* 0.3.0   04/04/06  bberlin      Added pot_low and pot_high variables to
*                                  handle sensitivity configuration parameter
* 0.3.0   04/05/06  bberlin      Use axis_value with simulate_analog parameter
*                                Fix vertical axis deadzone issue
* 0.4.0   04/16/06  bberlin      Changed Pokey_sound_init in 'pc_init' to
*                                  Pokey_sound_update_freq
* 0.4.0   05/30/06  bberlin      Changed 'pc_poll_events' to use UI Key param
* 0.4.0   05/31/06  bberlin      Added 'pc_toggle_fullscreen' function
* 0.6.2   06/25/09  bberlin      Changed default player 2 down to s key
* 0.6.2   06/25/09  bberlin      Changed default sensitivity to 96 to fix
*                                  Galaxian with digital stick
******************************************************************************/
#include <SDL.h>
#include <math.h>
#include "../kat5200.h"
#include "sdl_if.h"
#include "util.h"
#include "palette.h"
#include "../core/pokeysound.h"
#include "../core/pokey.h"
#include "kconfig.h"
#include "input.h"
#include "sound.h"
#include "ui.h"
#include "logger.h"

void pc_audio_callback (void *userdata, Uint8 *buffer, int len);
int pc_open_joystick (int index);

/*
 * Setup functions
 */
int pc_set_game_sound (void);

SDL_Joystick *joy[MAX_PC_DEVICE];

extern t_pc_input g_pc_input;
extern t_atari_input g_input;
extern t_atari_video g_video;
extern t_atari_sound g_sound;
extern t_ui g_ui;

static t_sdl_if g_pc_if;

/******************************************************************************
**  Function   :  pc_init
**                            
**  Objective  :  This function initializes the PC interface (SDL in this case)
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
int pc_init (void) {

	char msg[256];

	/*
	 * Initialize SDL and all sub-systems
	 */
    if((SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK)==-1)) { 
        sprintf(msg, "pc_init: SDL Initialization Error -  %s", SDL_GetError());
		logger_log_message ( LOG_ERROR, msg, "" );
        return -1;
    }

	g_pc_if.surface = 0;
	g_pc_if.ntsc_emu = 0;

	return 0;

} /* pc_init */

/******************************************************************************
**  Function   :  pc_game_init
**                            
**  Objective  :  This function sets up the PC interface for game play
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
int pc_game_init (void) {

	int i,j;
	int joy_num,joy_open;
	char msg[256];
	char file[512] = "kat5200.bmp";
	e_direction dir;
	t_config *p_config;

	p_config = config_get_ptr();

	/*
	 * Initialize Our Window
	 */
	sprintf ( msg, "kat5200 %s", VERSION );
	SDL_WM_SetCaption(msg, "");
	util_set_file_to_program_dir ( file );
	SDL_WM_SetIcon(SDL_LoadBMP(file), NULL);
	SDL_SetModState(KMOD_NONE);

	/*
	 * Setup for game video and sound
	 */
	pc_set_game_video ();
	pc_set_game_sound ();

	/*
	 * Start up Joysticks if enabled
	 */
	for ( joy_num = 0; joy_num < MAX_PC_DEVICE; joy_num++ ) {
		joy_open = 0;
		for ( i = 0; i < MAX_CONTROLLER; ++i ) {
			for ( j = 0; j < 0x10; ++j ) {
				if ( (g_input.players[i].keypad[j].device_num == joy_num) &&
				     g_input.players[i].keypad[j].device == DEV_JOYSTICK )
					joy_open = 1;	
			}
			if ( (g_input.players[i].top_button.device_num == joy_num) &&
			      g_input.players[i].top_button.device == DEV_JOYSTICK )
				joy_open = 1;
			if ( (g_input.players[i].bottom_button.device_num == joy_num) &&
			      g_input.players[i].bottom_button.device == DEV_JOYSTICK )
				joy_open = 1;
			for ( j = 0; j < MAX_DIR; ++j ) {
				if ( (g_input.players[i].stick.direction[j].device_num == joy_num) &&
				      g_input.players[i].stick.direction[j].device == DEV_JOYSTICK ) {
					joy_open = 1;
				}
			}

		} /* end for each 5200 emulated controller   */

		if ( joy_open )
			pc_open_joystick ( joy_num ); 

	} /* end for each possible physical joystick */

	/*
	 * Make sure pokey knows if we use trackball
	 *   Also, grab mouse if it is used
	 */
	SDL_WM_GrabInput(SDL_GRAB_OFF);
	for ( i = 0; i < MAX_CONTROLLER; ++i ) {

		if ( g_input.players[i].control_type == CTRLR_TYPE_NONE )
			continue;

		for ( dir = DIR_LEFT; dir < MAX_DIR; ++dir ) {
			if ( (g_input.players[i].stick.direction[dir].device == DEV_MOUSE) ||
			     (g_input.players[i].paddles.direction[dir].device == DEV_MOUSE) ) 
				SDL_WM_GrabInput(SDL_GRAB_ON);
		}

		pokey_set_controller_type ( i, CTRL_STICK );
		if ( g_input.players[i].control_type == CTRLR_TYPE_TRACKBALL &&
		     g_input.machine_type == MACHINE_TYPE_5200 ) {
			pokey_set_controller_type ( i, CTRL_BALL );
		}

	} /* end for controller */

	return 0;

} /* end pc_game_init */

/******************************************************************************
**  Function   :  pc_set_game_sound
**                            
**  Objective  :  This function sets up sound for emulator play
**
**  Parameters :  NONE
**                                                
**  return     :  0 on success, otherwise error
**      
******************************************************************************/
int pc_set_game_sound (void) {

	char msg[512];

	g_pc_if.audio_d.freq = g_sound.freq;
	g_pc_if.audio_d.format = AUDIO_U8;
	g_pc_if.audio_d.samples = g_sound.samples;
	g_pc_if.audio_d.channels = 1;
	g_pc_if.audio_d.callback = pc_audio_callback;
	g_pc_if.audio_d.userdata = NULL;

	if ( g_sound.on ) {
		if ( SDL_OpenAudio(&g_pc_if.audio_d, &g_pc_if.audio_o) < 0 ){
			sprintf(msg, "pc_set_game_sound: Sound Initialization Error - %s", SDL_GetError());
			logger_log_message ( LOG_ERROR, msg, "" );
			return -1;
		}
	
		/*
		 * Log output Sound parameters
		 */
		sprintf( msg, "pc_set_game_sound: Sound Initialized - Freq=%d, Samples=%d",
		         g_pc_if.audio_o.freq, g_pc_if.audio_o.samples );
		logger_log_message ( LOG_INFO, msg, "" );

		Pokey_sound_update_freq (FREQ_17_APPROX, (Uint16)g_pc_if.audio_o.freq);

		SDL_PauseAudio(0);
	}

	return 0;

} /* end pc_set_game_sound */

/******************************************************************************
**  Function   :  pc_set_palette
**                            
**  Objective  :  This function opens up and assigns the palette
**
**  Parameters :  NONE
**                                                
**  return     :  0
**      
******************************************************************************/
int pc_set_palette ( t_atari_video *p_video, SDL_Color *palette, unsigned int *rgb ) {

	int i;
	int real_pal[0x100];
	char *palette_title;
	t_config *p_config;

	p_config = config_get_ptr();

	if ( p_config->system_type == PAL )
		palette_title = p_video->pal_palette;
	else
		palette_title = p_video->ntsc_palette;
	if ( recall_palette(palette_title, real_pal ) )
		make_default_palette ( real_pal );

	for ( i = 0; i < 256; ++i ) {
    	palette[i].r = (real_pal[i] >> 16) & 0xff;
    	palette[i].g = (real_pal[i] >> 8) & 0xff;
    	palette[i].b = real_pal[i] & 0xff;
		rgb[i] = i;
	}

	return 0;

} /* end pc_set_palette */

/******************************************************************************
**  Function   :  pc_set_game_video
**                            
**  Objective  :  This function sets up video for emulator play
**
**  Parameters :  NONE
**                                                
**  return     :  pointer to screen
**      
******************************************************************************/
SDL_Surface * pc_set_game_video (void) {

	int i, best_fit_index, status;
	Uint32 flags;
	int vid_w, vid_h, shown_w, shown_h;
	SDL_Rect **p_modes;
	t_config *p_config;
	char msg[100];
	int best_score, this_score;

	p_config = config_get_ptr();

	/*
	 * First, handle colors, for 8 bit we have an RGB palette
	 *   Otherwise map RGB to a 32-bit array
	 */
	pc_set_palette ( &g_video, g_pc_if.color_palette, g_pc_if.color_rgb );

	flags = SDL_HWPALETTE;
	flags |= g_video.fullscreen ? SDL_FULLSCREEN : 0;

	/*
	 * Determine needed video resolution
	 */
	if ( g_video.ntsc_filter_on ) {
		vid_w = g_video.widescreen ? (ATARI_NTSC_OUT_WIDTH(342)) : atari_ntsc_min_out_width;
		/*vid_w = ATARI_NTSC_OUT_WIDTH( (g_video.widescreen ? 342:320) );*/
		vid_h = 480;
		g_video.pixel_depth = 16;
	}
	else {
		vid_w = (g_video.widescreen ? 342:320) * (g_video.zoom + 1);
		vid_h = (config_get_ptr()->system_type==PAL ? 240 : 240) * (g_video.zoom + 1);
	}
	
	shown_w = vid_w;
	shown_h = vid_h;
	
	/*
	 * Figure Out Actual
	 */
	status = pc_get_modes ( g_video.pixel_depth, flags, &p_modes );

	if ( vid_w < g_video.width )
		vid_w = g_video.width;
	if ( vid_h < g_video.height )
		vid_h = g_video.height;

	/*
	 * status = 0 means we need to pick from available modes
	 */
	best_fit_index = 0;
	best_score = 1000;
	if ( !status ) {
		for ( i = 0; p_modes[i]; i++ ) {
			if ( (p_modes[i]->h == vid_h) && 
			     (p_modes[i]->w == vid_w) ) {
				best_fit_index = i;
				break;
			}
			this_score = (p_modes[i]->h - vid_h) + (p_modes[i]->w - vid_w);
			this_score += (int)(fabs((double)p_modes[i]->w / (double)p_modes[i]->h - 4.0/3.0) * 20.0);
			if ( (this_score < best_score) && 
			     (p_modes[i]->h >= vid_h) && (p_modes[i]->w >= vid_w) ) {
				best_fit_index = i;
				best_score = this_score;
			}

		}
		vid_h = p_modes[best_fit_index]->h;
		vid_w = p_modes[best_fit_index]->w;
	}

	g_pc_if.screen=SDL_SetVideoMode(vid_w, vid_h, g_video.pixel_depth, flags);

	SDL_ShowCursor(SDL_DISABLE);

	/*
	 * Create a line at least double scale
	 *   line needs to be 448 pixels     
	 *  384 (wide) + 32 (hscroll) + 32 (pl width changes)
	 */
	//g_pc_if.line=SDL_CreateRGBSurface(SDL_SWSURFACE, 448, 1, 
	SDL_FreeSurface ( g_pc_if.surface );
	g_pc_if.surface=SDL_CreateRGBSurface(SDL_SWSURFACE, 448, (config_get_ptr()->system_type==PAL ? 240 : 240), 
	                          g_video.ntsc_filter_on ? 8 : g_pc_if.screen->format->BitsPerPixel, 
							  0, 0, 0, 0);

	/*
	 * Assign colors to the screen and surface
	 */
	if ( g_pc_if.screen->format->BitsPerPixel == 8 ) {
		SDL_SetColors(g_pc_if.screen, g_pc_if.color_palette, 0, 256);
		SDL_SetColors(g_pc_if.surface, g_pc_if.color_palette, 0, 256);
	}
	else {
		if ( !g_video.ntsc_filter_on ) {
			for ( i = 0; i < 256; ++i ) {
				g_pc_if.color_rgb[i] = SDL_MapRGB ( g_pc_if.screen->format,
				                              g_pc_if.color_palette[i].r,
				                              g_pc_if.color_palette[i].g,
											  g_pc_if.color_palette[i].b );
			}
		}
		else {
			SDL_SetColors(g_pc_if.surface, g_pc_if.color_palette, 0, 256);
		}
	}

	/*
	 * Setup Copying stuff
	 */
	g_pc_if.surface_rect.x = (g_video.widescreen ? 21 : 32);
	g_pc_if.surface_rect.y = 0;
	g_pc_if.surface_rect.w = (g_video.widescreen ? 342 : 320);
	g_pc_if.surface_rect.h = (config_get_ptr()->system_type==PAL ? 240 : 240);

	if ( (g_pc_if.screen->w - shown_w) > 0 )
		g_pc_if.screen_rect.x = (g_pc_if.screen->w - shown_w) / 2;
	else
		g_pc_if.screen_rect.x = 0;

	if ( (g_pc_if.screen->h - shown_h) > 0 )
		g_pc_if.screen_rect.y = (g_pc_if.screen->h - shown_h) / 2;
	else
		g_pc_if.screen_rect.y = 0;

	/*
	 * Setup NTSC filtering if on
	 */
	if ( g_pc_if.ntsc_emu ) {
		free ( g_pc_if.ntsc_emu );
		g_pc_if.ntsc_emu = 0;
	}
	if ( g_video.ntsc_filter_on ) {
		g_pc_if.ntsc_emu = (atari_ntsc_t*) malloc( sizeof (atari_ntsc_t) );
		atari_ntsc_init( g_pc_if.ntsc_emu, &g_video.ntsc_setup );
	}

	/*
	 * Setup GTIA drawing surface
	 */
	gtia_set_screen ( g_pc_if.surface->pixels, g_pc_if.color_rgb, g_pc_if.surface->pitch, 
	                    g_pc_if.surface->format->BytesPerPixel, 0 );

	sprintf( msg, "pc_set_game_video: Video Initialized - %d x %d x %d",
	         g_pc_if.screen->w, g_pc_if.screen->h, g_pc_if.screen->format->BitsPerPixel );
	logger_log_message ( LOG_INFO, msg, "" );

	return g_pc_if.screen;

} /* end pc_set_game_video */

/******************************************************************************
**  Function   :  pc_set_debugger_video
**                            
**  Objective  :  This function makes sure we are in windowed mode to see 
**                 command window
**
**  Parameters :  NONE
**                                                
**  return     :  pointer to screen
**      
******************************************************************************/
SDL_Surface * pc_set_debugger_video (void) {

	Uint32 flags;

	flags = g_pc_if.screen->flags;

	if ( flags & SDL_FULLSCREEN ) {

		flags &= ~SDL_FULLSCREEN;
		g_pc_if.screen=SDL_SetVideoMode(g_pc_if.screen->w, g_pc_if.screen->h, 8, flags);
	}

	return g_pc_if.screen;

} /* end pc_set_debugger_video */

/******************************************************************************
**  Function   :  pc_get_modes
**                            
**  Objective  :  This function queries for an array of available video modes
**
**  Parameters :  bpp     - bits per pixel
**                flags   - contains desired video format flags
**                p_modes - address to store array of resolutions
**                                                
**  return     :  0 for modes available, 1 for all resolutions, -1 for none
**      
******************************************************************************/
int pc_get_modes (int bpp, int flags, SDL_Rect ***p_modes) {

	SDL_Rect **modes;
	SDL_PixelFormat *vfmt;
	SDL_PixelFormat check_vfmt;

	/* Get available fullscreen/hardware modes */
	vfmt = SDL_GetVideoInfo()->vfmt;
	check_vfmt = *vfmt;
	check_vfmt.BitsPerPixel = bpp;
	check_vfmt.BytesPerPixel = bpp / 8;
	modes=SDL_ListModes(&check_vfmt, flags);

	/* Check if there are any modes available */
	if(modes == (SDL_Rect **)0){
	  return -1;
	}

	/* Check if our resolution is restricted */
	if(modes == (SDL_Rect **)-1){
	  return 1;
	}
	else{
	  /* Print valid modes */
		*p_modes = modes;
	/*
	  printf("Available Modes\n");
	  for(i=0;modes[i];++i)
	    printf("  %d x %d\n", modes[i]->w, modes[i]->h);
	*/
	}

	return 0;
}

/******************************************************************************
**  Function   :  pc_toggle_fullscreen
**                            
**  Objective  :  This function toggles between fullscreen and window
**
**  Parameters :  NONE
**                                                
**  return     :  pointer to screen
**      
******************************************************************************/
SDL_Surface * pc_toggle_fullscreen (void) {

	Uint32 flags;

	flags = g_pc_if.screen->flags;

	if ( flags & SDL_FULLSCREEN ) {
		flags &= ~SDL_FULLSCREEN;
		g_pc_if.screen=SDL_SetVideoMode(g_pc_if.screen->w, g_pc_if.screen->h, 
		                      g_pc_if.screen->format->BitsPerPixel, flags);
		if ( g_pc_if.screen->format->BitsPerPixel == 8 )
			SDL_SetColors(g_pc_if.screen, g_pc_if.color_palette, 0, 256);
	}
	else {
		flags |= SDL_FULLSCREEN;
		g_pc_if.screen=SDL_SetVideoMode(g_pc_if.screen->w, g_pc_if.screen->h, 
		                      g_pc_if.screen->format->BitsPerPixel, flags);
		if ( g_pc_if.screen->format->BitsPerPixel == 8 )
			SDL_SetColors(g_pc_if.screen, g_pc_if.color_palette, 0, 256);
	}

	return g_pc_if.screen;

} /* end pc_toggle_fullscreen */

/******************************************************************************
**  Function   :  pc_toggle_gui_fullscreen
**                            
**  Objective  :  This function toggles between fullscreen and window for gui
**
**  Parameters :  NONE
**                                                
**  return     :  pointer to screen
**      
******************************************************************************/
SDL_Surface * pc_toggle_gui_fullscreen (SDL_Surface *gui_screen) {

	Uint32 flags;

	flags = gui_screen->flags;

	if ( flags & SDL_FULLSCREEN ) {
		flags &= ~SDL_FULLSCREEN;
		gui_screen=SDL_SetVideoMode(gui_screen->w, gui_screen->h, 32, flags);
	}
	else {
		flags |= SDL_FULLSCREEN;
		gui_screen=SDL_SetVideoMode(gui_screen->w, gui_screen->h, 32, flags);
	}

	return gui_screen;

} /* end pc_toggle_gui_fullscreen */

/******************************************************************************
**  Function   :  pc_set_gui_video
**                            
**  Objective  :  This function sets up video for the gui menu 
**
**  Parameters :  NONE
**                                                
**  return     :  pointer to screen
**      
******************************************************************************/
SDL_Surface * pc_set_gui_video (void) {

	Uint32 flags = 0;
	t_config *p_config;
	SDL_Surface *screen;

	p_config = config_get_ptr();

	flags |= g_video.fullscreen ? SDL_FULLSCREEN : 0;

	screen=SDL_SetVideoMode(640, 480, 32, flags);

	SDL_ShowCursor(SDL_ENABLE);

	return screen;

} /* end pc_set_gui_video */

/******************************************************************************
**  Function   :  pc_save_screenshot
**                            
**  Objective  :  This function save a bitmap of the current output screen
**
**  Parameters :  file - filename for bitmap
**                                                
**  return     :  error code
**      
******************************************************************************/
int pc_save_screenshot (char *file) {

	return SDL_SaveBMP ( g_pc_if.screen, file );

} /* end pc_save_screenshot */

/******************************************************************************
**  Function   :  pc_exit
**                            
**  Objective  :  This function frees up the PC resources used
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
int pc_exit (void) {

	if ( g_pc_if.ntsc_emu ) {
		free ( g_pc_if.ntsc_emu );
		g_pc_if.ntsc_emu = 0;
	}

	/*
	 * Free the line we used
	 */
	SDL_FreeSurface ( g_pc_if.surface );

	/*
	 * Shutdown all subsystems
	 */
    SDL_Quit();

	return 0;

} /* end pc_exit */

/******************************************************************************
**  Function   :  pc_open_joystick
**                            
**  Objective  :  This function opens up a joystick device if needed
**
**  Parameters :  index - joystick index
**                                                
**  return     :  0 for success, otherwise failure
**      
******************************************************************************/
int pc_open_joystick (int index) {

	char msg[256];

	if ( (index > (MAX_PC_DEVICE-1)) || SDL_JoystickOpened(index) )
		return 0;

	joy[index] = SDL_JoystickOpen ( index );	

	if ( joy[index] ) {
		sprintf ( msg, "Joystick %d : %s Initialized", index, SDL_JoystickName(index) );
		logger_log_message ( LOG_INFO, msg, "" );
	}
	else { 
		sprintf(msg, "Joystick %d : Could not open!", index);
		logger_log_message ( LOG_ERROR, msg, "" );
		return -1;
	}

	return 0;

} /* end pc_open_joystick */

/******************************************************************************
**  Function   :  pc_detect_and_open_joysticks
**                            
**  Objective  :  This function checks number of joystick and tries to open
**                all of then quietly (for GUI autodetect)
**
**  Parameters :  NONE
**                                                
**  return     :  number found
**      
******************************************************************************/
int pc_detect_and_open_joysticks (void) {

	int i,total = 0;

	SDL_QuitSubSystem ( SDL_INIT_JOYSTICK );

	SDL_InitSubSystem ( SDL_INIT_JOYSTICK );

	total = SDL_NumJoysticks();

	for ( i = 0; i < total; ++i ) {
		SDL_JoystickOpen ( i );	
	}

	/*
	 * This clears the event queue, which we want for the GUI
	 *   Fixes problem where button event registers in GUI and we go back
	 */
	SDL_JoystickEventState ( SDL_ENABLE );

	return total;

} /* end pc_detect_and_open_joystick */

/******************************************************************************
**  Function   :  pc_detect_joysticks
**                            
**  Objective  :  This function checks number of joystick and returns result
**
**  Parameters :  NONE
**                                                
**  return     :  number found
**      
******************************************************************************/
int pc_detect_joysticks (void) {

	int total = 0;

	SDL_QuitSubSystem ( SDL_INIT_JOYSTICK );

	SDL_InitSubSystem ( SDL_INIT_JOYSTICK );

	total = SDL_NumJoysticks();

	return total;

} /* end pc_detect_joysticks */

/******************************************************************************
**  Function   :  pc_get_joystick_name
**                            
**  Objective  :  This function returns the joystick name by index
**
**  Parameters :  index
**                                                
**  return     :  0 for success, otherwise failure
**      
******************************************************************************/
const char * pc_get_joystick_name ( int index ) {

	return SDL_JoystickName(index);

} /* end pc_get_joystick_name */

/******************************************************************************
**  Function   :  pc_delay
**                            
**  Objective  :  This function delays the number of ms input 
**
**  Parameters :  ms - number of milliseconds to delay
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_delay (int ms) {

    SDL_Delay(ms);

} /* end pc_delay */

/******************************************************************************
**  Function   :  pc_get_ticks
**                            
**  Objective  :  This function return the number of ms since SDL was started
**
**  Parameters :  NONE
**                                                
**  return     :  ms
**      
******************************************************************************/
int pc_get_ticks (void) {

    return SDL_GetTicks();

} /* end pc_get_ticks */

/******************************************************************************
**  Function   :  pc_get_video_scale
**                            
**  Objective  :  This function returns the scaling needed from gtia
**
**  Parameters :  NONE
**                                                
**  return     :  scale
**      
******************************************************************************/
int pc_get_video_scale (void) {

    return g_video.zoom;

} /* end pc_get_video_scale */

/******************************************************************************
**  Function   :  pc_lock_audio
**                            
**  Objective  :  This function keeps audio callback from running
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
int pc_lock_audio (void) {

    SDL_LockAudio();

	return 0;

} /* end pc_lock_audio */

/******************************************************************************
**  Function   :  pc_unlock_audio
**                            
**  Objective  :  This function keeps allows audio callback to run      
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
int pc_unlock_audio (void) {

    SDL_UnlockAudio();

	return 0;

} /* end pc_unlock_audio */

/******************************************************************************
**  Function   :  pc_audio_callback
**                            
**  Objective  :  This function updates audio buffers from pokey sound
**
**  Parameters :  userdata - provided by SDL
**                buffer   - We fill this up from pokey sound
**                len      - length of the buffer to fill      
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_audio_callback (void *userdata, Uint8 *buffer, int len) {

	/*
     * Just call Pokey process with the buffer
	 */
	Pokey_process (buffer, (Uint16)len);

} /* end pc_audio_callback */

/******************************************************************************
**  Function   :  pc_get_scanline_ptr
**                            
**  Objective  :  This function returns a pointer to the beginning of the line
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
UINT8 * pc_get_scanline_ptr (void) {

	 return (g_pc_if.line->pixels);

} /* end pc_get_scanline_ptr */

void * pc_get_current_line_ptr (void) {

	 return (g_pc_if.line_pixel_ptr);

} /* end pc_get_current_line_ptr */

/******************************************************************************
**  Function   :  pc_set_scanline_ptr
**                            
**  Objective  :  This function sets the line pointer to the specified place
**
**  Parameters :  ptr - ptr to place in line
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_set_scanline_ptr ( void *ptr ) {

	 g_pc_if.line_pixel_ptr = ptr;

} /* end pc_set_scanline_ptr */

int pc_get_ntsc_filter ( void ) {

	return (int) g_pc_if.ntsc_emu;

} /* end pc_get_ntsc_filter */

/******************************************************************************
**  Function   :  pc_get_bytes_per_pixel
**                            
**  Objective  :  This function the number of bytes per pixel
**
**  Parameters :  NONE
**                                                
**  return     :  bytes per pixel
**      
******************************************************************************/
int pc_get_bytes_per_pixel (void) {

	 return (g_pc_if.line->format->BytesPerPixel);

} /* end pc_get_bytes_per_pixel */

/******************************************************************************
**  Function   :  pc_blit_ntsc_filtered_screen
**                            
**  Objective  :  This function filters the input surface and outputs to screen
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_blit_ntsc_filtered_screen (void) {

	static int burst_change = 0;
	int y;


	burst_change ^= 1;

	/*
	if ( g_video.ntsc_setup.merge_fields )
		burst_change = 0;
	*/

	atari_ntsc_blit( g_pc_if.ntsc_emu, 
	                 (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * 
	                     g_pc_if.surface->format->BytesPerPixel, 
	                 g_pc_if.surface->pitch,
					 g_video.widescreen ? (ATARI_NTSC_OUT_WIDTH(342)) : atari_ntsc_min_out_width,
	                 g_pc_if.surface->h, 
	                 (unsigned short *)((unsigned char *)(g_pc_if.screen->pixels) + g_pc_if.screen_rect.x * 
	                     g_pc_if.screen->format->BytesPerPixel + g_pc_if.screen_rect.y * g_pc_if.screen->pitch), 
	                 g_pc_if.screen->pitch );

	for ( y = g_pc_if.surface->h-1; --y >= 0; )
	{
		unsigned char const* in = (unsigned char *)(g_pc_if.screen->pixels) + (y + g_pc_if.screen_rect.y) * g_pc_if.screen->pitch;
		unsigned char* out = (unsigned char *)(g_pc_if.screen->pixels) + (y * 2 + g_pc_if.screen_rect.y) * g_pc_if.screen->pitch;
		int n;
		for ( n = g_pc_if.screen->w; n; --n )
		{
			unsigned prev = *(unsigned short*) in;
			unsigned next = *(unsigned short*) (in + g_pc_if.screen->pitch);
			/* mix 16-bit rgb without losing low bits */
			unsigned mixed = prev + next + ((prev ^ next) & 0x0821);
			/* darken by 12% */
			*(unsigned short*) out = prev;
			*(unsigned short*) (out + g_pc_if.screen->pitch) = (mixed >> 1) - (mixed >> 4 & 0x18E3);
			in += 2;
			out += 2;
		}
	}


} /* end pc_blit_ntsc_filtered_screen */

/******************************************************************************
**  Function   :  pc_blit_ntsc_filtered_screen_with_inputs
**                            
**  Objective  :  This function filters the input surface and outputs to screen
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_blit_ntsc_filtered_screen_with_inputs(atari_ntsc_t *ntsc_emu,SDL_Surface *surface, SDL_Rect surface_rect,
                                              SDL_Surface *screen, SDL_Rect screen_rect) {

	static int burst_change = 0;
	int y;


	burst_change ^= 1;

	/*
	if ( g_video.ntsc_setup.merge_fields )
		burst_change = 0;
	*/

	atari_ntsc_blit( ntsc_emu, (unsigned char *)(surface->pixels) + surface_rect.x * 
	                     surface->format->BytesPerPixel + surface_rect.y * surface->pitch, 
	                 surface->pitch,
	                  screen_rect.w, surface_rect.h, 
	                 (unsigned short *)((unsigned char *)(screen->pixels) + screen_rect.x * 
	                     screen->format->BytesPerPixel), 
	                 screen->pitch );

	for ( y = surface_rect.h; --y >= 0; )
	{
		unsigned char const* in = (unsigned char *)(screen->pixels) + y * screen->pitch;
		unsigned char* out = (unsigned char *)(screen->pixels) + y * 2 * screen->pitch;
		int n;
		for ( n = screen->w; n; --n )
		{
			unsigned prev = *(unsigned short*) in;
			unsigned next = *(unsigned short*) (in + screen->pitch);
			/* mix 16-bit rgb without losing low bits */
			unsigned mixed = prev + next + ((prev ^ next) & 0x0821);
			/* darken by 12% */
			*(unsigned short*) out = prev;
			*(unsigned short*) (out + screen->pitch) = (mixed >> 1) - (mixed >> 4 & 0x18E3);
			in += 2;
			out += 2;
		}
	}

} /* end pc_blit_ntsc_filtered_screen_with_inputs */

/******************************************************************************
**  Function   :  pc_blit_screen_scaled_2x
**                            
**  Objective  :  This function does a 2x scale to the output screen
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_blit_screen_scaled_2x (void) {

	int x,y;
	unsigned char *dp8, *dp8start, *dp8plus, *sp8, *sp8start;
	unsigned short *dp16, *dp16plus, *sp16;
	unsigned int *dp32, *dp32plus, *sp32;

	switch ( g_pc_if.screen->format->BytesPerPixel ) {
		case 1:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp8 = dp8start;
				dp8plus = dp8 + g_pc_if.screen->pitch;
				sp8 = sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp8++ = *dp8plus++ = *sp8;
					*dp8++ = *dp8plus++ = *sp8;
					sp8++;
				}
				dp8start += (g_pc_if.screen->pitch << 1 );
				sp8start += g_pc_if.surface->pitch;
			}
		break;
		case 2:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp16 = (unsigned short *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp16plus = (unsigned short *)dp8start;
				sp16 = (unsigned short *)sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp16++ = *dp16plus++ = *sp16;
					*dp16++ = *dp16plus++ = *sp16;
					sp16++;
				}
				dp8start += (g_pc_if.screen->pitch);
				sp8start += g_pc_if.surface->pitch;
			}
		break;
		case 3:
		break;
		case 4:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp32 = (unsigned int *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp32plus = (unsigned int *)dp8start;
				sp32 = (unsigned int *)sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp32++ = *dp32plus++ = *sp32;
					*dp32++ = *dp32plus++ = *sp32;
					sp32++;
				}
				dp8start += g_pc_if.screen->pitch;
				sp8start += g_pc_if.surface->pitch;
			}

		break;
	}
} /* end pc_blit_screen_scaled_2x */

/******************************************************************************
**  Function   :  pc_blit_screen_scaled_3x
**                            
**  Objective  :  This function does a 3x scale to the output screen
**
**  Parameters :  NONE
**                                                
**  return     :  NONE
**      
******************************************************************************/
void pc_blit_screen_scaled_3x (void) {

	int x,y;
	unsigned char *dp8, *dp8start, *dp8plus, *dp8plus2, *sp8, *sp8start;
	unsigned short *dp16, *dp16plus, *dp16plus2, *sp16;
	unsigned int *dp32, *dp32plus, *dp32plus2, *sp32;

	switch ( g_pc_if.screen->format->BytesPerPixel ) {
		case 1:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp8 = dp8start;
				dp8plus = dp8 + g_pc_if.screen->pitch;
				dp8plus2 = dp8plus + g_pc_if.screen->pitch;
				sp8 = sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp8++ = *dp8plus++ = *dp8plus2++ = *sp8;
					*dp8++ = *dp8plus++ = *dp8plus2++ = *sp8;
					*dp8++ = *dp8plus++ = *dp8plus2++ = *sp8;
					sp8++;
				}
				dp8start += (g_pc_if.screen->pitch << 1 );
				sp8start += g_pc_if.surface->pitch;
			}
		break;
		case 2:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp16 = (unsigned short *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp16plus = (unsigned short *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp16plus2 = (unsigned short *)dp8start;
				sp16 = (unsigned short *)sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp16++ = *dp16plus++ = *dp16plus2++ = *sp16;
					*dp16++ = *dp16plus++ = *dp16plus2++ = *sp16;
					*dp16++ = *dp16plus++ = *dp16plus2++ = *sp16;
					sp16++;
				}
				dp8start += g_pc_if.screen->pitch;
				sp8start += g_pc_if.surface->pitch;
			}
		break;
		case 3:
		break;
		case 4:
			dp8start = (unsigned char *)(g_pc_if.screen->pixels) + 
			            g_pc_if.screen_rect.y * g_pc_if.screen->pitch + 
			            g_pc_if.screen_rect.x * g_pc_if.screen->format->BytesPerPixel;
			sp8start = (unsigned char *)(g_pc_if.surface->pixels) + g_pc_if.surface_rect.x * g_pc_if.surface->format->BytesPerPixel;
			for ( y = 0; y < g_pc_if.surface_rect.h; ++y ) {
				dp32 = (unsigned int *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp32plus = (unsigned int *)dp8start;
				dp8start += g_pc_if.screen->pitch;
				dp32plus2 = (unsigned int *)dp8start;
				sp32 = (unsigned int *)sp8start;
				for ( x = 0; x < g_pc_if.surface_rect.w; ++x ) {
					*dp32++ = *dp32plus++ = *dp32plus2++ = *sp32;
					*dp32++ = *dp32plus++ = *dp32plus2++ = *sp32;
					*dp32++ = *dp32plus++ = *dp32plus2++ = *sp32;
					sp32++;
				}
				dp8start += g_pc_if.screen->pitch;
				sp8start += g_pc_if.surface->pitch;
			}
		break;
	}
} /* end pc_blit_screen_scaled_3x */

/******************************************************************************
**  Function   :  pc_poll_events
**                            
**  Objective  :  This function checks for events and acts accordingly
**
**  Parameters :  NONE
**                                                
**  return     :  return code (-1 to exit app)
**      
******************************************************************************/
int pc_poll_events (void) {

	int i,status;
	int tmp_key_mod;
	SDL_Event event;

	/*
	 * Temp struct declarations
	 */
	t_pc_mod_keyboard_key *p_ui;
	t_pc_keyboard_key *p_kb;
	t_pc_digital_hat *p_hat;
	t_pc_analog_axis *p_axis;
	t_pc_ball *p_ball;
	t_pc_ball *p_ms;
	t_pc_button *p_btn;
	t_pc_button *p_msbtn;

	if ( !g_video.zoom && !g_video.ntsc_filter_on ) {
		status = SDL_BlitSurface(g_pc_if.surface, &g_pc_if.surface_rect, g_pc_if.screen, &g_pc_if.screen_rect);
	}
	else if ( g_video.ntsc_filter_on )
		pc_blit_ntsc_filtered_screen ();
	else if ( g_video.zoom == 2 )
		pc_blit_screen_scaled_3x ();
	else 
		pc_blit_screen_scaled_2x ();

	SDL_UpdateRect(g_pc_if.screen, 0, 0, 0, 0);
	
	/*
	* Call functions to run every frame        
	*/
	for ( i = 0; i < MAX_CONTROLLER; ++i ) {

		g_pc_input.frame_x_function[i] ( 0, 0 );
		g_pc_input.frame_y_function[i] ( 0, 0 );

	} /* end for 5200 controller */

	status = 0;

	while ( SDL_PollEvent(&event) ) {
		switch (event.type) {
			case SDL_ACTIVEEVENT:
				break;

			case SDL_QUIT: 
				return -1;
				break;
            
         	case SDL_KEYDOWN:
				tmp_key_mod = event.key.keysym.mod & 0x0fff;

				/*
				 * If a modifier key is pressed, check for UI keys
				 */
				if ( tmp_key_mod ) {
					for ( i = 0; i < UI_KEY_MAX; ++i ) {
						p_ui = &g_pc_input.ui_keyboard[i];
						if ( (tmp_key_mod & p_ui->modifier) && event.key.keysym.sym == p_ui->key ) {
							if ( (status = p_ui->key_down_function ( p_ui->player, 0, 0 )) )
								return status;
						}
						else {
							p_kb = &g_pc_input.keyboard[event.key.keysym.sym];
							if ( (status = p_kb->key_down_function ( p_kb->player, p_kb->direction, 
							                                                       p_kb->atari_key )) )
								return status;
						}
					}
				}

				/*
				 * Else just call the function for this key
				 */
				else {
					p_kb = &g_pc_input.keyboard[event.key.keysym.sym];
					if ( (status = p_kb->key_down_function ( p_kb->player, p_kb->direction, 
					                                                       p_kb->atari_key )) )
						return status;
				}

				break;
        	case SDL_KEYUP:
				p_kb = &g_pc_input.keyboard[event.key.keysym.sym];
				p_kb->key_up_function ( p_kb->player, p_kb->direction, p_kb->atari_key );
        		break;
        	case SDL_JOYBALLMOTION:
				p_ball = &g_pc_input.trackball[event.jball.which][event.jball.ball];
				if ( event.motion.xrel < 0 ) {
					p_ball->x_axis.axis_pos_function ( p_ball->x_axis.player, p_ball->x_axis.direction_pos, 0 );
					p_ball->x_axis.axis_neg_function ( p_ball->x_axis.player, p_ball->x_axis.direction_neg, abs((event.motion.xrel*(p_ball->speed))>>1) );
				}
				else {
					p_ball->x_axis.axis_neg_function ( p_ball->x_axis.player, p_ball->x_axis.direction_neg, 0 );
					p_ball->x_axis.axis_pos_function ( p_ball->x_axis.player, p_ball->x_axis.direction_pos, abs((event.motion.xrel*(p_ball->speed))>>1) );
				}

				if ( event.motion.yrel < 0 ) {
					p_ball->y_axis.axis_pos_function ( p_ball->y_axis.player, p_ball->y_axis.direction_pos, 0 );
					p_ball->y_axis.axis_neg_function ( p_ball->y_axis.player, p_ball->y_axis.direction_neg, abs((event.motion.yrel*(p_ball->speed))>>1) );
				}
				else {
					p_ball->y_axis.axis_neg_function ( p_ball->y_axis.player, p_ball->y_axis.direction_neg, 0 );
					p_ball->y_axis.axis_pos_function ( p_ball->y_axis.player, p_ball->y_axis.direction_pos, abs((event.motion.yrel*(p_ball->speed))>>1) );
				}
        		break;
        	case SDL_JOYHATMOTION:
				p_hat = &g_pc_input.pov_hat[event.jhat.which][event.jhat.hat];
				if ( event.jhat.value & SDL_HAT_LEFT )
					p_hat->hat_pressed_function[DIR_LEFT] ( p_hat->player, p_hat->direction[DIR_LEFT], p_hat->atari_key );
				else
					p_hat->hat_not_pressed_function[DIR_LEFT] ( p_hat->player, p_hat->direction[DIR_LEFT], p_hat->atari_key );
				if ( event.jhat.value & SDL_HAT_RIGHT )
					p_hat->hat_pressed_function[DIR_RIGHT] ( p_hat->player, p_hat->direction[DIR_RIGHT], p_hat->atari_key );
				else
					p_hat->hat_not_pressed_function[DIR_RIGHT] ( p_hat->player, p_hat->direction[DIR_RIGHT], p_hat->atari_key );
				if ( event.jhat.value & SDL_HAT_UP )
					p_hat->hat_pressed_function[DIR_UP] ( p_hat->player, p_hat->direction[DIR_UP], p_hat->atari_key );
				else
					p_hat->hat_not_pressed_function[DIR_UP] ( p_hat->player, p_hat->direction[DIR_UP], p_hat->atari_key );
				if ( event.jhat.value & SDL_HAT_DOWN )
					p_hat->hat_pressed_function[DIR_DOWN] ( p_hat->player, p_hat->direction[DIR_DOWN], p_hat->atari_key );
				else
					p_hat->hat_not_pressed_function[DIR_DOWN] ( p_hat->player, p_hat->direction[DIR_DOWN], p_hat->atari_key );
        		break;
        	case SDL_JOYAXISMOTION:
				p_axis = &g_pc_input.analog_axis[event.jaxis.which][event.jaxis.axis];
				if ( event.jaxis.value < 0 ) {
					p_axis->axis_pos_function ( p_axis->player, p_axis->direction_pos, 0 );
					p_axis->axis_neg_function ( p_axis->player, p_axis->direction_neg, abs(event.jaxis.value) );
				}
				else {
					p_axis->axis_neg_function ( p_axis->player, p_axis->direction_neg, 0 );
					p_axis->axis_pos_function ( p_axis->player, p_axis->direction_pos, abs(event.jaxis.value) );
				}
        		break;
        	case SDL_MOUSEMOTION:
				p_ms = &g_pc_input.mouse;
				if ( event.motion.xrel < 0 ) {
					p_ms->x_axis.axis_pos_function ( p_ms->x_axis.player, p_ms->x_axis.direction_pos, 0 );
					p_ms->x_axis.axis_neg_function ( p_ms->x_axis.player, p_ms->x_axis.direction_neg, abs((event.motion.xrel*(p_ms->speed))>>1) );
				}
				else {
					p_ms->x_axis.axis_neg_function ( p_ms->x_axis.player, p_ms->x_axis.direction_neg, 0 );
					p_ms->x_axis.axis_pos_function ( p_ms->x_axis.player, p_ms->x_axis.direction_pos, abs((event.motion.xrel*(p_ms->speed))>>1) );
				}

				if ( event.motion.yrel < 0 ) {
					p_ms->y_axis.axis_pos_function ( p_ms->y_axis.player, p_ms->y_axis.direction_pos, 0 );
					p_ms->y_axis.axis_neg_function ( p_ms->y_axis.player, p_ms->y_axis.direction_neg, abs((event.motion.yrel*(p_ms->speed))>>1) );
				}
				else {
					p_ms->y_axis.axis_neg_function ( p_ms->y_axis.player, p_ms->y_axis.direction_neg, 0 );
					p_ms->y_axis.axis_pos_function ( p_ms->y_axis.player, p_ms->y_axis.direction_pos, abs((event.motion.yrel*(p_ms->speed))>>1) );
				}
				break;
        	case SDL_MOUSEBUTTONDOWN:
				p_msbtn = &g_pc_input.mouse_button[event.button.button];
				status = p_msbtn->button_down_function ( p_msbtn->player, p_msbtn->direction, p_msbtn->atari_key );
        		break;
        	case SDL_MOUSEBUTTONUP:
				p_msbtn = &g_pc_input.mouse_button[event.button.button];
				status = p_msbtn->button_up_function ( p_msbtn->player, p_msbtn->direction, p_msbtn->atari_key );
        		break;
        	case SDL_JOYBUTTONDOWN:
				p_btn = &g_pc_input.button[event.jbutton.which][event.jbutton.button];
				status = p_btn->button_down_function ( p_btn->player, p_btn->direction, p_btn->atari_key );
        		break;
        	case SDL_JOYBUTTONUP:
				p_btn = &g_pc_input.button[event.jbutton.which][event.jbutton.button];
				p_btn->button_up_function ( p_btn->player, p_btn->direction, p_btn->atari_key );
        		break;
				
		} /* end switch event type */
		
	} /* end check events */
    
    return status;

} /* end pc_poll_events */

/******************************************************************************
**  Function   :  pc_poll_debug
**                            
**  Objective  :  This function updates the screen during debug         
**
**  Parameters :  NONE
**                                                
**  return     :  return code (-1 to exit app)
**      
******************************************************************************/
int pc_poll_debug (void) {

	SDL_Event event;

	SDL_UpdateRect(g_pc_if.screen, 0, 0, 0, 0);
	
	while ( SDL_PollEvent(&event) ) {
		switch (event.type) {
			case SDL_ACTIVEEVENT:
				break;

			case SDL_QUIT: 
				return -1;
				break;
            
         	case SDL_KEYDOWN:
				if ( event.key.keysym.sym == SDLK_ESCAPE )
					return -1;
				break;
				
		} /* end switch event type */
		
	} /* end check events */
    
    return 0;

} /* end pc_poll_debug */

Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        return *p;

    case 2:
        return *(Uint16 *)p;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;

    case 4:
        return *(Uint32 *)p;

    default:
        return 0;    
    }
}

/******************************************************************************
**  Function   :  pc_convert_bitmap
**                            
**  Objective  :  This function converts a bitmap of colors to a palette file
**
**  Parameters :  old_file  - file of bitmap containing palette
**                new_file  - name of file to store value to
**                                                
**  return     :  Error code
**      
******************************************************************************/
int pc_convert_bitmap ( char *old_file, char *new_file ) {

	Uint8 r[100],g[100],b[100];
	Uint32 pixel;
	unsigned long red_avg, blue_avg, green_avg;
	SDL_Surface *bitmap;
	int i,x,y;
	int j,k,l,m;
	int pal[256];
	double x_size, y_size;

	bitmap = SDL_LoadBMP ( old_file );

	x_size = (double)bitmap->w/16.0;
	y_size = (double)bitmap->h/16.0;

	i = -1;
	for ( y = 0; y < 16; ++y ) {
		for ( x = 0; x < 16; ++x ) {
			l = 0;
			for ( j = 0; j < 12; j++ ) {
				for ( k = 0; k < 8; k++ ) {
					++l;
					pixel = getpixel ( bitmap, (int)(x_size*x+((x_size/2)-6)+j), (int)(y_size*y+((y_size/2)-4)+k) ); 
					SDL_GetRGB(pixel, bitmap->format, &r[l], &g[l], &b[l]);
				}
			}

			red_avg = 0;
			for ( m = 0; m < l; ++m )
				red_avg += r[m];
			red_avg = red_avg / l;
			green_avg = 0;
			for ( m = 0; m < l; ++m )
				green_avg += g[m];
			green_avg = green_avg / l;
			blue_avg = 0;
			for ( m = 0; m < l; ++m )
				blue_avg += b[m];
			blue_avg = blue_avg / l;
			pal[++i] = (red_avg<<16) + (green_avg<<8) + blue_avg;
		}
	}

	SDL_FreeSurface ( bitmap );

	save_palette ( new_file, pal );

	return 0;

} /* end pc_convert_bitmap */

/******************************************************************************
**  Function   :  pc_set_ui_defaults
**                            
**  Objective  :  This function sets up defaults for the ui keys
**
**  Parameters :  NONE
**                                                
**  return     :  NONE                             
**      
******************************************************************************/
void pc_set_ui_defaults ( void ) {

	int i;

	/*
	 * UI keys
	 */
	for ( i = 0; i < UI_KEY_MAX; ++i ) {
		g_ui.keys[i].device = DEV_KEYBOARD;
		g_ui.keys[i].part_type = PART_TYPE_KEY;
		g_ui.keys[i].direction = DIR_PRESS;
	}

	g_ui.keys[UI_KEY_EXIT].device_num = KMOD_ALT;
	g_ui.keys[UI_KEY_EXIT].part_num = SDLK_F4;

	g_ui.keys[UI_KEY_BACK].device_num = 0;
	g_ui.keys[UI_KEY_BACK].part_num = SDLK_ESCAPE;

	g_ui.keys[UI_KEY_FULLSCREEN].device_num = KMOD_ALT;
	g_ui.keys[UI_KEY_FULLSCREEN].part_num = SDLK_RETURN;

	g_ui.keys[UI_KEY_SCREENSHOT].device_num = KMOD_ALT;
	g_ui.keys[UI_KEY_SCREENSHOT].part_num = SDLK_s;

	for ( i = 0; i < 9; ++i ) {
		g_ui.keys[UI_KEY_LOAD_STATE_1 + i].device_num = KMOD_ALT;
		g_ui.keys[UI_KEY_LOAD_STATE_1 + i].part_num = SDLK_1 + i;
	}
	for ( i = 0; i < 9; ++i ) {
		g_ui.keys[UI_KEY_SAVE_STATE_1 + i].device_num = KMOD_SHIFT;
		g_ui.keys[UI_KEY_SAVE_STATE_1 + i].part_num = SDLK_1 + i;
	}

} /* end pc_set_ui_defaults */

/******************************************************************************
**  Function   :  pc_set_controller_defaults
**                            
**  Objective  :  This function sets up defaults for the controllers
**
**  Parameters :  NONE
**                                                
**  return     :  NONE                             
**      
******************************************************************************/
void pc_set_controller_defaults ( void ) {

	int i,j;
	t_config *p_config = config_get_ptr();

	if ( p_config->machine_type == MACHINE_TYPE_5200 )
		g_input.mouse_speed = 3;
	else
		g_input.mouse_speed = 1;

	g_input.players[0].control_type = CTRLR_TYPE_JOYSTICK;
	g_input.players[1].control_type = CTRLR_TYPE_JOYSTICK;
	g_input.players[2].control_type = CTRLR_TYPE_NONE;
	g_input.players[3].control_type = CTRLR_TYPE_NONE;

	for ( i = 0; i < MAX_DIR; ++i ) {
		for ( j = 0; j < MAX_CONTROLLER; ++j ) {
			g_input.players[j].stick.deadzone = 20;
			g_input.players[j].stick.sensitivity = 96;
			g_input.players[j].stick.simulate_analog = 0;
			g_input.players[j].paddles.deadzone = 20;
			g_input.players[j].paddles.sensitivity = 100;
			g_input.players[j].paddles.simulate_analog = 0;
		}
	}

	/*
	 * Keypad 1
	 */
	for ( i = 0; i < 0x10; ++i ) {
		g_input.players[0].keypad[i].device = DEV_KEYBOARD;
		g_input.players[0].keypad[i].device_num = 0;
		g_input.players[0].keypad[i].part_type = PART_TYPE_KEY;
		g_input.players[0].keypad[i].direction = DIR_PRESS;
	}
	g_input.players[0].keypad[KEY_0>>1].part_num = SDLK_KP0;
	g_input.players[0].keypad[KEY_1>>1].part_num = SDLK_KP1;
	g_input.players[0].keypad[KEY_2>>1].part_num = SDLK_KP2;
	g_input.players[0].keypad[KEY_3>>1].part_num = SDLK_KP3;
	g_input.players[0].keypad[KEY_4>>1].part_num = SDLK_KP4;
	g_input.players[0].keypad[KEY_5>>1].part_num = SDLK_KP5;
	g_input.players[0].keypad[KEY_6>>1].part_num = SDLK_KP6;
	g_input.players[0].keypad[KEY_7>>1].part_num = SDLK_KP7;
	g_input.players[0].keypad[KEY_8>>1].part_num = SDLK_KP8;
	g_input.players[0].keypad[KEY_9>>1].part_num = SDLK_KP9;
	g_input.players[0].keypad[KEY_START>>1].part_num = SDLK_F10;
	g_input.players[0].keypad[KEY_PAUSE>>1].part_num = SDLK_F11;
	g_input.players[0].keypad[KEY_RESET>>1].part_num = SDLK_F12;
	g_input.players[0].keypad[KEY_POUND>>1].part_num = SDLK_KP_DIVIDE;
	g_input.players[0].keypad[KEY_STAR>>1].part_num = SDLK_KP_MULTIPLY;

	/*
	 * Keypad 2
	 */
	for ( i = 0; i < 0x10; ++i ) {
		g_input.players[1].keypad[i].device = DEV_KEYBOARD;
		g_input.players[1].keypad[i].device_num = 0;
		g_input.players[1].keypad[i].part_type = PART_TYPE_KEY;
		g_input.players[1].keypad[i].direction = DIR_PRESS;
	}
	g_input.players[1].keypad[KEY_0>>1].part_num = SDLK_0;
	g_input.players[1].keypad[KEY_1>>1].part_num = SDLK_1;
	g_input.players[1].keypad[KEY_2>>1].part_num = SDLK_2;
	g_input.players[1].keypad[KEY_3>>1].part_num = SDLK_3;
	g_input.players[1].keypad[KEY_4>>1].part_num = SDLK_4;
	g_input.players[1].keypad[KEY_5>>1].part_num = SDLK_5;
	g_input.players[1].keypad[KEY_6>>1].part_num = SDLK_6;
	g_input.players[1].keypad[KEY_7>>1].part_num = SDLK_7;
	g_input.players[1].keypad[KEY_8>>1].part_num = SDLK_8;
	g_input.players[1].keypad[KEY_9>>1].part_num = SDLK_9;
	g_input.players[1].keypad[KEY_START>>1].part_num = SDLK_F3;
	g_input.players[1].keypad[KEY_PAUSE>>1].part_num = SDLK_F4;
	g_input.players[1].keypad[KEY_RESET>>1].part_num = SDLK_F5;
	g_input.players[1].keypad[KEY_POUND>>1].part_num = SDLK_TAB;
	g_input.players[1].keypad[KEY_STAR>>1].part_num = SDLK_q;

	/*
	 * Buttons
	 */
	for ( i = 0; i < MAX_CONTROLLER; ++i ) {
		g_input.players[i].top_button.device = DEV_KEYBOARD;
		g_input.players[i].bottom_button.device = DEV_KEYBOARD;

		g_input.players[i].top_button.device_num = 0;
		g_input.players[i].bottom_button.device_num = 0;

		g_input.players[i].top_button.part_type = PART_TYPE_KEY;
		g_input.players[i].bottom_button.part_type = PART_TYPE_KEY;

		if ( i == 0 ) {
			g_input.players[i].top_button.part_num = SDLK_RSHIFT;
			if ( p_config->machine_type == MACHINE_TYPE_5200 )
				g_input.players[i].bottom_button.part_num = SDLK_RETURN;
			else
				g_input.players[i].bottom_button.part_num = SDLK_KP_ENTER;
		}
		else if ( i == 1 ) {
			g_input.players[i].top_button.part_num = SDLK_LSHIFT;
			g_input.players[i].bottom_button.part_num = SDLK_LCTRL;
		}

		g_input.players[i].top_button.direction = DIR_PRESS;
		g_input.players[i].bottom_button.direction = DIR_PRESS;
	}

	/*
	 * Pots
	 */
	for ( i = 0; i < MAX_CONTROLLER; ++i ) {
		for ( j = 0; j < MAX_DIR; ++j ) {
			g_input.players[i].stick.direction[j].device = DEV_KEYBOARD;
			g_input.players[i].stick.direction[j].device_num = 0;
			g_input.players[i].stick.direction[j].part_type = PART_TYPE_KEY;
			g_input.players[i].stick.direction[j].direction = DIR_PRESS;
			g_input.players[i].paddles.direction[j].device = DEV_KEYBOARD;
			g_input.players[i].paddles.direction[j].device_num = 0;
			g_input.players[i].paddles.direction[j].part_type = PART_TYPE_KEY;
			g_input.players[i].paddles.direction[j].direction = DIR_PRESS;
		}
	}
	g_input.players[0].stick.direction[DIR_LEFT].part_num = SDLK_LEFT;
	g_input.players[0].stick.direction[DIR_RIGHT].part_num = SDLK_RIGHT;
	g_input.players[0].stick.direction[DIR_UP].part_num = SDLK_UP;
	g_input.players[0].stick.direction[DIR_DOWN].part_num = SDLK_DOWN;

	g_input.players[1].stick.direction[DIR_LEFT].part_num = SDLK_a;
	g_input.players[1].stick.direction[DIR_RIGHT].part_num = SDLK_d;
	g_input.players[1].stick.direction[DIR_UP].part_num = SDLK_w;
	g_input.players[1].stick.direction[DIR_DOWN].part_num = SDLK_s;

	g_input.players[0].paddles.direction[DIR_LEFT].part_num = SDLK_LEFT;
	g_input.players[0].paddles.direction[DIR_RIGHT].part_num = SDLK_RIGHT;
	g_input.players[0].paddles.direction[DIR_UP].part_num = SDLK_UP;
	g_input.players[0].paddles.direction[DIR_DOWN].part_num = SDLK_DOWN;

	g_input.players[1].paddles.direction[DIR_LEFT].part_num = SDLK_a;
	g_input.players[1].paddles.direction[DIR_RIGHT].part_num = SDLK_d;
	g_input.players[1].paddles.direction[DIR_UP].part_num = SDLK_w;
	g_input.players[1].paddles.direction[DIR_DOWN].part_num = SDLK_s;

} /* end pc_set_controller_defaults */

/******************************************************************************
**  Function   :  pc_set_keyboard_defaults
**                            
**  Objective  :  This function sets up defaults for the controllers
**
**  Parameters :  NONE
**                                                
**  return     :  NONE                             
**      
******************************************************************************/
void pc_set_keyboard_defaults ( void ) {

	int i;

	g_input.ctrl_key.device = DEV_KEYBOARD;
	g_input.ctrl_key.device_num = 0;
	g_input.ctrl_key.part_type = PART_TYPE_KEY;
	g_input.ctrl_key.direction = DIR_PRESS;
	g_input.ctrl_key.part_num = SDLK_LCTRL;

	g_input.shift_key.device = DEV_KEYBOARD;
	g_input.shift_key.device_num = 0;
	g_input.shift_key.part_type = PART_TYPE_KEY;
	g_input.shift_key.direction = DIR_PRESS;
	g_input.shift_key.part_num = SDLK_LSHIFT;

	g_input.reset_key.device = DEV_KEYBOARD;
	g_input.reset_key.device_num = 0;
	g_input.reset_key.part_type = PART_TYPE_KEY;
	g_input.reset_key.direction = DIR_PRESS;
	g_input.reset_key.part_num = SDLK_F8;

	g_input.break_key.device = DEV_KEYBOARD;
	g_input.break_key.device_num = 0;
	g_input.break_key.part_type = PART_TYPE_KEY;
	g_input.break_key.direction = DIR_PRESS;
	g_input.break_key.part_num = SDLK_BREAK;

	g_input.start_key.device = DEV_KEYBOARD;
	g_input.start_key.device_num = 0;
	g_input.start_key.part_type = PART_TYPE_KEY;
	g_input.start_key.direction = DIR_PRESS;
	g_input.start_key.part_num = SDLK_F10;

	g_input.select_key.device = DEV_KEYBOARD;
	g_input.select_key.device_num = 0;
	g_input.select_key.part_type = PART_TYPE_KEY;
	g_input.select_key.direction = DIR_PRESS;
	g_input.select_key.part_num = SDLK_F11;

	g_input.option_key.device = DEV_KEYBOARD;
	g_input.option_key.device_num = 0;
	g_input.option_key.part_type = PART_TYPE_KEY;
	g_input.option_key.direction = DIR_PRESS;
	g_input.option_key.part_num = SDLK_F12;

	for ( i = 0; i < 0x40; ++i ) {
		g_input.keyboard[i].device = DEV_KEYBOARD;
		g_input.keyboard[i].device_num = 0;
		g_input.keyboard[i].part_type = PART_TYPE_KEY;
		g_input.keyboard[i].direction = DIR_PRESS;
	}

	g_input.keyboard[A800_KEY_L].part_num = SDLK_l;
	g_input.keyboard[A800_KEY_J].part_num = SDLK_j;
	g_input.keyboard[A800_KEY_SEMI].part_num = SDLK_SEMICOLON;
	g_input.keyboard[A800_KEY_K].part_num = SDLK_k;
	g_input.keyboard[A800_KEY_PLUS].part_num = SDLK_KP_PLUS;
	g_input.keyboard[A800_KEY_STAR].part_num = SDLK_KP_MULTIPLY;
	g_input.keyboard[A800_KEY_O].part_num = SDLK_o;
	g_input.keyboard[A800_KEY_P].part_num = SDLK_p;
	g_input.keyboard[A800_KEY_U].part_num = SDLK_u;
	g_input.keyboard[A800_KEY_RETURN].part_num = SDLK_RETURN;
	g_input.keyboard[A800_KEY_I].part_num = SDLK_i;
	g_input.keyboard[A800_KEY_MINUS].part_num = SDLK_MINUS;
	g_input.keyboard[A800_KEY_EQUALS].part_num = SDLK_EQUALS;
	g_input.keyboard[A800_KEY_V].part_num = SDLK_v;
	g_input.keyboard[A800_KEY_HELP].part_num = SDLK_F9;
	g_input.keyboard[A800_KEY_C].part_num = SDLK_c;
	g_input.keyboard[A800_KEY_B].part_num = SDLK_b;
	g_input.keyboard[A800_KEY_X].part_num = SDLK_x;
	g_input.keyboard[A800_KEY_Z].part_num = SDLK_z;
	g_input.keyboard[A800_KEY_4].part_num = SDLK_4;
	g_input.keyboard[A800_KEY_3].part_num = SDLK_3;
	g_input.keyboard[A800_KEY_6].part_num = SDLK_6;
	g_input.keyboard[A800_KEY_ESC].part_num = SDLK_F2;
	g_input.keyboard[A800_KEY_5].part_num = SDLK_5;
	g_input.keyboard[A800_KEY_2].part_num = SDLK_2;
	g_input.keyboard[A800_KEY_1].part_num = SDLK_1;
	g_input.keyboard[A800_KEY_COMMA].part_num = SDLK_COMMA;
	g_input.keyboard[A800_KEY_SPACE].part_num = SDLK_SPACE;
	g_input.keyboard[A800_KEY_PERIOD].part_num = SDLK_PERIOD;
	g_input.keyboard[A800_KEY_N].part_num = SDLK_n;
	g_input.keyboard[A800_KEY_M].part_num = SDLK_m;
	g_input.keyboard[A800_KEY_SLASH].part_num = SDLK_SLASH;
	g_input.keyboard[A800_KEY_FUJI].part_num = SDLK_BACKQUOTE;
	g_input.keyboard[A800_KEY_R].part_num = SDLK_r;
	g_input.keyboard[A800_KEY_E].part_num = SDLK_e;
	g_input.keyboard[A800_KEY_Y].part_num = SDLK_y;
	g_input.keyboard[A800_KEY_TAB].part_num = SDLK_TAB;
	g_input.keyboard[A800_KEY_T].part_num = SDLK_t;
	g_input.keyboard[A800_KEY_W].part_num = SDLK_w;
	g_input.keyboard[A800_KEY_Q].part_num = SDLK_q;
	g_input.keyboard[A800_KEY_9].part_num = SDLK_9;
	g_input.keyboard[A800_KEY_0].part_num = SDLK_0;
	g_input.keyboard[A800_KEY_7].part_num = SDLK_7;
	g_input.keyboard[A800_KEY_BKSP].part_num = SDLK_BACKSPACE;
	g_input.keyboard[A800_KEY_8].part_num = SDLK_8;
	g_input.keyboard[A800_KEY_LESST].part_num = SDLK_LEFTBRACKET;
	g_input.keyboard[A800_KEY_MORET].part_num = SDLK_RIGHTBRACKET;
	g_input.keyboard[A800_KEY_F].part_num = SDLK_f;
	g_input.keyboard[A800_KEY_H].part_num = SDLK_h;
	g_input.keyboard[A800_KEY_D].part_num = SDLK_d;
	g_input.keyboard[A800_KEY_CAPS].part_num = SDLK_CAPSLOCK;
	g_input.keyboard[A800_KEY_G].part_num = SDLK_g;
	g_input.keyboard[A800_KEY_S].part_num = SDLK_s;
	g_input.keyboard[A800_KEY_A].part_num = SDLK_a;

} /* end pc_set_keyboard_defaults */

/******************************************************************************
**  Function   :  pc_check_for_lr_keys
**                            
**  Objective  :  This function checks for keys such as SHIFT and CTRL that
**                are on left and right side of keyboard and return the mate
**
**  Parameters :  key_value - key to check for 
**                                                
**  return     :  the key's mate on the other side
**      
******************************************************************************/
int pc_check_for_lr_keys ( int key_value ) {

	switch ( key_value ) {

		case SDLK_RSHIFT: return SDLK_LSHIFT; break;
		case SDLK_LSHIFT: return SDLK_RSHIFT; break;
		case SDLK_RCTRL:  return SDLK_LCTRL;  break;
		case SDLK_LCTRL:  return SDLK_RCTRL;  break;
		case SDLK_RALT:   return SDLK_LALT;   break;
		case SDLK_LALT:   return SDLK_RALT;   break;
		case SDLK_RMETA:  return SDLK_LMETA;  break;
		case SDLK_LMETA:  return SDLK_RMETA;  break;
		case SDLK_RSUPER: return SDLK_LSUPER; break;
		case SDLK_LSUPER: return SDLK_RSUPER; break;

		default: return 0; break;
	}

} /* pc_check_for_lf_keys */

/******************************************************************************
**  Function   :  pc_interpret_key
**                            
**  Objective  :  This function checks the string for the key desired
**                config file
**
**  Parameters :  string - string to check for key
**                                                
**  return     :  SDL key value                   
**      
******************************************************************************/
int pc_interpret_key ( char *string ) {

	int value = 0;

	/*
	 * Get rid of leading spaces
	 */
	while ( string[0] == ' ' )
		string += 1;

	/*
	 * Check for actual key value instead of string
	 */
	value = atoi ( string );
	if ( value > 9 )
		return value;

	/*
	 * Check for keypad
	 */
	if ( (string[0]=='K') && (string[1]=='P') ) {
		if ( strstr ( string, "MULTIPLY" ) )
			return SDLK_KP_MULTIPLY;
		else if ( strstr ( string, "DIVIDE" ) )
			return SDLK_KP_DIVIDE;
		else if ( strstr ( string, "PERIOD" ) )
			return SDLK_KP_PERIOD;
		else if ( strstr ( string, "MINUS" ) )
			return SDLK_KP_MINUS;
		else if ( strstr ( string, "PLUS" ) )
			return SDLK_KP_PLUS;
		else if ( strstr ( string, "ENTER" ) )
			return SDLK_KP_ENTER;
		else if ( strstr ( string, "EQUALS" ) )
			return SDLK_KP_EQUALS;
		else
			return SDLK_KP0 + atoi ( string+2 );
	}

	/*
	 * Check for arrows, home, end
	 */
	if ( strstr ( string, "UP" ) )
		return SDLK_UP;
	else if ( strstr ( string, "DOWN" ) )
		return SDLK_DOWN;
	else if ( strstr ( string, "RIGHT" ) )
		return SDLK_RIGHT;
	else if ( strstr ( string, "LEFT" ) )
		return SDLK_LEFT;
	else if ( strstr ( string, "INSERT" ) )
		return SDLK_INSERT;
	else if ( strstr ( string, "HOME" ) )
		return SDLK_HOME;
	else if ( strstr ( string, "END" ) )
		return SDLK_END;
	else if ( strstr ( string, "PAGE_UP" ) )
		return SDLK_PAGEUP;
	else if ( strstr ( string, "PAGE_DOWN" ) )
		return SDLK_PAGEDOWN;

	/*
	 * Check for function keys
	 */
	if ( (string[0] == 'F') && (strlen(string+1) > 0) ) {
		if ( atoi(string+1) )
			return (SDLK_F1-1) + atoi(string+1);
	}

	/*
	 * Check for state keys
	 */
	if ( strstr ( string, "NUMLOCK" ) )
		return SDLK_NUMLOCK;
	else if ( strstr ( string, "CAPSLOCK" ) )
		return SDLK_CAPSLOCK;
	else if ( strstr ( string, "SCROLLOCK" ) )
		return SDLK_SCROLLOCK;
	else if ( strstr ( string, "RSHIFT" ) )
		return SDLK_RSHIFT;
	else if ( strstr ( string, "LSHIFT" ) )
		return SDLK_LSHIFT;
	else if ( strstr ( string, "RCTRL" ) )
		return SDLK_RCTRL;
	else if ( strstr ( string, "LCTRL" ) )
		return SDLK_LCTRL;
	else if ( strstr ( string, "RALT" ) )
		return SDLK_RALT;
	else if ( strstr ( string, "LALT" ) )
		return SDLK_LALT;
	else if ( strstr ( string, "RMETA" ) )
		return SDLK_RMETA;
	else if ( strstr ( string, "LMETA" ) )
		return SDLK_LMETA;
	else if ( strstr ( string, "RSUPER" ) )
		return SDLK_RSUPER;
	else if ( strstr ( string, "LSUPER" ) )
		return SDLK_LSUPER;

	/*
	 * Check for other keys
	 */
	if ( strstr ( string, "BACKSPACE" ) )
		return SDLK_BACKSPACE;
	else if ( strstr ( string, "TAB" ) )
		return SDLK_TAB;
	else if ( strstr ( string, "ESC" ) )
		return SDLK_ESCAPE;
	else if ( strstr ( string, "CLEAR" ) )
		return SDLK_CLEAR;
	else if ( strstr ( string, "RETURN" ) )
		return SDLK_RETURN;
	else if ( strstr ( string, "SPACE" ) )
		return SDLK_SPACE;
	else if ( strstr ( string, "COMMA" ) )
		return SDLK_COMMA;
	else if ( strstr ( string, "MINUS" ) )
		return SDLK_MINUS;
	else if ( strstr ( string, "PLUS" ) )
		return SDLK_PLUS;
	else if ( strstr ( string, "EQUALS" ) )
		return SDLK_EQUALS;
	else if ( strstr ( string, "PERIOD" ) )
		return SDLK_PERIOD;
	else if ( strstr ( string, "SEMICOLON" ) )
		return SDLK_SEMICOLON;
	else if ( strstr ( string, "LEFTBRACKET" ) )
		return SDLK_LEFTBRACKET;
	else if ( strstr ( string, "RIGHTBRACKET" ) )
		return SDLK_RIGHTBRACKET;
	else if ( strstr ( string, "BACKSLASH" ) )
		return SDLK_BACKSLASH;
	else if ( strstr ( string, "SLASH" ) )
		return SDLK_SLASH;
	else if ( strstr ( string, "DELETE" ) )
		return SDLK_DELETE;
	else if ( strstr ( string, "BREAK" ) )
		return SDLK_BREAK;

	/*
	 * Check for numbers
	 */
	if ( atoi(string) || string[0] == '0' ) {
		return atoi(string) + SDLK_0;
	}

	/*
	 * Check for letters
	 */
	if ( string[0] >= 'A' && string[0] <= 'Z' )
		string[0] = tolower ( string[0] );

	return string[0];

} /* pc_interpret_key */

/******************************************************************************
**  Function   :  pc_interpret_key_value
**                            
**  Objective  :  This function checks the input value and outputs the key name
**
**  Parameters :  value  - SDL key value to get name for
**                string - Output containing name
**                                                
**  return     :  string                          
**      
******************************************************************************/
char * pc_interpret_key_value ( int value, char *string ) {

	*string = '\0';

	sprintf ( string, "%d", value );

	/*
	 * Check for keypad
	 */
	if ( (value >= SDLK_KP0) && (value <= SDLK_KP9) ) {
		sprintf ( string, "KP%d", value - SDLK_KP0 );
	}

	if ( value == SDLK_KP_MULTIPLY )
		return strcpy ( string, "KP_MULTIPLY" );
	else if ( value == SDLK_KP_DIVIDE )
		return strcpy ( string, "KP_DIVIDE" );
	else if ( value == SDLK_KP_PERIOD )
		return strcpy ( string, "KP_PERIOD" );
	else if ( value == SDLK_KP_MINUS )
		return strcpy ( string, "KP_MINUS" );
	else if ( value == SDLK_KP_PLUS )
		return strcpy ( string, "KP_PLUS" );
	else if ( value == SDLK_KP_ENTER )
		return strcpy ( string, "KP_ENTER" );
	else if ( value == SDLK_KP_EQUALS )
		return strcpy ( string, "KP_EQUALS" );

	/*
	 * Check for arrows, home, end
	 */
	if ( value == SDLK_UP )
		return strcpy ( string, "UP" );
	else if ( value == SDLK_DOWN )
		return strcpy ( string, "DOWN" );
	else if ( value == SDLK_RIGHT )
		return strcpy ( string, "RIGHT" );
	else if ( value == SDLK_LEFT )
		return strcpy ( string, "LEFT" );
	else if ( value == SDLK_INSERT )
		return strcpy ( string, "INSERT" );
	else if ( value == SDLK_HOME )
		return strcpy ( string, "HOME" );
	else if ( value == SDLK_END )
		return strcpy ( string, "END" );
	else if ( value == SDLK_PAGEUP )
		return strcpy ( string, "PAGE_UP" );
	else if ( value == SDLK_PAGEDOWN )
		return strcpy ( string, "PAGE_DOWN" );

	/*
	 * Check for function keys
	 */
	if ( (value >= SDLK_F1) && (value <= SDLK_F15) ) {
		sprintf ( string, "F%d", value-SDLK_F1+1 );
		return string;
	}

	/*
	 * Check for state keys
	 */
	if ( value == SDLK_NUMLOCK )
		return strcpy ( string, "NUMLOCK" );
	else if ( value == SDLK_CAPSLOCK )
		return strcpy ( string, "CAPSLOCK" );
	else if ( value == SDLK_SCROLLOCK )
		return strcpy ( string, "SCROLLOCK" );
	else if ( value == SDLK_RSHIFT )
		return strcpy ( string, "RSHIFT" );
	else if ( value == SDLK_LSHIFT )
		return strcpy ( string, "LSHIFT" );
	else if ( value == SDLK_RCTRL )
		return strcpy ( string, "RCTRL" );
	else if ( value == SDLK_LCTRL )
		return strcpy ( string, "LCTRL" );
	else if ( value == SDLK_RALT )
		return strcpy ( string, "RALT" );
	else if ( value == SDLK_LALT )
		return strcpy ( string, "LALT" );
	else if ( value == SDLK_RMETA )
		return strcpy ( string, "RMETA" );
	else if ( value == SDLK_LMETA )
		return strcpy ( string, "LMETA" );
	else if ( value == SDLK_RSUPER )
		return strcpy ( string, "RSUPER" );
	else if ( value == SDLK_LSUPER )
		return strcpy ( string, "LSUPER" );

	/*
	 * Check for other keys
	 */
	if ( value == SDLK_BACKSPACE )
		return strcpy ( string, "BACKSPACE" );
	if ( value == SDLK_TAB )
		return strcpy ( string, "TAB" );
	if ( value == SDLK_ESCAPE )
		return strcpy ( string, "ESC" );
	if ( value == SDLK_CLEAR )
		return strcpy ( string, "CLEAR" );
	if ( value == SDLK_RETURN )
		return strcpy ( string, "RETURN" );
	if ( value == SDLK_SPACE )
		return strcpy ( string, "SPACE" );
	if ( value == SDLK_COMMA )
		return strcpy ( string, "COMMA" );
	if ( value == SDLK_MINUS )
		return strcpy ( string, "MINUS" );
	if ( value == SDLK_PLUS )
		return strcpy ( string, "PLUS" );
	if ( value == SDLK_EQUALS )
		return strcpy ( string, "EQUALS" );
	if ( value == SDLK_PERIOD )
		return strcpy ( string, "PERIOD" );
	if ( value == SDLK_SEMICOLON )
		return strcpy ( string, "SEMICOLON" );
	if ( value == SDLK_LEFTBRACKET )
		return strcpy ( string, "LEFTBRACKET" );
	if ( value == SDLK_RIGHTBRACKET )
		return strcpy ( string, "RIGHTBRACKET" );
	if ( value == SDLK_BACKSLASH )
		return strcpy ( string, "BACKSLASH" );
	if ( value == SDLK_SLASH )
		return strcpy ( string, "SLASH" );
	if ( value == SDLK_DELETE )
		return strcpy ( string, "DELETE" );
	if ( value == SDLK_BREAK )
		return strcpy ( string, "BREAK" );

	/*
	 * Check for numbers
	 */
	if ( (value >= SDLK_0) && (value <= SDLK_9) ) {
		sprintf ( string, "%d", value-SDLK_0 );
		return string;
	}

	/*
	 * Check for letters
	 */
	if ( (value >= SDLK_a) && (value <= SDLK_z) ) {
		sprintf ( string, "%c", value );
		return string;
	}

	return string;

} /* pc_interpret_key_value */

/******************************************************************************
**  Function   :  pc_interpret_mod_key
**                            
**  Objective  :  This function checks the string for the key desired
**                config file
**
**  Parameters :  string - string to check for key
**                                                
**  return     :  SDL key value                   
**      
******************************************************************************/
int pc_interpret_mod_key ( char *string ) {

	int value = 0;

	/*
	 * Get rid of leading spaces
	 */
	while ( string[0] == ' ' )
		string += 1;

	/*
	 * Check for actual key value instead of string
	 */
	value = atoi ( string );
	if ( value > 9 )
		return value;

	/*
	 * Check for state keys
	 */
	if ( strstr ( string, "SHIFT" ) )
		return KMOD_SHIFT;
	else if ( strstr ( string, "CTRL" ) )
		return KMOD_CTRL;
	else if ( strstr ( string, "ALT" ) )
		return KMOD_ALT;
	else if ( strstr ( string, "META" ) )
		return KMOD_META;
	else if ( !strcmp ( string, "0" ) )
		return 0;

	return 0;

} /* pc_interpret_mod_key */

/******************************************************************************
**  Function   :  pc_interpret_mod_key_value
**                            
**  Objective  :  This function checks the input value and outputs the key name
**
**  Parameters :  value  - SDL key value to get name for
**                string - Output containing name
**                                                
**  return     :  string                          
**      
******************************************************************************/
char * pc_interpret_mod_key_value ( int value, char *string ) {

	*string = '\0';

	sprintf ( string, "%d", value );

	if ( value == KMOD_SHIFT )
		return strcpy ( string, "SHIFT" );
	else if ( value == KMOD_CTRL )
		return strcpy ( string, "CTRL" );
	else if ( value == KMOD_ALT )
		return strcpy ( string, "ALT" );
	else if ( value == KMOD_META )
		return strcpy ( string, "META" );
	else
		return strcpy ( string, "0" );

	return string;

} /* pc_interpret_mod_key_value */
