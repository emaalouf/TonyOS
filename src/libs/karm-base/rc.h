#pragma once

#include <karm-meta/traits.h>

#include "_prelude.h"

#include "opt.h"
#include "ordr.h"
#include "panic.h"
#include "ref.h"
#include "std.h"

namespace Karm {

struct _Rc {
    int _strong = 0;
    int _weak = 0;

    virtual ~_Rc() = default;

    virtual void *_unwrap() = 0;

    virtual Meta::Id id() = 0;

    bool dying() {
        return _strong == 0;
    }

    _Rc *collect() {
        if (_strong == 0 and _weak == 0) {
            delete this;
            return nullptr;
        }

        return this;
    }

    _Rc *refStrong() {
        _strong++;
        if (_strong == 0) {
            panic("refStrong() overflow");
        }
        return this;
    }

    _Rc *derefStrong() {
        _strong--;
        if (_strong < 0) {
            panic("derefStrong() underflow");
        }
        return collect();
    }

    template <typename T>
    T &unwrapStrong() {
        if (_strong == 0) {
            panic("unwrapStrong() called on weak");
        }
        return *static_cast<T *>(_unwrap());
    }

    _Rc *refWeak() {
        _weak++;
        if (_weak == 0) {
            panic("refWeak() overflow");
        }
        return this;
    }

    _Rc *derefWeak() {
        _weak--;
        if (_weak < 0) {
            panic("derefWeak() underflow");
        }
        return collect();
    }

    template <typename T>
    _Rc *unwrapWeak() {
        if (_weak == 0) {
            panic("unwrapWeak()");
        }

        return *static_cast<T *>(_unwrap());
    }
};

template <typename T>
struct Rc : public _Rc {
    T _buf{};

    template <typename... Args>
    Rc(Args &&...args) : _buf(std::forward<Args>(args)...) {}

    void *_unwrap() override { return &_buf; }
    Meta::Id id() override { return Meta::makeId<T>(); }
};

inline _Rc *tryRefStrong(_Rc *rc) {
    return rc ? rc->refStrong() : nullptr;
}

template <typename T>
struct Strong {
    _Rc *_rc{};

    constexpr Strong() = delete;

    constexpr Strong(_Rc *ptr) : _rc(ptr->refStrong()) {}

    constexpr Strong(Strong const &other) : _rc(other._rc->refStrong()) {}

    constexpr Strong(Strong &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    template <Meta::Derive<T> U>
    constexpr Strong(Strong<U> const &other) : _rc(other._rc->refStrong()) {}

    template <Meta::Derive<T> U>
    constexpr Strong(Strong<U> &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    constexpr ~Strong() {
        if (_rc) {
            _rc = _rc->derefStrong();
        }
    }

    constexpr Strong &operator=(Strong const &other) {
        return *this = Strong(other);
    }

    constexpr Strong &operator=(Strong &&other) {
        std::swap(_rc, other._rc);
        return *this;
    }

    constexpr T *operator->() const {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return &_rc->unwrapStrong<T>();
    }

    constexpr Ordr cmp(Strong const &other) const {
        if (_rc == other._rc)
            return Ordr::EQUAL;

        return ::cmp(unwrap(), other.unwrap());
    }

    constexpr T &operator*() const {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    constexpr T const &unwrap() const {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    constexpr T &unwrap() {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    template <typename U>
    constexpr U &unwrap() {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        if (not is<U>()) {
            panic("Unwrapping Strong<T> as Strong<U>");
        }

        return _rc->unwrapStrong<U>();
    }

    template <typename U>
    constexpr U const &unwrap() const {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        if (not is<U>()) {
            panic("Unwrapping Strong<T> as Strong<U>");
        }

        return _rc->unwrapStrong<U>();
    }

    template <typename U>
    constexpr bool is() {
        return Meta::Same<T, U> or
               Meta::Derive<T, U> or
               _rc->id() == Meta::makeId<U>();
    }

    Meta::Id id() const {
        if (not _rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return _rc->id();
    }

    template <typename U>
    constexpr Opt<Strong<U>> as() {
        if (not is<U>()) {
            return NONE;
        }

        return Strong<U>(_rc);
    }
};

template <typename T>
struct OptStrong {
    _Rc *_rc{};

    constexpr OptStrong() = default;

    constexpr OptStrong(None) : _rc(nullptr) {}

    constexpr OptStrong(_Rc *ptr) : _rc(tryRefStrong(ptr)) {}

    constexpr OptStrong(OptStrong const &other) : _rc(tryRefStrong(other._rc)) {}

    constexpr OptStrong(OptStrong &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    template <Meta::Derive<T> U>
    constexpr OptStrong(OptStrong<U> const &other) : _rc(tryRefStrong(other._rc)) {}

    template <Meta::Derive<T> U>
    constexpr OptStrong(OptStrong<U> &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    constexpr OptStrong(Strong<T> const &other) : _rc(tryRefStrong(other._rc)) {}

    constexpr OptStrong(Strong<T> &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    template <Meta::Derive<T> U>
    constexpr OptStrong(Strong<U> const &other) : _rc(tryRefStrong(other._rc)) {}

    template <Meta::Derive<T> U>
    constexpr OptStrong(Strong<U> &&other) : _rc(std::exchange(other._rc, nullptr)) {}

    constexpr ~OptStrong() {
        if (_rc) {
            _rc = _rc->derefStrong();
        }
    }

    constexpr OptStrong &operator=(OptStrong const &other) {
        return *this = Strong(other);
    }

    constexpr OptStrong &operator=(OptStrong &&other) {
        std::swap(_rc, other._rc);
        return *this;
    }

    constexpr operator bool() const { return _rc; }

    constexpr T *operator->() const {
        if (!_rc) {
            panic("Deferencing moved from Strong<T>");
        }

        return &_rc->unwrapStrong<T>();
    }

    constexpr Ordr cmp(OptStrong const &other) const {
        if (_rc == other._rc)
            return Ordr::EQUAL;

        if (!_rc or not other._rc) {
            return Ordr::LESS;
        }

        return ::cmp(unwrap(), other.unwrap());
    }

    constexpr T &operator*() const {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    constexpr T const &unwrap() const {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    constexpr T &unwrap() {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        return _rc->unwrapStrong<T>();
    }

    template <typename U>
    constexpr U &unwrap() {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        if (not is<U>()) {
            panic("Unwrapping Strong<T> as Strong<U>");
        }

        return _rc->unwrapStrong<U>();
    }

    template <typename U>
    constexpr U const &unwrap() const {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        if (not is<U>()) {
            panic("Unwrapping Strong<T> as Strong<U>");
        }

        return _rc->unwrapStrong<U>();
    }

    template <typename U>
    constexpr bool is() {
        return Meta::Same<T, U> or
               Meta::Derive<T, U> or
               _rc->id() == Meta::makeId<U>();
    }

    Meta::Id id() const {
        if (!_rc) {
            panic("Deferencing none OptStrong<T>");
        }

        return _rc->id();
    }

    template <typename U>
    constexpr Opt<Strong<U>> as() {
        if (not is<U>()) {
            return NONE;
        }

        return Strong<U>(_rc);
    }
};

template <typename T, typename... Args>
constexpr static Strong<T> makeStrong(Args &&...args) {
    return {new Rc<T>(std::forward<Args>(args)...)};
}

template <typename T>
struct Weak {
    _Rc *_rc{};

    constexpr Weak() = delete;

    template <Meta::Derive<T> U>
    constexpr Weak(Strong<U> const &other)
        : _rc(other._rc->refWeak()) {}

    template <Meta::Derive<T> U>
    constexpr Weak(Weak<U> const &other)
        : _rc(other._rc->refWeak()) {}

    template <Meta::Derive<T> U>
    constexpr Weak(Weak<U> &&other) {
        std::swap(_rc, other._rc);
    }

    constexpr ~Weak() {
        if (_rc) {
            _rc->derefWeak();
        }
    }

    constexpr Weak &operator=(Weak const &other) {
        return *this = Weak(other);
    }

    constexpr Weak &operator=(Weak &&other) {
        std::swap(_rc, other._rc);
        return *this;
    }

    OptStrong<T> lock() const {
        if (not _rc) {
            return NONE;
        }

        return Strong<T>(_rc);
    }

    void visit(auto visitor) {
        if (*this) {
            visitor(**this);
        }
    }
};

template <typename T>
using OptWeak = Opt<Weak<T>>;

} // namespace Karm
