#ifndef MC_SCRIPT_H
#define MC_SCRIPT_H

#include "gba_types.h"
#include "mc_entity.h"

// Capcom Event Script VM Opcodes
#define OP_SCRIPT_END         0x00
#define OP_WAIT_FRAMES        0x01
#define OP_SHOW_DIALOGUE      0x02
#define OP_MOVE_ENTITY        0x03
#define OP_PLAY_SOUND         0x04
#define OP_SET_QUEST_FLAG     0x05
#define OP_PAN_CAMERA         0x06
#define OP_FADE_SCREEN        0x07

typedef struct {
    const u8* bytecode;
    u32 pc;
    u16 wait_timer;
    bool is_executing;
    u16 current_message_id;
    char current_dialogue_text[256];
} ScriptContext;

void script_init(ScriptContext* ctx);
void script_start(ScriptContext* ctx, const u8* script_bytecode);
void script_update(ScriptContext* ctx, EntityManager* em);
bool script_is_active(const ScriptContext* ctx);

#endif // MC_SCRIPT_H

