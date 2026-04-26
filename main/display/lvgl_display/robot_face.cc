#include "robot_face.h"
#include "robot_face_images.h"
#include <esp_log.h>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>

#define TAG "RobotFace"

RobotFace::RobotFace(lv_obj_t* parent, int size, Mode mode)
    : size_(size)
    , eye_width_(size / 6)
    , eye_height_(size / 4)
    , pupil_size_(size / 12)
    , bg_width_(size)
    , bg_height_(size)
    , animation_enabled_(true)
    , is_blinking_(false)
    , is_speaking_(false)
    , eyes_closed_(false)
    , blink_min_ms_(2000)
    , blink_max_ms_(5000)
    , face_color_(lv_color_hex(0xFFFFFF))
    , eye_color_(lv_color_hex(0x000000))
    , pupil_color_(lv_color_hex(0x000000))
    , mouth_color_(lv_color_hex(0x000000))
    , blush_color_(lv_color_hex(0xFFB6C1))
    , mode_(mode) {

    // Create background image first to cover entire display
    const lv_image_dsc_t* bg = RobotFaceAssets::GetBackground();
    if (bg) {
        bg_img_ = lv_img_create(parent);
        lv_image_set_src(bg_img_, bg);
        lv_obj_set_width(bg_img_, bg->header.w);
        lv_obj_set_height(bg_img_, bg->header.h);
        lv_obj_align(bg_img_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_move_background(bg_img_);
        bg_width_ = bg->header.w;
        bg_height_ = bg->header.h;
    } else {
        bg_width_ = size_;
        bg_height_ = size_;
    }

    container_ = lv_obj_create(parent);
    if (bg) {
        lv_obj_set_size(container_, bg->header.w, bg->header.h);
    } else {
        lv_obj_set_size(container_, size_, size_);
    }
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(container_, LV_ALIGN_TOP_LEFT, 0, 0);

    if (mode_ == Mode::IMAGE) {
        CreateImageMode();
    } else {
        CreateDrawMode();
    }

    blink_timer_ = lv_timer_create(BlinkTimerCallback, blink_min_ms_, this);
    lv_timer_pause(blink_timer_);

    speak_timer_ = lv_timer_create(SpeakTimerCallback, 150, this);
    lv_timer_pause(speak_timer_);

    idle_timer_ = lv_timer_create(IdleTimerCallback, 3000, this);
    lv_timer_pause(idle_timer_);

    breath_timer_ = lv_timer_create(BreathTimerCallback, 50, this);
    lv_timer_pause(breath_timer_);

    ESP_LOGI(TAG, "RobotFace created with size %d, mode: %s", size_, mode_ == Mode::IMAGE ? "IMAGE" : "DRAW");
}

RobotFace::~RobotFace() {
    if (blink_timer_) lv_timer_delete(blink_timer_);
    if (speak_timer_) lv_timer_delete(speak_timer_);
    if (idle_timer_) lv_timer_delete(idle_timer_);
    if (breath_timer_) lv_timer_delete(breath_timer_);
}

void RobotFace::CreateDrawMode() {
    CreateEyes();
    CreateEyebrows();
    CreateMouth();
    CreateBlush();
}

void RobotFace::CreateImageMode() {
    CreateImageObjects();
}

void RobotFace::CreateImageObjects() {

    left_eye_img_ = lv_img_create(container_);
    lv_obj_add_flag(left_eye_img_, LV_OBJ_FLAG_HIDDEN);

    right_eye_img_ = lv_img_create(container_);
    lv_obj_add_flag(right_eye_img_, LV_OBJ_FLAG_HIDDEN);

    mouth_img_ = lv_img_create(container_);
    lv_obj_add_flag(mouth_img_, LV_OBJ_FLAG_HIDDEN);

    blush_left_img_ = lv_img_create(container_);
    lv_obj_add_flag(blush_left_img_, LV_OBJ_FLAG_HIDDEN);

    blush_right_img_ = lv_img_create(container_);
    lv_obj_add_flag(blush_right_img_, LV_OBJ_FLAG_HIDDEN);
}

void RobotFace::CreateEyes() {
    int eye_spacing = size_ / 4;
    int eye_y = size_ / 3 - eye_height_ / 2;

    left_eye_ = lv_obj_create(container_);
    lv_obj_set_size(left_eye_, eye_width_, eye_height_);
    lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(left_eye_, eye_color_, 0);
    lv_obj_set_style_border_width(left_eye_, 2, 0);
    lv_obj_set_style_radius(left_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_pad_all(left_eye_, 0, 0);
    lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(left_eye_, LV_ALIGN_CENTER, -eye_spacing, eye_y - size_ / 2);

    left_pupil_ = lv_obj_create(left_eye_);
    lv_obj_set_size(left_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_style_bg_color(left_pupil_, pupil_color_, 0);
    lv_obj_set_style_border_width(left_pupil_, 0, 0);
    lv_obj_set_style_radius(left_pupil_, pupil_size_ / 2, 0);
    lv_obj_set_style_pad_all(left_pupil_, 0, 0);
    lv_obj_center(left_pupil_);

    right_eye_ = lv_obj_create(container_);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_);
    lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(right_eye_, eye_color_, 0);
    lv_obj_set_style_border_width(right_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_pad_all(right_eye_, 0, 0);
    lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(right_eye_, LV_ALIGN_CENTER, eye_spacing, eye_y - size_ / 2);

    right_pupil_ = lv_obj_create(right_eye_);
    lv_obj_set_size(right_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_style_bg_color(right_pupil_, pupil_color_, 0);
    lv_obj_set_style_border_width(right_pupil_, 0, 0);
    lv_obj_set_style_radius(right_pupil_, pupil_size_ / 2, 0);
    lv_obj_set_style_pad_all(right_pupil_, 0, 0);
    lv_obj_center(right_pupil_);
}

void RobotFace::CreateEyebrows() {
    int eyebrow_y = size_ / 3 - eye_height_ - size_ / 16;
    int eyebrow_width = eye_width_ + 10;
    int eyebrow_height = 4;

    left_eyebrow_ = lv_obj_create(container_);
    lv_obj_set_size(left_eyebrow_, eyebrow_width, eyebrow_height);
    lv_obj_set_style_bg_color(left_eyebrow_, eye_color_, 0);
    lv_obj_set_style_border_width(left_eyebrow_, 0, 0);
    lv_obj_set_style_radius(left_eyebrow_, 2, 0);
    lv_obj_set_style_pad_all(left_eyebrow_, 0, 0);
    lv_obj_align(left_eyebrow_, LV_ALIGN_CENTER, -size_ / 4, eyebrow_y - size_ / 2);

    right_eyebrow_ = lv_obj_create(container_);
    lv_obj_set_size(right_eyebrow_, eyebrow_width, eyebrow_height);
    lv_obj_set_style_bg_color(right_eyebrow_, eye_color_, 0);
    lv_obj_set_style_border_width(right_eyebrow_, 0, 0);
    lv_obj_set_style_radius(right_eyebrow_, 2, 0);
    lv_obj_set_style_pad_all(right_eyebrow_, 0, 0);
    lv_obj_align(right_eyebrow_, LV_ALIGN_CENTER, size_ / 4, eyebrow_y - size_ / 2);
}

void RobotFace::CreateMouth() {
    int mouth_y = size_ * 2 / 3;

    mouth_ = lv_obj_create(container_);
    lv_obj_set_size(mouth_, size_ / 3, size_ / 8);
    lv_obj_set_style_bg_color(mouth_, mouth_color_, 0);
    lv_obj_set_style_border_width(mouth_, 0, 0);
    lv_obj_set_style_radius(mouth_, size_ / 16, 0);
    lv_obj_set_style_pad_all(mouth_, 0, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, 0, mouth_y - size_ / 2);
}

void RobotFace::CreateBlush() {
    int blush_y = size_ / 2 + size_ / 16;

    blush_left_ = lv_obj_create(container_);
    lv_obj_set_size(blush_left_, size_ / 8, size_ / 12);
    lv_obj_set_style_bg_color(blush_left_, blush_color_, 0);
    lv_obj_set_style_border_width(blush_left_, 0, 0);
    lv_obj_set_style_radius(blush_left_, size_ / 16, 0);
    lv_obj_set_style_pad_all(blush_left_, 0, 0);
    lv_obj_set_style_opa(blush_left_, LV_OPA_TRANSP, 0);
    lv_obj_align(blush_left_, LV_ALIGN_CENTER, -size_ / 3, blush_y - size_ / 2);

    blush_right_ = lv_obj_create(container_);
    lv_obj_set_size(blush_right_, size_ / 8, size_ / 12);
    lv_obj_set_style_bg_color(blush_right_, blush_color_, 0);
    lv_obj_set_style_border_width(blush_right_, 0, 0);
    lv_obj_set_style_radius(blush_right_, size_ / 16, 0);
    lv_obj_set_style_pad_all(blush_right_, 0, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_TRANSP, 0);
    lv_obj_align(blush_right_, LV_ALIGN_CENTER, size_ / 3, blush_y - size_ / 2);
}

void RobotFace::SetEmotion(const char* emotion) {
    if (emotion == nullptr || *emotion == '\0') {
        // 隐藏表情
        if (container_) {
            lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    current_emotion_ = emotion;
    // 确保表情显示
    if (container_) {
        lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
    }

    if (mode_ == Mode::IMAGE) {
        SetImageEmotion(emotion);
    } else {
        ResetFace();

        if (strcmp(emotion, EMOTION_HAPPY) == 0 || strcmp(emotion, "laughing") == 0) {
            DrawHappy();
        } else if (strcmp(emotion, EMOTION_SAD) == 0 || strcmp(emotion, "crying") == 0) {
            DrawSad();
        } else if (strcmp(emotion, EMOTION_ANGRY) == 0) {
            DrawAngry();
        } else if (strcmp(emotion, EMOTION_SURPRISED) == 0 || strcmp(emotion, "shocked") == 0) {
            DrawSurprised();
        } else if (strcmp(emotion, EMOTION_THINKING) == 0 || strcmp(emotion, "confused") == 0) {
            DrawThinking();
        } else if (strcmp(emotion, EMOTION_SLEEPY) == 0 || strcmp(emotion, "relaxed") == 0) {
            DrawSleepy();
        } else if (strcmp(emotion, EMOTION_LOVING) == 0) {
            DrawLoving();
        } else if (strcmp(emotion, EMOTION_COOL) == 0 || strcmp(emotion, "confident") == 0) {
            DrawCool();
        } else if (strcmp(emotion, EMOTION_WINKING) == 0) {
            DrawWinking();
        } else if (strcmp(emotion, EMOTION_LAUGHING) == 0 || strcmp(emotion, "funny") == 0) {
            DrawLaughing();
        } else {
            DrawNeutral();
        }
    }

    if (animation_enabled_) {
        lv_timer_resume(blink_timer_);
        lv_timer_resume(breath_timer_);
        lv_timer_resume(idle_timer_);
        uint32_t next_blink = blink_min_ms_ + (rand() % (blink_max_ms_ - blink_min_ms_));
        lv_timer_set_period(blink_timer_, next_blink);
    }

    ESP_LOGD(TAG, "Set emotion: %s", emotion);
}
//修改动态表情的在屏幕中图片位置
void RobotFace::SetImageEmotion(const char* emotion) {
    RobotFaceImages images;
    RobotFaceAssets::GetFaceImages(emotion, images);
    //眼睛位置
    if (images.eye_left) {
        lv_image_set_src(left_eye_img_, images.eye_left);
        lv_obj_remove_flag(left_eye_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(left_eye_img_, LV_ALIGN_CENTER, -bg_width_ / 4, -bg_height_ / 12);
        lv_obj_move_foreground(left_eye_img_);
    } else {
        lv_obj_add_flag(left_eye_img_, LV_OBJ_FLAG_HIDDEN);
    }

    if (images.eye_right) {
        lv_image_set_src(right_eye_img_, images.eye_right);
        lv_obj_remove_flag(right_eye_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(right_eye_img_, LV_ALIGN_CENTER, bg_width_ / 4, -bg_height_ / 12);
        lv_obj_move_foreground(right_eye_img_);
    } else {
        lv_obj_add_flag(right_eye_img_, LV_OBJ_FLAG_HIDDEN);
    }
    //嘴巴位置
    if (images.mouth) {
        lv_image_set_src(mouth_img_, images.mouth);
        lv_obj_remove_flag(mouth_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(mouth_img_, LV_ALIGN_CENTER, 0, bg_height_ / 5);
        lv_obj_move_foreground(mouth_img_);
    } else {
        lv_obj_add_flag(mouth_img_, LV_OBJ_FLAG_HIDDEN);
    }
    // 腮红 位置
    if (images.blush_left) {
        lv_image_set_src(blush_left_img_, images.blush_left);
        lv_obj_remove_flag(blush_left_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(blush_left_img_, LV_ALIGN_CENTER, -bg_width_ / 2.8, bg_height_ / 6);
    } else {
        lv_obj_add_flag(blush_left_img_, LV_OBJ_FLAG_HIDDEN);
    }

    if (images.blush_right) {
        lv_image_set_src(blush_right_img_, images.blush_right);
        lv_obj_remove_flag(blush_right_img_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(blush_right_img_, LV_ALIGN_CENTER, bg_width_ / 2.8, bg_height_ / 6);
    } else {
        lv_obj_add_flag(blush_right_img_, LV_OBJ_FLAG_HIDDEN);
    }
}

void RobotFace::ResetFace() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_);
    lv_obj_set_style_radius(left_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_radius(right_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);

    lv_obj_set_size(left_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_size(right_pupil_, pupil_size_, pupil_size_);
    lv_obj_set_style_radius(left_pupil_, pupil_size_ / 2, 0);
    lv_obj_set_style_radius(right_pupil_, pupil_size_ / 2, 0);
    lv_obj_center(left_pupil_);
    lv_obj_center(right_pupil_);

    lv_obj_set_style_opa(blush_left_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_TRANSP, 0);

    lv_obj_set_style_transform_rotation(left_eyebrow_, 0, 0);
    lv_obj_set_style_transform_rotation(right_eyebrow_, 0, 0);
    lv_obj_set_y(left_eyebrow_, size_ / 3 - eye_height_ - size_ / 16 - size_ / 2);
    lv_obj_set_y(right_eyebrow_, size_ / 3 - eye_height_ - size_ / 16 - size_ / 2);

    lv_obj_set_size(mouth_, size_ / 3, size_ / 8);
    lv_obj_set_style_radius(mouth_, size_ / 16, 0);
    lv_obj_set_style_bg_color(mouth_, mouth_color_, 0);
}

void RobotFace::DrawNeutral() {
    lv_obj_set_size(mouth_, size_ / 4, size_ / 20);
    lv_obj_set_style_radius(mouth_, size_ / 40, 0);
}

void RobotFace::DrawHappy() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_ * 0.7);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_ * 0.7);
    lv_obj_set_style_radius(left_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_radius(right_eye_, eye_width_ / 2, 0);

    lv_obj_set_style_opa(blush_left_, LV_OPA_50, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_50, 0);

    lv_obj_set_size(mouth_, size_ / 3, size_ / 6);
    lv_obj_set_style_radius(mouth_, size_ / 6, 0);

    lv_obj_set_y(left_eyebrow_, lv_obj_get_y(left_eyebrow_) - 5);
    lv_obj_set_y(right_eyebrow_, lv_obj_get_y(right_eyebrow_) - 5);
}

void RobotFace::DrawSad() {
    lv_obj_set_y(left_pupil_, 3);
    lv_obj_set_y(right_pupil_, 3);

    lv_obj_set_size(mouth_, size_ / 4, size_ / 10);
    lv_obj_set_style_radius(mouth_, size_ / 10, 0);
    lv_obj_set_style_bg_color(mouth_, lv_color_hex(0x4444FF), 0);

    lv_obj_set_style_transform_rotation(left_eyebrow_, 200, 0);
    lv_obj_set_style_transform_rotation(right_eyebrow_, 200, 0);
    lv_obj_set_style_transform_pivot_x(left_eyebrow_, 0, 0);
    lv_obj_set_style_transform_pivot_y(left_eyebrow_, lv_obj_get_height(left_eyebrow_) / 2, 0);
    lv_obj_set_style_transform_pivot_x(right_eyebrow_, lv_obj_get_width(right_eyebrow_), 0);
    lv_obj_set_style_transform_pivot_y(right_eyebrow_, lv_obj_get_height(right_eyebrow_) / 2, 0);
}

void RobotFace::DrawAngry() {
    lv_obj_set_size(left_eye_, eye_width_ * 1.1, eye_height_ * 0.8);
    lv_obj_set_size(right_eye_, eye_width_ * 1.1, eye_height_ * 0.8);

    lv_obj_set_size(left_pupil_, pupil_size_ * 1.2, pupil_size_ * 1.2);
    lv_obj_set_size(right_pupil_, pupil_size_ * 1.2, pupil_size_ * 1.2);

    lv_obj_set_size(mouth_, size_ / 4, size_ / 12);
    lv_obj_set_style_radius(mouth_, 2, 0);

    lv_obj_set_style_transform_rotation(left_eyebrow_, 300, 0);
    lv_obj_set_style_transform_rotation(right_eyebrow_, 300, 0);
    lv_obj_set_style_transform_pivot_x(left_eyebrow_, lv_obj_get_width(left_eyebrow_), 0);
    lv_obj_set_style_transform_pivot_y(left_eyebrow_, lv_obj_get_height(left_eyebrow_) / 2, 0);
    lv_obj_set_style_transform_pivot_x(right_eyebrow_, 0, 0);
    lv_obj_set_style_transform_pivot_y(right_eyebrow_, lv_obj_get_height(right_eyebrow_) / 2, 0);
}

void RobotFace::DrawSurprised() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_ * 1.3);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_ * 1.3);
    lv_obj_set_style_radius(left_eye_, eye_width_ / 2, 0);
    lv_obj_set_style_radius(right_eye_, eye_width_ / 2, 0);

    lv_obj_set_size(left_pupil_, pupil_size_ * 0.8, pupil_size_ * 0.8);
    lv_obj_set_size(right_pupil_, pupil_size_ * 0.8, pupil_size_ * 0.8);

    lv_obj_set_size(mouth_, size_ / 5, size_ / 5);
    lv_obj_set_style_radius(mouth_, size_ / 10, 0);

    lv_obj_set_y(left_eyebrow_, lv_obj_get_y(left_eyebrow_) - 10);
    lv_obj_set_y(right_eyebrow_, lv_obj_get_y(right_eyebrow_) - 10);
}

void RobotFace::DrawThinking() {
    lv_obj_set_x(left_pupil_, 3);
    lv_obj_set_x(right_pupil_, 3);
    lv_obj_set_y(left_pupil_, -2);
    lv_obj_set_y(right_pupil_, -2);

    lv_obj_set_size(mouth_, size_ / 5, size_ / 15);
    lv_obj_set_style_radius(mouth_, 2, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, size_ / 8, size_ * 2 / 3 - size_ / 2);

    lv_obj_set_style_transform_rotation(left_eyebrow_, 100, 0);
    lv_obj_set_style_transform_rotation(right_eyebrow_, 100, 0);
}

void RobotFace::DrawSleepy() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_ * 0.3);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_ * 0.3);
    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);

    lv_obj_set_style_opa(left_pupil_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(right_pupil_, LV_OPA_TRANSP, 0);

    lv_obj_set_size(mouth_, size_ / 6, size_ / 10);
    lv_obj_set_style_radius(mouth_, size_ / 20, 0);

    lv_obj_set_y(left_eyebrow_, lv_obj_get_y(left_eyebrow_) + 5);
    lv_obj_set_y(right_eyebrow_, lv_obj_get_y(right_eyebrow_) + 5);
}

void RobotFace::DrawLoving() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_ * 0.7);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_ * 0.7);

    lv_obj_set_style_opa(blush_left_, LV_OPA_70, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_70, 0);

    lv_obj_set_size(mouth_, size_ / 4, size_ / 8);
    lv_obj_set_style_radius(mouth_, size_ / 8, 0);

    lv_obj_set_y(left_eyebrow_, lv_obj_get_y(left_eyebrow_) - 5);
    lv_obj_set_y(right_eyebrow_, lv_obj_get_y(right_eyebrow_) - 5);
}

void RobotFace::DrawConfused() {
    lv_obj_set_x(left_pupil_, -2);
    lv_obj_set_x(right_pupil_, 2);
    lv_obj_set_y(left_pupil_, 2);
    lv_obj_set_y(right_pupil_, -2);

    lv_obj_set_size(mouth_, size_ / 4, size_ / 12);
    lv_obj_set_style_radius(mouth_, 2, 0);
    lv_obj_align(mouth_, LV_ALIGN_CENTER, size_ / 10, size_ * 2 / 3 - size_ / 2);

    lv_obj_set_style_transform_rotation(left_eyebrow_, 150, 0);
    lv_obj_set_style_transform_rotation(right_eyebrow_, 250, 0);
}

void RobotFace::DrawCool() {
    lv_obj_set_size(left_eye_, eye_width_ * 1.2, eye_height_ * 0.4);
    lv_obj_set_size(right_eye_, eye_width_ * 1.2, eye_height_ * 0.4);
    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);
    lv_obj_set_style_bg_color(left_eye_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);

    lv_obj_set_style_opa(left_pupil_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(right_pupil_, LV_OPA_TRANSP, 0);

    lv_obj_set_size(mouth_, size_ / 5, size_ / 15);
    lv_obj_set_style_radius(mouth_, 2, 0);
}

void RobotFace::DrawWinking() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_);
    lv_obj_set_size(right_eye_, eye_width_ * 1.2, eye_height_ * 0.2);
    lv_obj_set_style_radius(right_eye_, 2, 0);

    lv_obj_set_style_opa(blush_left_, LV_OPA_40, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_40, 0);

    lv_obj_set_size(mouth_, size_ / 4, size_ / 10);
    lv_obj_set_style_radius(mouth_, size_ / 10, 0);
}

void RobotFace::DrawLaughing() {
    lv_obj_set_size(left_eye_, eye_width_, eye_height_ * 0.4);
    lv_obj_set_size(right_eye_, eye_width_, eye_height_ * 0.4);
    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);

    lv_obj_set_style_opa(left_pupil_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(right_pupil_, LV_OPA_TRANSP, 0);

    lv_obj_set_style_opa(blush_left_, LV_OPA_60, 0);
    lv_obj_set_style_opa(blush_right_, LV_OPA_60, 0);

    lv_obj_set_size(mouth_, size_ / 3, size_ / 5);
    lv_obj_set_style_radius(mouth_, size_ / 5, 0);
}

void RobotFace::AnimateBlink() {
    if (mode_ == Mode::IMAGE) {
        if (eyes_closed_) {
            lv_image_dsc_t* left_img = const_cast<lv_image_dsc_t*>(RobotFaceAssets::GetEyeLeft(current_emotion_.c_str()));
            lv_image_dsc_t* right_img = const_cast<lv_image_dsc_t*>(RobotFaceAssets::GetEyeRight(current_emotion_.c_str()));
            if (left_img) lv_image_set_src(left_eye_img_, left_img);
            if (right_img) lv_image_set_src(right_eye_img_, right_img);
            eyes_closed_ = false;
        } else {
            lv_image_dsc_t* left_close = const_cast<lv_image_dsc_t*>(RobotFaceAssets::GetEyeLeft("sleepy"));
            lv_image_dsc_t* right_close = const_cast<lv_image_dsc_t*>(RobotFaceAssets::GetEyeRight("sleepy"));
            if (left_close) lv_image_set_src(left_eye_img_, left_close);
            if (right_close) lv_image_set_src(right_eye_img_, right_close);
            eyes_closed_ = true;
        }
        uint32_t next_blink = blink_min_ms_ + (rand() % (blink_max_ms_ - blink_min_ms_));
        lv_timer_set_period(blink_timer_, eyes_closed_ ? 100 : next_blink);
        return;
    }

    if (eyes_closed_) {
        lv_obj_set_size(left_eye_, eye_width_, eye_height_);
        lv_obj_set_size(right_eye_, eye_width_, eye_height_);
        eyes_closed_ = false;
    } else {
        lv_obj_set_size(left_eye_, eye_width_, 4);
        lv_obj_set_size(right_eye_, eye_width_, 4);
        eyes_closed_ = true;
    }

    uint32_t next_blink = blink_min_ms_ + (rand() % (blink_max_ms_ - blink_min_ms_));
    lv_timer_set_period(blink_timer_, eyes_closed_ ? 100 : next_blink);
}

void RobotFace::AnimateSpeak() {
    if (mode_ == Mode::IMAGE) {
        static const char* mouth_states[] = {"neutral", "happy", "surprised", "laughing"};
        static int mouth_state = 0;
        
        mouth_state = (mouth_state + 1) % 4;
        lv_image_dsc_t* mouth_img = const_cast<lv_image_dsc_t*>(RobotFaceAssets::GetMouth(mouth_states[mouth_state]));
        if (mouth_img) {
            lv_image_set_src(mouth_img_, mouth_img);
        }
        return;
    }

    static int mouth_state = 0;
    int heights[] = {size_ / 8, size_ / 5, size_ / 10, size_ / 6};
    
    mouth_state = (mouth_state + 1) % 4;
    lv_obj_set_height(mouth_, heights[mouth_state]);
}

void RobotFace::AnimateIdle() {
    if (mode_ == Mode::IMAGE) {
        return;
    }

    static int direction = 1;
    int current_x = lv_obj_get_x(left_pupil_) - (eye_width_ / 2 - pupil_size_ / 2);

    current_x += direction * 2;
    if (current_x > 4 || current_x < -4) {
        direction *= -1;
    }

    lv_obj_set_x(left_pupil_, (eye_width_ / 2 - pupil_size_ / 2) + current_x);
    lv_obj_set_x(right_pupil_, (eye_width_ / 2 - pupil_size_ / 2) + current_x);
}

void RobotFace::BlinkTimerCallback(lv_timer_t* timer) {
    RobotFace* face = static_cast<RobotFace*>(lv_timer_get_user_data(timer));
    if (face && face->animation_enabled_) {
        face->AnimateBlink();
    }
}

void RobotFace::SpeakTimerCallback(lv_timer_t* timer) {
    RobotFace* face = static_cast<RobotFace*>(lv_timer_get_user_data(timer));
    if (face && face->is_speaking_ && face->animation_enabled_) {
        face->AnimateSpeak();
    }
}

void RobotFace::IdleTimerCallback(lv_timer_t* timer) {
    RobotFace* face = static_cast<RobotFace*>(lv_timer_get_user_data(timer));
    if (face && face->animation_enabled_ && !face->is_speaking_) {
        face->AnimateIdle();
    }
}

void RobotFace::AnimateBreath() {
    static int32_t breath_zoom = 256;
    static int32_t breath_direction = 1;
    
    breath_zoom += breath_direction;
    if (breath_zoom > 262 || breath_zoom < 250) {
        breath_direction = -breath_direction;
    }
    
    if (mode_ == Mode::IMAGE) {
        lv_obj_set_style_transform_zoom(left_eye_img_, breath_zoom, 0);
        lv_obj_set_style_transform_zoom(right_eye_img_, breath_zoom, 0);
        lv_obj_set_style_transform_zoom(mouth_img_, breath_zoom, 0);
    } else {
        lv_obj_set_style_transform_zoom(container_, breath_zoom, 0);
    }
}

void RobotFace::BreathTimerCallback(lv_timer_t* timer) {
    RobotFace* face = static_cast<RobotFace*>(lv_timer_get_user_data(timer));
    if (face && face->animation_enabled_) {
        face->AnimateBreath();
    }
}

void RobotFace::SetAnimationEnabled(bool enabled) {
    animation_enabled_ = enabled;
    if (enabled) {
        lv_timer_resume(blink_timer_);
        lv_timer_resume(breath_timer_);
        lv_timer_resume(idle_timer_);
    } else {
        lv_timer_pause(blink_timer_);
        lv_timer_pause(speak_timer_);
        lv_timer_pause(idle_timer_);
        lv_timer_pause(breath_timer_);
    }
}

void RobotFace::SetBlinkInterval(uint32_t min_ms, uint32_t max_ms) {
    blink_min_ms_ = min_ms;
    blink_max_ms_ = max_ms;
}

void RobotFace::SetSpeaking(bool speaking) {
    is_speaking_ = speaking;
    if (speaking && animation_enabled_) {
        lv_timer_resume(speak_timer_);
    } else {
        lv_timer_pause(speak_timer_);
    }
}

void RobotFace::SetColors(lv_color_t face_color, lv_color_t eye_color, lv_color_t mouth_color) {
    face_color_ = face_color;
    eye_color_ = eye_color;
    mouth_color_ = mouth_color;

    lv_obj_set_style_bg_color(container_, face_color_, 0);
    lv_obj_set_style_border_color(left_eye_, eye_color_, 0);
    lv_obj_set_style_border_color(right_eye_, eye_color_, 0);
    lv_obj_set_style_bg_color(left_eyebrow_, eye_color_, 0);
    lv_obj_set_style_bg_color(right_eyebrow_, eye_color_, 0);
    lv_obj_set_style_bg_color(mouth_, mouth_color_, 0);
}

void RobotFace::SetMode(Mode mode) {
    if (mode_ == mode) return;
    
    mode_ = mode;
    
    if (left_eye_) lv_obj_delete(left_eye_);
    if (right_eye_) lv_obj_delete(right_eye_);
    if (left_pupil_) lv_obj_delete(left_pupil_);
    if (right_pupil_) lv_obj_delete(right_pupil_);
    if (left_eyebrow_) lv_obj_delete(left_eyebrow_);
    if (right_eyebrow_) lv_obj_delete(right_eyebrow_);
    if (mouth_) lv_obj_delete(mouth_);
    if (blush_left_) lv_obj_delete(blush_left_);
    if (blush_right_) lv_obj_delete(blush_right_);
    if (bg_img_) lv_obj_delete(bg_img_);
    if (left_eye_img_) lv_obj_delete(left_eye_img_);
    if (right_eye_img_) lv_obj_delete(right_eye_img_);
    if (mouth_img_) lv_obj_delete(mouth_img_);
    if (blush_left_img_) lv_obj_delete(blush_left_img_);
    if (blush_right_img_) lv_obj_delete(blush_right_img_);
    
    left_eye_ = right_eye_ = left_pupil_ = right_pupil_ = nullptr;
    left_eyebrow_ = right_eyebrow_ = mouth_ = nullptr;
    blush_left_ = blush_right_ = nullptr;
    bg_img_ = left_eye_img_ = right_eye_img_ = mouth_img_ = nullptr;
    blush_left_img_ = blush_right_img_ = nullptr;
    
    // Create background image for both modes
    const lv_image_dsc_t* bg = RobotFaceAssets::GetBackground();
    if (bg) {
        lv_obj_t* parent = container_ ? lv_obj_get_parent(container_) : lv_screen_active();
        bg_img_ = lv_img_create(parent);
        lv_image_set_src(bg_img_, bg);
        lv_obj_set_width(bg_img_, bg->header.w);
        lv_obj_set_height(bg_img_, bg->header.h);
        lv_obj_align(bg_img_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_move_background(bg_img_);
    }

    if (mode_ == Mode::IMAGE) {
        CreateImageMode();
    } else {
        CreateDrawMode();
    }
    
    if (!current_emotion_.empty()) {
        SetEmotion(current_emotion_.c_str());
    }
}
