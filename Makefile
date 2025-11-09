# --- Toolchains
EMCC        = em++
CXX         = g++

# --- Project layout
SRC_DIR     = src
SHARED_DIR  = shared
SERVER_DIR  = server
CLIENT_DIR  = client
ASSETS_DIR  = assets
SHELLFILE   = shell_minimal.html

# --- Build layout
BUILD_DIR        = build
CLIENT_BUILD     = $(BUILD_DIR)/client
SERVER_BUILD     = $(BUILD_DIR)/server
OBJ_DIR          = $(BUILD_DIR)/obj
CLIENT_OBJ_DIR   = $(OBJ_DIR)/client
SERVER_OBJ_DIR   = $(OBJ_DIR)/server

# --- Sources
CLIENT_SRC  := $(wildcard $(CLIENT_DIR)/*.cpp) \
               $(wildcard $(SRC_DIR)/*.cpp) \
               $(wildcard $(SHARED_DIR)/*.cpp)
SERVER_SRC  := $(wildcard $(SERVER_DIR)/*.cpp) \
               $(wildcard $(SHARED_DIR)/*.cpp)

# --- Objects & deps
CLIENT_OBJS := $(CLIENT_SRC:%=$(CLIENT_OBJ_DIR)/%.o)
SERVER_OBJS := $(SERVER_SRC:%=$(SERVER_OBJ_DIR)/%.o)
CLIENT_DEPS := $(CLIENT_OBJS:.o=.d)
SERVER_DEPS := $(SERVER_OBJS:.o=.d)

# --- Outputs
CLIENT_HTML = $(CLIENT_BUILD)/index.html
SERVER_BIN  = $(SERVER_BUILD)/jMineServer

# --- Flags
DEPFLAGS        = -MMD -MP
WARNINGS = -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable

CLIENT_EMLIBS = -lwebsocket.js

# Client compile flags (compile-only; quiet)
CLIENT_CPPFLAGS = -std=c++20 -O3 $(DEPFLAGS) $(WARNINGS)

# Client link flags (emscripten -s settings)
CLIENT_EMFLAGS = \
  -s USE_WEBGL2=1 \
  -s FULL_ES3=1 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s AUTO_JS_LIBRARIES=1 \
  -s TOTAL_MEMORY=536870912 \
  -s TOTAL_STACK=8388608 \
  -s EXPORTED_FUNCTIONS='["_main","_setPointerLocked"]' \
  -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']" \
  -s ASSERTIONS=1

CLIENT_LINK_FLAGS = $(CLIENT_EMFLAGS) $(CLIENT_EMLIBS) --shell-file $(SHELLFILE) --preload-file $(ASSETS_DIR)@/assets

# Server flags
SERVER_CFLAGS = -std=c++20 -O3 -flto -pthread $(DEPFLAGS) $(WARNINGS)
SERVER_LIBS   = -lboost_system -lboost_filesystem

# Default
all: client server

# --- Client
client: $(CLIENT_HTML)

$(CLIENT_HTML): $(CLIENT_OBJS) | $(CLIENT_BUILD)
	@echo "Linking client → $(CLIENT_HTML)"
	$(EMCC) $(CLIENT_CPPFLAGS) $(CLIENT_OBJS) -o $@ $(CLIENT_LINK_FLAGS)
	# Copy raw assets alongside the preloaded bundle
	@if [ -d "$(ASSETS_DIR)" ]; then \
	  mkdir -p "$(CLIENT_BUILD)/assets"; \
	  cp -a "$(ASSETS_DIR)/." "$(CLIENT_BUILD)/assets/"; \
	fi
	@echo "Client ready in $(CLIENT_BUILD)"

# Client object rule
$(CLIENT_OBJ_DIR)/%.cpp.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(EMCC) $(CLIENT_CPPFLAGS) -c $< -o $@

# --- Server
server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_OBJS) | $(SERVER_BUILD)
	@echo "Linking server → $(SERVER_BIN)"
	$(CXX) $(SERVER_CFLAGS) $(SERVER_OBJS) -o $@ $(SERVER_LIBS)
	@echo "Server ready in $(SERVER_BUILD)"

$(SERVER_OBJ_DIR)/%.cpp.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(CXX) $(SERVER_CFLAGS) -c $< -o $@

# --- Dirs
$(CLIENT_BUILD) $(SERVER_BUILD):
	@mkdir -p $@

# --- Convenience
clean:
	@echo "Cleaning build artifacts..."
	@# If world_save exists, move it aside temporarily
	@if [ -e "$(SERVER_BUILD)/world_save" ]; then \
	  echo "Preserving $(SERVER_BUILD)/world_save"; \
	  mv "$(SERVER_BUILD)/world_save" "$(BUILD_DIR).world_save"; \
	fi
	@# Remove the whole build directory
	rm -rf "$(BUILD_DIR)"
	@# Restore world_save into the (new) server build dir if we saved it
	@if [ -e "$(BUILD_DIR).world_save" ]; then \
	  mkdir -p "$(SERVER_BUILD)"; \
	  mv "$(BUILD_DIR).world_save" "$(SERVER_BUILD)/world_save"; \
	fi
	@echo "Clean complete"

# Start from build/server so world saves land there; serve build/client
run: all
	@echo "Starting server on port 8888 (serving from ../client)..."
	cd "$(SERVER_BUILD)" && ./jMineServer --root ../client --port 8888

.PHONY: all client server clean run
-include $(CLIENT_DEPS)
-include $(SERVER_DEPS)
