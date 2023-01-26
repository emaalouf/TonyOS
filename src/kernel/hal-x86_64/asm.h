#pragma once

#include <karm-base/std.h>

namespace x86_64 {

inline void cli(void) { asm volatile("cli"); }

inline void sti(void) { asm volatile("sti"); }

inline void hlt(void) { asm volatile("hlt"); }

inline void pause(void) { asm volatile("pause"); }

inline void invlpg(size_t addr) {
    asm volatile("invlpg (%0)" ::"r"(addr)
                 : "memory");
}

/* --- CRs ------------------------------------------------------------------ */

#define CR(N)                                         \
    inline uint64_t rdcr##N(void) {                   \
        uint64_t value = 0;                           \
        asm volatile("mov %%cr" #N ", %0"             \
                     : "=r"(value));                  \
        return value;                                 \
    }                                                 \
                                                      \
    inline void wrcr##N(uint64_t value) {             \
        asm volatile("mov %0, %%cr" #N ::"a"(value)); \
    }

CR(0)
CR(1)
CR(2)
CR(3)

#undef CR

/* --- AVX/SSSE ------------------------------------------------------------- */

inline uint64_t rdxcr(uint32_t i) {
    uint32_t eax, edx;
    asm volatile("xgetbv"

                 : "=a"(eax), "=d"(edx)
                 : "c"(i)
                 : "memory");

    return eax | ((uint64_t)edx << 32);
}

inline void wrxcr(uint32_t i, uint64_t value) {
    uint32_t edx = value >> 32;
    uint32_t eax = (uint32_t)value;
    asm volatile("xsetbv"
                 :
                 : "a"(eax), "d"(edx), "c"(i)
                 : "memory");
}

/* --- Msrs ----------------------------------------------------------------- */

enum struct Msrs : uint64_t {
    APIC = 0x1B,
    EFER = 0xC0000080,
    STAR = 0xC0000081,
    LSTAR = 0xC0000082,
    COMPAT_STAR = 0xC0000083,
    SYSCALL_FLAG_MASK = 0xC0000084,
    FS_BASE = 0xC0000100,
    GS_BASE = 0xC0000101,
    KERN_GS_BASE = 0xc0000102,
};

inline uint64_t rdmsr(Msrs msr) {

    uint32_t low, high;
    asm volatile("rdmsr"
                 : "=a"(low), "=d"(high)
                 : "c"((uint64_t)msr));
    return ((uint64_t)high << 32) | low;
}

inline void wrmsr(Msrs msr, uint64_t value) {

    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr"
                 :
                 : "c"((uint64_t)msr), "a"(low), "d"(high));
}

} // namespace x86_64
