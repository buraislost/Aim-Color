//! @file base64pp.h
//! @brief Single-header C++17 library for base64 encoding and decoding.
#ifndef BASE64PP_H
#define BASE64PP_H

// ===================================================================================
// >> DOCUMENTATION <<
//
// To use this library, do this in *one* C++ file:
// #define BASE64PP_IMPLEMENTATION
// #include "base64pp.h"
//
// In all other files, just include the header as usual:
// #include "base64pp.h"
//
// ===================================================================================

// >> INCLUDES <<
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// ===================================================================================
// >> API DECLARATIONS <<
// ===================================================================================

//! base64pp API namespace
namespace base64pp
{

    //! @brief This function will encode a blob of data into a base64
    //! string.
    //! @param input - a span pointing to a binary blob to encode.
    //! @return a base64 string containing the encoded data.
    std::string encode(std::span<std::uint8_t const> input);

    //! @brief Overload of the encode function for string_view. This converts
    //! the string input to a span and calls the conventional `encode`.
    //! @param input - a string_view to be encoded into base64.
    //! @return a base64 string containing the encoded string.
    std::string encode_str(std::string_view input);

    //! @brief Decodes a base64 encoded string, returning an optional
    //! blob. If the decoding fails, it returns std::nullopt
    //! @param encoded_str - the base64 encoded string
    //! @return an optional containing a valid blob of data, if
    //! decoding was successful. Otherwise, returns std::nullopt
    //! @note this function accepts unpadded strings, if they are valid
    //! otherwise. It rejects odd-sized unpadded strings.
    std::optional<std::vector<std::uint8_t>> decode(std::string_view encoded_str);

} // namespace base64pp

#endif // BASE64PP_H

// ===================================================================================
// >> IMPLEMENTATION <<
// ===================================================================================

#ifdef BASE64PP_IMPLEMENTATION

namespace
{
    // clang-format off
    std::array<char, 64> constexpr encode_table{'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
        'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5',
        '6', '7', '8', '9', '+', '/'};

    std::array<std::uint8_t, 256> constexpr decode_table {
        //  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0x00-0x0F
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0x10-0x1F
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x3E,0x64,0x64,0x64,0x3F, // 0x20-0x2F  (+, /)
        0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x64,0x64,0x64,0x64,0x64,0x64, // 0x30-0x3F  (0-9)
        0x64,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E, // 0x40-0x4F  (@, A-O)
        0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x64,0x64,0x64,0x64,0x64, // 0x50-0x5F  (P-Z)
        0x64,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28, // 0x60-0x6F  (`, a-o)
        0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x64,0x64,0x64,0x64,0x64, // 0x70-0x7F  (p-z)
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0x80-0x8F
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0x90-0x9F
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0xA0-0xAF
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0xB0-0xBF
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0xC0-0xCF
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0xD0-0xDF
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64, // 0xE0-0xEF
        0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64  // 0xF0-0xFF
    };
    // clang-format on

    std::array<char, 4> encode_tripplet(std::uint8_t a, std::uint8_t b, std::uint8_t c)
    {
        std::uint32_t const concat_bits = (a << 16) | (b << 8) | c;

        auto const b64_char1 = encode_table[(concat_bits >> 18) & 0b0011'1111];
        auto const b64_char2 = encode_table[(concat_bits >> 12) & 0b0011'1111];
        auto const b64_char3 = encode_table[(concat_bits >> 6) & 0b0011'1111];
        auto const b64_char4 = encode_table[concat_bits & 0b0011'1111];
        return {b64_char1, b64_char2, b64_char3, b64_char4};
    }

    inline bool is_valid_base64_char(char c)
    {
        // Unsigned promotion is safe here
        auto const decode_byte = decode_table[static_cast<unsigned char>(c)];
        return decode_byte != 0x64;
    }

    inline bool is_valid_base64_str(std::string_view const encoded_str)
    {
        if ((encoded_str.size() % 4) == 1)
        {
            return false;
        }

        if (encoded_str.empty())
        {
            return true;
        }

        auto it = begin(encoded_str);
        auto const end_it = end(encoded_str);
        std::size_t padding_count = 0;

        while (it != end_it)
        {
            char const c = *it;
            if (c == '=')
            {
                padding_count++;
            }
            else if (padding_count > 0 || !is_valid_base64_char(c))
            {
                // Invalid char or char after padding
                return false;
            }
            ++it;
        }

        if (padding_count > 2)
        {
            return false;
        }

        if (padding_count > 0 && encoded_str.size() % 4 != 0)
        {
            return false;
        }

        if (padding_count == 1 && encoded_str.size() >= 1 && encoded_str.back() != '=')
        {
             return false;
        }

        if (padding_count == 2 && encoded_str.size() >= 2)
        {
            if (encoded_str.back() != '=' || encoded_str[encoded_str.size()-2] != '=')
            {
                return false;
            }
        }

        return true;
    }


    std::array<std::uint8_t, 3> decode_quad(char a, char b, char c, char d)
    {
        // Unsigned promotion is safe here
        std::uint32_t const concat_bytes =
            (decode_table[static_cast<unsigned char>(a)] << 18) | (decode_table[static_cast<unsigned char>(b)] << 12) |
            (decode_table[static_cast<unsigned char>(c)] << 6) | decode_table[static_cast<unsigned char>(d)];

        std::uint8_t const byte1 = (concat_bytes >> 16) & 0b1111'1111;
        std::uint8_t const byte2 = (concat_bytes >> 8) & 0b1111'1111;
        std::uint8_t const byte3 = concat_bytes & 0b1111'1111;
        return {byte1, byte2, byte3};
    }
} // namespace

std::string base64pp::encode(std::span<std::uint8_t const> input)
{
    auto const size = input.size();
    auto const full_tripples = size / 3;

    std::string output;
    output.reserve(((size / 3) + (size % 3 > 0)) * 4);

    for (std::size_t i = 0; i < full_tripples; ++i)
    {
        auto const tripplet = input.subspan(i * 3, 3);
        auto const base64_chars = encode_tripplet(tripplet[0], tripplet[1], tripplet[2]);
        std::copy(begin(base64_chars), end(base64_chars), back_inserter(output));
    }

    if (auto const remaining_chars = size - full_tripples * 3; remaining_chars == 2)
    {
        auto const last_two = input.last(2);
        auto const base64_chars = encode_tripplet(last_two[0], last_two[1], 0x00);

        output.push_back(base64_chars[0]);
        output.push_back(base64_chars[1]);
        output.push_back(base64_chars[2]);
        output.push_back('=');
    }
    else if (remaining_chars == 1)
    {
        auto const base64_chars = encode_tripplet(input.back(), 0x00, 0x00);

        output.push_back(base64_chars[0]);
        output.push_back(base64_chars[1]);
        output.push_back('=');
        output.push_back('=');
    }

    return output;
}

std::string base64pp::encode_str(std::string_view input)
{
    return encode({reinterpret_cast<std::uint8_t const*>(input.data()), input.size()});
}

std::optional<std::vector<std::uint8_t>> base64pp::decode(std::string_view encoded_str)
{
    if (!is_valid_base64_str(encoded_str))
    {
        return std::nullopt;
    }

    if (encoded_str.empty())
    {
        return std::vector<std::uint8_t>{};
    }

    std::string working_copy(encoded_str);
    auto it = std::remove(working_copy.begin(), working_copy.end(), '=');
    working_copy.erase(it, working_copy.end());

    auto const full_quadruples = working_copy.size() / 4;

    std::vector<std::uint8_t> decoded_bytes;
    decoded_bytes.reserve(((working_copy.size() / 4) + 1) * 3);


    for (std::size_t i = 0; i < full_quadruples; ++i)
    {
        auto const quad = std::string_view(working_copy).substr(i * 4, 4);
        auto const bytes = decode_quad(quad[0], quad[1], quad[2], quad[3]);
        std::copy(begin(bytes), end(bytes), back_inserter(decoded_bytes));
    }

    auto const remaining_chars = working_copy.size() % 4;
    if (remaining_chars == 2)
    {
        auto const last_quad = working_copy.substr(full_quadruples * 4);
        auto const bytes = decode_quad(last_quad[0], last_quad[1], 'A', 'A');
        decoded_bytes.push_back(bytes[0]);
    }
    else if (remaining_chars == 3)
    {
        auto const last_quad = working_copy.substr(full_quadruples * 4);
        auto const bytes = decode_quad(last_quad[0], last_quad[1], last_quad[2], 'A');
        std::copy_n(begin(bytes), 2, back_inserter(decoded_bytes));
    }

    return decoded_bytes;
}

#endif // BASE64PP_IMPLEMENTATION