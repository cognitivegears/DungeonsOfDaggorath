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
// Filename: oslink.cpp
//
// Implementation of OS_Link class

#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

using namespace std;

#include "oslink.h"
#include "dodgame.h"
#include "viewer.h"
#include "sched.h"
#include "player.h"
#include "dungeon.h"
#include "parser.h"
#include "object.h"
#include "creature.h"
#include "enhanced.h"

extern Creature		creature;
extern Object		object;
extern Dungeon		dungeon;
extern Player		player;
extern Coordinate	crd;
extern Viewer		viewer;
extern dodGame		game;
extern Scheduler	scheduler;
extern Parser		parser;

// Constructor
OS_Link::OS_Link() : width(0), height(0), bpp(0), flags(0),
					 audio_rate(44100), audio_format(AUDIO_S16),
					 audio_channels(2), audio_buffers(512),
					 gamefileLen(50), keylayout(0), keyLen(256)
{
#define MACOSX
#ifdef MACOSX
	strcpy(pathSep,"/");
#else
	strcpy(pathSep,"\\");
#endif

	strcpy(confDir, "conf");
	strcpy(soundDir, "sound");
	strcpy(savedDir, "saved");
	memset(gamefile,0,gamefileLen);
}

void OS_Link::render() {
//    std::cout << "In render" << std::endl;
	bool handle_res = scheduler.SCHED();
//    std::cout << "after scheduler" << std::endl;
    if(handle_res) { 
	    if (scheduler.ZFLAG == 0xFF)
	    {
		    game.LoadGame();
		    scheduler.ZFLAG = 0;
	    }
	    else
	    {
		    if (game.AUTFLG)
		    {
			    if (game.demoRestart)
			    {
				    // Restart demo
				    game.hasWon = false;
				    game.DEMOPTR = 0;
				    object.Reset();
				    creature.Reset();
				    parser.Reset();
				    player.Reset();
				    scheduler.Reset();
				    viewer.Reset();
				    dungeon.VFTPTR = 0;
				    game.COMINI();
			    }
			    else
			    {
				    // Start new game
				    game.AUTFLG = false;
				    game.Restart();
			    }
		    }
		    else
		    {
			    game.Restart();
		    }
	    }
    }
}

void main_game_loop(void* arg) {
    // For emscripten, we don't care about the frame rate, we care about
    // not interrupting an existing run - otherwise we might not run everything
    // in a single frame and Daggorath runs synchronously.
    emscripten_pause_main_loop();
    static_cast<OS_Link*>(arg)->render();
    emscripten_resume_main_loop();
}

static void myError(GLenum error) {
    std::cout << "Regal error: " << glErrorStringREGAL(error) << std::endl;
}

// This routine will eventually need updated to allow
// user customization of screen size and resolution.
// It currently asks for an 1024x768 screen size.
// Updated - Now defaults to whatever is in the opts.ini file
// if opts.ini doesn't exist or has invalid or missing values
// uses defaults set by loadDefaults function (1024x768)
void OS_Link::init()
{
	loadOptFile();

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
	{
		fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
		quitSDL(1);
	}

	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers))
	{
		fprintf(stderr, "Unable to open audio!\n");
		quitSDL(1);
	}

	creature.LoadSounds();
	object.LoadSounds();
	scheduler.LoadSounds();
	player.LoadSounds();

	int allocatedChannels = Mix_AllocateChannels(4);
	if (allocatedChannels < 0)
	{
		allocatedChannels = 0;
	}
	scheduler.ConfigureChannelSync(allocatedChannels);
	Mix_Volume(-1, MIX_MAX_VOLUME);
	if(FullScreen == 0){
		sdlWindow = SDL_CreateWindow("DOD", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, (int) (width * 0.75),SDL_WINDOW_OPENGL);
	}else{
		sdlWindow = SDL_CreateWindow("DOD", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, (int) (width * 0.75),SDL_WINDOW_FULLSCREEN|SDL_WINDOW_OPENGL);
	}

	if(sdlWindow == 0){
		fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
		quitSDL(1);
	}

    // Error callback
    RegalSetErrorCallback(myError);

	sdlGlContext = SDL_GL_CreateContext(sdlWindow);

    // TRY THIS
    RegalMakeCurrent((RegalSystemContext)1);


	if(sdlGlContext == 0){
		fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
		quitSDL(1);
	}
//    std::cout << "After GL context" << std::endl;
	//bpp = info->vfmt->BitsPerPixel;
// TODO: ARE THESE NEEDED
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
//	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
//    SDL_GL_SetSwapInterval(0);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	//flags = SDL_OPENGL;

	changeVideoRes(width); // All changing video res code was moved here
	SDL_SetWindowTitle(sdlWindow, "Dungeons of Daggorath");

//    std::cout << "After video res" << std::endl;
	memset(keys, parser.C_SP, keyLen);

	if (keylayout == 0) // QWERTY
	{
		keys[SDLK_a] = 'A';
		keys[SDLK_b] = 'B';
		keys[SDLK_c] = 'C';
		keys[SDLK_d] = 'D';
		keys[SDLK_e] = 'E';
		keys[SDLK_f] = 'F';
		keys[SDLK_g] = 'G';
		keys[SDLK_h] = 'H';
		keys[SDLK_i] = 'I';
		keys[SDLK_j] = 'J';
		keys[SDLK_k] = 'K';
		keys[SDLK_l] = 'L';
		keys[SDLK_m] = 'M';
		keys[SDLK_n] = 'N';
		keys[SDLK_o] = 'O';
		keys[SDLK_p] = 'P';
		keys[SDLK_q] = 'Q';
		keys[SDLK_r] = 'R';
		keys[SDLK_s] = 'S';
		keys[SDLK_t] = 'T';
		keys[SDLK_u] = 'U';
		keys[SDLK_v] = 'V';
		keys[SDLK_w] = 'W';
		keys[SDLK_x] = 'X';
		keys[SDLK_y] = 'Y';
		keys[SDLK_z] = 'Z';
		keys[SDLK_BACKSPACE] = parser.C_BS;
		keys[SDLK_RETURN] = parser.C_CR;
		keys[SDLK_SPACE] = parser.C_SP;
	}
	else if (keylayout == 1) // Dvorak
	{
		keys[SDLK_a] = 'A';
		keys[SDLK_n] = 'B';
		keys[SDLK_i] = 'C';
		keys[SDLK_h] = 'D';
		keys[SDLK_d] = 'E';
		keys[SDLK_y] = 'F';
		keys[SDLK_u] = 'G';
		keys[SDLK_j] = 'H';
		keys[SDLK_g] = 'I';
		keys[SDLK_c] = 'J';
		keys[SDLK_v] = 'K';
		keys[SDLK_p] = 'L';
		keys[SDLK_m] = 'M';
		keys[SDLK_l] = 'N';
		keys[SDLK_s] = 'O';
		keys[SDLK_r] = 'P';
		keys[SDLK_x] = 'Q';
		keys[SDLK_o] = 'R';
		keys[SDLK_SEMICOLON] = 'S';
		keys[SDLK_k] = 'T';
		keys[SDLK_f] = 'U';
		keys[SDLK_PERIOD] = 'V';
		keys[SDLK_COMMA] = 'W';
		keys[SDLK_b] = 'X';
		keys[SDLK_t] = 'Y';
		keys[SDLK_SLASH] = 'Z';
		keys[SDLK_BACKSPACE] = parser.C_BS;
		keys[SDLK_RETURN] = parser.C_CR;
		keys[SDLK_SPACE] = parser.C_SP;
	}

//    std::cout << "After keys" << std::endl;
    // Wait for modes to change
    emscripten_sleep(2500);

    std::cout << "After sleep" << std::endl;
	game.COMINI();

    std::cout << "After COMINI" << std::endl;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_game_loop, this, 0, 0);
#else
    while(1) {
        main_game_loop(this);
    }
#endif
//    std::cout << "End of init" << std::endl;
}



// Used to check for keystrokes and application termination
void OS_Link::process_events()
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
		case SDL_KEYDOWN:
			handle_key_down(&event.key.keysym);
			break;
		case SDL_QUIT:
			quitSDL(0);
			break;
		case SDL_WINDOWEVENT_EXPOSED:
			SDL_GL_SwapWindow(sdlWindow);
			break;
		}
	}
}

// Quits application
void OS_Link::quitSDL(int code)
{
	Mix_CloseAudio();
	SDL_Quit();
	exit(code);
}

void OS_Link::send_input(char * keys) {
    for(int i=0; keys[i] != '\0'; i++) {
        parser.KBDPUT((dodBYTE)keys[i]);
    }
}

void OS_Link::stop_demo() {
    game.hasWon = true;
    game.demoRestart = false;
}

// Processes key strokes.
void OS_Link::handle_key_down(SDL_Keysym * keysym)
{
    dodBYTE c;
    if (viewer.display_mode == Viewer::MODE_MAP)
    {
        switch(keysym->sym)
        {
        case SDLK_ESCAPE:
            main_menu();
            break;
        default:
            viewer.display_mode = Viewer::MODE_3D;
            --viewer.UPDATE;
            parser.KBDPUT(32); // This is a (necessary ???) hack.
            break;
        }
    }
    else
    {
        switch(keysym->sym)
        {
        case SDLK_q:
        case SDLK_w:
        case SDLK_e:
        case SDLK_r:
        case SDLK_t:
        case SDLK_y:
        case SDLK_u:
        case SDLK_i:
        case SDLK_o:
        case SDLK_p:
        case SDLK_a:
        case SDLK_s:
        case SDLK_d:
        case SDLK_f:
        case SDLK_g:
        case SDLK_h:
        case SDLK_j:
        case SDLK_k:
        case SDLK_l:
        case SDLK_z:
        case SDLK_x:
        case SDLK_c:
        case SDLK_v:
        case SDLK_b:
        case SDLK_n:
        case SDLK_m:
        case SDLK_BACKSPACE:
        case SDLK_RETURN:
        case SDLK_SPACE:
            c = keys[keysym->sym];
            break;

        case SDLK_ESCAPE:
            main_menu();
            return;

        default:
            return;
        }
        parser.KBDPUT(c);
    }
}


/*********************************************************
  Member: main_menu

  Function: Implements the menu, and dispatches commands

  Returns:  true  - If a new game is started
            false - otherwise
*********************************************************/
bool OS_Link::main_menu()
{
 bool end = false;
 static int row = 0, col = 0;
 static menu mainMenu;

 scheduler.pause(true);
 viewer.drawMenu(mainMenu, col, row);

 do
   {
   SDL_Event event;
   while(SDL_PollEvent(&event))
   {
   switch(event.type)
     {
     case SDL_KEYDOWN:
      switch(event.key.keysym.sym)
        {
        case SDLK_RETURN:
	 end = menu_return(col, row, mainMenu);

	   // Used for Wizard fade functions, if it's a new game, it will trigger a key press
	 if(col == FILE_MENU_SWITCH && row == FILE_MENU_NEW)
	   return true;

         break;
        case SDLK_UP:
	 (row < 1) ? row = mainMenu.getMenuSize(col) - 1 : row--;
         break;
        case SDLK_DOWN:
	 (row > mainMenu.getMenuSize(col) - 2) ? row = 0 : row++;
         break;
       case SDLK_LEFT:
         if (NUM_MENU > 1)
          {
	 (col < 1) ? col = NUM_MENU - 1 : col--;
	 row = 0;
          }
         break;
        case SDLK_RIGHT:
         if (NUM_MENU > 1)
          {
	 (col > NUM_MENU - 2) ? col = 0 : col++;
	 row = 0;
          }
	 break;
        case SDLK_ESCAPE:
         end = true;
	 break;
        default:
	 break;
        }
      viewer.drawMenu(mainMenu, col, row);
      break;
     case SDL_QUIT:
      quitSDL(0);
      break;
     case SDL_WINDOWEVENT_EXPOSED:
		SDL_GL_SwapWindow(sdlWindow);
      break;
      }
     }
   emscripten_sleep(1);
  } while(!end);

  scheduler.pause(false);

  return false;
}

/* Function to process menu commands
*
*  Returns:  false - if menu should be redrawn
*            true  - otherwise */

bool OS_Link::menu_return(int menu_id, int item, menu Menu)
{
 switch(menu_id)
 {
  case FILE_MENU_SWITCH:
   switch(item)
   {
   case FILE_MENU_NEW:
    scheduler.pause(false);
    if(!game.AUTFLG)
    {
     game.hasWon = true;
     game.demoRestart = false;
    }
    return true;

   case FILE_MENU_RETURN:
    return true;

   case FILE_MENU_GRAPHICS:
    {
     std::string menuList[]={ "NORMAL GRAPHICS", "HIRES GRAPHICS", "VECTOR GRAPHICS" };

     switch(menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item), menuList, 3))
     {
     case 0:
      g_options &= ~(OPT_VECTOR|OPT_HIRES);
      break;
     case 1:
      g_options &= ~(OPT_VECTOR);
      g_options |= OPT_HIRES;
      break;
     case 2:
      g_options &= ~(OPT_HIRES);
      g_options |= OPT_VECTOR;
      break;
     default:
      return false;
     }
    }
    return true;

   case FILE_MENU_CREATURE_SPEED:
    {
     int newSpeed = menu_scrollbar("CREATURE SPEED", 50, 300, creature.creSpeedMul);
     if (newSpeed != creature.creSpeedMul)
     {
      creature.creSpeedMul = newSpeed;
      creature.UpdateCreSpeed();
     }
    }
    return false;

   case FILE_MENU_TURN_DELAY:
    {
     int newDelay = menu_scrollbar("TURN DELAY", 10, 200, player.turnDelay);
     player.turnDelay = newDelay;
    }
    return false;

   case FILE_MENU_MOVE_DELAY:
    {
     int newDelay = menu_scrollbar("MOVE DELAY", 100, 1000, player.moveDelay);
     player.moveDelay = newDelay;
    }
    return false;

   case FILE_MENU_CREATURE_REGEN:
    {
     int newRegen = menu_scrollbar("CREATURE REGEN (MIN)", 1, 10, creatureRegen);
     if (newRegen != creatureRegen)
     {
      creatureRegen = newRegen;
      scheduler.updateCreatureRegen(creatureRegen);
     }
    }
    return false;

   case FILE_MENU_VOLUME:
    {
     volumeLevel = menu_scrollbar("VOLUME LEVEL", 0, 128, volumeLevel);
     Mix_Volume(-1, volumeLevel);
    }
    return false;

   case FILE_MENU_RANDOM_MAZE:
    {
     std::string menuList[]={ "ON", "OFF" };
     switch(menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item), menuList, 2))
     {
     case 0:
      game.RandomMaze = true;
      break;
     case 1:
      game.RandomMaze = false;
      break;
     default:
      return false;
     }
    }
    return false;

   case FILE_MENU_SND_MODE:
    {
     std::string menuList[2] = {"STEREO", "MONO"};
     switch(menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item), menuList, 2))
     {
     case 0:
      g_options |= OPT_STEREO;
      break;
     case 1:
      g_options &= ~OPT_STEREO;
      break;
     default:
      return false;
     }
    }
    return false;

   case FILE_MENU_SAVE_OPT:
    saveOptFile();
    return true;

   case FILE_MENU_DEFAULTS:
    loadDefaults();
    changeVideoRes(width);
    return true;

   default:
    break;
   }
   break;

  default:
   break;
 }
 return true;
}


/*****************************************************************************
*  Function used to draw a list, move among that list, and return the item selected
*
*  Arguments: x        - The top-left x-coordinate to draw list at
*             y        - The top-left y-coordinate to draw list at
*             title    - The title of the list
*             list     - An array of strings (the list to be chosen from
*             listSize - The size of the array
******************************************************************************/

int OS_Link::menu_list(int x, int y, char *title, std::string list[], int listSize)
 {
 int currentChoice = 0;

 while(true)
   {
   viewer.drawMenuList(x, y, title, list, listSize, currentChoice);
   SDL_Event event;
   while(SDL_PollEvent(&event))
   {
   switch(event.type)
     {
     case SDL_KEYDOWN:
      switch(event.key.keysym.sym)
        {
        case SDLK_RETURN:
         return(currentChoice);
         break;

        case SDLK_UP:
	 (currentChoice < 1) ? currentChoice = listSize - 1 : currentChoice--;
         break;

        case SDLK_DOWN:
	 (currentChoice > listSize - 2) ? currentChoice = 0 : currentChoice++;
         break;

        case SDLK_ESCAPE:
	 return(-1);
	 break;

        default:
	 break;
        }
      break;
     case SDL_QUIT:
      quitSDL(0);
      break;
     case SDL_WINDOWEVENT_EXPOSED:
		SDL_GL_SwapWindow(sdlWindow);
      break;
      }
     }
     emscripten_sleep(1);
  } // End of while loop

 return(-1);
 }

/*****************************************************************************
*  Function used to draw a scrollbar, and return the value
*
*  Arguments: title     - The title of the entry
*             min       - The minimum value the scroll bar can take
*             max       - The maximum value the scroll bar can take
*             current   - The current position of the scrollbar
*
*  Returns: The value the user entered, or if they hit escape, the original
*           value.
******************************************************************************/

int OS_Link::menu_scrollbar(std::string title, int min, int max, int current)
 {
 int oldvalue  = current; //Save the old value in case the user escapes
 int range = max - min;
 if (range <= 0)
   {
   return current;
   }

int position = static_cast<int>(std::round((static_cast<double>(current - min) * 31.0) / static_cast<double>(range)));
position = std::max(0, std::min(31, position));
 int originalPosition = position;

 viewer.drawMenuScrollbar(title, position);

 while(true)
   {
   SDL_Event event;

   while(SDL_PollEvent(&event))
    {
    switch(event.type)
      {
      case SDL_KEYDOWN:
       switch(event.key.keysym.sym)
        {
        case SDLK_RETURN:
         {
         if (position == originalPosition)
           {
           return oldvalue;
           }
         int result = static_cast<int>(std::round(static_cast<double>(min) + (static_cast<double>(position) * static_cast<double>(range) / 31.0)));
         result = std::max(min, std::min(max, result));
         return result;
         }
         break;

      case SDLK_LEFT:
	 position = (position > 0) ? position - 1 : 0;
         break;

      case SDLK_RIGHT:
	 position = (position < 31) ? position + 1 : 31;
         break;

      case SDLK_ESCAPE:
	 return(oldvalue);
	 break;

        default:
	 break;
	}
       viewer.drawMenuScrollbar(title, position);
       break;
      case SDL_QUIT:
       quitSDL(0);
       break;
      case SDL_WINDOWEVENT_EXPOSED:
		SDL_GL_SwapWindow(sdlWindow);
       break;
      }
    }
    emscripten_sleep(1);
   }
 }

/*****************************************************************************
*  Function used to draw a box for a string entry, then return it
*
*  Arguments: newString - The string to be returned
*             title     - The title of the entry
*             maxLength - The maximum size of the entry
******************************************************************************/
void OS_Link::menu_string(char *newString, char *title, int maxLength)
 {
 *newString = '\0';
 viewer.drawMenuStringTitle(title);
 viewer.drawMenuString(newString);

 while(true)
   {
   SDL_Event event;

   while(SDL_PollEvent(&event))
   {
   switch(event.type)
     {
     case SDL_KEYDOWN:
      switch(event.key.keysym.sym)
        {
        case SDLK_RETURN:
         return;
         break;

        case SDLK_RSHIFT:
        case SDLK_LSHIFT:
        case SDLK_RCTRL:
        case SDLK_LCTRL:
        case SDLK_RALT:
        case SDLK_LALT:
        case SDLK_RGUI:
        case SDLK_LGUI:
        //case SDLK_LSUPER:
        //case SDLK_RSUPER:
        case SDLK_MODE:
        case SDLK_APPLICATION:
        case SDLK_NUMLOCKCLEAR:
        case SDLK_CAPSLOCK:
        case SDLK_SCROLLLOCK:
        case SDLK_UP:
        case SDLK_DOWN:
          // ignore these keys
          break;

        case SDLK_BACKSPACE:
	case SDLK_LEFT:
	 if(strlen(newString) > 0)
	  {
	  *(newString + strlen(newString) - 1) = '\0';
          viewer.drawMenuStringTitle(title);  // Update with the new word
          viewer.drawMenuString(newString);
	  }
         break;

        case SDLK_ESCAPE:
         *(newString) = '\0';
	 return;
	 break;

        default:
	 if(strlen(newString) < maxLength)
	  {
	  *(newString + strlen(newString) + 1) = '\0';
	  *(newString + strlen(newString)) = keys[event.key.keysym.sym];
          viewer.drawMenuStringTitle(title);  // Update with the new word
          viewer.drawMenuString(newString);
	  }
	 break;
        }
      break;
     case SDL_QUIT:
      quitSDL(0);
      break;
     case SDL_WINDOWEVENT_EXPOSED:
		SDL_GL_SwapWindow(sdlWindow);
      break;
      }
     emscripten_sleep(1);
     }
    emscripten_sleep(1);
  } // End of while loop
 }

/******************************************************************************
*  Function used to load & parse options file
*
*  Arguments: None
******************************************************************************/
void OS_Link::loadOptFile(void)
 {
 char     inputString[80];
 char     fn[20];
 int      in;
 ifstream fin;
 char *   breakPoint;

 loadDefaults(); // In case some variables aren't in the opts file, and if no file exists

 sprintf(fn, "%s%s%s", confDir, pathSep, "opts.ini");

 fin.open(fn);
 if (!fin)
 {
 return;
 }

 fin >> inputString;
 while(fin)
  {
  breakPoint = strchr(inputString, '=');

   // Ignore strings that have no equals, or are only an equals, or have no end
  if(breakPoint || breakPoint == inputString || breakPoint == (inputString + strlen(inputString) - 1))
   {
   *(breakPoint) = '\0';
   breakPoint++;

   if(!strcmp(inputString, "creatureSpeed"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) creature.creSpeedMul = in;
    }
   else if(!strcmp(inputString, "turnDelay"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) player.turnDelay = in;
    }
   else if(!strcmp(inputString, "moveDelay"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) player.moveDelay = in;
    }
   else if(!strcmp(inputString, "keylayout"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) keylayout = in;
    }
   else if(!strcmp(inputString, "graphicsMode"))
    {
    if(!strcmp(breakPoint, "NORMAL"))
     g_options &= ~(OPT_VECTOR|OPT_HIRES);
    else if(!strcmp(breakPoint, "HIRES"))
     {
     g_options &= ~(OPT_VECTOR);
     g_options |= OPT_HIRES;
     }
    else if(!strcmp(breakPoint, "VECTOR"))
     {
     g_options &= ~(OPT_HIRES);
     g_options |= OPT_VECTOR;
     }
    }
   else if(!strcmp(inputString, "stereoMode"))
    {
    if(!strcmp(breakPoint, "STEREO"))
     g_options |= OPT_STEREO;
    else if(!strcmp(breakPoint, "MONO"))
     g_options &= ~OPT_STEREO;
    }
   else if(!strcmp(inputString, "volumeLevel"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) volumeLevel = in;
    }
   else if(!strcmp(inputString, "saveDirectory"))
    {
    strncpy(savedDir, "saved", MAX_FILENAME_LENGTH);
    }
   else if(!strcmp(inputString, "fullScreen"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) FullScreen = in;
    }
   else if(!strcmp(inputString, "screenWidth"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) width = in;
    }
   else if(!strcmp(inputString, "creatureRegen"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) creatureRegen = in;
    }
   else if(!strcmp(inputString, "RandomMaze"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.RandomMaze = in;
    }
   else if(!strcmp(inputString, "ShieldFix"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.ShieldFix = in;
    }
   else if(!strcmp(inputString, "VisionScroll"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.VisionScroll = in;
    }
   else if(!strcmp(inputString, "CreaturesIgnoreObjects"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.CreaturesIgnoreObjects = in;
    }
   else if(!strcmp(inputString, "CreaturesInstaRegen"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.CreaturesInstaRegen = in;
    }
   else if(!strcmp(inputString, "MarkDoorsOnScrollMaps"))
    {
    if (1==sscanf(breakPoint, "%d", &in)) game.MarkDoorsOnScrollMaps = in;
    }
   }

  fin >> inputString;
  }

 fin.close();
 scheduler.updateCreatureRegen(creatureRegen);
 creature.UpdateCreSpeed();
 }

/******************************************************************************
*  Function used to save the options file from current settings
*
*  Arguments: None
*
*  Returns:   true - file saved successfully, false - file not saved
******************************************************************************/
bool OS_Link::saveOptFile(void)
 {
 ofstream fout;
 char     fn[MAX_FILENAME_LENGTH];

 sprintf(fn, "%s%s%s", confDir, pathSep, "opts.ini");

 fout.open(fn);
 if(!fout)
   return false;

 fout << "creatureSpeed=" << creature.creSpeedMul << endl;
 fout << "turnDelay=" << player.turnDelay << endl;
 fout << "moveDelay=" << player.moveDelay << endl;
 fout << "keylayout=" << keylayout << endl;
 fout << "volumeLevel=" << volumeLevel << endl;
 fout << "saveDirectory=" << savedDir << endl;
 fout << "fullScreen=" << FullScreen << endl;
 fout << "screenWidth=" << width << endl;
 fout << "creatureRegen=" << creatureRegen << endl;

 fout << "graphicsMode=";
 if(g_options & OPT_VECTOR)
   fout << "VECTOR" << endl;
 else if(g_options & OPT_HIRES)
   fout << "HIRES" << endl;
 else
   fout << "NORMAL" << endl;

 fout << "stereoMode=";
 if(g_options & OPT_STEREO)
   fout << "STEREO" << endl;
 else
   fout << "MONO" << endl;

 fout << "RandomMaze=" << game.RandomMaze << endl;
 fout << "ShieldFix=" << game.ShieldFix << endl;
 fout << "VisionScroll=" << game.VisionScroll << endl;
 fout << "CreaturesIgnoreObjects=" << game.CreaturesIgnoreObjects << endl;
 fout << "CreaturesInstaRegen=" << game.CreaturesInstaRegen << endl;
 fout << "MarkDoorsOnScrollMaps=" << game.MarkDoorsOnScrollMaps << endl;

 fout.close();

 return true;
 }

/******************************************************************************
*  Function used to load the options file from current settings
*
*  Arguments: None
******************************************************************************/
void OS_Link::loadDefaults(void)
 {
 player.turnDelay = 37;
 player.moveDelay = 500;
 keylayout   = 0;
 volumeLevel = MIX_MAX_VOLUME;
 creature.creSpeedMul = 200;
 creature.UpdateCreSpeed();
 strcpy(savedDir, "saved");
 FullScreen = false;
 width = 1024;
 creatureRegen = 5;
 scheduler.updateCreatureRegen(creatureRegen);

 g_options &= ~(OPT_VECTOR|OPT_HIRES);
 g_options |= OPT_STEREO;
 }

/******************************************************************************
*  Function used to swap fullscreen mode
*
*  Arguments: None
******************************************************************************/
void OS_Link::changeFullScreen(void)
 {
 FullScreen = !FullScreen;
 changeVideoRes(width);
 }

/******************************************************************************
*  Function used to change the video resolution
*
*  Arguments: newWidth - The screen width to change to
******************************************************************************/
void OS_Link::changeVideoRes(int newWidth)
 {
 int newHeight;

 newHeight = (int) (newWidth * 0.75);

	SDL_SetWindowSize(sdlWindow, newWidth, newHeight);

	if(FullScreen){
		if(SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN) == 0){
			SDL_ShowCursor(SDL_DISABLE);
		}else{
			fprintf(stderr, "Window fullscreen failed: %s\n", SDL_GetError());
		}

	}else{
		if(SDL_SetWindowFullscreen(sdlWindow, 0) == 0){
			SDL_ShowCursor(SDL_ENABLE);
		}else{
			fprintf(stderr, "Window fullscreen failed: %s\n", SDL_GetError());
		}
	}
  width  = newWidth;
  height = newHeight;
  crd.setCurWH((double) width);

 viewer.setup_opengl();
 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();
 }
