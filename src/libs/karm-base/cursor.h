#pragma once

#include "slice.h"

namespace Karm {

template <typename T>
struct Cursor {
    using Inner = T;

    T const *_begin = nullptr;
    T const *_end = nullptr;

    constexpr Cursor() = default;

    Cursor(Sliceable<T> auto &slice)
        : _begin(begin(slice)), _end(end(slice)) {}

    constexpr T const &operator[](size_t i) const {
        return _begin[i];
    }

    constexpr operator T const *() const { return _begin; }

    bool ended() const {
        return _begin >= _end;
    }

    size_t rem() const {
        return _end - _begin;
    }

    T curr() const {
        if (not ended()) {
            panic("curr() called on ended cursor");
        }
        return *_begin;
    }

    T next() {
        if (ended()) {
            panic("next() called on ended cursor");
        }
        T r = *_begin;
        _begin++;
        return r;
    }

    void next(size_t n) {
        for (size_t i = 0; i < n; i++)
            next();
    }

    constexpr T const *buf() const {
        return _begin;
    }

    constexpr size_t len() const {
        return _end - _begin;
    }
};

template <typename T>
struct MutCursor {
    T *_begin = nullptr;
    T *_end = nullptr;

    MutCursor(MutSliceable<T> auto &slice)
        : _begin(begin(slice)), _end(end(slice)) {}

    constexpr T &operator[](size_t i) {
        return _begin[i];
    }

    constexpr T const &operator[](size_t i) const {
        return _begin[i];
    }

    constexpr operator T *() { return _begin; }

    constexpr operator T const *() const { return _begin; }

    bool ended() const {
        return _begin == _end;
    }

    size_t rem() const {
        return _end - _begin;
    }

    T curr() const {
        return *_begin;
    }

    T next() {
        return *_begin++;
    }

    bool put(T c) {
        if (_begin == _end) {
            return true;
        }

        *_begin++ = c;
        return false;
    }

    void skip() {
        ++_begin;
    }

    void skip(size_t n) {
        _begin += n;
    }

    constexpr T *buf() {
        return _begin;
    }

    constexpr T const *buf() const {
        return _begin;
    }

    constexpr size_t len() const {
        return _end - _begin;
    }
};

} // namespace Karm
