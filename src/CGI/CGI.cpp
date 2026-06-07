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
    if (pid < 0)
        throw std::runtime_error("fork failed");
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
        while ((bytes = read(outputPipe[0], buffer, sizeof(buffer))) > 0)
            result.append(buffer, bytes);
        close(outputPipe[0]);
        int status;
        waitpid(pid, &status, 0);
        return result;
    }
}

Response CGIHandler::buildResponse(const std::string& cgiOutput) {
    Response res;
    std::
}