#include "Client.hpp"
#include "Channel.hpp"
#include "Server.hpp"
#include <sstream>
#include <algorithm>

Client::Client(int fd, Server* server) 
    : _fd(fd), _authenticated(false), _registered(false), 
      _passwordProvided(false), _operator(false), _server(server),
      _messageCount(0) {
    
    _hostname = "localhost";
    time(&_connectTime);
    _lastActivity = _connectTime;
    _lastMessageTime = _connectTime;
}

Client::~Client() {
    std::set<Channel*> channelsCopy = _channels;
    for (std::set<Channel*>::iterator it = channelsCopy.begin(); it != channelsCopy.end(); ++it) {
        leaveChannel(*it);
    }
}

void Client::setNickname(const std::string& nickname) {
    if (isValidNickname(nickname)) {
        _nickname = nickname;
        updateActivity();
    }
}

void Client::setUsername(const std::string& username) {
    if (isValidUsername(username)) {
        _username = username;
        updateActivity();
    }
}

void Client::setRealname(const std::string& realname) {
    if (!realname.empty() && realname.length() <= 255) {
        _realname = realname;
        updateActivity();
    }
}

void Client::setHostname(const std::string& hostname) {
    if (!hostname.empty()) {
        _hostname = hostname;
    }
}

void Client::appendToBuffer(const std::string& data) {
    if (_buffer.length() + data.length() > MAX_BUFFER_SIZE) {
        _buffer.clear();
        return;
    }
    
    _buffer += data;
    updateActivity();
}

std::vector<std::string> Client::extractMessages() {
    std::vector<std::string> messages;
    size_t pos = 0;
    
    while ((pos = _buffer.find('\n')) != std::string::npos) {
        std::string message = _buffer.substr(0, pos);
        
        if (!message.empty() && message[message.length() - 1] == '\r') {
            message = message.substr(0, message.length() - 1);
        }
        
        if (!message.empty() && message.length() <= MAX_MESSAGE_LENGTH) {
            messages.push_back(message);
            incrementMessageCount();
        }
        
        _buffer = _buffer.substr(pos + 1);
    }
    
    if (_buffer.length() > MAX_MESSAGE_LENGTH) {
        _buffer.clear();
    }
    
    return messages;
}

void Client::joinChannel(Channel* channel) {
    if (channel && _channels.find(channel) == _channels.end() && canJoinMoreChannels()) {
        _channels.insert(channel);
        channel->addClient(this);
        updateActivity();
    }
}

void Client::leaveChannel(Channel* channel) {
    if (channel && _channels.find(channel) != _channels.end()) {
        _channels.erase(channel);
        channel->removeClient(this);
        channel->removeOperator(this);
        updateActivity();
    }
}

bool Client::isInChannel(Channel* channel) const {
    return _channels.find(channel) != _channels.end();
}

void Client::tryRegister() {
    if (_passwordProvided && !_nickname.empty() && !_username.empty() && !_registered) {
        _registered = true;
        _authenticated = true;
        updateActivity();
    }
}

void Client::updateActivity() {
    time(&_lastActivity);
}

void Client::incrementMessageCount() {
    _messageCount++;
    time(&_lastMessageTime);
    updateActivity();
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

std::string Client::getMask() const {
    return "*!" + _username + "@" + _hostname;
}

int Client::getIdleTime() const {
    time_t now;
    time(&now);
    return static_cast<int>(difftime(now, _lastActivity));
}

bool Client::isValidNickname(const std::string& nickname) const {
    if (nickname.empty() || nickname.length() > 9) {
        return false;
    }
    
    char first = nickname[0];
    if (!isalpha(first) && first != '_' && first != '[' && first != ']' && 
        first != '{' && first != '}' && first != '\\' && first != '|') {
        return false;
    }
    
    for (size_t i = 1; i < nickname.length(); i++) {
        char c = nickname[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '[' && c != ']' && 
            c != '{' && c != '}' && c != '\\' && c != '|') {
            return false;
        }
    }
    
    const std::string forbidden[] = {
        "root", "admin", "operator", "op", "oper", "server", "service",
        "chanserv", "nickserv", "memoserv", "operserv", "hostserv",
        "anonymous", "guest", "null", "nobody", "bot"
    };
    
    std::string lowerNick = nickname;
    std::transform(lowerNick.begin(), lowerNick.end(), lowerNick.begin(), ::tolower);
    
    for (size_t i = 0; i < sizeof(forbidden) / sizeof(forbidden[0]); i++) {
        if (lowerNick == forbidden[i]) {
            return false;
        }
    }
    
    return true;
}

bool Client::isValidUsername(const std::string& username) const {
    if (username.empty() || username.length() > 10) {
        return false;
    }
    
    for (size_t i = 0; i < username.length(); i++) {
        char c = username[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
            return false;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}