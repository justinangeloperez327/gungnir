#pragma once
#include <gungnir/auth/authorization.hpp>
#include <concepts>
#include <type_traits>
#include <typeindex>
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
public:
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
