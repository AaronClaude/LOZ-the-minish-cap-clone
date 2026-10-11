#include "gba_keypad.h"

void keypad_init(GbaKeypad* keypad) {
    atomic_init(&keypad->key_state, 0x03FF); // All 10 keys unpressed (active low)
}

void keypad_press(GbaKeypad* keypad, u8 key) {
    if (key <= 9) {
        atomic_fetch_and(&keypad->key_state, (uint16_t)~(1 << key));
    }
}

void keypad_release(GbaKeypad* keypad, u8 key) {
    if (key <= 9) {
        atomic_fetch_or(&keypad->key_state, (uint16_t)(1 << key));
    }
}

u16 keypad_get_state(const GbaKeypad* keypad) {
    return atomic_load(&keypad->key_state);
}
