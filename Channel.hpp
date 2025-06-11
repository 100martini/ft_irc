#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>

class Client;

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::string _key;
    
    std::set<Client*> _clients;
    std::set<Client*> _operators;
    std::set<Client*> _invited;
    
    // Channel modes
    bool _inviteOnly;
    bool _topicRestricted;
    bool _hasKey;
    int _userLimit;
    
public:
    Channel(const std::string& name);
    ~Channel();
    
    // Getters
    const std::string& getName() const { return _name; }
    const std::string& getTopic() const { return _topic; }
    const std::string& getKey() const { return _key; }
    const std::set<Client*>& getClients() const { return _clients; }
    const std::set<Client*>& getOperators() const { return _operators; }
    bool isInviteOnly() const { return _inviteOnly; }
    bool isTopicRestricted() const { return _topicRestricted; }
    bool hasKey() const { return _hasKey; }
    int getUserLimit() const { return _userLimit; }
    size_t getClientCount() const { return _clients.size(); }
    
    // Setters
    void setTopic(const std::string& topic) { _topic = topic; }
    void setKey(const std::string& key);
    void removeKey();
    void setInviteOnly(bool inviteOnly) { _inviteOnly = inviteOnly; }
    void setTopicRestricted(bool restricted) { _topicRestricted = restricted; }
    void setUserLimit(int limit) { _userLimit = limit; }
    void removeUserLimit() { _userLimit = 0; }
    
    // Client management
    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;
    
    // Operator management
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    
    // Invite management
    void addInvited(Client* client);
    void removeInvited(Client* client);
    bool isInvited(Client* client) const;
    
    // Channel operations
    bool canJoin(Client* client, const std::string& key = "") const;
    void broadcast(const std::string& message, Client* exclude = NULL);
    std::string getModeString() const;
    std::string getNamesReply() const;
    
    // Utility
    bool isEmpty() const { return _clients.empty(); }
};

#endif