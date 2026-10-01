#include <gungnir/auth/password.hpp>
#include <gungnir/security/random.hpp>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <array>
#include <stdexcept>

namespace gungnir::auth {
namespace {
constexpr std::string_view prefix = "scrypt$131072$8$1$";
std::string hex(const unsigned char* bytes, std::size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result; result.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) { result += digits[bytes[i] >> 4]; result += digits[bytes[i] & 15]; }
    return result;
}
std::string derive(std::string_view password, std::string_view salt) {
    if (password.size() > 1024 * 1024) throw std::invalid_argument("Password is too large");
    std::array<unsigned char, 32> output{};
    if (EVP_PBE_scrypt(password.data(), password.size(), reinterpret_cast<const unsigned char*>(salt.data()), salt.size(),
        131072, 8, 1, 256ULL * 1024 * 1024, output.data(), output.size()) != 1)
        throw std::runtime_error("Password derivation failed");
    auto result = hex(output.data(), output.size());
    OPENSSL_cleanse(output.data(), output.size());
    return result;
}
bool hex_string(std::string_view text) {
    for (char c : text) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
}
std::string Password::hash(std::string_view password) {
    const auto salt = security::random_token(16);
    return std::string{prefix} + salt + "$" + derive(password, salt);
}
bool Password::verify(std::string_view password, std::string_view encoded) {
    if (needs_rehash(encoded)) return false;
    const auto salt = encoded.substr(prefix.size(), 32);
    const auto expected = encoded.substr(prefix.size() + 33);
    if (!hex_string(salt) || !hex_string(expected)) return false;
    const auto actual = derive(password, salt);
    return CRYPTO_memcmp(actual.data(), expected.data(), actual.size()) == 0;
}
bool Password::needs_rehash(std::string_view encoded) noexcept {
    return !encoded.starts_with(prefix) || encoded.size() != prefix.size() + 32 + 1 + 64 || encoded[prefix.size() + 32] != '$';
}
std::string Password::token_digest(std::string_view token) {
    std::array<unsigned char, 32> output{};
    unsigned size{};
    if (EVP_Digest(token.data(), token.size(), output.data(), &size, EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("Token digest failed");
    return hex(output.data(), size);
}
}
