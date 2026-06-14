#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include <string>
#include <map>

class SessionManager
{
private:
    std::map<std::string, std::string> _sessions;
public:
    SessionManager();
    ~SessionManager();

    bool exists(const std::string &sessionId) const;
    void create(const std::string &sessionId, const std::string &theme);
    std::string getTheme(const std::string &sessionId) const;
    void setTheme(const std::string &sessionId, const std::string &theme);
};

#endif
