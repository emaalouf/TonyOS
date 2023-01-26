#include <karm-base/bits.h>
#include <karm-base/lock.h>
#include <karm-base/size.h>
#include <karm-logger/logger.h>

#include "arch.h"
#include "mem.h"

namespace Hjert::Mem {

struct Pmm : public Hal::Pmm {
    Hal::PmmRange _usable;
    Bits _bits;
    Lock _lock;

    Pmm(Hal::PmmRange usable, Bits bits)
        : _usable(usable),
          _bits(bits) {
        clear();
    }

    Result<Hal::PmmRange> allocRange(size_t size, Hal::PmmFlags flags) override {
        LockScope scope(_lock);
        bool upper = (flags & Hal::PmmFlags::UPPER) == Hal::PmmFlags::UPPER;

        try$(ensureAlign(size, Hal::PAGE_SIZE));
        size /= Hal::PAGE_SIZE;
        return bits2Pmm(try$(_bits.alloc(size, upper ? -1 : 0, upper)));
    }

    Error used(Hal::PmmRange range, Hal::PmmFlags) override {
        if (not range.overlaps(_usable)) {
            return OK;
        }

        LockScope scope(_lock);
        try$(range.ensureAligned(Hal::PAGE_SIZE));
        _bits.set(pmm2Bits(range), true);
        return OK;
    }

    Error free(Hal::PmmRange range) override {
        if (not range.overlaps(_usable)) {
            return Error{"range is not in usable memory"};
        }

        LockScope scope(_lock);
        try$(range.ensureAligned(Hal::PAGE_SIZE));
        _bits.set(pmm2Bits(range), false);
        return OK;
    }

    void clear() {
        LockScope scope(_lock);
        _bits.fill(true);
    }

    BitsRange pmm2Bits(Hal::PmmRange range) {
        range.start -= _usable.start;
        range.start /= Hal::PAGE_SIZE;
        range.size /= Hal::PAGE_SIZE;

        return range.as<BitsRange>();
    }

    Hal::PmmRange bits2Pmm(BitsRange range) {
        range.size *= Hal::PAGE_SIZE;
        range.start *= Hal::PAGE_SIZE;
        range.start += _usable.start;

        return range.as<Hal::PmmRange>();
    }
};

struct Heap : public Hal::Heap {
    Hal::Pmm &_pmm;

    Heap(Hal::Pmm &pmm) : _pmm(pmm) {
    }

    Result<Hal::HeapRange> allocRange(size_t size) override {
        return pmm2Heap(try$(_pmm.allocRange(size, Hal::PmmFlags::NIL)));
    }

    Error free(Hal::HeapRange range) override {
        return _pmm.free(try$(heap2Pmm(range)));
    }

    Result<Hal::PmmRange> heap2Pmm(Hal::HeapRange range) override {
        if (range.start < Hal::UPPER_HALF) {
            return Error{"Invalid heap range"};
        }

        return Hal::PmmRange{
            range.start - Hal::UPPER_HALF,
            range.size,
        };
    }

    Result<Hal::HeapRange> pmm2Heap(Hal::PmmRange range) override {
        return Hal::HeapRange{
            range.start + Hal::UPPER_HALF,
            range.size,
        };
    }
};

static Opt<Pmm> _pmm = NONE;
static Opt<Heap> _heap = NONE;

Error init(Handover::Payload &payload) {
    auto usableRange = payload.usableRange<Hal::PmmRange>();

    if (usableRange.empty()) {
        logError("mem: no usable memory");
        return Error{"no usable memory"};
    }

    logInfo("mem: usable range: {x}-{x}", usableRange.start, usableRange.end());

    size_t bitsSize = usableRange.size / Hal::PAGE_SIZE / 8;

    auto pmmBits = payload.find(bitsSize);

    if (pmmBits.empty()) {
        logError("mem: no usable memory for pmm");
        return Error{"no usable memory for pmm"};
    }

    logInfo("mem: pmm bitmap range: {x}-{x}", pmmBits.start, pmmBits.end());

    _pmm = Pmm(usableRange,
               MutSlice{
                   reinterpret_cast<uint8_t *>(pmmBits.start + Hal::UPPER_HALF),
                   pmmBits.size,
               });

    _heap = Heap(_pmm.unwrap());

    logInfo("mem: marking kernel memory as used");
    for (auto &record : payload) {
        if (record.tag == Handover::Tag::FREE) {
            try$(pmm().free({record.start, record.size}));
        }
    }

    try$(_pmm->used({pmmBits.start, pmmBits.size}, Hal::PmmFlags::NIL));

    logInfo("mem: mapping kernel...");
    try$(vmm().allocRange(
        {Handover::KERNEL_BASE + Hal::PAGE_SIZE, gib(2) - Hal::PAGE_SIZE - Hal::PAGE_SIZE},
        {Hal::PAGE_SIZE, gib(2) - Hal::PAGE_SIZE - Hal::PAGE_SIZE},
        Hal::Vmm::READ | Hal::Vmm::WRITE));

    logInfo("mem: mapping upper half...");
    try$(vmm().allocRange(
        {Handover::UPPER_HALF + Hal::PAGE_SIZE, gib(4) - Hal::PAGE_SIZE},
        {Hal::PAGE_SIZE, gib(4) - Hal::PAGE_SIZE},
        Hal::Vmm::READ | Hal::Vmm::WRITE));

    vmm().activate();

    return OK;
}

Hal::Pmm &pmm() {
    if (not _pmm) {
        logFatal("mem: pmm not initialized");
    }
    return *_pmm;
}

Hal::Heap &heap() {
    if (not _heap) {
        logFatal("mem: heap not initialized");
    }
    return *_heap;
}

Hal::Vmm &vmm() {
    return Arch::vmm();
}

} // namespace Hjert::Mem
