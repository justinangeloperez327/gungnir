#include <gungnir/storage/s3_disk.hpp>
#include <curl/curl.h>
#include <memory>
#include <stdexcept>
#include <limits>
#include <charconv>
#include <unordered_set>

namespace gungnir::storage {
namespace {
std::string encode(std::string_view value) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += static_cast<char>(c);
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}
std::string key(std::string_view path) {
    if (path.empty() || path.front() == '/' || path.find('\0') != std::string_view::npos || path.find('\\') != std::string_view::npos)
        throw std::invalid_argument("S3 object key must be a relative logical path");
    std::string out;
    while (!path.empty()) {
        auto slash = path.find('/');
        auto part = path.substr(0,slash);
        if (part == ".." || part == "." || part.empty()) throw std::invalid_argument("Invalid S3 key segment");
        out += encode(part);
        if (slash == std::string_view::npos) break;
        out += '/'; path.remove_prefix(slash+1);
    }
    return out;
}
std::string decode(std::string_view value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '%') { out += value[i]; continue; }
        if (i+2 >= value.size()) throw std::runtime_error("Invalid S3 encoded key");
        unsigned byte{};
        auto [end,error] = std::from_chars(value.data()+i+1,value.data()+i+3,byte,16);
        if (error != std::errc{} || end != value.data()+i+3) throw std::runtime_error("Invalid S3 encoded key");
        out += static_cast<char>(byte); i += 2;
    }
    return out;
}
std::string xml_text(std::string text) {
    // S3 continuation tokens use XML's five predefined entities.
    for (auto [entity,value] : {std::pair{"&lt;","<"},{"&gt;",">"},{"&quot;","\""},{"&apos;","'"},{"&amp;","&"}}) {
        std::size_t offset{};
        while ((offset=text.find(entity,offset)) != std::string::npos) { text.replace(offset,std::char_traits<char>::length(entity),value); offset += std::char_traits<char>::length(value); }
    }
    return text;
}
}
S3Disk::S3Disk(S3Options options) : options_(std::move(options)) {
    if ((!options_.endpoint.starts_with("https://") && !(options_.allow_http && options_.endpoint.starts_with("http://"))) ||
        options_.endpoint.find_first_of("?#@\r\n") != std::string::npos || options_.bucket.empty() ||
        options_.bucket.find_first_of("/\\\r\n") != std::string::npos || options_.access_key.empty() || options_.secret_key.empty() ||
        options_.session_token.find_first_of("\r\n") != std::string::npos || options_.timeout.count() <= 0 ||
        options_.timeout.count() > std::numeric_limits<long>::max() || !options_.max_object_bytes || !options_.max_list_entries ||
        options_.endpoint.find('\0') != std::string::npos || options_.bucket.find('\0') != std::string::npos ||
        options_.access_key.find_first_of(":\r\n") != std::string::npos || options_.access_key.find('\0') != std::string::npos ||
        options_.secret_key.find('\0') != std::string::npos || options_.session_token.find('\0') != std::string::npos ||
        options_.region.empty() || options_.region.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
        throw std::invalid_argument("Invalid S3 endpoint, credentials, bucket, or limits");
    while (options_.endpoint.ends_with('/')) options_.endpoint.pop_back();
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK) throw std::runtime_error("Unable to initialize S3 HTTP transport");
}
S3Disk::Reply S3Disk::request(std::string_view method, std::string_view path, std::string_view query,
    std::string_view body, const CancellationToken& cancellation) const {
    cancellation.throw_if_cancelled();
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl{curl_easy_init(),curl_easy_cleanup};
    if (!curl) throw std::runtime_error("Unable to allocate S3 HTTP request");
    const auto url = options_.endpoint + "/" + encode(options_.bucket) + "/" + (path.empty() ? "" : key(path)) + std::string{query};
    const auto credentials = options_.access_key + ":" + options_.secret_key;
    const auto signature = "aws:amz:" + options_.region + ":s3";
    auto set = [&](CURLoption option, auto value) { if (curl_easy_setopt(curl.get(),option,value) != CURLE_OK) throw std::runtime_error("Unable to configure S3 HTTP request"); };
    Reply reply;
    struct Receiver { std::string& body; std::size_t limit; bool overflow{}; } receiver{reply.body,options_.max_object_bytes};
    set(CURLOPT_URL,url.c_str()); set(CURLOPT_USERPWD,credentials.c_str()); set(CURLOPT_AWS_SIGV4,signature.c_str());
    set(CURLOPT_TIMEOUT_MS,static_cast<long>(options_.timeout.count())); set(CURLOPT_CONNECTTIMEOUT_MS,static_cast<long>(options_.timeout.count()));
    set(CURLOPT_NOSIGNAL,1L); set(CURLOPT_FOLLOWLOCATION,0L);
    set(CURLOPT_WRITEFUNCTION,+[](char* data,std::size_t size,std::size_t count,void* opaque) -> std::size_t {
        auto& receiver = *static_cast<Receiver*>(opaque);
        if (size && count > std::numeric_limits<std::size_t>::max()/size) return 0;
        const auto bytes=size*count;
        if (bytes > receiver.limit-receiver.body.size()) { receiver.overflow=true; return 0; }
        try { receiver.body.append(data,bytes); } catch (...) { return 0; }
        return bytes;
    });
    set(CURLOPT_WRITEDATA,&receiver);
    set(CURLOPT_NOPROGRESS,0L); set(CURLOPT_XFERINFODATA,&cancellation);
    set(CURLOPT_XFERINFOFUNCTION,+[](void* opaque,curl_off_t,curl_off_t,curl_off_t,curl_off_t) -> int { return static_cast<const CancellationToken*>(opaque)->cancelled() ? 1 : 0; });
    std::unique_ptr<curl_slist,decltype(&curl_slist_free_all)> headers{nullptr,curl_slist_free_all};
    if (!options_.session_token.empty()) {
        headers.reset(curl_slist_append(nullptr,("x-amz-security-token: "+options_.session_token).c_str()));
        if (!headers) throw std::bad_alloc();
        set(CURLOPT_HTTPHEADER,headers.get());
    }
    const std::string verb{method};
    if (method == "HEAD") set(CURLOPT_NOBODY,1L);
    else if (method != "GET") {
        set(CURLOPT_CUSTOMREQUEST,verb.c_str());
        if (method == "PUT") {
            if (body.size() > options_.max_object_bytes) throw std::length_error("S3 object exceeds configured limit");
            set(CURLOPT_POSTFIELDS,body.data()); set(CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(body.size()));
        }
    }
    const auto code = curl_easy_perform(curl.get());
    cancellation.throw_if_cancelled();
    if (receiver.overflow) throw std::length_error("S3 response exceeds configured limit");
    if (code != CURLE_OK) throw std::runtime_error("S3 request failed: "+std::string{curl_easy_strerror(code)});
    curl_easy_getinfo(curl.get(),CURLINFO_RESPONSE_CODE,&reply.status);
    curl_off_t length{-1}; curl_easy_getinfo(curl.get(),CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,&length);
    if (length >= 0) reply.length=static_cast<std::uintmax_t>(length);
    if (reply.status != 404 && (reply.status < 200 || reply.status >= 300))
        throw std::runtime_error("S3 request returned HTTP "+std::to_string(reply.status));
    return reply;
}
bool S3Disk::exists(std::string_view path) const { return exists(path,{}); }
bool S3Disk::exists(std::string_view path,const CancellationToken& token) const { key(path); return request("HEAD",path,"","",token).status != 404; }
std::optional<std::string> S3Disk::get(std::string_view path) const { return get(path,{}); }
std::optional<std::string> S3Disk::get(std::string_view path,const CancellationToken& token) const { key(path); auto reply=request("GET",path,"","",token); if (reply.status==404) return std::nullopt; return std::move(reply.body); }
void S3Disk::put(std::string path,std::string content) { put(std::move(path),std::move(content),{}); }
void S3Disk::put(std::string path,std::string content,const CancellationToken& token) { key(path); auto result=request("PUT",path,"",content,token); if (result.status==404) throw std::runtime_error("S3 bucket does not exist"); }
bool S3Disk::remove(std::string_view path) { return remove(path,{}); }
bool S3Disk::remove(std::string_view path,const CancellationToken& token) { key(path); return request("DELETE",path,"","",token).status != 404; }
bool S3Disk::copy(std::string_view from,std::string_view to) { return copy(from,to,{}); }
bool S3Disk::copy(std::string_view from,std::string_view to,const CancellationToken& token) { key(to); auto body=get(from,token); if (!body) return false; put(std::string{to},std::move(*body),token); return true; }
bool S3Disk::move(std::string_view from,std::string_view to) { return move(from,to,{}); }
bool S3Disk::move(std::string_view from,std::string_view to,const CancellationToken& token) { if (from==to) return exists(from,token); return copy(from,to,token) && remove(from,token); }
std::uintmax_t S3Disk::size(std::string_view path) const { return size(path,{}); }
std::uintmax_t S3Disk::size(std::string_view path,const CancellationToken& token) const { key(path); auto reply=request("HEAD",path,"","",token); if (reply.status==404) throw std::out_of_range("S3 object does not exist"); return reply.length; }
std::vector<std::string> S3Disk::files(std::string_view directory) const { return files(directory,{}); }
std::vector<std::string> S3Disk::files(std::string_view directory,const CancellationToken& token) const {
    if (!directory.empty()) key(directory);
    const auto prefix=directory.empty() ? "" : std::string{directory} + (directory.ends_with('/') ? "" : "/");
    std::vector<std::string> result; std::string continuation; std::unordered_set<std::string> seen;
    do {
        auto query="?encoding-type=url&list-type=2&prefix="+encode(prefix);
        if (!continuation.empty()) query += "&continuation-token="+encode(continuation);
        auto reply=request("GET","",query,"",token);
        if (reply.status==404) throw std::out_of_range("S3 bucket does not exist");
        if (reply.body.find("<ListBucketResult") == std::string::npos) throw std::runtime_error("Invalid S3 list response");
        std::size_t offset{};
        while ((offset=reply.body.find("<Key>",offset)) != std::string::npos) {
            auto close=reply.body.find("</Key>",offset+5); if (close==std::string::npos) throw std::runtime_error("Invalid S3 key response");
            if (result.size() >= options_.max_list_entries) throw std::length_error("S3 listing exceeds configured limit");
            result.push_back(decode(std::string_view{reply.body}.substr(offset+5,close-offset-5))); offset=close+6;
        }
        continuation.clear();
        if (reply.body.find("<IsTruncated>true</IsTruncated>") != std::string::npos) {
            auto open=reply.body.find("<NextContinuationToken>"); auto close=reply.body.find("</NextContinuationToken>");
            if (open==std::string::npos || close==std::string::npos || close<=open+23) throw std::runtime_error("Missing S3 continuation token");
            continuation=xml_text(reply.body.substr(open+23,close-open-23));
            if (!seen.insert(continuation).second) throw std::runtime_error("Repeated S3 continuation token");
            if (seen.size() >= options_.max_list_entries) throw std::length_error("S3 pagination exceeds configured limit");
        }
    } while (!continuation.empty());
    return result;
}
}
