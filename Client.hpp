#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <set>

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
    
    std::set<Channel*> _channels;
    Server* _server;
    
public:
    Client(int fd, Server* server);
    ~Client();
    
    // Getters
    int getFd() const { return _fd; }
    const std::string& getNickname() const { return _nickname; }
    const std::string& getUsername() const { return _username; }
    const std::string& getRealname() const { return _realname; }
    const std::string& getHostname() const { return _hostname; }
    const std::string& getBuffer() const { return _buffer; }
    bool isAuthenticated() const { return _authenticated; }
    bool isRegistered() const { return _registered; }
    bool hasPasswordProvided() const { return _passwordProvided; }
    const std::set<Channel*>& getChannels() const { return _channels; }
    
    // Setters
    void setNickname(const std::string& nickname) { _nickname = nickname; }
    void setUsername(const std::string& username) { _username = username; }
    void setRealname(const std::string& realname) { _realname = realname; }
    void setHostname(const std::string& hostname) { _hostname = hostname; }
    void setAuthenticated(bool auth) { _authenticated = auth; }
    void setPasswordProvided(bool provided) { _passwordProvided = provided; }
    
    // Buffer management
    void appendToBuffer(const std::string& data);
    std::vector<std::string> extractMessages();
    void clearBuffer() { _buffer.clear(); }
    
    // Channel management
    void joinChannel(Channel* channel);
    void leaveChannel(Channel* channel);
    bool isInChannel(Channel* channel) const;
    
    // Registration
    void tryRegister();
    
    // Utility
    std::string getPrefix() const;
    std::string getFullIdentifier() const;
};

#endif