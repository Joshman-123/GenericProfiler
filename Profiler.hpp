#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <map>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>
namespace prf
{
    class ScopeProfile;
    class BlockProfiler;

    // C++14 header-only global state wrapped in static functions (Meyer's Singleton)
    // This prevents ODR violations without needing C++17 'inline' variables.
    inline std::atomic<bool>& getProfileEnabled()
    {
        static std::atomic<bool> s_isProfileEnabled{false};
        return s_isProfileEnabled;
    }

    inline std::mutex& getMapMutex()
    {
        static std::mutex s_mapMutex{};
        return s_mapMutex;
    }

    // std::map with std::less<> supports heterogeneous lookup in C++14 (avoids allocation)
    inline auto& getInstances()
    {
        static std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>> s_instances{};
        return s_instances;
    }

    class BlockProfiler
    {
    private:
        struct ScopeData
        {
            uint64_t m_count{};
            uint64_t m_avgTimeNs{};
            uint64_t m_minTimeNs{std::numeric_limits<uint64_t>::max()};
            uint64_t m_maxTimeNs{};
            uint64_t m_totalTimeNs{};
        };
    public:
        // Templates ensure we can pass both const char* or std::string without conversions
        template <typename StringType>
        static BlockProfiler& getInstance(const StringType& f_blockName)
        {
            std::lock_guard<std::mutex> lock{getMapMutex()};
            auto& instances = getInstances();
            auto it = instances.find(f_blockName);
            if (it == instances.end())
            {
                std::unique_ptr<BlockProfiler> instance(new BlockProfiler());
                instance->m_blockName = f_blockName;
                it = instances.emplace(std::string{f_blockName}, std::move(instance)).first;
            }
            return *it->second; // Return dereferenced unique_ptr
        }

        template <typename StringType>
        void add(const StringType& f_scopeName, const uint64_t f_timeTaken)
        {
            std::lock_guard<std::mutex> lock{m_mutex};
            auto it = m_scopesDataMap.find(f_scopeName);
            if (it == m_scopesDataMap.end())
            {
                it = m_scopesDataMap.emplace(std::string{f_scopeName}, ScopeData{}).first;
            }
            
            auto& scopeData = it->second;
            scopeData.m_count++;
            scopeData.m_totalTimeNs += f_timeTaken;
            scopeData.m_minTimeNs = std::min(scopeData.m_minTimeNs, f_timeTaken);
            scopeData.m_maxTimeNs = std::max(scopeData.m_maxTimeNs, f_timeTaken);
            scopeData.m_avgTimeNs = scopeData.m_totalTimeNs / scopeData.m_count;
        }

        static void printNs()
        {
            printImpl(1.0, "ns", [](const std::string& str) { std::cout << str; });
        }

        template <typename PrintHook>
        static void printNs(PrintHook hook)
        {
            printImpl(1.0, "ns", hook);
        }

        static void printUs()
        {
            printImpl(1000.0, "us", [](const std::string& str) { std::cout << str; });
        }

        template <typename PrintHook>
        static void printUs(PrintHook hook)
        {
            printImpl(1000.0, "us", hook);
        }

        static void printMs()
        {
            printImpl(1000000.0, "ms", [](const std::string& str) { std::cout << str; });
        }

        template <typename PrintHook>
        static void printMs(PrintHook hook)
        {
            printImpl(1000000.0, "ms", hook);
        }

    private:
        template <typename PrintHook>
        static void printImpl(double divisor, const char* unit, PrintHook hook)
        {
            std::lock_guard<std::mutex> lock{getMapMutex()};
            auto& instances = getInstances();

            if (instances.empty())
            {
                hook("No profiling data recorded.\n");
                return;
            }

            std::ostringstream oss;
            oss << "=========================================\n";
            oss << "             PROFILER STATS              \n";
            oss << "=========================================\n";

            for (const auto& blockPair : instances)
            {
                oss << "Block: [" << blockPair.first << "]\n";
                BlockProfiler* profiler = blockPair.second.get();

                std::lock_guard<std::mutex> blockLock{profiler->m_mutex};
                
                std::string minCol = std::string("Min (") + unit + ")";
                std::string maxCol = std::string("Max (") + unit + ")";
                std::string avgCol = std::string("Avg (") + unit + ")";
                std::string totalCol = std::string("Total (") + unit + ")";

                oss << std::left 
                    << std::setw(25) << "Instance" << " | "
                    << std::setw(8)  << "Count" << " | "
                    << std::setw(12) << minCol << " | "
                    << std::setw(12) << maxCol << " | "
                    << std::setw(12) << avgCol << " | "
                    << std::setw(15) << totalCol << " |\n";

                if (divisor != 1.0)
                {
                    oss << std::fixed << std::setprecision(3);
                }

                for (const auto& scopePair : profiler->m_scopesDataMap)
                {
                    const ScopeData& data = scopePair.second;
                    oss << std::left 
                        << std::setw(25) << scopePair.first << " | "
                        << std::setw(8)  << data.m_count << " | ";
                    
                    if (divisor == 1.0)
                    {
                        oss << std::setw(12) << data.m_minTimeNs << " | "
                            << std::setw(12) << data.m_maxTimeNs << " | "
                            << std::setw(12) << data.m_avgTimeNs << " | "
                            << std::setw(15) << data.m_totalTimeNs << " |\n";
                    }
                    else
                    {
                        oss << std::setw(12) << (data.m_minTimeNs / divisor) << " | "
                            << std::setw(12) << (data.m_maxTimeNs / divisor) << " | "
                            << std::setw(12) << (data.m_avgTimeNs / divisor) << " | "
                            << std::setw(15) << (data.m_totalTimeNs / divisor) << " |\n";
                    }
                }

                oss << "-----------------------------------------\n";
            }

            hook(oss.str());
        }
    private:
        BlockProfiler() = default;

    public:
        BlockProfiler(const BlockProfiler &) = delete;
        BlockProfiler(BlockProfiler &&) = delete;
        BlockProfiler &operator=(const BlockProfiler &) = delete;
        BlockProfiler &operator=(BlockProfiler &&) = delete;
        ~BlockProfiler() = default;

    private:
        std::string m_blockName{};
        std::mutex m_mutex{};
        std::map<std::string, ScopeData, std::less<>> m_scopesDataMap{};
    };

    class ScopeProfile
    {
    public:
        // Take const char* to avoid string allocations entirely.
        // Expects passing static string literals to prevent dangling references.
        explicit ScopeProfile(const char* f_blockName, const char* scopeName) :
            m_profiler(BlockProfiler::getInstance(f_blockName)), // Cache reference to avoid map lookup on destruction
            m_scopeName(scopeName)
        {
            if (getProfileEnabled().load(std::memory_order_relaxed))
            {
                m_start = std::chrono::steady_clock::now();
            }
        }

        ~ScopeProfile()
        {
            if (!getProfileEnabled().load(std::memory_order_relaxed))
            {
                return;
            }

            m_end = std::chrono::steady_clock::now();

            const auto l_timeTaken = std::chrono::duration_cast<std::chrono::nanoseconds>(
                m_end - m_start
            ).count();

            m_profiler.add(m_scopeName, l_timeTaken);
        }

    private:
        BlockProfiler& m_profiler;
        const char* m_scopeName; // Replaces dangerous dangling references & string_view
        std::chrono::steady_clock::time_point m_start{};
        std::chrono::steady_clock::time_point m_end{};
    };
}