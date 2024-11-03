# Makefile

# Compiler
EMCC = emcc

# Source files
SRC = main.cpp

ASSETS_DIR = assets

SHELLFILE = shell_minimal.html

# Preload files with mapping: local assets/ to virtual /assets/
PRELOAD = assets@/assets

# Output file name
OUT = index.html

# Compiler flags
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
        --preload-file assets@/assets

# Linker flags
LDFLAGS = 

# Build target
all: $(OUT)

# use shell shell_minimal.html

$(OUT): $(SRC)
	$(EMCC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS) --shell-file $(SHELLFILE)

# Clean build files
clean:
	rm -f $(OUT) *.js *.wasm

.PHONY: all clean
