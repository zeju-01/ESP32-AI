#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <font_awesome.h>

#include "lvgl_display.h"
#include "board.h"
#include "application.h"
#include "audio_codec.h"
#include "settings.h"
#include "assets/lang_config.h"
#include "jpg/image_to_jpeg.h"

#define TAG "Display"

LvglDisplay::LvglDisplay() {
    emotion_auto_update_ = true;
    // Notification timer
    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg) {
            LvglDisplay *display = static_cast<LvglDisplay*>(arg);
            DisplayLockGuard lock(display);
            lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN);
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "notification_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    // Create a power management lock
    auto ret = esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "display_update", &pm_lock_);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGI(TAG, "Power management not supported");
    } else {
        ESP_ERROR_CHECK(ret);
    }
}

LvglDisplay::~LvglDisplay() {
    if (notification_timer_ != nullptr) {
        esp_timer_stop(notification_timer_);
        esp_timer_delete(notification_timer_);
    }

    if (network_label_ != nullptr) {
        lv_obj_del(network_label_);
    }
    if (notification_label_ != nullptr) {
        lv_obj_del(notification_label_);
    }
    if (status_label_ != nullptr) {
        lv_obj_del(status_label_);
    }
    if (mute_label_ != nullptr) {
        lv_obj_del(mute_label_);
    }
    if (battery_label_ != nullptr) {
        lv_obj_del(battery_label_);
    }
    if( low_battery_popup_ != nullptr ) {
        lv_obj_del(low_battery_popup_);
    }
    if (temperature_label_ != nullptr) {
        lv_obj_del(temperature_label_);
    }
    if (humidity_label_ != nullptr) {
        lv_obj_del(humidity_label_);
    }
    if (pm_lock_ != nullptr) {
        esp_pm_lock_delete(pm_lock_);
    }
}

void LvglDisplay::SetStatus(const char* status) {
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetStatus('%s') called before SetupUI() - message will be lost!", status);
    }
    DisplayLockGuard lock(this);
    if (status_label_ == nullptr) {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "SetStatus('%s') failed: status_label_ is nullptr (SetupUI() was called but label not created)", status);
        }
        return;
    }
    lv_label_set_text(status_label_, status);
    lv_obj_remove_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    last_status_update_time_ = std::chrono::system_clock::now();
}

void LvglDisplay::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void LvglDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "ShowNotification('%s') called before SetupUI() - message will be lost!", notification);
    }
    DisplayLockGuard lock(this);
    if (notification_label_ == nullptr) {
        if (setup_ui_called_) {
            ESP_LOGW(TAG, "ShowNotification('%s') failed: notification_label_ is nullptr (SetupUI() was called but label not created)", notification);
        }
        return;
    }
    lv_label_set_text(notification_label_, notification);
    lv_obj_remove_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);

    esp_timer_stop(notification_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
}

void LvglDisplay::UpdateStatusBar(bool update_all) {
    auto& app = Application::GetInstance();
    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();

    // Update mute icon
    {
        DisplayLockGuard lock(this);
        if (mute_label_ == nullptr) {
            return;
        }

        // Update icon if mute state changes
        if (codec->output_volume() == 0 && !muted_) {
            muted_ = true;
            lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_XMARK);
        } else if (codec->output_volume() > 0 && muted_) {
            muted_ = false;
            lv_label_set_text(mute_label_, "");
        }
    }

    // Update time
    if (app.GetDeviceState() == kDeviceStateIdle) {
        if (last_status_update_time_ + std::chrono::seconds(10) < std::chrono::system_clock::now()) {
            // Set status to clock "HH:MM"
            time_t now = time(NULL);
            struct tm* tm = localtime(&now);
            // Check if the we have already set the time
            if (tm->tm_year >= 2025 - 1900) {
                char time_str[16];
                strftime(time_str, sizeof(time_str), "%H:%M", tm);
                SetStatus(time_str);
            } else {
                ESP_LOGW(TAG, "System time is not set, tm_year: %d", tm->tm_year);
            }
        }
    }

    esp_pm_lock_acquire(pm_lock_);
    // Update battery icon
    int battery_level;
    bool charging, discharging;
    const char* icon = nullptr;
    if (board.GetBatteryLevel(battery_level, charging, discharging)) {
        if (charging) {
            icon = FONT_AWESOME_BATTERY_BOLT;
        } else {
            const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_QUARTER,    // 20-39%
                FONT_AWESOME_BATTERY_HALF,    // 40-59%
                FONT_AWESOME_BATTERY_THREE_QUARTERS,    // 60-79%
                FONT_AWESOME_BATTERY_FULL, // 80-99%
                FONT_AWESOME_BATTERY_FULL, // 100%
            };
            icon = levels[battery_level / 20];
        }
        DisplayLockGuard lock(this);
        if (battery_label_ != nullptr && battery_icon_ != icon) {
            battery_icon_ = icon;
            lv_label_set_text(battery_label_, battery_icon_);
        }

        // Check low battery popup only when clock tick event is triggered
        // Because when initializing, the battery level is not ready yet.
        if (low_battery_popup_ != nullptr && !update_all) {
            if (strcmp(icon, FONT_AWESOME_BATTERY_EMPTY) == 0 && discharging) {
                if (lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // Show if low battery popup is hidden
                    lv_obj_remove_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                    app.Schedule([&app]() {
                        app.PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
                    });
                }
            } else {
                // Hide the low battery popup when the battery is not empty
                if (!lv_obj_has_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN)) { // Hide if low battery popup is shown
                    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    // Update network icon every 10 seconds
    static int seconds_counter = 0;
    if (update_all || seconds_counter++ % 10 == 0) {
        // Don't read 4G network status during firmware upgrade to avoid occupying UART resources
        auto device_state = Application::GetInstance().GetDeviceState();
        static const std::vector<DeviceState> allowed_states = {
            kDeviceStateIdle,
            kDeviceStateStarting,
            kDeviceStateWifiConfiguring,
            kDeviceStateListening,
            kDeviceStateActivating,
        };
        if (std::find(allowed_states.begin(), allowed_states.end(), device_state) != allowed_states.end()) {
            icon = board.GetNetworkStateIcon();
            if (network_label_ != nullptr && icon != nullptr && network_icon_ != icon) {
                DisplayLockGuard lock(this);
                network_icon_ = icon;
                lv_label_set_text(network_label_, network_icon_);
            }
        }
    }

    esp_pm_lock_release(pm_lock_);
}

void LvglDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
}

void LvglDisplay::SetTemperatureHumidity(float temperature, float humidity) {
    ESP_LOGI("LvglDisplay", "SetTemperatureHumidity called: %.1fC, %.1f%%", temperature, humidity);
    DisplayLockGuard lock(this);
    if (temperature_label_ != nullptr) {
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%.1fC", temperature);
        lv_label_set_text(temperature_label_, temp_str);
        ESP_LOGI("LvglDisplay", "Temperature label updated: %s", temp_str);
    } else {
        ESP_LOGW("LvglDisplay", "Temperature label is null");
    }
    if (humidity_label_ != nullptr) {
        char hum_str[32];
        snprintf(hum_str, sizeof(hum_str), "%.1f%%", humidity);
        lv_label_set_text(humidity_label_, hum_str);
        ESP_LOGI("LvglDisplay", "Humidity label updated: %s", hum_str);
    } else {
        ESP_LOGW("LvglDisplay", "Humidity label is null");
    }
}

void LvglDisplay::SetMpu6050Data(float pitch, float roll, float yaw) {
    ESP_LOGI("LvglDisplay", "SetMpu6050Data called: Pitch=%.1f, Roll=%.1f, Yaw=%.1f", pitch, roll, yaw);
    DisplayLockGuard lock(this);
    if (mpu6050_label_ != nullptr) {
        char mpu_str[64];
        snprintf(mpu_str, sizeof(mpu_str), "Pitch:%.1f Roll:%.1f Yaw:%.1f", pitch, roll, yaw);
        lv_label_set_text(mpu6050_label_, mpu_str);
        ESP_LOGI("LvglDisplay", "MPU6050 label updated: %s", mpu_str);
    } else {
        ESP_LOGW("LvglDisplay", "MPU6050 label is null");
    }
    
    // 根据MPU6050数据调整表情
    if (!emotion_auto_update_) {
        ESP_LOGD("LvglDisplay", "Emotion auto-update is disabled, skipping");
        return;
    }
    
    const char* emotion = "neutral";
    
    // 倾斜阈值（度数），降低到10度使表情更容易触发
    const float TILT_THRESHOLD = 10.0f;
    
    // 使用pitch和roll的绝对值来判断倾斜程度
    float abs_pitch = fabsf(pitch);
    float abs_roll = fabsf(roll);
    
    if (abs_roll > TILT_THRESHOLD) {
        if (roll < 0) {
            emotion = "confused";  // 向左倾斜
        } else {
            emotion = "cool";      // 向右倾斜
        }
    } else if (abs_pitch > TILT_THRESHOLD) {
        if (pitch > 0) {
            emotion = "thinking";  // 向前倾斜
        } else {
            emotion = "sleepy";    // 向后倾斜
        }
    } else {
        emotion = "neutral";       // 正常状态
    }
    
    // 更新表情
    SetEmotion(emotion);
    ESP_LOGI("LvglDisplay", "MPU6050 emotion updated: %s", emotion);
}

void LvglDisplay::SetPowerSaveMode(bool on) {
    if (on) {
        SetChatMessage("system", "");
        SetEmotion("sleepy");
    } else {
        SetChatMessage("system", "");
        SetEmotion("neutral");
    }
}

bool LvglDisplay::SnapshotToJpeg(std::string& jpeg_data, int quality) {
#if CONFIG_LV_USE_SNAPSHOT
    DisplayLockGuard lock(this);

    lv_obj_t* screen = lv_screen_active();
    lv_draw_buf_t* draw_buffer = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB565);
    if (draw_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to take snapshot, draw_buffer is nullptr");
        return false;
    }

    // swap bytes
    uint16_t* data = (uint16_t*)draw_buffer->data;
    size_t pixel_count = draw_buffer->data_size / 2;
    for (size_t i = 0; i < pixel_count; i++) {
        data[i] = __builtin_bswap16(data[i]);
    }

    // Clear output string and use callback version to avoid pre-allocating large memory blocks
    jpeg_data.clear();

    // Use callback-based JPEG encoder to further save memory
    bool ret = image_to_jpeg_cb((uint8_t*)draw_buffer->data, draw_buffer->data_size, draw_buffer->header.w, draw_buffer->header.h, V4L2_PIX_FMT_RGB565, quality,
        [](void *arg, size_t index, const void *data, size_t len) -> size_t {
        std::string* output = static_cast<std::string*>(arg);
        if (data && len > 0) {
            output->append(static_cast<const char*>(data), len);
        }
        return len;
    }, &jpeg_data);
    if (!ret) {
        ESP_LOGE(TAG, "Failed to convert image to JPEG");
    }

    lv_draw_buf_destroy(draw_buffer);
    return ret;
#else
    ESP_LOGE(TAG, "LV_USE_SNAPSHOT is not enabled");
    return false;
#endif
}

void LvglDisplay::ShowSensorLabels(bool show) {
    DisplayLockGuard lock(this);
    if (temperature_label_) {
        if (show) {
            lv_obj_remove_flag(temperature_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(temperature_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (humidity_label_) {
        if (show) {
            lv_obj_remove_flag(humidity_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(humidity_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (mpu6050_label_) {
        if (show) {
            lv_obj_remove_flag(mpu6050_label_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(mpu6050_label_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LvglDisplay::ShowEmotion(bool show) {
    // 这个方法需要在具体的子类中实现，因为表情显示的方式不同
    // 这里只是一个基类的空实现
}

void LvglDisplay::SetEmotionAutoUpdate(bool enable) {
    emotion_auto_update_ = enable;
    ESP_LOGI(TAG, "Emotion auto-update set to: %s", enable ? "enabled" : "disabled");
}

void LvglDisplay::UpdateSensorDisplay(float temperature, float humidity, float pitch, float roll, float yaw) {
    DisplayLockGuard lock(this);
    if (temperature_label_) {
        char temp_str[32];
        snprintf(temp_str, sizeof(temp_str), "%.1fC", temperature);
        lv_label_set_text(temperature_label_, temp_str);
    }
    if (humidity_label_) {
        char hum_str[32];
        snprintf(hum_str, sizeof(hum_str), "%.1f%%", humidity);
        lv_label_set_text(humidity_label_, hum_str);
    }
    if (mpu6050_label_) {
        char mpu_str[64];
        snprintf(mpu_str, sizeof(mpu_str), "P:%.1f R:%.1f Y:%.1f", pitch, roll, yaw);
        lv_label_set_text(mpu6050_label_, mpu_str);
    }
}

void LvglDisplay::ShowStatusBar(bool show) {
    DisplayLockGuard lock(this);
    // 控制top_bar_的显示/隐藏
    if (top_bar_) {
        if (show) {
            lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    // 控制status_bar_的显示/隐藏
    if (status_bar_) {
        if (show) {
            lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
