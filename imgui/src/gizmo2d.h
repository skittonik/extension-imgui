// gizmo2d.h
// A screen-space 2D transform gizmo drawn on ImGui's foreground draw list.
//
// Dear ImGui has no gizmo of its own, and ImGuizmo (vendored alongside this
// file) is built around a 3D view/projection pair - overkill for dragging a
// sprite's anchor around a 2D scene. This is the 2D counterpart: a cyan square
// at the origin for free movement, a red X arrow and a green Y arrow for
// single-axis movement, and a blue square handle for uniform scale.
//
// Everything is in ImGui screen coordinates (pixels, y down). The caller owns
// the values; Manipulate() writes the dragged result back through the pointers
// and reports whether a handle is currently held.

#pragma once

namespace Gizmo2D
{
    enum Flags
    {
        FLAGS_NONE = 0,
        // Hide the scale handle for values that only ever move.
        FLAGS_NO_SCALE = 1 << 0,
        // Hide the single-axis arrows, leaving only free movement.
        FLAGS_NO_AXES = 1 << 1,
    };

    // id separates the drag state of several gizmos alive in the same frame.
    // scale may be null when FLAGS_NO_SCALE is set. size scales the whole
    // widget so it stays usable on a high-DPI window.
    bool Manipulate(const char* id, float* x, float* y, float* scale, float size, int flags);

    // True while any gizmo is being dragged - the host must swallow the click
    // so it never also reaches the game underneath.
    bool IsUsing();
}
