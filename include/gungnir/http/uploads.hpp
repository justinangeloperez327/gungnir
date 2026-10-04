#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gungnir::http {

// Owns the bytes independently of the request and of a suspended action.
// The public fields retain the original security.hpp native API.
struct UploadedFile {
    std::string name;
    std::string filename;
    std::string content_type;
    std::string content;

    [[nodiscard]] const std::string& field() const noexcept { return name; }
    [[nodiscard]] const std::string& original_name() const noexcept { return filename; }
    [[nodiscard]] const std::string& media_type() const noexcept { return content_type; }
    [[nodiscard]] const std::string& bytes() const noexcept { return content; }
    [[nodiscard]] std::size_t size() const noexcept { return content.size(); }
    void store(const std::filesystem::path& destination) const {
        if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
        std::ofstream output{destination, std::ios::binary | std::ios::trunc};
        if (!output) throw std::runtime_error("Unable to store uploaded file");
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.close();
        if (!output) throw std::runtime_error("Unable to store uploaded file");
    }
};

struct MultipartLimits {
    std::size_t body_bytes{16 * 1024 * 1024};
    std::size_t file_bytes{8 * 1024 * 1024};
    std::size_t field_bytes{1024 * 1024};
    std::size_t header_bytes{8 * 1024};
    std::size_t parts{128};
};

struct MultipartInput {
    std::unordered_map<std::string, std::string> fields;
    std::vector<UploadedFile> files;
};

[[nodiscard]] MultipartInput parse_multipart(
    std::string_view content_type, std::string_view body, const MultipartLimits& limits = {}
);

// Format recognition checks container headers and bounds, not pixel decoding.
[[nodiscard]] std::string_view detected_media_type(std::string_view bytes) noexcept;

} // namespace gungnir::http
