OBJECTS = creature.o dod.o dodgame.o dungeon.o enhanced.o object.o oslink.o parser.o player.o sched.o shader.o viewer.o

# Single-threaded WASM build - no ASYNCIFY or pthreads
# Timing is handled via delta-time compensation in the scheduler
# Performance optimizations: ENVIRONMENT=web strips Node.js code, GL_POOL_TEMP_BUFFERS reuses GL buffers
EMFLAGS = -s INITIAL_MEMORY=16777216 -s FULL_ES2=1 -s MIN_WEBGL_VERSION=1 -s MAX_WEBGL_VERSION=1 -s ASSERTIONS=0 -s WASM=1 -s EXIT_RUNTIME=0 -s FORCE_FILESYSTEM=1 -s ENVIRONMENT=web -s GL_POOL_TEMP_BUFFERS=1 -lidbfs.js

BUILD_VERSION := $(shell git describe --always --dirty 2>/dev/null)
ifeq ($(strip $(BUILD_VERSION)),)
BUILD_VERSION := dev
endif

BUILD_TIMESTAMP := $(shell date -u +"%y-%m-%dT%H-%MZ" 2>/dev/null)
ifeq ($(strip $(BUILD_TIMESTAMP)),)
BUILD_TIMESTAMP := UNKNOWN
endif

CXXFLAGS = -std=c++11 -c -DBUILD_VERSION=\"$(BUILD_VERSION)\" -DBUILD_TIMESTAMP=\"$(BUILD_TIMESTAMP)\"
ifdef EMSCRIPTEN
#CCLINK  = -s USE_SDL=2 -O3 -s USE_SDL_MIXER=2 -s USE_REGAL=1 --preload-file ../assets@/ -s FULL_ES2=1 -s ASYNCIFY -s WASM=1 -s EXIT_RUNTIME=1
ifdef WEBSITE
OUTPUT  = ../../index.js
CCLINK  = $(EMFLAGS) -s USE_SDL=2 -O3 -flto -s USE_SDL_MIXER=2 -s USE_REGAL=1 --preload-file ../assets@/ -s EXPORTED_FUNCTIONS='["_sendinput", "_stopdemo", "_getinventory","_getfloor", "_main","_triggermenu","_isdemo","_applyconfig"]' -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]'
else
OUTPUT  = ../docs/index.html
CCLINK  = $(EMFLAGS) -s USE_SDL=2 -O3 -flto -s USE_SDL_MIXER=2 -s USE_REGAL=1 --preload-file ../assets@/ --shell-file standalone.html
endif

#CCLINK  = -s USE_SDL=2 -O3 -s USE_SDL_MIXER=2--preload-file ../assets@/ -s FULL_ES2=1 -s ASYNCIFY -s WASM=1 -s EXIT_RUNTIME=1 --shell-file template.html
CXXFLAGS += -O3 -flto
#CXX      = clang++-3.6
else
ifeq ($(OS),Windows_NT)
OUTPUT  = ../dod.exe
CCLINK  = -lopengl32 -lmingw32 -lSDL2main -lSDL2 -mwindows -lSDL2_mixer
else
OUTPUT  = ../dod
CCLINK  = -lGL -lSDL2 -lSDL2_mixer
endif
endif
#RM = rm
# -g is debug

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CXX) -o $(OUTPUT) $(OBJECTS) $(CCLINK)

creature.o: creature.cpp creature.h dod.h
	$(CXX) $(CXXFLAGS) creature.cpp

dod.o: dod.cpp dod.h dodgame.h player.h object.h creature.h dungeon.h sched.h viewer.h oslink.h parser.h
	$(CXX) $(CXXFLAGS) dod.cpp

dodgame.o: dodgame.cpp dodgame.h player.h object.h viewer.h sched.h creature.h parser.h dungeon.h oslink.h dod.h
	$(CXX) $(CXXFLAGS) dodgame.cpp

dungeon.o: dungeon.cpp dungeon.h dodgame.h player.h sched.h dod.h
	$(CXX) $(CXXFLAGS) dungeon.cpp

enhanced.o: enhanced.cpp oslink.h dodgame.h parser.h enhanced.h dod.h
	$(CXX) $(CXXFLAGS) enhanced.cpp

object.o: object.cpp object.h dodgame.h parser.h oslink.h dod.h
	$(CXX) $(CXXFLAGS) object.cpp

oslink.o: oslink.cpp oslink.h dodgame.h viewer.h sched.h player.h dungeon.h parser.h object.h creature.h enhanced.h dod.h shader.h
	$(CXX) $(CXXFLAGS) oslink.cpp

parser.o: parser.cpp parser.h viewer.h dod.h
	$(CXX) $(CXXFLAGS) parser.cpp

player.o: player.cpp player.h dodgame.h viewer.h sched.h parser.h object.h dungeon.h creature.h oslink.h enhanced.h dod.h
	$(CXX) $(CXXFLAGS) player.cpp

sched.o: sched.cpp sched.h player.h viewer.h oslink.h creature.h parser.h dodgame.h dungeon.h object.h dod.h
	$(CXX) $(CXXFLAGS) sched.cpp

shader.o: shader.cpp shader.h artifact_shader.h dod.h oslink.h
	$(CXX) $(CXXFLAGS) shader.cpp

viewer.o: viewer.cpp viewer.h oslink.h player.h sched.h parser.h object.h dungeon.h creature.h enhanced.h dod.h shader.h
	$(CXX) $(CXXFLAGS) viewer.cpp

clean:
	@echo -n Cleaning...
	$(RM) $(OBJECTS)
	$(RM) $(OUTPUT)
	@echo Done
