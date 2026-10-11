CC = clang
CFLAGS = -Wall -Wextra -O2 -Iinclude
OBJCFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -framework Cocoa -framework Metal -framework MetalKit -framework CoreVideo -framework CoreGraphics -framework QuartzCore -framework JavaScriptCore

SRC_C = main.c src/gba_keypad.c
SRC_M = src/gba_bridge.m src/platform_macos.m

OBJ = $(SRC_C:.c=.o) $(SRC_M:.m=.o)
TARGET = bin/minish_cap

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ) minish_cap.entitlements
	@mkdir -p bin
	$(CC) $(OBJ) $(LDFLAGS) -o $@
	codesign --force --sign - --entitlements minish_cap.entitlements $@
	@echo "=========================================================="
	@echo " Build Success: Native binary generated at $(TARGET) (JIT enabled)"
	@echo "=========================================================="

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.m
	$(CC) $(OBJCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) src/*.o *.o $(TARGET)

run: $(TARGET)
	./$(TARGET) "Legend of Zelda, The - The Minish Cap (USA).gba"
