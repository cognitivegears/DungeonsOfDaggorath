/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: oslink.h
//
// This class manages the SDL operations, which abstract
// the link to the operating system.  By keeping these
// separate, it will be somewhat easier to change to a
// different library if necessary.

#ifndef OS_LINK_HEADER
#define OS_LINK_HEADER

#include "dod.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>


 // Arbitrary Length of 80, maybe be changed if needed
#define MAX_FILENAME_LENGTH 80

// Define a main game loop to handle set_main_game_loop_arg
void main_game_loop(void* arg);

class OS_Link
{
public:
	// Constructor
	OS_Link();

	// Public Interface
	void init();			// main entry point for dod application
	void quitSDL(int code);	// shuts down SDL before exiting
	void process_events();	// used mainly to retrieve keystrokes
	bool main_menu();       // used to implement the meta-menu
	bool saveOptFile(void);
    void send_input(char * keys); // Send keys from external interfaces
    void stop_demo(); // Stop the demo / start game
    void trigger_menu(); // Open in-game menu
    void render(void);
    bool menuReturn(int, int, menu); // Non-blocking wrapper for main menu handling
	const std::string& getBuildVersion() const;
	const std::string& getBuildTimestamp() const;
	const std::string& getBuildInfo() const;

	// Menu state for non-blocking menus
	enum MenuPendingType {
		MENU_PENDING_NONE,
		MENU_PENDING_LIST,
		MENU_PENDING_SCROLLBAR,
		MENU_PENDING_STRING
	};
	MenuPendingType menuPending;
	int menuPendingId;    // Which menu ID triggered the pending
	int menuPendingItem;  // Which item triggered the pending

	// Public Data Fields
	int		width;	// actual screen width after video setup
	int		height;	// same for height
	int     volumeLevel; // Volume level
	int		creatureRegen; // Creature Regen Speed

	char	gamefile[50];
	int		gamefileLen;
	char	pathSep[2];
	FILE *	fptr;
	char	confDir[5];
	char	soundDir[6];
	char	savedDir[MAX_FILENAME_LENGTH + 1];
	dodBYTE	keys[256];
	int		keylayout;	// 0 = QWERTY, 1 = Dvorak
	int		keyLen;

	int		audio_rate;
	Uint16	audio_format;
	int		audio_channels;
	int		audio_buffers;

	SDL_Window * sdlWindow;

private:
	// Internal Implementation
	void handle_key_down(SDL_Keysym * keysym);	// keyboard handler
	bool menu_return(int, int, menu);		// Used by main menu
	//int  menu_list(int x, int y, char *title, char *list[], int listSize);
	int  menu_list(int x, int y, char *title, std::string list[], int listSize);
	void menu_string(char *newString, char *title, int maxLength);
	//int  menu_scrollbar(char *title, int min, int max, int current);
	int  menu_scrollbar(std::string title, int min, int max, int current);
	void loadOptFile(void);
	void loadDefaults(void);
	void changeFullScreen(void);
	void changeVideoRes(int newWidth);


	SDL_GLContext sdlGlContext;

	// Data Fields
	int  bpp;	    // bits per pixel
	int  flags;	    // SDL flags
	bool FullScreen;    // FullScreen
	std::string buildVersion;
	std::string buildTimestamp;
	std::string buildInfo;
};

#endif // OS_LINK_HEADER
