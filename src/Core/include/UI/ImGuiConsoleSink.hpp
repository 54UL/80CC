#ifndef IMGUI_CONSOLE_SINK_HPP
#define IMGUI_CONSOLE_SINK_HPP

#include <UI/Console.hpp>
#include <spdlog/sinks/base_sink.h>
#include <chrono>
#include <ctime>
#include <mutex>

namespace ettycc
{
    template <typename Mutex>
    class ImGuiConsoleSink : public spdlog::sinks::base_sink<Mutex>
    {
    public:
        explicit ImGuiConsoleSink(DebugConsole *console) : console_(console) {}

    protected:
        void sink_it_(const spdlog::details::log_msg &msg) override
        {
            if (!console_) return;

            // Timestamp matching spdlog's default format: HH:MM:SS.mmm
            auto t   = std::chrono::system_clock::to_time_t(msg.time);
            auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                           msg.time.time_since_epoch()) % 1000;
            std::tm tm{};
            localtime_s(&tm, &t);

            // Map level to tag — indices match spdlog::level::level_enum
            static constexpr const char *kTags[] = {
                "[trace]", "[debug]", "[info]", "[warn]", "[error]", "[critical]", ""
            };
            const char *tag = kTags[std::min<int>(msg.level, spdlog::level::off)];

            std::string payload(msg.payload.begin(), msg.payload.end());
            console_->AddLog("[%02d:%02d:%02d.%03lld] %s %s",
                             tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count(),
                             tag, payload.c_str());
        }

        void flush_() override {}

    private:
        DebugConsole *console_;
    };

    using ImGuiConsoleSink_mt = ImGuiConsoleSink<std::mutex>;
    using ImGuiConsoleSink_st = ImGuiConsoleSink<spdlog::details::null_mutex>;

} // namespace ettycc

#endif
