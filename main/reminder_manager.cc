#include "reminder_manager.h"
#include "settings.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include "audio_service.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <cJSON.h>
#include <sstream>

#define TAG "Reminder"
#define NVS_NAMESPACE "reminders"

ReminderManager::ReminderManager() {
}

ReminderManager::~ReminderManager() {
}

void ReminderManager::Initialize() {
    LoadFromNvs();
    ESP_LOGI(TAG, "Loaded %d reminders from NVS", reminders_.size());
}

void ReminderManager::SetTriggerCallback(std::function<void(const ReminderTask&)> callback) {
    trigger_callback_ = callback;
}

int ReminderManager::AddReminder(time_t trigger_time, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);

    ReminderTask task;
    task.id = GenerateId();
    task.trigger_time = trigger_time;
    task.content = content;
    task.enabled = true;
    task.triggered = false;

    reminders_.push_back(task);
    SaveToNvs();

    // 打印调试信息
    time_t now = time(nullptr);
    struct tm tm_now, tm_trigger;
    localtime_r(&now, &tm_now);
    localtime_r(&trigger_time, &tm_trigger);
    
    ESP_LOGI(TAG, "Added reminder %d", task.id);
    ESP_LOGI(TAG, "  Content: %s", task.content.c_str());
    ESP_LOGI(TAG, "  Current time: %04d-%02d-%02d %02d:%02d:%02d", 
             tm_now.tm_year+1900, tm_now.tm_mon+1, tm_now.tm_mday,
             tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    ESP_LOGI(TAG, "  Trigger time: %04d-%02d-%02d %02d:%02d:%02d",
             tm_trigger.tm_year+1900, tm_trigger.tm_mon+1, tm_trigger.tm_mday,
             tm_trigger.tm_hour, tm_trigger.tm_min, tm_trigger.tm_sec);
    ESP_LOGI(TAG, "  Will trigger in %ld seconds", trigger_time - now);
    
    return task.id;
}

int ReminderManager::AddAlarm(time_t trigger_time, const std::string& content, const std::string& repeat) {
    std::lock_guard<std::mutex> lock(mutex_);

    ReminderTask task;
    task.id = GenerateId();
    task.trigger_time = trigger_time;
    task.content = content;
    task.enabled = true;
    task.triggered = false;
    task.is_alarm = true;
    task.repeat = repeat;

    reminders_.push_back(task);
    SaveToNvs();

    // 打印调试信息
    time_t now = time(nullptr);
    struct tm tm_now, tm_trigger;
    localtime_r(&now, &tm_now);
    localtime_r(&trigger_time, &tm_trigger);
    
    ESP_LOGI(TAG, "Added alarm %d", task.id);
    ESP_LOGI(TAG, "  Content: %s", task.content.c_str());
    ESP_LOGI(TAG, "  Repeat: %s", task.repeat.c_str());
    ESP_LOGI(TAG, "  Trigger time: %04d-%02d-%02d %02d:%02d:%02d",
             tm_trigger.tm_year+1900, tm_trigger.tm_mon+1, tm_trigger.tm_mday,
             tm_trigger.tm_hour, tm_trigger.tm_min, tm_trigger.tm_sec);
    
    return task.id;
}

time_t ReminderManager::CalculateNextTriggerTime(const ReminderTask& task) {
    if (task.repeat == "once") {
        return 0;
    }

    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&task.trigger_time, &tm_info);
    localtime_r(&now, &tm_info); // 保持当前日期，但使用闹钟的时间

    if (task.repeat == "daily") {
        return now + 24 * 3600;
    } else if (task.repeat == "weekly") {
        return now + 7 * 24 * 3600;
    } else if (task.repeat == "monthly") {
        tm_info.tm_mon++;
        return mktime(&tm_info);
    }

    return 0;
}

bool ReminderManager::DeleteReminder(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(reminders_.begin(), reminders_.end(),
        [id](const ReminderTask& t) { return t.id == id; });

    if (it != reminders_.end()) {
        reminders_.erase(it);
        SaveToNvs();
        ESP_LOGI(TAG, "Deleted reminder %d", id);
        return true;
    }
    return false;
}

bool ReminderManager::UpdateReminder(int id, time_t trigger_time, const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(reminders_.begin(), reminders_.end(),
        [id](const ReminderTask& t) { return t.id == id; });

    if (it != reminders_.end()) {
        it->trigger_time = trigger_time;
        it->content = content;
        SaveToNvs();
        return true;
    }
    return false;
}

std::vector<ReminderTask> ReminderManager::GetAllReminders() {
    std::lock_guard<std::mutex> lock(mutex_);
    return reminders_;
}

ReminderTask* ReminderManager::GetReminder(int id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = std::find_if(reminders_.begin(), reminders_.end(),
        [id](const ReminderTask& t) { return t.id == id; });

    if (it != reminders_.end()) {
        return &(*it);
    }
    return nullptr;
}

int ReminderManager::GenerateId() {
    return next_id_++;
}

void ReminderManager::CheckReminders() {
    std::lock_guard<std::mutex> lock(mutex_);

    time_t now = time(nullptr);

    // 每秒打印一次状态日志（调试用）
    static int check_count = 0;
    check_count++;
    if (!reminders_.empty()) {
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        ESP_LOGI(TAG, "[%02d:%02d:%02d] Checking %d reminders", 
                 tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec, reminders_.size());

        // 打印每个提醒的倒计时
        for (const auto& task : reminders_) {
            if (task.enabled && !task.triggered) {
                time_t diff = task.trigger_time - now;
                if (diff > 0) {
                    int hours = diff / 3600;
                    int minutes = (diff % 3600) / 60;
                    int seconds = diff % 60;
                    ESP_LOGI(TAG, "  %s #%d: %s (倒计时: %02d:%02d:%02d)%s", 
                             task.is_alarm ? "Alarm" : "Reminder",
                             task.id, task.content.c_str(), hours, minutes, seconds,
                             task.is_alarm ? " (" + task.repeat + ")" : "");
                } else {
                    ESP_LOGI(TAG, "  %s #%d: %s (即将触发)%s", 
                             task.is_alarm ? "Alarm" : "Reminder",
                             task.id, task.content.c_str(),
                             task.is_alarm ? " (" + task.repeat + ")" : "");
                }
            }
        }
    }

    for (auto it = reminders_.begin(); it != reminders_.end(); ) {
        auto& task = *it;

        if (!task.enabled || task.triggered) {
            ++it;
            continue;
        }

        time_t diff = task.trigger_time - now;
        if (diff <= 0) {
            task.triggered = true;
            ESP_LOGI(TAG, "%s %d triggered: %s", task.is_alarm ? "Alarm" : "Reminder", task.id, task.content.c_str());

            ReminderTask triggered_task = task;

            if (task.is_alarm && task.repeat != "once") {
                time_t next_time = CalculateNextTriggerTime(task);
                if (next_time > 0) {
                    task.trigger_time = next_time;
                    task.triggered = false;
                    ESP_LOGI(TAG, "Alarm %d scheduled for next trigger: %s", task.id, ctime(&next_time));
                    ++it;
                } else {
                    it = reminders_.erase(it);
                }
            } else {
                it = reminders_.erase(it);
            }

            SaveToNvs();

            if (trigger_callback_) {
                trigger_callback_(triggered_task);
            } else {
                auto display = Board::GetInstance().GetDisplay();
                display->ShowNotification(triggered_task.content.c_str(), 30000);
                Application::GetInstance().Alert(task.is_alarm ? "闹钟" : "提醒", triggered_task.content.c_str(), "clock", "");
            }
        } else {
            ++it;
        }
    }
}

static std::string ReminderTaskToJson(const ReminderTask& task) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "id", task.id);
    cJSON_AddNumberToObject(json, "trigger_time", (double)task.trigger_time);
    cJSON_AddStringToObject(json, "content", task.content.c_str());
    cJSON_AddBoolToObject(json, "enabled", task.enabled);
    cJSON_AddBoolToObject(json, "triggered", task.triggered);
    cJSON_AddBoolToObject(json, "is_alarm", task.is_alarm);
    cJSON_AddStringToObject(json, "repeat", task.repeat.c_str());
    cJSON_AddStringToObject(json, "ringtone", task.ringtone.c_str());
    cJSON_AddNumberToObject(json, "volume", task.volume);

    char* json_str = cJSON_PrintUnformatted(json);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);
    return result;
}

static bool JsonToReminderTask(const std::string& json_str, ReminderTask& task) {
    cJSON* json = cJSON_Parse(json_str.c_str());
    if (json == nullptr) {
        return false;
    }

    cJSON* id = cJSON_GetObjectItem(json, "id");
    cJSON* trigger_time = cJSON_GetObjectItem(json, "trigger_time");
    cJSON* content = cJSON_GetObjectItem(json, "content");
    cJSON* enabled = cJSON_GetObjectItem(json, "enabled");
    cJSON* triggered = cJSON_GetObjectItem(json, "triggered");
    cJSON* is_alarm = cJSON_GetObjectItem(json, "is_alarm");
    cJSON* repeat = cJSON_GetObjectItem(json, "repeat");
    cJSON* ringtone = cJSON_GetObjectItem(json, "ringtone");
    cJSON* volume = cJSON_GetObjectItem(json, "volume");

    if (id && trigger_time && content) {
        task.id = id->valueint;
        task.trigger_time = (time_t)trigger_time->valuedouble;
        task.content = content->valuestring;
        task.enabled = enabled ? enabled->valueint : true;
        task.triggered = triggered ? triggered->valueint : false;
        task.is_alarm = is_alarm ? is_alarm->valueint : false;
        task.repeat = repeat ? repeat->valuestring : "once";
        task.ringtone = ringtone ? ringtone->valuestring : "default";
        task.volume = volume ? volume->valueint : 50;
        cJSON_Delete(json);
        return true;
    }

    cJSON_Delete(json);
    return false;
}

void ReminderManager::SaveToNvs() {
    Settings settings(NVS_NAMESPACE, true);

    settings.SetInt("next_id", next_id_);
    settings.SetInt("count", reminders_.size());

    for (size_t i = 0; i < reminders_.size(); i++) {
        std::string key = "task_" + std::to_string(i);
        std::string json = ReminderTaskToJson(reminders_[i]);
        settings.SetString(key, json);
    }
}

void ReminderManager::LoadFromNvs() {
    Settings settings(NVS_NAMESPACE, false);

    next_id_ = settings.GetInt("next_id", 1);
    int count = settings.GetInt("count", 0);

    for (int i = 0; i < count; i++) {
        std::string key = "task_" + std::to_string(i);
        std::string json = settings.GetString(key);

        if (!json.empty()) {
            ReminderTask task;
            if (JsonToReminderTask(json, task)) {
                reminders_.push_back(task);
            }
        }
    }
}
