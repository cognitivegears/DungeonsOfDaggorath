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
// Filename: sched.h
//
// This class manages the scheduler.

#ifndef DOD_SCHEDULER_HEADER
#define DOD_SCHEDULER_HEADER

#include "dod.h"
#include <functional>
#include <vector>

class Scheduler {
public:
  // Constructor
  Scheduler();

  // Scaffolding
  // void		printTrace(char * st, int rout);
  // void		printClock();

  // Public Interface
  void SYSTCB();
  bool SCHED();
  void CLOCK();
  int GETTCB();
  bool fadeLoop();
  void deathFadeLoop();
  void winFadeLoop();
  bool keyCheck();
  bool keyHandler(const SDL_Keysym *keysym);
  void Reset();
  void SAVE();
  void LOAD();
  void LoadSounds();
  bool EscCheck();
  bool EscHandler(const SDL_Keysym *keysym);
  void pause(bool state);
  void updateCreatureRegen(int newTime);
  void ConfigureChannelSync(int channelCount);

  using WaitPump = std::function<bool()>;
  // nonBlocking: if true, returns immediately without waiting for audio.
  // Use this for sounds that don't need synchronous completion (e.g., heartbeat)
  // to avoid iOS Safari ASYNCIFY timeout issues.
  bool WaitForChannel(int channel, const WaitPump &pump = WaitPump(), bool nonBlocking = false);

  // Public Data Fields
  Task TCBLND[38];

  dodBYTE DERR[15];

  enum { // task IDs
    TID_CLOCK = 0,
    TID_PLAYER = 1,
    TID_REFRESH_DISP = 2,
    TID_HRTSLOW = 3,
    TID_TORCHBURN = 4,
    TID_CRTREGEN = 5,
    TID_CRTMOVE = 6,
  };

  Uint32 curTime;
  Uint32 elapsedTime;

  // Delta-time tracking for frame-independent timing
  Uint32 lastFrameTime;
  Uint32 accumulator;
  static const Uint32 TICK_STEP = 17; // Fixed tick step in ms (original game timing)
  static const Uint32 MAX_CATCHUP = 10; // Max ticks to process per frame to avoid spiral

  Mix_Chunk *hrtSound[2];
  int hrtChannel;

  dodBYTE ZFLAG;

private:
  // Data Fields
  int TCBPTR;
  dodBYTE KBDHDR;
  dodBYTE KBDTAL;

  dodBYTE SLEEP;
  dodBYTE NOISEF;
  dodBYTE NOISEV;

  int schedCtr = 0;

  static Scheduler *instance;

private:
  void OnChannelFinished(int channel);
  SDL_sem *GetChannelSemaphore(int channel);
  static void ChannelFinishedThunk(int channel);

  std::vector<SDL_sem *> channelSemaphores;
  SDL_mutex *channelMutex = nullptr;
};

#endif // DOD_SCHEDULER_HEADER
