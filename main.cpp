#include "Server.hpp"
#include <iostream>
#include <cstdlib>

void printUsage(const std::string& programName) {
    std::cout << "Usage: " << programName << " <port> <password>" << std::endl;
    std::cout << "  port:     The port number on which the IRC server will listen" << std::endl;
    std::cout << "  password: The connection password required by IRC clients" << std::endl;
}

bool isValidPort(const std::string& portStr) {
    if (portStr.empty()) return false;
    
    for (size_t i = 0; i < portStr.length(); i++) {
        if (!isdigit(portStr[i])) return false;
    }
    
    int port = atoi(portStr.c_str());
    return port > 0 && port <= 65535;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string portStr = argv[1];
    std::string password = argv[2];
    
    if (!isValidPort(portStr)) {
        std::cerr << "Error: Invalid port number. Port must be between 1 and 65535." << std::endl;
        return 1;
    }
    
    if (password.empty()) {
        std::cerr << "Error: Password cannot be empty." << std::endl;
        return 1;
    }
    
    int port = atoi(portStr.c_str());
    
    try {
        Server server(port, password);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}