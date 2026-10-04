#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <gungnir/http/response.hpp>
#include <gungnir/http/uploads.hpp>

namespace gungnir::http {

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
