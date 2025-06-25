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
#include <iomanip>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>

class Client;
class Channel;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define BOLD    "\033[1m"

class Server {
private:
    int _port;
    std::string _password;
    int _serverSocket;
    bool _running;
    
    std::vector<struct pollfd> _pollFds;
    std::map<int, Client*> _clients;
    std::map<std::string, Channel*> _channels;
    
    std::string _serverName;
    std::string _serverVersion;
    std::string _creationDate;
    std::string _motd;
    size_t _maxClients;
    
    size_t _totalConnections;
    size_t _currentConnections;
    time_t _startTime;
    
    void _setupSocket();
    void _acceptNewClient();
    void _handleClientData(int clientFd);
    void _removeClient(int clientFd);
    void _processMessage(Client* client, const std::string& message);
    void _parseCommand(Client* client, const std::string& command);
    
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
    void _handleWho(Client* client, const std::vector<std::string>& params);
    void _handleWhois(Client* client, const std::vector<std::string>& params);
    void _handleList(Client* client, const std::vector<std::string>& params);
    void _handleNames(Client* client, const std::vector<std::string>& params);
    void _handleMotd(Client* client, const std::vector<std::string>& params);
    void _handleAdmin(Client* client, const std::vector<std::string>& params);
    void _handleTime(Client* client, const std::vector<std::string>& params);
    void _handleVersion(Client* client, const std::vector<std::string>& params);
    void _handleInfo(Client* client, const std::vector<std::string>& params);
    void _handleStats(Client* client, const std::vector<std::string>& params);
    
    std::vector<std::string> _splitMessage(const std::string& message);
    void _sendToClient(int clientFd, const std::string& message);
    void _sendToChannel(Channel* channel, const std::string& message, Client* exclude = NULL);
    bool _isValidNickname(const std::string& nickname);
    bool _isValidChannelName(const std::string& channelName);
    bool _isChannelOperator(Client* client, Channel* channel);
    Channel* _getOrCreateChannel(const std::string& channelName);
    std::string _formatTime(time_t timestamp);
    std::string _getUptime();
    void _logMessage(const std::string& level, const std::string& message);
    void _validateClientInput(Client* client, const std::string& input);
    bool _rateLimitCheck(Client* client);
    
    void _sendNumericReply(Client* client, int code, const std::string& message);
    void _sendWelcomeSequence(Client* client);
    void _sendMotd(Client* client);
    void _sendChannelModes(Client* client, Channel* channel);
    void _sendWhoReply(Client* client, Channel* channel, Client* target);
    void _sendWhoisReply(Client* client, Client* target);
    void _sendListReply(Client* client, Channel* channel);
    void _sendStatsReply(Client* client);
    
    void _cleanupEmptyChannels();
    bool _isClientFlooding(Client* client);
    void _disconnectClient(int clientFd, const std::string& reason);
    
public:
    Server(int port, const std::string& password);
    ~Server();
    
    void start();
    void stop();
    void shutdown();
    
    const std::string& getPassword() const { return _password; }
    const std::string& getServerName() const { return _serverName; }
    const std::string& getServerVersion() const { return _serverVersion; }
    const std::string& getMotd() const { return _motd; }
    size_t getMaxClients() const { return _maxClients; }
    size_t getTotalConnections() const { return _totalConnections; }
    size_t getCurrentConnections() const { return _currentConnections; }
    time_t getStartTime() const { return _startTime; }
    
    Client* getClientByNick(const std::string& nickname);
    Channel* getChannel(const std::string& channelName);
    std::vector<Channel*> getChannelList();
    std::vector<Client*> getClientList();
    
    void setMotd(const std::string& motd) { _motd = motd; }
    void setMaxClients(size_t maxClients) { _maxClients = maxClients; }
    
    bool isRunning() const { return _running; }
    bool isValidPassword(const std::string& password) const;
    
    static Server* instance;
    static void signalHandler(int signum);
};

#define RPL_WELCOME 001
#define RPL_YOURHOST 002
#define RPL_CREATED 003
#define RPL_MYINFO 004
#define RPL_BOUNCE 005
#define RPL_USERHOST 302
#define RPL_ISON 303
#define RPL_AWAY 301
#define RPL_UNAWAY 305
#define RPL_NOWAWAY 306
#define RPL_WHOISUSER 311
#define RPL_WHOISSERVER 312
#define RPL_WHOISOPERATOR 313
#define RPL_WHOISIDLE 317
#define RPL_ENDOFWHOIS 318
#define RPL_WHOISCHANNELS 319
#define RPL_WHOWASUSER 314
#define RPL_ENDOFWHOWAS 369
#define RPL_LISTSTART 321
#define RPL_LIST 322
#define RPL_LISTEND 323
#define RPL_CHANNELMODEIS 324
#define RPL_UNIQOPIS 325
#define RPL_NOTOPIC 331
#define RPL_TOPIC 332
#define RPL_INVITING 341
#define RPL_SUMMONING 342
#define RPL_INVITELIST 346
#define RPL_ENDOFINVITELIST 347
#define RPL_EXCEPTLIST 348
#define RPL_ENDOFEXCEPTLIST 349
#define RPL_VERSION 351
#define RPL_WHOREPLY 352
#define RPL_ENDOFWHO 315
#define RPL_NAMREPLY 353
#define RPL_ENDOFNAMES 366
#define RPL_LINKS 364
#define RPL_ENDOFLINKS 365
#define RPL_BANLIST 367
#define RPL_ENDOFBANLIST 368
#define RPL_INFO 371
#define RPL_ENDOFINFO 374
#define RPL_MOTDSTART 375
#define RPL_MOTD 372
#define RPL_ENDOFMOTD 376
#define RPL_YOUREOPER 381
#define RPL_REHASHING 382
#define RPL_YOURESERVICE 383
#define RPL_MYPORTIS 384
#define RPL_TIME 391
#define RPL_USERSSTART 392
#define RPL_USERS 393
#define RPL_ENDOFUSERS 394
#define RPL_NOUSERS 395

#define ERR_NOSUCHNICK 401
#define ERR_NOSUCHSERVER 402
#define ERR_NOSUCHCHANNEL 403
#define ERR_CANNOTSENDTOCHAN 404
#define ERR_TOOMANYCHANNELS 405
#define ERR_WASNOSUCHNICK 406
#define ERR_TOOMANYTARGETS 407
#define ERR_NOSUCHSERVICE 408
#define ERR_NOORIGIN 409
#define ERR_NORECIPIENT 411
#define ERR_NOTEXTTOSEND 412
#define ERR_NOTOPLEVEL 413
#define ERR_WILDTOPLEVEL 414
#define ERR_BADMASK 415
#define ERR_UNKNOWNCOMMAND 421
#define ERR_NOMOTD 422
#define ERR_NOADMININFO 423
#define ERR_FILEERROR 424
#define ERR_NONICKNAMEGIVEN 431
#define ERR_ERRONEUSNICKNAME 432
#define ERR_NICKNAMEINUSE 433
#define ERR_NICKCOLLISION 436
#define ERR_UNAVAILRESOURCE 437
#define ERR_USERNOTINCHANNEL 441
#define ERR_NOTONCHANNEL 442
#define ERR_USERONCHANNEL 443
#define ERR_NOLOGIN 444
#define ERR_SUMMONDISABLED 445
#define ERR_USERSDISABLED 446
#define ERR_NOTREGISTERED 451
#define ERR_NEEDMOREPARAMS 461
#define ERR_ALREADYREGISTRED 462
#define ERR_NOPERMFORHOST 463
#define ERR_PASSWDMISMATCH 464
#define ERR_YOUREBANNEDCREEP 465
#define ERR_YOUWILLBEBANNED 466
#define ERR_KEYSET 467
#define ERR_CHANNELISFULL 471
#define ERR_UNKNOWNMODE 472
#define ERR_INVITEONLYCHAN 473
#define ERR_BANNEDFROMCHAN 474
#define ERR_BADCHANNELKEY 475
#define ERR_BADCHANMASK 476
#define ERR_NOCHANMODES 477
#define ERR_BANLISTFULL 478
#define ERR_NOPRIVILEGES 481
#define ERR_CHANOPRIVSNEEDED 482
#define ERR_CANTKILLSERVER 483
#define ERR_RESTRICTED 484
#define ERR_UNIQOPPRIVSNEEDED 485
#define ERR_NOOPERHOST 491
#define ERR_NOSERVICEHOST 492
#define ERR_UMODEUNKNOWNFLAG 501
#define ERR_USERSDONTMATCH 502

#endif