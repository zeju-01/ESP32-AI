#pragma once

#include <lvgl.h>

LV_IMG_DECLARE(eye_left_normal);
LV_IMG_DECLARE(eye_right_normal);
LV_IMG_DECLARE(eye_left_big);
LV_IMG_DECLARE(eye_right_big);
LV_IMG_DECLARE(eye_left_happy);
LV_IMG_DECLARE(eye_right_happy);
LV_IMG_DECLARE(eye_left_think);
LV_IMG_DECLARE(eye_right_think);
LV_IMG_DECLARE(eye_left_shy);
LV_IMG_DECLARE(eye_right_shy);
LV_IMG_DECLARE(eye_left_sad);
LV_IMG_DECLARE(eye_right_sad);
LV_IMG_DECLARE(eye_left_close);
LV_IMG_DECLARE(eye_right_close);
LV_IMG_DECLARE(eye_left_big_big);
LV_IMG_DECLARE(eye_right_big_big);
LV_IMG_DECLARE(eye_left_wink);
LV_IMG_DECLARE(eye_right_wink);

LV_IMG_DECLARE(mouth_close);
LV_IMG_DECLARE(mouth_smile);
LV_IMG_DECLARE(mouth_small_smile);
LV_IMG_DECLARE(mouth_laugh);
LV_IMG_DECLARE(mouth_open1);
LV_IMG_DECLARE(mouth_open2);
LV_IMG_DECLARE(mouth_o);
LV_IMG_DECLARE(mouth_down);
LV_IMG_DECLARE(mouth_relax);
LV_IMG_DECLARE(mouth_surprise);

LV_IMG_DECLARE(blush);
LV_IMG_DECLARE(bg);

typedef struct {
    lv_image_dsc_t* eye_left;
    lv_image_dsc_t* eye_right;
    lv_image_dsc_t* mouth;
    lv_image_dsc_t* blush_left;
    lv_image_dsc_t* blush_right;
} RobotFaceImages;

namespace RobotFaceAssets {
    extern const lv_image_dsc_t* GetEyeLeft(const char* emotion);
    extern const lv_image_dsc_t* GetEyeRight(const char* emotion);
    extern const lv_image_dsc_t* GetMouth(const char* emotion);
    extern const lv_image_dsc_t* GetBlush();
    extern const lv_image_dsc_t* GetBackground();
    extern void GetFaceImages(const char* emotion, RobotFaceImages& images);
}
