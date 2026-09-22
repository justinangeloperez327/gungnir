#include <cassert>
#include <string>
#include <vector>

#include <gungnir/events/events.hpp>

class Saved final : public gungnir::events::Event {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "model.saved";
    }
};

int main() {
    using namespace gungnir;
    events::Dispatcher events;
    std::vector<int> order;

    const auto low = events.listen("model.saved", [&](const events::Event&) {
        order.push_back(1);
    });
    events.listen("model.saved", [&](const events::Event&) {
        order.push_back(2);
    }, 10);

    Saved event;
    events.dispatch(event);
    assert((order == std::vector<int>{2, 1}));
    assert(events.listener_count("model.saved") == 2);
    assert(events.forget("model.saved", low));
    assert(events.listener_count("model.saved") == 1);

    return 0;
}
