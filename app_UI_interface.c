#include "global_variation.h"
#include "lvgl.h"
#include <stdlib.h>

/* 外部函数与资源声明 */
extern int read_file_to_array(const char* filename, uint8_t* buffer, uint32_t max_size); 
extern void* sdram_malloc(uint32_t size); 

// 静态资源句柄
static lv_img_dsc_t img_struct; 
static void* image_buffer = NULL;
static lv_obj_t * spray_img_obj = NULL;
static uint8_t frame_toggle = 0;
extern const lv_img_dsc_t img_spray_f1; 
extern const lv_img_dsc_t img_spray_f2;
static lv_obj_t * refill_bar = NULL;
static lv_obj_t * refill_label = NULL;

static void spray_anim_timer_cb(lv_timer_t * timer) {
    // 1. 逻辑切换：0 变 1，1 变 0
    frame_toggle = !frame_toggle;

    // 2. 根据状态切换显示的图片源
    // 外部已经通过 extern 声明了这两帧位图
    if(frame_toggle) {
        lv_img_set_src(spray_img_obj, &img_spray_f1);
    } else {
        lv_img_set_src(spray_img_obj, &img_spray_f2);
    }
}
static void refill_timer_cb(lv_timer_t * timer) {
    static int16_t val = 0;
    val += 2; // 每次增加 2%，约 2.5 秒填满

    if(val <= 100) {
        lv_bar_set_value(refill_bar, val, LV_ANIM_ON);
        lv_label_set_text_fmt(refill_label, "Refilling: %d%%", val);
    } else {
        // --- 关键状态迁移 ---
        val = 0;
        lv_timer_del(timer);             // 停止定时器
        lv_obj_del(refill_bar);          // 销毁进度条
        
        g_drone_sys.is_pesticide_full = true;
        g_drone_sys.work_mode = MODE_SPRAY_TASK; // 自动切换到喷洒模式
        
        // 提示用户可以开始第二次飞行
        lv_label_set_text(refill_label, "Refill Done! Ready to Spray.");
    }
}
void App_UI_Show_Refill_Process(void) {
    // 1. 创建进度条对象
    refill_bar = lv_bar_create(lv_scr_act());
    lv_obj_set_size(refill_bar, 400, 30);
    lv_obj_center(refill_bar);
    lv_bar_set_range(refill_bar, 0, 100);

    // 2. 创建百分比文字
    refill_label = lv_label_create(lv_scr_act());
    lv_obj_align_to(refill_label, refill_bar, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // 3. 启动补给模拟定时器
    lv_timer_create(refill_timer_cb, 50, NULL);
}
static void confirm_btn_event_cb(lv_event_t * e) {
    lv_obj_t * mbox = lv_event_get_current_target(e);
    
    // 1. 销毁对话框
    lv_msgbox_close(mbox);
    
    // 2. 核心状态迁移：识别完成 -> 准备好补给
    uint8_t id = g_drone_sys.active_target;
    g_targets[id].stage = STAGE_READY;
    
    // 3. 衔接之前的逻辑：开启补给进度条
    App_UI_Show_Refill_Process();
}
// 根据病害名称获取处方信息（逻辑推演：结构化数据检索）
static const char* Get_Pesticide_Info(uint8_t id) {
    if (id == 0) return "Rust Control / 500ml per mu";
    if (id == 1) return "Blast Shield / 300ml per mu";
    return "General Pesticide / 400ml";
}
/* --- 1. 交互回调函数 --- */

// 触屏拖拽回调：通过位移矢量实现实时跟随
static void img_drag_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t vect;
    
    lv_indev_get_vect(indev, &vect); // 获取触摸偏移量 
    lv_obj_set_pos(obj, lv_obj_get_x(obj) + vect.x, lv_obj_get_y(obj) + vect.y);
}

// 图像透明度动画回调
static void img_opa_anim_cb(void * var, int32_t v) {
    lv_obj_set_style_img_opa((lv_obj_t *)var, v, 0); 
}

/* --- 2. 业务功能函数 --- */

/**
 * @brief 第一次触发：从 SD 卡加载照片并渐变显示
 */
static void discovery_anim_ready_cb(lv_anim_t * a) {
    uint8_t id = g_drone_sys.active_target;
    
    // 创建消息弹窗 (Msgbox)
    static const char * btns[] = {"Confirm Refill", ""}; // 确认按钮
    
    lv_obj_t * mbox = lv_msgbox_create(NULL, "Pesticide Prescription", 
                                        Get_Pesticide_Info(id), btns, true);
    
    lv_obj_center(mbox);
    
    // 绑定确认按钮的事件
    lv_obj_add_event_cb(mbox, confirm_btn_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 标记当前作物名称（可选）
    lv_msgbox_get_title(mbox); // 可以在这里动态修改标题为作物名
	}
void App_UI_Show_Discovery(uint8_t id) {
    // 1. SDRAM 动态内存管理
    uint32_t size = 253 * 256 * 3 + 4;
    if(image_buffer == NULL) {
        image_buffer = sdram_malloc(size); // 仅在首次或复位后申请，避免内存碎片
    }
    
    // 2. 读取 SD 卡数据 
    if(read_file_to_array(g_targets[id].file_path, (uint8_t*)image_buffer, size) != 0) return;

    // 3. 封装 LVGL 图像描述符 
    img_struct.header.always_zero = 0;
    img_struct.header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
    img_struct.header.w = 253;
    img_struct.header.h = 256;
    img_struct.data_size = 253 * 256 * 3;
    img_struct.data = (uint8_t*)image_buffer + 4;

    // 4. 创建图像并执行渐变动画 
    lv_obj_t * img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &img_struct);
    lv_obj_center(img);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);//绑定目标。告诉动画引擎：这个动画是作用在你刚刚从 SDRAM 加载并创建的 img 对象上的。
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 2000); //设置持续时间。这里设定为 2000 毫秒（2秒）
    lv_anim_set_exec_cb(&a, img_opa_anim_cb);//关联执行函数。这是最关键的一步，它告诉引擎每隔十几毫秒就去调用你写的那个 img_opa_anim_cb 函数，并传入当前的计算值
		lv_anim_set_ready_cb(&a, discovery_anim_ready_cb);
    lv_anim_start(&a);
}

/**
 * @brief 第二次触发：创建带双帧动画的拖拽喷洒框
 */
void App_UI_Create_Spray_Box(uint8_t id) {
    // 1. 创建容器（box），开启点击和拖拽
    lv_obj_t * box = lv_obj_create(lv_scr_act());
    lv_obj_set_size(box, 200, 240);
    lv_obj_set_style_bg_opa(box, LV_OPA_70, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(box, img_drag_event_cb, LV_EVENT_PRESSING, NULL);

    // 2. 在 box 内部创建基础图片对象 (lv_img) 而不是 lv_animimg
    spray_img_obj = lv_img_create(box);
    lv_obj_center(spray_img_obj);
    lv_img_set_src(spray_img_obj, &img_spray_f1); // 默认显示第一帧

    // 3. 核心步骤：创建一个 LVGL 定时器，频率 150ms
    // 这个定时器会自动在后台运行，不断调用上面的 spray_anim_timer_cb
    lv_timer_create(spray_anim_timer_cb, 150, NULL);

    // 4. 添加文字
    lv_obj_t * label = lv_label_create(box);
    lv_label_set_text(label, "Spraying...");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
}