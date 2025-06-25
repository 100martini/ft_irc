#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

Server* Server::instance = NULL;

std::string intToString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string sizeToString(size_t value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

Server::Server(int port, const std::string& password) 
    : _port(port), _password(password), _serverSocket(-1), _running(false),
      _maxClients(100), _totalConnections(0), _currentConnections(0) {
    
    _serverName = "msn.chat.1337";
    _serverVersion = "msn-1.0.1337";
    _motd = "Welcome to ft_irc - A 1337 Project Implementation\n"
            "This server supports standard IRC protocol features.\n"
            "For help, contact your system administrator.\n"
            "O chati m3a rassk!";
    
    time(&_startTime);
    time_t rawtime;
    time(&rawtime);
    _creationDate = ctime(&rawtime);
    if (!_creationDate.empty() && _creationDate[_creationDate.length() - 1] == '\n') {
        _creationDate.erase(_creationDate.length() - 1);
    }
    
    instance = this;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);
    
    _logMessage("INFO", "IRC Server initialized");
}

Server::~Server() {
    shutdown();
}

void Server::signalHandler(int signum) {
    (void)signum;
    if (instance) {
        std::cout << "\n" << YELLOW << "Signal received. Initiating graceful shutdown..." << RESET << std::endl;
        instance->shutdown();
    }
}

void Server::start() {
    try {
        _setupSocket();
        _running = true;
        
        std::cout << BOLD << GREEN << "╔══════════════════════════════════╗" << std::endl;
        std::cout << "║          IRC SERVER STARTED      ║" << std::endl;
        std::cout << "╠══════════════════════════════════╣" << std::endl;
        std::cout << "║ " << CYAN << "Server Name: " << RESET << std::setw(19) << std::left << _serverName << GREEN << " ║" << std::endl;
        std::cout << "║ " << CYAN << "Version:     " << RESET << std::setw(19) << std::left << _serverVersion << GREEN << " ║" << std::endl;
        std::cout << "║ " << CYAN << "Port:        " << RESET << std::setw(19) << std::left << _port << GREEN << " ║" << std::endl;
        std::cout << "║ " << CYAN << "Max Clients: " << RESET << std::setw(19) << std::left << _maxClients << GREEN << " ║" << std::endl;
        std::cout << "║ " << CYAN << "Started:     " << RESET << std::setw(19) << std::left << _formatTime(_startTime) << GREEN << " ║" << std::endl;
        std::cout << "╚══════════════════════════════════╝" << RESET << std::endl;
        
        _logMessage("INFO", "Server listening on port " + intToString(_port));
        
        while (_running) {
            int pollResult = poll(_pollFds.data(), _pollFds.size(), 1000);
            
            if (pollResult == -1) {
                if (errno == EINTR) {
                    continue;
                }
                _logMessage("ERROR", "poll() failed: " + std::string(strerror(errno)));
                break;
            }
            
            if (pollResult == 0) {
                _cleanupEmptyChannels();
                continue;
            }
            
            for (size_t i = 0; i < _pollFds.size() && _running; i++) {
                if (_pollFds[i].revents == 0) continue;
                
                if (_pollFds[i].revents & POLLIN) {
                    if (_pollFds[i].fd == _serverSocket) {
                        _acceptNewClient();
                    } else {
                        _handleClientData(_pollFds[i].fd);
                    }
                }
                
                if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                    _logMessage("WARNING", "Client connection error on fd " + intToString(_pollFds[i].fd));
                    _disconnectClient(_pollFds[i].fd, "Connection error");
                    break;
                }
            }
        }
    } catch (const std::exception& e) {
        _logMessage("FATAL", "Server error: " + std::string(e.what()));
        throw;
    }
}

void Server::stop() {
    _running = false;
    _logMessage("INFO", "Server stop requested");
}

void Server::shutdown() {
    if (!_running && _serverSocket == -1) return;
    
    _running = false;
    
    std::cout << YELLOW << "Shutting down server gracefully..." << RESET << std::endl;
    
    std::map<int, Client*> clientsCopy = _clients;
    for (std::map<int, Client*>::iterator it = clientsCopy.begin(); it != clientsCopy.end(); ++it) {
        _sendToClient(it->first, "ERROR :Server shutting down");
        delete it->second;
    }
    _clients.clear();
    
    std::map<std::string, Channel*> channelsCopy = _channels;
    for (std::map<std::string, Channel*>::iterator it = channelsCopy.begin(); it != channelsCopy.end(); ++it) {
        delete it->second;
    }
    _channels.clear();
    
    if (_serverSocket != -1) {
        close(_serverSocket);
        _serverSocket = -1;
    }
    
    _pollFds.clear();
    
    std::cout << GREEN << "Server shutdown complete. Goodbye!" << RESET << std::endl;
    _logMessage("INFO", "Server shutdown completed successfully");
}

void Server::_setupSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket == -1) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }
    
    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(_serverSocket);
        throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }
    
    if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) == -1) {
        close(_serverSocket);
        throw std::runtime_error("Failed to set non-blocking: " + std::string(strerror(errno)));
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(_port);
    
    if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        close(_serverSocket);
        throw std::runtime_error("Failed to bind to port " + intToString(_port) + ": " + std::string(strerror(errno)));
    }
    
    if (listen(_serverSocket, 128) == -1) {
        close(_serverSocket);
        throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
    }
    
    struct pollfd serverPollFd;
    serverPollFd.fd = _serverSocket;
    serverPollFd.events = POLLIN;
    serverPollFd.revents = 0;
    _pollFds.push_back(serverPollFd);
}

void Server::_acceptNewClient() {
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    
    int clientFd = accept(_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientFd == -1) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            _logMessage("WARNING", "Failed to accept connection: " + std::string(strerror(errno)));
        }
        return;
    }
    
    if (_currentConnections >= _maxClients) {
        std::string errorMsg = "ERROR :Server is full (max " + sizeToString(_maxClients) + " clients)";
        send(clientFd, errorMsg.c_str(), errorMsg.length(), 0);
        close(clientFd);
        _logMessage("INFO", "Connection rejected - server full");
        return;
    }
    
    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        _logMessage("ERROR", "Failed to set client socket non-blocking: " + std::string(strerror(errno)));
        close(clientFd);
        return;
    }
    
    int keepAlive = 1;
    if (setsockopt(clientFd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) == -1) {
        _logMessage("WARNING", "Failed to set keepalive on client socket");
    }
    
    Client* client = new Client(clientFd, this);
    std::string hostname = inet_ntoa(clientAddr.sin_addr);
    client->setHostname(hostname);
    
    _clients[clientFd] = client;
    _totalConnections++;
    _currentConnections++;
    
    struct pollfd clientPollFd;
    clientPollFd.fd = clientFd;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;
    _pollFds.push_back(clientPollFd);
    
    std::cout << GREEN << "[" << _formatTime(time(NULL)) << "] " 
              << CYAN << "New connection from " << hostname 
              << " (fd: " << clientFd << ") - Total: " << _currentConnections 
              << "/" << _maxClients << RESET << std::endl;
    
    _logMessage("INFO", "Client connected from " + hostname + " (fd: " + intToString(clientFd) + ")");
}

void Server::_handleClientData(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    char buffer[4096];
    
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0) {
            _disconnectClient(clientFd, "Client disconnected");
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            _disconnectClient(clientFd, "Read error: " + std::string(strerror(errno)));
        }
        return;
    }
    
    buffer[bytesRead] = '\0';
    std::string receivedData(buffer);
    
    _validateClientInput(client, receivedData);
    
    if (_isClientFlooding(client)) {
        _disconnectClient(clientFd, "Excess flood");
        return;
    }
    
    client->appendToBuffer(receivedData);
    
    std::vector<std::string> messages = client->extractMessages();
    for (size_t i = 0; i < messages.size(); i++) {
        if (!messages[i].empty()) {
            _processMessage(client, messages[i]);
        }
    }
}

void Server::_removeClient(int clientFd) {
    _disconnectClient(clientFd, "Connection closed");
}

void Server::_disconnectClient(int clientFd, const std::string& reason) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    std::string nickname = client->getNickname().empty() ? "*" : client->getNickname();
    
    std::set<Channel*> channels = client->getChannels();
    for (std::set<Channel*>::iterator chIt = channels.begin(); chIt != channels.end(); ++chIt) {
        Channel* channel = *chIt;
        if (!channel->isEmpty()) {
            std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + reason;
            _sendToChannel(channel, quitMsg, client);
        }
        channel->removeClient(client);
        channel->removeOperator(client);
    }
    
    for (std::vector<struct pollfd>::iterator pIt = _pollFds.begin(); pIt != _pollFds.end(); ++pIt) {
        if (pIt->fd == clientFd) {
            _pollFds.erase(pIt);
            break;
        }
    }
    
    close(clientFd);
    delete client;
    _clients.erase(it);
    _currentConnections--;
    
    std::cout << RED << "[" << _formatTime(time(NULL)) << "] " 
              << "Client " << nickname << " disconnected: " << reason 
              << " (fd: " << clientFd << ") - Total: " << _currentConnections 
              << "/" << _maxClients << RESET << std::endl;
    
    _logMessage("INFO", "Client " + nickname + " disconnected: " + reason);
    _cleanupEmptyChannels();
}

void Server::_processMessage(Client* client, const std::string& message) {
    if (message.empty() || message.length() > 512) {
        return;
    }
    
    if (client->isRegistered()) {
        std::cout << BLUE << "[" << _formatTime(time(NULL)) << "] " 
                  << client->getNickname() << ": " << message << RESET << std::endl;
    }
    
    _parseCommand(client, message);
}

std::vector<std::string> Server::_splitMessage(const std::string& message) {
    std::vector<std::string> tokens;
    std::istringstream iss(message);
    std::string token;
    bool foundColon = false;
    
    while (iss >> token && !foundColon) {
        if (token[0] == ':' && !tokens.empty()) {
            std::string rest;
            std::getline(iss, rest);
            token += rest;
            token = token.substr(1);
            foundColon = true;
        }
        tokens.push_back(token);
    }
    
    return tokens;
}

void Server::_sendToClient(int clientFd, const std::string& message) {
    if (message.empty()) return;
    
    std::string fullMessage = message + "\r\n";
    ssize_t sent = send(clientFd, fullMessage.c_str(), fullMessage.length(), MSG_NOSIGNAL);
    
    if (sent == -1) {
        if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EPIPE) {
            _logMessage("WARNING", "Send failed to fd " + intToString(clientFd) + ": " + strerror(errno));
        }
    }
}

void Server::_sendNumericReply(Client* client, int code, const std::string& message) {
    std::ostringstream oss;
    oss << ":" << _serverName << " ";
    
    if (code < 100) oss << "0";
    if (code < 10) oss << "0";
    oss << code << " ";
    
    if (client->isRegistered() && !client->getNickname().empty()) {
        oss << client->getNickname();
    } else {
        oss << "*";
    }
    
    oss << " " << message;
    _sendToClient(client->getFd(), oss.str());
}

bool Server::_isValidNickname(const std::string& nickname) {
    if (nickname.empty() || nickname.length() > 9) {
        return false;
    }
    
    if (!isalpha(nickname[0]) && nickname[0] != '_' && nickname[0] != '[' && 
        nickname[0] != ']' && nickname[0] != '{' && nickname[0] != '}' && 
        nickname[0] != '\\' && nickname[0] != '|') {
        return false;
    }
    
    for (size_t i = 1; i < nickname.length(); i++) {
        char c = nickname[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '[' && c != ']' && 
            c != '{' && c != '}' && c != '\\' && c != '|') {
            return false;
        }
    }
    
    return true;
}

bool Server::_isValidChannelName(const std::string& channelName) {
    if (channelName.empty() || channelName.length() > 50) {
        return false;
    }
    
    if (channelName[0] != '#' && channelName[0] != '&') {
        return false;
    }
    
    for (size_t i = 1; i < channelName.length(); i++) {
        char c = channelName[i];
        if (c == ' ' || c == ',' || c == 7) {
            return false;
        }
    }
    
    return true;
}

Channel* Server::_getOrCreateChannel(const std::string& channelName) {
    Channel* channel = getChannel(channelName);
    if (!channel) {
        channel = new Channel(channelName);
        _channels[channelName] = channel;
        _logMessage("INFO", "Channel created: " + channelName);
    }
    return channel;
}

Client* Server::getClientByNick(const std::string& nickname) {
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (it->second->getNickname() == nickname) {
            return it->second;
        }
    }
    return NULL;
}

Channel* Server::getChannel(const std::string& channelName) {
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    return (it != _channels.end()) ? it->second : NULL;
}

std::vector<Channel*> Server::getChannelList() {
    std::vector<Channel*> channels;
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        channels.push_back(it->second);
    }
    return channels;
}

std::vector<Client*> Server::getClientList() {
    std::vector<Client*> clients;
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        clients.push_back(it->second);
    }
    return clients;
}

bool Server::isValidPassword(const std::string& password) const {
    return password == _password;
}

std::string Server::_formatTime(time_t timestamp) {
    struct tm* timeinfo = localtime(&timestamp);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
    return std::string(buffer);
}

std::string Server::_getUptime() {
    time_t now;
    time(&now);
    int uptime = static_cast<int>(difftime(now, _startTime));
    
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0) oss << hours << "h ";
    if (minutes > 0) oss << minutes << "m ";
    oss << seconds << "s";
    
    return oss.str();
}

void Server::_logMessage(const std::string& level, const std::string& message) {
    std::string color = WHITE;
    if (level == "ERROR" || level == "FATAL") color = RED;
    else if (level == "WARNING") color = YELLOW;
    else if (level == "INFO") color = GREEN;
    
    std::cout << color << "[" << _formatTime(time(NULL)) << "] [" << level << "] " 
              << message << RESET << std::endl;
}

void Server::_validateClientInput(Client* client, const std::string& input) {
    if (input.length() > 512) {
        _disconnectClient(client->getFd(), "Input line too long");
        return;
    }
    
    for (size_t i = 0; i < input.length(); i++) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 32 && c != 9 && c != 10 && c != 13) {
            _logMessage("WARNING", "Invalid character in input from " + client->getNickname());
            break;
        }
    }
}

bool Server::_rateLimitCheck(Client* client) {
    (void)client;
    return true;
}

bool Server::_isClientFlooding(Client* client) {
    if (client->getBuffer().length() > 8192) {
        return true;
    }
    return false;
}

void Server::_cleanupEmptyChannels() {
    std::vector<std::string> emptyChannels;
    
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        if (it->second->isEmpty()) {
            emptyChannels.push_back(it->first);
        }
    }
    
    for (size_t i = 0; i < emptyChannels.size(); i++) {
        std::map<std::string, Channel*>::iterator it = _channels.find(emptyChannels[i]);
        if (it != _channels.end()) {
            delete it->second;
            _channels.erase(it);
            _logMessage("INFO", "Empty channel removed: " + emptyChannels[i]);
        }
    }
}

void Server::_sendToChannel(Channel* channel, const std::string& message, Client* exclude) {
    if (!channel) return;
    
    const std::set<Client*>& clients = channel->getClients();
    for (std::set<Client*>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
        if (*it != exclude) {
            _sendToClient((*it)->getFd(), message);
        }
    }
}

void Server::_sendWelcomeSequence(Client* client) {
    std::string nick = client->getNickname();
    std::string user = client->getUsername();
    std::string host = client->getHostname();
    
    _sendNumericReply(client, RPL_WELCOME, ":Welcome to the " + _serverName + " Network " + nick + "!" + user + "@" + host);
    _sendNumericReply(client, RPL_YOURHOST, ":Your host is " + _serverName + ", running version " + _serverVersion);
    _sendNumericReply(client, RPL_CREATED, ":This server was created " + _creationDate);
    _sendNumericReply(client, RPL_MYINFO, _serverName + " " + _serverVersion + " o itkol");
    
    _sendMotd(client);
    
    std::cout << GREEN << "[" << _formatTime(time(NULL)) << "] " 
              << "User " << nick << " registered successfully" << RESET << std::endl;
}

void Server::_sendMotd(Client* client) {
    if (_motd.empty()) {
        _sendNumericReply(client, ERR_NOMOTD, ":MOTD File is missing");
        return;
    }
    
    _sendNumericReply(client, RPL_MOTDSTART, ":- " + _serverName + " Message of the day -");
    
    std::istringstream iss(_motd);
    std::string line;
    while (std::getline(iss, line)) {
        _sendNumericReply(client, RPL_MOTD, ":- " + line);
    }
    
    _sendNumericReply(client, RPL_ENDOFMOTD, ":End of /MOTD command");
}