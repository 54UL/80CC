#ifndef ETTYCC_RANDOM_HPP
#define ETTYCC_RANDOM_HPP

#include <random>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <memory>

namespace ettycc
{
    // ------------------------------------------------------------------
    // RNG  -- lightweight wrapper around std::mt19937.
    //
    // Register a shared instance via the Dependency system so the same
    // engine is available everywhere without thread-local statics:
    //
    //   auto rng = std::make_shared<RNG>();
    //   RegisterDependency(RNG, rng);
    //   ...
    //   auto rng = GetDependency(RNG);
    //   rng->Float(-1.0f, 1.0f);
    //
    // Or create scoped instances with a fixed seed for reproducible
    // sequences without touching the shared one:
    //
    //   RNG local(42);
    //   local.Float(0.0f, 10.0f);
    // ------------------------------------------------------------------

    class RNG
    {
    public:
        RNG() : engine_(std::random_device{}()) {}

        explicit RNG(uint32_t seed) : engine_(seed) {}

        void Seed(uint32_t seed) { engine_.seed(seed); }

        int Int(int lo, int hi)
        {
            if (lo > hi) std::swap(lo, hi);
            std::uniform_int_distribution<int> dist(lo, hi);
            return dist(engine_);
        }

        int Int(int max = std::numeric_limits<int>::max())
        {
            return Int(0, max);
        }

        float Float(float lo, float hi)
        {
            if (lo == hi) return lo;
            if (lo > hi) std::swap(lo, hi);
            std::uniform_real_distribution<float> dist(lo, hi);
            return dist(engine_);
        }

        float Float()
        {
            return Float(0.0f, 1.0f);
        }

        std::mt19937& Engine() { return engine_; }

    private:
        std::mt19937 engine_;
    };

} // namespace ettycc

#endif // ETTYCC_RANDOM_HPP
