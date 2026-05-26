#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <chrono>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <functional>

/**
 * \brief Profiler namespace containing all profiling utilities and frameworks.
 */
namespace prf
{
    /** \brief Callable type used as a hook for logging text output. */
    using logHook = std::function<void(const std::string&)>;

    /** \brief Callable type used as a hook for fetching the current time in nanoseconds. */
    using timeHook = std::function<uint64_t()>;

    /**
     * \brief Configuration structure used for initializing the profiler framework.
     */
    struct ProfileInput
    {
        logHook m_logCallback;
        timeHook m_getCurrentTime;
    };

    class BlockProfiler;

    /** \brief Convenient alias for the BlockProfiler class. */
    using BlkProf = BlockProfiler;

    /**
     * \brief Enum defining the time precision when printing profiler output.
     */
    enum class Precision : uint8_t
    {
        Nano,
        Micro,
        Milli
    };

    /**
     * \brief Thread-safe singleton-based class for managing profiling blocks and scopes.
     */
    class BlockProfiler final
    {
    public:
        /**
         * \brief Retrieves the atomic flag controlling whether profiling is actively recorded globally.
         * \return Reference to the atomic boolean flag.
         */
        static std::atomic<bool>& getProfileEnabled();

        /**
         * \brief Initializes the profiler with custom configuration hooks.
         * \param input Configuration struct containing custom logging and time hooks.
         */
        static void profilerInit(const ProfileInput& f_input);

        /**
         * \brief Prints formatted profiling statistics for a specific block.
         * \param precision The time precision to format the output with.
         * \param f_blockNames A vector of exact names of the blocks to print.
         */
        static void print(const Precision f_precision, const std::vector<std::string>& f_blockNames);

        /**
         * \brief Prints formatted profiling statistics for all recorded blocks.
         * \param precision The time precision to format the output with.
         */
        static void printAll(const Precision f_precision);
        ~BlockProfiler() = default;

    private:
        friend class ScopeProfile;

        /**
         * \brief Gets or creates a profiler instance for a given block grouping.
         * \param f_blockName The block name to look up.
         * \return Reference to the BlockProfiler instance.
         */
        template <typename StringType>
        static BlockProfiler& getInstance(const StringType& f_blockName);

        /**
         * \brief Adds timing data to a specific scope within this block grouping.
         * \param f_scopeName The name of the profiled scope.
         * \param f_timeTaken The elapsed time for the scope in nanoseconds.
         */
        template <typename StringType>
        void add(const StringType& f_scopeName, const uint64_t f_timeTaken);

        /**
         * \brief Internal structure holding accumulated statistics for a single scope.
         */
        struct ScopeData
        {
            uint64_t m_count{};
            uint64_t m_avgTimeNs{};
            uint64_t m_minTimeNs{std::numeric_limits<uint64_t>::max()};
            uint64_t m_maxTimeNs{};
            uint64_t m_totalTimeNs{};
        };

        /**
         * \brief Implementation routine for formatting and printing statistics.
         * \param divisor Divisor to convert recorded nanoseconds to the requested precision.
         * \param unit String literal representing the appended time unit.
         * \param hook The logging hook to sequentially output strings.
         * \param f_targetBlocks The specific blocks to print, or empty to print all blocks.
         */
        static void printImpl(const double f_divisor, const char* const f_unit, const std::function<void(const std::string&)>& f_hook, const std::vector<std::string>& f_targetBlocks);
        
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

    /**
     * \brief RAII-based scope profiler that records execution time taken in a given lexical scope.
     */
    class ScopeProfile final
    {
    public:
        /**
         * \brief Constructs a ScopeProfile to start timing immediately if profiling is enabled.
         * \param f_blockName Name of the block grouping.
         * \param scopeName Name of the specific scope being profiled.
         */
        explicit ScopeProfile(const char* const f_blockName, const char* const f_scopeName);

        /**
         * \brief Destructor that concludes timing and securely records the elapsed data.
         */
        ~ScopeProfile();

    private:
        BlockProfiler& m_profiler;
        const char* m_scopeName = nullptr; // Replaces dangerous dangling references & string_view
        uint64_t m_start{};
        uint64_t m_end{};
    };
}