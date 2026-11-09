// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <spdlog/async.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>


class Logger {
public:
    using LoggerPtr = std::shared_ptr<spdlog::logger>;

    Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * Create an asynchronous logger for a component.
     *
     * Examples:
     *
     *   auto log = logging.create("main");
     *   auto log = logging.create("plugin.cpu.0");
     *   auto log = logging.create("plugin.network.0");
     *
     * All loggers share the same sinks and async thread pool.
     */
    LoggerPtr create(const std::string& name);

    /**
     * Flush all shared sinks.
     */
    void flush();

private:
    std::vector<spdlog::sink_ptr> sinks_;
    static std::shared_ptr<spdlog::details::thread_pool> thread_pool;
};
