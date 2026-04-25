#include "time_parser.h"
#include <esp_log.h>
#include <cstring>
#include <regex>
#include <sstream>

#define TAG "TimeParser"

time_t ParsedTime::GetTimestamp() const {
    if (!success) {
        return 0;
    }

    time_t now = time(nullptr);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    if (is_absolute) {
        tm_info.tm_hour = hour;
        tm_info.tm_min = minute;
        tm_info.tm_sec = 0;

        time_t target = mktime(&tm_info);

        if (next_day) {
            target += 24 * 3600;
        } else if (target <= now) {
            target += 24 * 3600;
        }

        return target;
    } else {
        return now + delay_minutes * 60;
    }
}

static int ChineseDigitToInt(char c) {
    switch (c) {
        case '0': case '零': return 0;
        case '1': case '一': return 1;
        case '2': case '二': case '两': return 2;
        case '3': case '三': return 3;
        case '4': case '四': return 4;
        case '5': case '五': return 5;
        case '6': case '六': return 6;
        case '7': case '七': return 7;
        case '8': case '八': return 8;
        case '9': case '九': return 9;
        default: return -1;
    }
}

static bool ParseNumber(const std::string& text, size_t& pos, int& value) {
    if (pos >= text.size()) return false;

    if (std::isdigit(text[pos])) {
        value = 0;
        while (pos < text.size() && std::isdigit(text[pos])) {
            value = value * 10 + (text[pos] - '0');
            pos++;
        }
        return true;
    }

    if (pos + 1 < text.size()) {
        int d1 = ChineseDigitToInt(text[pos]);
        int d2 = ChineseDigitToInt(text[pos + 1]);
        if (d1 >= 0 && d2 >= 0) {
            value = d1 * 10 + d2;
            pos += 2;
            return true;
        }
    }

    int d = ChineseDigitToInt(text[pos]);
    if (d >= 0) {
        value = d;
        pos++;
        return true;
    }

    return false;
}

std::string TimeParser::ExtractTimePhrase(const std::string& text) {
    size_t reminder_pos = std::string::npos;

    const char* reminder_keywords[] = {
        "提醒", "闹钟", "叫我", "告诉我", "记得", "不要忘记"
    };
    for (const char* kw : reminder_keywords) {
        size_t pos = text.find(kw);
        if (pos != std::string::npos) {
            if (reminder_pos == std::string::npos || pos < reminder_pos) {
                reminder_pos = pos;
            }
        }
    }

    if (reminder_pos != std::string::npos) {
        return text.substr(reminder_pos);
    }
    return text;
}

bool TimeParser::ParseAbsoluteTime(const std::string& text, ParsedTime& result) {
    int hour = -1, minute = -1;
    bool has_ampm = false;
    bool is_pm = false;
    size_t pos = 0;

    while (pos < text.size()) {
        if (text.find("上午", pos) == pos || text.find("早上", pos) == pos || text.find("凌晨", pos) == pos) {
            has_ampm = true;
            is_pm = false;
            pos += 2;
            continue;
        }
        if (text.find("下午", pos) == pos || text.find("晚上", pos) == pos || text.find("傍晚", pos) == pos) {
            has_ampm = true;
            is_pm = true;
            pos += 2;
            continue;
        }
        if (text.find("中午", pos) == pos) {
            has_ampm = true;
            is_pm = true;
            pos += 2;
            continue;
        }

        int num = -1;
        if (ParseNumber(text, pos, num)) {
            if (hour == -1) {
                hour = num;
            } else if (minute == -1) {
                minute = num;
            }
            continue;
        }

        if (text.find("点", pos) == pos || text.find("時", pos) == pos || text.find("时", pos) == pos) {
            pos++;
            continue;
        }
        if (text.find("分", pos) == pos || text.find("分钟", pos) == pos) {
            pos++;
            if (pos < text.size() && text[pos] == '钟') pos++;
            continue;
        }
        if (text.find(":", pos) == pos || text.find("：", pos) == pos) {
            pos++;
            continue;
        }
        if (text.find("半", pos) == pos) {
            if (hour != -1 && minute == -1) {
                minute = 30;
            }
            pos++;
            continue;
        }

        pos++;
    }

    if (hour == -1) return false;
    if (minute == -1) minute = 0;

    if (has_ampm) {
        if (is_pm && hour < 12) {
            hour += 12;
        }
        if (!is_pm && hour == 12) {
            hour = 0;
        }
    }

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    result.success = true;
    result.is_absolute = true;
    result.hour = hour;
    result.minute = minute;
    result.next_day = false;
    return true;
}

bool TimeParser::ParseRelativeTime(const std::string& text, ParsedTime& result) {
    int number = -1;
    size_t pos = 0;

    while (pos < text.size()) {
        if (ParseNumber(text, pos, number)) {
            continue;
        }

        if (text.find("分钟", pos) == pos || text.find("分钟后", pos) == pos) {
            pos++;
            if (pos < text.size() && text[pos] == '钟') pos++;
            if (number > 0) {
                result.success = true;
                result.is_absolute = false;
                result.delay_minutes = number;
                return true;
            }
            continue;
        }

        if (text.find("小时", pos) == pos) {
            pos += 2;
            if (number > 0) {
                result.success = true;
                result.is_absolute = false;
                result.delay_minutes = number * 60;
                return true;
            }
            continue;
        }

        pos++;
    }

    return false;
}

ParsedTime TimeParser::Parse(const std::string& text) {
    ParsedTime result;

    std::string phrase = ExtractTimePhrase(text);

    if (ParseAbsoluteTime(phrase, result)) {
        return result;
    }

    if (ParseRelativeTime(phrase, result)) {
        return result;
    }

    ESP_LOGW(TAG, "Failed to parse time from: %s", text.c_str());
    return result;
}
