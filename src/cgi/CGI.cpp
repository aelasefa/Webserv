#include "../../includes/CGI.hpp"

#include <sys/wait.h>
#include <sys/types.h>

CGI::CGI() {}

CGI::~CGI() {}

void CGI::setScriptPath(const std::string& path) {
    _scriptPath = path;
}

void CGI::setInterpreter(const std::string& interpreter) {
    _interpreter = interpreter;
}

std::string CGI::execute(const Request& request) {
    int inputPipe[2];
    int outputPipe[2];
    pipe(inputPipe);
    pipe(outputPipe);

    pid_t pid = fork();
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
        execve(_interpreter.c_str(), argv, environ);
        return NULL;
    } else {
        close(inputPipe[0]);
        close(outputPipe[1]);
        write(inputPipe[1], request.body.c_str(),
        request.body.size());
        close(inputPipe[1]);
        char buffer[1024];
        std::string result;
        ssize_t bytes;
        while ((bytes == read(outputPipe[0], buffer, sizeof(buffer))) > 0)
            result.append(buffer, bytes);
        close(outputPipe[0]);
        waitpid(pid, NULL, 0);
        return result;
    }
}