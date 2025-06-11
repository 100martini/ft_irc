#include "Server.hpp"
#include "Client.hpp"
#include "Channel.hpp"

void Server::_handlePass(Client* client, const std::vector<std::string>& params) {
    if (client->isRegistered()) {
        _sendNumericReply(client, ERR_ALREADYREGISTRED, ":You may not reregister");
        return;
    }
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PASS :Not enough parameters");
        return;
    }
    
    if (params[0] == _password) {
        client->setPasswordProvided(true);
        client->tryRegister();
    } else {
        _sendNumericReply(client, ERR_PASSWDMISMATCH, ":Password incorrect");
    }
}

void Server::_handleNick(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        _sendNumericReply(client, ERR_NONICKNAMEGIVEN, ":No nickname given");
        return;
    }
    
    std::string newNick = params[0];
    
    if (!_isValidNickname(newNick)) {
        _sendNumericReply(client, ERR_ERRONEUSNICKNAME, newNick + " :Erroneous nickname");
        return;
    }
    
    if (getClientByNick(newNick) != NULL) {
        _sendNumericReply(client, ERR_NICKNAMEINUSE, newNick + " :Nickname is already in use");
        return;
    }
    
    std::string oldNick = client->getNickname();
    client->setNickname(newNick);
    
    if (client->isRegistered()) {
        // Notify all channels about nick change
        std::string nickMsg = ":" + client->getPrefix() + " NICK :" + newNick;
        std::set<Channel*> channels = client->getChannels();
        for (std::set<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
            _sendToChannel(*it, nickMsg);
        }
        _sendToClient(client->getFd(), nickMsg);
    } else {
        client->tryRegister();
        if (client->isRegistered()) {
            _sendWelcomeSequence(client);
        }
    }
}

void Server::_handleUser(Client* client, const std::vector<std::string>& params) {
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
    if (!client->isRegistered()) return;
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "JOIN :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string key = (params.size() > 1) ? params[1] : "";
    
    if (channelName[0] != '#') {
        channelName = "#" + channelName;
    }
    
    Channel* channel = _getOrCreateChannel(channelName);
    
    if (!channel->canJoin(client, key)) {
        if (channel->getUserLimit() > 0 && channel->getClientCount() >= static_cast<size_t>(channel->getUserLimit())) {
            _sendNumericReply(client, ERR_CHANNELISFULL, channelName + " :Cannot join channel (+l)");
        } else if (channel->isInviteOnly() && !channel->isInvited(client)) {
            _sendNumericReply(client, ERR_INVITEONLYCHAN, channelName + " :Cannot join channel (+i)");
        } else if (channel->hasKey() && key != channel->getKey()) {
            _sendNumericReply(client, ERR_BADCHANNELKEY, channelName + " :Cannot join channel (+k)");
        }
        return;
    }
    
    client->joinChannel(channel);
    channel->removeInvited(client); // Remove from invite list after joining
    
    // Send join message to all users in channel
    std::string joinMsg = ":" + client->getPrefix() + " JOIN :" + channelName;
    _sendToChannel(channel, joinMsg);
    
    // Send topic if exists
    if (!channel->getTopic().empty()) {
        _sendNumericReply(client, RPL_TOPIC, channelName + " :" + channel->getTopic());
    }
    
    // Send names list
    _sendNumericReply(client, RPL_NAMREPLY, "= " + channelName + " :" + channel->getNamesReply());
    _sendNumericReply(client, RPL_ENDOFNAMES, channelName + " :End of /NAMES list");
}

void Server::_handlePart(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) return;
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PART :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string reason = (params.size() > 1) ? params[1] : client->getNickname();
    
    Channel* channel = getChannel(channelName);
    if (!channel || !channel->hasClient(client)) {
        _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :You're not on that channel");
        return;
    }
    
    std::string partMsg = ":" + client->getPrefix() + " PART " + channelName + " :" + reason;
    _sendToChannel(channel, partMsg);
    
    client->leaveChannel(channel);
    
    if (channel->isEmpty()) {
        _channels.erase(channelName);
        delete channel;
    }
}

void Server::_handlePrivmsg(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) return;
    
    if (params.size() < 2) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PRIVMSG :Not enough parameters");
        return;
    }
    
    std::string target = params[0];
    std::string message = params[1];
    
    if (target[0] == '#') {
        // Channel message
        Channel* channel = getChannel(target);
        if (!channel) {
            _sendNumericReply(client, ERR_NOSUCHCHANNEL, target + " :No such channel");
            return;
        }
        
        if (!channel->hasClient(client)) {
            _sendNumericReply(client, ERR_CANNOTSENDTOCHAN, target + " :Cannot send to channel");
            return;
        }
        
        std::string privmsgMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
        _sendToChannel(channel, privmsgMsg, client);
    } else {
        // Private message
        Client* targetClient = getClientByNick(target);
        if (!targetClient) {
            _sendNumericReply(client, ERR_NOSUCHNICK, target + " :No such nick/channel");
            return;
        }
        
        std::string privmsgMsg = ":" + client->getPrefix() + " PRIVMSG " + target + " :" + message;
        _sendToClient(targetClient->getFd(), privmsgMsg);
    }
}

void Server::_handleQuit(Client* client, const std::vector<std::string>& params) {
    std::string reason = params.empty() ? "Client quit" : params[0];
    
    std::string quitMsg = ":" + client->getPrefix() + " QUIT :" + reason;
    
    // Send quit message to all channels
    std::set<Channel*> channels = client->getChannels();
    for (std::set<Channel*>::iterator it = channels.begin(); it != channels.end(); ++it) {
        _sendToChannel(*it, quitMsg, client);
    }
    
    _removeClient(client->getFd());
}

void Server::_handlePing(Client* client, const std::vector<std::string>& params) {
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "PING :Not enough parameters");
        return;
    }
    
    std::string pongMsg = ":" + _serverName + " PONG " + _serverName + " :" + params[0];
    _sendToClient(client->getFd(), pongMsg);
}

void Server::_handleKick(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) return;
    
    if (params.size() < 2) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "KICK :Not enough parameters");
        return;
    }
    
    std::string channelName = params[0];
    std::string targetNick = params[1];
    std::string reason = (params.size() > 2) ? params[2] : client->getNickname();
    
    Channel* channel = getChannel(channelName);
    if (!channel) {
        _sendNumericReply(client, ERR_NOSUCHCHANNEL, channelName + " :No such channel");
        return;
    }
    
    if (!channel->isOperator(client)) {
        _sendNumericReply(client, ERR_CHANOPRIVSNEEDED, channelName + " :You're not channel operator");
        return;
    }
    
    Client* targetClient = getClientByNick(targetNick);
    if (!targetClient || !channel->hasClient(targetClient)) {
        _sendNumericReply(client, ERR_NOTONCHANNEL, channelName + " :They aren't on that channel");
        return;
    }
    
    std::string kickMsg = ":" + client->getPrefix() + " KICK " + channelName + " " + targetNick + " :" + reason;
    _sendToChannel(channel, kickMsg);
    
    targetClient->leaveChannel(channel);
}

void Server::_handleInvite(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) return;
    
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
        _sendNumericReply(client, ERR_NOSUCHNICK, targetNick + " :No such nick/channel");
        return;
    }
    
    if (channel->hasClient(targetClient)) {
        _sendNumericReply(client, ERR_USERONCHANNEL, targetNick + " " + channelName + " :is already on channel");
        return;
    }
    
    channel->addInvited(targetClient);
    
    std::string inviteMsg = ":" + client->getPrefix() + " INVITE " + targetNick + " :" + channelName;
    _sendToClient(targetClient->getFd(), inviteMsg);
}

void Server::_handleTopic(Client* client, const std::vector<std::string>& params) {
    if (!client->isRegistered()) return;
    
    if (params.empty()) {
        _sendNumericReply(client, ERR_NEEDMOREPARAMS, "TOPIC :Not enough parameters");
        return;
    }
    
    std::string channelName = param