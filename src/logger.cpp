#include "logger.hpp"

#include <mutex>
#include <utility>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::shared_ptr<spdlog::details::thread_pool>
    Logger::thread_pool =
        std::make_shared<spdlog::details::thread_pool>(8192, 1);

namespace
{

    /*
     * Wrapper around another sink that forwards messages only when
     * the message level is between MinLevel and MaxLevel, inclusive.
     *
     * Example:
     *
     *   trace .. info      -> stdout / .out
     *   warn  .. critical  -> stderr / .err
     */
    template <
        spdlog::level::level_enum MinLevel,
        spdlog::level::level_enum MaxLevel>
    class LevelRangeSink final : public spdlog::sinks::sink
    {
    public:
        explicit LevelRangeSink(spdlog::sink_ptr sink)
            : sink_(std::move(sink))
        {
        }

        void log(const spdlog::details::log_msg &msg) override
        {
            if (msg.level >= MinLevel && msg.level <= MaxLevel)
            {
                sink_->log(msg);
            }
        }

        void flush() override
        {
            sink_->flush();
        }

        void set_pattern(const std::string &pattern) override
        {
            sink_->set_pattern(pattern);
        }

        void set_formatter(
            std::unique_ptr<spdlog::formatter> sink_formatter) override
        {
            sink_->set_formatter(std::move(sink_formatter));
        }

    private:
        spdlog::sink_ptr sink_;
    };

} // namespace

Logger::Logger()
{
    /*
     * Initialize one process-wide async queue.
     *
     * Queue size:       8192 messages
     * Logging threads:  1
     *
     * All plugin loggers created below use this same thread pool.
     *
     * IMPORTANT:
     * There should be only one owner responsible for calling
     * init_thread_pool().
     */
    // thread_pool = std::make_shared<spdlog::details::thread_pool>(8192, 1);

    /*
     * Actual output sinks.
     *
     * These are the resources shared by every plugin logger.
     */
    auto stdout_sink =
        std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    auto stderr_sink =
        std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

    stdout_sink->set_color(
        spdlog::level::trace,
        stdout_sink->white);

    stdout_sink->set_color(
        spdlog::level::debug,
        stdout_sink->cyan);

    stdout_sink->set_color(
        spdlog::level::info,
        stdout_sink->green);

    stderr_sink->set_color(
        spdlog::level::warn,
        stderr_sink->yellow_bold);

    stderr_sink->set_color(
        spdlog::level::err,
        stderr_sink->red_bold);

    stderr_sink->set_color(
        spdlog::level::critical,
        stderr_sink->bold_on_red);

    std::shared_ptr<spdlog::sinks::sink> err_file_sink, out_file_sink;
    try
    {
        // may be some flag later on ...
        err_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/var/log/cmi/telemetry-collector.err", true);
        out_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/var/log/cmi/telemetry-collector.out", true);
    }
    catch (const spdlog::spdlog_ex &)
    {
        err_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("./telemetry-collector.err", true);
        out_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("./telemetry-collector.out", true);
    }

    /*
     * OUT:
     *
     *   trace
     *   debug
     *   info
     */
    using OutSink = LevelRangeSink<
        spdlog::level::trace,
        spdlog::level::info>;

    /*
     * ERR:
     *
     *   warn
     *   error
     *   critical
     */
    using ErrSink = LevelRangeSink<
        spdlog::level::warn,
        spdlog::level::critical>;

    sinks_.push_back(
        std::make_shared<OutSink>(stdout_sink));

    sinks_.push_back(
        std::make_shared<OutSink>(out_file_sink));

    sinks_.push_back(
        std::make_shared<ErrSink>(stderr_sink));

    sinks_.push_back(
        std::make_shared<ErrSink>(err_file_sink));

    /*
     * Same format everywhere.
     *
     * Example:
     *
     * 2026-10-06 12:34:12.482
     * [plugin.cpu.0]
     * [12345]
     * [info]
     * Collector started
     *
     * %Y-%m-%d    date
     * %H:%M:%S    time
     * %e           milliseconds
     * %n           logger/component name
     * %t           thread ID
     * %l           log level
     * %v           message
     */
    const std::string pattern =
        "%Y-%m-%d %H:%M:%S.%e [%n] [%t] [%l] %v";

    for (auto &sink : sinks_)
    {
        sink->set_pattern(pattern);
    }
}

Logger::LoggerPtr Logger::create(const std::string &name)
{
    /*
     * Every logger uses:
     *
     *   - the same async thread pool
     *   - the same stdout sink
     *   - the same stderr sink
     *   - the same .out file
     *   - the same .err file
     *
     * But each logger has its own identifying name.
     */
    auto logger = std::make_shared<spdlog::async_logger>(
        name,
        sinks_.begin(),
        sinks_.end(),
        thread_pool,

        /*
         * For a telemetry collector I prefer overrun_oldest.
         *
         * If logging cannot keep up, old log messages can be
         * discarded rather than blocking telemetry collection.
         */
        spdlog::async_overflow_policy::overrun_oldest);

    /*
     * Permit every level.
     *
     * Individual sink filters decide where each level goes.
     */
    logger->set_level(spdlog::level::trace);

    /*
     * Don't flush after every message.
     *
     * Errors and critical messages will request a flush.
     */
    logger->flush_on(spdlog::level::err);

    return logger;
}

void Logger::flush()
{
    for (auto &sink : sinks_)
    {
        sink->flush();
    }
}

/*
int main()
{
    Logger logging;

    auto log = logging.create("main");

    log->trace("trace message");
    log->debug("debug message");
    log->info("info message");

    log->warn("warning message");
    log->error("error message");
    log->critical("critical message");

    logging.flush();

    return 0;
}
*/
