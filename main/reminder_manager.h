#ifndef REMINDER_MANAGER_H
#define REMINDER_MANAGER_H

#include <string>
#include <vector>
#include <mutex>
#include <ctime>
#include <functional>

struct ReminderTask {
    int id = 0;
    time_t trigger_time = 0;
    std::string content;
    bool enabled = true;
    bool triggered = false;
    bool is_alarm = false;       // 是否为闹钟
    std::string repeat = "once";  // 重复方式: once, daily, weekly, monthly
    std::string ringtone = "default"; // 铃声
    int volume = 50;             // 音量
};

class ReminderManager {
public:
    static ReminderManager& GetInstance() {
        static ReminderManager instance;
        return instance;
    }

    void Initialize();

    int AddReminder(time_t trigger_time, const std::string& content);
    int AddAlarm(time_t trigger_time, const std::string& content, const std::string& repeat = "once");
    bool DeleteReminder(int id);
    bool UpdateReminder(int id, time_t trigger_time, const std::string& content);

    std::vector<ReminderTask> GetAllReminders();
    ReminderTask* GetReminder(int id);

    void CheckReminders();

    void SetTriggerCallback(std::function<void(const ReminderTask&)> callback);

private:
    ReminderManager();
    ~ReminderManager();

    void SaveToNvs();
    void LoadFromNvs();
    int GenerateId();
    time_t CalculateNextTriggerTime(const ReminderTask& task);

    std::vector<ReminderTask> reminders_;
    std::mutex mutex_;
    int next_id_ = 1;
    std::function<void(const ReminderTask&)> trigger_callback_;
};

#endif // REMINDER_MANAGER_H
