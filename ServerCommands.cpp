#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

extern std::string intToString(int value);
extern std::string sizeToString(size_t value);

void Server::_parseCommand(Client* client, const std::string& command) {
    std::vector<std::string> tokens = _splitMessage(command);
    if (tokens.empty()) return;
    
    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
    
    std::vector<std::string> params(tokens.begin() + 1, tokens.end());
    
    if (cmd == "CAP") {
        if (!params.empty() && params[0] == "LS") {
            _sendToClient(client->getFd(), "CAP * LS :");
        }
        return;
    }
    
    if (cmd == "PASS") {
        _handlePass(client, params);
    } else if (cmd == "NICK") {
        _handleNick(client, params);
    } else if (cmd == "USER") {
        _handleUser(client, params);
    } else if (cmd == "JOIN") {
        _handleJoin(client, params);
    } else if (cmd == "PART") {
        _handlePart(client, params);
    } else if (cmd == "PRIVMSG") {
        _handlePrivmsg(client, params);
    } else if (cmd == "QUIT") {
        _handleQuit(client, params);
    } else if (cmd == "PING") {
        _handlePing(client, params);
    } else if (cmd == "PONG") {
        return;
    } else if (cmd == "KICK") {
        _handleKick(client, params);
    } else if (cmd == "INVITE") {
        _handleInvite(client, params);
    } else if (cmd == "TOPIC") {
        _handleTopic(client, params);
    } else if (cmd == "MODE") {
        _handleMode(client, params);
    } else if (cmd == "WHO") {
        _handleWho(client, params);
    } else if (cmd == "WHOIS") {
        _handleWhois(client, params);
    } else if (cmd == "LIST") {
        _handleList(client, params);
    } else if (cmd == "NAMES") {
        _handleNames(client, params);
    } else if (cmd == "MOTD") {
        _handleMotd(client, params);
    } else if (cmd == "ADMIN") {
        _handleAdmin(client, params);
    } else if (cmd == "TIME") {
        _handleTime(client, params);
    } else if (cmd == "VERSION") {
        _handleVersion(client, params);
    } else if (cmd == "INFO") {
        _handleInfo(client, params);
    } else if (cmd == "STATS") {
        _handleStats(client, params);
    } else if (client->isRegistered()) {
        _sendNumericReply(client, ERR_UNKNOWNCOMMAND, cmd + " :Unknown command");
    }
}

void Server::_handlePass(Client* client, const std::vector<std::string>& params) {
    if (client->isRegistered()) {
        _sendNumericReply(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }
    
    if (isValidPassword(params[0])) {
        client->setPasswordProvided(true);
        client->tryRegister();
        if (client->isRegistered()) {
            _sendWelcomeSequence(client);
        }
    } else {
        _sendNumericReply(client, ERR_PASSWDMISMATCH, ":Password incorrect");
        _logMessage("WARNING", "Invalid password attempt from " + client->getHostname());
    }
}

void Server::_handleNick(Client* client, const std::vector<std::string>& params) {
    if (!client->hasPasswordProvided() && !_password.empty()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":Password required");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }
    
    std::string newNick = params[0];
    
    if (!_isValidNickname(newNick)) {
        _sendNumericReply(client, ERR_ERRONEUSNICKNAME, newNick + " :Erroneous nickname");
        return;
    }
    
    Client* existingClient = getClientByNick(newNick);
    if (existingClient && existingClient != client) {
        _sendNumericReply(client, ERR_NICKNAMEINUSE, newNick + " :Nickname is already in use");
        return;
    }
    
    std::string oldNick = client->getNickname();
    client->setNickname(newNick);
    
    if (client->isRegistered()) {
        std::string nickMsg = ":" + oldNick + "!" + client->getUsername() + "@" + client->getHostname() + " NICK :" + newNick;
        
        std::set<Channel*> channels = client->getChannels();
        std::set<Client*> notifiedClients;
        
        for (std::set<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            const std::set<Client*>& channelClients = (*it)->getClients();
            for (std::set<Client*>::const_iterator cIt = channelClients.begin(); cIt != channelClients.end(); ++cIt) {
                if (notifiedClients.find(*cIt) == notifiedClients.end()) {
                    _sendToClient((*cIt)->getFd(), nickMsg);
                    notifiedClients.insert(*cIt);
                }
            }
        }
        
        _logMessage("INFO", "Nick change: " + oldNick + " -> " + newNick);
    } else {
        client->tryRegister();
        if (client->isRegistered()) {
            _sendWelcomeSequence(client);
        }
    }
}

void Server::_handleUser(Client* client, const std::vector<std::string>& params) {
    if (!client->hasPasswordProvided() && !_password.empty()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":Password required");
        return;
    }
    
    if (client->isRegistered()) {
        _sendNumericReply(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    
    if (params.size() < 4) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "USER :Not enough parameters");
        return;
    }
    
    client->setUsername(params[0]);
    client->setRealname(params[3]);
    
    client->tryRegister();
    if (client->isRegistered()) {
        _sendWelcomeSequence(client);
    }
}

void Server::_handleJoin(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }
    
    std::string channelList = params[0];
    std::string keyList = (params.size() > 1) ? params[1] : "";
    
    std::vector<std::string> channels;
    std::vector<std::string> keys;
    
    std::istringstream channelStream(channelList);
    std::string channel;
    while (std::getline(channelStream, channel, ',')) {
        if (!channel.empty()) {
            channels.push_back(channel);
        }
    }
    
    if (!keyList.empty()) {
        std::istringstream keyStream(keyList);
        std::string key;
        while (std::getline(keyStream, key, ',')) {
            keys.push_back(key);
        }
    }
    
    for (size_t i = 0; i < channels.size(); i++) {
        std::string channelName = channels[i];
        std::string key = (i < keys.size()) ? keys[i] : "";
        
        if (channelName[0] != '#' && channelName[0] != '&') {
            channelName = "#" + channelName;
        }
        
        if (!_isValidChannelName(channelName)) {
            _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
            continue;
        }
        
        if (client->getChannels().size() >= 20) {
            _sendNumericReply(client, ERR_TOOMANYCHANNELS, channelName + " :You have joined too many channels");
            break;
        }
        
        Channel* ch = _getOrCreateChannel(channelName);
        if (!ch) {
            _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :Channel creation failed");
            continue;
        }
        
        if (ch->hasClient(client)) {
            continue;
        }
        
        if (!ch->canJoin(client, key)) {
            if (ch->getUserLimit() > 0 && ch->getClientCount() >= static_cast<size_t>(ch->getUserLimit())) {
                _sendNumericReply(client, ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
            } else if (ch->isInviteOnly() && !ch->isInvited(client)) {
                _sendNumericReply(client, ERR_INVITEONLYCHAN, channelName + " :Cannot join channel (+i)");
            } else if (ch->hasKey() && key != ch->getKey()) {
                _sendNumericReply(client, ERR_BADCHANNELKEY, channelName + " :Cannot join channel (+k)");
            } else {
                _sendNumericReply(client, ERR_CANNOTSENDTOCHAN, channelName + " :Cannot join channel");
            }
            continue;
        }
        
        client->joinChannel(ch);
        ch->removeInvited(client);
        
        std::string joinMsg = ":" + client->getPrefix() + " JOIN :" + channelName;
        _sendToChannel(ch, joinMsg);
        
        if (!ch->getTopic().empty()) {
            _sendNumericReply(client, RPL_TOPIC, channelName + " :" + ch->getTopic());
        } else {
            _sendNumericReply(client, RPL_NOTOPIC, channelName + " :No topic is set");
        }
        
        _sendNumericReply(client, RPL_NAMREPLY, "= " + channelName + " :" + ch->getNamesReply());
        _sendNumericReply(client, RPL_ENDOFNAMES, channelName + " :End of /NAMES list");
        
        _logMessage("INFO", client->getNickname() + " joined " + channelName);
    }
}

void Server::_handlePart(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }
    
    std::string channelList = params[0];
    std::string reason = (params.size() > 1) ? params[1] : "Leaving";
    
    std::istringstream channelStream(channelList);
    std::string channelName;
    
    while (std::getline(channelStream, channelName, ',')) {
        if (channelName.empty()) continue;
        
        Channel* channel = getChannel(channelName);
        if (!channel || !channel->hasClient(client)) {
            _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
            continue;
        }
        
        std::string partMsg = ":" + client->getPrefix() + " PART " + channelName + " :" + reason;
        _sendToChannel(channel, partMsg);
        
        client->leaveChannel(channel);
        _logMessage("INFO", client->getNickname() + " left " + channelName + " (" + reason + ")");
    }
}

void Server::_handlePrivmsg(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NORECIPIENT, ":No recipient given (PRIVMSG)");
        return;
    }
    
    if (params.size() < 2) {
        _sendNumericReply(client, ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }
    
    std::string targets = params[0];
    std::string message = params[1];
    
    if (message.empty()) {
        _sendNumericReply(client, ERR_NOTEXTTOSEND, ":No text to send");
        return;
    }
    
    std::istringstream targetStream(targets);
    std::string target;
    
    while (std::getline(targetStream, target, ',')) {
        if (target.empty()) continue;
        
        if (target[0] == '#' || target[0] == '&') {
            Channel* channel = getChannel(target);
            if (!channel) {
                _sendNumericReply(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
                continue;
            }
            
            if (!channel->hasClient(client)) {
                _sendNumericReply(client, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");
                continue;
            }
            
            if (!channel->canSpeak(client)) {
                _sendNumericReply(client, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");
                continue;
            }
            
            std::string privmsgMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
            _sendToChannel(channel, privmsgMsg, client);
        } else {
            Client* targetClient = getClientByNick(target);
            if (!targetClient) {
                _sendNumericReply(client, ERR_NOSUCHNICK, target + " :No such nick/channel");
                continue;
            }
            
            std::string privmsgMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
            _sendToClient(targetClient->getFd(), privmsgMsg);
        }
    }
}

void Server::_handleQuit(Client* client, const std::vector<std::string>& params) {
    std::string reason = params.empty() ? "Client quit" : params[0];
    
    if (reason.length() > 255) {
        reason = reason.substr(0, 255);
    }
    
    _disconnectClient(client->getFd(), reason);
}

void Server::_handlePing(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        _sendNumericReply(client, ERR_NOORIGIN, ":No origin specified");
        return;
    }
    
    std::string pongMsg = ":" + _serverName + " PONG " + _serverName + " :" + params[0];
    _sendToClient(client->getFd(), pongMsg);
}

void Server::_handleKick(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.size() < 2) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string targetNicks = params[1];
    std::string reason = (params.size() > 2) ? params[2] : client->getNickname();
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasClient(client)) {
        _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }
    
    if (!channel->isOperator(client)) {
        _sendNumericReply(client, ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }
    
    std::istringstream nickStream(targetNicks);
    std::string targetNick;
    
    while (std::getline(nickStream, targetNick, ',')) {
        if (targetNick.empty()) continue;
        
        Client* targetClient = getClientByNick(targetNick);
        if (!targetClient) {
            _sendNumericReply(client, ERR_NOSUCHNICK, targetNick + " :No such nick");
            continue;
        }
        
        if (!channel->hasClient(targetClient)) {
            _sendNumericReply(client, ERR_USERNOTINCHANNEL, targetNick + " " + channelName + " :They aren't on that channel");
            continue;
        }
        
        std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason;
        _sendToChannel(channel, kickMsg);
        
        targetClient->leaveChannel(channel);
        _logMessage("INFO", client->getNickname() + " kicked " + targetNick + " from " + channelName + " (" + reason + ")");
    }
}

void Server::_handleInvite(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.size() < 2) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "INVITE :Not enough parameters");
        return;
    }
    
    std::string targetNick = params[0];
    std::string channelName = params[1];
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasClient(client)) {
        _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }
    
    if (channel->isInviteOnly() && !channel->isOperator(client)) {
        _sendNumericReply(client, ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNick(targetNick);
    if (!targetClient) {
        _sendNumericReply(client, ERR_NOSUCHNICK, targetNick + " :No such nick");
        return;
    }
    
    if (channel->hasClient(targetClient)) {
        _sendNumericReply(client, ERR_USERONCHANNEL, targetNick + " " + channelName + " :is already on channel");
        return;
    }
    
    channel->addInvited(targetClient);
    
    _sendNumericReply(client, RPL_INVITING, targetNick + " " + channelName);
    
    std::string inviteMsg = ":" + client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
    _sendToClient(targetClient->getFd(), inviteMsg);
    
    _logMessage("INFO", client->getNickname() + " invited " + targetNick + " to " + channelName);
}

void Server::_handleTopic(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }
    
    if (!channel->hasClient(client)) {
        _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }
    
    if (params.size() == 1) {
        if (channel->getTopic().empty()) {
            _sendNumericReply(client, RPL_NOTOPIC, channelName + " :No topic is set");
        } else {
            _sendNumericReply(client, RPL_TOPIC, channelName + " :" + channel->getTopic());
        }
    } else {
        if (channel->isTopicRestricted() && !channel->isOperator(client)) {
            _sendNumericReply(client, ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
            return;
        }
        
        std::string newTopic = params[1];
        if (newTopic.length() > 307) {
            newTopic = newTopic.substr(0, 307);
        }
        
        channel->setTopic(newTopic, client);
        
        std::string topicMsg = ":" + client->getPrefix() + " TOPIC " + channelName + " :" + newTopic;
        _sendToChannel(channel, topicMsg);
        
        _logMessage("INFO", client->getNickname() + " changed topic in " + channelName + " to: " + newTopic);
    }
}

void Server::_handleMode(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "MODE :Not enough parameters");
        return;
    }
    
    std::string target = params[0];
    
    if (target[0] == '#' || target[0] == '&') {
        Channel* channel = getChannel(target);
        if (!channel) {
            _sendNumericReply(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
            return;
        }
        
        if (!channel->hasClient(client)) {
            _sendNumericReply(client, ERR_NOTONCHANNEL, target + " :You're not on that channel");
            return;
        }
        
        if (params.size() == 1) {
            _sendNumericReply(client, RPL_CHANNELMODEIS, target + " " + channel->getModeString());
            return;
        }
        
        if (!channel->isOperator(client)) {
            _sendNumericReply(client, ERR_CHANOPRIVSNEEDED, target + " :You're not channel operator");
            return;
        }
        
        std::string modes = params[1];
        bool adding = true;
        size_t paramIndex = 2;
        std::string appliedModes;
        std::string appliedParams;
        
        for (size_t i = 0; i < modes.length(); i++) {
            char mode = modes[i];
            
            if (mode == '+') {
                adding = true;
                if (appliedModes.empty() || appliedModes[appliedModes.length()-1] != '+') {
                    appliedModes += '+';
                }
            } else if (mode == '-') {
                adding = false;
                if (appliedModes.empty() || appliedModes[appliedModes.length()-1] != '-') {
                    appliedModes += '-';
                }
            } else if (mode == 'i') {
                channel->setInviteOnly(adding);
                appliedModes += 'i';
            } else if (mode == 't') {
                channel->setTopicRestricted(adding);
                appliedModes += 't';
            } else if (mode == 'k') {
                if (adding) {
                    if (paramIndex < params.size()) {
                        std::string key = params[paramIndex++];
                        if (key.find(' ') == std::string::npos && 
                            key.find(',') == std::string::npos &&
                            key.find(7) == std::string::npos) {
                            channel->setKey(key);
                            appliedModes += 'k';
                            appliedParams += " " + key;
                        }
                    }
                } else {
                    if (channel->hasKey()) {
                        channel->removeKey();
                        appliedModes += 'k';
                    }
                }
            } else if (mode == 'l') {
                if (adding) {
                    if (paramIndex < params.size()) {
                        int limit = atoi(params[paramIndex++].c_str());
                        if (limit > 0 && limit <= 999) {
                            channel->setUserLimit(limit);
                            appliedModes += 'l';
                            appliedParams += " " + intToString(limit);
                        }
                    }
                } else {
                    channel->removeUserLimit();
                    appliedModes += 'l';
                }
            } else if (mode == 'o') {
                if (paramIndex < params.size()) {
                    std::string targetNick = params[paramIndex++];
                    Client* targetClient = getClientByNick(targetNick);
                    if (targetClient && channel->hasClient(targetClient)) {
                        if (adding) {
                            channel->addOperator(targetClient);
                        } else {
                            channel->removeOperator(targetClient);
                        }
                        appliedModes += 'o';
                        appliedParams += " " + targetNick;
                    }
                }
            } else {
                _sendNumericReply(client, ERR_UNKNOWNMODE, std::string(1, mode) + " :is unknown mode char to me");
            }
        }
        
        if (!appliedModes.empty() && appliedModes != "+" && appliedModes != "-") {
            std::string modeMsg = ":" + client->getPrefix() + " MODE " + target + " " + appliedModes + appliedParams;
            _sendToChannel(channel, modeMsg);
            _logMessage("INFO", client->getNickname() + " set mode " + appliedModes + " on " + target);
        }
    } else {
        _sendNumericReply(client, ERR_USERSDONTMATCH, ":Cannot change mode for other users");
    }
}

void Server::_handleWho(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "WHO :Not enough parameters");
        return;
    }
    
    std::string mask = params[0];
    
    if (mask[0] == '#' || mask[0] == '&') {
        Channel* channel = getChannel(mask);
        if (!channel) {
            _sendNumericReply(client, ERR_NOSUCHCHANNEL, mask + " :No such channel");
            return;
        }
        
        if (!channel->hasClient(client)) {
            _sendNumericReply(client, ERR_NOTONCHANNEL, mask + " :You're not on that channel");
            return;
        }
        
        const std::set<Client*>& clients = channel->getClients();
        for (std::set<Client*>::const_iterator it = clients.begin(); it != clients.end(); ++it) {
            _sendWhoReply(client, channel, *it);
        }
    }
    
    _sendNumericReply(client, RPL_ENDOFWHO, mask + " :End of /WHO list");
}

void Server::_handleWhois(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "WHOIS :Not enough parameters");
        return;
    }
    
    std::string targetNick = params[0];
    Client* targetClient = getClientByNick(targetNick);
    
    if (!targetClient) {
        _sendNumericReply(client, ERR_NOSUCHNICK, targetNick + " :No such nick");
        return;
    }
    
    _sendWhoisReply(client, targetClient);
    _sendNumericReply(client, RPL_ENDOFWHOIS, targetNick + " :End of /WHOIS list");
}

void Server::_handleList(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendNumericReply(client, RPL_LISTSTART, "Channel :Users  Name");
    
    if (params.empty()) {
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
            _sendListReply(client, it->second);
        }
    } else {
        std::string channelList = params[0];
        std::istringstream channelStream(channelList);
        std::string channelName;
        
        while (std::getline(channelStream, channelName, ',')) {
            if (!channelName.empty()) {
                Channel* channel = getChannel(channelName);
                if (channel) {
                    _sendListReply(client, channel);
                }
            }
        }
    }
    
    _sendNumericReply(client, RPL_LISTEND, ":End of /LIST");
}

void Server::_handleNames(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    if (params.empty()) {
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it) {
            _sendNumericReply(client, RPL_NAMREPLY, "= " + it->first + " :" + it->second->getNamesReply());
            _sendNumericReply(client, RPL_ENDOFNAMES, it->first + " :End of /NAMES list");
        }
    } else {
        std::string channelList = params[0];
        std::istringstream channelStream(channelList);
        std::string channelName;
        
        while (std::getline(channelStream, channelName, ',')) {
            if (!channelName.empty()) {
                Channel* channel = getChannel(channelName);
                if (channel) {
                    _sendNumericReply(client, RPL_NAMREPLY, "= " + channelName + " :" + channel->getNamesReply());
                    _sendNumericReply(client, RPL_ENDOFNAMES, channelName + " :End of /NAMES list");
                }
            }
        }
    }
}

void Server::_handleMotd(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendMotd(client);
}

void Server::_handleAdmin(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendNumericReply(client, 256, ":Administrative info about " + _serverName);
    _sendNumericReply(client, 257, ":42 School IRC Server");
    _sendNumericReply(client, 258, ":ft_irc project implementation");
    _sendNumericReply(client, 259, ":Contact your system administrator");
}

void Server::_handleTime(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    time_t now;
    time(&now);
    std::string timeStr = ctime(&now);
    if (!timeStr.empty() && timeStr[timeStr.length()-1] == '\n') {
        timeStr.erase(timeStr.length()-1);
    }
    
    _sendNumericReply(client, RPL_TIME, _serverName + " :" + timeStr);
}

void Server::_handleVersion(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendNumericReply(client, RPL_VERSION, _serverVersion + "." + _serverName + " :ft_irc server");
}

void Server::_handleInfo(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendNumericReply(client, RPL_INFO, ":ft_irc - Internet Relay Chat Server");
    _sendNumericReply(client, RPL_INFO, ":Version " + _serverVersion);
    _sendNumericReply(client, RPL_INFO, ":Created by 42 School students");
    _sendNumericReply(client, RPL_INFO, ":This is a project implementation");
    _sendNumericReply(client, RPL_INFO, ":Built with C++98 compliance");
    _sendNumericReply(client, RPL_INFO, ":Supports standard IRC commands");
    _sendNumericReply(client, RPL_ENDOFINFO, ":End of /INFO list");
}

void Server::_handleStats(Client* client, const std::vector<std::string>& params) {
    (void)params;
    if (!client->isRegistered()) {
        _sendNumericReply(client, ERR_NOTREGISTERED, ":You have not registered");
        return;
    }
    
    _sendStatsReply(client);
}

void Server::_sendWhoReply(Client* client, Channel* channel, Client* target) {
    std::string flags = "H";
    if (channel->isOperator(target)) {
        flags += "@";
    }
    
    std::ostringstream oss;
    oss << channel->getName() << " " << target->getUsername() << " " 
        << target->getHostname() << " " << _serverName << " " 
        << target->getNickname() << " " << flags << " :0 " << target->getRealname();
    
    _sendNumericReply(client, RPL_WHOREPLY, oss.str());
}

void Server::_sendWhoisReply(Client* client, Client* target) {
    _sendNumericReply(client, RPL_WHOISUSER, target->getNickname() + " " + 
                     target->getUsername() + " " + target->getHostname() + " * :" + target->getRealname());
    
    _sendNumericReply(client, RPL_WHOISSERVER, target->getNickname() + " " + 
                     _serverName + " :" + _serverName + " IRC Server");
    
    if (!target->getChannels().empty()) {
        std::string channels;
        const std::set<Channel*>& clientChannels = target->getChannels();
        for (std::set<Channel*>::const_iterator it = clientChannels.begin(); it != clientChannels.end(); ++it) {
            if (!channels.empty()) channels += " ";
            if ((*it)->isOperator(target)) channels += "@";
            channels += (*it)->getName();
        }
        _sendNumericReply(client, RPL_WHOISCHANNELS, target->getNickname() + " :" + channels);
    }
    
    _sendNumericReply(client, RPL_WHOISIDLE, target->getNickname() + " 0 " + 
                     intToString(_startTime) + " :seconds idle, signon time");
}

void Server::_sendListReply(Client* client, Channel* channel) {
    std::ostringstream oss;
    oss << channel->getName() << " " << channel->getClientCount() << " :";
    if (!channel->getTopic().empty()) {
        oss << channel->getTopic();
    }
    
    _sendNumericReply(client, RPL_LIST, oss.str());
}

void Server::_sendStatsReply(Client* client) {
    _sendNumericReply(client, 242, ":Server Up " + _getUptime());
    _sendNumericReply(client, 243, ":Total connections: " + sizeToString(_totalConnections));
    _sendNumericReply(client, 244, ":Current connections: " + sizeToString(_currentConnections));
    _sendNumericReply(client, 245, ":Maximum connections: " + sizeToString(_maxClients));
    _sendNumericReply(client, 246, ":Active channels: " + sizeToString(_channels.size()));
    _sendNumericReply(client, 219, "u :End of /STATS report");
}