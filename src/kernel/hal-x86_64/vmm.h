#pragma once

#include <hal/mem.h>
#include <hal/pmm.h>
#include <hal/vmm.h>
#include <karm-logger/logger.h>

#include "asm.h"
#include "paging.h"

namespace x86_64 {

template <typename Mapper = Hal::IdentityMapper>
struct Vmm : public Hal::Vmm {
    Hal::Pmm &_pmm;
    Pml<4> *_pml4 = nullptr;
    Mapper _mapper;

    Vmm(Hal::Pmm &pmm, Pml<4> *pml4, Mapper mapper = {})
        : _pmm(pmm),
          _pml4(pml4),
          _mapper(mapper) {}

    template <size_t L>
    Result<Pml<L - 1> *> pml(Pml<L> &upper, size_t vaddr) {
        auto page = upper.pageAt(vaddr);

        if (not page.present()) {
            return Error::ADDR_NOT_AVAILABLE;
        }

        return _mapper.map(page.template as<Pml<L - 1>>());
    }

    template <size_t L>
    Result<Pml<L - 1> *> pmlOrAlloc(Pml<L> &upper, size_t vaddr) {
        auto page = upper.pageAt(vaddr);

        if (page.present()) {
            return _mapper.map(page.template as<Pml<L - 1>>());
        }

        size_t lower = try$(_pmm.allocRange(Hal::PAGE_SIZE, Hal::PmmFlags::NIL)).start;
        memset(_mapper.map((void *)lower), 0, Hal::PAGE_SIZE);
        upper.putPage(vaddr, {lower, Entry::WRITE | Entry::PRESENT | Entry::USER});
        return _mapper.map((Pml<L - 1> *)lower);
    }

    Error freePage(size_t vaddr, size_t paddr, Hal::VmmFlags) {
        auto pml3 = try$(pmlOrAlloc(*_pml4, vaddr));
        auto pml2 = try$(pmlOrAlloc(*pml3, vaddr));
        auto pml1 = try$(pmlOrAlloc(*pml2, vaddr));
        pml1->putPage(vaddr, {paddr, Entry::WRITE | Entry::PRESENT | Entry::USER});
        return OK;
    }

    Error freePage(size_t vaddr) {
        auto pml3 = try$(pml(*_pml4, vaddr));
        auto pml2 = try$(pml(*pml3, vaddr));
        auto pml1 = try$(pml(*pml2, vaddr));
        pml1->putPage(vaddr, {});
        return OK;
    }

    Result<Hal::VmmRange> allocRange(Hal::VmmRange vaddr, Hal::PmmRange paddr, Hal::VmmFlags flags) override {
        if (paddr.size != vaddr.size) {
            return Error::INVALID_INPUT;
        }

        for (size_t page = 0; page < vaddr.size; page += Hal::PAGE_SIZE) {
            try$(freePage(vaddr.start + page, paddr.start + page, flags));
        }
        return vaddr;
    }

    Error free(Hal::VmmRange vaddr) override {
        for (size_t page = 0; page < vaddr.size; page += Hal::PAGE_SIZE) {
            try$(freePage(vaddr.start + page));
        }
        return OK;
    }

    Error update(Hal::VmmRange, Hal::VmmFlags) override {
        notImplemented();
    }

    Error flush(Hal::VmmRange vaddr) override {
        for (size_t i = 0; i < vaddr.size; i += Hal::PAGE_SIZE) {
            x86_64::invlpg(vaddr.start + i);
        }

        return OK;
    }

    void activate() override {
        x86_64::wrcr3(root());
    }

    template <size_t L>
    void _dumpPml(Pml<L> &pml, size_t vaddr) {
        for (size_t i = 0; i < 512; i++) {
            auto page = pml[i];
            size_t curr = pml.index2virt(i) | vaddr;
            if constexpr (L == 1) {
                if (page.present()) {
                    logInfo("x86_64: vmm: {x} {x}", curr, page._raw);
                }
            } else if (page.present()) {
                auto &lower = *_mapper.map(page.template as<Pml<L - 1>>());
                _dumpPml(lower, curr);
            }
        }
    }

    void dump() override {
        _dumpPml(*_pml4, 0);
    }

    size_t root() override {
        return _mapper.unmap((size_t)_pml4);
    }
};

} // namespace x86_64
