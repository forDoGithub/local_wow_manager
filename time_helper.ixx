#include "framework.h"

export module time_helper;

export namespace time_helper {
    std::string readable() {
        using namespace std::chrono;

        auto now = system_clock::now(); // Current time
        auto current_time = zoned_time{ current_zone(), now }; // Get current time in the current time zone
        auto local_time = current_time.get_local_time(); // Convert to local time
        auto dp = floor<days>(local_time); // Get date part
        auto time = hh_mm_ss{ local_time - dp }; // Get time part

        return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
            int(year_month_day{ dp }.year()),
            unsigned(year_month_day{ dp }.month()),
            unsigned(year_month_day{ dp }.day()),
            time.hours().count(),
            time.minutes().count(),
            time.seconds().count());
    }

    std::string exact() {
        using namespace std::chrono;

        system_clock::time_point now = system_clock::now(); // Current time
        system_clock::duration duration_since_epoch = now.time_since_epoch(); // Get duration since epoch
        std::chrono::microseconds microseconds = duration_cast<std::chrono::microseconds>(duration_since_epoch);

        return std::to_string(microseconds.count());
    }

}