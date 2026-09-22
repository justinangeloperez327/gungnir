#pragma once

#include <string>
#include <utility>
#include <vector>

namespace gungnir::mail {

struct Address {
    std::string email;
    std::string name;
};

class Message {
public:
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
    std::vector<Address> to_;
    std::vector<Address> cc_;
    std::vector<Address> bcc_;
    std::string subject_;
    std::string text_;
    std::string html_;
};

} // namespace gungnir::mail
