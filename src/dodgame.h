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
// Filename: dodgame.h
//
// This class is intended to be a controller class, but
// so much of the functionality is closely associated
// with the primary objects that there is not much going
// on here.  As the game grows, this may change.

#ifndef DOD_GAME_HEADER
#define DOD_GAME_HEADER

#include "dod.h"
#include <string>

class dodGame
{
public:
	// Game states for non-blocking state machine
	enum GameState {
		STATE_INIT,              // Initial state
		STATE_FADE_INTRO,        // Intro fade sequence
		STATE_PREPARE_WAIT,      // Waiting on PREPARE screen
		STATE_DEMO_MAP_WAIT,     // Demo mode showing map
		STATE_PLAYING,           // Normal gameplay
		STATE_DEATH_FADE,        // Death sequence
		STATE_WIN_FADE,          // Victory sequence
		STATE_INTERMISSION_FADE, // After killing wizard's image
		STATE_MENU,              // In-game menu
		STATE_MENU_LIST,         // Menu list selection
		STATE_MENU_SCROLLBAR,    // Menu scrollbar
		STATE_MENU_STRING,       // Menu string input
		STATE_RESTART_WAIT,      // Waiting during restart
		STATE_TURN_ANIMATION,    // Turn animation in progress
		STATE_MOVE_ANIMATION,    // Move animation in progress
		STATE_FAINT_ANIMATION,   // Fainting (screen dimming with heartbeat)
		STATE_RECOVER_ANIMATION, // Recovering from faint (screen brightening)
	};

	// Animation types
	enum AnimationType {
		ANIM_NONE,
		ANIM_TURN_LEFT,
		ANIM_TURN_RIGHT,
		ANIM_TURN_AROUND,
		ANIM_MOVE_FORWARD,
		ANIM_MOVE_BACK,
	};

	// Fade sub-phases
	enum FadePhase {
		FADE_PHASE_BUZZ_IN,      // Buzzing while fading in
		FADE_PHASE_CRASH,        // Playing crash sound
		FADE_PHASE_MESSAGE,      // Showing message (for intro/intermission)
		FADE_PHASE_CRASH2,       // Second crash sound
		FADE_PHASE_BUZZ_OUT,     // Buzzing while fading out
		FADE_PHASE_WAIT_KEY,     // Waiting for key (death/victory)
		FADE_PHASE_DONE,         // Fade complete
	};

	// Post-fade actions (what to do after fade completes)
	enum PostFadeAction {
		POST_FADE_NONE,          // Just return to playing
		POST_FADE_RESTART,       // Restart game
		POST_FADE_LEVEL3_SETUP,  // Setup level 3 after killing wizard image
		POST_FADE_VICTORY,       // Handle victory
	};

	// Constructor
	dodGame();

	// Public Interface
	void COMINI();	// Common initialization
	void INIVU();	// View initialization
	void Restart();
	void LoadGame();
	void WAIT();

	// State machine interface (non-blocking)
	bool updateState();  // Called each frame, returns true when scheduler should run
	GameState getState() const { return gameState; }
	void setState(GameState state) { gameState = state; }

	// Request state transitions (called from gameplay code)
	void requestDeathFade();
	void requestVictoryFade();
	void requestIntermissionFade(PostFadeAction postAction = POST_FADE_NONE);
	void requestMenu();

	// Public Data Fields
	dodBYTE	LEVEL;	// Dungeon level (0-4)
	bool	IsDemo;
	bool	RandomMaze;
	bool	ShieldFix;
	bool	VisionScroll;
	bool	CreaturesIgnoreObjects;
	bool	CreaturesInstaRegen;
	bool	MarkDoorsOnScrollMaps;
	bool	ModernControls;		// Arrow keys, TAB, mouse clicks for controls
	bool	ModernControlsExamineMode; // Toggle for TAB: true=EX, false=L
	bool	AUTFLG;	// Autoplay (demo) flag
	bool	hasWon;
	bool	demoRestart;
	int		DEMOPTR;
	dodBYTE DEMO_CMDS[256];

	// State machine variables
	GameState gameState;
	GameState returnState;       // State to return to after menu/fade
	Uint32 stateStartTime;       // When current state started
	Uint32 stateWaitTime;        // How long to wait in current state
	Uint32 nextFrameTime;        // For rate limiting
	bool initialized;            // Whether COMINI has been called

	// Fade state
	FadePhase fadePhase;
	PostFadeAction postFadeAction; // What to do when fade completes
	int fadeMode;                // Which fade type (FADE_BEGIN, etc.)
	int fadeVCTFAD;              // Fade value counter
	bool fadeInterrupted;        // Was fade interrupted by keypress?

	// Menu state
	int menuRow;
	int menuCol;
	int menuListChoice;
	int menuScrollPosition;
	int menuOriginalScrollPos;
	int menuScrollMin;
	int menuScrollMax;
	int menuListSize;
	int menuMaxLength;
	std::string menuTitle;
	std::string* menuList;       // Pointer to list array
	char* menuStringBuffer;      // Buffer for string input
	int menuX, menuY;            // Menu position
	int menuReturnValue;         // Return value from menu
	bool menuComplete;           // Menu selection complete

	// Animation state (for non-blocking turn/move animations)
	AnimationType animationType;
	int animFrame;               // Current animation frame
	int animTotalFrames;         // Total frames for this animation
	int animDir;                 // Direction for animation (-1 or 1)
	int animOffset;              // X offset for turn animation
	Uint32 animFrameStart;       // When current frame started
	Uint32 animFrameDuration;    // Duration of each frame
	int animPhase;               // Phase within move animation (0=first half, 1=second half)
	dodBYTE animMoveDir;         // Direction of move (for completing move after anim)

	// Request turn/move animations (returns immediately, animation runs via state machine)
	void requestTurnAnimation(dodBYTE direction);
	void requestMoveAnimation(dodBYTE direction);

	// Request faint/recover animations (returns immediately, animation runs via state machine)
	void requestFaintAnimation();
	void requestRecoverAnimation();

	// Faint animation state
	int faintTargetLight;        // Target RLIGHT value for faint (248) or recover (OLIGHT)
	bool faintIsDeath;           // True if faint leads to death
	int faintStepCount;          // Number of steps taken in faint/recover animation
	int faintStartLight;         // Starting RLIGHT value when faint/recover began

private:
	// Internal state handlers
	bool updateFadeIntro();
	bool updateDeathFade();
	bool updateVictoryFade();
	bool updateIntermissionFade();
	bool updateMenu();
	bool updateMenuList();
	bool updateMenuScrollbar();
	bool updateMenuString();
	bool updateTurnAnimation();
	bool updateMoveAnimation();
	bool updateFaintAnimation();
	bool updateRecoverAnimation();

	// Fade helpers
	void initFadeState(int mode);
	bool processFadeFrame();
	void drawFadeFrame();
};

#endif // DOD_GAME_HEADER
