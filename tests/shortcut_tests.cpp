#include "shortcut_capture.hpp"
#include <iostream>

using namespace mini;

int checks = 0;
int failures = 0;
void check(bool passed, const char* name) {
    ++checks;
    if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}

int main() {
    // Single modifiers are intentional; the recorder must not add Ctrl+Alt.
    struct SpaceCase { UINT modifiers; const char* name; };
    for (const auto& sample : {
        SpaceCase{MOD_CONTROL, "Ctrl+Space"},
        SpaceCase{MOD_ALT, "Alt+Space"},
        SpaceCase{MOD_SHIFT, "Shift+Space"},
        SpaceCase{MOD_WIN, "Win+Space"}}) {
        check(shortcut_input(VK_SPACE, sample.modifiers) == ShortcutInput::capture, sample.name);
        check(shortcut_problem({sample.modifiers, VK_SPACE}) == ShortcutProblem::none, sample.name);
        const auto chord = shortcut_from_hotkey(MAKELPARAM(sample.modifiers, VK_SPACE));
        check(chord.modifiers == sample.modifiers && chord.key == VK_SPACE, sample.name);
    }

    check(shortcut_input(VK_TAB, 0) == ShortcutInput::next, "Tab moves to the next control");
    check(shortcut_input(VK_TAB, MOD_SHIFT) == ShortcutInput::previous, "Shift+Tab moves to the previous control");
    check(shortcut_input(VK_ESCAPE, 0) == ShortcutInput::cancel, "Escape cancels recording");
    check(shortcut_input(VK_RETURN, 0) == ShortcutInput::accept, "Enter accepts the captured chord");
    check(shortcut_input(VK_BACK, 0) == ShortcutInput::clear, "Backspace clears the draft");
    check(shortcut_input(VK_DELETE, 0) == ShortcutInput::clear, "Delete clears the draft");

    // Modified dialog keys are shortcut candidates, not dialog actions.
    for (UINT key : {VK_TAB, VK_ESCAPE, VK_RETURN, VK_BACK, VK_DELETE}) {
        check(shortcut_input(key, MOD_CONTROL) == ShortcutInput::capture, "Ctrl+dialog key records a shortcut");
        check(shortcut_input(key, MOD_ALT) == ShortcutInput::capture, "Alt+dialog key records a shortcut");
        check(shortcut_input(key, MOD_WIN) == ShortcutInput::capture, "Win+dialog key records a shortcut");
    }
    check(shortcut_input(VK_TAB, MOD_CONTROL | MOD_SHIFT) == ShortcutInput::capture,
          "Ctrl+Shift+Tab records instead of moving focus");
    check(shortcut_input(VK_RETURN, MOD_SHIFT) == ShortcutInput::capture,
          "Shift+Enter records instead of accepting");

    for (UINT key : {VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU,
                     VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN}) {
        check(shortcut_input(key, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN) == ShortcutInput::ignore,
              "modifier key alone cannot replace the captured chord");
    }
    for (UINT key : {VK_SPACE, VK_TAB, VK_ESCAPE, VK_RETURN, VK_BACK, VK_DELETE}) {
        check(shortcut_input(key, 0, true) == ShortcutInput::ignore, "repeated keys cannot change the draft or navigate");
    }
    check(shortcut_input(VK_SPACE, MOD_ALT, true) == ShortcutInput::ignore,
          "repeated Alt+Space cannot replace the captured chord");
    check(shortcut_input(0, MOD_CONTROL) == ShortcutInput::ignore, "missing key is ignored");
    check(shortcut_input(256, MOD_CONTROL) == ShortcutInput::ignore, "out of range key is ignored");

    check(shortcut_problem({0, 0}) == ShortcutProblem::empty, "empty draft cannot be applied");
    check(shortcut_problem({MOD_CONTROL, 256}) == ShortcutProblem::empty, "invalid key cannot be applied");
    check(shortcut_problem({MOD_CONTROL, VK_CONTROL}) == ShortcutProblem::modifier,
          "modifier-only draft cannot be applied");
    check(shortcut_problem({0, VK_SPACE}) == ShortcutProblem::needs_modifier,
          "bare Space cannot become a global shortcut");
    check(shortcut_problem({0, 'A'}) == ShortcutProblem::needs_modifier,
          "bare letters cannot become a global shortcut");
    check(shortcut_problem({0, VK_F1}) == ShortcutProblem::none, "bare F1 is allowed");
    check(shortcut_problem({0, VK_F11}) == ShortcutProblem::none, "bare F11 is allowed");
    check(shortcut_problem({0, VK_F13}) == ShortcutProblem::none, "bare F13 is allowed");
    check(shortcut_problem({0, VK_F24}) == ShortcutProblem::none, "bare F24 is allowed");
    check(shortcut_problem({0, VK_F12}) == ShortcutProblem::reserved, "bare F12 stays reserved by Windows");
    check(shortcut_problem({MOD_CONTROL | MOD_ALT, VK_F12}) == ShortcutProblem::reserved,
          "modified F12 stays reserved by Windows");

    const auto combined = shortcut_from_hotkey(MAKELPARAM(MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN, 'K'));
    check(combined.modifiers == (MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN) && combined.key == 'K',
          "WM_HOTKEY preserves every recorded modifier and key");
    const auto function = shortcut_from_hotkey(MAKELPARAM(0, VK_F9));
    check(function.modifiers == 0 && function.key == VK_F9, "WM_HOTKEY preserves a bare function key");
    const auto filtered = shortcut_from_hotkey(MAKELPARAM(MOD_ALT | MOD_NOREPEAT | 0x8000, VK_SPACE));
    check(filtered.modifiers == MOD_ALT && filtered.key == VK_SPACE,
          "WM_HOTKEY decoding excludes registration flags and unrelated bits");

    std::cout << checks << " shortcut checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
