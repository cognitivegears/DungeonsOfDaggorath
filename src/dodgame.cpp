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
// Filename: dodgame.cpp
//
// Implementation of dodGame class.

#include "dodgame.h"
#include "creature.h"
#include "dungeon.h"
#include "enhanced.h"
#include "object.h"
#include "oslink.h"
#include "parser.h"
#include "player.h"
#include "sched.h"
#include "viewer.h"

extern OS_Link oslink;
extern Dungeon dungeon;
extern Parser parser;
extern Creature creature;
extern Object object;
extern Player player;
extern Viewer viewer;
extern Scheduler scheduler;
extern RNG rng;

#define D_EXAMINE "010f"
#define D_PULL_RIGHT "032605"
#define D_TORCH "19"
#define D_USE_RIGHT "023705"
#define D_LOOK "011e"
#define D_MOVE "0122"
#define D_PULL_LEFT "032601"
#define D_SHIELD "0f"
#define D_SWORD "14"
#define D_ATTACK_RIGHT "020105"
#define D_TURN_RIGHT "023305"
#define D_END "00"

// Constructor
dodGame::dodGame()
    : LEVEL(2), AUTFLG(true), hasWon(false), DEMOPTR(0), demoRestart(true),
      gameState(STATE_INIT), returnState(STATE_PLAYING),
      stateStartTime(0), stateWaitTime(0), nextFrameTime(0), initialized(false),
      fadePhase(FADE_PHASE_DONE), postFadeAction(POST_FADE_NONE),
      fadeMode(0), fadeVCTFAD(0), fadeInterrupted(false),
      menuRow(0), menuCol(0), menuListChoice(0), menuScrollPosition(0),
      menuOriginalScrollPos(0), menuScrollMin(0), menuScrollMax(0),
      menuListSize(0), menuMaxLength(0), menuList(nullptr), menuStringBuffer(nullptr),
      menuX(0), menuY(0), menuReturnValue(-1), menuComplete(false),
      animationType(ANIM_NONE), animFrame(0), animTotalFrames(0), animDir(0),
      animOffset(0), animFrameStart(0), animFrameDuration(0), animPhase(0), animMoveDir(0),
      faintTargetLight(0), faintIsDeath(false), faintStepCount(0), faintStartLight(0) {
  Utils::LoadFromHex(DEMO_CMDS,
                     D_EXAMINE D_PULL_RIGHT D_TORCH D_USE_RIGHT D_LOOK D_MOVE
                         D_PULL_LEFT D_SHIELD D_PULL_RIGHT D_SWORD D_MOVE D_MOVE
                             D_ATTACK_RIGHT D_TURN_RIGHT D_MOVE D_MOVE D_MOVE
                                 D_TURN_RIGHT D_MOVE D_MOVE D_END);
}

// Game initialization - non-blocking, sets up state machine
void dodGame::COMINI() {
  scheduler.SYSTCB();
  object.CreateAll();
  player.HBEATF = 0;
  viewer.clearArea(&viewer.TXTSTS);
  viewer.clearArea(&viewer.TXTPRI);
  viewer.VXSCAL = 0x80;
  viewer.VYSCAL = 0x80;
  viewer.VXSCALf = 128.0f;
  viewer.VYSCALf = 128.0f;

  // Start the fade intro sequence (non-blocking)
  initFadeState(Viewer::FADE_BEGIN);
  gameState = STATE_FADE_INTRO;
  initialized = true;
}

// Initialize fade state for a particular fade mode
void dodGame::initFadeState(int mode) {
  fadeMode = mode;
  fadePhase = FADE_PHASE_BUZZ_IN;
  fadeVCTFAD = 32;
  fadeInterrupted = false;
  postFadeAction = POST_FADE_NONE; // Clear any previous post-fade action
  nextFrameTime = SDL_GetTicks();
  stateStartTime = SDL_GetTicks();

  // Set up viewer state
  viewer.VXSCAL = 0x80;
  viewer.VYSCAL = 0x80;
  viewer.VXSCALf = 128.0f;
  viewer.VYSCALf = 128.0f;
  viewer.clearArea(&viewer.TXTPRI);
  viewer.RANGE = 1;
  viewer.SETSCL();
  viewer.VCTFAD = 32;
  viewer.fadeVal = -2;
  viewer.done = false;

  // Reset lighting for death/victory fades (may have been dimmed by faint)
  if (mode == Viewer::FADE_DEATH || mode == Viewer::FADE_VICTORY) {
    viewer.MLIGHT = viewer.OLIGHT;
    viewer.RLIGHT = viewer.OLIGHT;
  }

  // Display appropriate message
  switch (mode) {
  case Viewer::FADE_BEGIN:
    viewer.displayCopyright();
    viewer.displayWelcomeMessage();
    break;
  case Viewer::FADE_MIDDLE:
    viewer.clearArea(&viewer.TXTSTS);
    viewer.displayEnough();
    break;
  case Viewer::FADE_DEATH:
    viewer.clearArea(&viewer.TXTSTS);
    viewer.displayDeath();
    break;
  case Viewer::FADE_VICTORY:
    viewer.clearArea(&viewer.TXTSTS);
    viewer.displayWinner();
    break;
  }

  // Clear event buffer
  SDL_Event event;
  while (SDL_PollEvent(&event))
    ;

  // Start buzz sound
  Mix_Volume(viewer.fadChannel, 0);
  Mix_PlayChannel(viewer.fadChannel, creature.buzz, -1);
}

// Draw one frame of the fade animation
void dodGame::drawFadeFrame() {
  int *wiz = (fadeMode == Viewer::FADE_VICTORY) ? viewer.W2_VLA : viewer.W1_VLA;

  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  viewer.drawArea(&viewer.TXTSTS);
  glColor3fv(viewer.fgColor);
  glLoadIdentity();
  viewer.drawVectorList(wiz);

  // Draw message for certain phases
  if (fadePhase == FADE_PHASE_MESSAGE || fadePhase == FADE_PHASE_WAIT_KEY) {
    viewer.drawArea(&viewer.TXTPRI);
  }

  SDL_GL_SwapWindow(oslink.sdlWindow);
}

// Process one frame of fade animation, returns true when complete
bool dodGame::processFadeFrame() {
  Uint32 now = SDL_GetTicks();

  // Draw every frame (~60fps) for smooth visuals
  bool shouldDraw = (now >= nextFrameTime);
  if (shouldDraw) {
    nextFrameTime = now + 16; // 60fps for drawing
  }

  // Only advance fade value every buzzStep (300ms) to match original timing
  // This is only used during BUZZ_IN and BUZZ_OUT phases
  bool shouldAdvanceFade = false;
  if (fadePhase == FADE_PHASE_BUZZ_IN || fadePhase == FADE_PHASE_BUZZ_OUT) {
    shouldAdvanceFade = (now >= stateStartTime + viewer.buzzStep);
    if (shouldAdvanceFade) {
      stateStartTime = now;
    }
  }

  // Check for key press during intro fade
  if (fadeMode == Viewer::FADE_BEGIN && scheduler.keyCheck()) {
    Mix_HaltChannel(viewer.fadChannel);
    viewer.clearArea(&viewer.TXTPRI);
    SDL_Event event;
    while (SDL_PollEvent(&event))
      ;
    fadeInterrupted = false; // Key pressed = not demo mode
    fadePhase = FADE_PHASE_DONE;
    return true;
  }

  switch (fadePhase) {
  case FADE_PHASE_BUZZ_IN:
    if ((fadeVCTFAD & 128) == 0) {
      // Set buzz volume based on current fade value
      Mix_Volume(viewer.fadChannel,
                 static_cast<int>(
                     ((32 - fadeVCTFAD) / 2) *
                     ((oslink.volumeLevel * MIX_MAX_VOLUME) / 128.0 / 16.0)));
      viewer.VCTFAD = fadeVCTFAD;

      // Draw every frame for smooth visuals
      if (shouldDraw) {
        drawFadeFrame();
      }

      // Only advance fade value every buzzStep
      if (shouldAdvanceFade) {
        fadeVCTFAD -= 2;
      }
    } else {
      // Fade in complete
      fadeVCTFAD = 0;
      viewer.VCTFAD = 0;
      Mix_HaltChannel(viewer.fadChannel);

      // Play crash sound
      Mix_Volume(viewer.fadChannel,
                 static_cast<int>((oslink.volumeLevel * MIX_MAX_VOLUME) / 128));
      Mix_PlayChannel(viewer.fadChannel, creature.kaboom, 0);
      fadePhase = FADE_PHASE_CRASH;
      stateStartTime = now;
    }
    break;

  case FADE_PHASE_CRASH:
    // Ensure wizard is fully visible during crash
    viewer.VCTFAD = 0;
    // Draw every frame
    if (shouldDraw) {
      drawFadeFrame();
    }
    // Wait for crash sound to finish
    if (Mix_Playing(viewer.fadChannel) == 0) {
      if (fadeMode < Viewer::FADE_DEATH) {
        // Show message phase
        fadePhase = FADE_PHASE_MESSAGE;
        stateStartTime = now;
        stateWaitTime = viewer.midPause;
      } else {
        // Death/victory - wait for key
        viewer.VCTFAD = 0; // Ensure wizard fully visible
        fadePhase = FADE_PHASE_WAIT_KEY;
      }
    }
    break;

  case FADE_PHASE_MESSAGE:
    // Draw every frame
    if (shouldDraw) {
      drawFadeFrame();
    }
    // Check for key during message
    if (fadeMode != Viewer::FADE_MIDDLE && scheduler.keyCheck()) {
      Mix_HaltChannel(viewer.fadChannel);
      viewer.clearArea(&viewer.TXTPRI);
      SDL_Event event;
      while (SDL_PollEvent(&event))
        ;
      fadeInterrupted = false;
      fadePhase = FADE_PHASE_DONE;
      return true;
    }
    // Wait for message duration
    if (now >= stateStartTime + stateWaitTime) {
      // Play second crash
      Mix_PlayChannel(viewer.fadChannel, creature.kaboom, 0);
      fadePhase = FADE_PHASE_CRASH2;
      stateStartTime = now;
    }
    break;

  case FADE_PHASE_CRASH2:
    // Draw every frame
    if (shouldDraw) {
      drawFadeFrame();
    }
    // Wait for crash sound
    if (Mix_Playing(viewer.fadChannel) == 0) {
      // Start buzz out
      Mix_Volume(viewer.fadChannel, 0);
      Mix_PlayChannel(viewer.fadChannel, creature.buzz, -1);
      fadeVCTFAD = 0;
      fadePhase = FADE_PHASE_BUZZ_OUT;
      stateStartTime = now; // Reset for buzz out timing
    }
    break;

  case FADE_PHASE_BUZZ_OUT:
    if (fadeVCTFAD <= 32) {
      Mix_Volume(viewer.fadChannel,
                 static_cast<int>(
                     ((32 - fadeVCTFAD) / 2) *
                     ((oslink.volumeLevel * MIX_MAX_VOLUME) / 128.0 / 16.0)));
      viewer.VCTFAD = fadeVCTFAD;

      // Draw every frame
      if (shouldDraw) {
        drawFadeFrame();
      }

      // Check for key
      if (fadeMode != Viewer::FADE_MIDDLE && scheduler.keyCheck()) {
        Mix_HaltChannel(viewer.fadChannel);
        viewer.clearArea(&viewer.TXTPRI);
        SDL_Event event;
        while (SDL_PollEvent(&event))
          ;
        fadeInterrupted = false;
        fadePhase = FADE_PHASE_DONE;
        return true;
      }

      // Only advance fade value every buzzStep
      if (shouldAdvanceFade) {
        fadeVCTFAD += 2;
      }
    } else {
      Mix_HaltChannel(viewer.fadChannel);
      viewer.clearArea(&viewer.TXTPRI);
      SDL_Event event;
      while (SDL_PollEvent(&event))
        ;
      fadeInterrupted = true; // No key pressed during intro = demo mode
      fadePhase = FADE_PHASE_DONE;
      return true;
    }
    break;

  case FADE_PHASE_WAIT_KEY:
    // Ensure wizard is fully visible (no fade effect)
    viewer.VCTFAD = 0;
    // Draw every frame
    if (shouldDraw) {
      drawFadeFrame();
    }
    if (scheduler.keyCheck()) {
      viewer.clearArea(&viewer.TXTPRI);
      SDL_Event event;
      while (SDL_PollEvent(&event))
        ;
      fadePhase = FADE_PHASE_DONE;
      return true;
    }
    break;

  case FADE_PHASE_DONE:
    return true;
  }

  return false;
}

// Request death fade from gameplay
void dodGame::requestDeathFade() {
  initFadeState(Viewer::FADE_DEATH);
  returnState = STATE_PLAYING;
  gameState = STATE_DEATH_FADE;
}

// Request victory fade from gameplay
void dodGame::requestVictoryFade() {
  initFadeState(Viewer::FADE_VICTORY);
  returnState = STATE_PLAYING;
  gameState = STATE_WIN_FADE;
}

// Request intermission fade from gameplay
void dodGame::requestIntermissionFade(PostFadeAction postAction) {
  initFadeState(Viewer::FADE_MIDDLE);
  postFadeAction = postAction;
  returnState = STATE_PLAYING;
  gameState = STATE_INTERMISSION_FADE;
}

// Request menu from gameplay
void dodGame::requestMenu() {
  returnState = STATE_PLAYING;
  gameState = STATE_MENU;
  menuRow = 0;
  menuCol = 0;
  menuComplete = false;
  menuReturnValue = -1;
  oslink.menuPending = OS_Link::MENU_PENDING_NONE; // Clear any pending submenu
  scheduler.pause(true); // Pause game while in menu
}

// State machine update - called each frame, returns true when scheduler should run
bool dodGame::updateState() {
  Uint32 now = SDL_GetTicks();

  switch (gameState) {
  case STATE_INIT:
    return false;

  case STATE_FADE_INTRO:
    return updateFadeIntro();

  case STATE_PREPARE_WAIT:
    oslink.process_events();
    if (now >= stateStartTime + stateWaitTime) {
      creature.NEWLVL();
      if (AUTFLG) {
        viewer.display_mode = Viewer::MODE_TITLE;
        viewer.showSeerMap = true;
        --viewer.UPDATE;
        viewer.draw_game();
        gameState = STATE_DEMO_MAP_WAIT;
        stateStartTime = now;
        stateWaitTime = 3000;
      } else {
        INIVU();
        viewer.PROMPT();
        gameState = STATE_PLAYING;
      }
    }
    return false;

  case STATE_DEMO_MAP_WAIT:
    oslink.process_events();
    if (now >= stateStartTime + stateWaitTime) {
      INIVU();
      viewer.PROMPT();
      gameState = STATE_PLAYING;
    }
    return false;

  case STATE_PLAYING:
    return true;

  case STATE_DEATH_FADE:
    return updateDeathFade();

  case STATE_WIN_FADE:
    return updateVictoryFade();

  case STATE_INTERMISSION_FADE:
    return updateIntermissionFade();

  case STATE_MENU:
    return updateMenu();

  case STATE_MENU_LIST:
    return updateMenuList();

  case STATE_MENU_SCROLLBAR:
    return updateMenuScrollbar();

  case STATE_MENU_STRING:
    return updateMenuString();

  case STATE_RESTART_WAIT:
    oslink.process_events();
    if (now >= stateStartTime + stateWaitTime) {
      creature.NEWLVL();
      INIVU();
      viewer.PROMPT();
      gameState = STATE_PLAYING;
    }
    return false;

  case STATE_TURN_ANIMATION:
    return updateTurnAnimation();

  case STATE_MOVE_ANIMATION:
    return updateMoveAnimation();

  case STATE_FAINT_ANIMATION:
    return updateFaintAnimation();

  case STATE_RECOVER_ANIMATION:
    return updateRecoverAnimation();

  default:
    return false;
  }
}

bool dodGame::updateFadeIntro() {
  // Don't call process_events() here - let keyCheck() in processFadeFrame handle events
  // so that key presses can skip the intro fade
  if (processFadeFrame()) {
    // Fade complete
    AUTFLG = fadeInterrupted; // true = demo mode (no key pressed)
    player.setInitialObjects(AUTFLG);
    viewer.displayPrepare();
    viewer.display_mode = Viewer::MODE_TITLE;
    viewer.draw_game();

    gameState = STATE_PREPARE_WAIT;
    stateStartTime = SDL_GetTicks();
    stateWaitTime = viewer.prepPause;
  }
  return false;
}

bool dodGame::updateDeathFade() {
  // Don't call process_events() - let keyCheck() in processFadeFrame handle events
  // so key press during WAIT_KEY phase can be detected
  if (processFadeFrame()) {
    // Death fade complete - keep state as STATE_DEATH_FADE
    // render() will detect this and handle restart
    return true; // Signal that fade is complete
  }
  return false;
}

bool dodGame::updateVictoryFade() {
  // Don't call process_events() - let keyCheck() in processFadeFrame handle events
  // so key press during WAIT_KEY phase can be detected
  if (processFadeFrame()) {
    // Victory fade complete - keep state as STATE_WIN_FADE
    // render() will detect this and handle restart
    hasWon = true;
    return true; // Signal that fade is complete
  }
  return false;
}

bool dodGame::updateIntermissionFade() {
  oslink.process_events();
  if (processFadeFrame()) {
    // Intermission complete - handle post-fade action
    PostFadeAction action = postFadeAction;
    postFadeAction = POST_FADE_NONE;

    if (action == POST_FADE_LEVEL3_SETUP) {
      // Setup level 3 after killing wizard image
      player.BAGPTR = player.PTORCH;
      if (player.PTORCH != -1) {
        object.OCBLND[player.PTORCH].P_OCPTR = -1;
      }
      player.POBJWT = 200;
      LEVEL = 3;
      creature.NEWLVL();

      // Find random position
      dodBYTE c, r, val;
      do {
        c = (rng.RANDOM() & 31);
        r = (rng.RANDOM() & 31);
        val = dungeon.MAZLND[dungeon.RC2IDX(r, c)];
      } while (val == 0xFF);
      player.PROW = r;
      player.PCOL = c;

      INIVU();
    }

    gameState = STATE_PLAYING;
    return true;
  }
  return false;
}

bool dodGame::updateMenu() {
  Uint32 now = SDL_GetTicks();
  if (now < nextFrameTime) {
    return false;
  }
  nextFrameTime = now + 16;

  // Check if returning from a submenu
  if (menuComplete && oslink.menuPending != OS_Link::MENU_PENDING_NONE) {
    // Apply the submenu result
    int result = menuReturnValue;
    int pendingId = oslink.menuPendingId;
    int pendingItem = oslink.menuPendingItem;
    oslink.menuPending = OS_Link::MENU_PENDING_NONE;
    menuComplete = false;

    bool closeMenu = false; // Whether to close menu after applying result

    // Apply result based on which menu item triggered it
    if (pendingId == FILE_MENU_SWITCH) {
      switch (pendingItem) {
      case FILE_MENU_GRAPHICS:
        if (result >= 0) {
          switch (result) {
          case 0: g_options &= ~(OPT_VECTOR | OPT_HIRES); break;
          case 1: g_options &= ~(OPT_VECTOR); g_options |= OPT_HIRES; break;
          case 2: g_options &= ~(OPT_HIRES); g_options |= OPT_VECTOR; break;
          }
          closeMenu = true; // Graphics selection closes menu
        }
        break;
      case FILE_MENU_CREATURE_SPEED:
        if (result != creature.creSpeedMul) {
          creature.creSpeedMul = result;
          creature.UpdateCreSpeed();
        }
        // Does not close menu
        break;
      case FILE_MENU_TURN_DELAY:
        player.turnDelay = result;
        // Does not close menu
        break;
      case FILE_MENU_MOVE_DELAY:
        player.moveDelay = result;
        // Does not close menu
        break;
      case FILE_MENU_CREATURE_REGEN:
        if (result != oslink.creatureRegen) {
          oslink.creatureRegen = result;
          scheduler.updateCreatureRegen(oslink.creatureRegen);
        }
        // Does not close menu
        break;
      case FILE_MENU_VOLUME:
        oslink.volumeLevel = result;
        Mix_Volume(-1, static_cast<int>((oslink.volumeLevel * MIX_MAX_VOLUME) / 128));
        // Does not close menu
        break;
      case FILE_MENU_RANDOM_MAZE:
        if (result == 0) RandomMaze = true;
        else if (result == 1) RandomMaze = false;
        // Does not close menu
        break;
      case FILE_MENU_SND_MODE:
        if (result == 0) g_options |= OPT_STEREO;
        else if (result == 1) g_options &= ~OPT_STEREO;
        // Does not close menu
        break;
      }
    }

    if (closeMenu) {
      scheduler.pause(false);
      gameState = STATE_PLAYING;
      return true;
    }
  }

  // Draw menu
  static menu mainMenu;
  viewer.drawMenu(mainMenu, menuCol, menuRow);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_RETURN: {
        // Handle menu selection
        // Store pending context before calling menu return
        oslink.menuPendingId = menuCol;
        oslink.menuPendingItem = menuRow;
        bool done = oslink.menuReturn(menuCol, menuRow, mainMenu);
        // Check if a submenu was started
        if (oslink.menuPending != OS_Link::MENU_PENDING_NONE) {
          // Submenu started, will return here when complete
          return false;
        }
        if (done) {
          scheduler.pause(false);
          if (menuCol == FILE_MENU_SWITCH && menuRow == FILE_MENU_NEW) {
            if (!AUTFLG) {
              hasWon = true;
              demoRestart = false;
            }
          }
          gameState = STATE_PLAYING;
          return true;
        }
        break;
      }
      case SDLK_UP:
        menuRow = (menuRow < 1) ? mainMenu.getMenuSize(menuCol) - 1 : menuRow - 1;
        break;
      case SDLK_DOWN:
        menuRow = (menuRow > mainMenu.getMenuSize(menuCol) - 2) ? 0 : menuRow + 1;
        break;
      case SDLK_LEFT:
        if (NUM_MENU > 1) {
          menuCol = (menuCol < 1) ? NUM_MENU - 1 : menuCol - 1;
          menuRow = 0;
        }
        break;
      case SDLK_RIGHT:
        if (NUM_MENU > 1) {
          menuCol = (menuCol > NUM_MENU - 2) ? 0 : menuCol + 1;
          menuRow = 0;
        }
        break;
      case SDLK_ESCAPE:
        scheduler.pause(false);
        gameState = STATE_PLAYING;
        return true;
      default:
        break;
      }
      break;
    case SDL_QUIT:
      oslink.quitSDL(0);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_GL_SwapWindow(oslink.sdlWindow);
      break;
    }
  }

  return false;
}

bool dodGame::updateMenuList() {
  Uint32 now = SDL_GetTicks();
  if (now < nextFrameTime) {
    return false;
  }
  nextFrameTime = now + 16;

  viewer.drawMenuList(menuX, menuY, const_cast<char*>(menuTitle.c_str()),
                      menuList, menuListSize, menuListChoice);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_RETURN:
        menuReturnValue = menuListChoice;
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      case SDLK_UP:
        menuListChoice = (menuListChoice < 1) ? menuListSize - 1 : menuListChoice - 1;
        break;
      case SDLK_DOWN:
        menuListChoice = (menuListChoice > menuListSize - 2) ? 0 : menuListChoice + 1;
        break;
      case SDLK_ESCAPE:
        menuReturnValue = -1;
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      default:
        break;
      }
      break;
    case SDL_QUIT:
      oslink.quitSDL(0);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_GL_SwapWindow(oslink.sdlWindow);
      break;
    }
  }

  return false;
}

bool dodGame::updateMenuScrollbar() {
  Uint32 now = SDL_GetTicks();
  if (now < nextFrameTime) {
    return false;
  }
  nextFrameTime = now + 16;

  viewer.drawMenuScrollbar(menuTitle, menuScrollPosition);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_RETURN: {
        if (menuScrollPosition == menuOriginalScrollPos) {
          menuReturnValue = menuScrollMin + (menuOriginalScrollPos * (menuScrollMax - menuScrollMin) / 31);
        } else {
          int range = menuScrollMax - menuScrollMin;
          menuReturnValue = static_cast<int>(std::round(
              static_cast<double>(menuScrollMin) +
              (static_cast<double>(menuScrollPosition) * static_cast<double>(range) / 31.0)));
          menuReturnValue = std::max(menuScrollMin, std::min(menuScrollMax, menuReturnValue));
        }
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      }
      case SDLK_LEFT:
        menuScrollPosition = (menuScrollPosition > 0) ? menuScrollPosition - 1 : 0;
        break;
      case SDLK_RIGHT:
        menuScrollPosition = (menuScrollPosition < 31) ? menuScrollPosition + 1 : 31;
        break;
      case SDLK_ESCAPE:
        menuReturnValue = menuScrollMin + (menuOriginalScrollPos * (menuScrollMax - menuScrollMin) / 31);
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      default:
        break;
      }
      break;
    case SDL_QUIT:
      oslink.quitSDL(0);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_GL_SwapWindow(oslink.sdlWindow);
      break;
    }
  }

  return false;
}

bool dodGame::updateMenuString() {
  Uint32 now = SDL_GetTicks();
  if (now < nextFrameTime) {
    return false;
  }
  nextFrameTime = now + 16;

  viewer.drawMenuStringTitle(const_cast<char*>(menuTitle.c_str()));
  viewer.drawMenuString(menuStringBuffer);

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      case SDLK_RETURN:
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      case SDLK_RSHIFT:
      case SDLK_LSHIFT:
      case SDLK_RCTRL:
      case SDLK_LCTRL:
      case SDLK_RALT:
      case SDLK_LALT:
      case SDLK_RGUI:
      case SDLK_LGUI:
      case SDLK_MODE:
      case SDLK_APPLICATION:
      case SDLK_NUMLOCKCLEAR:
      case SDLK_CAPSLOCK:
      case SDLK_SCROLLLOCK:
      case SDLK_UP:
      case SDLK_DOWN:
        // Ignore these keys
        break;
      case SDLK_BACKSPACE:
      case SDLK_LEFT:
        if (strlen(menuStringBuffer) > 0) {
          menuStringBuffer[strlen(menuStringBuffer) - 1] = '\0';
        }
        break;
      case SDLK_ESCAPE:
        menuStringBuffer[0] = '\0';
        menuComplete = true;
        gameState = STATE_MENU;
        return false;
      default:
        if (strlen(menuStringBuffer) < static_cast<size_t>(menuMaxLength)) {
          size_t len = strlen(menuStringBuffer);
          menuStringBuffer[len] = oslink.keys[event.key.keysym.sym];
          menuStringBuffer[len + 1] = '\0';
        }
        break;
      }
      break;
    case SDL_QUIT:
      oslink.quitSDL(0);
      break;
    case SDL_WINDOWEVENT_EXPOSED:
      SDL_GL_SwapWindow(oslink.sdlWindow);
      break;
    }
  }

  return false;
}

void dodGame::Restart() {
  object.Reset();
  creature.Reset();
  parser.Reset();
  player.Reset();
  scheduler.Reset();
  viewer.Reset();
  hasWon = false;

  dungeon.VFTPTR = 0;
  scheduler.SYSTCB();
  object.CreateAll();
  player.HBEATF = 0;
  player.setInitialObjects(false);
  viewer.displayPrepare();
  viewer.displayCopyright();
  viewer.display_mode = Viewer::MODE_TITLE;
  viewer.draw_game();

  // Non-blocking: set state to wait
  gameState = STATE_RESTART_WAIT;
  stateStartTime = SDL_GetTicks();
  stateWaitTime = 2500;
}

void dodGame::LoadGame() {
  scheduler.LOAD();
  viewer.setVidInv((game.LEVEL % 2) ? true : false);
  --viewer.UPDATE;
  viewer.draw_game();
  INIVU();
  viewer.PROMPT();
}

// Initializes 3D viewer
void dodGame::INIVU() {
  viewer.clearArea(&viewer.TXTSTS);
  viewer.clearArea(&viewer.TXTPRI);
  player.HUPDAT();
  ++player.HEARTC;
  --player.HEARTF;
  --player.HBEATF;
  viewer.STATUS();
  player.PLOOK();
}

// Pause 1.5 seconds - non-blocking version
void dodGame::WAIT() {
  scheduler.curTime = SDL_GetTicks();
  if (scheduler.curTime >= scheduler.TCBLND[0].next_time) {
    scheduler.CLOCK();
  }
  scheduler.EscCheck();
}

// Request a turn animation (non-blocking)
void dodGame::requestTurnAnimation(dodBYTE direction) {
  animationType = ANIM_NONE;
  animFrame = 0;
  animTotalFrames = 8; // 8 lines to draw

  // Parser::DIR_LEFT=0, DIR_RIGHT=1, DIR_BACK=2, DIR_AROUND=3
  switch (direction) {
  case 0: // DIR_LEFT
    animationType = ANIM_TURN_LEFT;
    animOffset = 8;
    animDir = 1;
    animTotalFrames = 8;
    break;
  case 1: // DIR_RIGHT
    animationType = ANIM_TURN_RIGHT;
    animOffset = 248;
    animDir = -1;
    animTotalFrames = 8;
    break;
  case 3: // DIR_AROUND
    animationType = ANIM_TURN_AROUND;
    animOffset = 248;
    animDir = -1;
    animTotalFrames = 16; // 2 times through
    break;
  default:
    return; // Unknown direction
  }

  // Set up viewer for animation
  viewer.VXSCAL = 0x80;
  viewer.VYSCAL = 0x80;
  viewer.VXSCALf = 128.0f;
  viewer.VYSCALf = 128.0f;
  viewer.RANGE = 0;
  viewer.SETFAD();
  player.turning = true;

  animFrameStart = SDL_GetTicks();
  animFrameDuration = player.turnDelay;
  returnState = STATE_PLAYING;
  gameState = STATE_TURN_ANIMATION;
}

// Request a move animation (non-blocking)
void dodGame::requestMoveAnimation(dodBYTE direction) {
  animMoveDir = direction;
  animPhase = 0; // Start with first half
  animFrame = 0;

  if (direction == 0) {
    // Move forward
    animationType = ANIM_MOVE_FORWARD;
    --viewer.HLFSTP;
  } else if (direction == 2) {
    // Move back
    animationType = ANIM_MOVE_BACK;
    --viewer.BAKSTP;
  } else {
    return; // Side moves don't use this animation
  }

  viewer.PUPDAT();
  animFrameStart = SDL_GetTicks();
  animFrameDuration = player.moveDelay / 2;
  returnState = STATE_PLAYING;
  gameState = STATE_MOVE_ANIMATION;
}

// Update turn animation, returns true when complete
bool dodGame::updateTurnAnimation() {
  oslink.process_events();
  Uint32 now = SDL_GetTicks();

  // Run scheduler clock during animation
  scheduler.curTime = now;
  if (scheduler.curTime >= scheduler.TCBLND[0].next_time) {
    scheduler.CLOCK();
    // Check if demo was interrupted
    if (AUTFLG && demoRestart == false) {
      player.turning = false;
      gameState = STATE_PLAYING;
      return true;
    }
  }

  // Check if frame duration elapsed
  if (now >= animFrameStart + animFrameDuration) {
    animFrame++;
    animFrameStart = now;

    // Check if animation complete
    if (animFrame >= animTotalFrames) {
      player.turning = false;
      --player.HEARTF;
      // Now show the new view after turn animation completes
      --viewer.UPDATE;
      viewer.draw_game();
      gameState = STATE_PLAYING;
      return true;
    }

    // Handle wrap for turn-around (reset at frame 8)
    if (animationType == ANIM_TURN_AROUND && animFrame == 8) {
      // Second pass starts
    }
  }

  // Draw the turn animation frame (just the frame + sweeping line, not dungeon)
  int inc = 32;
  int y0 = 17;
  int y1 = 135;
  int frameInPass = animFrame % 8;

  glClear(GL_COLOR_BUFFER_BIT);
  glLoadIdentity();
  glColor3fv(viewer.fgColor);

  // Draw the frame (two horizontal lines at top and bottom of view area)
  viewer.drawVectorList(viewer.LINES);

  // Draw the sweeping turn line
  viewer.drawVector((frameInPass * inc * animDir) + animOffset, y0,
                    (frameInPass * inc * animDir) + animOffset, y1);

  // Draw status and text areas
  viewer.drawArea(&viewer.TXTSTS);
  viewer.drawArea(&viewer.TXTPRI);
  SDL_GL_SwapWindow(oslink.sdlWindow);

  return false;
}

// Update move animation, returns true when complete
bool dodGame::updateMoveAnimation() {
  oslink.process_events();
  Uint32 now = SDL_GetTicks();

  // Run scheduler clock during animation
  scheduler.curTime = now;
  if (scheduler.curTime >= scheduler.TCBLND[0].next_time) {
    scheduler.CLOCK();
    // Check if demo was interrupted
    if (AUTFLG && demoRestart == false) {
      // Reset view state
      if (animationType == ANIM_MOVE_FORWARD) {
        viewer.HLFSTP = 0;
      } else {
        viewer.BAKSTP = 0;
      }
      gameState = STATE_PLAYING;
      return true;
    }
  }

  // Check if phase duration elapsed
  if (now >= animFrameStart + animFrameDuration) {
    animPhase++;

    if (animPhase == 1) {
      // First half done - do the actual step
      if (animationType == ANIM_MOVE_FORWARD) {
        viewer.HLFSTP = 0;
        player.PSTEP(0);
      } else {
        viewer.BAKSTP = 0;
        player.PSTEP(2);
      }
      player.PDAM += (player.POBJWT >> 3) + 3;
      player.HUPDAT();
      // Check if HUPDAT triggered a faint - if so, exit move animation immediately
      // The faint animation has already drawn its first frame
      if (gameState == STATE_FAINT_ANIMATION) {
        return false; // Faint animation now in control
      }
      --viewer.UPDATE;
      viewer.draw_game();
      animFrameStart = now;
    } else {
      // Animation complete
      gameState = STATE_PLAYING;
      return true;
    }
  }

  return false;
}

// Request faint animation (non-blocking, heartbeat racing with screen dimming)
void dodGame::requestFaintAnimation() {
  gameState = STATE_FAINT_ANIMATION;
  // Original target: signed -8 -> unsigned 248
  faintTargetLight = 248;
  faintIsDeath = false;
  faintStepCount = 0;
  faintStartLight = viewer.RLIGHT; // Save starting light for reference
  animFrameDuration = 750; // 750ms per step, matching original

  // Do the first step immediately (matching original do-while behavior)
  // Original: decrement MLIGHT, draw, decrement RLIGHT, then wait
  --viewer.MLIGHT;
  --viewer.UPDATE;
  // Debug: log RLIGHT values to understand the animation
  printf("FAINT START: RLIGHT=%d, MLIGHT=%d, OLIGHT=%d\n",
         (int)viewer.RLIGHT, (int)viewer.MLIGHT, (int)viewer.OLIGHT);
  viewer.draw_game();
  --viewer.RLIGHT;
  ++faintStepCount;
  animFrameStart = SDL_GetTicks(); // Start timer for next step

  // Check if animation completed on first step (RLIGHT was already close to 248)
  // This shouldn't happen in normal gameplay but handle it gracefully
  if (viewer.RLIGHT == (dodBYTE)faintTargetLight) {
    viewer.RLIGHT = (dodBYTE)faintTargetLight;
    viewer.MLIGHT = (dodBYTE)faintTargetLight;
    --viewer.UPDATE;
    parser.KBDHDR = 0;
    parser.KBDTAL = 0;
    // Stay in FAINT_ANIMATION state - updateFaintAnimation will handle completion
  }
}

// Request recover animation (non-blocking, heartbeat with screen brightening)
void dodGame::requestRecoverAnimation() {
  gameState = STATE_RECOVER_ANIMATION;
  faintTargetLight = viewer.OLIGHT; // Restore to original light level
  faintStepCount = 0;

  // IMPORTANT: PUPDAT may have reset RLIGHT between faint completion and recovery start.
  // Ensure we start recovery from the fainted state (248 = dark).
  // This matches original behavior where you stay dark until recovery animation runs.
  if (viewer.RLIGHT != 248) {
    viewer.RLIGHT = 248;
    viewer.MLIGHT = 248;
  }
  faintStartLight = viewer.RLIGHT; // Should be 248

  animFrameDuration = 750; // 750ms per step, matching original

  // Do the first step immediately (matching original do-while behavior)
  // Original: draw, increment MLIGHT and RLIGHT, then wait
  --viewer.UPDATE;
  viewer.draw_game();
  ++viewer.MLIGHT;
  ++viewer.RLIGHT;
  ++faintStepCount;
  animFrameStart = SDL_GetTicks(); // Start timer for next step
}

// Update faint animation (screen dims while heartbeat races)
bool dodGame::updateFaintAnimation() {
  oslink.process_events();
  Uint32 now = SDL_GetTicks();

  // Run scheduler clock during animation (this drives the heartbeat!)
  scheduler.curTime = now;
  if (scheduler.curTime >= scheduler.TCBLND[0].next_time) {
    scheduler.CLOCK();
    scheduler.EscCheck();
  }

  // Check if step duration elapsed
  if (now >= animFrameStart + animFrameDuration) {
    // Original behavior: decrement MLIGHT first, draw, then decrement RLIGHT
    // This uses unsigned byte wraparound (e.g., 0 -> 255 -> 254 -> ... -> 248)
    --viewer.MLIGHT;
    --viewer.UPDATE;
    // Debug: log each faint step
    printf("FAINT STEP %d: RLIGHT=%d (drawing), target=%d\n",
           faintStepCount, (int)viewer.RLIGHT, faintTargetLight);
    viewer.draw_game();
    --viewer.RLIGHT;
    ++faintStepCount;
    animFrameStart = now;

    // Check if faint animation complete (original condition: RLIGHT == 248)
    // Also add maximum step limit to prevent infinite loop
    if (viewer.RLIGHT == (dodBYTE)faintTargetLight || faintStepCount >= 256) {
      // Faint complete - ensure light levels are exactly at target (dark)
      printf("FAINT COMPLETE: RLIGHT=%d after %d steps\n",
             (int)viewer.RLIGHT, faintStepCount);
      viewer.RLIGHT = (dodBYTE)faintTargetLight;
      viewer.MLIGHT = (dodBYTE)faintTargetLight;
      --viewer.UPDATE;
      parser.KBDHDR = 0;
      parser.KBDTAL = 0;

      // Check if player is dead (power < damage)
      if (player.PLRBLK.P_ATPOW < player.PLRBLK.P_ATDAM) {
        // Trigger death fade
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
          ; // clear event buffer
        viewer.clearArea(&viewer.TXTSTS);
        viewer.clearArea(&viewer.TXTPRI);
        requestDeathFade();
        // Return false so render() doesn't think we're ready for scheduler
        // Death fade will now run on next frame
        return false;
      } else {
        // Just fainted, return to playing (will recover when HEARTR > 3)
        gameState = STATE_PLAYING;
        return true;
      }
    }
  }

  return false;
}

// Update recover animation (screen brightens as player recovers)
bool dodGame::updateRecoverAnimation() {
  oslink.process_events();
  Uint32 now = SDL_GetTicks();

  // Run scheduler clock during animation (keeps heartbeat going)
  scheduler.curTime = now;
  if (scheduler.curTime >= scheduler.TCBLND[0].next_time) {
    scheduler.CLOCK();
    scheduler.EscCheck();
  }

  // Check if step duration elapsed
  if (now >= animFrameStart + animFrameDuration) {
    // Original behavior: draw first, then increment both
    // This uses unsigned byte wraparound (e.g., 248 -> 249 -> ... -> 255 -> 0 -> ... -> OLIGHT)
    --viewer.UPDATE;
    viewer.draw_game();
    ++viewer.MLIGHT;
    ++viewer.RLIGHT;
    ++faintStepCount;
    animFrameStart = now;

    // Check if recover animation complete (original condition: RLIGHT == OLIGHT)
    // Use saved faintTargetLight in case viewer.OLIGHT changed
    // Also add maximum step limit to prevent infinite loop
    if (viewer.RLIGHT == (dodBYTE)faintTargetLight || faintStepCount >= 256) {
      // Recovery complete - ensure light levels are exactly at target
      viewer.RLIGHT = (dodBYTE)faintTargetLight;
      viewer.MLIGHT = (dodBYTE)faintTargetLight;
      player.FAINT = 0;
      viewer.PROMPT();
      --viewer.UPDATE;

      // Check if player died during recovery (power < damage)
      if (player.PLRBLK.P_ATPOW < player.PLRBLK.P_ATDAM) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
          ; // clear event buffer
        viewer.clearArea(&viewer.TXTSTS);
        viewer.clearArea(&viewer.TXTPRI);
        requestDeathFade();
        // Return false so render() doesn't think we're ready for scheduler
        return false;
      } else {
        gameState = STATE_PLAYING;
        return true;
      }
    }
  }

  return false;
}
