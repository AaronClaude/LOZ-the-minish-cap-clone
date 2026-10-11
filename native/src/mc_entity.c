#include "mc_entity.h"
#include <string.h>

void entity_manager_init(EntityManager* em) {
    memset(em, 0, sizeof(EntityManager));
}

Entity* entity_create(EntityManager* em, u8 type, u8 id, s16 x, s16 y) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (!em->pool[i].active) {
            Entity* ent = &em->pool[i];
            memset(ent, 0, sizeof(Entity));
            ent->active = true;
            ent->type = type;
            ent->id = id;
            ent->x = x;
            ent->y = y;
            ent->health = 4;
            ent->maxHealth = 4;
            ent->hitbox.width = 16;
            ent->hitbox.height = 16;

            // Link into active list
            ent->next = em->active_head;
            if (em->active_head) {
                em->active_head->prev = ent;
            }
            em->active_head = ent;
            em->active_count++;
            return ent;
        }
    }
    return NULL;
}

void entity_destroy(EntityManager* em, Entity* ent) {
    if (!ent || !ent->active) return;

    if (ent->prev) ent->prev->next = ent->next;
    if (ent->next) ent->next->prev = ent->prev;
    if (em->active_head == ent) em->active_head = ent->next;

    ent->active = false;
    em->active_count--;
}

void entity_update_all(EntityManager* em) {
    Entity* cur = em->active_head;
    while (cur) {
        Entity* next = cur->next;
        if (cur->update) {
            cur->update(cur);
        }
        cur = next;
    }
}

