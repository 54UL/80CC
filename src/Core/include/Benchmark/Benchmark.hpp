#ifndef BENCHMARK_80CC_HPP
#define BENCHMARK_80CC_HPP

#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

namespace ettycc
{
    struct BenchmarkCheckpoint
    {
        std::string name;
        float       ms = 0.f;
    };

    class Benchmark
    {
    public:
        using Clock = std::chrono::high_resolution_clock;
        using Ms    = std::chrono::duration<float, std::milli>;

        void Begin()
        {
            origin_ = Clock::now();
            last_   = origin_;
            checkpoints_.clear();
        }

        void Mark(const std::string& name)
        {
            auto now = Clock::now();
            checkpoints_.push_back({name, Ms(now - last_).count()});
            last_ = now;
        }

        float TotalMs() const
        {
            if (checkpoints_.empty()) return 0.f;
            float sum = 0.f;
            for (const auto& cp : checkpoints_) sum += cp.ms;
            return sum;
        }


        //   date,config_resources_ms,physics_init_ms,audio_init_ms,
        //   scene_load_ms,total_ms,improvement_%

        // improvement_% = ((prev_total - cur_total) / prev_total) * 100
        //   positive = faster, negative = slower
        void WriteToFile(const std::string& filePath) const
        {
            float prevTotal = -1.f;
            {
                std::ifstream in(filePath);
                if (in.is_open())
                {
                    std::string line, lastLine;
                    while (std::getline(in, line))
                    {
                        if (!line.empty() && line[0] != 'd')
                            lastLine = line;
                    }
                    if (!lastLine.empty())
                    {

                        auto pos = lastLine.rfind(',');
                        if (pos != std::string::npos)
                        {
                            auto pos2 = lastLine.rfind(',', pos - 1);
                            if (pos2 != std::string::npos)
                            {
                                try { prevTotal = std::stof(lastLine.substr(pos2 + 1, pos - pos2 - 1)); }
                                catch (...) {}
                            }
                        }
                    }
                }
            }

            float total = TotalMs();
            float improvement = 0.f;
            if (prevTotal > 0.f)
                improvement = ((prevTotal - total) / prevTotal) * 100.f;

            bool needsHeader = false;
            {
                std::ifstream test(filePath);
                needsHeader = !test.good() || test.peek() == std::ifstream::traits_type::eof();
            }

            std::ofstream out(filePath, std::ios::app);
            if (!out.is_open())
            {
                spdlog::error("[Benchmark] Cannot open benchmark file: {}", filePath);
                return;
            }

            if (needsHeader)
            {
                out << "date";
                for (const auto& cp : checkpoints_)
                    out << "," << cp.name << "_ms";
                out << ",total_ms,improvement_%\n";
            }

            auto now   = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &timeT);
#else
            localtime_r(&timeT, &tm);
#endif
            out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

            for (const auto& cp : checkpoints_)
                out << "," << std::fixed << std::setprecision(2) << cp.ms;

            out << "," << std::fixed << std::setprecision(2) << total;
            if (prevTotal > 0.f)
                out << "," << std::showpos << std::fixed << std::setprecision(2) << improvement << "%";
            else
                out << ",N/A (first run)";
            out << "\n";
            out.flush();

            spdlog::info("╔══════════════════════════════════════════════════════╗");
            spdlog::info("║            BENCHMARK RESULTS                         ║");
            spdlog::info("╠══════════════════════════════════════════════════════╣");
            for (const auto& cp : checkpoints_)
                spdlog::info("║  {:<28s} {:>10.2f} ms    ║", cp.name, cp.ms);
            spdlog::info("╠══════════════════════════════════════════════════════╣");
            spdlog::info("║  {:<28s} {:>10.2f} ms    ║", "TOTAL", total);
            if (prevTotal > 0.f)
            {
                const char* tag = improvement > 0 ? "FASTER" : (improvement < 0 ? "SLOWER" : "SAME");
                spdlog::info("║  {:<28s} {:>+10.2f} %     ║",
                             std::string("vs last run (") + tag + ")", improvement);
            }
            else
            {
                spdlog::info("║  {:<28s} {:>10s}       ║", "vs last run", "first run");
            }
            spdlog::info("╚══════════════════════════════════════════════════════╝");
            spdlog::info("[Benchmark] Results appended to {}", filePath);
        }

    private:
        Clock::time_point              origin_;
        Clock::time_point              last_;
        std::vector<BenchmarkCheckpoint> checkpoints_;
    };

} // namespace ettycc

#endif
