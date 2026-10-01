#pragma once
#include <chrono>
#include <gungnir/storage/disk.hpp>

namespace gungnir::storage {
struct S3Options {
    std::string endpoint; // HTTPS service endpoint; path-style bucket addressing.
    std::string bucket;
    std::string region{"us-east-1"};
    std::string access_key;
    std::string secret_key;
    std::string session_token;
    std::chrono::milliseconds timeout{30000};
    std::size_t max_object_bytes{64 * 1024 * 1024};
    bool allow_http{false}; // Explicit local development/test endpoint support.
    std::size_t max_list_entries{100000};
};
class S3Disk final : public Disk {
public:
    explicit S3Disk(S3Options options);
    bool exists(std::string_view path) const override;
    bool exists(std::string_view path, const CancellationToken&) const override;
    std::optional<std::string> get(std::string_view path) const override;
    std::optional<std::string> get(std::string_view path, const CancellationToken&) const override;
    void put(std::string path, std::string contents) override;
    void put(std::string path, std::string contents, const CancellationToken&) override;
    bool remove(std::string_view path) override;
    bool remove(std::string_view path, const CancellationToken&) override;
    bool copy(std::string_view from, std::string_view to) override;
    bool copy(std::string_view from, std::string_view to, const CancellationToken&) override;
    bool move(std::string_view from, std::string_view to) override;
    bool move(std::string_view from, std::string_view to, const CancellationToken&) override;
    std::uintmax_t size(std::string_view path) const override;
    std::uintmax_t size(std::string_view path, const CancellationToken&) const override;
    std::vector<std::string> files(std::string_view directory = {}) const override;
    std::vector<std::string> files(std::string_view directory, const CancellationToken&) const override;
private:
    struct Reply { long status{}; std::string body; std::uintmax_t length{}; };
    Reply request(std::string_view method, std::string_view path, std::string_view query,
        std::string_view body, const CancellationToken&) const;
    S3Options options_;
};
}
