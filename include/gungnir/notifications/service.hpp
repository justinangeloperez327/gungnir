#pragma once

#include <gungnir/mail/delivery.hpp>
#include <gungnir/notifications/channel.hpp>
#include <gungnir/queue/service.hpp>
#include <algorithm>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <typeindex>
#include <variant>

namespace gungnir::notifications {
struct Target { String channel; String recipient; };
class Snapshot final : public Notification {
public:
    String type;
    std::vector<Target> targets;
    std::optional<mail::Message> message;
    std::optional<http::Json> data;
    std::string_view name() const noexcept override { return type; }
    std::vector<String> channels() const override {
        std::vector<String> result; for (const auto& target : targets) result.push_back(target.channel); return result;
    }
    std::optional<mail::Message> mail_message() const override { return message; }
    std::optional<http::Json> database_payload() const override { return data; }
    void validate() const {
        if (type.empty()) throw std::invalid_argument("Notification name cannot be empty");
        std::set<String> channels;
        for (const auto& target : targets) {
            if (target.channel.empty() || target.recipient.empty() || !channels.insert(target.channel).second)
                throw std::invalid_argument("Invalid notification channel or recipient");
            if (target.channel == "mail") {
                mail::delivery::address({target.recipient, {}});
                if (!message) throw std::invalid_argument("Notification does not define a mail message");
            }
            if (target.channel == "database" && !data) throw std::invalid_argument("Notification does not define a database payload");
        }
        if (message) mail::delivery::validate(*message);
    }
    http::Json encode() const {
        validate(); http::Json::Array routes;
        for (const auto& target : targets) routes.push_back(http::Json::object({{"channel", target.channel}, {"recipient", target.recipient}}));
        return http::Json::object({{"version", Int64{1}}, {"name", type}, {"targets", http::Json::array(std::move(routes))},
            {"mail", message ? mail::delivery::encode(*message) : http::Json{nullptr}},
            {"hasData", data.has_value()}, {"data", data.value_or(http::Json{nullptr})}});
    }
    static Snapshot read(std::string_view payload) {
        const auto value = http::Json::parse(payload); mail::delivery::version(value);
        Snapshot result; result.type = mail::delivery::string(value, "name");
        for (const auto& target : mail::delivery::array(value, "targets"))
            result.targets.push_back({mail::delivery::string(target, "channel"), mail::delivery::string(target, "recipient")});
        const auto& message = mail::delivery::field(value, "mail");
        if (!message.is_null()) result.message = mail::delivery::decode(message);
        const auto& has_data = mail::delivery::field(value, "hasData");
        if (!has_data.is_boolean()) throw std::invalid_argument("Invalid notification data flag");
        const auto& data = mail::delivery::field(value, "data");
        if (has_data == http::Json{true}) result.data = data;
        else if (!data.is_null()) throw std::invalid_argument("Invalid absent notification data");
        result.validate(); return result;
    }
};
class Delivery final : public queue::Job {
public:
    inline static constexpr std::string_view job_name = "gungnir.notification.delivery.v1";
    explicit Delivery(Snapshot snapshot) : snapshot_(std::move(snapshot)) { snapshot_.validate(); }
    std::string_view name() const noexcept override { return job_name; }
    String payload() const override { return snapshot_.encode().dump(); }
private:
    Snapshot snapshot_;
};
class Service {
    using Route = std::function<String(const void*)>;
    struct Routes { std::mutex mutex; std::map<std::pair<std::type_index, String>, Route> values; };
public:
    Service(const std::shared_ptr<Service>& owner) : Service(checked(owner)) {}
    Service(std::shared_ptr<Manager> manager, mail::Address sender = {}, std::shared_ptr<queue::Service> queue = {})
        : manager_(std::move(manager)), queue_(std::move(queue)), sender_(std::move(sender)), routes_(std::make_shared<Routes>()) {
        if (!manager_) throw std::invalid_argument("Notifications are not configured");
    }
    template<class Recipient, class Resolver> void route(String channel, Resolver resolver) {
        if (channel.empty()) throw std::invalid_argument("Notification route channel cannot be empty");
        std::lock_guard lock{routes_->mutex};
        routes_->values.insert_or_assign({std::type_index{typeid(Recipient)}, std::move(channel)},
            [resolver = std::move(resolver)](const void* value) { return String{std::invoke(resolver, *static_cast<const Recipient*>(value))}; });
    }
    template<class Recipient, class Source> void send(const Recipient& recipient, Source source) const { deliver(compose(recipient, std::move(source))); }
    template<class Recipient, class Source> String queue(const Recipient& recipient, Source source, Int64 attempts = 1, bool after_commit = true) const {
        if (!queue_) throw std::invalid_argument("Queued notifications require a queue adapter");
        return queue_->dispatch(Delivery{compose(recipient, std::move(source))}, attempts, after_commit);
    }
    void deliver(const Snapshot& value) const {
        value.validate(); configured(value.targets);
        for (const auto& target : value.targets) manager_->send_on(target.channel, target.recipient, value);
    }
private:
    static const Service& checked(const std::shared_ptr<Service>& owner) {
        if (!owner) throw std::invalid_argument("Notifications are not configured");
        return *owner;
    }
    void configured(const std::vector<Target>& targets) const {
        for (const auto& target : targets) if (!manager_->has_channel(target.channel))
            throw std::logic_error("Gungnir notification channel is not configured: " + target.channel);
    }
    template<class Value> static String identity(const Value& value) {
        return std::visit([](const auto& item) -> String {
            using T = std::remove_cvref_t<decltype(item)>;
            if constexpr (std::same_as<T, String>) return item;
            else if constexpr (std::integral<T> && !std::same_as<T, bool>) return std::to_string(item);
            else throw std::invalid_argument("Notification recipient requires a string or integer model key");
        }, value);
    }
    template<class Recipient> String address(const Recipient& recipient, const String& channel) const {
        Route override;
        {
            std::lock_guard lock{routes_->mutex};
            if (const auto found = routes_->values.find({std::type_index{typeid(Recipient)}, channel}); found != routes_->values.end()) override = found->second;
        }
        if (override) return override(&recipient);
        const auto value = channel == "mail" ? recipient.attribute_value("email") : recipient.primary_key_value();
        if (!value) throw std::invalid_argument("Notification recipient address is missing");
        if (channel == "mail") {
            if (const auto* email = std::get_if<String>(&*value)) return *email;
            throw std::invalid_argument("Mail notification recipient email must be a string");
        }
        return identity(*value);
    }
    template<class Recipient, class Source> Snapshot compose(const Recipient& recipient, Source source) const {
        auto bound = source.bind(recipient);
        Snapshot result; result.type = String{bound.name()};
        for (const auto& channel : bound.channels()) result.targets.push_back({channel, address(recipient, channel)});
        configured(result.targets);
        const bool needs_mail = std::any_of(result.targets.begin(), result.targets.end(), [](const auto& target) { return target.channel == "mail"; });
        if (needs_mail) {
            result.message = bound.mail_message();
            if (result.message) {
                if (result.message->sender().email.empty()) result.message->from(sender_);
                // Validate the selected route now; the native channel avoids
                // adding the same recipient again during delivery.
                auto validation = *result.message;
                for (const auto& target : result.targets) if (target.channel == "mail") validation.to({target.recipient, {}});
                mail::delivery::validate(validation);
                // Persist a complete message and deliver its envelope exactly once.
                result.message = std::move(validation);
            }
        }
        if (std::any_of(result.targets.begin(), result.targets.end(), [](const auto& target) { return target.channel != "mail"; }))
            result.data = bound.database_payload();
        result.validate(); return result;
    }
    std::shared_ptr<Manager> manager_;
    std::shared_ptr<queue::Service> queue_;
    mail::Address sender_;
    std::shared_ptr<Routes> routes_;
};
}
