#pragma once
#include <gungnir/auth/authorization.hpp>
#include <concepts>
#include <memory>
#include <gungnir/auth/authorize.hpp>
#include <gungnir/http/request.hpp>
#include <type_traits>
#include <typeindex>
#include <gungnir/orm/executor.hpp>
namespace gungnir::auth {
// Explicit actor/resource types prevent policy registration from erasing the
// resource contract or accidentally casting an unrelated authenticated user.
class ResourceAuthorization {
    struct Binding { std::function<Decision(const void*,const void*)> invoke; };
    using TypePair = std::pair<std::type_index,std::type_index>;
    struct TypePairHash {
        std::size_t operator()(const TypePair& key) const noexcept {
            return key.first.hash_code() ^ (key.second.hash_code() << 1);
        }
    };
    std::unordered_map<std::string,std::unordered_map<TypePair,Binding,TypePairHash>> bindings_;
    std::unordered_map<std::type_index,std::function<std::shared_ptr<void>(const Identity&)>> actors_;
public:
    // Default model actors use the owning application's ordinary ORM scope.
    // An explicitly registered identity mapping always takes precedence.
    template<class Actor> ResourceAuthorization& model_actor() {
        if (!actors_.contains(typeid(Actor)))
            actor<Actor>([](const Identity& identity) { return Actor::find(model::AttributeValue{identity.id}); });
        return *this;
    }
    template<class Actor,class Resolver> ResourceAuthorization& actor(Resolver resolver) {
        actors_.insert_or_assign(typeid(Actor),[resolver=std::move(resolver)](const Identity& identity) -> std::shared_ptr<void> {
            auto actor = resolver(identity);
            if (!actor) return {};
            return std::make_shared<Actor>(std::move(*actor));
        });
        return *this;
    }
    template<class Resource> Decision inspect(std::string_view ability,const http::Request& request,const Resource& resource) const {
        const auto* identity = request.user();
        if (!identity) return Decision::deny("Authentication required");
        auto found = bindings_.find(std::string{ability});
        if (found == bindings_.end()) return Decision::deny("Authorization ability is not defined");
        const Binding* selected = nullptr;
        std::shared_ptr<void> actor_value;
        for (const auto& [types,binding] : found->second) {
            if (types.second != typeid(Resource)) continue;
            std::shared_ptr<void> value;
            if (types.first == typeid(Identity)) value = std::make_shared<Identity>(*identity);
            else if (auto resolver = actors_.find(types.first); resolver != actors_.end()) value = resolver->second(*identity);
            if (!value) continue;
            if (selected) return Decision::deny("Ambiguous authorization actor binding");
            selected = &binding; actor_value = std::move(value);
        }
        return selected ? selected->invoke(actor_value.get(),&resource) : Decision::deny("Authorization actor is not configured");
    }
    template<class Resource> void authorize(const http::Request& request,std::string_view ability,const Resource& resource) const {
        if (!request.authenticated()) throw AuthorizationError{"Authentication required",401};
        auto decision = inspect(ability,request,resource);
        if (!decision.allowed) throw AuthorizationError{decision.message.empty() ? "Action is not authorized" : decision.message};
    }
    template<class Actor,class Resource,class Fn>
    ResourceAuthorization& define(std::string ability, Fn fn) {
        bindings_[std::move(ability)].insert_or_assign(TypePair{typeid(Actor),typeid(Resource)},Binding{[fn=std::move(fn)](const void* actor,const void* resource) { const auto result = fn(*static_cast<const Actor*>(actor),*static_cast<const Resource*>(resource)); if constexpr (std::same_as<std::remove_cvref_t<decltype(result)>,bool>) return Decision{result,{}}; else return result; }}); return *this;
    }
    template<class Actor,class Resource> Decision inspect(std::string_view ability,const Actor& actor,const Resource& resource) const {
        const auto found = bindings_.find(std::string{ability});
        if (found == bindings_.end()) return Decision::deny("Authorization ability is not defined");
        const auto policy = found->second.find(TypePair{typeid(Actor),typeid(Resource)});
        if (policy == found->second.end()) return Decision::deny("Authorization policy type mismatch");
        return policy->second.invoke(&actor,&resource);
    }
};
}
