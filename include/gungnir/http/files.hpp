#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gungnir/http/response.hpp>

namespace gungnir::http {

class UploadedFile {
public:
    UploadedFile(std::string field, std::string name, std::string type, std::string bytes)
        : field_(std::move(field)), name_(std::move(name)), type_(std::move(type)), bytes_(std::move(bytes)) {}
    [[nodiscard]] const std::string& field() const noexcept{return field_;}
    [[nodiscard]] const std::string& name() const noexcept{return name_;}
    [[nodiscard]] const std::string& content_type() const noexcept{return type_;}
    [[nodiscard]] const std::string& bytes() const noexcept{return bytes_;}
    [[nodiscard]] std::size_t size() const noexcept{return bytes_.size();}
    void store(const std::filesystem::path& destination) const {
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output{destination,std::ios::binary|std::ios::trunc};
        if(!output)throw std::runtime_error("Unable to store uploaded file");
        output.write(bytes_.data(),static_cast<std::streamsize>(bytes_.size()));
    }
private:
    std::string field_,name_,type_,bytes_;
};

inline Response download(const std::filesystem::path& path,std::string filename={}) {
    std::ifstream input{path,std::ios::binary};
    if(!input)return Response::not_found();
    std::string body{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
    Response response{200,std::move(body)};
    response.header("content-type","application/octet-stream");
    response.header("content-disposition","attachment; filename=\""+(filename.empty()?path.filename().string():filename)+"\"");
    return response;
}

inline Response file(const std::filesystem::path& path,std::string content_type="application/octet-stream") {
    std::ifstream input{path,std::ios::binary};
    if(!input)return Response::not_found();
    std::string body{std::istreambuf_iterator<char>{input},std::istreambuf_iterator<char>{}};
    Response response{200,std::move(body)}; response.header("content-type",std::move(content_type)); return response;
}

} // namespace gungnir::http
