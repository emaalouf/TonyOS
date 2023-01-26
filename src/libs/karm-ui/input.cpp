#include "input.h"

#include "funcs.h"
#include "layout.h"
#include "view.h"

namespace Karm::Ui {

/* --- Button ---------------------------------------------------------------- */

ButtonStyle ButtonStyle::regular() {
    return {
        .idleStyle = {
            .borderRadius = RADIUS,
            .backgroundPaint = Gfx::ZINC700,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::ZINC600,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::ZINC600,
            .backgroundPaint = Gfx::ZINC700,
        },
    };
}

ButtonStyle ButtonStyle::secondary() {
    return {
        .idleStyle = {
            .borderRadius = RADIUS,
            .backgroundPaint = Gfx::ZINC800,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::ZINC700,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::ZINC700,
            .backgroundPaint = Gfx::ZINC800,
        },
    };
}

ButtonStyle ButtonStyle::primary() {
    return {
        .idleStyle = {
            .borderRadius = RADIUS,
            .backgroundPaint = Gfx::BLUE700,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::BLUE600,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::BLUE600,
            .backgroundPaint = Gfx::BLUE700,
        },
    };
}

ButtonStyle ButtonStyle::outline() {
    return {
        .idleStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::ZINC700,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::ZINC600,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::ZINC600,
            .backgroundPaint = Gfx::ZINC700,
        },
    };
}

ButtonStyle ButtonStyle::subtle() {
    return {
        .idleStyle = {
            .foregroundPaint = Gfx::ZINC300,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::ZINC600,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::ZINC600,
            .backgroundPaint = Gfx::ZINC700,
        },
    };
}

ButtonStyle ButtonStyle::destructive() {
    return {
        .idleStyle = {
            .borderRadius = RADIUS,
            .foregroundPaint = Gfx::RED500,
        },
        .hoverStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .backgroundPaint = Gfx::RED600,
        },
        .pressStyle = {
            .borderRadius = RADIUS,
            .borderWidth = 1,
            .borderPaint = Gfx::RED600,
            .backgroundPaint = Gfx::RED700,
        },
    };
}

ButtonStyle ButtonStyle::withRadius(Gfx::BorderRadius radius) const {
    return {
        idleStyle.withRadius(radius),
        hoverStyle.withRadius(radius),
        pressStyle.withRadius(radius),
    };
}

ButtonStyle ButtonStyle::withForegroundPaint(Gfx::Paint paint) const {
    return {
        idleStyle.withForegroundPaint(paint),
        hoverStyle.withForegroundPaint(paint),
        pressStyle.withForegroundPaint(paint),
    };
}

struct Button : public _Box<Button> {
    OnPress _onPress;
    ButtonStyle _buttonStyle = ButtonStyle::regular();
    MouseListener _mouseListener;

    Button(OnPress onPress, ButtonStyle style, Child child)
        : _Box<Button>(child),
          _onPress(std::move(onPress)),
          _buttonStyle(style) {}

    void reconcile(Button &o) override {
        _buttonStyle = o._buttonStyle;
        _onPress = std::move(o._onPress);
        _Box<Button>::reconcile(o);
    }

    BoxStyle &boxStyle() override {
        if (not _onPress) {
            return _buttonStyle.disabledStyle;
        }

        if (_mouseListener.isIdle()) {
            return _buttonStyle.idleStyle;
        } else if (_mouseListener.isHover()) {
            return _buttonStyle.hoverStyle;
        } else {
            return _buttonStyle.pressStyle;
        }
    }

    void event(Events::Event &e) override {
        if (_onPress and _mouseListener.listen(*this, e)) {
            _onPress(*this);
        }
    };
};

Child button(OnPress onPress, ButtonStyle style, Child child) {
    return makeStrong<Button>(std::move(onPress), style, child);
}

Child button(OnPress onPress, ButtonStyle style, Str t) {
    return button(
        std::move(onPress),
        style,
        minSize(
            {UNCONSTRAINED, 36},
            center(
                spacing(
                    {16, 6},
                    text(t)))));
}

Child button(OnPress onPress, ButtonStyle style, Media::Icon i) {
    return button(
        std::move(onPress),
        style,
        minSize(
            {36, 36},
            center(
                spacing(
                    {6, 6},
                    icon(i)))));
}

Child button(OnPress onPress, ButtonStyle style, Media::Icon i, Str t) {
    return button(
        std::move(onPress),
        style,
        minSize(
            {36, 36},
            spacing(
                {12, 6, 16, 6},
                hflow(
                    8,
                    Layout::Align::CENTER,
                    icon(i),
                    text(t)))));
}

Child button(OnPress onPress, Child child) {
    return button(std::move(onPress), ButtonStyle::regular(), child);
}

Child button(OnPress onPress, Str t) {
    return button(std::move(onPress), ButtonStyle::regular(), t);
}

Child button(OnPress onPress, Media::Icons i) {
    return button(std::move(onPress), ButtonStyle::regular(), i);
}

Child button(OnPress onPress, Media::Icons i, Str t) {
    return button(std::move(onPress), ButtonStyle::regular(), i, t);
}

/* --- Input ---------------------------------------------------------------- */

struct TextModel {};

struct Input : public View<Input> {
    TextStyle _style;
    String _text;
    OnChange<String> _onChange;
    size_t _cursor = 0;
    Opt<Media::FontMesure> _mesure;

    Input(TextStyle style, String text, OnChange<String> onChange)
        : _style(style), _text(text), _onChange(std::move(onChange)) {}

    void reconcile(Input &o) override {
        _text = o._text;
        _mesure = NONE;
    }

    Media::FontMesure mesure() {
        if (_mesure) {
            return *_mesure;
        }
        _mesure = _style.font.mesureStr(_text);
        return *_mesure;
    }

    void paint(Gfx::Context &g, Math::Recti) override {
        g.save();

        auto m = mesure();
        auto baseline = bound().topStart() + m.baseline.cast<int>();

        if (_style.color) {
            g.fillStyle(*_style.color);
        }

        g.textFont(_style.font);
        g.fill(baseline, _text);

        if (debugShowLayoutBounds) {
            g.debugLine(
                {
                    bound().topStart() + m.baseline.cast<int>(),
                    bound().topEnd() + m.baseline.cast<int>(),
                },
                Gfx::PINK);
            g.debugRect(bound(), Gfx::CYAN);
        }

        g.restore();
    }

    Math::Vec2i size(Math::Vec2i, Layout::Hint) override {
        return mesure().linebound.size().cast<int>();
    }
};

Child input(TextStyle style, String text, OnChange<String> onChange) {
    return makeStrong<Input>(style, text, std::move(onChange));
}

Child input(String text, OnChange<String> onChange) {
    return makeStrong<Input>(TextStyle::bodyMedium(), text, std::move(onChange));
}

/* --- Toggle --------------------------------------------------------------- */

struct Toggle : public View<Toggle> {
    bool _value = false;
    OnChange<bool> _onChange;
    MouseListener _mouseListener;

    Toggle(bool value, OnChange<bool> onChange)
        : _value(value), _onChange(std::move(onChange)) {
    }

    void reconcile(Toggle &o) override {
        _value = o._value;
        _onChange = std::move(o._onChange);
    }

    void paint(Gfx::Context &g, Math::Recti) override {
        g.save();

        auto thumb = bound().hsplit(26).get(_value).shrink(_value ? 4 : 6);

        if (_value) {
            g.fillStyle(_mouseListener.isHover() ? Gfx::BLUE600 : Gfx::BLUE700);
            g.fill(bound(), 999);

            g.fillStyle(Gfx::WHITE);
            g.fill(thumb, 999);

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::BLUE600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 999);
            }
        } else {
            g.fillStyle(_mouseListener.isHover() ? Gfx::ZINC600 : Gfx::ZINC700);
            g.fill(bound(), 999);

            g.fillStyle(_mouseListener.isHover() ? Gfx::ZINC400 : Gfx::ZINC500);
            g.fill(thumb, 999);

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::ZINC600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 999);
            }
        }

        g.restore();
    }

    void event(Events::Event &e) override {
        if (_mouseListener.listen(*this, e)) {
            _value = not _value;
            _onChange(*this, _value);
        }
    }

    Math::Vec2i size(Math::Vec2i, Layout::Hint) override {
        return {52, 26};
    }
};

Child toggle(bool value, OnChange<bool> onChange) {
    return makeStrong<Toggle>(value, std::move(onChange));
}

/* --- Checkbox ------------------------------------------------------------- */

struct Checkbox : public View<Checkbox> {
    bool _value = false;
    OnChange<bool> _onChange;
    MouseListener _mouseListener;

    Checkbox(bool value, OnChange<bool> onChange)
        : _value(value), _onChange(std::move(onChange)) {
    }

    void reconcile(Checkbox &o) override {
        _value = o._value;
        _onChange = std::move(o._onChange);
    }

    void paint(Gfx::Context &g, Math::Recti) override {
        g.save();

        if (_value) {
            g.fillStyle(_mouseListener.isHover() ? Gfx::BLUE600 : Gfx::BLUE700);
            g.fill(bound(), 4);

            g.fillStyle(Gfx::WHITE);
            g.fill(bound().topStart(), Media::Icon{Media::Icons::CHECK_BOLD, 26});

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::BLUE600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 4);
            }
        } else {
            g.fillStyle(_mouseListener.isHover() ? Gfx::ZINC600 : Gfx::ZINC700);
            g.fill(bound(), 4);

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::ZINC600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 4);
            }
        }

        g.restore();
    }

    void event(Events::Event &e) override {
        if (_mouseListener.listen(*this, e)) {
            _value = not _value;
            _onChange(*this, _value);
        }
    }

    Math::Vec2i size(Math::Vec2i, Layout::Hint) override {
        return {26, 26};
    }
};

Child checkbox(bool value, OnChange<bool> onChange) {
    return makeStrong<Checkbox>(value, std::move(onChange));
}

/* --- Radio ----------------------------------------------------------------- */

struct Radio : public View<Radio> {
    bool _value = false;
    OnChange<bool> _onChange;
    MouseListener _mouseListener;

    Radio(bool value, OnChange<bool> onChange)
        : _value(value), _onChange(std::move(onChange)) {
    }

    void reconcile(Radio &o) override {
        _value = o._value;
        _onChange = std::move(o._onChange);
    }

    void paint(Gfx::Context &g, Math::Recti) override {
        g.save();
        if (_value) {
            g.fillStyle(_mouseListener.isHover() ? Gfx::BLUE600 : Gfx::BLUE700);
            g.fill(bound(), 999);

            g.fillStyle(Gfx::WHITE);
            g.fill(bound().shrink(6), 999);

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::BLUE600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 999);
            }
        } else {
            g.fillStyle(_mouseListener.isHover() ? Gfx::ZINC600 : Gfx::ZINC700);
            g.fill(bound(), 999);

            if (_mouseListener.isPress()) {
                g.strokeStyle(Gfx::stroke(Gfx::ZINC600).withWidth(1).withAlign(Gfx::INSIDE_ALIGN));
                g.stroke(bound(), 999);
            }
        }
        g.restore();
    }

    void event(Events::Event &e) override {
        if (_mouseListener.listen(*this, e)) {
            _value = not _value;
            _onChange(*this, _value);
        }
    }

    Math::Vec2i size(Math::Vec2i, Layout::Hint) override {
        return {26, 26};
    }
};

Child radio(bool value, OnChange<bool> onChange) {
    return makeStrong<Radio>(value, std::move(onChange));
}

/* --- Slider ---------------------------------------------------------------- */

SliderStyle SliderStyle::regular() {
    return {
        .thumbStyle = {
            .margin = 2,
            .borderRadius = 999,
            .borderWidth = 4,
            .borderPaint = Gfx::WHITE,
            .backgroundPaint = Gfx::BLUE700,
        },
        .trackSize = {128, 26},
        .trackStyle = {
            .margin = 8,
            .borderRadius = 999,
            .backgroundPaint = Gfx::ZINC700,
        },
        .valueStyle = BoxStyle{
            .margin = 8,
            .borderRadius = 999,
            .backgroundPaint = Gfx::BLUE700,
        },
    };
}

SliderStyle SliderStyle::hsv() {
    return {
        .thumbStyle = {
            .margin = 4,
            .borderRadius = 999,
            .borderWidth = 2,
            .borderPaint = Gfx::WHITE,
        },
        .trackSize = {128, 26},
        .trackStyle = {
            .margin = 2,
            .borderRadius = 999,
            .backgroundPaint = Gfx::Gradient::hsv(),
        },
    };
}

SliderStyle SliderStyle::gradiant(Gfx::Color from, Gfx::Color to) {
    return {
        .thumbStyle = {
            .margin = 4,
            .borderRadius = 999,
            .borderWidth = 2,
            .borderPaint = Gfx::WHITE,
        },
        .trackSize = {128, 26},
        .trackStyle = {
            .margin = 2,
            .borderRadius = 999,
            .backgroundPaint = Gfx::Gradient::hlinear().withColors(from, to),
        },
    };
}

struct Slider : public View<Slider> {
    SliderStyle _style;
    double _value = 0.0f;
    OnChange<double> _onChange;
    MouseListener _mouseListener;

    Slider(SliderStyle style, double value, OnChange<double> onChange)
        : _style(style), _value(value), _onChange(std::move(onChange)) {
    }

    void reconcile(Slider &o) override {
        _style = o._style;
        _value = o._value;
        _onChange = std::move(o._onChange);
    }

    auto thumbRadius() {
        return _style.trackSize.min() / 2;
    }

    void paint(Gfx::Context &g, Math::Recti) override {
        if (_style.valueStyle) {
            auto [rhs, lhs] = bound().hsplit(bound().width * _value);

            g.save();
            g.clip(rhs);
            _style.valueStyle->paint(g, bound());
            g.restore();

            g.save();
            g.clip(lhs);
            _style.trackStyle.paint(g, bound());
            g.restore();
        } else {
            _style.trackStyle.paint(g, bound());
        }

        auto thumbX = bound().x + ((bound().width - thumbRadius() * 2) * _value);
        Math::Recti thumbBound = {
            {
                (int)thumbX,
                bound().top(),
            },
            thumbRadius() * 2,
        };

        _style.thumbStyle.paint(g, thumbBound);
    }

    void event(Events::Event &e) override {
        _mouseListener.listen(*this, e);

        if (_mouseListener.isPress() and e.is<Events::MouseEvent>()) {
            auto p = _mouseListener.pos();
            _value = (p.x - thumbRadius()) / ((double)bound().width - thumbRadius() * 2);
            _value = clamp01(_value);
            if (_onChange)
                _onChange(*this, _value);
            else
                Ui::shouldRepaint(*this);
        }
    }

    Math::Vec2i size(Math::Vec2i, Layout::Hint) override {
        return _style.trackSize;
    }
};

Child slider(SliderStyle style, double value, OnChange<double> onChange) {
    return makeStrong<Slider>(style, value, std::move(onChange));
}

Child slider(double value, OnChange<double> onChange) {
    return makeStrong<Slider>(SliderStyle::regular(), value, std::move(onChange));
}

} // namespace Karm::Ui
