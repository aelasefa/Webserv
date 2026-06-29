#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include <string>
#include <map>
#include <ctime>

class SessionManager
{
private:
    struct SessionData
    {
        std::string theme;
        time_t lastActive;
    };
    std::map<std::string, SessionData> _sessions;
    void evictExpired();

public:
    SessionManager();
    ~SessionManager();

    bool exists(const std::string &sessionId);
    void create(const std::string &sessionId, const std::string &theme);
    std::string getTheme(const std::string &sessionId);
    void setTheme(const std::string &sessionId, const std::string &theme);
};

#endif
