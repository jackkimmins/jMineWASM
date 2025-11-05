EMCC = em++
CXX = g++
SRC_DIR = src
SHARED_DIR = shared
SERVER_DIR = server
CLIENT_DIR = client
BUILD_DIR = build
WWW_DIR = www
BIN_DIR = bin
SRC = $(SRC_DIR)/main.cpp
ASSETS_DIR = assets
SHELLFILE = shell_minimal.html
OUT = $(BUILD_DIR)/index.html
SERVER_OUT = $(BIN_DIR)/jMineServer

# Client build flags (WebAssembly)
CLIENT_CFLAGS = -O3 \
        -s USE_WEBGL2=1 \
        -s FULL_ES3=1 \
        -s WASM=1 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s AUTO_JS_LIBRARIES=1 \
        -s TOTAL_MEMORY=536870912 \
        -s TOTAL_STACK=8388608 \
        -s EXPORTED_FUNCTIONS='["_main", "_setPointerLocked"]' \
        -std=c++20 \
        -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap']" \
        -s ASSERTIONS=1 \
        -lwebsocket.js \
        --preload-file $(ASSETS_DIR)@/assets

# Server build flags (native Linux)
SERVER_CFLAGS = -std=c++20 -O3 -pthread -Wall -Wextra
SERVER_LIBS = -lboost_system -lboost_filesystem

# Default target
all: client server

# Client target (browser build)
client: $(OUT)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUT): $(SRC) | $(BUILD_DIR)
	@echo "Building client (WebAssembly)..."
	$(EMCC) $(CLIENT_CFLAGS) $(SRC) -o $(OUT) --shell-file $(SHELLFILE)
	@echo "Client build complete: $(OUT)"

# Server target (native Linux)
server: $(SERVER_OUT)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(SERVER_OUT): $(SERVER_DIR)/main.cpp $(SERVER_DIR)/hub.cpp $(SERVER_DIR)/hub.hpp | $(BIN_DIR)
	@echo "Building server (native Linux)..."
	$(CXX) $(SERVER_CFLAGS) $(SERVER_DIR)/main.cpp $(SERVER_DIR)/hub.cpp -o $(SERVER_OUT) $(SERVER_LIBS)
	@echo "Server build complete: $(SERVER_OUT)"

# Prepare www directory for server deployment
www: client
	@echo "Preparing www directory..."
	mkdir -p $(WWW_DIR)
	cp $(BUILD_DIR)/index.html $(WWW_DIR)/
	cp $(BUILD_DIR)/index.js $(WWW_DIR)/
	cp $(BUILD_DIR)/index.wasm $(WWW_DIR)/
	cp $(BUILD_DIR)/index.data $(WWW_DIR)/
	cp -r $(ASSETS_DIR) $(WWW_DIR)/
	@echo "www directory ready"

# Package everything for deployment
package: all www
	@echo "Packaging complete"
	@echo "Run: $(SERVER_OUT) --root ./$(WWW_DIR) --port 8888"

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR) $(WWW_DIR) $(BIN_DIR)
	@echo "Clean complete"

# Run server with default settings
run: all www
	@echo "Starting server on port 8888..."
	$(SERVER_OUT) --root ./$(WWW_DIR) --port 8888

.PHONY: all client server www package clean run