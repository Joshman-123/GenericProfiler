#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <functional>
namespace prf
{
    using logHook = std::function<void(const std::string&)>;

    class BlockProfiler;

    enum class Precision : uint8_t
    {
        Nano,
        Micro,
        Milli
    };

    std::atomic<bool>& getProfileEnabled();

    std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>>& getInstances();

    void profilerInit(logHook &&hook);

    class BlockProfiler final
    {
    public:
        template <typename StringType>
        static BlockProfiler& getInstance(const StringType& f_blockName);

        template <typename StringType>
        void add(const StringType& f_scopeName, const uint64_t f_timeTaken);

        static void print(const Precision precision, const std::string& blockName);
        static void printAll(const Precision precision);
        ~BlockProfiler() = default;

    private:

        struct ScopeData
        {
            uint64_t m_count{};
            uint64_t m_avgTimeNs{};
            uint64_t m_minTimeNs{std::numeric_limits<uint64_t>::max()};
            uint64_t m_maxTimeNs{};
            uint64_t m_totalTimeNs{};
        };
        static void printImpl(double divisor, const char* unit, const std::function<void(const std::string&)>& hook, const std::string& targetBlock);
        BlockProfiler() = default;
        BlockProfiler(const BlockProfiler &) = delete;
        BlockProfiler(BlockProfiler &&) = delete;
        BlockProfiler &operator=(const BlockProfiler &) = delete;
        BlockProfiler &operator=(BlockProfiler &&) = delete;


    private:
        std::string m_blockName{};
        std::mutex m_mutex{};
        std::map<std::string, ScopeData, std::less<>> m_scopesDataMap{};
    };

    class ScopeProfile final
    {
    public:
        explicit ScopeProfile(const char* f_blockName, const char* scopeName);

        ~ScopeProfile();

    private:
        BlockProfiler& m_profiler;
        const char* m_scopeName = nullptr; // Replaces dangerous dangling references & string_view
        std::chrono::steady_clock::time_point m_start{};
        std::chrono::steady_clock::time_point m_end{};
    };
}