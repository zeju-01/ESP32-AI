#include "mcp_server.h"
#include "application.h"
#include "reminder_manager.h"
#include "time_parser.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>
#include <cJSON.h>
#include <sstream>
#include <ctime>

#define TAG "ReminderTool"

static std::string FormatTimestamp(time_t timestamp) {
    struct tm tm_info;
    localtime_r(&timestamp, &tm_info);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &tm_info);
    return std::string(buffer);
}

void RegisterReminderTools() {
    auto& mcp_server = McpServer::GetInstance();

    mcp_server.AddUserOnlyTool("self.reminder.set",
        "Set a reminder or alarm. Parse the time from the text and create a reminder task.\n"
        "Supports absolute time: '下午3点', '15点30分', '15:30'\n"
        "Supports relative time: '10分钟后', '1小时后'\n"
        "The content should describe what to remind about.\n",
        PropertyList({
            Property("text", kPropertyTypeString, "The full text describing the reminder with time info")
        }),
        [](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI(TAG, ">>> self.reminder.set CALLED");
            auto text = properties["text"].value<std::string>();
            ESP_LOGI(TAG, "Setting reminder: %s", text.c_str());

            ParsedTime parsed = TimeParser::Parse(text);
            if (!parsed.success) {
                std::string error_msg = "无法解析时间，请提供更明确的时间信息。例如：\"下午3点提醒我开会\" 或 \"10分钟后提醒我\"";
                ESP_LOGW(TAG, "Failed to parse time from: %s", text.c_str());
                return error_msg;
            }

            time_t trigger_time = parsed.GetTimestamp();

            std::string content = text;
            size_t reminder_pos = content.find("提醒");
            if (reminder_pos != std::string::npos) {
                content = content.substr(reminder_pos);
            }

            int id = ReminderManager::GetInstance().AddReminder(trigger_time, content);

            char response[256];
            snprintf(response, sizeof(response), 
                "提醒已设置：\n内容：%s\n时间：%s", 
                content.c_str(), FormatTimestamp(trigger_time).c_str());
            
            auto display = Board::GetInstance().GetDisplay();
            display->ShowNotification(response, 5000);

            return std::string(response);
        });

    mcp_server.AddUserOnlyTool("self.alarm.set",
        "Set an alarm with repeat option. Parse the time from the text and create an alarm task.\n"
        "Supports absolute time: '下午3点', '15点30分', '15:30'\n"
        "Repeat options: 'once', 'daily', 'weekly', 'monthly'\n",
        PropertyList({
            Property("text", kPropertyTypeString, "The full text describing the alarm with time info"),
            Property("repeat", kPropertyTypeString, "once")
        }),
        [](const PropertyList& properties) -> ReturnValue {
            ESP_LOGI(TAG, ">>> self.alarm.set CALLED");
            auto text = properties["text"].value<std::string>();
            auto repeat = properties["repeat"].value<std::string>();
            ESP_LOGI(TAG, "Setting alarm: %s, repeat: %s", text.c_str(), repeat.c_str());

            ParsedTime parsed = TimeParser::Parse(text);
            if (!parsed.success) {
                std::string error_msg = "无法解析时间，请提供更明确的时间信息。例如：\"早上7点闹钟\" 或 \"每天8点闹钟\"";
                ESP_LOGW(TAG, "Failed to parse time from: %s", text.c_str());
                return error_msg;
            }

            time_t trigger_time = parsed.GetTimestamp();

            std::string content = text;
            size_t alarm_pos = content.find("闹钟");
            if (alarm_pos != std::string::npos) {
                content = content.substr(alarm_pos);
            } else {
                content = "闹钟: " + content;
            }

            int id = ReminderManager::GetInstance().AddAlarm(trigger_time, content, repeat);

            char response[256];
            snprintf(response, sizeof(response), 
                "闹钟已设置：\n内容：%s\n时间：%s\n重复：%s", 
                content.c_str(), FormatTimestamp(trigger_time).c_str(), repeat.c_str());
            
            auto display = Board::GetInstance().GetDisplay();
            display->ShowNotification(response, 5000);

            return std::string(response);
        });

    mcp_server.AddUserOnlyTool("self.reminder.list",
        "List all current reminders and alarms",
        PropertyList(),
        [](const PropertyList& properties) -> ReturnValue {
            auto reminders = ReminderManager::GetInstance().GetAllReminders();
            
            if (reminders.empty()) {
                return std::string("当前没有提醒或闹钟");
            }

            std::string result = "当前提醒和闹钟：\n";
            for (const auto& r : reminders) {
                char line[256];
                const char* type = r.is_alarm ? "闹钟" : "提醒";
                const char* status = r.triggered ? "已触发" : (r.enabled ? "未触发" : "已禁用");
                snprintf(line, sizeof(line), 
                    "[%d] %s - %s - %s [%s]\n", 
                    r.id, type, FormatTimestamp(r.trigger_time).c_str(), 
                    r.content.c_str(), status);
                result += line;
                if (r.is_alarm) {
                    char repeat_line[100];
                    snprintf(repeat_line, sizeof(repeat_line), "  重复：%s\n", r.repeat.c_str());
                    result += repeat_line;
                }
            }

            ESP_LOGI(TAG, "Listing %d reminders/alarms", reminders.size());
            return result;
        });

    mcp_server.AddUserOnlyTool("self.reminder.delete",
        "Delete a reminder or alarm by its ID",
        PropertyList({
            Property("id", kPropertyTypeInteger, "The ID of the reminder or alarm to delete")
        }),
        [](const PropertyList& properties) -> ReturnValue {
            int id = properties["id"].value<int>();
            bool success = ReminderManager::GetInstance().DeleteReminder(id);
            
            if (success) {
                return std::string("已删除提醒/闹钟 #" + std::to_string(id));
            } else {
                return std::string("未找到提醒/闹钟 #" + std::to_string(id));
            }
        });

    ESP_LOGI(TAG, "Reminder tools registered");
    ESP_LOGI(TAG, "  - self.reminder.set");
    ESP_LOGI(TAG, "  - self.alarm.set");
    ESP_LOGI(TAG, "  - self.reminder.list");
    ESP_LOGI(TAG, "  - self.reminder.delete");
}
