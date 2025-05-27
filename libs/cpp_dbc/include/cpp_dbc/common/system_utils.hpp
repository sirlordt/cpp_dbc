// system_utils.hpp

#ifndef SYSTEM_UTILS_HPP
#define SYSTEM_UTILS_HPP

#include <mutex>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>

namespace cpp_dbc
{

    namespace system_utils
    {

        // Declare the mutex as external
        extern std::mutex global_cout_mutex;

        // Thread-safe print function
        inline void safePrint(const std::string &mark, const std::string &message)
        {
            std::lock_guard<std::mutex> lock(global_cout_mutex);
            std::cout << mark << ": " << message << std::endl;
        }

        inline std::string currentTimeMillis()
        {
            using namespace std::chrono;

            // Get current system time
            auto now = system_clock::now();

            // Convert to time_t for formatting hours/minutes/seconds
            std::time_t now_c = system_clock::to_time_t(now);
            std::tm tm = *std::localtime(&now_c);

            // Extract milliseconds
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            // Format to string
            std::ostringstream oss;
            oss << std::put_time(&tm, "%H:%M:%S")
                << '.' << std::setw(3) << std::setfill('0') << ms.count();

            return oss.str();
        }
    }

}

#endif // SYSTEM_UTILS_HPP
