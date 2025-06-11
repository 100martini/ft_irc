#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <ctime>

// Network includes
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

// Forward declarations
class Client;
class Channel;

class Server {
private:
    // Server configuration
    int _port;
    std::string _password;
    int _serverSocket;
    bool _running;
    
    // Client management
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    
    // Server info
    std::string _serverName;
    std::string _serverVersion;
    std::string _creationDate;
    
    // Private methods
    void _setupSocket();
    void _acceptNewClient();
    void _handleClientData(int clientFd);
    void _removeClient(int clientFd);
    void _processMessage(Client* client, const std::string& message);
    void _parseCommand(Client* client, const std::string& command);
    
    // Command handlers
    void _handlePass(Client* client, const std::vector<std::string>& params);
    void _handleNick(Client* client, const std::vector<std::string>& params);
    void _handleUser(Client* client, const std::vector<std::string>& params);
    void _handleJoin(Client* client, const std::vector<std::string>& params);
    void _handlePart(Client* client, const std::vector<std::string>& params);
    void _handlePrivmsg(Client* client, const std::vector<std::string>& params);
    void _handleQuit(Client* client, const std::vector<std::string>& params);
    void _handlePing(Client* client, const std::vector<std::string>& params);
    void _handleKick(Client* client, const std::vector<std::string>& params);
    void _handleInvite(Client* client, const std::vector<std::string>& params);
    void _handleTopic(Client* client, const std::vector<std::string>& params);
    void _handleMode(Client* client, const std::vector<std::string>& params);
    
    // Utility methods
    std::vector<std::string> _splitMessage(const std::string& message);
    void _sendToClient(int clientFd, const std::string& message);
    void _sendToChannel(Channel* channel, const std::string& message, Client* exclude = NULL);
    bool _isValidNickname(const std::string& nickname);
    Channel* _getOrCreateChannel(const std::string& channelName);
    
    // Error responses
    void _sendNumericReply(Client* client, int code, const std::string& message);
    void _sendWelcomeSequence(Client* client);
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();
    void stop();
    
    // Getters
    const std::string& getPassword() const { return _password; }
    const std::string& getServerName() const { return _serverName; }
    Client* getClientByNick(const std::string& nickname);
    Channel* getChannel(const std::string& channelName);
    
    // Static signal handler
    static Server* instance;
    static void signalHandler(int signum);
};

// IRC numeric reply codes
#define RPL_WELCOME 001
#define RPL_YOURHOST 002
#define RPL_CREATED 003
#define RPL_MYINFO 004
#define RPL_TOPIC 332
#define RPL_NAMREPLY 353
#define RPL_ENDOFNAMES 366
#define RPL_MOTD 372
#define RPL_MOTDSTART 375
#define RPL_ENDOFMOTD 376

#define ERR_NOSUCHNICK 401
#define ERR_NOSUCHCHANNEL 403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_TOOMANYCHANNELS 405
#define ERR_NONICKNAMEGIVEN 431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE 433
#define ERR_NOTONCHANNEL 442
#define ERR_USERONCHANNEL 443
#define ERR_NEEDMOREPARAMS 461
#define ERR_ALREADYREGISTRED 462
#define ERR_PASSWDMISMATCH 464
#define ERR_CHANNELISFULL 471
#define ERR_INVITEONLYCHAN 473
#define ERR_BADCHANNELKEY 475
#define ERR_CHANOPRIVSNEEDED 482

#endif