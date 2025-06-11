#include "Channel.hpp"
#include "Client.hpp"
#include <sstream>

Channel::Channel(const std::string& name) 
    : _name(name), _inviteOnly(false), _topicRestricted(true), _hasKey(false), _userLimit(0) {
}

Channel::~Channel() {
    // Notify all clients they're leaving the channel
    std::set<Client*> clientsCopy = _clients;
    for (std::set<Client*>::iterator it = clientsCopy.begin(); it != clientsCopy.end(); ++it) {
        (*it)->leaveChannel(this);
    }
}

void Channel::setKey(const std::string& key) {
    _key = key;
    _hasKey = !key.empty();
}

void Channel::removeKey() {
    _key.clear();
    _hasKey = false;
}

void Channel::addClient(Client* client) {
    if (client && _clients.find(client) == _clients.end()) {
        _clients.insert(client);
        
        // First client becomes operator
        if (_clients.size() == 1) {
            addOperator(client);
        }
    }
}

void Channel::removeClient(Client* client) {
    if (client) {
        _clients.erase(client);
        _operators.erase(client);
        _invited.erase(client);
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
    if (client) {
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

bool Channel::canJoin(Client* client, const std::string& key) const {
    if (!client) return false;
    
    // Check if already in channel
    if (hasClient(client)) return false;
    
    // Check user limit
    if (_userLimit > 0 && _clients.size() >= static_cast<size_t>(_userLimit)) {
        return false;
    }
    
    // Check invite-only mode
    if (_inviteOnly && !isInvited(client)) {
        return false;
    }
    
    // Check channel key
    if (_hasKey && key != _key) {
        return false;
    }
    
    return true;
}

void Channel::broadcast(const std::string& message, Client* exclude) {
    for (std::set<Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        if (*it != exclude) {
            // This would normally send to the client, but we need server access
            // The server will handle this through _sendToClient
        }
    }
}

std::string Channel::getModeString() const {
    std::string modes = "+";
    std::string params;
    
    if (_inviteOnly) modes += "i";
    if (_topicRestricted) modes += "t";
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