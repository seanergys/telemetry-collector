// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
//
// SPDX-License-Identifier: EUPL-1.2

#include <seanerbus/client/connection.hpp>
#include <seanerbus/client/simple_event_loop.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <string>
#include <utility>

int main(int argc, char **argv) try
{
    if (argc != 5)
    {
        std::cerr << "usage: subscribe ENDPOINT UUID FIRST_VALUE VALUE_STEP\n";
        return 1;
    }

    constexpr int readingCount = 5;
    const double firstValue = std::stod(argv[3]);
    const double valueStep = std::stod(argv[4]);
    int validatedReadings = 0;
    seanerbus::client::Connection connection(argv[1], argv[1]);
    const seanerbus::msg::Entity entity{seanerbus::msg::Uuid{argv[2]}};
    connection.subscribe(entity, [&](auto &conn, auto result)
    {
        if (!result.ok() || result.unwrap_ok().payload_type() != TimeValueV1::TYPE)
        {
            std::cerr << "Failed to receive a TimeValueV1 message\n";
            conn.shutdown();
            return;
        }

        auto reading = seanerbus::msg::MessageReader<TimeValueV1>::from_msg(
            result.unwrap_ok()).unwrap_ok();
        const auto time = reading.reader().getTime();
        const auto value = reading.reader().getValue();
        std::cout << "time=" << time << ", value=" << value << std::endl;
        const double expected = firstValue + validatedReadings * valueStep;
        if (time == 0 || !std::isfinite(value) || value != expected)
        {
            std::cerr << "Invalid reading " << validatedReadings + 1
                      << ": expected value=" << expected << " and a positive timestamp\n";
            conn.shutdown();
            return;
        }

        ++validatedReadings;
        if (validatedReadings == readingCount)
            conn.shutdown();
    });

    seanerbus::client::SimpleEventLoop(connection).run();
    return validatedReadings == readingCount ? 0 : 1;
}
catch (const std::exception &error)
{
    std::cerr << "Subscriber failed: " << error.what() << '\n';
    return 1;
}
