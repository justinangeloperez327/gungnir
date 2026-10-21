#pragma once
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <chrono>
#include <thread>

namespace gungnir::cli::detail {
class Child {
public:
    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    ~Child() { stop(); }
    void start(const std::vector<std::string>& arguments,const std::filesystem::path& cwd={},bool group=false) {
        if(arguments.empty()) throw std::invalid_argument("Empty subprocess command");
        stop();
#ifdef _WIN32
        auto quote=[](const std::wstring& value) {
            std::wstring result=L"\""; std::size_t slashes=0;
            for(wchar_t c:value) { if(c==L'\\') { ++slashes; continue; } result.append(c==L'"'?slashes*2+1:slashes,L'\\'); slashes=0; result+=c; }
            result.append(slashes*2,L'\\'); return result+L'"';
        };
        std::wstring command;
        for(const auto& argument:arguments) { if(!command.empty()) command+=L' '; command+=quote(std::filesystem::path{argument}.wstring()); }
        STARTUPINFOW info{}; info.cb=sizeof(info); PROCESS_INFORMATION process{};
        const auto directory=cwd.wstring();
        if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,TRUE,group?CREATE_NEW_PROCESS_GROUP:0,nullptr,cwd.empty()?nullptr:directory.c_str(),&info,&process))
            throw std::runtime_error("Unable to start " + arguments[0] + " (Windows error " + std::to_string(GetLastError()) + ")");
        CloseHandle(process.hThread);
        handle_=process.hProcess;
        group_id_=group ? process.dwProcessId : 0;
#else
        std::vector<char*> argv; for(const auto& argument:arguments) argv.push_back(const_cast<char*>(argument.c_str())); argv.push_back(nullptr);
        // The child reports a launch failure through this pipe. Its writer
        // closes automatically on successful exec (FD_CLOEXEC), allowing
        // start() to distinguish a launched process from a forked child that
        // immediately fails chdir/exec. The dev watcher can then roll back.
        int launch_pipe[2];
        if (pipe(launch_pipe) != 0)
            throw std::runtime_error("Unable to create subprocess launch pipe (errno " + std::to_string(errno) + ")");
        if (fcntl(launch_pipe[1], F_SETFD, FD_CLOEXEC) == -1) {
            const int error=errno;
            close(launch_pipe[0]); close(launch_pipe[1]);
            throw std::runtime_error("Unable to configure subprocess launch pipe (errno " + std::to_string(error) + ")");
        }
        pid_=fork();
        if(pid_<0) {
            const int error=errno;
            close(launch_pipe[0]); close(launch_pipe[1]);
            throw std::runtime_error("Unable to fork subprocess (errno " + std::to_string(error) + ")");
        }
        if(pid_==0) {
            close(launch_pipe[0]);
            const auto failed=[&](int error) {
                // A single int is smaller than PIPE_BUF, so this write is
                // atomic. Use only async-signal-safe calls after fork.
                while(write(launch_pipe[1],&error,sizeof(error))<0 && errno==EINTR) {}
                _exit(127);
            };
            if(group && setpgid(0,0)!=0) failed(errno);
            std::signal(SIGINT,SIG_DFL); std::signal(SIGTERM,SIG_DFL);
            if(!cwd.empty() && chdir(cwd.c_str())!=0) failed(errno);
            execvp(argv[0],argv.data());
            failed(errno);
        }
        close(launch_pipe[1]);
        if(group) (void)setpgid(pid_,pid_);
        int launch_error=0;
        std::size_t received=0;
        bool pipe_failed=false;
        auto* error_bytes=reinterpret_cast<char*>(&launch_error);
        while(received<sizeof(launch_error)) {
            const auto count=read(launch_pipe[0],error_bytes+received,sizeof(launch_error)-received);
            if(count>0) { received+=static_cast<std::size_t>(count); continue; }
            if(count==0) break;  // EOF from the close-on-exec writer.
            if(errno==EINTR) continue;
            launch_error=errno; pipe_failed=true;
            break;
        }
        close(launch_pipe[0]);
        if(received>0 || pipe_failed) {
            if(pipe_failed) (void)kill(pid_,SIGKILL);
            int status=0;
            while(waitpid(pid_,&status,0)<0 && errno==EINTR) {}
            pid_=-1;
            throw std::runtime_error(
                "Unable to start " + arguments[0] + " (errno " + std::to_string(launch_error) + ")");
        }
        group_=group;
#endif
    }
    std::optional<int> poll() { return collect(false); }
    int wait() { return collect(true).value_or(0); }
    void stop() noexcept {
#ifdef _WIN32
        if (!handle_) return;
        if (WaitForSingleObject(handle_, 0) != WAIT_OBJECT_0) {
            // A Windows console child created as its own process group can
            // receive CTRL+BREAK without also stopping the dev CLI. That
            // gives generated apps a chance to cancel requests and run their
            // SignalWatcher-backed shutdown handlers.
            const bool signalled = group_id_ != 0 &&
                GetConsoleWindow() != nullptr &&
                GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, group_id_) != 0;
            if (!signalled || WaitForSingleObject(handle_, 3000) != WAIT_OBJECT_0) {
                // A headless runner, detached child, or unresponsive process
                // cannot accept the console event. Bound restart/shutdown.
                TerminateProcess(handle_, 0);
            }
        }
        WaitForSingleObject(handle_, 5000);
        CloseHandle(handle_);
        handle_=nullptr;
        group_id_=0;
#else
        if(pid_<=0) return;
        const auto target=group_?-pid_:pid_;
        kill(target,SIGTERM);
        for(int i=0;i<40;++i) { if(collect(false)) return; std::this_thread::sleep_for(std::chrono::milliseconds(25)); }
        kill(target,SIGKILL); (void)collect(true);
#endif
    }
private:
#ifdef _WIN32
    HANDLE handle_{nullptr};
    DWORD group_id_{0};
#else
    pid_t pid_{-1}; bool group_{false};
#endif
    std::optional<int> collect(bool block) noexcept {
#ifdef _WIN32
        if(!handle_) return {};
        const auto result=WaitForSingleObject(handle_,block?INFINITE:0); if(result==WAIT_TIMEOUT) return {};
        DWORD code=1; GetExitCodeProcess(handle_,&code); CloseHandle(handle_); handle_=nullptr; group_id_=0; return static_cast<int>(code);
#else
        if(pid_<=0) return {};
        int status=0; pid_t result;
        do { result=waitpid(pid_,&status,block?0:WNOHANG); } while(result<0 && errno==EINTR);
        if(result==0) return {};
        pid_=-1;
        if(result<0) return 1;
        return WIFEXITED(status)?WEXITSTATUS(status):WIFSIGNALED(status)?128+WTERMSIG(status):1;
#endif
    }
};
inline int execute(const std::vector<std::string>& arguments,const std::filesystem::path& cwd={}) {
    Child child; child.start(arguments,cwd); return child.wait();
}
}
