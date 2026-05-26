#include "Profiler.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace prf
{
    std::atomic<bool>& getProfileEnabled()
    {
        static std::atomic<bool> s_isProfileEnabled{false};
        return s_isProfileEnabled;
    }

    std::mutex& getMapMutex()
    {
        static std::mutex s_mapMutex{};
        return s_mapMutex;
    }

    std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>>& getInstances()
    {
        static std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>> s_instances{};
        return s_instances;
    }

    std::function<void(const std::string&)>& getLogHook()
    {
        static std::function<void(const std::string&)> s_logHook = [](const std::string& str) { std::cout << str; };
        return s_logHook;
    }

    void profilerInit(std::function<void(const std::string&)> &&hook)
    {
        getLogHook() = std::move(hook);
    }

    void BlockProfiler::print(const Precision precision, const std::string& blockName)
    {
        double divisor = 1.0;
        const char* unit = "ns";
        if (precision == Precision::Micro) { divisor = 1000.0; unit = "us"; }
        else if (precision == Precision::Milli) { divisor = 1000000.0; unit = "ms"; }
        printImpl(divisor, unit, getLogHook(), blockName);
    }

    void BlockProfiler::printAll(const Precision precision)
    {
        double divisor = 1.0;
        const char* unit = "ns";
        if (precision == Precision::Micro) { divisor = 1000.0; unit = "us"; }
        else if (precision == Precision::Milli) { divisor = 1000000.0; unit = "ms"; }
        printImpl(divisor, unit, getLogHook(), "");
    }

    void BlockProfiler::printImpl(double divisor, const char* unit, const std::function<void(const std::string&)>& hook, const std::string& targetBlock)
    {
        std::lock_guard<std::mutex> lock{getMapMutex()};
        auto& instances = getInstances();

        if (instances.empty())
        {
            hook("No profiling data recorded.\n");
            return;
        }

        if (!targetBlock.empty() && instances.find(targetBlock) == instances.end())
        {
            hook("No profiling data found for block: " + targetBlock + "\n");
            return;
        }

        hook("=========================================\n");
        hook("             PROFILER STATS              \n");
        hook("=========================================\n");

        std::ostringstream oss;
        auto flushLine = [&oss, &hook]() {
            hook(oss.str());
            oss.str("");
            oss.clear();
        };

        for (const auto& blockPair : instances)
        {
            if (!targetBlock.empty() && blockPair.first != targetBlock)
            {
                continue;
            }

            oss << "Block: [" << blockPair.first << "]\n";
            flushLine();
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
            flushLine();

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
                flushLine();
            }

            hook("-----------------------------------------\n");
        }
    }

    template <typename StringType>
    BlockProfiler& BlockProfiler::getInstance(const StringType& f_blockName)
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

    // Explicit instantiations for getInstance
    template BlockProfiler& BlockProfiler::getInstance<const char*>(const char* const&);
    template BlockProfiler& BlockProfiler::getInstance<std::string>(const std::string&);

    template <typename StringType>
    void BlockProfiler::add(const StringType& f_scopeName, const uint64_t f_timeTaken)
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

    // Explicit instantiations for add
    template void BlockProfiler::add<const char*>(const char* const&, const uint64_t);
    template void BlockProfiler::add<std::string>(const std::string&, const uint64_t);

    ScopeProfile::ScopeProfile(const char* f_blockName, const char* scopeName) :
        m_profiler(BlockProfiler::getInstance(f_blockName)), // Cache reference to avoid map lookup on destruction
        m_scopeName(scopeName)
    {
        if (getProfileEnabled().load(std::memory_order_relaxed))
        {
            m_start = std::chrono::steady_clock::now();
        }
    }

    ScopeProfile::~ScopeProfile()
    {
        if (getProfileEnabled().load(std::memory_order_relaxed))
        {
            m_end = std::chrono::steady_clock::now();
            const auto l_timeTaken = std::chrono::duration_cast<std::chrono::nanoseconds>(m_end - m_start).count();
            m_profiler.add(m_scopeName, l_timeTaken);
        }
    }
}