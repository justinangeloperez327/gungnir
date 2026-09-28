#include <gungnir/http/websocket.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gungnir::http::websocket::wire {

namespace {

constexpr std::string_view websocket_guid =
    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

[[nodiscard]]
char ascii_lower(
    char value
) noexcept {
    if (
        value >= 'A' &&
        value <= 'Z'
    ) {
        return
            static_cast<char>(
                value - 'A' + 'a'
            );
    }

    return value;
}

[[nodiscard]]
std::string_view trim(
    std::string_view value
) noexcept {
    while (
        !value.empty() &&
        (
            value.front() == ' ' ||
            value.front() == '\t'
        )
    ) {
        value.remove_prefix(1);
    }

    while (
        !value.empty() &&
        (
            value.back() == ' ' ||
            value.back() == '\t'
        )
    ) {
        value.remove_suffix(1);
    }

    return value;
}

[[nodiscard]]
bool iequals(
    std::string_view left,
    std::string_view right
) noexcept {
    if (left.size() != right.size()) {
        return false;
    }

    for (
        std::size_t index = 0;
        index < left.size();
        ++index
    ) {
        if (
            ascii_lower(left[index]) !=
            ascii_lower(right[index])
        ) {
            return false;
        }
    }

    return true;
}

[[nodiscard]]
bool token_contains(
    std::string_view values,
    std::string_view expected
) noexcept {
    std::size_t cursor = 0;

    while (cursor <= values.size()) {
        const auto comma =
            values.find(
                ',',
                cursor
            );

        const auto end =
            comma ==
                std::string_view::npos
            ? values.size()
            : comma;

        if (
            iequals(
                trim(
                    values.substr(
                        cursor,
                        end - cursor
                    )
                ),
                expected
            )
        ) {
            return true;
        }

        if (
            comma ==
            std::string_view::npos
        ) {
            break;
        }

        cursor = comma + 1;
    }

    return false;
}

[[nodiscard]]
int base64_value(
    char value
) noexcept {
    if (
        value >= 'A' &&
        value <= 'Z'
    ) {
        return value - 'A';
    }

    if (
        value >= 'a' &&
        value <= 'z'
    ) {
        return value - 'a' + 26;
    }

    if (
        value >= '0' &&
        value <= '9'
    ) {
        return value - '0' + 52;
    }

    if (value == '+') {
        return 62;
    }

    if (value == '/') {
        return 63;
    }

    return -1;
}

[[nodiscard]]
std::optional<std::vector<std::uint8_t>>
base64_decode(
    std::string_view input
) {
    if (
        input.empty() ||
        input.size() % 4 != 0
    ) {
        return std::nullopt;
    }

    std::vector<std::uint8_t>
        output;

    output.reserve(
        input.size() / 4 * 3
    );

    for (
        std::size_t offset = 0;
        offset < input.size();
        offset += 4
    ) {
        std::array<int, 4>
            values{};

        int padding = 0;

        for (
            std::size_t index = 0;
            index < 4;
            ++index
        ) {
            const auto character =
                input[offset + index];

            if (character == '=') {
                values[index] = 0;
                ++padding;

                if (
                    offset + 4 !=
                    input.size() ||
                    index < 2
                ) {
                    return std::nullopt;
                }

                continue;
            }

            if (padding != 0) {
                return std::nullopt;
            }

            values[index] =
                base64_value(
                    character
                );

            if (values[index] < 0) {
                return std::nullopt;
            }
        }

        if (padding > 2) {
            return std::nullopt;
        }

        const auto bits =
            (
                static_cast<std::uint32_t>(
                    values[0]
                ) << 18
            ) |
            (
                static_cast<std::uint32_t>(
                    values[1]
                ) << 12
            ) |
            (
                static_cast<std::uint32_t>(
                    values[2]
                ) << 6
            ) |
            static_cast<std::uint32_t>(
                values[3]
            );

        output.push_back(
            static_cast<std::uint8_t>(
                (bits >> 16) &
                0xff
            )
        );

        if (padding < 2) {
            output.push_back(
                static_cast<std::uint8_t>(
                    (bits >> 8) &
                    0xff
                )
            );
        }

        if (padding < 1) {
            output.push_back(
                static_cast<std::uint8_t>(
                    bits &
                    0xff
                )
            );
        }
    }

    return output;
}

[[nodiscard]]
std::string base64_encode(
    const std::array<
        std::uint8_t,
        20
    >& input
) {
    static constexpr
        std::string_view alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

    std::string output;

    output.reserve(
        (
            input.size() + 2
        ) /
        3 *
        4
    );

    for (
        std::size_t offset = 0;
        offset < input.size();
        offset += 3
    ) {
        const auto remaining =
            input.size() - offset;

        const std::uint32_t first =
            input[offset];

        const std::uint32_t second =
            remaining > 1
            ? input[offset + 1]
            : 0;

        const std::uint32_t third =
            remaining > 2
            ? input[offset + 2]
            : 0;

        const auto bits =
            (first << 16) |
            (second << 8) |
            third;

        output.push_back(
            alphabet[
                (bits >> 18) &
                0x3f
            ]
        );

        output.push_back(
            alphabet[
                (bits >> 12) &
                0x3f
            ]
        );

        output.push_back(
            remaining > 1
            ? alphabet[
                (bits >> 6) &
                0x3f
              ]
            : '='
        );

        output.push_back(
            remaining > 2
            ? alphabet[
                bits &
                0x3f
              ]
            : '='
        );
    }

    return output;
}

[[nodiscard]]
std::array<std::uint8_t, 20>
sha1(
    std::string_view input
) {
    std::vector<std::uint8_t>
        message{
            input.begin(),
            input.end()
        };

    const auto bit_length =
        static_cast<std::uint64_t>(
            message.size()
        ) *
        8;

    message.push_back(0x80);

    while (
        message.size() % 64 !=
        56
    ) {
        message.push_back(0);
    }

    for (
        int shift = 56;
        shift >= 0;
        shift -= 8
    ) {
        message.push_back(
            static_cast<std::uint8_t>(
                bit_length >>
                shift
            )
        );
    }

    std::uint32_t h0 =
        0x67452301;
    std::uint32_t h1 =
        0xefcdab89;
    std::uint32_t h2 =
        0x98badcfe;
    std::uint32_t h3 =
        0x10325476;
    std::uint32_t h4 =
        0xc3d2e1f0;

    for (
        std::size_t offset = 0;
        offset < message.size();
        offset += 64
    ) {
        std::array<
            std::uint32_t,
            80
        > words{};

        for (
            std::size_t index = 0;
            index < 16;
            ++index
        ) {
            const auto base =
                offset +
                index * 4;

            words[index] =
                (
                    static_cast<
                        std::uint32_t
                    >(
                        message[base]
                    ) << 24
                ) |
                (
                    static_cast<
                        std::uint32_t
                    >(
                        message[
                            base + 1
                        ]
                    ) << 16
                ) |
                (
                    static_cast<
                        std::uint32_t
                    >(
                        message[
                            base + 2
                        ]
                    ) << 8
                ) |
                static_cast<
                    std::uint32_t
                >(
                    message[
                        base + 3
                    ]
                );
        }

        for (
            std::size_t index = 16;
            index < 80;
            ++index
        ) {
            words[index] =
                std::rotl(
                    words[index - 3] ^
                    words[index - 8] ^
                    words[index - 14] ^
                    words[index - 16],
                    1
                );
        }

        auto a = h0;
        auto b = h1;
        auto c = h2;
        auto d = h3;
        auto e = h4;

        for (
            std::size_t index = 0;
            index < 80;
            ++index
        ) {
            std::uint32_t function = 0;
            std::uint32_t constant = 0;

            if (index < 20) {
                function =
                    (b & c) |
                    (
                        (~b) &
                        d
                    );

                constant =
                    0x5a827999;
            } else if (
                index < 40
            ) {
                function =
                    b ^ c ^ d;

                constant =
                    0x6ed9eba1;
            } else if (
                index < 60
            ) {
                function =
                    (b & c) |
                    (b & d) |
                    (c & d);

                constant =
                    0x8f1bbcdc;
            } else {
                function =
                    b ^ c ^ d;

                constant =
                    0xca62c1d6;
            }

            const auto temporary =
                std::rotl(
                    a,
                    5
                ) +
                function +
                e +
                constant +
                words[index];

            e = d;
            d = c;

            c =
                std::rotl(
                    b,
                    30
                );

            b = a;
            a = temporary;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    const std::array<
        std::uint32_t,
        5
    > words{
        h0,
        h1,
        h2,
        h3,
        h4
    };

    std::array<
        std::uint8_t,
        20
    > digest{};

    for (
        std::size_t index = 0;
        index < words.size();
        ++index
    ) {
        digest[index * 4] =
            static_cast<std::uint8_t>(
                words[index] >> 24
            );

        digest[index * 4 + 1] =
            static_cast<std::uint8_t>(
                words[index] >> 16
            );

        digest[index * 4 + 2] =
            static_cast<std::uint8_t>(
                words[index] >> 8
            );

        digest[index * 4 + 3] =
            static_cast<std::uint8_t>(
                words[index]
            );
    }

    return digest;
}

[[nodiscard]]
bool protocol_offered(
    std::string_view offered,
    std::string_view selected
) noexcept {
    if (selected.empty()) {
        return true;
    }

    std::size_t cursor = 0;

    while (cursor <= offered.size()) {
        const auto comma =
            offered.find(
                ',',
                cursor
            );

        const auto end =
            comma ==
                std::string_view::npos
            ? offered.size()
            : comma;

        if (
            trim(
                offered.substr(
                    cursor,
                    end - cursor
                )
            ) ==
            selected
        ) {
            return true;
        }

        if (
            comma ==
            std::string_view::npos
        ) {
            break;
        }

        cursor = comma + 1;
    }

    return false;
}

[[nodiscard]]
bool valid_opcode(
    std::uint8_t value
) noexcept {
    return
        value == 0x0 ||
        value == 0x1 ||
        value == 0x2 ||
        value == 0x8 ||
        value == 0x9 ||
        value == 0xa;
}

} // namespace

bool upgrade_requested(
    const Request& request
) noexcept {
    return
        request.method() ==
            Method::get &&
        iequals(
            trim(
                request.header(
                    "upgrade"
                )
            ),
            "websocket"
        ) &&
        token_contains(
            request.header(
                "connection"
            ),
            "upgrade"
        );
}

std::optional<WebSocketHandshake>
handshake(
    const Request& request,
    std::string_view selected_protocol
) {
    if (
        !upgrade_requested(
            request
        )
    ) {
        return std::nullopt;
    }

    if (
        trim(
            request.header(
                "sec-websocket-version"
            )
        ) !=
        "13"
    ) {
        return std::nullopt;
    }

    const auto key =
        trim(
            request.header(
                "sec-websocket-key"
            )
        );

    const auto decoded =
        base64_decode(key);

    if (
        !decoded ||
        decoded->size() != 16
    ) {
        return std::nullopt;
    }

    if (
        !protocol_offered(
            request.header(
                "sec-websocket-protocol"
            ),
            selected_protocol
        )
    ) {
        return std::nullopt;
    }

    std::string challenge{
        key
    };

    challenge += websocket_guid;

    WebSocketHandshake result;

    result.accept =
        base64_encode(
            sha1(challenge)
        );

    result.protocol =
        std::string{
            selected_protocol
        };

    return result;
}

ParseResult parse_client_frame(
    std::string_view bytes,
    std::size_t max_payload_bytes
) {
    if (bytes.size() < 2) {
        return {};
    }

    const auto first =
        static_cast<std::uint8_t>(
            bytes[0]
        );

    const auto second =
        static_cast<std::uint8_t>(
            bytes[1]
        );

    if ((first & 0x70) != 0) {
        throw std::invalid_argument(
            "WebSocket RSV bits require a negotiated extension"
        );
    }

    const auto opcode_value =
        static_cast<std::uint8_t>(
            first & 0x0f
        );

    if (
        !valid_opcode(
            opcode_value
        )
    ) {
        throw std::invalid_argument(
            "Unsupported WebSocket opcode"
        );
    }

    const bool final =
        (first & 0x80) != 0;

    const bool masked =
        (second & 0x80) != 0;

    if (!masked) {
        throw std::invalid_argument(
            "Client WebSocket frames must be masked"
        );
    }

    std::uint64_t payload_size =
        second & 0x7f;

    std::size_t cursor = 2;

    if (payload_size == 126) {
        if (
            bytes.size() <
            cursor + 2
        ) {
            return {};
        }

        payload_size =
            (
                static_cast<
                    std::uint64_t
                >(
                    static_cast<
                        std::uint8_t
                    >(bytes[cursor])
                ) << 8
            ) |
            static_cast<
                std::uint64_t
            >(
                static_cast<
                    std::uint8_t
                >(
                    bytes[cursor + 1]
                )
            );

        cursor += 2;

        if (payload_size < 126) {
            throw std::invalid_argument(
                "Non-canonical WebSocket payload length"
            );
        }
    } else if (
        payload_size == 127
    ) {
        if (
            bytes.size() <
            cursor + 8
        ) {
            return {};
        }

        payload_size = 0;

        for (
            int index = 0;
            index < 8;
            ++index
        ) {
            const auto value =
                static_cast<
                    std::uint8_t
                >(
                    bytes[
                        cursor +
                        static_cast<
                            std::size_t
                        >(index)
                    ]
                );

            if (
                index == 0 &&
                (value & 0x80) != 0
            ) {
                throw std::invalid_argument(
                    "WebSocket payload length exceeds signed 63-bit range"
                );
            }

            payload_size =
                (
                    payload_size << 8
                ) |
                value;
        }

        cursor += 8;

        if (payload_size <= 65535) {
            throw std::invalid_argument(
                "Non-canonical WebSocket payload length"
            );
        }
    }

    const bool control =
        opcode_value >= 0x8;

    if (
        control &&
        (
            !final ||
            payload_size > 125
        )
    ) {
        throw std::invalid_argument(
            "Invalid fragmented or oversized WebSocket control frame"
        );
    }

    if (
        payload_size >
        max_payload_bytes
    ) {
        throw std::length_error(
            "WebSocket frame payload exceeds configured limit"
        );
    }

    if (
        payload_size >
        static_cast<std::uint64_t>(
            std::numeric_limits<
                std::size_t
            >::max()
        )
    ) {
        throw std::length_error(
            "WebSocket frame payload exceeds platform size limit"
        );
    }

    if (
        bytes.size() <
        cursor + 4
    ) {
        return {};
    }

    std::array<
        std::uint8_t,
        4
    > mask{};

    for (
        std::size_t index = 0;
        index < mask.size();
        ++index
    ) {
        mask[index] =
            static_cast<std::uint8_t>(
                bytes[
                    cursor + index
                ]
            );
    }

    cursor += mask.size();

    const auto payload =
        static_cast<std::size_t>(
            payload_size
        );

    if (
        bytes.size() <
        cursor + payload
    ) {
        return {};
    }

    Frame frame;
    frame.final = final;
    frame.opcode =
        static_cast<
            WebSocketOpcode
        >(opcode_value);

    frame.payload.resize(
        payload
    );

    for (
        std::size_t index = 0;
        index < payload;
        ++index
    ) {
        frame.payload[index] =
            static_cast<char>(
                static_cast<
                    std::uint8_t
                >(
                    bytes[
                        cursor +
                        index
                    ]
                ) ^
                mask[
                    index % 4
                ]
            );
    }

    return {
        std::move(frame),
        cursor + payload
    };
}

std::string serialize_server_frame(
    WebSocketOpcode opcode,
    std::string_view payload,
    bool final
) {
    const auto opcode_value =
        static_cast<std::uint8_t>(
            opcode
        );

    if (
        !valid_opcode(
            opcode_value
        )
    ) {
        throw std::invalid_argument(
            "Unsupported WebSocket opcode"
        );
    }

    const bool control =
        opcode_value >= 0x8;

    if (
        control &&
        (
            !final ||
            payload.size() > 125
        )
    ) {
        throw std::invalid_argument(
            "Invalid WebSocket control frame"
        );
    }

    std::string output;
    output.reserve(
        payload.size() + 10
    );

    output.push_back(
        static_cast<char>(
            (
                final
                ? 0x80
                : 0
            ) |
            opcode_value
        )
    );

    if (payload.size() <= 125) {
        output.push_back(
            static_cast<char>(
                payload.size()
            )
        );
    } else if (
        payload.size() <= 65535
    ) {
        output.push_back(
            static_cast<char>(126)
        );

        output.push_back(
            static_cast<char>(
                (
                    payload.size() >>
                    8
                ) &
                0xff
            )
        );

        output.push_back(
            static_cast<char>(
                payload.size() &
                0xff
            )
        );
    } else {
        output.push_back(
            static_cast<char>(127)
        );

        const auto length =
            static_cast<std::uint64_t>(
                payload.size()
            );

        for (
            int shift = 56;
            shift >= 0;
            shift -= 8
        ) {
            output.push_back(
                static_cast<char>(
                    (
                        length >>
                        shift
                    ) &
                    0xff
                )
            );
        }
    }

    output.append(
        payload.data(),
        payload.size()
    );

    return output;
}

bool valid_utf8(
    std::string_view value
) noexcept {
    std::size_t cursor = 0;

    while (cursor < value.size()) {
        const auto first =
            static_cast<std::uint8_t>(
                value[cursor]
            );

        if (first <= 0x7f) {
            ++cursor;
            continue;
        }

        std::size_t count = 0;
        std::uint32_t codepoint = 0;
        std::uint32_t minimum = 0;

        if (
            (first & 0xe0) ==
            0xc0
        ) {
            count = 2;
            codepoint =
                first & 0x1f;
            minimum = 0x80;
        } else if (
            (first & 0xf0) ==
            0xe0
        ) {
            count = 3;
            codepoint =
                first & 0x0f;
            minimum = 0x800;
        } else if (
            (first & 0xf8) ==
            0xf0
        ) {
            count = 4;
            codepoint =
                first & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }

        if (
            cursor + count >
            value.size()
        ) {
            return false;
        }

        for (
            std::size_t index = 1;
            index < count;
            ++index
        ) {
            const auto continuation =
                static_cast<
                    std::uint8_t
                >(
                    value[
                        cursor + index
                    ]
                );

            if (
                (continuation & 0xc0) !=
                0x80
            ) {
                return false;
            }

            codepoint =
                (
                    codepoint << 6
                ) |
                (
                    continuation &
                    0x3f
                );
        }

        if (
            codepoint < minimum ||
            codepoint > 0x10ffff ||
            (
                codepoint >= 0xd800 &&
                codepoint <= 0xdfff
            )
        ) {
            return false;
        }

        cursor += count;
    }

    return true;
}

bool valid_close_code(
    std::uint16_t code
) noexcept {
    if (
        code >= 3000 &&
        code <= 4999
    ) {
        return true;
    }

    switch (code) {
        case 1000:
        case 1001:
        case 1002:
        case 1003:
        case 1007:
        case 1008:
        case 1009:
        case 1010:
        case 1011:
        case 1012:
        case 1013:
        case 1014:
            return true;
        default:
            return false;
    }
}

} // namespace gungnir::http::websocket::wire
