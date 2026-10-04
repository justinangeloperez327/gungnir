#include <gungnir/http/uploads.hpp>
#include <gungnir/http/errors.hpp>
#include <gungnir/security/security.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>

namespace gungnir::http {
namespace {
using View = std::string_view;
View trim(View value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}
std::string lower(View value) {
    std::string result{value};
    for (auto& c : result) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    return result;
}
[[noreturn]] void malformed() { throw BadRequestException{"Malformed multipart request body"}; }
[[noreturn]] void too_large() { throw HttpException{413, "Multipart request exceeds configured limits"}; }

struct Parameters {
    std::string type;
    std::unordered_map<std::string, std::string> values;
};
Parameters parameters(View input) {
    Parameters result;
    const auto semicolon = input.find(';');
    result.type = lower(trim(input.substr(0, semicolon)));
    if (semicolon == View::npos) return result;
    input.remove_prefix(semicolon + 1);
    while (!trim(input).empty()) {
        input = trim(input);
        const auto equals = input.find('=');
        if (equals == View::npos) malformed();
        const auto name = lower(trim(input.substr(0, equals)));
        if (!security::valid_header_name(name)) malformed();
        input = trim(input.substr(equals + 1));
        std::string value;
        if (input.starts_with('"')) {
            input.remove_prefix(1);
            bool closed = false;
            while (!input.empty()) {
                char c = input.front(); input.remove_prefix(1);
                if (c == '"') { closed = true; break; }
                if (c == '\\') {
                    if (input.empty()) malformed();
                    c = input.front(); input.remove_prefix(1);
                }
                if (static_cast<unsigned char>(c) < 32 || c == 127) malformed();
                value += c;
            }
            if (!closed) malformed();
            input = trim(input);
            if (!input.empty() && input.front() != ';') malformed();
        } else {
            const auto end = input.find(';');
            const auto token = trim(input.substr(0, end));
            if (token.empty() || !security::valid_header_name(token)) malformed();
            value = token;
            input = end == View::npos ? View{} : input.substr(end);
        }
        if (!result.values.emplace(name, std::move(value)).second) malformed();
        if (input.empty()) break;
        input.remove_prefix(1);
        if (trim(input).empty()) malformed();
    }
    return result;
}
View field_key(View name) { return name.ends_with("[]") ? name.substr(0, name.size() - 2) : name; }
bool text(View bytes) noexcept;
bool safe_name(View name) {
    return !name.empty() && name.size() <= 255 && text(name) && std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

struct Delimiter { std::size_t position; std::size_t next; bool last; };
std::optional<Delimiter> delimiter(View body, View marker, std::size_t start) {
    while (true) {
        const auto found = body.find(marker, start);
        if (found == View::npos) return {};
        const bool at_line_start = found == 0 || (found >= 2 && body.substr(found - 2, 2) == "\r\n");
        auto end = found + marker.size();
        const bool last = body.substr(end, 2) == "--";
        if (last) end += 2;
        while (end < body.size() && (body[end] == ' ' || body[end] == '\t')) ++end;
        if (at_line_start && (body.substr(end, 2) == "\r\n" || (last && end == body.size())))
            return Delimiter{found, end == body.size() ? end : end + 2, last};
        start = found + marker.size();
    }
}

std::uint32_t be32(View bytes, std::size_t at) noexcept {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at])) << 24) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 2])) << 8) |
        static_cast<unsigned char>(bytes[at + 3]);
}
std::uint32_t le32(View bytes, std::size_t at) noexcept {
    return static_cast<unsigned char>(bytes[at]) | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 2])) << 16) | (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 3])) << 24);
}
unsigned le16(View bytes, std::size_t at) noexcept { return static_cast<unsigned char>(bytes[at]) | (static_cast<unsigned>(static_cast<unsigned char>(bytes[at + 1])) << 8); }
unsigned be16(View bytes, std::size_t at) noexcept { return (static_cast<unsigned>(static_cast<unsigned char>(bytes[at])) << 8) | static_cast<unsigned char>(bytes[at + 1]); }
std::uint32_t crc32(View bytes) noexcept {
    std::uint32_t crc = 0xffffffffU;
    for (unsigned char byte : bytes) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
bool png(View bytes) noexcept {
    if (!bytes.starts_with(View{"\x89PNG\r\n\x1a\n", 8})) return false;
    std::size_t at = 8;
    bool header = false, data = false, data_ended = false;
    while (bytes.size() - at >= 12) {
        const auto length = be32(bytes, at);
        if (length > bytes.size() - at - 12) return false;
        const auto type = bytes.substr(at + 4, 4);
        if (crc32(bytes.substr(at + 4, length + 4)) != be32(bytes, at + 8 + length)) return false;
        if (!header) {
            if (type != "IHDR" || length != 13 || be32(bytes, at + 8) == 0 || be32(bytes, at + 12) == 0) return false;
            const auto depth = static_cast<unsigned char>(bytes[at + 16]), color = static_cast<unsigned char>(bytes[at + 17]);
            const bool depth_ok = color == 0 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16) :
                color == 3 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8) : (color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16);
            if (!depth_ok || bytes[at + 18] != 0 || bytes[at + 19] != 0 || static_cast<unsigned char>(bytes[at + 20]) > 1) return false;
            header = true;
        } else if (type == "IHDR") return false;
        else if (type == "IDAT") { if (data_ended) return false; data = data || length != 0; }
        else if (type == "IEND") return length == 0 && data && at + 12 == bytes.size();
        else if (data) data_ended = true;
        at += 12 + length;
    }
    return false;
}
bool gif(View bytes) noexcept {
    if (bytes.size() < 13 || (bytes.substr(0, 6) != "GIF87a" && bytes.substr(0, 6) != "GIF89a") || le16(bytes, 6) == 0 || le16(bytes, 8) == 0) return false;
    std::size_t at = 13;
    const auto packed = static_cast<unsigned char>(bytes[10]);
    if (packed & 128) at += 3U * (2U << (packed & 7));
    bool image = false;
    auto blocks = [&]() {
        while (at < bytes.size()) { const auto count = static_cast<unsigned char>(bytes[at++]); if (!count) return true; if (count > bytes.size() - at) return false; at += count; }
        return false;
    };
    while (at < bytes.size()) {
        const auto marker = static_cast<unsigned char>(bytes[at++]);
        if (marker == 0x3b) return image && at == bytes.size();
        if (marker == 0x21) { if (at == bytes.size()) return false; ++at; if (!blocks()) return false; }
        else if (marker == 0x2c) {
            if (bytes.size() - at < 10 || le16(bytes, at + 4) == 0 || le16(bytes, at + 6) == 0) return false;
            const auto flags = static_cast<unsigned char>(bytes[at + 8]); at += 9;
            if (flags & 128) at += 3U * (2U << (flags & 7));
            if (at >= bytes.size() || static_cast<unsigned char>(bytes[at]) < 2 || static_cast<unsigned char>(bytes[at]) > 8) return false;
            ++at; const auto first = at; if (!blocks() || at == first + 1) return false; image = true;
        } else return false;
    }
    return false;
}
bool jpeg(View bytes) noexcept {
    if (!bytes.starts_with(View{"\xff\xd8", 2})) return false;
    std::size_t at = 2;
    bool frame = false, scan = false;
    while (at < bytes.size()) {
        if (static_cast<unsigned char>(bytes[at++]) != 0xff) return false;
        while (at < bytes.size() && static_cast<unsigned char>(bytes[at]) == 0xff) ++at;
        if (at == bytes.size()) return false;
        const auto marker = static_cast<unsigned char>(bytes[at++]);
        if (marker == 0xd9) return frame && scan && at == bytes.size();
        if (marker == 0 || marker == 0xd8 || (marker >= 0xd0 && marker <= 0xd7)) return false;
        if (bytes.size() - at < 2) return false;
        const auto length = be16(bytes, at);
        if (length < 2 || length > bytes.size() - at) return false;
        const bool sof = marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc;
        if (sof) {
            if (length < 11 || be16(bytes, at + 3) == 0 || be16(bytes, at + 5) == 0 || static_cast<unsigned char>(bytes[at + 7]) == 0 || length != 8U + 3U * static_cast<unsigned char>(bytes[at + 7])) return false;
            frame = true;
        }
        if (marker == 0xda) {
            if (!frame || length < 8 || length != 6U + 2U * static_cast<unsigned char>(bytes[at + 2])) return false;
            scan = true; at += length;
            while (at < bytes.size()) {
                if (static_cast<unsigned char>(bytes[at]) != 0xff) { ++at; continue; }
                if (at + 1 == bytes.size()) return false;
                const auto next = static_cast<unsigned char>(bytes[at + 1]);
                if (next == 0 || (next >= 0xd0 && next <= 0xd7)) { at += 2; continue; }
                break;
            }
        } else at += length;
    }
    return false;
}
bool webp(View bytes) noexcept {
    if (bytes.size() < 20 || bytes.substr(0, 4) != "RIFF" || bytes.substr(8, 4) != "WEBP" || le32(bytes, 4) != bytes.size() - 8) return false;
    std::size_t at = 12;
    bool image = false;
    while (bytes.size() - at >= 8) {
        const auto tag = bytes.substr(at, 4); const auto size = le32(bytes, at + 4); at += 8;
        if (size > bytes.size() - at) return false;
        if (tag == "VP8 ") {
            if (size < 10 || (static_cast<unsigned char>(bytes[at]) & 1) || bytes.substr(at + 3, 3) != View{"\x9d\x01\x2a", 3} || !(le16(bytes, at + 6) & 0x3fff) || !(le16(bytes, at + 8) & 0x3fff)) return false;
            image = true;
        } else if (tag == "VP8L") { if (size < 5 || static_cast<unsigned char>(bytes[at]) != 0x2f || (static_cast<unsigned char>(bytes[at + 4]) & 0xe0)) return false; image = true; }
        else if (tag == "VP8X" && size != 10) return false;
        at += size;
        if (size & 1) { if (at >= bytes.size() || bytes[at] != 0) return false; ++at; }
    }
    return image && at == bytes.size();
}
bool text(View bytes) noexcept {
    // UTF-8 text, with the whitespace controls normally used in text files.
    for (std::size_t at = 0; at < bytes.size();) {
        const auto c = static_cast<unsigned char>(bytes[at++]);
        if (c < 128) { if ((c < 32 && c != '\t' && c != '\r' && c != '\n') || c == 127) return false; continue; }
        unsigned count = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 0;
        if (!count || count > bytes.size() - at) return false;
        const auto next = static_cast<unsigned char>(bytes[at]);
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) || (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)) return false;
        for (unsigned i = 0; i < count; ++i) if ((static_cast<unsigned char>(bytes[at++]) & 0xc0) != 0x80) return false;
    }
    return true;
}
} // namespace

MultipartInput parse_multipart(View content_type, View body, const MultipartLimits& limits) {
    if (body.size() > limits.body_bytes) too_large();
    const auto type = parameters(content_type);
    const auto boundary = type.values.find("boundary");
    if (type.type != "multipart/form-data" || boundary == type.values.end()) malformed();
    const auto& value = boundary->second;
    if (value.empty() || value.size() > 70 || value.back() == ' ' || !std::all_of(value.begin(), value.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || View{"'()+_,-./:=? "}.find(c) != View::npos; })) malformed();
    const auto marker = "--" + value;
    auto current = delimiter(body, marker, 0);
    if (!current) malformed();
    MultipartInput result;
    std::size_t parts = 0;
    while (!current->last) {
        if (++parts > limits.parts) too_large();
        const auto begin = current->next;
        const auto header_end = body.find("\r\n\r\n", begin);
        if (header_end == View::npos) malformed();
        if (header_end - begin > limits.header_bytes) too_large();
        std::unordered_map<std::string, std::string> headers;
        auto section = body.substr(begin, header_end - begin);
        while (!section.empty()) {
            const auto end = section.find("\r\n"); const auto line = section.substr(0, end);
            const auto colon = line.find(':');
            if (colon == View::npos || !security::valid_header_name(line.substr(0, colon)) || !security::valid_header_value(trim(line.substr(colon + 1)))) malformed();
            if (!headers.emplace(lower(line.substr(0, colon)), std::string{trim(line.substr(colon + 1))}).second) malformed();
            if (end == View::npos) break;
            section.remove_prefix(end + 2);
        }
        const auto disposition = headers.find("content-disposition");
        if (disposition == headers.end() || headers.contains("content-transfer-encoding")) malformed();
        const auto attributes = parameters(disposition->second);
        const auto name = attributes.values.find("name"), filename = attributes.values.find("filename");
        if (attributes.type != "form-data" || name == attributes.values.end() || !safe_name(name->second) || attributes.values.contains("filename*")) malformed();
        const auto key = field_key(name->second);
        if (key.empty() || key.find_first_of(".[]*") != View::npos) malformed();
        const auto data_start = header_end + 4;
        const auto next = delimiter(body, marker, data_start);
        if (!next || next->position < data_start + 2) malformed();
        const auto bytes = body.substr(data_start, next->position - data_start - 2);
        if (filename != attributes.values.end()) {
            if (bytes.size() > limits.file_bytes) too_large();
            if (result.fields.contains(std::string{key})) malformed();
            if (filename->second.empty()) { if (!bytes.empty()) malformed(); current = next; continue; }
            if (!safe_name(filename->second)) malformed();
            auto base = View{filename->second}; const auto slash = base.find_last_of("/\\");
            if (slash != View::npos) base.remove_prefix(slash + 1);
            if (base.empty() || base == "." || base == "..") malformed();
            auto media = headers.contains("content-type") ? parameters(headers.at("content-type")).type : "application/octet-stream";
            const auto slash_position = media.find('/');
            if (slash_position == std::string::npos || !security::valid_header_name(View{media}.substr(0,slash_position)) || !security::valid_header_name(View{media}.substr(slash_position + 1))) malformed();
            result.files.push_back({name->second, std::string{base}, std::move(media), std::string{bytes}});
        } else {
            if (bytes.size() > limits.field_bytes) too_large();
            if (name->second.ends_with("[]") || std::any_of(result.files.begin(), result.files.end(), [&](const auto& file) { return field_key(file.name) == key; })) malformed();
            result.fields.insert_or_assign(name->second, std::string{bytes});
        }
        current = next;
    }
    return result;
}

std::string_view detected_media_type(View bytes) noexcept {
    if (png(bytes)) return "image/png";
    if (jpeg(bytes)) return "image/jpeg";
    if (gif(bytes)) return "image/gif";
    if (webp(bytes)) return "image/webp";
    auto pdf = bytes;
    while (!pdf.empty() && (pdf.back() == ' ' || pdf.back() == '\t' || pdf.back() == '\r' || pdf.back() == '\n')) pdf.remove_suffix(1);
    if (bytes.size() >= 8 && bytes.starts_with("%PDF-") && bytes[5] >= '0' && bytes[5] <= '9' && bytes[6] == '.' && bytes[7] >= '0' && bytes[7] <= '9' && pdf.ends_with("%%EOF")) return "application/pdf";
    return text(bytes) ? "text/plain" : "application/octet-stream";
}
} // namespace gungnir::http
