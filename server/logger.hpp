// server/logger.hpp
// Cross-platform colored logging utility
#ifndef SERVER_LOGGER_HPP
#define SERVER_LOGGER_HPP

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include <mutex>

#ifdef _WIN32
    #include <windows.h>
#endif

class Logger {
public:
    enum class Category {
        SERVER,
        HUB,
        HTTP,
        SESSION,
        CHAT,
        CONFIG,
        LISTENER,
        ERROR
    };

private:
    static inline std::mutex logMutex;  // inline static to avoid multiple definition

    // ANSI color codes for different categories
    static const char* getCategoryColor(Category cat) {
        switch (cat) {
            case Category::SERVER:   return "\033[1;36m"; // Cyan
            case Category::HUB:      return "\033[1;32m"; // Green
            case Category::HTTP:     return "\033[1;35m"; // Magenta
            case Category::SESSION:  return "\033[1;33m"; // Yellow
            case Category::CHAT:     return "\033[1;34m"; // Blue
            case Category::CONFIG:   return "\033[1;37m"; // White
            case Category::LISTENER: return "\033[1;35m"; // Magenta
            case Category::ERROR:    return "\033[1;31m"; // Red
            default:                 return "\033[0m";    // Reset
        }
    }

    static const char* getCategoryName(Category cat) {
        switch (cat) {
            case Category::SERVER:   return "Server";
            case Category::HUB:      return "Hub";
            case Category::HTTP:     return "HTTP";
            case Category::SESSION:  return "Session";
            case Category::CHAT:     return "Chat";
            case Category::CONFIG:   return "Config";
            case Category::LISTENER: return "Listener";
            case Category::ERROR:    return "Error";
            default:                 return "Unknown";
        }
    }

    static const char* RESET_COLOR() { return "\033[0m"; }

    static std::string getTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::tm tm_now;
#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif
        
        std::ostringstream oss;
        oss << std::setfill('0') 
            << std::setw(2) << (tm_now.tm_mon + 1) << "."
            << std::setw(2) << tm_now.tm_mday << " "
            << std::setw(2) << tm_now.tm_hour << ":"
            << std::setw(2) << tm_now.tm_min << ":"
            << std::setw(2) << tm_now.tm_sec;
        
        return oss.str();
    }

#ifdef _WIN32
    static void enableWindowsColors() {
        static bool enabled = false;
        if (!enabled) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD dwMode = 0;
            GetConsoleMode(hOut, &dwMode);
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
            enabled = true;
        }
    }
#endif

public:
    static void log(Category category, const std::string& message) {
#ifdef _WIN32
        enableWindowsColors();
#endif
        std::lock_guard<std::mutex> lock(logMutex);
        
        std::cout << getTimestamp() << " "
                  << getCategoryColor(category) << "[" << getCategoryName(category) << "]" << RESET_COLOR()
                  << " " << message << std::endl;
    }

    static void logChat(const std::string& username, const std::string& message) {
#ifdef _WIN32
        enableWindowsColors();
#endif
        std::lock_guard<std::mutex> lock(logMutex);
        
        std::cout << getTimestamp() << " "
                  << getCategoryColor(Category::CHAT) << "[Chat]" << RESET_COLOR()
                  << "\t" << username << ": " << message << std::endl;
    }

    // Convenience methods for each category
    static void server(const std::string& msg) { log(Category::SERVER, msg); }
    static void hub(const std::string& msg) { log(Category::HUB, msg); }
    static void http(const std::string& msg) { log(Category::HTTP, msg); }
    static void session(const std::string& msg) { log(Category::SESSION, msg); }
    static void chat(const std::string& username, const std::string& msg) { logChat(username, msg); }
    static void config(const std::string& msg) { log(Category::CONFIG, msg); }
    static void listener(const std::string& msg) { log(Category::LISTENER, msg); }
    static void error(const std::string& msg) { log(Category::ERROR, msg); }
};

#endif // SERVER_LOGGER_HPP
