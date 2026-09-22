#include <gungnir/routing/router.hpp>

#include <exception>
#include <stdexcept>
#include <utility>

namespace gungnir::routing {
namespace {
struct RouteEntry {
    http::Method method;
    std::string path;
    Handler handler;
    std::vector<http::MiddlewareHandler> middleware;
    std::string name;
    std::unordered_map<std::string, std::regex> constraints;
};

std::vector<std::string_view> split(std::string_view path) {
    std::vector<std::string_view> result;
    for (std::size_t start = 0; start < path.size();) {
        while (start < path.size() && path[start] == '/') ++start;
        if (start >= path.size()) break;
        auto end = path.find('/', start);
        if (end == std::string_view::npos) { result.push_back(path.substr(start)); break; }
        result.push_back(path.substr(start, end - start)); start = end + 1;
    }
    return result;
}

bool match(const RouteEntry& route, std::string_view actual, http::Request::Parameters& parameters) {
    const auto expected = split(route.path);
    const auto found = split(actual);
    if (expected.size() != found.size()) return false;
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto segment = expected[i];
        if (segment.size() >= 3 && segment.front() == '{' && segment.back() == '}') {
            const std::string name{segment.substr(1, segment.size() - 2)};
            if (name.empty()) return false;
            if (const auto constraint = route.constraints.find(name); constraint != route.constraints.end() &&
                !std::regex_match(found[i].begin(), found[i].end(), constraint->second)) return false;
            parameters.insert_or_assign(name, std::string{found[i]});
        } else if (segment != found[i]) return false;
    }
    return true;
}

Task<http::Response> pipeline(const std::vector<http::MiddlewareHandler>& middleware, std::size_t index,
                              const Handler& terminal, http::Request& request) {
    if (index >= middleware.size()) co_return co_await terminal(request);
    http::Next next{[&middleware, index, &terminal](http::Request& next_request) -> Task<http::Response> {
        co_return co_await pipeline(middleware, index + 1, terminal, next_request);
    }};
    co_return co_await middleware[index](request, std::move(next));
}

std::string join(std::string_view prefix, std::string_view path) {
    if (prefix.empty()) return std::string{path};
    if (path.empty()) return std::string{prefix};
    return std::string{prefix} + ((prefix.back() == '/' || path.front() == '/') ? "" : "/") + std::string{path};
}
}

class Router::Impl {
public:
    std::vector<RouteEntry> routes;
    std::vector<http::MiddlewareHandler> middleware;
    http::MiddlewareRegistry* middleware_registry{nullptr};
    http::ExceptionHandler exceptions;
    std::optional<Handler> fallback;
};

RouteRegistration& RouteRegistration::middleware(http::MiddlewareHandler handler) {
    if (!router_) throw std::logic_error("Route registration is not attached to a router");
    router_->add_middleware(index_, std::move(handler)); return *this;
}
RouteRegistration& RouteRegistration::middleware(std::string alias) {
    if (!router_) throw std::logic_error("Route registration is not attached to a router");
    router_->add_middleware(index_, std::move(alias)); return *this;
}
RouteRegistration& RouteRegistration::name(std::string value) {
    if (!router_) throw std::logic_error("Route registration is not attached to a router");
    router_->set_name(index_, std::move(value)); return *this;
}
RouteRegistration& RouteRegistration::where(std::string parameter, std::string expression) {
    if (!router_) throw std::logic_error("Route registration is not attached to a router");
    router_->set_constraint(index_, std::move(parameter), std::move(expression)); return *this;
}
RouteRegistration& RouteRegistration::where_number(std::string parameter) { return where(std::move(parameter), "[0-9]+"); }
RouteRegistration& RouteRegistration::where_uuid(std::string parameter) {
    return where(std::move(parameter), "[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[1-5][0-9a-fA-F]{3}-[89abAB][0-9a-fA-F]{3}-[0-9a-fA-F]{12}");
}

Router::Router() : impl_(std::make_unique<Impl>()) {}
Router::~Router() = default;
Router::Router(Router&&) noexcept = default;
Router& Router::operator=(Router&&) noexcept = default;

RouteRegistration Router::add(http::Method method, std::string path, Handler handler) {
    const auto index = impl_->routes.size();
    impl_->routes.push_back({method, std::move(path), std::move(handler), {}, {}, {}});
    return {this, index};
}
RouteRegistration Router::add(http::Method method, std::string path, SyncHandler handler) {
    return add(method, std::move(path), [handler=std::move(handler)](http::Request& request)->Task<http::Response>{ co_return handler(request); });
}
RouteRegistration Router::add(http::Method method, std::string path, SimpleHandler handler) {
    return add(method, std::move(path), [handler=std::move(handler)](http::Request&)->Task<http::Response>{ co_return handler(); });
}
#define GUNGNIR_ROUTE(method_name, enum_name) RouteRegistration Router::method_name(std::string p, Handler h){return add(http::Method::enum_name,std::move(p),std::move(h));} RouteRegistration Router::method_name(std::string p, SyncHandler h){return add(http::Method::enum_name,std::move(p),std::move(h));} RouteRegistration Router::method_name(std::string p, SimpleHandler h){return add(http::Method::enum_name,std::move(p),std::move(h));}
GUNGNIR_ROUTE(get,get)
GUNGNIR_ROUTE(post,post)
GUNGNIR_ROUTE(put,put)
GUNGNIR_ROUTE(patch,patch)
GUNGNIR_ROUTE(delete_,delete_)
GUNGNIR_ROUTE(options,options)
GUNGNIR_ROUTE(head,head)
#undef GUNGNIR_ROUTE
RouteRegistration Router::remove(std::string p, Handler h){return delete_(std::move(p),std::move(h));}
RouteRegistration Router::remove(std::string p, SyncHandler h){return delete_(std::move(p),std::move(h));}
RouteRegistration Router::remove(std::string p, SimpleHandler h){return delete_(std::move(p),std::move(h));}

Router& Router::use(http::MiddlewareHandler m){impl_->middleware.push_back(std::move(m));return *this;}
Router& Router::middleware_registry(http::MiddlewareRegistry& registry){impl_->middleware_registry=&registry;return *this;}
Router& Router::fallback(Handler h){impl_->fallback=std::move(h);return *this;}
Router& Router::fallback(SyncHandler h){return fallback([h=std::move(h)](http::Request& r)->Task<http::Response>{co_return h(r);});}
Router& Router::fallback(SimpleHandler h){return fallback([h=std::move(h)](http::Request&)->Task<http::Response>{co_return h();});}
bool Router::has(std::string_view name)const noexcept{for(const auto& r:impl_->routes)if(r.name==name)return true;return false;}
std::size_t Router::route_count()const noexcept{return impl_->routes.size();}
RouteGroup Router::group(std::string prefix){return RouteGroup{*this,std::move(prefix)};}
void Router::add_middleware(std::size_t i,http::MiddlewareHandler m){if(i>=impl_->routes.size())throw std::out_of_range("Invalid route");impl_->routes[i].middleware.push_back(std::move(m));}
void Router::add_middleware(std::size_t i,std::string alias){if(!impl_->middleware_registry)throw std::logic_error("Middleware registry is not attached");add_middleware(i,impl_->middleware_registry->resolve(alias));}
void Router::set_name(std::size_t i,std::string n){if(i>=impl_->routes.size())throw std::out_of_range("Invalid route");for(std::size_t x=0;x<impl_->routes.size();++x)if(x!=i&&!n.empty()&&impl_->routes[x].name==n)throw std::invalid_argument("Duplicate named route: "+n);impl_->routes[i].name=std::move(n);}
void Router::set_constraint(std::size_t i,std::string p,std::string e){if(i>=impl_->routes.size())throw std::out_of_range("Invalid route");impl_->routes[i].constraints.insert_or_assign(std::move(p),std::regex{e});}

std::string Router::url(std::string_view name,const std::unordered_map<std::string,std::string>& parameters) const {
    for(const auto& route:impl_->routes) if(route.name==name){
        auto result=route.path;
        for(const auto& [key,value]:parameters){const auto token="{"+key+"}";if(auto pos=result.find(token);pos!=std::string::npos)result.replace(pos,token.size(),value);}
        if(result.find('{')!=std::string::npos)throw std::invalid_argument("Missing named route parameter");
        return result;
    }
    throw std::out_of_range("Unknown named route: "+std::string{name});
}

Task<http::Response> Router::dispatch(http::Request& request) const {
    request.clear_route_parameters(); const RouteEntry* selected=nullptr;
    for(const auto& route:impl_->routes){if(route.method!=request.method())continue;http::Request::Parameters params;if(!match(route,request.path(),params))continue;for(auto& [k,v]:params)request.set_route_parameter(k,std::move(v));selected=&route;break;}
    std::vector<http::MiddlewareHandler> chain=impl_->middleware;
    Handler terminal=impl_->fallback.value_or(Handler{[](http::Request&)->Task<http::Response>{co_return http::Response::not_found();}});
    if(selected){chain.insert(chain.end(),selected->middleware.begin(),selected->middleware.end());terminal=selected->handler;}
    try{co_return co_await pipeline(chain,0,terminal,request);}catch(...){co_return impl_->exceptions.render(request,std::current_exception());}
}

RouteGroup::RouteGroup(Router& router,std::string prefix):router_(&router),prefix_(std::move(prefix)){}
RouteGroup& RouteGroup::middleware(http::MiddlewareHandler handler){middleware_.push_back(std::move(handler));return *this;}
RouteGroup& RouteGroup::middleware(std::string alias){middleware_aliases_.push_back(std::move(alias));return *this;}
RouteGroup& RouteGroup::middleware_group(std::string group){middleware_groups_.push_back(std::move(group));return *this;}
std::string RouteGroup::path(std::string_view value)const{return join(prefix_,value);}
RouteRegistration RouteGroup::apply(RouteRegistration r){
    for(const auto& m:middleware_) r.middleware(m);
    for(const auto& alias:middleware_aliases_) r.middleware(alias);
    for(const auto& group:middleware_groups_) {
        if(!router_->impl_->middleware_registry) throw std::logic_error("Middleware registry is not attached");
        for(auto& handler:router_->impl_->middleware_registry->resolve_group(group)) r.middleware(std::move(handler));
    }
    return r;
}
RouteRegistration RouteGroup::get(std::string p,Handler h){return apply(router_->get(path(p),std::move(h)));}
RouteRegistration RouteGroup::get(std::string p,SyncHandler h){return apply(router_->get(path(p),std::move(h)));}
RouteRegistration RouteGroup::get(std::string p,SimpleHandler h){return apply(router_->get(path(p),std::move(h)));}
RouteRegistration RouteGroup::post(std::string p,Handler h){return apply(router_->post(path(p),std::move(h)));}
RouteRegistration RouteGroup::post(std::string p,SyncHandler h){return apply(router_->post(path(p),std::move(h)));}
RouteRegistration RouteGroup::post(std::string p,SimpleHandler h){return apply(router_->post(path(p),std::move(h)));}
RouteRegistration RouteGroup::put(std::string p,Handler h){return apply(router_->put(path(p),std::move(h)));}
RouteRegistration RouteGroup::put(std::string p,SyncHandler h){return apply(router_->put(path(p),std::move(h)));}
RouteRegistration RouteGroup::put(std::string p,SimpleHandler h){return apply(router_->put(path(p),std::move(h)));}
RouteRegistration RouteGroup::patch(std::string p,Handler h){return apply(router_->patch(path(p),std::move(h)));}
RouteRegistration RouteGroup::patch(std::string p,SyncHandler h){return apply(router_->patch(path(p),std::move(h)));}
RouteRegistration RouteGroup::patch(std::string p,SimpleHandler h){return apply(router_->patch(path(p),std::move(h)));}
RouteRegistration RouteGroup::delete_(std::string p,Handler h){return apply(router_->delete_(path(p),std::move(h)));}
RouteRegistration RouteGroup::delete_(std::string p,SyncHandler h){return apply(router_->delete_(path(p),std::move(h)));}
RouteRegistration RouteGroup::delete_(std::string p,SimpleHandler h){return apply(router_->delete_(path(p),std::move(h)));}
RouteRegistration RouteGroup::options(std::string p,Handler h){return apply(router_->options(path(p),std::move(h)));}
RouteRegistration RouteGroup::head(std::string p,Handler h){return apply(router_->head(path(p),std::move(h)));}

} // namespace gungnir::routing
