#pragma once

#include <lvgl.h>
#include <string>
#include <functional>

class RobotFace {
public:
    enum class Mode {
        DRAW,
        IMAGE
    };

    RobotFace(lv_obj_t* parent, int size = 200, Mode mode = Mode::IMAGE);
    ~RobotFace();

    lv_obj_t* GetCanvas() const { return container_; }

    void SetEmotion(const char* emotion);
    const char* GetCurrentEmotion() const { return current_emotion_.c_str(); }

    void SetAnimationEnabled(bool enabled);
    bool IsAnimationEnabled() const { return animation_enabled_; }

    void SetBlinkInterval(uint32_t min_ms, uint32_t max_ms);
    void SetSpeaking(bool speaking);
    
    void SetColors(lv_color_t face_color, lv_color_t eye_color, lv_color_t mouth_color);
    void SetMode(Mode mode);
    Mode GetMode() const { return mode_; }

    static constexpr const char* EMOTION_NEUTRAL = "neutral";
    static constexpr const char* EMOTION_HAPPY = "happy";
    static constexpr const char* EMOTION_SAD = "sad";
    static constexpr const char* EMOTION_ANGRY = "angry";
    static constexpr const char* EMOTION_SURPRISED = "surprised";
    static constexpr const char* EMOTION_THINKING = "thinking";
    static constexpr const char* EMOTION_SLEEPY = "sleepy";
    static constexpr const char* EMOTION_LOVING = "loving";
    static constexpr const char* EMOTION_CONFUSED = "confused";
    static constexpr const char* EMOTION_COOL = "cool";
    static constexpr const char* EMOTION_WINKING = "winking";
    static constexpr const char* EMOTION_LAUGHING = "laughing";

private:
    lv_obj_t* container_;

    lv_obj_t* left_eye_;
    lv_obj_t* right_eye_;
    lv_obj_t* left_pupil_;
    lv_obj_t* right_pupil_;
    lv_obj_t* left_eyebrow_;
    lv_obj_t* right_eyebrow_;
    lv_obj_t* mouth_;
    lv_obj_t* blush_left_;
    lv_obj_t* blush_right_;

    lv_obj_t* bg_img_;
    lv_obj_t* left_eye_img_;
    lv_obj_t* right_eye_img_;
    lv_obj_t* mouth_img_;
    lv_obj_t* blush_left_img_;
    lv_obj_t* blush_right_img_;

    lv_timer_t* blink_timer_;
    lv_timer_t* speak_timer_;
    lv_timer_t* idle_timer_;
    lv_timer_t* breath_timer_;

    std::string current_emotion_;
    int size_;
    int eye_width_;
    int eye_height_;
    int pupil_size_;
    int bg_width_;
    int bg_height_;
    bool animation_enabled_;
    bool is_blinking_;
    bool is_speaking_;
    bool eyes_closed_;

    uint32_t blink_min_ms_;
    uint32_t blink_max_ms_;

    lv_color_t face_color_;
    lv_color_t eye_color_;
    lv_color_t pupil_color_;
    lv_color_t mouth_color_;
    lv_color_t blush_color_;

    Mode mode_;

    void CreateDrawMode();
    void CreateImageMode();
    void CreateEyes();
    void CreateEyebrows();
    void CreateMouth();
    void CreateBlush();
    void CreateImageObjects();

    void DrawNeutral();
    void DrawHappy();
    void DrawSad();
    void DrawAngry();
    void DrawSurprised();
    void DrawThinking();
    void DrawSleepy();
    void DrawLoving();
    void DrawConfused();
    void DrawCool();
    void DrawWinking();
    void DrawLaughing();

    void SetImageEmotion(const char* emotion);

    void AnimateBlink();
    void AnimateSpeak();
    void AnimateIdle();
    void AnimateBreath();

    void ResetFace();
    void SetEyeShape(int width, int height, int radius = -1);
    void SetPupilPosition(int x_offset, int y_offset);
    void SetEyebrowAngle(lv_obj_t* eyebrow, int angle, int y_offset = 0);
    void SetMouthShape(int width, int height, int arc_start = 0, int arc_end = 0);

    static void BlinkTimerCallback(lv_timer_t* timer);
    static void SpeakTimerCallback(lv_timer_t* timer);
    static void IdleTimerCallback(lv_timer_t* timer);
    static void BreathTimerCallback(lv_timer_t* timer);
};
