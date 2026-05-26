#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include "Profiler.hpp"

void workerTask(int sleepMs)
{
    prf::ScopeProfile profile("WorkerTasks", "Thread Sleep");
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

    {
        prf::ScopeProfile subProfile("WorkerTasks", "Inner Loop");
        for (int i = 0; i < 3; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
}

void dbQuery()
{
    prf::ScopeProfile profile("MainThread", "Database Query");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

int main()
{
    // Enable profiling globally
    prf::getProfileEnabled().store(true);

    prf::profilerInit([](const std::string& output) 
    {
        std::cout << "[Gen4] " << output;
    });

    std::cout << "Starting profiled tasks...\n";

    {
        prf::ScopeProfile mainProfile("MainThread", "Total Execution");
        
        dbQuery();

        std::vector<std::thread> threads;
        for (int i = 1; i <= 4; ++i)
            threads.emplace_back(workerTask, i * 25);

        for (auto& t : threads)
            t.join();
    }

    // Print recorded profiling data
    // This uses the custom hook we provided to profilerInit()
    prf::BlockProfiler::printUs();
    return 0;
}