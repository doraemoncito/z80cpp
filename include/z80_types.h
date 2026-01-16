#ifndef Z80_TYPES_H
#define Z80_TYPES_H

#include <cstdint>

#ifndef Z80_FORCE_INLINE
#    ifdef _MSC_VER
#        define Z80_FORCE_INLINE __forceinline
#    elif defined(__GNUC__) || defined(__clang__)
#        define Z80_FORCE_INLINE __attribute__((always_inline)) inline
#    else
#        define Z80_FORCE_INLINE inline
#    endif
#endif

#ifndef Z80_LIKELY
#    if defined(__GNUC__) || defined(__clang__)
#        define Z80_LIKELY(x)   __builtin_expect(!!(x), 1) // NOLINT(cppcoreguidelines-macro-usage)
#        define Z80_UNLIKELY(x) __builtin_expect(!!(x), 0) // NOLINT(cppcoreguidelines-macro-usage)
#    else
#        define Z80_LIKELY(x)   (x)
#        define Z80_UNLIKELY(x) (x)
#    endif
#endif

#ifndef Z80_RESTRICT
#    ifdef _MSC_VER
#        define Z80_RESTRICT __restrict
#    elif defined(__GNUC__) || defined(__clang__)
#        define Z80_RESTRICT __restrict__
#    else
#        define Z80_RESTRICT
#    endif
#endif

/* Union allowing a register pair to be accessed as bytes or as a word */
union RegisterPair {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    struct {
        uint8_t hi, lo;
    } byte8;
#else
    struct {
        uint8_t lo, hi;
    } byte8;
#endif
    uint16_t word;
};

#endif // Z80_TYPES_H
