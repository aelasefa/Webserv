#include "../../includes/CGI.hpp"

CGI::CGI() {}

CGI::~CGI() {}

void CGI::setScriptPath(const std::string& path) {
    _scriptPath = path;
}

void CGI::setInterpreter(const std::string& interpreter) {
    _interpreter = interpreter;
}

std::vector<std::string> CGI::setEnv(const Request& request) {
    std::vector<std::string> env;
    env.push_back("REQUEST_METHOD=" + request.getMethod());
    env.push_back("QUERY_STRING=" + request.getQueryString());
    env.push_back("CONTENT_LENGTH=" + intToString(request.getBody().size()));
    env.push_back("CONTENT_TYPE=" + request.getHeader("Content-Type"));
    env.push_back("SCRIPT_NAME=" + _scriptPath);
    env.push_back("GATEWAY_INTERFACE=" + request.getVersion());
    env.push_back("SCRIPT_FILENAME=" + _scriptPath);
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    return env;
}

std::string CGI::execute(const Request& request) {
    if (_interpreter.empty() || _scriptPath.empty())
        throw std::runtime_error("interpreter or script path not set");
    int inputPipe[2];
    int outputPipe[2];
    if (pipe(inputPipe) == -1 || pipe(outputPipe) == -1)
        throw std::runtime_error("pipe failed");
        
    pid_t pid = fork();
    if (pid < 0) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);
        throw std::runtime_error("fork failed");
    }
    std::vector<std::string> envStrings = setEnv(request);
    std::vector<char *> envp;
    for (size_t i = 0; i < envStrings.size(); i++)
        envp.push_back(const_cast<char*>(envStrings[i].c_str()));
    envp.push_back(NULL);
    if (pid == 0) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        close(inputPipe[0]);
        close(outputPipe[1]);
        char *argv[] = {
            (char *)_interpreter.c_str(),
            (char *)_scriptPath.c_str(),
            NULL
        };
        execve(_interpreter.c_str(), argv, envp.data());
        _exit(1);
    } else {
        close(inputPipe[0]);
        close(outputPipe[1]);
        write(inputPipe[1], request.getBody().c_str(),
            request.getBody().size());
        close(inputPipe[1]);
        char buffer[1024];
        std::string result;
        ssize_t bytes;
        fcntl(outputPipe[0], F_SETFL, O_NONBLOCK);
        int status;
        time_t start_time = time(NULL);
        while (true) {
            pid_t proc = waitpid(pid, &status, WNOHANG);
            time_t elapsed = time(NULL) - start_time;
            while ((bytes = read(outputPipe[0], buffer, sizeof(buffer))) > 0)
                result.append(buffer, bytes);
            if (proc == pid)
                break;
            else if (proc == 0) {
                if (elapsed >= 30) {
                    kill(pid, SIGKILL);
                    waitpid(pid, &status, 0);
                    close(outputPipe[0]);
                    throw std::runtime_error("CGI timeout");
                }
                usleep(10000);
            }
            else if (proc == -1) {
                close(outputPipe[0]);
                throw std::runtime_error("waitpid failed");
            }
        }
        close(outputPipe[0]);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            throw std::runtime_error("CGI execution failed");
        return result;
    }
}

Response CGIHandler::buildResponse(const std::string& cgiOutput)
{
    Response res;

    res.setStatus(200);

    size_t pos = cgiOutput.find("\r\n\r\n");
    size_t separatorLen = 4;

    if (pos == std::string::npos)
    {
        pos = cgiOutput.find("\n\n");
        separatorLen = 2;
    }

    std::string headersPart;
    std::string bodyPart;

    if (pos != std::string::npos)
    {
        headersPart = cgiOutput.substr(0, pos);
        bodyPart = cgiOutput.substr(pos + separatorLen);
    }
    else
        bodyPart = cgiOutput;

    std::stringstream ss(headersPart);
    std::string line;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        while (!value.empty() && value[0] == ' ')
            value.erase(0, 1);

        if (key == "Status")
        {
            int status = atoi(value.c_str());
            res.setStatus(status);
        }
        else
            res.setHeader(key, value);
    }

    if (headersPart.empty())
        res.setHeader("Content-Type", "text/html");

    res.setBody(bodyPart);
    res.setHeader("Content-Length", intToString(bodyPart.size()));
    return res;
}