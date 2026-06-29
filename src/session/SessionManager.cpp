#include "../../includes/SessionManager.hpp"

SessionManager::SessionManager() {}
SessionManager::~SessionManager() {}

void SessionManager::evictExpired()
{
    time_t now = time(NULL);
    for (std::map<std::string, SessionData>::iterator it = _sessions.begin(); it != _sessions.end(); )
    {
        if (now - it->second.lastActive > 1800)
        {
            std::map<std::string, SessionData>::iterator to_erase = it;
            ++it;
            _sessions.erase(to_erase);
        }
        else
        {
            ++it;
        }
    }
}

bool SessionManager::exists(const std::string &sessionId)
{
    evictExpired();
    std::map<std::string, SessionData>::iterator it = _sessions.find(sessionId);
    if (it != _sessions.end())
    {
        it->second.lastActive = time(NULL);
        return true;
    }
    return false;
}

void SessionManager::create(const std::string &sessionId, const std::string &theme)
{
    evictExpired();
    SessionData data;
    data.theme = theme;
    data.lastActive = time(NULL);
    _sessions[sessionId] = data;
}

std::string SessionManager::getTheme(const std::string &sessionId)
{
    evictExpired();
    std::map<std::string, SessionData>::iterator it = _sessions.find(sessionId);
    if (it == _sessions.end())
        return "";
    it->second.lastActive = time(NULL);
    return it->second.theme;
}

void SessionManager::setTheme(const std::string &sessionId, const std::string &theme)
{
    evictExpired();
    std::map<std::string, SessionData>::iterator it = _sessions.find(sessionId);
    if (it != _sessions.end())
    {
        it->second.theme = theme;
        it->second.lastActive = time(NULL);
    }
}
