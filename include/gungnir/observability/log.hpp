#pragma once

#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace gungnir::observability {

enum class Level { debug, info, warning, error };

class Logger {
public:
    void write(Level level,std::string_view message,const std::unordered_map<std::string,std::string>& fields={}) {
        std::lock_guard lock{mutex_};
        std::clog<<"{\"level\":\""<<name(level)<<"\",\"message\":\""<<escape(message)<<"\"";
        for(const auto& [key,value]:fields)std::clog<<",\""<<escape(key)<<"\":\""<<escape(value)<<"\"";
        std::clog<<"}\n";
    }
    void info(std::string_view message,const std::unordered_map<std::string,std::string>& fields={}){write(Level::info,message,fields);}
    void error(std::string_view message,const std::unordered_map<std::string,std::string>& fields={}){write(Level::error,message,fields);}
private:
    std::mutex mutex_;
    static std::string_view name(Level level){switch(level){case Level::debug:return "debug";case Level::info:return "info";case Level::warning:return "warning";case Level::error:return "error";}return "info";}
    static std::string escape(std::string_view value){std::string out;for(char c:value){if(c=='"'||c=='\\')out+='\\';out+=c;}return out;}
};

} // namespace gungnir::observability
