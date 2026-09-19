// Empty in release: see imconfig.h.
#if !defined(DM_RELEASE)

#include "imgui/imgui.h"
#include "gizmo2d.h"

#include <string.h>
#include <math.h>

namespace Gizmo2D
{

enum Handle
{
    HANDLE_NONE = 0,
    HANDLE_FREE,
    HANDLE_X,
    HANDLE_Y,
    HANDLE_SCALE,
};

// One drag at a time, so the whole widget needs a single slot of state. The id
// is kept so a second gizmo drawn in the same frame cannot steal the drag.
static struct
{
    char id[64];
    int handle;
} s_Active = { { 0 }, HANDLE_NONE };

static const ImU32 COLOR_X = IM_COL32(224, 48, 48, 255);
static const ImU32 COLOR_X_HOT = IM_COL32(255, 128, 128, 255);
static const ImU32 COLOR_Y = IM_COL32(48, 208, 48, 255);
static const ImU32 COLOR_Y_HOT = IM_COL32(128, 255, 128, 255);
static const ImU32 COLOR_FREE = IM_COL32(64, 224, 224, 255);
static const ImU32 COLOR_FREE_HOT = IM_COL32(160, 255, 255, 255);
static const ImU32 COLOR_SCALE = IM_COL32(72, 128, 240, 255);
static const ImU32 COLOR_SCALE_HOT = IM_COL32(150, 190, 255, 255);

// Widget geometry at size == 1, in pixels.
static const float AXIS_GAP = 14.0f;
static const float AXIS_LENGTH = 70.0f;
static const float ARROW_HALF = 6.0f;
static const float ARROW_LENGTH = 14.0f;
static const float FREE_HALF = 6.0f;
static const float SCALE_HALF = 5.5f;
static const float SCALE_OFFSET = 50.0f;
static const float GRAB_HALF = 9.0f;
// Pixels of drag mapped to one doubling of scale.
static const float SCALE_SPEED = 0.006f;

static bool IsNear(const ImVec2& p, const ImVec2& c, float half)
{
    return p.x >= c.x - half && p.x <= c.x + half && p.y >= c.y - half && p.y <= c.y + half;
}

// A point counts as on the axis when it is within the grab band around the
// segment, which makes the thin arrow shaft comfortably clickable.
static bool IsOnSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b, float half)
{
    float min_x = a.x < b.x ? a.x : b.x;
    float max_x = a.x > b.x ? a.x : b.x;
    float min_y = a.y < b.y ? a.y : b.y;
    float max_y = a.y > b.y ? a.y : b.y;
    return p.x >= min_x - half && p.x <= max_x + half && p.y >= min_y - half && p.y <= max_y + half;
}

static void DrawArrow(ImDrawList* draw, const ImVec2& from, const ImVec2& to, ImU32 color, float size)
{
    draw->AddLine(from, to, color, 2.0f * size);
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = sqrtf(dx * dx + dy * dy);
    if (length <= 0.0001f) return;
    dx /= length;
    dy /= length;
    ImVec2 base(to.x - dx * ARROW_LENGTH * size, to.y - dy * ARROW_LENGTH * size);
    ImVec2 side(-dy * ARROW_HALF * size, dx * ARROW_HALF * size);
    draw->AddTriangleFilled(to,
        ImVec2(base.x + side.x, base.y + side.y),
        ImVec2(base.x - side.x, base.y - side.y), color);
}

static void DrawBox(ImDrawList* draw, const ImVec2& c, float half, ImU32 color, bool filled, float size)
{
    ImVec2 a(c.x - half, c.y - half);
    ImVec2 b(c.x + half, c.y + half);
    if (filled)
    {
        draw->AddRectFilled(a, b, color);
    }
    else
    {
        // Hollow, so the anchor pixel it marks stays readable underneath.
        draw->AddRectFilled(a, b, IM_COL32(0, 0, 0, 80));
        draw->AddRect(a, b, color, 0.0f, 0, 2.0f * size);
    }
}

bool Manipulate(const char* id, float* x, float* y, float* scale, float size, int flags)
{
    if (x == 0 || y == 0) return false;
    if (size <= 0.0f) size = 1.0f;

    const bool has_scale = scale != 0 && (flags & FLAGS_NO_SCALE) == 0;
    const bool has_axes = (flags & FLAGS_NO_AXES) == 0;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 origin(*x, *y);
    const ImVec2 mouse = io.MousePos;

    const ImVec2 x_from(origin.x + AXIS_GAP * size, origin.y);
    const ImVec2 x_to(origin.x + AXIS_LENGTH * size, origin.y);
    // Screen y grows downwards; the Y arrow points up so it reads like the
    // editor's, and the drag is inverted to match when it is applied.
    const ImVec2 y_from(origin.x, origin.y - AXIS_GAP * size);
    const ImVec2 y_to(origin.x, origin.y - AXIS_LENGTH * size);
    const ImVec2 scale_at(origin.x + SCALE_OFFSET * size, origin.y - SCALE_OFFSET * size);

    const bool is_mine = s_Active.handle != HANDLE_NONE && strncmp(s_Active.id, id, sizeof(s_Active.id) - 1) == 0;

    int hot = HANDLE_NONE;
    if (s_Active.handle == HANDLE_NONE)
    {
        if (IsNear(mouse, origin, FREE_HALF * size + 3.0f))
        {
            hot = HANDLE_FREE;
        }
        else if (has_scale && IsNear(mouse, scale_at, GRAB_HALF * size))
        {
            hot = HANDLE_SCALE;
        }
        else if (has_axes && IsOnSegment(mouse, x_from, x_to, GRAB_HALF * size))
        {
            hot = HANDLE_X;
        }
        else if (has_axes && IsOnSegment(mouse, y_to, y_from, GRAB_HALF * size))
        {
            hot = HANDLE_Y;
        }

        if (hot != HANDLE_NONE && ImGui::IsMouseClicked(0))
        {
            strncpy(s_Active.id, id, sizeof(s_Active.id) - 1);
            s_Active.id[sizeof(s_Active.id) - 1] = 0;
            s_Active.handle = hot;
        }
    }
    else if (is_mine)
    {
        hot = s_Active.handle;
    }

    bool changed = false;
    if (is_mine)
    {
        if (!ImGui::IsMouseDown(0))
        {
            s_Active.handle = HANDLE_NONE;
            s_Active.id[0] = 0;
        }
        else
        {
            const ImVec2 delta = io.MouseDelta;
            if (delta.x != 0.0f || delta.y != 0.0f)
            {
                switch (s_Active.handle)
                {
                case HANDLE_FREE:
                    *x += delta.x;
                    *y += delta.y;
                    changed = true;
                    break;
                case HANDLE_X:
                    *x += delta.x;
                    changed = true;
                    break;
                case HANDLE_Y:
                    *y += delta.y;
                    changed = true;
                    break;
                case HANDLE_SCALE:
                    if (has_scale)
                    {
                        // Right and up both grow it, so the handle follows the
                        // corner it sits on.
                        *scale *= powf(2.0f, (delta.x - delta.y) * SCALE_SPEED);
                        changed = true;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (has_axes)
    {
        DrawArrow(draw, x_from, x_to, hot == HANDLE_X ? COLOR_X_HOT : COLOR_X, size);
        DrawArrow(draw, y_from, y_to, hot == HANDLE_Y ? COLOR_Y_HOT : COLOR_Y, size);
    }
    if (has_scale)
    {
        draw->AddLine(ImVec2(origin.x + AXIS_GAP * size * 0.7f, origin.y - AXIS_GAP * size * 0.7f),
            scale_at, hot == HANDLE_SCALE ? COLOR_SCALE_HOT : COLOR_SCALE, 1.5f * size);
        DrawBox(draw, scale_at, SCALE_HALF * size, hot == HANDLE_SCALE ? COLOR_SCALE_HOT : COLOR_SCALE, true, size);
    }
    DrawBox(draw, origin, FREE_HALF * size, hot == HANDLE_FREE ? COLOR_FREE_HOT : COLOR_FREE, false, size);

    return changed;
}

bool IsUsing()
{
    return s_Active.handle != HANDLE_NONE;
}

}

#endif // !DM_RELEASE
