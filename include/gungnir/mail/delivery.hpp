#pragma once

#include <gungnir/http/json.hpp>
#include <gungnir/mail/message.hpp>
#include <gungnir/queue/job.hpp>
#include <cctype>
#include <limits>
#include <string_view>

namespace gungnir::mail::delivery {
inline void header(std::string_view value) {
    for (unsigned char byte : value)
        if (byte < 0x20 || byte == 0x7f) throw std::invalid_argument("Invalid mail header characters");
}
inline void address(const Address& value) {
    const auto at = value.email.find('@');
    if (at == std::string::npos || at == 0 || at + 1 == value.email.size() || value.email.find('@', at + 1) != std::string::npos)
        throw std::invalid_argument("Invalid mail address");
    for (unsigned char byte : value.email)
        if (byte <= 0x20 || byte >= 0x7f || byte == '<' || byte == '>' || byte == ',' || byte == ';')
            throw std::invalid_argument("Invalid mail address");
    header(value.name);
}
inline void validate(const Message& value) {
    if (value.sender().email.empty()) throw std::invalid_argument("Mail sender is not configured");
    address(value.sender()); header(value.subject_line());
    if (value.recipients().empty() && value.cc_recipients().empty() && value.bcc_recipients().empty())
        throw std::invalid_argument("Mail requires at least one recipient");
    for (const auto* list : {&value.recipients(), &value.cc_recipients(), &value.bcc_recipients()})
        for (const auto& item : *list) address(item);
    if (value.reply_address()) address(*value.reply_address());
    for (const auto& item : value.attachments()) {
        if (item.filename.empty() || item.content_type.empty()) throw std::invalid_argument("Invalid mail attachment");
        header(item.filename); header(item.content_type);
    }
}
inline const http::Json& field(const http::Json& value, std::string_view key) {
    if (!value.is_object()) throw std::invalid_argument("Invalid delivery payload object");
    const auto* result = value.get(key);
    if (!result) throw std::invalid_argument("Incomplete delivery payload");
    return *result;
}
inline std::string string(const http::Json& value, std::string_view key) {
    const auto& result = field(value, key);
    if (!result.is_string()) throw std::invalid_argument("Invalid delivery payload string");
    return result.string();
}
inline const http::Json::Array& array(const http::Json& value, std::string_view key) {
    const auto& result = field(value, key);
    if (!result.is_array()) throw std::invalid_argument("Invalid delivery payload array");
    return result.as_array();
}
inline http::Json encode(const Address& value) {
    return http::Json::object({{"email", value.email}, {"name", value.name}});
}
inline Address decode_address(const http::Json& value) {
    Address result{string(value, "email"), string(value, "name")}; address(result); return result;
}
// Binary attachment bytes are hexadecimal, so the envelope remains valid UTF-8 JSON.
inline std::string hex(std::string_view bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    if (bytes.size() > std::string{}.max_size() / 2) throw std::length_error("Attachment is too large");
    std::string result; result.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
inline std::string unhex(std::string_view value) {
    if (value.size() % 2) throw std::invalid_argument("Invalid attachment encoding");
    auto digit = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        throw std::invalid_argument("Invalid attachment encoding");
    };
    std::string result; result.reserve(value.size() / 2);
    for (std::size_t i = 0; i < value.size(); i += 2) result += static_cast<char>((digit(value[i]) << 4) | digit(value[i + 1]));
    return result;
}
inline http::Json encode(const Message& value) {
    validate(value);
    auto addresses = [](const auto& source) { http::Json::Array result; for (const auto& item : source) result.push_back(encode(item)); return http::Json::array(std::move(result)); };
    http::Json::Array attachments;
    for (const auto& item : value.attachments()) attachments.push_back(http::Json::object({
        {"filename", item.filename}, {"contentType", item.content_type}, {"hex", hex(item.contents)}}));
    return http::Json::object({{"from", encode(value.sender())}, {"to", addresses(value.recipients())},
        {"cc", addresses(value.cc_recipients())}, {"bcc", addresses(value.bcc_recipients())},
        {"replyTo", value.reply_address() ? encode(*value.reply_address()) : http::Json{nullptr}},
        {"subject", value.subject_line()}, {"text", value.text_body()}, {"html", value.html_body()},
        {"attachments", http::Json::array(std::move(attachments))}});
}
inline Message decode(const http::Json& value) {
    Message result;
    result.from(decode_address(field(value, "from"))).subject(string(value, "subject"))
        .text(string(value, "text")).html(string(value, "html"));
    for (const auto& item : array(value, "to")) result.to(decode_address(item));
    for (const auto& item : array(value, "cc")) result.cc(decode_address(item));
    for (const auto& item : array(value, "bcc")) result.bcc(decode_address(item));
    const auto& reply = field(value, "replyTo");
    if (!reply.is_null()) result.reply_to(decode_address(reply));
    for (const auto& item : array(value, "attachments"))
        result.attach({string(item, "filename"), unhex(string(item, "hex")), string(item, "contentType")});
    validate(result); return result;
}
inline void version(const http::Json& value) {
    const auto& tag = field(value, "version");
    if (!tag.is_number() || tag.dump() != "1") throw std::invalid_argument("Unsupported delivery payload version");
}
class Job final : public queue::Job {
public:
    inline static constexpr std::string_view job_name = "gungnir.mail.delivery.v1";
    explicit Job(Message value) : value_(std::move(value)) { validate(value_); }
    std::string_view name() const noexcept override { return job_name; }
    std::string payload() const override { return http::Json::object({{"version", Int64{1}}, {"message", encode(value_)}}).dump(); }
    static Message read(std::string_view payload) { const auto value = http::Json::parse(payload); version(value); return decode(field(value, "message")); }
private:
    Message value_;
};
}
