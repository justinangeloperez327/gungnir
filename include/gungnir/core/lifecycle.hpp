#pragma once
#include <functional>
#include <vector>
#include <utility>

namespace gungnir {

class Application;

enum class LifecycleStage { created, registering, booting, ready, running, stopping, stopped };

class Lifecycle {
public:
    using Hook = std::function<void(Application&)>;
    void on_boot(Hook hook) { boot_.push_back(std::move(hook)); }
    void on_ready(Hook hook) { ready_.push_back(std::move(hook)); }
    void on_shutdown(Hook hook) { shutdown_.push_back(std::move(hook)); }
    [[nodiscard]] LifecycleStage stage() const noexcept { return stage_; }
    void stage(LifecycleStage value) noexcept { stage_ = value; }
    void fire_boot(Application& app) { for (auto& hook : boot_) hook(app); }
    void fire_ready(Application& app) { for (auto& hook : ready_) hook(app); }
    void fire_shutdown(Application& app) { for (auto it = shutdown_.rbegin(); it != shutdown_.rend(); ++it) (*it)(app); }
private:
    LifecycleStage stage_{LifecycleStage::created};
    std::vector<Hook> boot_, ready_, shutdown_;
};

} // namespace gungnir
