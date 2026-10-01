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
        CloseHandle(process.hThread); handle_=process.hProcess;
#else
        std::vector<char*> argv; for(const auto& argument:arguments) argv.push_back(const_cast<char*>(argument.c_str())); argv.push_back(nullptr);
        group_=group;
        pid_=fork();
        if(pid_<0) throw std::runtime_error("Unable to fork subprocess");
        if(pid_==0) {
            if(group) setpgid(0,0);
            std::signal(SIGINT,SIG_DFL); std::signal(SIGTERM,SIG_DFL);
            if(!cwd.empty() && chdir(cwd.c_str())!=0) { std::perror("gungnir: chdir"); _exit(127); }
            execvp(argv[0],argv.data()); std::perror(argv[0]); _exit(127);
        }
        if(group) setpgid(pid_,pid_);
#endif
    }
    std::optional<int> poll() { return collect(false); }
    int wait() { return collect(true).value_or(0); }
    void stop() noexcept {
#ifdef _WIN32
        if(handle_) { TerminateProcess(handle_,0); WaitForSingleObject(handle_,5000); CloseHandle(handle_); handle_=nullptr; }
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
#else
    pid_t pid_{-1}; bool group_{false};
#endif
    std::optional<int> collect(bool block) noexcept {
#ifdef _WIN32
        if(!handle_) return {};
        const auto result=WaitForSingleObject(handle_,block?INFINITE:0); if(result==WAIT_TIMEOUT) return {};
        DWORD code=1; GetExitCodeProcess(handle_,&code); CloseHandle(handle_); handle_=nullptr; return static_cast<int>(code);
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
