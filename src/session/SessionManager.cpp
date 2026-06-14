#include "../../includes/SessionManager.hpp"

SessionManager::SessionManager() {}
SessionManager::~SessionManager() {}

bool SessionManager::exists(const std::string &sessionId) const
{
    return _sessions.find(sessionId) != _sessions.end();
}

void SessionManager::create(const std::string &sessionId, const std::string &theme)
{
    _sessions[sessionId] = theme;
}

std::string SessionManager::getTheme(const std::string &sessionId) const
{
    std::map<std::string, std::string>::const_iterator it = _sessions.find(sessionId);
    if (it == _sessions.end())
        return "";
    return it->second;
}

void SessionManager::setTheme(const std::string &sessionId, const std::string &theme)
{
    _sessions[sessionId] = theme;
}
