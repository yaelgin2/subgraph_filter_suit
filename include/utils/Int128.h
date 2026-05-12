#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace sgf
{

/**
 * @brief Portable 128-bit unsigned integer.
 *
 * Provides the arithmetic and bitwise operations needed by the motif and path
 * preprocessors to pack color and structure data into compact 128-bit keys.
 * Replaces the GCC/Clang extension __uint128_t so the code compiles on MSVC.
 */
struct UInt128
{
    uint64_t m_high{0U};  ///< High 64 bits.
    uint64_t m_low{0U};   ///< Low 64 bits.

    /**
     * @brief Default-construct (value zero).
     */
    constexpr UInt128() noexcept = default;

    /**
     * @brief Construct from a single 32-bit value (high word = 0).
     * @param low The value placed in the low word.
     */
    constexpr explicit UInt128(const uint32_t low) noexcept
        : m_low(static_cast<uint64_t>(low))
    {
    }

    /**
     * @brief Construct from a single 64-bit value (high word = 0).
     * @param low The value placed in the low word.
     */
    constexpr explicit UInt128(const uint64_t low) noexcept
        : m_low(low)
    {
    }

    /**
     * @brief Construct from explicit high and low 64-bit words.
     * @param high The high 64 bits.
     * @param low The low 64 bits.
     */
    constexpr UInt128(const uint64_t high, const uint64_t low) noexcept
        : m_high(high)
        , m_low(low)
    {
    }

    /** @brief Extract the low 32 bits. */
    constexpr explicit operator uint32_t() const noexcept
    {
        return static_cast<uint32_t>(m_low);
    }

    /** @brief Extract the low 64 bits. */
    constexpr explicit operator uint64_t() const noexcept
    {
        return m_low;
    }

    /** @brief True if value is non-zero. */
    constexpr explicit operator bool() const noexcept
    {
        return m_high != 0U || m_low != 0U;
    }

    // ── Comparison ────────────────────────────────────────────────────────────

    /**
     * @brief Equality comparison.
     * @param other Value to compare against.
     * @return True if both words are equal.
     */
    constexpr bool operator==(const UInt128& other) const noexcept
    {
        return m_high == other.m_high && m_low == other.m_low;
    }

    /**
     * @brief Inequality comparison.
     * @param other Value to compare against.
     * @return True if any word differs.
     */
    constexpr bool operator!=(const UInt128& other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * @brief Less-than comparison.
     * @param other Value to compare against.
     */
    constexpr bool operator<(const UInt128& other) const noexcept
    {
        return m_high < other.m_high || (m_high == other.m_high && m_low < other.m_low);
    }

    /**
     * @brief Greater-than comparison.
     * @param other Value to compare against.
     */
    constexpr bool operator>(const UInt128& other) const noexcept
    {
        return other < *this;
    }

    /**
     * @brief Less-than-or-equal comparison.
     * @param other Value to compare against.
     */
    constexpr bool operator<=(const UInt128& other) const noexcept
    {
        return !(other < *this);
    }

    /**
     * @brief Greater-than-or-equal comparison.
     * @param other Value to compare against.
     */
    constexpr bool operator>=(const UInt128& other) const noexcept
    {
        return !(*this < other);
    }

    // ── Bitwise ───────────────────────────────────────────────────────────────

    /**
     * @brief Bitwise NOT.
     * @return Complement of every bit.
     */
    constexpr UInt128 operator~() const noexcept
    {
        return UInt128{~m_high, ~m_low};
    }

    /**
     * @brief Bitwise OR assignment.
     * @param other Operand.
     * @return Reference to this.
     */
    constexpr UInt128& operator|=(const UInt128& other) noexcept
    {
        m_high |= other.m_high;
        m_low |= other.m_low;
        return *this;
    }

    /**
     * @brief Bitwise OR.
     * @param other Operand.
     * @return Result.
     */
    constexpr UInt128 operator|(const UInt128& other) const noexcept
    {
        UInt128 result = *this;
        result |= other;
        return result;
    }

    /**
     * @brief Bitwise OR assignment with a 32-bit value.
     * @param value Value to OR into the low word.
     * @return Reference to this.
     */
    constexpr UInt128& operator|=(const uint32_t value) noexcept
    {
        m_low |= static_cast<uint64_t>(value);
        return *this;
    }

    /**
     * @brief Bitwise AND assignment.
     * @param other Operand.
     * @return Reference to this.
     */
    constexpr UInt128& operator&=(const UInt128& other) noexcept
    {
        m_high &= other.m_high;
        m_low &= other.m_low;
        return *this;
    }

    /**
     * @brief Bitwise AND.
     * @param other Operand.
     * @return Result.
     */
    constexpr UInt128 operator&(const UInt128& other) const noexcept
    {
        UInt128 result = *this;
        result &= other;
        return result;
    }

    // ── Shift ─────────────────────────────────────────────────────────────────

    /**
     * @brief Left-shift assignment.
     * @param shift Number of bit positions to shift (0–127).
     * @return Reference to this.
     */
    constexpr UInt128& operator<<=(const uint32_t shift) noexcept
    {
        constexpr uint32_t word_bits = 64U;
        if (shift == 0U)
        {
            return *this;
        }
        if (shift >= word_bits * 2U)
        {
            m_high = 0U;
            m_low = 0U;
        }
        else if (shift >= word_bits)
        {
            m_high = m_low << (shift - word_bits);
            m_low = 0U;
        }
        else
        {
            m_high = (m_high << shift) | (m_low >> (word_bits - shift));
            m_low <<= shift;
        }
        return *this;
    }

    /**
     * @brief Left-shift.
     * @param shift Number of bit positions to shift (0–127).
     * @return Shifted result.
     */
    constexpr UInt128 operator<<(const uint32_t shift) const noexcept
    {
        UInt128 result = *this;
        result <<= shift;
        return result;
    }

    /**
     * @brief Right-shift assignment.
     * @param shift Number of bit positions to shift (0–127).
     * @return Reference to this.
     */
    constexpr UInt128& operator>>=(const uint32_t shift) noexcept
    {
        constexpr uint32_t word_bits = 64U;
        if (shift == 0U)
        {
            return *this;
        }
        if (shift >= word_bits * 2U)
        {
            m_high = 0U;
            m_low = 0U;
        }
        else if (shift >= word_bits)
        {
            m_low = m_high >> (shift - word_bits);
            m_high = 0U;
        }
        else
        {
            m_low = (m_low >> shift) | (m_high << (word_bits - shift));
            m_high >>= shift;
        }
        return *this;
    }

    /**
     * @brief Right-shift.
     * @param shift Number of bit positions to shift (0–127).
     * @return Shifted result.
     */
    constexpr UInt128 operator>>(const uint32_t shift) const noexcept
    {
        UInt128 result = *this;
        result >>= shift;
        return result;
    }

    // ── Arithmetic ────────────────────────────────────────────────────────────

    /**
     * @brief Addition assignment.
     * @param other Addend.
     * @return Reference to this.
     */
    constexpr UInt128& operator+=(const UInt128& other) noexcept
    {
        const uint64_t prev_low = m_low;
        m_low += other.m_low;
        m_high += other.m_high;
        if (m_low < prev_low)
        {
            ++m_high;  // carry from low word overflow
        }
        return *this;
    }

    /**
     * @brief Addition.
     * @param other Addend.
     * @return Sum.
     */
    constexpr UInt128 operator+(const UInt128& other) const noexcept
    {
        UInt128 result = *this;
        result += other;
        return result;
    }

    /**
     * @brief Subtraction.
     * @param other Subtrahend.
     * @return Difference (wraps on underflow).
     */
    constexpr UInt128 operator-(const UInt128& other) const noexcept
    {
        const UInt128 negated = ~other + UInt128{0U, 1U};
        return *this + negated;
    }

    /**
     * @brief Divide by a 32-bit divisor, producing quotient in-place and returning remainder.
     *
     * Uses 32-bit-chunk long division to avoid needing native 128-bit support.
     *
     * @param divisor Non-zero 32-bit divisor.
     * @return Remainder of the division.
     */
    uint32_t divmod_uint32(const uint32_t divisor) noexcept
    {
        constexpr uint32_t bits_in_32_int = 32U;
        const uint64_t high_q = m_high / divisor;
        const uint64_t high_r = m_high % divisor;

        const uint64_t mid_dividend = (high_r << bits_in_32_int) | (m_low >> bits_in_32_int);
        const uint64_t mid_q = mid_dividend / divisor;
        const uint64_t mid_r = mid_dividend % divisor;

        const uint64_t low_dividend = (mid_r << bits_in_32_int) | (m_low & 0xFFFF'FFFFUL);
        const uint64_t low_q = low_dividend / divisor;
        const uint32_t remainder = static_cast<uint32_t>(low_dividend % divisor);

        m_high = high_q;
        m_low = (mid_q << bits_in_32_int) | low_q;
        return remainder;
    }

    /**
     * @brief Division-assignment by a 32-bit divisor.
     * @param divisor Non-zero 32-bit divisor.
     * @return Reference to this (quotient).
     */
    UInt128& operator/=(const uint32_t divisor) noexcept
    {
        divmod_uint32(divisor);
        return *this;
    }

    /**
     * @brief Modulo by a 32-bit divisor.
     * @param divisor Non-zero 32-bit divisor.
     * @return Remainder.
     */
    uint32_t operator%(const uint32_t divisor) const noexcept
    {
        UInt128 copy = *this;
        return copy.divmod_uint32(divisor);
    }
};

/**
 * @brief Hash functor for UInt128.
 *
 * Combines hashes of the high and low 64-bit words for use as an
 * unordered_map key type.
 */
struct UInt128Hash
{
    /**
     * @brief Hash a 128-bit unsigned integer.
     * @param value The value to hash.
     * @return Combined hash of the high and low 64-bit halves.
     */
    size_t operator()(const UInt128& value) const noexcept
    {
        constexpr size_t hash_mix_shift = 1U;
        return std::hash<uint64_t>{}(value.m_high) ^
               (std::hash<uint64_t>{}(value.m_low) << hash_mix_shift);
    }
};

}  // namespace sgf
