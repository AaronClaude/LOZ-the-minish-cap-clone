#ifndef MC_ENTITY_H
#define MC_ENTITY_H

#include "gba_types.h"

// Minish Cap Entity Types (Capcom Flagship Engine)
#define ENTITY_TYPE_PLAYER       0x00
#define ENTITY_TYPE_ITEM         0x01
#define ENTITY_TYPE_PROJECTILE   0x02
#define ENTITY_TYPE_OBJECT       0x06
#define ENTITY_TYPE_NPC          0x07
#define ENTITY_TYPE_ENEMY        0x08
#define ENTITY_TYPE_BOSS         0x09

// Directions
#define MC_DIR_NORTH 0
#define MC_DIR_EAST  1
#define MC_DIR_SOUTH 2
#define MC_DIR_WEST  3

typedef struct Hitbox {
    s8 offset_x;
    s8 offset_y;
    u8 width;
    u8 height;
} Hitbox;

typedef struct Entity {
    struct Entity* prev;
    struct Entity* next;
    struct Entity* parent;
    struct Entity* child;

    u8 type;
    u8 id;
    u8 form;
    u8 action;
    u8 subAction;
    u8 timer;
    u8 subTimer;
    u8 flags;

    u16 status;
    s16 x;
    s16 y;
    s16 z;

    s8 speed;
    u8 direction;
    u8 animationState;
    u8 frameIndex;
    u8 frameDuration;

    u8 health;
    u8 maxHealth;
    u8 damage;
    u8 collisionLayer;

    Hitbox hitbox;
    u32 spriteOffset;
    u8 paletteIndex;

    bool active;
    void (*update)(struct Entity* self);
    void (*draw)(struct Entity* self, u32* framebuffer);
} Entity;

#define MAX_ENTITIES 64

typedef struct {
    Entity pool[MAX_ENTITIES];
    Entity* active_head;
    u32 active_count;
} EntityManager;

void entity_manager_init(EntityManager* em);
Entity* entity_create(EntityManager* em, u8 type, u8 id, s16 x, s16 y);
void entity_destroy(EntityManager* em, Entity* ent);
void entity_update_all(EntityManager* em);

#endif // MC_ENTITY_H

