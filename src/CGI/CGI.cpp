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
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SCRIPT_FILENAME=" + _scriptPath);
    env.push_back("REDIRECT_STATUS=200");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    
    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); 
         it != headers.end(); ++it) {
        std::string headerKey = it->first;
        std::string headerValue = it->second;
        
        if (headerKey == "Content-Type" || headerKey == "Content-Length")
            continue;
        
        std::string envKey = "HTTP_";
        for (size_t i = 0; i < headerKey.length(); ++i) {
            char c = headerKey[i];
            if (c == '-')
                envKey += '_';
            else
                envKey += std::toupper(static_cast<unsigned char>(c));
        }
        
        env.push_back(envKey + "=" + headerValue);
    }
    
    return env;
}

void CGI::start(const Request& request, CGIState& state) {
    if (_interpreter.empty() || _scriptPath.empty())
        throw std::runtime_error("interpreter or script path not set");
    
    int inputPipe[2];
    int outputPipe[2];
    std::vector<std::string> envStrings = setEnv(request);
    std::vector<char *> envp;
    for (size_t i = 0; i < envStrings.size(); i++)
        envp.push_back(const_cast<char*>(envStrings[i].c_str()));
    envp.push_back(NULL);
    if (pipe(inputPipe) == -1 || pipe(outputPipe) == -1)
        throw std::runtime_error("pipe failed");
    state.pid = fork();
    if (state.pid == -1) {
        close(inputPipe[0]);
        close(inputPipe[1]);
        close(outputPipe[0]);
        close(outputPipe[1]);
        throw std::runtime_error("fork failed");
    }
    if (state.pid == 0) {
        close(inputPipe[1]);
        close(outputPipe[0]);
        dup2(inputPipe[0], STDIN_FILENO);
        dup2(outputPipe[1], STDOUT_FILENO);
        close(inputPipe[0]);
        close(outputPipe[1]);
        char* argv[] = {
            (char *)_interpreter.c_str(),
            (char *)_scriptPath.c_str(),
            NULL
        };
        execve(_interpreter.c_str(), argv, envp.data());
        _exit(1);
    }
    close(inputPipe[0]);
    close(outputPipe[1]);

    // if (request.getMethod() == "POST") {
    //     const std::string body = request.getBody();
    //     size_t total = 0;
    //     while (total < body.size()) {
    //         ssize_t written = write(inputPipe[1],
    //             body.c_str() + total,
    //             body.size() - total);
    //         if (written <= 0)
    //             break;
    //         total += written;
    //     }
    // }
    state.outputFd = outputPipe[0];
    state.startTime = time(NULL);
    state.running = true;
    state.inputFd = inputPipe[1];
    state.requestBody = request.getBody();
    state.written = 0;

    int flags = fcntl(state.outputFd, F_GETFL, 0);
    fcntl(state.outputFd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(state.inputFd, F_GETFL, 0);
    fcntl(state.inputFd, F_SETFL, flags | O_NONBLOCK);
}

bool CGI::update(CGIState& state) {
    ssize_t bytes;
    char buffer[1024];
    time_t elapsed = time(NULL) - state.startTime;
    if (elapsed >= CGI_TIMEOUT ) {
        kill(state.pid, SIGKILL);
        waitpid(state.pid, NULL, 0);
        if (state.inputFd != -1)
            close(state.inputFd);

        if (state.outputFd != -1)
            close(state.outputFd);
        state.running = false;
        throw std::runtime_error("CGI timeout");
    }

    if (state.inputFd != -1)
    {
        const std::string& body = state.requestBody;

        while (state.written < body.size())
        {
            ssize_t n = write(
                state.inputFd,
                body.c_str() + state.written,
                body.size() - state.written
        );

        if (n > 0)
            state.written += n;
        else
            throw std::runtime_error("write failed");
    }

    if (state.written == body.size())
    {
        close(state.inputFd);
        state.inputFd = -1;
    }
    }
    while ((bytes = read(state.outputFd, buffer, sizeof(buffer))) > 0)
        state.result.append(buffer, bytes);
    pid_t p = waitpid(state.pid, &state.status, WNOHANG);
    if (p == -1)
        throw std::runtime_error("waitpid failed");
    if (p == 0)
        return false;
    if (!WIFEXITED(state.status) || WEXITSTATUS(state.status) != 0)
    {
        close(state.outputFd);
        throw std::runtime_error("CGI execution failed");
    }
    state.running = false;
    close(state.outputFd);
    state.outputFd = -1;    
    return true;
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