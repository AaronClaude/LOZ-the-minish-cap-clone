#ifndef MC_PLAYER_H
#define MC_PLAYER_H

#include "mc_entity.h"
#include "gba_keypad.h"

// Player Actions (gPlayerState.action in pret/mcap)
#define PLAYER_ACTION_NORMAL        0x01
#define PLAYER_ACTION_SWORD         0x02
#define PLAYER_ACTION_ROLL          0x03
#define PLAYER_ACTION_MINISH_SHRINK 0x04
#define PLAYER_ACTION_MINISH_GROW   0x05
#define PLAYER_ACTION_LIFT          0x06
#define PLAYER_ACTION_HURT          0x07

typedef struct {
    Entity* entity;
    u8 sword_level;
    u8 shield_level;
    u16 rupees;
    u8 current_hearts;  // Quarter hearts (4 = 1 full heart container)
    u8 max_hearts;      // Quarter hearts (12 = 3 hearts at start)
    bool is_minish;
    u8 roll_timer;
    u8 attack_timer;
    u8 invulnerability_timer;

    // Movement speed constants (subpixel precision)
    s16 speed_x;
    s16 speed_y;
} PlayerState;

void player_init(PlayerState* player, Entity* ent);
void player_update(PlayerState* player, u16 key_state);
void player_attack(PlayerState* player);
void player_roll(PlayerState* player);
void player_toggle_minish(PlayerState* player);
void player_take_damage(PlayerState* player, u8 damage, s8 knockback_x, s8 knockback_y);

#endif // MC_PLAYER_H

