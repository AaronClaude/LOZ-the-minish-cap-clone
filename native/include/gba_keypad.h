#ifndef GBA_KEYPAD_H
#define GBA_KEYPAD_H

#include "gba_types.h"
#include <stdatomic.h>

// GBA Keypad bit positions
#define GBA_KEY_A      0
#define GBA_KEY_B      1
#define GBA_KEY_SELECT 2
#define GBA_KEY_START  3
#define GBA_KEY_RIGHT  4
#define GBA_KEY_LEFT   5
#define GBA_KEY_UP     6
#define GBA_KEY_DOWN   7
#define GBA_KEY_R      8
#define GBA_KEY_L      9

typedef struct {
    _Atomic uint16_t key_state; // Active low (0 = pressed, 1 = unpressed)
} GbaKeypad;

void keypad_init(GbaKeypad* keypad);
void keypad_press(GbaKeypad* keypad, u8 key);
void keypad_release(GbaKeypad* keypad, u8 key);
u16  keypad_get_state(const GbaKeypad* keypad);

#endif // GBA_KEYPAD_H
