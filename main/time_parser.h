#ifndef TIME_PARSER_H
#define TIME_PARSER_H

#include <string>
#include <ctime>

struct ParsedTime {
    bool success = false;
    bool is_absolute = false;
    int hour = 0;
    int minute = 0;
    int delay_minutes = 0;
    bool next_day = false;

    time_t GetTimestamp() const;
};

class TimeParser {
public:
    static ParsedTime Parse(const std::string& text);

private:
    static bool ParseAbsoluteTime(const std::string& text, ParsedTime& result);
    static bool ParseRelativeTime(const std::string& text, ParsedTime& result);
    static bool ParseChineseDigits(const std::string& text, int& start, int& value);
    static std::string ExtractTimePhrase(const std::string& text);
};

#endif // TIME_PARSER_H
