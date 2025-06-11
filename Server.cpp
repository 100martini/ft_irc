#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

Server* Server::instance = NULL;

Server::Server(int port, const std::string& password) 
    : _port(port), _password(password), _serverSocket(-1), _running(false) {
    _serverName = "ft_irc.42.fr";
    _serverVersion = "1.0";
    
    time_t rawtime;
    time(&rawtime);
    _creationDate = ctime(&rawtime);
    _creationDate.erase(_creationDate.length() - 1); // Remove newline
    
    instance = this;
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
}

Server::~Server() {
    stop();
}

void Server::signalHandler(int signum) {
    (void)signum;
    if (instance) {
        std::cout << "\nShutting down server..." << std::endl;
        instance->stop();
    }
}

void Server::start() {
    _setupSocket();
    _running = true;
    
    std::cout << "IRC Server started on port " << _port << std::endl;
    std::cout << "Server name: " << _serverName << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    
    while (_running) {
        int pollResult = poll(_pollFds.data(), _pollFds.size(), -1);
        
        if (pollResult == -1) {
            if (errno == EINTR) continue;
            throw std::runtime_error("poll() failed: " + std::string(strerror(errno)));
        }
        
        for (size_t i = 0; i < _pollFds.size(); i++) {
            if (_pollFds[i].revents == 0) continue;
            
            if (_pollFds[i].revents & POLLIN) {
                if (_pollFds[i].fd == _serverSocket) {
                    _acceptNewClient();
                } else {
                    _handleClientData(_pollFds[i].fd);
                }
            }
            
            if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
                _removeClient(_pollFds[i].fd);
            }
        }
    }
}

void Server::stop() {
    _running = false;
    
    // Clean up clients
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        delete it->second;
    }
    _clients.clear();
    
    // Clean up channels
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
        delete it->second;
    }
    _channels.clear();
    
    // Close server socket
    if (_serverSocket != -1) {
        close(_serverSocket);
        _serverSocket = -1;
    }
    
    _pollFds.clear();
}

void Server::_setupSocket() {
    _serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_serverSocket == -1) {
        throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
    }
    
    int opt = 1;
    if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        close(_serverSocket);
        throw std::runtime_error("Failed to set socket options: " + std::string(strerror(errno)));
    }
    
    // Set non-blocking
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
        throw std::runtime_error("Failed to bind socket: " + std::string(strerror(errno)));
    }
    
    if (listen(_serverSocket, 10) == -1) {
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
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
        }
        return;
    }
    
    // Set client socket to non-blocking
    if (fcntl(clientFd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "Failed to set client socket non-blocking: " << strerror(errno) << std::endl;
        close(clientFd);
        return;
    }
    
    // Create client object
    Client* client = new Client(clientFd, this);
    client->setHostname(inet_ntoa(clientAddr.sin_addr));
    
    _clients[clientFd] = client;
    
    struct pollfd clientPollFd;
    clientPollFd.fd = clientFd;
    clientPollFd.events = POLLIN;
    clientPollFd.revents = 0;
    _pollFds.push_back(clientPollFd);
    
    std::cout << "New client connected from " << client->getHostname() << " (fd: " << clientFd << ")" << std::endl;
}

void Server::_handleClientData(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    char buffer[1024];
    
    ssize_t bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead <= 0) {
        if (bytesRead == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) {
            _removeClient(clientFd);
        }
        return;
    }
    
    buffer[bytesRead] = '\0';
    client->appendToBuffer(std::string(buffer));
    
    std::vector<std::string> messages = client->extractMessages();
    for (size_t i = 0; i < messages.size(); i++) {
        _processMessage(client, messages[i]);
    }
}

void Server::_removeClient(int clientFd) {
    std::map<int, Client*>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) return;
    
    Client* client = it->second;
    
    // Remove from all channels
    std::set<Channel*> channels = client->getChannels();
    for (std::set<Channel*>::iterator chIt = channels.begin(); chIt != channels.end(); ++chIt) {
        Channel* channel = *chIt;
        channel->removeClient(client);
        channel->removeOperator(client);
        
        // Notify other users in channel
        if (!channel->isEmpty()) {
            std::string quitMsg = ":" + client->getPrefix() + " QUIT :Client disconnected";
            channel->broadcast(quitMsg, client);
        } else {
            // Remove empty channel
            _channels.erase(channel->getName());
            delete channel;
        }
    }
    
    // Remove from poll fds
    for (std::vector<struct pollfd>::iterator pIt = _pollFds.begin(); pIt != _pollFds.end(); ++pIt) {
        if (pIt->fd == clientFd) {
            _pollFds.erase(pIt);
            break;
        }
    }
    
    close(clientFd);
    delete client;
    _clients.erase(it);
    
    std::cout << "Client disconnected (fd: " << clientFd << ")" << std::endl;
}

void Server::_processMessage(Client* client, const std::string& message) {
    if (message.empty()) return;
    
    std::cout << "Received from " << client->getFd() << ": " << message << std::endl;
    
    _parseCommand(client, message);
}

void Server::_parseCommand(Client* client, const std::string& command) {
    std::vector<std::string> tokens = _splitMessage(command);
    if (tokens.empty()) return;
    
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    std::vector<std::string> params(tokens.begin() + 1, tokens.end());
    
    if (cmd == "PASS") {
        _handlePass(client, params);
    } else if (cmd == "NICK") {
        _handleNick(client, params);
    } else if (cmd == "USER") {
        _handleUser(client, params);
    } else if (cmd == "JOIN") {
        _handleJoin(client, params);
    } else if (cmd == "PART") {
        _handlePart(client, params);
    } else if (cmd == "PRIVMSG") {
        _handlePrivmsg(client, params);
    } else if (cmd == "QUIT") {
        _handleQuit(client, params);
    } else if (cmd == "PING") {
        _handlePing(client, params);
    } else if (cmd == "KICK") {
        _handleKick(client, params);
    } else if (cmd == "INVITE") {
        _handleInvite(client, params);
    } else if (cmd == "TOPIC") {
        _handleTopic(client, params);
    } else if (cmd == "MODE") {
        _handleMode(client, params);
    }
}

std::vector<std::string> Server::_splitMessage(const std::string& message) {
    std::vector<std::string> tokens;
    std::istringstream iss(message);
    std::string token;
    
    while (iss >> token) {
        if (token[0] == ':' && !tokens.empty()) {
            // Rest of the line is one parameter
            std::string rest;
            std::getline(iss, rest);
            token += rest;
            token = token.substr(1); // Remove leading ':'
        }
        tokens.push_back(token);
        if (token.find(':') == 0 && token.length() > 1) break;
    }
    
    return tokens;
}

void Server::_sendToClient(int clientFd, const std::string& message) {
    std::string fullMessage = message + "\r\n";
    send(clientFd, fullMessage.c_str(), fullMessage.length(), 0);
    std::cout << "Sent to " << clientFd << ": " << message << std::endl;
}

void Server::_sendNumericReply(Client* client, int code, const std::string& message) {
    std::ostringstream oss;
    oss << ":" << _serverName << " ";
    
    if (code < 100) oss << "0";
    if (code < 10) oss << "0";
    oss << code << " ";
    
    if (client->isRegistered()) {
        oss << client->getNickname();
    } else {
        oss << "*";
    }
    
    oss << " " << message;
    _sendToClient(client->getFd(), oss.str());
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

Channel* Server::_getOrCreateChannel(const std::string& channelName) {
    Channel* channel = getChannel(channelName);
    if (!channel) {
        channel = new Channel(channelName);
        _channels[channelName] = channel;
    }
    return channel;
}