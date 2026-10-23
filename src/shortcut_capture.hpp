#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace mini {
struct ShortcutChord { UINT modifiers=0,key=0; };
enum class ShortcutInput { ignore, capture, clear, cancel, accept, next, previous };
enum class ShortcutProblem { none, empty, modifier, needs_modifier, reserved };

inline bool shortcut_modifier(UINT key) {
    return key==VK_CONTROL||key==VK_LCONTROL||key==VK_RCONTROL||
        key==VK_MENU||key==VK_LMENU||key==VK_RMENU||
        key==VK_SHIFT||key==VK_LSHIFT||key==VK_RSHIFT||key==VK_LWIN||key==VK_RWIN;
}

// Dialog navigation applies only to bare keys. Modified Space, Enter, Escape,
// Tab, Delete and Backspace are genuine shortcut candidates.
inline ShortcutInput shortcut_input(UINT key,UINT modifiers,bool repeat=false) {
    if(repeat||!key||key>255||shortcut_modifier(key))return ShortcutInput::ignore;
    if(key==VK_TAB){
        if(!modifiers)return ShortcutInput::next;
        if(modifiers==MOD_SHIFT)return ShortcutInput::previous;
    }
    if(!modifiers){
        if(key==VK_ESCAPE)return ShortcutInput::cancel;
        if(key==VK_RETURN)return ShortcutInput::accept;
        if(key==VK_BACK||key==VK_DELETE)return ShortcutInput::clear;
    }
    return ShortcutInput::capture;
}

inline ShortcutProblem shortcut_problem(ShortcutChord chord) {
    if(!chord.key||chord.key>255)return ShortcutProblem::empty;
    if(shortcut_modifier(chord.key))return ShortcutProblem::modifier;
    if(chord.key==VK_F12)return ShortcutProblem::reserved;
    if(!chord.modifiers&&(chord.key<VK_F1||chord.key>VK_F24))return ShortcutProblem::needs_modifier;
    return ShortcutProblem::none;
}

// Registered hotkeys do not necessarily produce a key-down at the recorder.
// WM_HOTKEY carries the exact chord; never reconstruct it from later key state.
inline ShortcutChord shortcut_from_hotkey(LPARAM value) {
    return {UINT(LOWORD(value))&(MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_WIN),UINT(HIWORD(value))};
}
}
