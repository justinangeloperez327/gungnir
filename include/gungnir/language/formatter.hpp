#pragma once

#include <string>
#include <string_view>

namespace gungnir::language {

class Formatter {
public:
    [[nodiscard]] std::string format(std::string_view source) const {
        std::string out;
        out.reserve(source.size()+16);
        std::size_t indent=0;
        bool line_start=true;
        for(char c:source){
            if(line_start && c!='\n'){out.append(indent*4,' ');line_start=false;}
            if(c=='}' && indent>0){
                if(!out.empty() && out.back()!='\n'){out+='\n';}
                --indent; out.append(indent*4,' '); out+=c;
            } else {
                out+=c;
                if(c=='{')++indent;
            }
            if(c=='\n')line_start=true;
        }
        if(!out.empty() && out.back()!='\n')out+='\n';
        return out;
    }
};

} // namespace gungnir::language
