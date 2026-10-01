#include <gungnir/cli/lsp.hpp>
#include <gungnir/http/json.hpp>
#include <gungnir/language/lsp.hpp>
#include <gungnir/language/formatter.hpp>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace gungnir::cli {
namespace {
using Json = http::Json;
Json num(std::size_t n) { return Json{static_cast<UInt64>(n)}; }
const Json& field(const Json& object,std::string_view key) { const auto* value=object.get(key); if (!value) throw std::invalid_argument("Missing field: " + std::string{key}); return *value; }
std::string str(const Json& object,std::string_view key) { const auto& value=field(object,key); if (!value.is_string()) throw std::invalid_argument("Expected string"); return value.string(); }
std::int64_t integer(const Json& value) { if (!value.is_integer()) throw std::invalid_argument("Expected integer"); return std::stoll(value.dump()); }
std::size_t natural(const Json& value) { const auto n=integer(value); if(n<0) throw std::invalid_argument("Expected nonnegative integer"); return static_cast<std::size_t>(n); }
std::string path_from_uri(std::string_view uri) {
    if (!uri.starts_with("file://")) throw std::invalid_argument("Only local file URIs are supported");
    uri.remove_prefix(7);
    if (uri.starts_with("localhost/")) uri.remove_prefix(9);
    if (uri.empty() || uri.front() != '/') throw std::invalid_argument("File URI must have an absolute local path");
    std::string result;
    auto hex=[](char c)->int { if(c>='0' && c<='9') return c-'0'; if(c>='a' && c<='f') return c-'a'+10; if(c>='A' && c<='F') return c-'A'+10; return -1; };
    for (std::size_t i=0;i<uri.size();++i) {
        if (uri[i]=='%') { if (i+2>=uri.size() || hex(uri[i+1])<0 || hex(uri[i+2])<0) throw std::invalid_argument("Malformed URI escape"); const char c=static_cast<char>(hex(uri[i+1])*16+hex(uri[i+2])); if(c=='\0') throw std::invalid_argument("Null in URI"); result+=c; i+=2; }
        else result+=uri[i];
    }
#ifdef _WIN32
    if(result.size()>2 && result[0]=='/' && result[2]==':') result.erase(0,1);
#endif
    return std::filesystem::absolute(result).lexically_normal().generic_string();
}
std::string uri_from_path(const std::string& path) {
    static constexpr char hex[]="0123456789ABCDEF"; std::string result="file://";
    if (!path.starts_with('/')) result+='/';
    for(unsigned char c:path) if(std::isalnum(c) || c=='/' || c==':' || c=='-' || c=='_' || c=='.' || c=='~') result+=c;
        else { result+='%'; result+=hex[c>>4]; result+=hex[c&15]; }
    return result;
}
// UTF-8 source offsets and UTF-16 LSP code units. Never split a surrogate pair.
std::size_t width(unsigned char c) { return c<0x80?1:c<0xE0?2:c<0xF0?3:4; }
Json position(std::string_view text,std::size_t offset) {
    offset=std::min(offset,text.size()); std::size_t line=0,column=0;
    for(std::size_t i=0;i<offset;) { if(text[i]=='\n') { ++line; column=0; ++i; } else { const auto w=width(static_cast<unsigned char>(text[i])); column+=w==4?2:1; i+=std::min(w,text.size()-i); } }
    return Json::object({{"line",num(line)},{"character",num(column)}});
}
std::size_t offset(std::string_view text,const Json& pos) {
    const auto line=natural(field(pos,"line")), character=natural(field(pos,"character")); std::size_t i=0;
    for(std::size_t n=0;n<line;++n) { const auto next=text.find('\n',i); if(next==text.npos) return text.size(); i=next+1; }
    std::size_t column=0;
    while(i<text.size() && text[i]!='\n' && text[i]!='\r' && column<character) { const auto w=width(static_cast<unsigned char>(text[i])); if(column+(w==4?2:1)>character) throw std::invalid_argument("Position splits a UTF-16 surrogate pair"); column+=w==4?2:1; i+=std::min(w,text.size()-i); }
    return i;
}
Json range(std::string_view text,std::size_t begin,std::size_t end) { return Json::object({{"start",position(text,begin)},{"end",position(text,end)}}); }
struct Document { std::string uri,text; std::int64_t version; };
class Server {
    std::istream& input; std::ostream& output;
    bool initialized=false, shutdown=false;
    std::filesystem::path root;
    std::map<std::string,Document> documents;
    std::map<std::string,std::string> sources;
    language::LanguageServer language;
    void send(Json message) { const auto body=message.dump(); output << "Content-Length: " << body.size() << "\r\n\r\n" << body; output.flush(); }
    void reply(const Json& id,Json value) { send(Json::object({{"jsonrpc","2.0"},{"id",id},{"result",std::move(value)}})); }
    void error(const Json& id,int code,std::string message) { send(Json::object({{"jsonrpc","2.0"},{"id",id},{"error",Json::object({{"code",Json{static_cast<Int64>(code)}},{"message",std::move(message)}})}})); }
    std::string module(const std::string& path) const {
        if(root.empty()) return {};
        auto relative=std::filesystem::path{path}.lexically_relative(root);
        if(relative.empty() || relative.generic_string().starts_with("..")) return {};
        relative.replace_extension(); auto name=relative.generic_string(); std::replace(name.begin(),name.end(),'/','.'); return name;
    }
    void refresh() {
        sources.clear();
        if(!root.empty() && std::filesystem::is_directory(root)) {
            for(auto it=std::filesystem::recursive_directory_iterator(root,std::filesystem::directory_options::skip_permission_denied);it!=std::filesystem::recursive_directory_iterator{};++it) {
                const auto name=it->path().filename().string();
                if(it->is_directory() && (name==".git" || name==".gungnir" || name=="build" || name=="vendor" || name=="routes" || name=="storage")) { it.disable_recursion_pending(); continue; }
                if(it->is_regular_file() && it->path().extension()==".gnr") { std::ifstream in(it->path(),std::ios::binary); if(in) sources[it->path().generic_string()]={std::istreambuf_iterator<char>{in},{}}; }
            }
        }
        for(const auto& [path,doc]:documents) sources[path]=doc.text;
        std::vector<language::SourceFile> snapshot;
        for(const auto& [path,text]:sources) snapshot.push_back({path,module(path),text});
        language.update(std::move(snapshot));
        for(const auto& [path,doc]:documents) {
            Json::Array diagnostics;
            for(const auto& d:language.diagnostics()) if(d.location.file==path) {
                std::size_t begin=0;
                for(std::size_t line=1;line<d.location.line;++line) { auto at=doc.text.find('\n',begin); if(at==std::string::npos) break; begin=at+1; }
                begin=std::min(doc.text.size(),begin+(d.location.column?d.location.column-1:0));
                const auto end=begin<doc.text.size()?begin+std::min(width(static_cast<unsigned char>(doc.text[begin])),doc.text.size()-begin):begin;
                diagnostics.push_back(Json::object({{"range",range(doc.text,begin,end)},{"severity",num(d.level==language::DiagnosticLevel::error?1:2)},{"code",d.code},{"source","gungnir"},{"message",d.message}}));
            }
            publish(doc.uri,std::move(diagnostics),doc.version);
        }
    }
    void publish(const std::string& uri,Json::Array diagnostics,std::optional<std::int64_t> version={}) {
        Json::Object params{{"uri",uri},{"diagnostics",Json::array(std::move(diagnostics))}};
        if(version) params["version"]=Json{static_cast<Int64>(*version)};
        send(Json::object({{"jsonrpc","2.0"},{"method","textDocument/publishDiagnostics"},{"params",Json::object(std::move(params))}}));
    }
    Json symbol(const language::DocumentSymbol& s) const {
        const auto& text=sources.at(s.range.file); Json::Array children;
        for(const auto& child:s.children) children.push_back(symbol(child));
        return Json::object({{"name",s.name},{"detail",s.detail},{"kind",num(s.kind)},{"range",range(text,s.range.begin,s.range.end)},{"selectionRange",range(text,s.selection.begin,s.selection.end)},{"children",Json::array(std::move(children))}});
    }
    bool dispatch(const Json& message) {
        const auto* id=message.get("id"); const auto* method=message.get("method");
        if(!message.is_object() || !method || !method->is_string() || !message.get("jsonrpc") || !message.get("jsonrpc")->is_string() || message.get("jsonrpc")->string()!="2.0" || (id && !id->is_integer() && !id->is_string() && !id->is_null())) { error(nullptr,-32600,"Invalid request"); return true; }
        const auto name=method->string();
        if(name=="exit") return false;
        try {
            if(name=="initialize") {
                if(initialized) { if(id) error(*id,-32600,"Already initialized"); return true; }
                if(!id) return true;
                const auto& params=field(message,"params");
                if(const auto* uri=params.get("rootUri");uri && !uri->is_null()) root=path_from_uri(uri->string());
                if (root.empty()) if (const auto* folders=params.get("workspaceFolders"); folders && folders->is_array() && !folders->as_array().empty())
                    root=path_from_uri(str(folders->as_array().front(),"uri"));
                initialized=true;
                reply(*id,Json::object({{"capabilities",Json::object({{"positionEncoding","utf-16"},{"textDocumentSync",Json::object({{"openClose",true},{"change",num(2)},{"save",true}})},{"hoverProvider",true},{"definitionProvider",true},{"documentSymbolProvider",true},{"completionProvider",Json::object({{"resolveProvider",false}})},{"documentFormattingProvider",true}})},{"serverInfo",Json::object({{"name","gungnir"},{"version","0.1.0"}})}})); return true;
            }
            if(!initialized) { if(id) error(*id,-32002,"Server not initialized"); return true; }
            if(shutdown) { if(id) error(*id,-32600,"Server is shutting down"); return true; }
            if(name=="shutdown") { if(id) { shutdown=true; reply(*id,nullptr); } return true; }
            if(name=="initialized" || name=="$/cancelRequest" || name=="$/setTrace") return true;
            const auto& params=field(message,"params");
            if(name=="workspace/didChangeWatchedFiles") { refresh(); return true; }
            if(!name.starts_with("textDocument/")) { if(id) error(*id,-32601,"Method not found"); return true; }
            const auto& doc=field(params,"textDocument"); const auto uri=str(doc,"uri"), path=path_from_uri(uri);
            if(name=="textDocument/didOpen") {
                if (root.empty()) {
                    root=std::filesystem::path{path}.parent_path();
                    for (auto directory=root; !directory.empty(); directory=directory.parent_path()) {
                        if (std::filesystem::exists(directory/".gungnir-project")) { root=directory; break; }
                        if (directory==directory.root_path()) break;
                    }
                }
                documents[path]={uri,str(doc,"text"),integer(field(doc,"version"))}; refresh(); return true; }
            if(name=="textDocument/didClose") { documents.erase(path); publish(uri,{}); refresh(); return true; }
            if(name=="textDocument/didSave") { refresh(); return true; }
            auto found=documents.find(path);
            if(found==documents.end()) { if(id) error(*id,-32602,"Document is not open"); return true; }
            auto& current=found->second;
            if(name=="textDocument/didChange") {
                const auto version=integer(field(doc,"version")); if(version<=current.version) return true;
                auto changed=current.text;
                for(const auto& change:field(params,"contentChanges").as_array()) {
                    if(const auto* span=change.get("range")) { const auto begin=offset(changed,field(*span,"start")), end=offset(changed,field(*span,"end")); if(end<begin) throw std::invalid_argument("Reversed edit range"); changed.replace(begin,end-begin,str(change,"text")); }
                    else changed=str(change,"text");
                }
                current.text=std::move(changed); current.version=version; refresh(); return true;
            }
            if(!id) return true;
            if(name=="textDocument/documentSymbol") { Json::Array result; for(const auto& entry:language.symbols(path)) result.push_back(symbol(entry)); reply(*id,Json::array(std::move(result))); }
            else if(name=="textDocument/formatting") { const auto formatted=language::Formatter{}.format(current.text); Json::Array edits; if(formatted!=current.text) edits.push_back(Json::object({{"range",range(current.text,0,current.text.size())},{"newText",formatted}})); reply(*id,Json::array(std::move(edits))); }
            else if(name=="textDocument/hover" || name=="textDocument/definition") {
                const auto info=language.symbol_at(path,offset(current.text,field(params,"position")));
                if(!info) reply(*id,nullptr);
                else if(name=="textDocument/hover") reply(*id,Json::object({{"contents",Json::object({{"kind","plaintext"},{"value",info->detail}})},{"range",range(current.text,info->selection.begin,info->selection.end)}}));
                else if(info->definition.file.empty() || !sources.contains(info->definition.file)) reply(*id,nullptr);
                else reply(*id,Json::object({{"uri",uri_from_path(info->definition.file)},{"range",range(sources.at(info->definition.file),info->definition.begin,info->definition.end)}}));
            } else if(name=="textDocument/completion") {
                Json::Array items; for(const auto& item:language.completions(path,offset(current.text,field(params,"position")))) items.push_back(Json::object({{"label",item.label},{"detail",item.detail},{"kind",num(item.kind)}}));
                reply(*id,Json::object({{"isIncomplete",false},{"items",Json::array(std::move(items))}}));
            } else error(*id,-32601,"Method not found");
        } catch(const std::exception& e) { if(id) error(*id,-32602,e.what()); }
        return true;
    }
public:
    Server(std::istream& in,std::ostream& out):input(in),output(out){}
    int run() {
        while(input) {
            std::string line; std::optional<std::size_t> length; std::size_t headers=0;
            while(std::getline(input,line)) {
                headers+=line.size()+1; if(headers>8192) return 1;
                if(!line.empty() && line.back()=='\r') line.pop_back();
                if(line.empty()) break;
                const auto colon=line.find(':'); if(colon==std::string::npos) return 1;
                auto key=line.substr(0,colon); std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                if(key=="content-length") { if(length) return 1; auto value=std::string_view{line}.substr(colon+1); while(!value.empty() && value.front()==' ') value.remove_prefix(1); std::size_t n; const auto parsed=std::from_chars(value.data(),value.data()+value.size(),n); if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size() || n>16*1024*1024) return 1; length=n; }
            }
            if(!input || !length) return shutdown?0:1;
            std::string body(*length,'\0'); if(!input.read(body.data(),static_cast<std::streamsize>(body.size()))) return 1;
            Json message;
            try { message=Json::parse(body); } catch(const std::exception&) { error(nullptr,-32700,"Parse error"); continue; }
            if(!dispatch(message)) return shutdown?0:1;
        }
        return shutdown?0:1;
    }
};
}
int run_language_server(std::istream& input,std::ostream& output) { return Server{input,output}.run(); }
}
