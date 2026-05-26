#include "Profiler.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace prf
{
    ProfileInput& getProfileInput()
    {
        static ProfileInput l_input = {
            [](const std::string& f_str) { std::cout << f_str; },
            []() -> uint64_t {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count());
            }
        };
        return l_input;
    }

    static std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>>& getInstances();

    std::atomic<bool>& BlockProfiler::getProfileEnabled()
    {
        static std::atomic<bool> l_isProfileEnabled{false};
        return l_isProfileEnabled;
    }

    std::mutex& getMapMutex()
    {
        static std::mutex l_mapMutex{};
        return l_mapMutex;
    }

    std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>>& getInstances()
    {
        static std::map<std::string, std::unique_ptr<BlockProfiler>, std::less<>> l_instances{};
        return l_instances;
    }

    void BlockProfiler::profilerInit(const ProfileInput& f_input)
    {
        if (f_input.m_logCallback) getProfileInput().m_logCallback = f_input.m_logCallback;
        if (f_input.m_getCurrentTime) getProfileInput().m_getCurrentTime = f_input.m_getCurrentTime;
        getProfileInput().m_logCallback("Profiler Status:"+ std::to_string(BlockProfiler::getProfileEnabled().load(std::memory_order_relaxed) ? 1 : 0) + "\n");
    }

    void BlockProfiler::print(const Precision f_precision, const std::vector<std::string>& f_blockNames)
    {
        double l_divisor = 1.0;
        const char* l_unit = "ns";
        if (f_precision == Precision::Micro) { l_divisor = 1000.0; l_unit = "us"; }
        else if (f_precision == Precision::Milli) { l_divisor = 1000000.0; l_unit = "ms"; }
        printImpl(l_divisor, l_unit, getProfileInput().m_logCallback, f_blockNames);
    }

    void BlockProfiler::printAll(const Precision f_precision)
    {
        double l_divisor = 1.0;
        const char* l_unit = "ns";
        if (f_precision == Precision::Micro) { l_divisor = 1000.0; l_unit = "us"; }
        else if (f_precision == Precision::Milli) { l_divisor = 1000000.0; l_unit = "ms"; }
        printImpl(l_divisor, l_unit, getProfileInput().m_logCallback, {});
    }

    void BlockProfiler::printImpl(const double f_divisor, const char* const f_unit, const std::function<void(const std::string&)>& f_hook, const std::vector<std::string>& f_targetBlocks)
    {
        std::lock_guard<std::mutex> l_lock{getMapMutex()};
        auto& l_instances = getInstances();

        if (l_instances.empty())
        {
            f_hook("No profiling data recorded.\n");
            return;
        }

        if (!f_targetBlocks.empty())
        {
            bool l_anyFound = false;
            for (const auto& l_target : f_targetBlocks)
            {
                if (l_instances.find(l_target) != l_instances.end())
                {
                    l_anyFound = true;
                    break;
                }
            }
            if (!l_anyFound)
            {
                f_hook("No profiling data found for the specified blocks.\n");
                return;
            }
        }

        f_hook("=========================================\n");
        f_hook("             PROFILER STATS              \n");
        f_hook("=========================================\n");

        std::ostringstream l_oss;
        auto l_flushLine = [&l_oss, &f_hook]() {
            f_hook(l_oss.str());
            l_oss.str("");
            l_oss.clear();
        };

        for (const auto& l_blockPair : l_instances)
        {
            if (!f_targetBlocks.empty() && std::find(f_targetBlocks.begin(), f_targetBlocks.end(), l_blockPair.first) == f_targetBlocks.end())
            {
                continue;
            }

            l_oss << "Block: [" << l_blockPair.first << "]\n";
            l_flushLine();
            BlockProfiler* l_profiler = l_blockPair.second.get();

            std::lock_guard<std::mutex> l_blockLock{l_profiler->m_mutex};
            
            std::string l_minCol = std::string("Min (") + f_unit + ")";
            std::string l_maxCol = std::string("Max (") + f_unit + ")";
            std::string l_avgCol = std::string("Avg (") + f_unit + ")";
            std::string l_totalCol = std::string("Total (") + f_unit + ")";

            l_oss << std::left 
                << std::setw(25) << "Instance" << " | "
                << std::setw(8)  << "Count" << " | "
                << std::setw(12) << l_minCol << " | "
                << std::setw(12) << l_maxCol << " | "
                << std::setw(12) << l_avgCol << " | "
                << std::setw(15) << l_totalCol << " |\n";
            l_flushLine();

            if (f_divisor != 1.0)
            {
                l_oss << std::fixed << std::setprecision(3);
            }

            for (const auto& l_scopePair : l_profiler->m_scopesDataMap)
            {
                const ScopeData& l_data = l_scopePair.second;
                l_oss << std::left 
                    << std::setw(25) << l_scopePair.first << " | "
                    << std::setw(8)  << l_data.m_count << " | ";
                
                if (f_divisor == 1.0)
                {
                    l_oss << std::setw(12) << l_data.m_minTimeNs << " | "
                        << std::setw(12) << l_data.m_maxTimeNs << " | "
                        << std::setw(12) << l_data.m_avgTimeNs << " | "
                        << std::setw(15) << l_data.m_totalTimeNs << " |\n";
                }
                else
                {
                    l_oss << std::setw(12) << (l_data.m_minTimeNs / f_divisor) << " | "
                        << std::setw(12) << (l_data.m_maxTimeNs / f_divisor) << " | "
                        << std::setw(12) << (l_data.m_avgTimeNs / f_divisor) << " | "
                        << std::setw(15) << (l_data.m_totalTimeNs / f_divisor) << " |\n";
                }
                l_flushLine();
            }

            f_hook("-----------------------------------------\n");
        }
    }

    template <typename StringType>
    BlockProfiler& BlockProfiler::getInstance(const StringType& f_blockName)
    {
        std::lock_guard<std::mutex> l_lock{getMapMutex()};
        auto& l_instances = getInstances();
        auto l_it = l_instances.find(f_blockName);
        if (l_it == l_instances.end())
        {
            std::unique_ptr<BlockProfiler> l_instance(new BlockProfiler());
            l_instance->m_blockName = f_blockName;
            l_it = l_instances.emplace(std::string{f_blockName}, std::move(l_instance)).first;
        }
        return *l_it->second; // Return dereferenced unique_ptr
    }

    // Explicit instantiations for getInstance
    template BlockProfiler& BlockProfiler::getInstance<const char*>(const char* const&);
    template BlockProfiler& BlockProfiler::getInstance<std::string>(const std::string&);

    template <typename StringType>
    void BlockProfiler::add(const StringType& f_scopeName, const uint64_t f_timeTaken)
    {
        std::lock_guard<std::mutex> l_lock{m_mutex};
        auto l_it = m_scopesDataMap.find(f_scopeName);
        if (l_it == m_scopesDataMap.end())
        {
            l_it = m_scopesDataMap.emplace(std::string{f_scopeName}, ScopeData{}).first;
        }
        
        auto& l_scopeData = l_it->second;
        l_scopeData.m_count++;
        l_scopeData.m_totalTimeNs += f_timeTaken;
        l_scopeData.m_minTimeNs = std::min(l_scopeData.m_minTimeNs, f_timeTaken);
        l_scopeData.m_maxTimeNs = std::max(l_scopeData.m_maxTimeNs, f_timeTaken);
        l_scopeData.m_avgTimeNs = l_scopeData.m_totalTimeNs / l_scopeData.m_count;
    }

    // Explicit instantiations for add
    template void BlockProfiler::add<const char*>(const char* const&, const uint64_t);
    template void BlockProfiler::add<std::string>(const std::string&, const uint64_t);

    ScopeProfile::ScopeProfile(const char* const f_blockName, const char* const f_scopeName) :
        m_profiler(BlockProfiler::getInstance(f_blockName)), // Cache reference to avoid map lookup on destruction
        m_scopeName(f_scopeName)
    {
        if (BlockProfiler::getProfileEnabled().load(std::memory_order_relaxed))
        {
            m_start = getProfileInput().m_getCurrentTime();
        }
    }

    ScopeProfile::~ScopeProfile()
    {
        if (BlockProfiler::getProfileEnabled().load(std::memory_order_relaxed))
        {
            m_end = getProfileInput().m_getCurrentTime();
            const uint64_t l_timeTaken = m_end - m_start;
            m_profiler.add(m_scopeName, l_timeTaken);
        }
    }
}