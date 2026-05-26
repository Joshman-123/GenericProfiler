#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <utility>
#include "Profiler.hpp"

static const char* mainThread = "MainThread";
static const char* workerTasks = "WorkerTasks";

void workerTask(const int f_sleepMs)
{
    prf::ScopeProfile l_profile(workerTasks, "Thread Sleep");
    std::this_thread::sleep_for(std::chrono::milliseconds(f_sleepMs));

    {
        prf::ScopeProfile l_subProfile(workerTasks, "Inner Loop");
        for (int l_i = 0; l_i < 3; ++l_i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
}

void dbQuery()
{
    prf::ScopeProfile l_profile(mainThread, "Database Query");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

int main()
{
    // Enable profiling globally
    prf::BlkProf::getProfileEnabled().store(true);

    prf::ProfileInput l_input{};

    l_input.m_logCallback = [](const std::string& f_output) 
    {
        std::cout << "[Gen4] " << f_output;
    };

    l_input.m_getCurrentTime = []() -> uint64_t 
    {
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count());
    };

    prf::BlkProf::profilerInit(l_input);

    std::cout << "Starting profiled tasks...\n";

    {
        prf::ScopeProfile l_mainProfile(mainThread, "Total Execution");
        
        dbQuery();

        std::vector<std::thread> l_threads;
        for (int l_i = 1; l_i <= 4; ++l_i)
            l_threads.emplace_back(workerTask, l_i * 25);

        for (auto& l_t : l_threads)
            l_t.join();
    }

    // Print recorded profiling data
    // This uses the custom hook we provided to profilerInit()
    prf::BlkProf::printAll(prf::Precision::Micro);
    std::vector<std::string> l_vecBlock{workerTasks, mainThread};
    prf::BlkProf::print(prf::Precision::Milli, l_vecBlock);

    return 0;
}