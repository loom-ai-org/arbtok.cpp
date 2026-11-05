#include "fetch.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <fstream>

#include "detail.hpp"

namespace arbtok::detail {

std::string fetch_url(const std::string& url) {
    int pipefd[2];
    if (::pipe(pipefd) != 0) throw Error("OSError", "cannot create a pipe for " + url);
    const pid_t pid = ::fork();
    if (pid == 0) {
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        ::execlp("curl", "curl", "--fail", "--silent", "--show-error", "--location", url.c_str(),
                 static_cast<char*>(nullptr));
        ::_exit(127);
    }
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        throw Error("OSError", "cannot start curl for " + url);
    }
    ::close(pipefd[1]);
    std::string body;
    char buffer[65536];
    for (;;) {
        const ssize_t got = ::read(pipefd[0], buffer, sizeof buffer);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        body.append(buffer, static_cast<size_t>(got));
    }
    ::close(pipefd[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) throw Error("OSError", "curl could not fetch " + url);
    return body;
}

void write_file_atomic(const std::string& path, const std::string& data) {
    const std::string tmp = path + ".part" + std::to_string(::getpid());
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << data;
        if (!out) throw Error("OSError", "cannot write " + tmp);
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        throw Error("OSError", "cannot write " + path);
    }
}

}  // namespace arbtok::detail
