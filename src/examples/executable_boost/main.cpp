#include <array>
#include <iomanip>
#include <iostream>

#include <boost/log/expressions.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sinks.hpp>

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace sinks = boost::log::sinks;

// Define severity levels
enum severity_level
{
    normal,
    notification,
    warning,
    error,
    critical
};

// Define the attribute keywords
BOOST_LOG_ATTRIBUTE_KEYWORD(line_id, "LineID", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)

std::ostream& operator<<(std::ostream& stream, const severity_level lvl)
{
    static const std::array<const char*, 9> severities = {
        "normal", "notify", "warn", "error", "crit"
    };

    if (severities.size() > static_cast<size_t>(lvl)) {
        stream << std::setw(7) << std::left << severities[static_cast<size_t>(lvl)];
    } else {
        stream << static_cast<int>(lvl);
    }
    return stream;
}

void init()
{
    // Create a minimal severity table filter
    using min_severity_filter = expr::channel_severity_filter_actor<std::string, severity_level>;
    min_severity_filter min_severity = expr::channel_severity_filter(channel, severity);

    // Set up the minimum severity levels for different channels
    min_severity["general"] = notification;
    min_severity["network"] = warning;
    min_severity["gui"] = error;

    logging::add_console_log
    (
        std::clog,
        keywords::filter = min_severity || severity >= normal,
        keywords::format =
        (
            expr::stream
                << line_id
                << ": <" << severity
                << "> [" << channel << "] "
                << expr::smessage
        )
    );

    logging::add_common_attributes();
}

int main(int, char**)
{
    using logger_type = src::severity_channel_logger<severity_level, std::string>;

    init();

    logger_type lg;
    const std::string channel_name = "Root";

    BOOST_LOG_CHANNEL_SEV(lg, channel_name, normal) << "A normal severity level message";
    BOOST_LOG_CHANNEL_SEV(lg, channel_name, notification) << "A notification severity level message";
    BOOST_LOG_CHANNEL_SEV(lg, channel_name, warning) << "A warning severity level message";
    BOOST_LOG_CHANNEL_SEV(lg, channel_name, error) << "An error severity level message";
    BOOST_LOG_CHANNEL_SEV(lg, channel_name, critical) << "A critical severity level message";

    return 0;
}
