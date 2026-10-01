#pragma once

#include <string>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace gungnir::mail {

struct Address {
    std::string email;
    std::string name;
};

struct Attachment { std::string filename; std::string contents; std::string content_type{"application/octet-stream"}; };

class Message {
public:
    Message& reply_to(Address value) { reply_to_ = std::move(value); return *this; }
    Message& attach(Attachment value) {
        if (value.filename.empty() || value.filename.find_first_of("\r\n\0", 0, 3) != std::string::npos || value.content_type.find_first_of("\r\n") != std::string::npos)
            throw std::invalid_argument("Invalid attachment name or content type");
        attachments_.push_back(std::move(value)); return *this;
    }
    [[nodiscard]] const std::optional<Address>& reply_address() const noexcept { return reply_to_; }
    [[nodiscard]] const std::vector<Attachment>& attachments() const noexcept { return attachments_; }
    Message& from(Address value) { from_ = std::move(value); return *this; }
    Message& to(Address value) { to_.push_back(std::move(value)); return *this; }
    Message& cc(Address value) { cc_.push_back(std::move(value)); return *this; }
    Message& bcc(Address value) { bcc_.push_back(std::move(value)); return *this; }
    Message& subject(std::string value) { subject_ = std::move(value); return *this; }
    Message& text(std::string value) { text_ = std::move(value); return *this; }
    Message& html(std::string value) { html_ = std::move(value); return *this; }

    [[nodiscard]] const Address& sender() const noexcept { return from_; }
    [[nodiscard]] const std::vector<Address>& recipients() const noexcept { return to_; }
    [[nodiscard]] const std::vector<Address>& cc_recipients() const noexcept { return cc_; }
    [[nodiscard]] const std::vector<Address>& bcc_recipients() const noexcept { return bcc_; }
    [[nodiscard]] const std::string& subject_line() const noexcept { return subject_; }
    [[nodiscard]] const std::string& text_body() const noexcept { return text_; }
    [[nodiscard]] const std::string& html_body() const noexcept { return html_; }

private:
    Address from_;
    std::optional<Address> reply_to_;
    std::vector<Attachment> attachments_;
    std::vector<Address> to_;
    std::vector<Address> cc_;
    std::vector<Address> bcc_;
    std::string subject_;
    std::string text_;
    std::string html_;
};

} // namespace gungnir::mail
