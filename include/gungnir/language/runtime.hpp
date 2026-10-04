#pragma once
#include <gungnir/gungnir.hpp>
#include <gungnir/language/spec.hpp>
#include <gungnir/orm/orm.hpp>
#include <gungnir/core/services.hpp>
#include <gungnir/queue/job.hpp>
#include <gungnir/queue/worker.hpp>
#include <charconv>
#include <gungnir/auth/resource_authorization.hpp>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <vector>
namespace gungnir::language::runtime {
inline Json validate(Request& request, const Json& definitions) {
    if (!definitions.is_object()) throw std::invalid_argument("Validation rules must be an object");
    validation::Rules rules;
    for (const auto& [field, expression] : definitions.as_object()) {
        if (!expression.is_string()) throw std::invalid_argument("Validation rule expressions must be strings");
        rules.add(field, expression.string());
    }
    return request.validate_structured(rules);
}
template<class Resource> void authorize(Request& request,String ability,const Resource& resource) {
    auto authorization = request.services().resolve<auth::ResourceAuthorization>();
    authorization->authorize(request,ability,resource);
}
template<class T> auto hold_receiver(T& value) { return std::ref(value); }
template<class T> auto hold_receiver(T&& value) { return std::forward<T>(value); }
template<class T> T& receiver(std::reference_wrapper<T>& value) { return value.get(); }
template<class T> T& receiver(T& value) { return value; }
template<auto Member, class Parent> auto relationship_query(const Parent& parent) {
    return orm::relation_query(parent, parent.*Member);
}
template<class Relation> auto relationship_value(const Relation& relation) {
    using Related = typename Relation::related_type;
    const auto value = relation.value();
    return value ? std::optional<Related>{*value} : std::nullopt;
}
template<class Range> auto attribute_values(const Range& values) {
    std::vector<model::AttributeValue> result;
    result.reserve(values.size());
    for (const auto& value : values) result.push_back(model::to_value(value));
    return result;
}
template<auto Member, class Parent, class Keys> std::size_t relationship_attach(Parent& parent, const Keys& keys) {
    if constexpr (!std::same_as<Keys, String> && requires { keys.begin(); keys.end(); })
        return orm::attach(parent, parent.*Member, attribute_values(keys));
    else return orm::attach(parent, parent.*Member, model::to_value(keys));
}
template<auto Member, class Parent> std::size_t relationship_detach(Parent& parent) {
    return orm::detach(parent, parent.*Member);
}
template<auto Member, class Parent, class Keys> std::size_t relationship_detach(Parent& parent, const Keys& keys) {
    if constexpr (!std::same_as<Keys, String> && requires { keys.begin(); keys.end(); })
        return orm::detach(parent, parent.*Member, attribute_values(keys));
    else return orm::detach(parent, parent.*Member, model::to_value(keys));
}
template<class Model, class Key> auto query_find(orm::Query<Model> query, const Key& key) {
    return query.where_key(model::to_value(key)).first();
}
template<class Model, class Key> auto query_find_or_fail(orm::Query<Model> query, const Key& key) {
    return query.where_key(model::to_value(key)).first_or_fail();
}
inline orm::SortDirection sort_direction(std::string_view direction) {
    if (direction == "asc") return orm::SortDirection::asc;
    if (direction == "desc") return orm::SortDirection::desc;
    throw std::invalid_argument("Order direction must be asc or desc");
}
inline orm::Comparison comparison(std::string_view value) {
    if (value == "=" || value == "==") return orm::Comparison::equal;
    if (value == "!=" || value == "<>") return orm::Comparison::not_equal;
    if (value == "<") return orm::Comparison::less_than;
    if (value == "<=") return orm::Comparison::less_or_equal;
    if (value == ">") return orm::Comparison::greater_than;
    if (value == ">=") return orm::Comparison::greater_or_equal;
    if (value == "like") return orm::Comparison::like;
    throw std::invalid_argument("Unsupported ORM comparison");
}
inline std::size_t query_size(Int64 value, bool positive = false) {
    if (value < 0 || (positive && value == 0) || !std::in_range<std::size_t>(value))
        throw std::invalid_argument("Query size is out of range");
    return static_cast<std::size_t>(value);
}
inline Json lookup(const Json& object, std::string_view key) { const auto* value = object.get(key); if (!value) throw std::out_of_range("Missing JSON key: " + String{key}); return *value; }
inline Response text(String body, int status = 200) { return Response::text(std::move(body),status); }
inline Response html(String body, int status = 200) { return Response::html(std::move(body),status); }
template<class T> Response json(T&& value, int status = 200) { return Response::json(std::forward<T>(value),status); }
inline Response redirect(String location, int status = 302) { return Response::redirect(std::move(location),status); }
inline Response response(String body, int status = 200) { return Response{status,std::move(body)}; }
inline Response no_content() { return Response::no_content(); }
inline Response download(String body, String filename, String content_type = "application/octet-stream", int status = 200) {
    return Response::download(std::move(body), std::move(filename), std::move(content_type), status);
}
inline view::Value view_value(const Json& value) {
    if (value.is_null()) return nullptr;
    if (value.is_array()) { std::vector<view::Value> items; for (const auto& item : value.as_array()) items.push_back(view_value(item)); return view::Value::array(std::move(items)); }
    if (value.is_object()) { std::unordered_map<String,view::Value> items; for (const auto& [key,item] : value.as_object()) items.emplace(key,view_value(item)); return view::Value::object(std::move(items)); }
    if (value.is_boolean()) return value.string() == "true";
    if (value.is_integer()) return static_cast<Int64>(std::stoll(value.string()));
    if (value.is_number()) return std::stod(value.string());
    return value.string();
}
inline Response view(String name, const Json& data = Json::object({}), int status = 200) {
    if (!data.is_object()) throw std::invalid_argument("View data must be an object");
    gungnir::view::Data values; for (const auto& [key,item] : data.as_object()) values.with(key,view_value(item));
    return Response::view(std::move(name),std::move(values),status);
}
template<class Range,class Fn> auto map(const Range& range, Fn fn) { using T = std::remove_cvref_t<decltype(fn(*std::begin(range)))>; std::vector<T> out; for (const auto& item : range) out.push_back(fn(item)); return out; }
template<class Range,class Fn> auto filter(const Range& range, Fn fn) { using T = std::remove_cvref_t<decltype(*std::begin(range))>; std::vector<T> out; for (const auto& item : range) if (fn(item)) out.push_back(item); return out; }
template<class Range,class Fn> void each(const Range& range, Fn fn) { for (const auto& item : range) fn(item); }
template<class T> T decode(const Json& value) {
    if constexpr (std::same_as<T,Json>) return value;
    else if constexpr (model::is_optional_v<T>) { if (value.is_null()) return std::nullopt; return T{decode<typename model::is_optional<T>::value_type>(value)}; }
    else if constexpr (std::same_as<T,model::Decimal>) { if (!value.is_string()) throw std::invalid_argument("Exact decimal payload must be a string"); return model::Decimal{value.string()}; }
    else if constexpr (std::same_as<T,String>) { if (!value.is_string()) throw std::invalid_argument("Expected string payload"); return value.string(); }
    else if constexpr (std::same_as<T,bool>) { if (!value.is_boolean()) throw std::invalid_argument("Expected bool payload"); return value.string() == "true"; }
    else if constexpr (std::integral<T>) { if (!value.is_integer()) throw std::invalid_argument("Expected integer payload"); T result{}; const auto text = value.string(); auto [end,error] = std::from_chars(text.data(),text.data()+text.size(),result); if (error != std::errc{} || end != text.data()+text.size()) throw std::invalid_argument("Invalid integer payload"); return result; }
    else if constexpr (std::floating_point<T>) { if (!value.is_number()) throw std::invalid_argument("Expected numeric payload"); return static_cast<T>(std::stod(value.string())); }
    else if constexpr (requires { T::from_payload(value.dump()); }) return T::from_payload(value.dump());
    else if constexpr (requires { typename T::mapped_type; typename T::key_type; }) { if (!value.is_object()) throw std::invalid_argument("Expected map payload"); T result; for (const auto& [key,item] : value.as_object()) result.emplace(key,decode<typename T::mapped_type>(item)); return result; }
    else if constexpr (requires(T t) { t.attributes(); }) { if (!value.is_object()) throw std::invalid_argument("Expected model payload"); T result; model::for_each_attribute<T>([&](const auto& descriptor) { auto& field = result.*descriptor.member; using FieldType = typename std::remove_cvref_t<decltype(field)>::value_type; if (const auto* item = value.get(descriptor.name)) field = decode<FieldType>(*item); }); return result; }
    else { T result; for (const auto& item : value.as_array()) result.push_back(decode<typename T::value_type>(item)); return result; }
}
template<class T> T required(const Json& value, std::string_view key) { const auto* item = value.get(key); if (!item) throw std::invalid_argument("Missing job payload field: " + String{key}); return decode<T>(*item); }
inline model::AttributeMap attributes(const Json& value) {
    if (!value.is_object()) throw std::invalid_argument("Model attributes must be an object");
    model::AttributeMap result;
    for (const auto& [key,item] : value.as_object()) {
        if (item.is_null()) result[key] = nullptr;
        else if (item.is_boolean()) result[key] = item.string() == "true";
        else if (item.is_integer()) result[key] = decode<Int64>(item);
        else if (item.is_number()) result[key] = decode<double>(item);
        else if (item.is_string()) result[key] = item.string();
        else throw std::invalid_argument("Nested model attributes require an explicit cast");
    }
    return result;
}
template<class Model> model::AttributeMap attributes(const Json& value) {
    if (!value.is_object()) throw std::invalid_argument("Model attributes must be an object");
    model::AttributeMap result;
    for (const auto& [key, item] : value.as_object()) {
        bool structured = false, nullable = false;
        model::for_each_attribute<Model>([&](const auto& descriptor) {
            if (descriptor.name != key) return;
            using Field = std::remove_cvref_t<decltype(std::declval<Model>().*descriptor.member)>;
            using Value = typename Field::value_type;
            if constexpr (model::is_optional_v<Value>) {
                structured = std::same_as<typename model::is_optional<Value>::value_type, Json>;
                nullable = true;
            } else structured = std::same_as<Value, Json>;
        });
        if (structured && !(nullable && item.is_null())) result[key] = item.dump();
        else {
            auto scalar = attributes(Json::object({{key, item}}));
            result.emplace(key, std::move(scalar.at(key)));
        }
    }
    return result;
}
namespace detail {
struct WaitState { std::mutex mutex; std::condition_variable completed; bool ready = false; std::exception_ptr error; };
struct Waiter {
    struct promise_type {
        WaitState& state;
        promise_type(WaitState& state, Task<void>&) : state(state) {}
        Waiter get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        struct Final {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> frame) const noexcept { auto& state = frame.promise().state; std::lock_guard lock{state.mutex}; state.ready = true; state.completed.notify_one(); }
            void await_resume() const noexcept {}
        };
        Final final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept { state.error = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> frame;
    ~Waiter() { frame.destroy(); }
};
inline Waiter complete(WaitState& state, Task<void>& task) { co_await task; }
}
// Used by synchronous queue workers. Async event dispatch remains nonblocking.
inline void wait(Task<void> task) { detail::WaitState state; auto waiter = detail::complete(state,task); waiter.frame.resume(); std::unique_lock lock{state.mutex}; state.completed.wait(lock,[&]{return state.ready;}); if (state.error) std::rethrow_exception(state.error); }

template<class Source,class Recipient> class BoundNotification final : public notifications::Notification {
    mutable Source source_; Recipient recipient_;
public:
    BoundNotification(Source source, Recipient recipient) : source_(std::move(source)),recipient_(std::move(recipient)) {}
    std::string_view name() const noexcept override { return Source::notification_name; }
    std::vector<std::string> channels() const override { return source_.via(recipient_); }
    auto to_mail() const requires requires { source_.toMail(recipient_); } { return source_.toMail(recipient_); }
    auto to_database() const requires requires { source_.toDatabase(recipient_); } { return source_.toDatabase(recipient_); }
    std::optional<mail::Message> mail_message() const override {
        if constexpr (requires { source_.toMail(recipient_); }) {
            auto value = source_.toMail(recipient_);
            if constexpr (requires { value.message(); }) return value.message();
            else return value;
        }
        return std::nullopt;
    }
    std::optional<Json> database_payload() const override {
        if constexpr (requires { source_.toDatabase(recipient_); }) return http::make_json(source_.toDatabase(recipient_));
        return std::nullopt;
    }
};

}
