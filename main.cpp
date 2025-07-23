#include "Server.hpp"
#include <iostream>
#include <cstdlib>
#include <limits>
#include <new>

void printBanner() {
    std::cout << BOLD << CYAN << std::endl;
    std::cout << "╔══════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                                                  ║" << std::endl;
    std::cout << "║         " << WHITE << "███╗   ███╗███████╗███╗   ██╗" << CYAN << "            ║" << std::endl;
    std::cout << "║         " << WHITE << "████╗ ████║██╔════╝████╗  ██║" << CYAN << "            ║" << std::endl;
    std::cout << "║         " << WHITE << "██╔████╔██║███████╗██╔██╗ ██║" << CYAN << "            ║" << std::endl;
    std::cout << "║         " << WHITE << "██║╚██╔╝██║╚════██║██║╚██╗██║" << CYAN << "            ║" << std::endl;
    std::cout << "║         " << WHITE << "██║ ╚═╝ ██║███████║██║ ╚████║" << CYAN << "            ║" << std::endl;
    std::cout << "║         " << WHITE << "╚═╝     ╚═╝╚══════╝╚═╝  ╚═══╝" << CYAN << "            ║" << std::endl;
    std::cout << "║                                                  ║" << std::endl;
    std::cout << "║          " << YELLOW << "Chat Server made by Martini" << CYAN << "             ║" << std::endl;
    std::cout << "║             " << WHITE << "A 1337 School Project" << CYAN << "                ║" << std::endl;
    std::cout << "║                                                  ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════╝" << RESET << std::endl;
    std::cout << std::endl;
}

void printUsage(const std::string& programName) {
    std::cout << BOLD << "Usage:" << RESET << std::endl;
    std::cout << "  " << CYAN << programName << " <port> <password>" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << BOLD << "Parameters:" << RESET << std::endl;
    std::cout << "  " << YELLOW << "port" << RESET << "     : The port number (1-65535) on which the IRC server will listen" << std::endl;
    std::cout << "  " << YELLOW << "password" << RESET << " : The connection password required by IRC clients" << std::endl;
    std::cout << std::endl;
    std::cout << BOLD << "Examples:" << RESET << std::endl;
    std::cout << "  " << CYAN << programName << " 6667 mypassword" << RESET << std::endl;
    std::cout << "  " << CYAN << programName << " 8080 \"secret password\"" << RESET << std::endl;
    std::cout << std::endl;
    std::cout << BOLD << "Notes:" << RESET << std::endl;
    std::cout << "  • Standard IRC port is 6667" << std::endl;
    std::cout << "  • Use quotes if password contains spaces" << std::endl;
    std::cout << "  • Server supports standard IRC commands" << std::endl;
    std::cout << "  • Press Ctrl+C to stop the server gracefully" << std::endl;
}

bool isValidPort(const std::string& portStr) {
    if (portStr.empty()) return false;
    
    for (size_t i = 0; i < portStr.length(); i++) {
        if (!isdigit(portStr[i])) return false;
    }
    
    long port = strtol(portStr.c_str(), NULL, 10);
    return port > 0 && port <= 65535;
}

bool isValidPassword(const std::string& password) {
    if (password.empty() || password.length() > 255) {
        return false;
    }
    
    for (size_t i = 0; i < password.length(); i++) {
        char c = password[i];
        if (c < 32 && c != 9) {
            return false;
        }
    }
    
    return true;
}

void printServerInfo() {
    std::cout << BOLD << "\nServer Features:" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Multi-client support with non-blocking I/O" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Channel management with operators" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Private messaging support" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Standard IRC commands (JOIN, PART, KICK, etc.)" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Channel modes (invite-only, topic restriction, etc.)" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "User authentication and registration" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Server statistics and information commands" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Graceful shutdown handling" << RESET << std::endl;
    std::cout << "  • " << MAGENTA << "Memory-safe implementation" << RESET << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    printBanner();
    
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage(argv[0]);
        printServerInfo();
        return 0;
    }
    
    if (argc != 3) {
        std::cout << RED << "Error: Invalid number of arguments! Chouf chwya lte7t wakha? <3" << RESET << std::endl << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    std::string portStr = argv[1];
    std::string password = argv[2];
    
    if (!isValidPort(portStr)) {
        std::cout << RED << "Error: Invalid port number." << RESET << std::endl;
        std::cout << "Port must be a number between 1 and 65535." << std::endl;
        std::cout << "Common IRC ports: 6667, 6668, 6669, 8080" << std::endl;
        return 1;
    }
    
    if (!isValidPassword(password)) {
        std::cout << RED << "Error: Invalid password." << RESET << std::endl;
        if (password.empty()) {
            std::cout << "Password cannot be empty." << std::endl;
        } else if (password.length() > 255) {
            std::cout << "Password too long (maximum 255 characters)." << std::endl;
        } else {
            std::cout << "Password contains invalid characters." << std::endl;
        }
        return 1;
    }
    
    int port = atoi(portStr.c_str());
    
    if (port < 1024) {
        std::cout << YELLOW << "Warning: Using privileged port " << port 
                  << ". You may need root privileges." << RESET << std::endl;
    }
    
    std::cout << BLUE << "Initializing server with:" << RESET << std::endl;
    std::cout << "  Port: " << BOLD << port << RESET << std::endl;
    std::cout << "  Password: " << BOLD << std::string(password.length(), '*') << RESET << std::endl;
    std::cout << std::endl;
    
    try {
        Server* server = NULL;
        
        try {
            server = new Server(port, password);
        } catch (const std::bad_alloc& e) {
            std::cout << RED << BOLD << "Fatal Error: " << RESET << RED 
                      << "Failed to allocate memory for server" << RESET << std::endl;
            return 1;
        }
        
        std::cout << GREEN << "Server initialized successfully!" << RESET << std::endl;
        std::cout << "Ready to accept connections..." << std::endl;
        std::cout << std::endl;
        
        server->start();
        
        delete server;
        
    } catch (const std::exception& e) {
        std::cout << std::endl << RED << BOLD << "Server Error: " << RESET << RED 
                  << e.what() << RESET << std::endl;
        
        std::cout << std::endl << YELLOW << "Troubleshooting tips:" << RESET << std::endl;
        std::cout << "  • Check if port " << port << " is already in use" << std::endl;
        std::cout << "  • Ensure you have permission to bind to port " << port << std::endl;
        std::cout << "  • Try a different port number" << std::endl;
        std::cout << "  • Check firewall settings" << std::endl;
        
        return 1;
    }
    
    return 0;
}