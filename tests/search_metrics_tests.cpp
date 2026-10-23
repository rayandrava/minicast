#include "search_metrics.hpp"
#include <cmath>
#include <iostream>

using namespace mini;

int checks = 0;
int failures = 0;
void check(bool passed, const char* name) {
    ++checks;
    if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
bool nearly_equal(float actual, float expected) { return std::fabs(actual - expected) < .0001f; }

int main() {
    // A 19-DIP search font becomes a 29-pixel HFONT at 150%. Treating that
    // physical height as DIPs produced 435 twips and a mismatched caret.
    check(SearchInputScale(144).font_twips(29) == 290,
          "150% font converts 29 physical pixels to 290 twips, not 435");

    // HFONT heights are rounded to whole pixels. Expected twips retain that
    // rounding while the logical search size stays within half a pixel of 19 DIP.
    struct FontCase { UINT dpi; int pixels; LONG twips; };
    for (const auto& sample : {
        FontCase{96, 19, 285}, FontCase{120, 24, 288}, FontCase{144, 29, 290},
        FontCase{168, 33, 283}, FontCase{192, 38, 285}, FontCase{240, 48, 288},
        FontCase{288, 57, 285}}) {
        const SearchInputScale scale(sample.dpi);
        check(scale.font_twips(sample.pixels) == sample.twips,
              "font twips preserve the native font at common display scales");
        check(std::fabs(scale.dips(static_cast<float>(sample.pixels)) - 19.f) <=
                  scale.dips(.5f) + .0001f,
              "placeholder size stays within native pixel rounding of 19 DIP");
    }

    const SearchInputScale fallback(0);
    check(fallback.dpi == 96, "unavailable window DPI falls back to 96");
    check(fallback.font_twips(19) == 285, "fallback font retains the default size");
    check(nearly_equal(fallback.dips(19.f), 19.f), "fallback coordinates remain finite and unscaled");

    struct CoordinateCase { UINT dpi; float pixels; float dips; };
    for (const auto& sample : {
        CoordinateCase{120, 1.f, .8f}, CoordinateCase{144, 74.f, 49.333333f},
        CoordinateCase{168, 3.f, 1.7142857f}, CoordinateCase{144, -29.f, -19.333333f},
        CoordinateCase{120, 140.f, 112.f}, CoordinateCase{168, 168.f, 96.f}}) {
        check(nearly_equal(SearchInputScale(sample.dpi).dips(sample.pixels), sample.dips),
              "caret and selection coordinates retain fractional and negative positions");
    }

    const SearchInputScale fractional(144);
    check(nearly_equal(fractional.dips(30.f) - fractional.dips(29.f), 2.f / 3.f),
          "one physical pixel caret remains one pixel after D2D scaling");
    check(nearly_equal(fractional.dips(143.f) - fractional.dips(29.f), 76.f),
          "selection width uses the same coordinate scale as text");
    check(nearly_equal(fractional.dips(38.f) - fractional.dips(2.f), 24.f),
          "selection height uses the same coordinate scale as text");

    std::cout << checks << " search metrics checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
