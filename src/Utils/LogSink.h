#pragma once

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/log_msg.h>
#include <memory>
#include <mutex>
#include <deque>
#include <string>
#include <vector>

namespace Binax {

struct LogEntry
{
    spdlog::level::level_enum Level = spdlog::level::info;
    std::string               Message;
};

class LogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    static std::shared_ptr<LogSink> Instance()
    {
        static std::shared_ptr<LogSink> s(new LogSink());
        return s;
    }

    std::vector<LogEntry> Snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<LogEntry>(m_Entries.begin(), m_Entries.end());
    }

    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        m_Entries.clear();
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        m_Entries.push_back({ msg.level,
            std::string(msg.payload.begin(), msg.payload.end()) });

        if (m_Entries.size() > kMaxEntries)
            m_Entries.pop_front();
    }

    void flush_() override {}

private:
    LogSink() = default;
    static constexpr size_t kMaxEntries = 8192;
    std::deque<LogEntry> m_Entries;
};

} // namespace Binax