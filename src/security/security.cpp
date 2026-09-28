#include <gungnir/security/security.hpp>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#else
#include <fstream>
#endif

namespace gungnir::security {

namespace {

void fill_random(
    unsigned char* output,
    std::size_t size
) {
    if (size == 0) {
        return;
    }

#ifdef _WIN32
    if (
        size >
        static_cast<std::size_t>(
            std::numeric_limits<ULONG>::max()
        )
    ) {
        throw std::length_error(
            "Requested random buffer is too large"
        );
    }

    const auto status =
        BCryptGenRandom(
            nullptr,
            reinterpret_cast<PUCHAR>(
                output
            ),
            static_cast<ULONG>(size),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG
        );

    if (status < 0) {
        throw std::runtime_error(
            "Unable to obtain cryptographic random bytes"
        );
    }
#elif defined(__linux__)
    std::size_t offset = 0;

    while (offset < size) {
        const auto received =
            ::getrandom(
                output + offset,
                size - offset,
                0
            );

        if (received > 0) {
            offset +=
                static_cast<std::size_t>(
                    received
                );
            continue;
        }

        if (
            received < 0 &&
            errno == EINTR
        ) {
            continue;
        }

        throw std::runtime_error(
            "Unable to obtain cryptographic random bytes"
        );
    }
#elif defined(__APPLE__)
    arc4random_buf(
        output,
        size
    );
#else
    std::ifstream source{
        "/dev/urandom",
        std::ios::binary
    };

    if (!source) {
        throw std::runtime_error(
            "Unable to open operating-system random source"
        );
    }

    source.read(
        reinterpret_cast<char*>(
            output
        ),
        static_cast<std::streamsize>(
            size
        )
    );

    if (
        source.gcount() !=
        static_cast<std::streamsize>(
            size
        )
    ) {
        throw std::runtime_error(
            "Unable to read cryptographic random bytes"
        );
    }
#endif
}

} // namespace

bool constant_time_equal(
    std::string_view left,
    std::string_view right
) noexcept {
    const auto maximum =
        left.size() > right.size()
            ? left.size()
            : right.size();

    unsigned char difference =
        static_cast<unsigned char>(
            left.size() != right.size()
        );

    for (
        std::size_t index = 0;
        index < maximum;
        ++index
    ) {
        const auto lhs =
            index < left.size()
                ? static_cast<unsigned char>(
                    left[index]
                  )
                : static_cast<unsigned char>(
                    0
                  );

        const auto rhs =
            index < right.size()
                ? static_cast<unsigned char>(
                    right[index]
                  )
                : static_cast<unsigned char>(
                    0
                  );

        difference |=
            static_cast<unsigned char>(
                lhs ^ rhs
            );
    }

    return difference == 0;
}

bool valid_header_value(
    std::string_view value
) noexcept {
    for (const auto character : value) {
        if (
            character == '\r' ||
            character == '\n'
        ) {
            return false;
        }
    }

    return true;
}

bool valid_cookie_name(
    std::string_view value
) noexcept {
    if (value.empty()) {
        return false;
    }

    for (const auto character : value) {
        const auto c =
            static_cast<unsigned char>(
                character
            );

        if (
            c <= 0x20 ||
            c >= 0x7f ||
            character == '(' ||
            character == ')' ||
            character == '<' ||
            character == '>' ||
            character == '@' ||
            character == ',' ||
            character == ';' ||
            character == ':' ||
            character == '\\' ||
            character == '"' ||
            character == '/' ||
            character == '[' ||
            character == ']' ||
            character == '?' ||
            character == '=' ||
            character == '{' ||
            character == '}'
        ) {
            return false;
        }
    }

    return true;
}

std::string random_token(
    std::size_t bytes
) {
    if (bytes == 0) {
        throw std::invalid_argument(
            "Random token size must be greater than zero"
        );
    }

    std::vector<unsigned char>
        random(bytes);

    fill_random(
        random.data(),
        random.size()
    );

    constexpr std::array<
        char,
        16
    > hexadecimal{
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'a', 'b',
        'c', 'd', 'e', 'f'
    };

    std::string token;
    token.resize(
        bytes * 2
    );

    for (
        std::size_t index = 0;
        index < bytes;
        ++index
    ) {
        const auto value =
            random[index];

        token[
            index * 2
        ] =
            hexadecimal[
                static_cast<std::size_t>(
                    value >> 4
                )
            ];

        token[
            index * 2 + 1
        ] =
            hexadecimal[
                static_cast<std::size_t>(
                    value & 0x0f
                )
            ];
    }

    return token;
}

} // namespace gungnir::security
