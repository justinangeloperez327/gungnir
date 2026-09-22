#include <gungnir/http/request.hpp>

#include <gungnir/validation/validator.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace gungnir::http {

namespace {

std::string normalize_header_name(std::string_view name) {
    std::string normalized{name};

    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );

    return normalized;
}

std::string_view trim(std::string_view value) {
    while (
        !value.empty() &&
        (value.front() == ' ' || value.front() == '\t')
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        (value.back() == ' ' || value.back() == '\t')
    ) {
        value.remove_suffix(1);
    }

    return value;
}

int hex_value(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }

    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }

    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }

    return -1;
}

std::string decode_component(std::string_view value) {
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        const char character = value[index];

        if (character == '+') {
            output.push_back(' ');
            continue;
        }

        if (character == '%' && index + 2 < value.size()) {
            const auto high = hex_value(value[index + 1]);
            const auto low = hex_value(value[index + 2]);

            if (high >= 0 && low >= 0) {
                output.push_back(
                    static_cast<char>((high << 4) | low)
                );
                index += 2;
                continue;
            }
        }

        output.push_back(character);
    }

    return output;
}

Request::Input parse_urlencoded(std::string_view source) {
    Request::Input values;
    std::size_t cursor = 0;

    while (cursor <= source.size()) {
        const auto separator = source.find('&', cursor);
        const auto end = separator == std::string_view::npos
            ? source.size()
            : separator;

        const auto pair = source.substr(cursor, end - cursor);

        if (!pair.empty()) {
            const auto equals = pair.find('=');
            const auto key = equals == std::string_view::npos
                ? pair
                : pair.substr(0, equals);
            const auto value = equals == std::string_view::npos
                ? std::string_view{}
                : pair.substr(equals + 1);

            values.insert_or_assign(
                decode_component(key),
                decode_component(value)
            );
        }

        if (separator == std::string_view::npos) {
            break;
        }

        cursor = separator + 1;
    }

    return values;
}

bool media_type_is(
    std::string_view content_type,
    std::string_view expected
) {
    const auto semicolon = content_type.find(';');
    const auto media_type = trim(
        semicolon == std::string_view::npos
            ? content_type
            : content_type.substr(0, semicolon)
    );

    if (media_type.size() != expected.size()) {
        return false;
    }

    for (std::size_t index = 0; index < media_type.size(); ++index) {
        if (
            std::tolower(
                static_cast<unsigned char>(media_type[index])
            ) !=
            std::tolower(
                static_cast<unsigned char>(expected[index])
            )
        ) {
            return false;
        }
    }

    return true;
}

} // namespace

Request::Request(
    Method method,
    std::string target,
    std::string body
)
    : method_(method),
      target_(std::move(target)),
      body_(std::move(body)) {
    parse_target();
}

Method Request::method() const noexcept {
    return method_;
}

std::string_view Request::target() const noexcept {
    return target_;
}

std::string_view Request::path() const noexcept {
    return path_;
}

std::string_view Request::body() const noexcept {
    return body_;
}

void Request::set_header(std::string name, std::string value) {
    headers_.insert_or_assign(
        normalize_header_name(name),
        std::move(value)
    );

    body_parsed_ = false;
    cookies_parsed_ = false;
    form_.clear();
    cookies_.clear();
    json_.reset();
}

std::string_view Request::header(
    std::string_view name
) const noexcept {
    const auto found = headers_.find(
        normalize_header_name(name)
    );

    if (found == headers_.end()) {
        return {};
    }

    return found->second;
}

const Request::Headers& Request::headers() const noexcept {
    return headers_;
}

std::string_view Request::parameter(
    std::string_view name
) const noexcept {
    const auto found = parameters_.find(std::string{name});

    if (found == parameters_.end()) {
        return {};
    }

    return found->second;
}

bool Request::has_parameter(
    std::string_view name
) const noexcept {
    return parameters_.contains(std::string{name});
}

const Request::Parameters& Request::parameters() const noexcept {
    return parameters_;
}

std::string_view Request::query(
    std::string_view name
) const noexcept {
    const auto found = query_.find(std::string{name});

    if (found == query_.end()) {
        return {};
    }

    return found->second;
}

const Request::Input& Request::query() const noexcept {
    return query_;
}

std::string_view Request::cookie(
    std::string_view name
) const noexcept {
    parse_cookies();

    const auto found = cookies_.find(std::string{name});

    if (found == cookies_.end()) {
        return {};
    }

    return found->second;
}

const Request::Input& Request::cookies() const noexcept {
    parse_cookies();
    return cookies_;
}

const Json& Request::json() const {
    parse_body_input();

    if (!json_) {
        throw std::logic_error("Request body is not JSON");
    }

    return *json_;
}

const Request::Input& Request::form() const noexcept {
    try {
        parse_body_input();
    } catch (...) {
        form_.clear();
        body_parsed_ = true;
    }

    return form_;
}

bool Request::expects_json() const noexcept {
    const auto accept = header("accept");

    return
        accept.find("application/json") !=
            std::string_view::npos ||
        accept.find("+json") !=
            std::string_view::npos;
}

bool Request::is_json() const noexcept {
    return media_type_is(header("content-type"), "application/json");
}

bool Request::accepts(std::string_view media_type) const noexcept {
    const auto accept = header("accept");
    return accept.empty() || accept.find("*/*") != std::string_view::npos || accept.find(media_type) != std::string_view::npos;
}

std::string_view Request::content_type() const noexcept { return header("content-type"); }
std::string_view Request::user_agent() const noexcept { return header("user-agent"); }
std::string_view Request::host() const noexcept { return header("host"); }
std::string_view Request::authorization() const noexcept { return header("authorization"); }
bool Request::bearer_authenticated() const noexcept { return authorization().starts_with("Bearer "); }
std::string_view Request::bearer_token() const noexcept {
    const auto value = authorization();
    return value.starts_with("Bearer ") ? value.substr(7) : std::string_view{};
}

std::string Request::input(std::string_view name) const {
    parse_body_input();

    if (json_ && json_->is_object()) {
        if (const auto* value = json_->get(name)) {
            return value->string();
        }
    }

    const auto form_value = form_.find(std::string{name});

    if (form_value != form_.end()) {
        return form_value->second;
    }

    const auto query_value = query_.find(std::string{name});

    if (query_value != query_.end()) {
        return query_value->second;
    }

    return {};
}

bool Request::has(std::string_view name) const {
    parse_body_input();

    return
        (
            json_ &&
            json_->is_object() &&
            json_->get(name) != nullptr
        ) ||
        form_.contains(std::string{name}) ||
        query_.contains(std::string{name});
}

Request::Input Request::all() const {
    parse_body_input();

    Input values = query_;

    for (const auto& [name, value] : form_) {
        values.insert_or_assign(name, value);
    }

    if (json_ && json_->is_object()) {
        for (const auto& [name, value] : json_->as_object()) {
            values.insert_or_assign(name, value.string());
        }
    }

    return values;
}

Request::Input Request::only(
    std::initializer_list<std::string_view> names
) const {
    const auto values = all();
    Input selected;

    for (const auto name : names) {
        const auto found = values.find(std::string{name});

        if (found != values.end()) {
            selected.insert_or_assign(
                found->first,
                found->second
            );
        }
    }

    return selected;
}

Request::Input Request::except(
    std::initializer_list<std::string_view> names
) const {
    auto values = all();

    for (const auto name : names) {
        values.erase(std::string{name});
    }

    return values;
}

Request::Input Request::validate(
    const validation::Rules& rules
) const {
    return validation::Validator::validate(
        all(),
        rules
    );
}

void Request::parse_target() {
    const auto query = target_.find('?');

    path_ = query == std::string::npos
        ? target_
        : target_.substr(0, query);

    if (path_.empty()) {
        path_ = "/";
    }

    query_.clear();

    if (
        query != std::string::npos &&
        query + 1 < target_.size()
    ) {
        query_ = parse_urlencoded(
            std::string_view{target_}.substr(query + 1)
        );
    }
}

void Request::parse_body_input() const {
    if (body_parsed_) {
        return;
    }

    body_parsed_ = true;
    form_.clear();
    json_.reset();

    const auto content_type = header("content-type");

    if (media_type_is(content_type, "application/json")) {
        if (!body_.empty()) {
            json_ = Json::parse(body_);
        }

        return;
    }

    if (
        media_type_is(
            content_type,
            "application/x-www-form-urlencoded"
        )
    ) {
        form_ = parse_urlencoded(body_);
    }
}

void Request::parse_cookies() const {
    if (cookies_parsed_) {
        return;
    }

    cookies_parsed_ = true;
    cookies_.clear();

    const auto source = header("cookie");
    std::size_t cursor = 0;

    while (cursor < source.size()) {
        const auto separator = source.find(';', cursor);
        const auto end = separator == std::string_view::npos
            ? source.size()
            : separator;

        const auto pair = trim(source.substr(cursor, end - cursor));
        const auto equals = pair.find('=');

        if (equals != std::string_view::npos) {
            cookies_.insert_or_assign(
                std::string{
                    trim(pair.substr(0, equals))
                },
                decode_component(
                    trim(pair.substr(equals + 1))
                )
            );
        }

        if (separator == std::string_view::npos) {
            break;
        }

        cursor = separator + 1;
    }
}

void Request::clear_route_parameters() noexcept {
    parameters_.clear();
}

void Request::set_route_parameter(
    std::string name,
    std::string value
) {
    parameters_.insert_or_assign(
        std::move(name),
        std::move(value)
    );
}

} // namespace gungnir::http
