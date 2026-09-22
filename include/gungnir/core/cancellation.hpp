#pragma once
#include <atomic>
#include <memory>

namespace gungnir {

class CancellationToken {
public:
    CancellationToken() : state_(std::make_shared<std::atomic_bool>(false)) {}
    [[nodiscard]] bool cancelled() const noexcept { return state_->load(); }
private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> state) : state_(std::move(state)) {}
    std::shared_ptr<std::atomic_bool> state_;
};

class CancellationSource {
public:
    CancellationSource() : state_(std::make_shared<std::atomic_bool>(false)) {}
    [[nodiscard]] CancellationToken token() const noexcept { return CancellationToken{state_}; }
    void cancel() noexcept { state_->store(true); }
private:
    std::shared_ptr<std::atomic_bool> state_;
};

} // namespace gungnir
