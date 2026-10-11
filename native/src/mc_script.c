#include "mc_script.h"
#include <stdio.h>
#include <string.h>

void script_init(ScriptContext* ctx) {
    memset(ctx, 0, sizeof(ScriptContext));
}

void script_start(ScriptContext* ctx, const u8* script_bytecode) {
    ctx->bytecode = script_bytecode;
    ctx->pc = 0;
    ctx->wait_timer = 0;
    ctx->is_executing = (script_bytecode != NULL);
}

void script_update(ScriptContext* ctx, EntityManager* em) {
    (void)em;
    if (!ctx->is_executing || !ctx->bytecode) return;

    if (ctx->wait_timer > 0) {
        ctx->wait_timer--;
        return;
    }

    while (ctx->is_executing) {
        u8 opcode = ctx->bytecode[ctx->pc++];

        switch (opcode) {
            case OP_SCRIPT_END:
                ctx->is_executing = false;
                printf("[Script VM] Cutscene script execution finished.\n");
                return;

            case OP_WAIT_FRAMES: {
                u16 frames = (u16)ctx->bytecode[ctx->pc] | ((u16)ctx->bytecode[ctx->pc + 1] << 8);
                ctx->pc += 2;
                ctx->wait_timer = frames;
                return;
            }

            case OP_SHOW_DIALOGUE: {
                u16 msg_id = (u16)ctx->bytecode[ctx->pc] | ((u16)ctx->bytecode[ctx->pc + 1] << 8);
                ctx->pc += 2;
                ctx->current_message_id = msg_id;
                printf("[Script VM] Display dialogue text ID: 0x%04X\n", msg_id);
                break;
            }

            case OP_MOVE_ENTITY: {
                u8 ent_id = ctx->bytecode[ctx->pc++];
                s16 target_x = (s16)(ctx->bytecode[ctx->pc] | (ctx->bytecode[ctx->pc + 1] << 8));
                ctx->pc += 2;
                s16 target_y = (s16)(ctx->bytecode[ctx->pc] | (ctx->bytecode[ctx->pc + 1] << 8));
                ctx->pc += 2;
                printf("[Script VM] Entity 0x%02X moving to (%d, %d)\n", ent_id, target_x, target_y);
                break;
            }

            case OP_PLAY_SOUND: {
                u16 sfx_id = (u16)ctx->bytecode[ctx->pc] | ((u16)ctx->bytecode[ctx->pc + 1] << 8);
                ctx->pc += 2;
                printf("[Script VM] Play audio track/SFX: 0x%04X\n", sfx_id);
                break;
            }

            default:
                fprintf(stderr, "[Script VM] Unknown opcode: 0x%02X at PC 0x%04X\n", opcode, ctx->pc - 1);
                ctx->is_executing = false;
                return;
        }
    }
}

bool script_is_active(const ScriptContext* ctx) {
    return ctx->is_executing;
}
