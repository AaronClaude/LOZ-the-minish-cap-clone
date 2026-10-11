#include "mc_player.h"
#include <string.h>

void player_init(PlayerState* player, Entity* ent) {
    memset(player, 0, sizeof(PlayerState));
    player->entity = ent;
    player->max_hearts = 12;      // 3 full hearts
    player->current_hearts = 12;  // Full health
    player->sword_level = 1;      // Smith's Sword
    player->is_minish = false;

    if (ent) {
        ent->type = ENTITY_TYPE_PLAYER;
        ent->action = PLAYER_ACTION_NORMAL;
        ent->direction = MC_DIR_SOUTH;
        ent->speed = 2;
    }
}

void player_update(PlayerState* player, u16 key_state) {
    if (!player || !player->entity) return;
    Entity* ent = player->entity;

    if (player->invulnerability_timer > 0) {
        player->invulnerability_timer--;
    }

    // State Machine
    switch (ent->action) {
        case PLAYER_ACTION_SWORD:
            if (player->attack_timer > 0) {
                player->attack_timer--;
            } else {
                ent->action = PLAYER_ACTION_NORMAL;
            }
            return;

        case PLAYER_ACTION_ROLL:
            if (player->roll_timer > 0) {
                player->roll_timer--;
                // Fast roll movement in facing direction
                s16 roll_speed = player->is_minish ? 2 : 4;
                switch (ent->direction) {
                    case MC_DIR_NORTH: ent->y -= roll_speed; break;
                    case MC_DIR_SOUTH: ent->y += roll_speed; break;
                    case MC_DIR_WEST:  ent->x -= roll_speed; break;
                    case MC_DIR_EAST:  ent->x += roll_speed; break;
                }
            } else {
                ent->action = PLAYER_ACTION_NORMAL;
            }
            return;

        case PLAYER_ACTION_NORMAL:
        default: {
            s16 dx = 0;
            s16 dy = 0;
            s16 spd = player->is_minish ? 1 : 2;

            // Check D-Pad (active low: 0 = pressed)
            if (!(key_state & (1 << GBA_KEY_UP)))    { dy -= spd; ent->direction = MC_DIR_NORTH; }
            if (!(key_state & (1 << GBA_KEY_DOWN)))  { dy += spd; ent->direction = MC_DIR_SOUTH; }
            if (!(key_state & (1 << GBA_KEY_LEFT)))  { dx -= spd; ent->direction = MC_DIR_WEST; }
            if (!(key_state & (1 << GBA_KEY_RIGHT))) { dx += spd; ent->direction = MC_DIR_EAST; }

            ent->x += dx;
            ent->y += dy;

            // Attack button [A]
            if (!(key_state & (1 << GBA_KEY_A))) {
                player_attack(player);
                return;
            }

            // Roll button [B]
            if (!(key_state & (1 << GBA_KEY_B)) && (dx != 0 || dy != 0)) {
                player_roll(player);
                return;
            }

            // Minish Portal trigger [R]
            if (!(key_state & (1 << GBA_KEY_R))) {
                player_toggle_minish(player);
                return;
            }
            break;
        }
    }
}

void player_attack(PlayerState* player) {
    if (!player || !player->entity) return;
    player->entity->action = PLAYER_ACTION_SWORD;
    player->attack_timer = 12; // 12 frames sword slash animation
}

void player_roll(PlayerState* player) {
    if (!player || !player->entity) return;
    player->entity->action = PLAYER_ACTION_ROLL;
    player->roll_timer = 14; // 14 frames roll animation
}

void player_toggle_minish(PlayerState* player) {
    if (!player || !player->entity) return;
    player->is_minish = !player->is_minish;
    player->entity->action = player->is_minish ? PLAYER_ACTION_MINISH_SHRINK : PLAYER_ACTION_MINISH_GROW;
}

void player_take_damage(PlayerState* player, u8 damage, s8 knockback_x, s8 knockback_y) {
    if (!player || !player->entity || player->invulnerability_timer > 0) return;

    if (player->current_hearts > damage) {
        player->current_hearts -= damage;
    } else {
        player->current_hearts = 0; // Game Over trigger
    }

    player->entity->x += knockback_x;
    player->entity->y += knockback_y;
    player->invulnerability_timer = 30; // Half second invulnerability
}

