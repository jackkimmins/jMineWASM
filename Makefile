EMCC = em++
SRC_DIR = src
BUILD_DIR = build
SRC = $(SRC_DIR)/main.cpp
ASSETS_DIR = assets
SHELLFILE = shell_minimal.html
OUT = $(BUILD_DIR)/index.html
CFLAGS = -O3 \
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
        --preload-file $(ASSETS_DIR)@/assets

all: $(OUT)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUT): $(SRC) | $(BUILD_DIR)
	$(EMCC) $(CFLAGS) $(SRC) -o $(OUT) --shell-file $(SHELLFILE)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean