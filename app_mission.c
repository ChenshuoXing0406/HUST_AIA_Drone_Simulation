#include "global_variation.h"
#include "math.h"
extern void App_UI_Show_Discovery(uint8_t id);
extern void App_UI_Create_Spray_Box(uint8_t id);
// 变量定义
MissionTarget_t g_targets[MAX_TARGETS] = {
    {0, 450.0f, 300.0f, TRIGGER_RADIUS, "0:/Rust.bin", STAGE_INIT},
    {1, 850.0f, 450.0f, TRIGGER_RADIUS, "0:/Blast.bin", STAGE_INIT}
};
DroneSystem_t g_drone_sys = {0.0f, 0.0f, MODE_SURVEY, 0};

// 函数原型
static void Handle_Collision(uint8_t id);

void App_Mission_Loop(void) {
    if (g_drone_sys.work_mode == MODE_REFILLING) return;

    for (uint8_t i = 0; i < MAX_TARGETS; i++) {
        if (g_targets[i].stage == STAGE_DONE) continue;

        float dx = g_drone_sys.cur_x - g_targets[i].x;
        float dy = g_drone_sys.cur_y - g_targets[i].y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist <= g_targets[i].radius) {
            Handle_Collision(i);
        }
    }
}

static void Handle_Collision(uint8_t id) {
    if (g_targets[id].stage == STAGE_INIT && g_drone_sys.work_mode == MODE_SURVEY) {
        g_targets[id].stage = STAGE_IDENTIFIED;
        g_drone_sys.active_target = id;
        App_UI_Show_Discovery(id); // 触发照片渐变
    } 
    else if (g_targets[id].stage == STAGE_READY && g_drone_sys.work_mode == MODE_SPRAY_TASK) {
        g_targets[id].stage = STAGE_SPRAYING;
        App_UI_Create_Spray_Box(id); // 触发双帧动画与拖动框
    }
}