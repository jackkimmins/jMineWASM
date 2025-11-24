// server/server_config.hpp
// Server configuration file handler
#ifndef SERVER_CONFIG_HPP
#define SERVER_CONFIG_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include "logger.hpp"

struct ServerConfig {
    unsigned short port = 8080;
    std::string motd = "Welcome to jMineWASM Server!";
    
    // Load configuration from file
    bool loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            Logger::config("Config file not found, creating default: " + filename);
            return createDefaultConfig(filename);
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;
            
            // Parse key=value pairs
            size_t equalsPos = line.find('=');
            if (equalsPos == std::string::npos) continue;
            
            std::string key = trim(line.substr(0, equalsPos));
            std::string value = trim(line.substr(equalsPos + 1));
            
            if (key == "port") {
                try {
                    port = static_cast<unsigned short>(std::stoi(value));
                } catch (...) {
                    Logger::error("Invalid port value: " + value);
                }
            } else if (key == "motd") {
                motd = value;
            }
        }
        
        file.close();
        Logger::config("Configuration loaded from " + filename);
        return true;
    }
    
    // Create default configuration file
    bool createDefaultConfig(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            Logger::error("Failed to create config file: " + filename);
            return false;
        }
        
        file << "# jMineWASM Server Configuration\n";
        file << "# Edit these settings to customize your server\n\n";
        file << "# Server port (default: 8080)\n";
        file << "port=" << port << "\n\n";
        file << "# Message of the day - displayed when players connect\n";
        file << "motd=" << motd << "\n";
        
        file.close();
        Logger::config("Default configuration created: " + filename);
        return true;
    }
    
private:
    // Helper function to trim whitespace
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
};

#endif // SERVER_CONFIG_HPP
