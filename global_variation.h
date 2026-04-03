#ifndef __GLOBAL_VARIATION_H
#define __GLOBAL_VARIATION_H

#include "gd32h7xx.h"
#include "lvgl.h"
#include <stdbool.h>

#define MAX_TARGETS      2
#define TRIGGER_RADIUS   50.0f

typedef enum {
    MODE_SURVEY,        // 巡检
    MODE_REFILLING,     // 换药
    MODE_SPRAY_TASK     // 喷洒
} WorkMode_t;

typedef enum {
    STAGE_INIT,         // 初始
    STAGE_IDENTIFIED,   // 已识别
    STAGE_READY,        // 已换药
    STAGE_SPRAYING,     // 喷洒中
    STAGE_DONE          // 完成
} TargetStage_t;

typedef struct {
    uint8_t id;
    float x, y;
    float radius;
    const char* file_path; // SD卡文件路径
    TargetStage_t stage;
} MissionTarget_t;

typedef struct {
    float cur_x;
    float cur_y;
    WorkMode_t work_mode;
    uint8_t active_target;
    bool is_pesticide_full;  
    lv_obj_t* ui_overlay;
} DroneSystem_t;

extern MissionTarget_t g_targets[MAX_TARGETS];
extern DroneSystem_t g_drone_sys;

#endif