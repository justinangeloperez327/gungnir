#pragma once
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <gungnir/auth/session.hpp>
#include <gungnir/auth/password.hpp>
#include <gungnir/auth/context.hpp>
#include <gungnir/http/request.hpp>
#include <gungnir/http/response.hpp>
#include <gungnir/session/session.hpp>

namespace gungnir::auth {
struct PasswordIdentity { Identity identity; std::string password_hash; };
using PasswordResolver = std::function<std::optional<PasswordIdentity>(std::string_view)>;

class RememberStore {
public:
    virtual ~RememberStore() = default;
    virtual void put(std::string digest, std::string identity, std::chrono::system_clock::time_point expires) = 0;
    // Atomically consume a token so replay cannot authenticate twice.
    virtual std::optional<std::string> consume(std::string_view digest) = 0;
    virtual void revoke(std::string_view digest) = 0;
};
class MemoryRememberStore final : public RememberStore {
    struct Entry { std::string identity; std::chrono::system_clock::time_point expires; };
    std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
public:
    void put(std::string digest, std::string identity, std::chrono::system_clock::time_point expires) override {
        std::lock_guard lock{mutex_};
        const auto now = std::chrono::system_clock::now();
        std::erase_if(entries_, [&](const auto& item) { return item.second.expires <= now; });
        entries_.insert_or_assign(std::move(digest), Entry{std::move(identity), expires});
    }
    std::optional<std::string> consume(std::string_view digest) override {
        std::lock_guard lock{mutex_};
        auto found = entries_.find(std::string{digest});
        if (found == entries_.end()) return std::nullopt;
        auto entry = std::move(found->second); entries_.erase(found);
        if (entry.expires <= std::chrono::system_clock::now()) return std::nullopt;
        return std::move(entry.identity);
    }
    void revoke(std::string_view digest) override { std::lock_guard lock{mutex_}; entries_.erase(std::string{digest}); }
};

struct LoginOptions {
    SessionOptions session;
    std::string remember_cookie{"gungnir_remember"};
    std::chrono::seconds remember_for{std::chrono::hours{24 * 30}};
    bool secure{true};
};
// Use after session and AuthenticateSession middleware. Password hashes and
// remember digests stay in providers/stores; they are never exposed as Identity attributes.
class SessionGuard {
public:
    SessionGuard(PasswordResolver credentials, SessionIdentityResolver identities,
        std::shared_ptr<RememberStore> remember = {}, LoginOptions options = {})
        : credentials_(std::move(credentials)), identities_(std::move(identities)), remember_(std::move(remember)), options_(std::move(options)) {
        if (!credentials_ || !identities_ || options_.remember_for.count() <= 0)
            throw std::invalid_argument("SessionGuard requires identity providers and a positive remember lifetime");
        (void)http::serialize_cookie(http::Cookie{.name=options_.remember_cookie, .value="", .secure=options_.secure});
    }
    bool attempt(http::Request& request, http::Response& response, std::string_view login,
        std::string_view password, bool remember = false) const {
        auto account = credentials_(login);
        // Use a real dummy hash for an unknown account to avoid a cheap timing oracle.
        static const auto dummy = Password::hash(security::random_token());
        const bool valid = Password::verify(password, account ? account->password_hash : dummy);
        if (!account || !valid) return false;
        this->login(request, response, account->identity, remember);
        return true;
    }
    bool attempt(http::Request& request, std::string_view login,
        std::string_view password, bool remember = false) const {
        require_context(request);
        http::Response response;
        const bool result = attempt(request, response, login, password, remember);
        stage(request, response);
        return result;
    }
    void login(http::Request& request, http::Response& response, Identity identity, bool remember = false) const {
        if (!request.has_session()) throw std::logic_error("Login requires session middleware");
        if (identity.id.empty()) throw std::invalid_argument("Login identity cannot be empty");
        if (remember && !remember_) throw std::logic_error("Remember login requires a token store");
        std::string token, digest;
        if (remember) {
            const auto now = std::chrono::system_clock::now();
            if (options_.remember_for > std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::time_point::max() - now))
                throw std::invalid_argument("Remember lifetime is out of range");
            token = security::random_token(); digest = Password::token_digest(token);
            // A persistent-store failure must happen before authenticated state
            // is installed in the request or session.
            remember_->put(digest, identity.id, now + options_.remember_for);
        }
        revoke(request);
        request.session().regenerate();
        request.session().put(options_.session.key, identity.id);
        request.session().forget("_gungnir_csrf_token");
        if (!request.has_auth()) request.attach_auth(std::make_shared<Context>());
        request.auth().login(identity);
        if (remember) issue(request, response, std::move(token), std::move(digest));
        else expire(response);
    }
    bool recall(http::Request& request, http::Response& response) const {
        if (!request.has_session()) throw std::logic_error("Remember login requires session middleware");
        if (request.authenticated() || !remember_) return false;
        const auto token = request.cookie(options_.remember_cookie);
        if (token.empty() || token.size() != 64) return false;
        auto id = remember_->consume(Password::token_digest(token));
        if (!id) { expire(response); return false; }
        auto identity = identities_(*id);
        if (!identity || identity->id != *id) { expire(response); return false; }
        login(request, response, *identity, true);
        return true;
    }
    bool recall(http::Request& request) const {
        require_context(request);
        http::Response response;
        const bool result = recall(request, response);
        stage(request, response);
        return result;
    }
    void logout(http::Request& request, http::Response& response) const {
        if (!request.has_session()) throw std::logic_error("Logout requires session middleware");
        revoke(request);
        request.session().invalidate();
        if (request.has_auth()) request.auth().logout();
        expire(response);
    }
    void logout(http::Request& request) const {
        require_context(request);
        http::Response response;
        logout(request, response);
        stage(request, response);
    }
    static http::MiddlewareHandler middleware(std::shared_ptr<SessionGuard> current) {
        if (!current) throw std::invalid_argument("Session guard middleware requires a guard");
#ifdef GUNGNIR_WITH_PASSWORD
        return [current = std::move(current)](http::Request& request, http::Next next) -> Task<http::Response> {
            if (!request.has_session() || !request.has_auth())
                throw std::logic_error("Session guard requires session and authentication middleware");
            const auto context = request.auth_;
            context->begin_guard(current.get());
            struct Finish {
                std::shared_ptr<Context> context;
                ~Finish() { context->end_guard(); }
            } finish{context};
            (void)current->recall(request);
            auto response = co_await next(request);
            for (auto& cookie : context->take_cookies()) response.cookie(std::move(cookie));
            co_return response;
        };
#else
        throw std::logic_error("Session guard middleware requires the Gungnir application SDK or a framework build with GUNGNIR_WITH_PASSWORD=ON. Select that SDK with GUNGNIR_CMAKE_PREFIX and rebuild the application.");
#endif
    }
private:
    void require_context(const http::Request& request) const {
        if (!request.has_session() || !request.has_auth())
            throw std::logic_error("Session guard requires session and authentication middleware");
        if (request.auth().active_guard() != this)
            throw std::logic_error("Session guard requires its configured guard middleware");
    }
    static void stage(http::Request& request, const http::Response& response) {
        for (const auto& cookie : response.cookies()) request.auth().stage_cookie(cookie);
    }
    void revoke(http::Request& request) const {
        if (remember_) {
            const auto digest = request.session().get("_gungnir_remember_digest");
            if (!digest.empty()) remember_->revoke(digest);
            const auto token = request.cookie(options_.remember_cookie);
            if (token.size() == 64) remember_->revoke(Password::token_digest(token));
        }
        request.session().forget("_gungnir_remember_digest");
    }
    void issue(http::Request& request, http::Response& response, std::string token, std::string digest) const {
        request.session().put("_gungnir_remember_digest", std::move(digest));
        response.cookie(http::Cookie{.name = options_.remember_cookie, .value = std::move(token), .max_age = options_.remember_for, .secure = options_.secure});
    }
    void expire(http::Response& response) const {
        response.cookie(http::Cookie{.name = options_.remember_cookie, .value = "", .max_age = std::chrono::seconds{0}, .secure = options_.secure});
    }
    PasswordResolver credentials_;
    SessionIdentityResolver identities_;
    std::shared_ptr<RememberStore> remember_;
    LoginOptions options_;
};

// Run after session and AuthenticateSession middleware. The shared owner keeps
// the configured providers and token store alive across suspended requests.
inline http::MiddlewareHandler guard(std::shared_ptr<SessionGuard> current) {
    return SessionGuard::middleware(std::move(current));
}
}
