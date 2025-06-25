#include "Channel.hpp"
#include "Client.hpp"
#include <sstream>
#include <algorithm>

Channel::Channel(const std::string& name) 
    : _name(name), _topicSetTime(0), _inviteOnly(false), _topicRestricted(true), 
      _hasKey(false), _moderated(false), _noExternalMessages(true), 
      _secret(false), _private(false), _userLimit(0) {
    
    time(&_creationTime);
}

Channel::~Channel() {
    std::set<Client*> clientsCopy = _clients;
    for (std::set<Client*>::iterator it = clientsCopy.begin(); it != clientsCopy.end(); ++it) {
        (*it)->leaveChannel(this);
    }
}

void Channel::setTopic(const std::string& topic, Client* setter) {
    if (topic.length() > MAX_TOPIC_LENGTH) {
        _topic = topic.substr(0, MAX_TOPIC_LENGTH);
    } else {
        _topic = topic;
    }
    
    if (setter) {
        _topicSetBy = setter->getNickname() + "!" + setter->getUsername() + "@" + setter->getHostname();
    } else {
        _topicSetBy = "server";
    }
    
    time(&_topicSetTime);
}

void Channel::setKey(const std::string& key) {
    if (key.length() > MAX_KEY_LENGTH) {
        _key = key.substr(0, MAX_KEY_LENGTH);
    } else {
        _key = key;
    }
    _hasKey = !_key.empty();
}

void Channel::removeKey() {
    _key.clear();
    _hasKey = false;
}

void Channel::setUserLimit(int limit) {
    if (limit > 0 && limit <= MAX_USER_LIMIT) {
        _userLimit = limit;
    } else if (limit <= 0) {
        _userLimit = 0;
    } else {
        _userLimit = MAX_USER_LIMIT;
    }
}

void Channel::addClient(Client* client) {
    if (client && _clients.find(client) == _clients.end()) {
        _clients.insert(client);
        
        if (_clients.size() == 1) {
            addOperator(client);
        }
        
        removeInvited(client);
    }
}

void Channel::removeClient(Client* client) {
    if (client) {
        _clients.erase(client);
        _operators.erase(client);
        _invited.erase(client);
        
        if (_operators.empty() && !_clients.empty()) {
            std::set<Client*>::iterator it = _clients.begin();
            if (it != _clients.end()) {
                addOperator(*it);
            }
        }
    }
}

bool Channel::hasClient(Client* client) const {
    return _clients.find(client) != _clients.end();
}

void Channel::addOperator(Client* client) {
    if (client && hasClient(client)) {
        _operators.insert(client);
    }
}

void Channel::removeOperator(Client* client) {
    if (client && _operators.size() > 1) {
        _operators.erase(client);
    }
}

bool Channel::isOperator(Client* client) const {
    return _operators.find(client) != _operators.end();
}

void Channel::addInvited(Client* client) {
    if (client) {
        _invited.insert(client);
    }
}

void Channel::removeInvited(Client* client) {
    if (client) {
        _invited.erase(client);
    }
}

bool Channel::isInvited(Client* client) const {
    return _invited.find(client) != _invited.end();
}

void Channel::addBanned(Client* client) {
    if (client) {
        _banned.insert(client);
    }
}

void Channel::removeBanned(Client* client) {
    if (client) {
        _banned.erase(client);
    }
}

bool Channel::isBanned(Client* client) const {
    return _banned.find(client) != _banned.end();
}

bool Channel::canJoin(Client* client, const std::string& key) const {
    if (!client) return false;
    
    if (hasClient(client)) return false;
    
    if (isBanned(client)) return false;
    
    if (_userLimit > 0 && _clients.size() >= static_cast<size_t>(_userLimit)) {
        return false;
    }
    
    if (_inviteOnly && !isInvited(client)) {
        return false;
    }
    
    if (_hasKey && key != _key) {
        return false;
    }
    
    return true;
}

bool Channel::canSpeak(Client* client) const {
    if (!client || !hasClient(client)) {
        return false;
    }
    
    if (isBanned(client)) {
        return false;
    }
    
    if (_moderated && !isOperator(client)) {
        return false;
    }
    
    return true;
}

void Channel::broadcast(const std::string& message, Client* exclude) {
    (void)message;
    (void)exclude;
}

std::string Channel::getModeString() const {
    std::string modes = "+";
    std::string params;
    
    if (_inviteOnly) modes += "i";
    if (_topicRestricted) modes += "t";
    if (_moderated) modes += "m";
    if (_noExternalMessages) modes += "n";
    if (_secret) modes += "s";
    if (_private) modes += "p";
    
    if (_hasKey) {
        modes += "k";
        params += " " + _key;
    }
    
    if (_userLimit > 0) {
        modes += "l";
        std::ostringstream oss;
        oss << " " << _userLimit;
        params += oss.str();
    }
    
    return modes + params;
}

std::string Channel::getNamesReply() const {
    std::string names;
    
    for (std::set<Client*>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (!names.empty()) names += " ";
        
        if (isOperator(*it)) {
            names += "@";
        }
        names += (*it)->getNickname();
    }
    
    return names;
}

std::string Channel::getChannelInfo() const {
    std::ostringstream oss;
    oss << _name << " " << _clients.size();
    
    if (!_topic.empty()) {
        oss << " :" << _topic;
    } else {
        oss << " :No topic set";
    }
    
    return oss.str();
}

bool Channel::isValidChannelName(const std::string& name) const {
    if (name.empty() || name.length() > MAX_CHANNEL_NAME_LENGTH) {
        return false;
    }
    
    if (name[0] != '#' && name[0] != '&') {
        return false;
    }
    
    for (size_t i = 1; i < name.length(); i++) {
        char c = name[i];
        if (c == ' ' || c == ',' || c == 7 || c == '\r' || c == '\n') {
            return false;
        }
    }
    
    return true;
}

void Channel::cleanup() {
    std::set<Client*> clientsToRemove;
    
    for (std::set<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (!(*it)->isRegistered()) {
            clientsToRemove.insert(*it);
        }
    }
    
    for (std::set<Client*>::iterator it = clientsToRemove.begin(); it != clientsToRemove.end(); ++it) {
        removeClient(*it);
    }
    
    std::set<Client*> invitesToRemove;
    for (std::set<Client*>::iterator it = _invited.begin(); it != _invited.end(); ++it) {
        if (!(*it)->isRegistered()) {
            invitesToRemove.insert(*it);
        }
    }
    
    for (std::set<Client*>::iterator it = invitesToRemove.begin(); it != invitesToRemove.end(); ++it) {
        _invited.erase(*it);
    }
}