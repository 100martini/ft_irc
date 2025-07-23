#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <set>
#include <map>
#include <ctime>

class Client;
class Server;

class Channel {
private:
    std::string _name;
    std::string _topic;
    std::string _topicSetBy;
    time_t _topicSetTime;
    std::string _key;
    
    std::set<Client*> _clients;
    std::set<Client*> _operators;
    std::set<Client*> _invited;
    std::set<Client*> _banned;
    
    bool _inviteOnly;
    bool _topicRestricted;
    bool _hasKey;
    bool _moderated;
    bool _noExternalMessages;
    bool _secret;
    bool _private;
    int _userLimit;
    
    time_t _creationTime;
    Server* _server;
    
    static const size_t MAX_TOPIC_LENGTH = 307;
    static const size_t MAX_KEY_LENGTH = 23;
    static const size_t MAX_CHANNEL_NAME_LENGTH = 50;
    static const int MAX_USER_LIMIT = 999;
    
public:
    Channel(const std::string& name);
    ~Channel();
    
    const std::string& getName() const { return _name; }
    const std::string& getTopic() const { return _topic; }
    const std::string& getTopicSetBy() const { return _topicSetBy; }
    time_t getTopicSetTime() const { return _topicSetTime; }
    const std::string& getKey() const { return _key; }
    const std::set<Client*>& getClients() const { return _clients; }
    const std::set<Client*>& getOperators() const { return _operators; }
    const std::set<Client*>& getInvited() const { return _invited; }
    const std::set<Client*>& getBanned() const { return _banned; }
    
    bool isInviteOnly() const { return _inviteOnly; }
    bool isTopicRestricted() const { return _topicRestricted; }
    bool hasKey() const { return _hasKey; }
    bool isModerated() const { return _moderated; }
    bool isNoExternalMessages() const { return _noExternalMessages; }
    bool isSecret() const { return _secret; }
    bool isPrivate() const { return _private; }
    int getUserLimit() const { return _userLimit; }
    size_t getClientCount() const { return _clients.size(); }
    time_t getCreationTime() const { return _creationTime; }
    
    void setTopic(const std::string& topic, Client* setter = NULL);
    void setKey(const std::string& key);
    void removeKey();
    void setInviteOnly(bool inviteOnly) { _inviteOnly = inviteOnly; }
    void setTopicRestricted(bool restricted) { _topicRestricted = restricted; }
    void setModerated(bool moderated) { _moderated = moderated; }
    void setNoExternalMessages(bool noExternal) { _noExternalMessages = noExternal; }
    void setSecret(bool secret) { _secret = secret; }
    void setPrivate(bool priv) { _private = priv; }
    void setUserLimit(int limit);
    void removeUserLimit() { _userLimit = 0; }
    void setServer(Server* server) { _server = server; }
    
    void addClient(Client* client);
    void removeClient(Client* client);
    bool hasClient(Client* client) const;
    
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;
    size_t getOperatorCount() const { return _operators.size(); }
    
    void addInvited(Client* client);
    void removeInvited(Client* client);
    bool isInvited(Client* client) const;
    void clearInvites() { _invited.clear(); }
    
    void addBanned(Client* client);
    void removeBanned(Client* client);
    bool isBanned(Client* client) const;
    void clearBans() { _banned.clear(); }
    
    bool canJoin(Client* client, const std::string& key = "") const;
    bool canSpeak(Client* client) const;
    void broadcast(const std::string& message, Client* exclude = NULL);
    
    std::string getModeString() const;
    std::string getNamesReply() const;
    std::string getChannelInfo() const;
    
    bool isEmpty() const { return _clients.empty(); }
    bool isValidChannelName(const std::string& name) const;
    
    void cleanup();
};

#endif