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

#include <algorithm>
#include <cmath>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <iostream>

using namespace std;

#ifndef BUILD_VERSION
#define BUILD_VERSION "dev"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

namespace {
std::string collapseSpaces(std::string value) {
  bool previousSpace = false;
  std::string result;
  result.reserve(value.size());
  for (char ch : value) {
    if (ch == ' ') {
      if (!previousSpace) {
        result += ch;
        previousSpace = true;
      }
    } else {
      result += ch;
      previousSpace = false;
    }
  }
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  while (!result.empty() && result.front() == ' ') {
    result.erase(result.begin());
  }
  return result;
}

std::string sanitizeForMenu(std::string value) {
  for (char &ch : value) {
    unsigned char uchar = static_cast<unsigned char>(ch);
    if (std::islower(uchar)) {
      ch = static_cast<char>(std::toupper(uchar));
      continue;
    }
    if (ch == ':') {
      ch = '-';
      continue;
    }
    switch (ch) {
    case '(':
    case ')':
    case '[':
    case ']':
    case '@':
      ch = ' ';
      continue;
    default:
      break;
    }
    const bool allowed = (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                         ch == ' ' || ch == '.' || ch == '-' || ch == '_' ||
                         ch == '+' || ch == '!' || ch == '?' || ch == '/' ||
                         ch == '\\' || ch == '%' || ch == '<' || ch == '>' ||
                         ch == '{' || ch == '}';
    if (!allowed) {
      ch = ' ';
    }
  }
  return collapseSpaces(value);
}

std::string formatTimestampForMenu(const std::string &raw) {
  if (raw.empty() || raw == "UNKNOWN") {
    return std::string();
  }
  std::string formatted = raw;
  auto tPos = formatted.find('T');
  if (tPos != std::string::npos) {
    formatted[tPos] = ' ';
  }
  auto spacePos = formatted.find(' ');
  if (spacePos != std::string::npos) {
    auto dashPos = formatted.find('-', spacePos + 1);
    if (dashPos != std::string::npos) {
      formatted.erase(dashPos, 1);
    }
  }
  return sanitizeForMenu(formatted);
}
} // namespace

#include "creature.h"
#include "dodgame.h"
#include "dungeon.h"
#include "enhanced.h"
#include "object.h"
#include "oslink.h"
#include "parser.h"
#include "player.h"
#include "sched.h"
#include "viewer.h"

extern Creature creature;
extern Object object;
extern Dungeon dungeon;
extern Player player;
extern Coordinate crd;
extern Viewer viewer;
extern dodGame game;
extern Scheduler scheduler;
extern Parser parser;

// Constructor
OS_Link::OS_Link()
    : menuPending(MENU_PENDING_NONE), menuPendingId(0), menuPendingItem(0),
      width(0), height(0), bpp(0), flags(0), audio_rate(44100),
      audio_format(AUDIO_S16), audio_channels(2), audio_buffers(512),
  gamefileLen(50), keylayout(0), keyLen(256),
  buildVersion(sanitizeForMenu(BUILD_VERSION)),
  buildTimestamp(BUILD_TIMESTAMP) {
#define MACOSX
#ifdef MACOSX
  strcpy(pathSep, "/");
#else
  strcpy(pathSep, "\\");
#endif

  strcpy(confDir, "conf");
  strcpy(soundDir, "sound");
  strcpy(savedDir, "saved");
  memset(gamefile, 0, gamefileLen);
  memset(saveNameBuffer, 0, sizeof(saveNameBuffer));

  const std::string timestampForMenu = formatTimestampForMenu(buildTimestamp);
  buildInfo = sanitizeForMenu(buildVersion);
  if (!timestampForMenu.empty()) {
    if (!buildInfo.empty()) {
      buildInfo += " " + timestampForMenu;
    } else {
      buildInfo = timestampForMenu;
    }
  }
  if (buildInfo.empty()) {
    buildInfo = "UNKNOWN";
  }
}

const std::string &OS_Link::getBuildVersion() const { return buildVersion; }

const std::string &OS_Link::getBuildTimestamp() const { return buildTimestamp; }

const std::string &OS_Link::getBuildInfo() const { return buildInfo; }

void OS_Link::syncSavedGames() {
#ifdef __EMSCRIPTEN__
  // Copy saves to persistent storage and sync to IndexedDB
  EM_ASM({
    try {
      // Copy all .dod files from /saved to /saved_persistent
      var files = FS.readdir('/saved');
      for (var i = 0; i < files.length; i++) {
        var file = files[i];
        if (file !== '.' && file !== '..' && file.endsWith('.dod')) {
          try {
            var data = FS.readFile('/saved/' + file);
            FS.writeFile('/saved_persistent/' + file, data);
          } catch(e) {
            console.log('Error copying ' + file + ' to persistent storage:', e);
          }
        }
      }
      // Sync to IndexedDB (populate = false means write to persistent storage)
      FS.syncfs(false, function(err) {
        if (err) {
          console.log('Error syncing saved games to IndexedDB:', err);
        } else {
          console.log('Saved games synced to IndexedDB');
        }
      });
    } catch(e) {
      console.log('Error during save sync:', e);
    }
  });
#endif
}

std::vector<std::string> OS_Link::listSavedGames() {
  std::vector<std::string> saves;
  DIR *dir;
  struct dirent *ent;

  if ((dir = opendir(savedDir)) != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      std::string filename = ent->d_name;
      // Check if file ends with .dod
      if (filename.length() > 4 &&
          filename.substr(filename.length() - 4) == ".dod") {
        // Remove the .dod extension for display
        std::string displayName = filename.substr(0, filename.length() - 4);
        // Convert to uppercase for consistency with game style
        for (char &c : displayName) {
          c = toupper(c);
        }
        saves.push_back(displayName);
      }
    }
    closedir(dir);
  }

  // Sort alphabetically
  std::sort(saves.begin(), saves.end());

  return saves;
}

bool OS_Link::loadSavedGame(const std::string& filename) {
  // Build the full path
  memset(gamefile, 0, gamefileLen);
  strcpy(gamefile, savedDir);
  strcat(gamefile, pathSep);

  // First try the display name as-is (for files created by ZSAVE which are uppercase)
  std::string actualFilename = filename;
  strcat(gamefile, actualFilename.c_str());
  strcat(gamefile, ".dod");

  // Check if file exists with this name
  FILE *fptr = fopen(gamefile, "r");
  if (fptr == NULL) {
    // Try lowercase version (for preloaded files like game.dod)
    memset(gamefile, 0, gamefileLen);
    strcpy(gamefile, savedDir);
    strcat(gamefile, pathSep);
    std::string lowerFilename = filename;
    for (char &c : lowerFilename) {
      c = tolower(c);
    }
    strcat(gamefile, lowerFilename.c_str());
    strcat(gamefile, ".dod");

    // Final check with lowercase name
    fptr = fopen(gamefile, "r");
    if (fptr == NULL) {
      return false;
    }
  }
  fclose(fptr);

  // Trigger the load by decrementing ZFLAG (same as PZLOAD)
  --scheduler.ZFLAG;
  return true;
}

bool OS_Link::saveGameWithName(const std::string& filename) {
  // Build the full path - use uppercase like ZSAVE does
  memset(gamefile, 0, gamefileLen);
  strcpy(gamefile, savedDir);
  strcat(gamefile, pathSep);

  // Convert to uppercase for consistency with ZSAVE
  std::string upperFilename = filename;
  for (char &c : upperFilename) {
    c = toupper(c);
  }
  strcat(gamefile, upperFilename.c_str());
  strcat(gamefile, ".dod");

  // Trigger the save by incrementing ZFLAG (same as PZSAVE)
  ++scheduler.ZFLAG;
  return true;
}

bool OS_Link::deleteSavedGame(const std::string& filename) {
  char filepath[MAX_FILENAME_LENGTH];

  // Try uppercase version first (files created by ZSAVE)
  memset(filepath, 0, MAX_FILENAME_LENGTH);
  strcpy(filepath, savedDir);
  strcat(filepath, pathSep);
  std::string upperFilename = filename;
  for (char &c : upperFilename) {
    c = toupper(c);
  }
  strcat(filepath, upperFilename.c_str());
  strcat(filepath, ".dod");

  if (remove(filepath) == 0) {
    syncSavedGames(); // Sync deletion to persistent storage
    return true;
  }

  // Try lowercase version (for preloaded files like game.dod)
  memset(filepath, 0, MAX_FILENAME_LENGTH);
  strcpy(filepath, savedDir);
  strcat(filepath, pathSep);
  std::string lowerFilename = filename;
  for (char &c : lowerFilename) {
    c = tolower(c);
  }
  strcat(filepath, lowerFilename.c_str());
  strcat(filepath, ".dod");

  if (remove(filepath) == 0) {
    syncSavedGames(); // Sync deletion to persistent storage
    return true;
  }

  return false;
}

void OS_Link::render() {
  // All states go through the state machine
  bool runScheduler = game.updateState();

  if (!runScheduler) {
    return;
  }

  // STATE_PLAYING or fade/menu states that signal completion
  dodGame::GameState state = game.getState();

  // Handle death fade completion - restart the game
  if (state == dodGame::STATE_DEATH_FADE) {
    // Death fade completed, handle restart
    if (game.AUTFLG) {
      if (game.demoRestart) {
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
      } else {
        game.AUTFLG = false;
        game.Restart();
      }
    } else {
      game.Restart();
    }
    return;
  }

  // Handle victory fade completion - restart but keep hasWon=true
  if (state == dodGame::STATE_WIN_FADE) {
    // Victory fade completed - hasWon is already set by updateVictoryFade
    // Now restart to show the win or go back to demo
    if (game.AUTFLG) {
      if (game.demoRestart) {
        // Demo won - restart demo
        game.DEMOPTR = 0;
        object.Reset();
        creature.Reset();
        parser.Reset();
        player.Reset();
        scheduler.Reset();
        viewer.Reset();
        dungeon.VFTPTR = 0;
        game.hasWon = false; // Clear for demo restart
        game.COMINI();
      } else {
        // Player started new game, won - restart
        game.AUTFLG = false;
        game.Restart();
      }
    } else {
      // Normal game - restart after victory
      game.Restart();
    }
    return;
  }

  // Note: STATE_INTERMISSION_FADE sets state to PLAYING before returning,
  // so it falls through to normal gameplay below

  if (state != dodGame::STATE_PLAYING) {
    return;
  }

  // Normal gameplay - run scheduler
  bool handle_res = scheduler.SCHED();

  // If SCHED triggered a state change (e.g., death/victory fade), don't process result yet
  // Let the state machine handle it next frame
  if (game.getState() != dodGame::STATE_PLAYING) {
    return;
  }

  if (handle_res) {
    if (scheduler.ZFLAG == 0xFF) {
      game.LoadGame();
      scheduler.ZFLAG = 0;
    } else {
      if (game.AUTFLG) {
        if (game.demoRestart) {
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
        } else {
          // Start new game
          game.AUTFLG = false;
          game.Restart();
        }
      } else {
        game.Restart();
      }
    }
  }
}

void main_game_loop(void *arg) {
  // Main game loop - called at browser frame rate
  // Game timing is handled by delta-time compensation in the scheduler
  static_cast<OS_Link *>(arg)->render();
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
void OS_Link::init() {
  loadOptFile();

#ifdef __EMSCRIPTEN__
  // Set up IDBFS for persistent saved games
  EM_ASM({
    // Mount IDBFS to /saved directory for persistent storage
    try {
      FS.mkdir('/saved_persistent');
    } catch(e) {
      // Directory might already exist
    }
    FS.mount(IDBFS, {}, '/saved_persistent');

    // Sync from IndexedDB to memory (populate = true)
    FS.syncfs(true, function(err) {
      if (err) {
        console.log('Error loading saved games from IndexedDB:', err);
      } else {
        console.log('Saved games loaded from IndexedDB');
        // Copy any persisted saves to the /saved directory
        try {
          var files = FS.readdir('/saved_persistent');
          for (var i = 0; i < files.length; i++) {
            var file = files[i];
            if (file !== '.' && file !== '..' && file.endsWith('.dod')) {
              try {
                var data = FS.readFile('/saved_persistent/' + file);
                FS.writeFile('/saved/' + file, data);
                console.log('Restored save: ' + file);
              } catch(e) {
                console.log('Error copying ' + file + ':', e);
              }
            }
          }
        } catch(e) {
          console.log('Error reading saved_persistent:', e);
        }
      }
    });
  });
#endif

  std::cout << "DOD build info: " << buildInfo << std::endl;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
    fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
    quitSDL(1);
  }

  if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers)) {
    fprintf(stderr, "Unable to open audio!\n");
    quitSDL(1);
  }

  creature.LoadSounds();
  object.LoadSounds();
  scheduler.LoadSounds();
  player.LoadSounds();

  int allocatedChannels = Mix_AllocateChannels(4);
  if (allocatedChannels < 0) {
    allocatedChannels = 0;
  }
  scheduler.ConfigureChannelSync(allocatedChannels);
  Mix_Volume(-1, MIX_MAX_VOLUME);
  if (FullScreen == 0) {
    sdlWindow = SDL_CreateWindow("DOD", SDL_WINDOWPOS_UNDEFINED,
                                 SDL_WINDOWPOS_UNDEFINED, width,
                                 (int)(width * 0.75), SDL_WINDOW_OPENGL);
  } else {
    sdlWindow = SDL_CreateWindow(
        "DOD", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
        (int)(width * 0.75), SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
  }

  if (sdlWindow == 0) {
    fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
    quitSDL(1);
  }

  // Error callback
  RegalSetErrorCallback(myError);

  sdlGlContext = SDL_GL_CreateContext(sdlWindow);

  if (sdlGlContext == 0) {
    fprintf(stderr, "OpenGL context creation failed: %s\n", SDL_GetError());
    quitSDL(1);
  }

  // Initialize Regal after context is confirmed to be valid
  RegalMakeCurrent((RegalSystemContext)1);
  //    std::cout << "After GL context" << std::endl;
  // bpp = info->vfmt->BitsPerPixel;
  // TODO: ARE THESE NEEDED
  //	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  //	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  //    SDL_GL_SetSwapInterval(0);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  // flags = SDL_OPENGL;

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
  } else if (keylayout == 1) // Dvorak
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
  std::cout << "Build Info: " << getBuildInfo() << std::endl;
  // Note: Removed DOD_Delay(2500) - assets are preloaded by Emscripten
  // and blocking delays don't work without ASYNCIFY

  std::cout << "Starting game..." << std::endl;
  game.COMINI();

  std::cout << "After COMINI" << std::endl;
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(main_game_loop, this, 0, 0);
#else
  while (1) {
    main_game_loop(this);
  }
#endif
  //    std::cout << "End of init" << std::endl;
}

// Used to check for keystrokes and application termination
void OS_Link::process_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      handle_key_down(&event.key.keysym);
      break;
    case SDL_MOUSEBUTTONDOWN:
      // Handle mouse clicks when Modern Controls is enabled
      if (game.ModernControls && viewer.display_mode != Viewer::MODE_MAP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          sendCommand("A L");  // Attack left
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          sendCommand("A R");  // Attack right
        }
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
}

// Quits application
void OS_Link::quitSDL(int code) {
  Mix_CloseAudio();
  SDL_Quit();
  exit(code);
}

void OS_Link::send_input(char *keys) {
  for (int i = 0; keys[i] != '\0'; i++) {
    parser.KBDPUT((dodBYTE)keys[i]);
  }
}

// Send a command string as if typed and press Enter to execute it
void OS_Link::sendCommand(const char *cmd) {
  // Send each character of the command
  for (int i = 0; cmd[i] != '\0'; i++) {
    char c = cmd[i];
    // Convert lowercase to uppercase (DOD uses uppercase)
    if (c >= 'a' && c <= 'z') {
      c = c - 'a' + 'A';
    }
    parser.KBDPUT((dodBYTE)c);
  }
  // Send carriage return to execute the command
  parser.KBDPUT(parser.C_CR);
}

void OS_Link::stop_demo() {
  game.hasWon = true;
  game.demoRestart = false;

  SDL_Event ev;
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = SDLK_SPACE;
  ev.key.state = SDL_PRESSED;
  ev.key.repeat = 0;
  SDL_PushEvent(&ev);

  SDL_Event evUp;
  evUp.type = SDL_KEYUP;
  evUp.key.keysym.sym = SDLK_SPACE;
  evUp.key.state = SDL_RELEASED;
  evUp.key.repeat = 0;
  SDL_PushEvent(&evUp);
}

void OS_Link::trigger_menu() {
  SDL_Event ev = {};
  ev.type = SDL_KEYDOWN;
  ev.key.keysym.sym = SDLK_ESCAPE;
  ev.key.state = SDL_PRESSED;
  ev.key.repeat = 0;
  SDL_PushEvent(&ev);

  SDL_Event evUp = {};
  evUp.type = SDL_KEYUP;
  evUp.key.keysym.sym = SDLK_ESCAPE;
  evUp.key.state = SDL_RELEASED;
  SDL_PushEvent(&evUp);
}

// Processes key strokes.
void OS_Link::handle_key_down(SDL_Keysym *keysym) {
  dodBYTE c;
  if (viewer.display_mode == Viewer::MODE_MAP) {
    switch (keysym->sym) {
    case SDLK_ESCAPE:
      game.requestMenu();
      break;
    case SDLK_BACKQUOTE:
    case SDLK_SLASH:
      // Toggle map off with backtick or slash when Modern Controls enabled
      if (game.ModernControls) {
        viewer.display_mode = Viewer::MODE_3D;
        --viewer.UPDATE;
        return;
      }
      // Fall through to default if Modern Controls disabled
    default:
      viewer.display_mode = Viewer::MODE_3D;
      --viewer.UPDATE;
      parser.KBDPUT(32); // This is a (necessary ???) hack.
      break;
    }
  } else {
    // Handle Modern Controls when enabled
    if (game.ModernControls) {
      switch (keysym->sym) {
      case SDLK_UP:
        sendCommand("M");  // Move forward
        return;
      case SDLK_DOWN:
        sendCommand("M B");  // Move backward
        return;
      case SDLK_LEFT:
        sendCommand("T L");  // Turn left
        return;
      case SDLK_RIGHT:
        sendCommand("T R");  // Turn right
        return;
      case SDLK_TAB:
        // Toggle between Examine and Look
        if (game.ModernControlsExamineMode) {
          sendCommand("L");  // Look
        } else {
          sendCommand("EX");  // Examine backpack
        }
        game.ModernControlsExamineMode = !game.ModernControlsExamineMode;
        return;
      case SDLK_BACKQUOTE:
      case SDLK_SLASH:
        // Toggle map on with backtick or slash
        viewer.display_mode = Viewer::MODE_MAP;
        viewer.PUPDAT();
        return;
      default:
        break; // Fall through to normal key handling
      }
    }

    switch (keysym->sym) {
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
      game.requestMenu();
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
bool OS_Link::main_menu() {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_ASYNCIFY__)
  // Non-blocking: set up state and return
  game.requestMenu();
  return false;
#else
  bool end = false;
  static int row = 0, col = 0;
  static menu mainMenu;

  scheduler.pause(true);
  viewer.drawMenu(mainMenu, col, row);

  auto handleMenuEvent = [&](const SDL_Event &event) {
    switch (event.type) {
    case SDL_KEYDOWN: {
      bool redraw = false;
      switch (event.key.keysym.sym) {
      case SDLK_RETURN:
      case SDLK_SPACE:
        end = menu_return(col, row, mainMenu);
        if (col == FILE_MENU_SWITCH && row == FILE_MENU_NEW) {
          return true;
        }
        redraw = true;
        break;
      case SDLK_UP:
        row = (row < 1) ? mainMenu.getMenuSize(col) - 1 : row - 1;
        redraw = true;
        break;
      case SDLK_DOWN:
        row = (row > mainMenu.getMenuSize(col) - 2) ? 0 : row + 1;
        redraw = true;
        break;
      case SDLK_LEFT:
        if (NUM_MENU > 1) {
          col = (col < 1) ? NUM_MENU - 1 : col - 1;
          row = 0;
          redraw = true;
        }
        break;
      case SDLK_RIGHT:
        if (NUM_MENU > 1) {
          col = (col > NUM_MENU - 2) ? 0 : col + 1;
          row = 0;
          redraw = true;
        }
        break;
      case SDLK_ESCAPE:
        end = true;
        redraw = true;
        break;
      default:
        break;
      }

      if (redraw) {
        viewer.drawMenu(mainMenu, col, row);
      }
      break;
    }
    case SDL_QUIT:
      quitSDL(0);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_GL_SwapWindow(sdlWindow);
      break;
    default:
      break;
    }
    return false;
  };

  SDL_Event event;
  while (!end) {
    if (!SDL_WaitEventTimeout(&event, 16)) {
      continue;
    }

    if (handleMenuEvent(event)) {
      return true;
    }

    while (!end && SDL_PollEvent(&event)) {
      if (handleMenuEvent(event)) {
        return true;
      }
    }
  }

  scheduler.pause(false);

  return false;
#endif
}

/*********************************************************
 * Public wrapper for non-blocking menu flow
 *********************************************************/
bool OS_Link::menuReturn(int menu_id, int item, menu Menu) {
  return menu_return(menu_id, item, Menu);
}

/* Function to process menu commands
 *
 *  Returns:  false - if menu should be redrawn
 *            true  - otherwise */

bool OS_Link::menu_return(int menu_id, int item, menu Menu) {
  switch (menu_id) {
  case FILE_MENU_SWITCH:
    switch (item) {
    case FILE_MENU_NEW:
      scheduler.pause(false);
      if (!game.AUTFLG) {
        game.hasWon = true;
        game.demoRestart = false;
      }
      return true;

    case FILE_MENU_RETURN:
      return true;

    case FILE_MENU_LOAD_GAME: {
      // Get list of saved games
      std::vector<std::string> saves = listSavedGames();

      if (saves.empty()) {
        // No saved games found - show message
        static std::string noSavesMenuList[] = {"NO SAVED GAMES FOUND", "BACK"};
        int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                               noSavesMenuList, 2);
        if (result == -2) return false; // Pending - submenu started
        return false; // Return to main menu
      }

      // Build menu list from saved games (max 10 saves + BACK)
      static std::string savedGamesMenuList[11];
      int numSaves = std::min(static_cast<int>(saves.size()), 10);
      for (int i = 0; i < numSaves; i++) {
        savedGamesMenuList[i] = saves[i];
      }
      savedGamesMenuList[numSaves] = "BACK";

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             savedGamesMenuList, numSaves + 1);
      if (result == -2) return false; // Pending - submenu started

      if (result >= 0 && result < numSaves) {
        // User selected a saved game - trigger load
        if (loadSavedGame(saves[result])) {
          scheduler.pause(false);
          return true; // Close menu and trigger load
        }
      }
      // BACK or ESC - return to main menu
      return false;
    }

    case FILE_MENU_SAVE_GAME: {
      // Clear the save name buffer
      memset(saveNameBuffer, 0, sizeof(saveNameBuffer));

      // Show text input for save name
      menu_string(saveNameBuffer, const_cast<char*>("ENTER SAVE NAME"), 15);
      // In non-blocking mode, this returns and we handle result in dodgame.cpp
      return false;
    }

    case FILE_MENU_DELETE_SAVE: {
      // Get list of saved games
      std::vector<std::string> saves = listSavedGames();

      if (saves.empty()) {
        // No saved games found - show message
        static std::string noSavesMenuList[] = {"NO SAVED GAMES FOUND", "BACK"};
        int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                               noSavesMenuList, 2);
        if (result == -2) return false; // Pending - submenu started
        return false; // Return to main menu
      }

      // Build menu list from saved games (max 10 saves + BACK)
      static std::string deleteSaveMenuList[11];
      int numSaves = std::min(static_cast<int>(saves.size()), 10);
      for (int i = 0; i < numSaves; i++) {
        deleteSaveMenuList[i] = saves[i];
      }
      deleteSaveMenuList[numSaves] = "BACK";

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             deleteSaveMenuList, numSaves + 1);
      if (result == -2) return false; // Pending - submenu started

      if (result >= 0 && result < numSaves) {
        // User selected a save to delete
        deleteSavedGame(saves[result]);
      }
      // Return to main menu after delete or BACK
      return false;
    }

    case FILE_MENU_GRAPHICS: {
      // Static to survive function return for non-blocking menu
      static std::string graphicsMenuList[] = {"NORMAL GRAPHICS", "HIRES GRAPHICS",
                                               "VECTOR GRAPHICS"};

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             graphicsMenuList, 3);
      if (result == -2) return false; // Pending - submenu started
      switch (result) {
      case 0:
        g_options &= ~(OPT_VECTOR | OPT_HIRES);
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

    case FILE_MENU_VOLUME: {
      int newVolume = menu_scrollbar("VOLUME LEVEL", 0, 128, volumeLevel);
      if (newVolume == -2) return false; // Pending - submenu started
      volumeLevel = newVolume;
      Mix_Volume(-1, static_cast<int>((volumeLevel * MIX_MAX_VOLUME) / 128));
    }
      return false;

    case FILE_MENU_SND_MODE: {
      // Static to survive function return for non-blocking menu
      static std::string sndModeMenuList[] = {"STEREO", "MONO"};
      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             sndModeMenuList, 2);
      if (result == -2) return false; // Pending - submenu started
      switch (result) {
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
      return true;

    case FILE_MENU_CHEATS: {
      // Static to survive function return for non-blocking menu
      // Build list with current status for each cheat
      static std::string cheatsMenuList[7];
      cheatsMenuList[0] = ((g_cheats & CHEAT_ITEMS) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[0] += "MITHRIL ITEMS";
      cheatsMenuList[1] = ((g_cheats & CHEAT_INVULNERABLE) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[1] += "INVULNERABLE";
      cheatsMenuList[2] = ((g_cheats & CHEAT_REGEN_SCALING) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[2] += "CREATURE SCALING";
      cheatsMenuList[3] = ((g_cheats & CHEAT_REVEAL) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[3] += "EASY REVEAL";
      cheatsMenuList[4] = ((g_cheats & CHEAT_RING) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[4] += "RING ALWAYS WORKS";
      cheatsMenuList[5] = ((g_cheats & CHEAT_TORCH) ? "[ON]  " : "[OFF] ");
      cheatsMenuList[5] += "TORCH ALWAYS LIT";
      cheatsMenuList[6] = "BACK";

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             cheatsMenuList, 7);
      if (result == -2) return false; // Pending - submenu started
      switch (result) {
      case 0: // Mithril Items
        g_cheats ^= CHEAT_ITEMS;
        break;
      case 1: // Invulnerable
        g_cheats ^= CHEAT_INVULNERABLE;
        break;
      case 2: // Creature Scaling
        g_cheats ^= CHEAT_REGEN_SCALING;
        break;
      case 3: // Easy Reveal
        g_cheats ^= CHEAT_REVEAL;
        break;
      case 4: // Ring Always Works
        g_cheats ^= CHEAT_RING;
        break;
      case 5: // Torch Always Lit
        g_cheats ^= CHEAT_TORCH;
        break;
      case 6: // Back
        return false;
      default:
        return false;
      }
    }
      return false;

    case FILE_MENU_GAMEPLAY_MODS: {
      // Static to survive function return for non-blocking menu
      // Build list with current status for each gameplay mod
      static std::string gameplayModsMenuList[8];
      gameplayModsMenuList[0] = (game.ShieldFix ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[0] += "SHIELD FIX";
      gameplayModsMenuList[1] = (game.VisionScroll ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[1] += "VISION SCROLL";
      gameplayModsMenuList[2] = (game.MarkDoorsOnScrollMaps ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[2] += "MARK DOORS ON MAPS";
      gameplayModsMenuList[3] = (game.CreaturesIgnoreObjects ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[3] += "CREATURES IGNORE OBJS";
      gameplayModsMenuList[4] = (game.CreaturesInstaRegen ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[4] += "CREATURES INSTA-REGEN";
      gameplayModsMenuList[5] = (game.RandomMaze ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[5] += "RANDOM MAZES";
      gameplayModsMenuList[6] = (game.ModernControls ? "[ON]  " : "[OFF] ");
      gameplayModsMenuList[6] += "MODERN CONTROLS";
      gameplayModsMenuList[7] = "BACK";

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             gameplayModsMenuList, 8);
      if (result == -2) return false; // Pending - submenu started
      switch (result) {
      case 0: // Shield Fix
        game.ShieldFix = !game.ShieldFix;
        break;
      case 1: // Vision Scroll
        game.VisionScroll = !game.VisionScroll;
        break;
      case 2: // Mark Doors on Maps
        game.MarkDoorsOnScrollMaps = !game.MarkDoorsOnScrollMaps;
        break;
      case 3: // Creatures Ignore Objects
        game.CreaturesIgnoreObjects = !game.CreaturesIgnoreObjects;
        break;
      case 4: // Creatures Insta-Regen
        game.CreaturesInstaRegen = !game.CreaturesInstaRegen;
        break;
      case 5: // Random Mazes
        game.RandomMaze = !game.RandomMaze;
        break;
      case 6: // Modern Controls
        game.ModernControls = !game.ModernControls;
        game.ModernControlsExamineMode = false; // Reset TAB toggle
        break;
      case 7: // Back
        return false;
      default:
        return false;
      }
    }
      return false;

    case FILE_MENU_GAME_TIMING: {
      // Static to survive function return for non-blocking menu
      static std::string gameTimingMenuList[5];
      gameTimingMenuList[0] = "CREATURE SPEED";
      gameTimingMenuList[1] = "TURN DELAY";
      gameTimingMenuList[2] = "MOVE DELAY";
      gameTimingMenuList[3] = "CREATURE REGEN";
      gameTimingMenuList[4] = "BACK";

      int result = menu_list(menu_id * 5, item + 2, Menu.getMenuItem(menu_id, item),
                             gameTimingMenuList, 5);
      if (result == -2) return false; // Pending - submenu started
      // Results 0-3 are handled in dodgame.cpp to open scrollbars
      // Result 4 (BACK) or -1 (ESC) returns to main menu
    }
      return false;

    case FILE_MENU_BUILD_INFO:
      return false;

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
 *  Function used to draw a list, move among that list, and return the item
 *selected
 *
 *  Arguments: x        - The top-left x-coordinate to draw list at
 *             y        - The top-left y-coordinate to draw list at
 *             title    - The title of the list
 *             list     - An array of strings (the list to be chosen from
 *             listSize - The size of the array
 ******************************************************************************/

int OS_Link::menu_list(int x, int y, char *title, std::string list[],
                       int listSize) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_ASYNCIFY__)
  // Non-blocking: set up state and return pending
  game.menuX = x;
  game.menuY = y;
  game.menuTitle = title;
  game.menuList = list;
  game.menuListSize = listSize;
  game.menuListChoice = 0;
  game.menuComplete = false;
  game.menuReturnValue = -1;
  menuPending = MENU_PENDING_LIST;
  game.setState(dodGame::STATE_MENU_LIST);
  return -2; // Pending
#else
  int currentChoice = 0;

  while (true) {
    viewer.drawMenuList(x, y, title, list, listSize, currentChoice);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_SPACE:
          return (currentChoice);
          break;

        case SDLK_UP:
          (currentChoice < 1) ? currentChoice = listSize - 1 : currentChoice--;
          break;

        case SDLK_DOWN:
          (currentChoice > listSize - 2) ? currentChoice = 0 : currentChoice++;
          break;

        case SDLK_ESCAPE:
          return (-1);
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
    DOD_Delay(16); // Reduced ASYNCIFY overhead for mobile browsers
  } // End of while loop

  return (-1);
#endif
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

int OS_Link::menu_scrollbar(std::string title, int min, int max, int current) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_ASYNCIFY__)
  // Non-blocking: set up state and return pending
  int range = max - min;
  if (range <= 0) {
    return current;
  }
  int position = static_cast<int>(std::round((static_cast<double>(current - min) * 31.0) /
                                              static_cast<double>(range)));
  position = std::max(0, std::min(31, position));

  game.menuTitle = title;
  game.menuScrollMin = min;
  game.menuScrollMax = max;
  game.menuScrollPosition = position;
  game.menuOriginalScrollPos = position;
  game.menuComplete = false;
  game.menuReturnValue = current;
  menuPending = MENU_PENDING_SCROLLBAR;
  game.setState(dodGame::STATE_MENU_SCROLLBAR);
  return -2; // Pending
#else
  int oldvalue = current; // Save the old value in case the user escapes
  int range = max - min;
  if (range <= 0) {
    return current;
  }

  int position =
      static_cast<int>(std::round((static_cast<double>(current - min) * 31.0) /
                                  static_cast<double>(range)));
  position = std::max(0, std::min(31, position));
  int originalPosition = position;

  viewer.drawMenuScrollbar(title, position);

  while (true) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_SPACE: {
          if (position == originalPosition) {
            return oldvalue;
          }
          int result = static_cast<int>(std::round(
              static_cast<double>(min) + (static_cast<double>(position) *
                                          static_cast<double>(range) / 31.0)));
          result = std::max(min, std::min(max, result));
          return result;
        } break;

        case SDLK_LEFT:
          position = (position > 0) ? position - 1 : 0;
          break;

        case SDLK_RIGHT:
          position = (position < 31) ? position + 1 : 31;
          break;

        case SDLK_ESCAPE:
          return (oldvalue);
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
    DOD_Delay(16); // Reduced ASYNCIFY overhead for mobile browsers
  }
#endif
}

/*****************************************************************************
 *  Function used to draw a box for a string entry, then return it
 *
 *  Arguments: newString - The string to be returned
 *             title     - The title of the entry
 *             maxLength - The maximum size of the entry
 ******************************************************************************/
void OS_Link::menu_string(char *newString, char *title, int maxLength) {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_ASYNCIFY__)
  // Non-blocking: set up state and return
  *newString = '\0';
  game.menuTitle = title;
  game.menuStringBuffer = newString;
  game.menuMaxLength = maxLength;
  game.menuComplete = false;
  menuPending = MENU_PENDING_STRING;
  game.setState(dodGame::STATE_MENU_STRING);
  return;
#else
  *newString = '\0';
  viewer.drawMenuStringTitle(title);
  viewer.drawMenuString(newString);

  while (true) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
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
        // case SDLK_LSUPER:
        // case SDLK_RSUPER:
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
          if (strlen(newString) > 0) {
            *(newString + strlen(newString) - 1) = '\0';
            viewer.drawMenuStringTitle(title); // Update with the new word
            viewer.drawMenuString(newString);
          }
          break;

        case SDLK_ESCAPE:
          *(newString) = '\0';
          return;
          break;

        default:
          if (strlen(newString) < maxLength) {
            *(newString + strlen(newString) + 1) = '\0';
            *(newString + strlen(newString)) = keys[event.key.keysym.sym];
            viewer.drawMenuStringTitle(title); // Update with the new word
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
      DOD_Delay(16); // Reduced ASYNCIFY overhead for mobile browsers
    }
  } // End of while loop
#endif
}

/******************************************************************************
 *  Function used to load & parse options file
 *
 *  Arguments: None
 ******************************************************************************/
void OS_Link::loadOptFile(void) {
  char inputString[80];
  char fn[20];
  int in;
  ifstream fin;
  char *breakPoint;

  loadDefaults(); // In case some variables aren't in the opts file, and if no
                  // file exists

  sprintf(fn, "%s%s%s", confDir, pathSep, "opts.ini");

  fin.open(fn);
  if (!fin) {
    return;
  }

  fin >> inputString;
  while (fin) {
    breakPoint = strchr(inputString, '=');

    // Ignore strings that have no equals, or are only an equals, or have no end
    if (breakPoint || breakPoint == inputString ||
        breakPoint == (inputString + strlen(inputString) - 1)) {
      *(breakPoint) = '\0';
      breakPoint++;

      if (!strcmp(inputString, "creatureSpeed")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          creature.creSpeedMul = in;
      } else if (!strcmp(inputString, "turnDelay")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          player.turnDelay = in;
      } else if (!strcmp(inputString, "moveDelay")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          player.moveDelay = in;
      } else if (!strcmp(inputString, "keylayout")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          keylayout = in;
      } else if (!strcmp(inputString, "graphicsMode")) {
        if (!strcmp(breakPoint, "NORMAL"))
          g_options &= ~(OPT_VECTOR | OPT_HIRES);
        else if (!strcmp(breakPoint, "HIRES")) {
          g_options &= ~(OPT_VECTOR);
          g_options |= OPT_HIRES;
        } else if (!strcmp(breakPoint, "VECTOR")) {
          g_options &= ~(OPT_HIRES);
          g_options |= OPT_VECTOR;
        }
      } else if (!strcmp(inputString, "stereoMode")) {
        if (!strcmp(breakPoint, "STEREO"))
          g_options |= OPT_STEREO;
        else if (!strcmp(breakPoint, "MONO"))
          g_options &= ~OPT_STEREO;
      } else if (!strcmp(inputString, "volumeLevel")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          volumeLevel = in;
      } else if (!strcmp(inputString, "saveDirectory")) {
        strncpy(savedDir, "saved", MAX_FILENAME_LENGTH);
      } else if (!strcmp(inputString, "fullScreen")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          FullScreen = in;
      } else if (!strcmp(inputString, "screenWidth")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          width = in;
      } else if (!strcmp(inputString, "creatureRegen")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          creatureRegen = in;
      } else if (!strcmp(inputString, "RandomMaze")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.RandomMaze = in;
      } else if (!strcmp(inputString, "ShieldFix")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.ShieldFix = in;
      } else if (!strcmp(inputString, "VisionScroll")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.VisionScroll = in;
      } else if (!strcmp(inputString, "CreaturesIgnoreObjects")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.CreaturesIgnoreObjects = in;
      } else if (!strcmp(inputString, "CreaturesInstaRegen")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.CreaturesInstaRegen = in;
      } else if (!strcmp(inputString, "MarkDoorsOnScrollMaps")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.MarkDoorsOnScrollMaps = in;
      } else if (!strcmp(inputString, "ModernControls")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          game.ModernControls = in;
      } else if (!strcmp(inputString, "Cheats")) {
        if (1 == sscanf(breakPoint, "%d", &in))
          g_cheats = in;
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
bool OS_Link::saveOptFile(void) {
  ofstream fout;
  char fn[MAX_FILENAME_LENGTH];

  sprintf(fn, "%s%s%s", confDir, pathSep, "opts.ini");

  fout.open(fn);
  if (!fout)
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
  if (g_options & OPT_VECTOR)
    fout << "VECTOR" << endl;
  else if (g_options & OPT_HIRES)
    fout << "HIRES" << endl;
  else
    fout << "NORMAL" << endl;

  fout << "stereoMode=";
  if (g_options & OPT_STEREO)
    fout << "STEREO" << endl;
  else
    fout << "MONO" << endl;

  fout << "RandomMaze=" << game.RandomMaze << endl;
  fout << "ShieldFix=" << game.ShieldFix << endl;
  fout << "VisionScroll=" << game.VisionScroll << endl;
  fout << "CreaturesIgnoreObjects=" << game.CreaturesIgnoreObjects << endl;
  fout << "CreaturesInstaRegen=" << game.CreaturesInstaRegen << endl;
  fout << "MarkDoorsOnScrollMaps=" << game.MarkDoorsOnScrollMaps << endl;
  fout << "ModernControls=" << game.ModernControls << endl;
  fout << "Cheats=" << g_cheats << endl;

  fout.close();

  return true;
}

/******************************************************************************
 *  Function used to load the options file from current settings
 *
 *  Arguments: None
 ******************************************************************************/
void OS_Link::loadDefaults(void) {
  player.turnDelay = 37;
  player.moveDelay = 500;
  keylayout = 0;
  volumeLevel = MIX_MAX_VOLUME;
  creature.creSpeedMul = 200;
  creature.UpdateCreSpeed();
  strcpy(savedDir, "saved");
  FullScreen = false;
  width = 1024;
  creatureRegen = 5;
  scheduler.updateCreatureRegen(creatureRegen);

  g_options &= ~(OPT_VECTOR | OPT_HIRES);
  g_options |= OPT_STEREO;
  g_cheats = 0;

  // Initialize gameplay mod settings
  game.RandomMaze = false;
  game.ShieldFix = false;
  game.VisionScroll = false;
  game.CreaturesIgnoreObjects = false;
  game.CreaturesInstaRegen = false;
  game.MarkDoorsOnScrollMaps = false;
  game.ModernControls = false;
  game.ModernControlsExamineMode = false;
}

/******************************************************************************
 *  Function used to swap fullscreen mode
 *
 *  Arguments: None
 ******************************************************************************/
void OS_Link::changeFullScreen(void) {
  FullScreen = !FullScreen;
  changeVideoRes(width);
}

/******************************************************************************
 *  Function used to change the video resolution
 *
 *  Arguments: newWidth - The screen width to change to
 ******************************************************************************/
void OS_Link::changeVideoRes(int newWidth) {
  int newHeight;

  newHeight = (int)(newWidth * 0.75);

  SDL_SetWindowSize(sdlWindow, newWidth, newHeight);

  if (FullScreen) {
    if (SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN) == 0) {
      SDL_ShowCursor(SDL_DISABLE);
    } else {
      fprintf(stderr, "Window fullscreen failed: %s\n", SDL_GetError());
    }

  } else {
    if (SDL_SetWindowFullscreen(sdlWindow, 0) == 0) {
      SDL_ShowCursor(SDL_ENABLE);
    } else {
      fprintf(stderr, "Window fullscreen failed: %s\n", SDL_GetError());
    }
  }
  width = newWidth;
  height = newHeight;
  crd.setCurWH((double)width);

  viewer.setup_opengl();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}
