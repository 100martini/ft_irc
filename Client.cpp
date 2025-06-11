#include "Client.hpp"
#include "Channel.hpp"
#include "Server.hpp"

Client::Client(int fd, Server* server) 
    : _fd(fd), _server(server), _authenticated(false), _registered(false), _passwordProvided(false) {
    _hostname = "localhost";
}

Client::~Client() {
    // Leave all channels before destruction
    std::set<Channel*> channelsCopy = _channels;
    for (std::set<Channel*>::iterator it = channelsCopy.begin(); it != channelsCopy.end(); ++it) {
        leaveChannel(*it);
    }
}

void Client::appendToBuffer(const std::string& data) {
    _buffer += data;
}

std::vector<std::string> Client::extractMessages() {
    std::vector<std::string> messages;
    size_t pos = 0;
    
    while ((pos = _buffer.find('\n')) != std::string::npos) {
        std::string message = _buffer.substr(0, pos);
        
        // Remove carriage return if present
        if (!message.empty() && message[message.length() - 1] == '\r') {
            message = message.substr(0, message.length() - 1);
        }
        
        if (!message.empty()) {
            messages.push_back(message);
        }
        
        _buffer = _buffer.substr(pos + 1);
    }
    
    return messages;
}

void Client::joinChannel(Channel* channel) {
    if (channel && _channels.find(channel) == _channels.end()) {
        _channels.insert(channel);
        channel->addClient(this);
    }
}

void Client::leaveChannel(Channel* channel) {
    if (channel && _channels.find(channel) != _channels.end()) {
        _channels.erase(channel);
        channel->removeClient(this);
        channel->removeOperator(this);
    }
}

bool Client::isInChannel(Channel* channel) const {
    return _channels.find(channel) != _channels.end();
}

void Client::tryRegister() {
    if (_passwordProvided && !_nickname.empty() && !_username.empty() && !_registered) {
        _registered = true;
        _authenticated = true;
        
        // Send welcome sequence
        std::ostringstream oss;
        oss << "Welcome to the " << _server->getServerName() << " Network, " << getFullIdentifier();
        
        // This would be handled by the server's _sendWelcomeSequence method
        // We'll implement this in the server command handlers
    }
}

std::string Client::getPrefix() const {
    if (_nickname.empty()) {
        return _hostname;
    }
    
    std::string prefix = _nickname;
    if (!_username.empty()) {
        prefix += "!" + _username;
    }
    if (!_hostname.empty()) {
        prefix += "@" + _hostname;
    }
    
    return prefix;
}

std::string Client::getFullIdentifier() const {
    if (_nickname.empty()) {
        return "*";
    }
    
    std::string identifier = _nickname;
    if (!_username.empty() && !_hostname.empty()) {
        identifier += "!" + _username + "@" + _hostname;
    }
    
    return identifier;
}