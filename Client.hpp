#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <set>
#include <ctime>

class Channel;
class Server;

class Client {
private:
    int _fd;
    std::string _nickname;
    std::string _username;
    std::string _realname;
    std::string _hostname;
    std::string _buffer;
    
    bool _authenticated;
    bool _registered;
    bool _passwordProvided;
    bool _operator;
    
    std::set<Channel*> _channels;
    Server* _server;
    
    time_t _connectTime;
    time_t _lastActivity;
    size_t _messageCount;
    time_t _lastMessageTime;
    
    static const size_t MAX_BUFFER_SIZE = 8192;
    static const size_t MAX_MESSAGE_LENGTH = 512;
    static const size_t MAX_CHANNELS = 20;
    
public:
    Client(int fd, Server* server);
    ~Client();
    
    int getFd() const { return _fd; }
    const std::string& getNickname() const { return _nickname; }
    const std::string& getUsername() const { return _username; }
    const std::string& getRealname() const { return _realname; }
    const std::string& getHostname() const { return _hostname; }
    const std::string& getBuffer() const { return _buffer; }
    bool isAuthenticated() const { return _authenticated; }
    bool isRegistered() const { return _registered; }
    bool hasPasswordProvided() const { return _passwordProvided; }
    bool isOperator() const { return _operator; }
    const std::set<Channel*>& getChannels() const { return _channels; }
    time_t getConnectTime() const { return _connectTime; }
    time_t getLastActivity() const { return _lastActivity; }
    size_t getMessageCount() const { return _messageCount; }
    
    void setNickname(const std::string& nickname);
    void setUsername(const std::string& username);
    void setRealname(const std::string& realname);
    void setHostname(const std::string& hostname);
    void setAuthenticated(bool auth) { _authenticated = auth; }
    void setPasswordProvided(bool provided) { _passwordProvided = provided; }
    void setOperator(bool op) { _operator = op; }
    
    void appendToBuffer(const std::string& data);
    std::vector<std::string> extractMessages();
    void clearBuffer() { _buffer.clear(); }
    bool isBufferFull() const { return _buffer.length() >= MAX_BUFFER_SIZE; }
    
    void joinChannel(Channel* channel);
    void leaveChannel(Channel* channel);
    bool isInChannel(Channel* channel) const;
    bool canJoinMoreChannels() const { return _channels.size() < MAX_CHANNELS; }
    
    void tryRegister();
    void updateActivity();
    void incrementMessageCount();
    
    std::string getPrefix() const;
    std::string getFullIdentifier() const;
    std::string getMask() const;
    int getIdleTime() const;
    
    bool isValidNickname(const std::string& nickname) const;
    bool isValidUsername(const std::string& username) const;
};

#endif