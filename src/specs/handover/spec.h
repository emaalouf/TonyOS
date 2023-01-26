#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Handover {

inline size_t KERNEL_BASE = 0xffffffff80000000;
inline size_t UPPER_HALF = 0xffff800000000000;

namespace Utils {

inline bool cstrEq(const char *str1, const char *str2) {
    while (*str1 and *str2) {
        if (*str1++ != *str2++)
            return false;
    }
    return *str1 == *str2;
}

} // namespace Utils

static constexpr uint32_t COOLBOOT = 0xc001b001;

#define FOREACH_TAG(TAG)      \
    TAG(FREE, 0)              \
    TAG(MAGIC, COOLBOOT)      \
    TAG(SELF, 0xa24f988d)     \
    TAG(STACK, 0xf65b391b)    \
    TAG(KERNEL, 0xbfc71b20)   \
    TAG(LOADER, 0xf1f80c26)   \
    TAG(FILE, 0xcbc36d3b)     \
    TAG(RSDP, 0x8d3bbb)       \
    TAG(FDT, 0xb628bbc1)      \
    TAG(FB, 0xe2d55685)       \
    TAG(RESERVED, 0xb8841d2d) \
    TAG(END, 0xffffffff)

enum struct Tag : uint32_t {
#define ITER(NAME, VALUE) NAME = VALUE,
    FOREACH_TAG(ITER)
#undef ITER
};

using enum Tag;

static char const *tagName(Tag tag) {
    switch (tag) {
#define ITER(NAME, VALUE) \
    case Tag::NAME:       \
        return #NAME;
        FOREACH_TAG(ITER)
#undef ITER
    }
    return "UNKNOWN";
}

inline bool shouldMerge(Tag tag) {
    switch (tag) {
    case Tag::FREE:
    case Tag::LOADER:
    case Tag::KERNEL:
    case Tag::RESERVED:
        return true;

    default:
        return false;
    }
}

enum struct PixelFormat : uint16_t {
    RGBX8888 = 0x7451,
    BGRX8888 = 0xd040,
};

struct Record {
    Tag tag;
    uint32_t flags;
    uint64_t start;
    uint64_t size;

    char const *name() const {
        return tagName(tag);
    }

    union {
        struct
        {
            uint16_t width;
            uint16_t height;
            uint16_t pitch;
            PixelFormat format;
        } fb;

        struct
        {
            uint32_t name;
            uint32_t meta;
        } file;

        uint64_t more;
    };

    uint64_t end() const {
        return start + size;
    }

    bool empty() const {
        return size == 0;
    }
};

struct Payload {
    uint32_t magic, agent, size, len;
    Record records[];

    char const *stringAt(uint64_t offset) const {
        if (offset == 0) {
            return "";
        }
        char const *data = reinterpret_cast<char const *>(this);
        return data + offset;
    }

    char const *agentName() const {
        return stringAt(agent);
    }

    Record const *findTag(Tag tag) const {
        for (auto const &r : *this) {
            if (r.tag == tag) {
                return &r;
            }
        }

        return nullptr;
    }

    Record const *fileByName(char const *name) const {
        for (auto const &r : *this) {
            if (r.tag == Tag::FILE and Utils::cstrEq(stringAt(r.file.name), name)) {
                return &r;
            }
        }

        return nullptr;
    }

    Record *begin() {
        return records;
    }

    Record *end() {
        return records + len;
    }

    Record const *begin() const {
        return records;
    }

    Record const *end() const {
        return records + len;
    }

    size_t sum(Handover::Tag tag) {
        size_t total = 0;
        for (auto const &r : *this) {
            if (r.tag == tag) {
                total += r.size;
            }
        }
        return total;
    }

    Record find(size_t size) {
        for (auto &r : *this) {
            if (r.tag == Tag::FREE and r.size >= size) {
                return r;
            }
        }

        return {};
    }

    template <typename R>
    R usableRange() const {
        size_t start = 0, end = 0;

        for (auto const &r : *this) {
            if (r.tag == Tag::FREE) {
                if (r.start < start or start == 0) {
                    start = r.start;
                }
                if (r.end() > end) {
                    end = r.end();
                }
            }
        }

        return R{
            start,
            end - start,
        };
    }
};

struct Request {
    Tag tag;
    uint32_t flags;
    uint64_t more;

    char const *name() const {
        return tagName(tag);
    }
};

inline constexpr Request requestSelf() {
    return {Tag::SELF, 0, 0};
}

inline constexpr Request requestStack(uint64_t preferedSize = 64 * 1024) {
    return {Tag::STACK, 0, preferedSize};
}

inline constexpr Request requestKernel() {
    return {Tag::KERNEL, 0, 0};
}

inline constexpr Request requestFiles() {
    return {Tag::FILE, 0, 0};
}

inline constexpr Request requestRsdp() {
    return {Tag::RSDP, 0, 0};
}

inline constexpr Request requestFdt() {
    return {Tag::FDT, 0, 0};
}

inline constexpr Request requestFb(PixelFormat preferedFormat = PixelFormat::BGRX8888) {
    return {Tag::FB, 0, (uint64_t)preferedFormat};
}

inline bool valid(uint32_t magic, Payload const &payload) {
    if (magic != COOLBOOT) {
        return false;
    }

    if (payload.magic != COOLBOOT) {
        return false;
    }

    return true;
}

static constexpr char const *REQUEST_SECTION = ".handover";

#define HandoverSection$() \
    [[gnu::used, gnu::section(".handover")]]

// clang-format off

#define HandoverRequests$(...)                          \
    HandoverSection$()                                  \
    static ::Handover::Request const __handover__[] = { \
        {::Handover::Tag::MAGIC, 0, 0},                 \
        __VA_ARGS__ __VA_OPT__(, )                      \
        {::Handover::Tag::END, 0, 0},                   \
    };

// clang-format on

using EntryPoint = void (*)(uint64_t magic, Payload const *handover);

} // namespace Handover
