// imgui-notify by patrickcjk
// https://github.com/patrickcjk/imgui-notify
//
// Changes applied:
//  - Includes <imgui.h> and <imgui_internal.h> so the header is self-contained.
//  - get_default_title() returns const char* (MSVC /Zc:strictStrings safe).
//  - get_type()/get_color()/get_phase() return by value (no dangling refs).
//  - get_color()/get_icon() have a fallback return (kills C4715).
//  - Toasts call BringWindowToDisplayFront() after Begin() so they stay
//    on top of every docked window without stealing focus.
//  - MergeIconsWithLatestFont uses void (not VOID) to kill C4508 on Linux/MSVC.

#ifndef IMGUI_NOTIFY
#define IMGUI_NOTIFY

#pragma once
#include <vector>
#include <string>

#include "font_awesome_5.h"
#include "fa_solid_900.h"

// imgui_notify needs the public ImGui API plus BringWindowToDisplayFront
// (the latter lives in imgui_internal.h).
#include <imgui.h>
#include <imgui_internal.h>

#define NOTIFY_MAX_MSG_LENGTH       4096        // Max message content length
#define NOTIFY_PADDING_X            20.f        // Bottom-left X padding
#define NOTIFY_PADDING_Y            20.f        // Bottom-left Y padding
#define NOTIFY_PADDING_MESSAGE_Y    10.f        // Padding Y between each message
#define NOTIFY_FADE_IN_OUT_TIME     150         // Fade in and out duration
#define NOTIFY_DEFAULT_DISMISS      3000        // Auto dismiss after X ms
#define NOTIFY_OPACITY              1.0f        // 0-1 Toast opacity
#define NOTIFY_TOAST_FLAGS          ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing

#define NOTIFY_USE_SEPARATOR

#define NOTIFY_INLINE               inline
#define NOTIFY_NULL_OR_EMPTY(str)   (!str || !strlen(str))
#define NOTIFY_FORMAT(fn, format, ...) \
    if (format) { \
        va_list args; \
        va_start(args, format); \
        fn(format, args, __VA_ARGS__); \
        va_end(args); \
    }

typedef int ImGuiToastType;
typedef int ImGuiToastPhase;
typedef int ImGuiToastPos;

enum ImGuiToastType_
{
    ImGuiToastType_None,
    ImGuiToastType_Success,
    ImGuiToastType_Warning,
    ImGuiToastType_Error,
    ImGuiToastType_Info,
    ImGuiToastType_COUNT
};

enum ImGuiToastPhase_
{
    ImGuiToastPhase_FadeIn,
    ImGuiToastPhase_Wait,
    ImGuiToastPhase_FadeOut,
    ImGuiToastPhase_Expired,
    ImGuiToastPhase_COUNT
};

enum ImGuiToastPos_
{
    ImGuiToastPos_TopLeft,
    ImGuiToastPos_TopCenter,
    ImGuiToastPos_TopRight,
    ImGuiToastPos_BottomLeft,
    ImGuiToastPos_BottomCenter,
    ImGuiToastPos_BottomRight,
    ImGuiToastPos_Center,
    ImGuiToastPos_COUNT
};

class ImGuiToast
{
private:
    ImGuiToastType  type          = ImGuiToastType_None;
    char            title[NOTIFY_MAX_MSG_LENGTH];
    char            content[NOTIFY_MAX_MSG_LENGTH];
    int             dismiss_time  = NOTIFY_DEFAULT_DISMISS;
    uint64_t        creation_time = 0;

private:
    NOTIFY_INLINE auto set_title(const char* format, va_list args)
        { vsnprintf(this->title, sizeof(this->title), format, args); }

    NOTIFY_INLINE auto set_content(const char* format, va_list args)
        { vsnprintf(this->content, sizeof(this->content), format, args); }

public:
    NOTIFY_INLINE auto set_title(const char* format, ...) -> void
    {
        NOTIFY_FORMAT(this->set_title, format);
    }

    NOTIFY_INLINE auto set_content(const char* format, ...) -> void
    {
        NOTIFY_FORMAT(this->set_content, format);
    }

    NOTIFY_INLINE auto set_type(const ImGuiToastType& type) -> void
    {
        IM_ASSERT(type < ImGuiToastType_COUNT);
        this->type = type;
    }

public:
    NOTIFY_INLINE auto get_title() -> char*
    {
        return this->title;
    }

    // Returns const char* — string literals are const char[N] and would be
    // rejected by MSVC /Zc:strictStrings otherwise.
    NOTIFY_INLINE auto get_default_title() -> const char*
    {
        if (!strlen(this->title))
        {
            switch (this->type)
            {
            case ImGuiToastType_None:    return NULL;
            case ImGuiToastType_Success: return "Success";
            case ImGuiToastType_Warning: return "Warning";
            case ImGuiToastType_Error:   return "Error";
            case ImGuiToastType_Info:    return "Info";
            }
        }
        return this->title;
    }

    // Returned by value: int, no dangling reference.
    NOTIFY_INLINE auto get_type() -> ImGuiToastType
    {
        return this->type;
    }

    // Returned by value: ImVec4 is small, avoids C4172.
    NOTIFY_INLINE auto get_color() -> ImVec4
    {
        switch (this->type)
        {
        case ImGuiToastType_None:    return { 1.0f, 1.0f, 1.0f, 1.0f };      // White
        case ImGuiToastType_Success: return { 0.0f, 1.0f, 0.0f, 1.0f };      // Green
        case ImGuiToastType_Warning: return { 1.0f, 1.0f, 0.0f, 1.0f };      // Yellow
        case ImGuiToastType_Error:   return { 1.0f, 0.0f, 0.0f, 1.0f };      // Red
        case ImGuiToastType_Info:    return { 0.0f, 0.615f, 1.0f, 1.0f };    // Blue
        }
        return { 1.0f, 1.0f, 1.0f, 1.0f };  // fallback
    }

    NOTIFY_INLINE auto get_icon() -> const char*
    {
        switch (this->type)
        {
        case ImGuiToastType_None:    return NULL;
        case ImGuiToastType_Success: return ICON_FA_CHECK_CIRCLE;
        case ImGuiToastType_Warning: return ICON_FA_EXCLAMATION_TRIANGLE;
        case ImGuiToastType_Error:   return ICON_FA_TIMES_CIRCLE;
        case ImGuiToastType_Info:    return ICON_FA_INFO_CIRCLE;
        }
        return NULL;  // fallback
    }

    NOTIFY_INLINE auto get_content() -> char*
    {
        return this->content;
    }

    NOTIFY_INLINE auto get_elapsed_time()
    {
        return GetTickCount64() - this->creation_time;
    }

    // Returned by value: int, no dangling reference.
    NOTIFY_INLINE auto get_phase() -> ImGuiToastPhase
    {
        const auto elapsed = get_elapsed_time();

        if (elapsed > NOTIFY_FADE_IN_OUT_TIME + this->dismiss_time + NOTIFY_FADE_IN_OUT_TIME)
            return ImGuiToastPhase_Expired;
        else if (elapsed > NOTIFY_FADE_IN_OUT_TIME + this->dismiss_time)
            return ImGuiToastPhase_FadeOut;
        else if (elapsed > NOTIFY_FADE_IN_OUT_TIME)
            return ImGuiToastPhase_Wait;
        else
            return ImGuiToastPhase_FadeIn;
    }

    NOTIFY_INLINE auto get_fade_percent() -> const float
    {
        const auto phase   = get_phase();
        const auto elapsed = get_elapsed_time();

        if (phase == ImGuiToastPhase_FadeIn)
        {
            return ((float)elapsed / (float)NOTIFY_FADE_IN_OUT_TIME) * NOTIFY_OPACITY;
        }
        else if (phase == ImGuiToastPhase_FadeOut)
        {
            return (1.f - (((float)elapsed - (float)NOTIFY_FADE_IN_OUT_TIME - (float)this->dismiss_time) / (float)NOTIFY_FADE_IN_OUT_TIME)) * NOTIFY_OPACITY;
        }

        return 1.f * NOTIFY_OPACITY;
    }

public:
    ImGuiToast(ImGuiToastType type, int dismiss_time = NOTIFY_DEFAULT_DISMISS)
    {
        IM_ASSERT(type < ImGuiToastType_COUNT);

        this->type          = type;
        this->dismiss_time  = dismiss_time;
        this->creation_time = GetTickCount64();

        memset(this->title,   0, sizeof(this->title));
        memset(this->content, 0, sizeof(this->content));
    }

    ImGuiToast(ImGuiToastType type, const char* format, ...) : ImGuiToast(type)
    {
        NOTIFY_FORMAT(this->set_content, format);
    }

    ImGuiToast(ImGuiToastType type, int dismiss_time, const char* format, ...) : ImGuiToast(type, dismiss_time)
    {
        NOTIFY_FORMAT(this->set_content, format);
    }
};

namespace ImGui
{
    NOTIFY_INLINE std::vector<ImGuiToast> notifications;

    /// <summary>
    /// Insert a new toast in the list
    /// </summary>
    NOTIFY_INLINE void InsertNotification(const ImGuiToast& toast)
    {
        notifications.push_back(toast);
    }

    /// <summary>
    /// Remove a toast from the list by its index
    /// </summary>
    NOTIFY_INLINE void RemoveNotification(int index)
    {
        notifications.erase(notifications.begin() + index);
    }

    /// <summary>
    /// Render toasts, call at the end of your rendering!
    /// </summary>
    NOTIFY_INLINE void RenderNotifications()
    {
        const auto vp_size = GetMainViewport()->Size;

        float height = 0.f;

        for (auto i = 0; i < notifications.size(); i++)
        {
            auto* current_toast = &notifications[i];

            // Remove toast if expired
            if (current_toast->get_phase() == ImGuiToastPhase_Expired)
            {
                RemoveNotification(i);
                continue;
            }

            // Get icon, title and other data
            const auto icon          = current_toast->get_icon();
            const auto title         = current_toast->get_title();
            const auto content       = current_toast->get_content();
            const auto default_title = current_toast->get_default_title();
            const auto opacity       = current_toast->get_fade_percent();

            // Window rendering
            auto text_color = current_toast->get_color();
            text_color.w = opacity;

            // Generate new unique name for this toast
            char window_name[50];
            sprintf_s(window_name, "##TOAST%d", i);

            SetNextWindowBgAlpha(opacity);
            SetNextWindowPos(ImVec2(vp_size.x - NOTIFY_PADDING_X,
                                    vp_size.y - NOTIFY_PADDING_Y - height),
                             ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            Begin(window_name, NULL, NOTIFY_TOAST_FLAGS);

            // Force the toast window above every other window. The
            // NoBringToFrontOnFocus flag prevents ImGui from doing this
            // automatically, so we call the internal API explicitly.
            // Focus stays where it was.
            BringWindowToDisplayFront(GetCurrentWindow());

            // Here we render the toast content
            {
                PushTextWrapPos(vp_size.x / 3.f);

                bool was_title_rendered = false;

                // If an icon is set
                if (!NOTIFY_NULL_OR_EMPTY(icon))
                {
                    TextColored(text_color, icon);
                    was_title_rendered = true;
                }

                // If a title is set
                if (!NOTIFY_NULL_OR_EMPTY(title))
                {
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        SameLine();

                    Text(title);
                    was_title_rendered = true;
                }
                else if (!NOTIFY_NULL_OR_EMPTY(default_title))
                {
                    if (!NOTIFY_NULL_OR_EMPTY(icon))
                        SameLine();

                    Text(default_title);
                    was_title_rendered = true;
                }

                if (was_title_rendered && !NOTIFY_NULL_OR_EMPTY(content))
                {
                    SetCursorPosY(GetCursorPosY() + 5.f);
                }

                // If a content is set
                if (!NOTIFY_NULL_OR_EMPTY(content))
                {
                    if (was_title_rendered)
                    {
#ifdef NOTIFY_USE_SEPARATOR
                        Separator();
#endif
                    }

                    Text(content);
                }

                PopTextWrapPos();
            }

            // Save height for next toasts
            height += GetWindowHeight() + NOTIFY_PADDING_MESSAGE_Y;

            End();
        }
    }

    /// <summary>
    /// Adds font-awesome font, must be called ONCE on initialization
    /// <param name="FontDataOwnedByAtlas">Fonts are loaded from read-only memory, should be set to false!</param>
    /// </summary>
    NOTIFY_INLINE void MergeIconsWithLatestFont(float font_size, bool FontDataOwnedByAtlas = false)
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        ImFontConfig icons_config;
        icons_config.MergeMode            = true;
        icons_config.PixelSnapH           = true;
        icons_config.FontDataOwnedByAtlas = FontDataOwnedByAtlas;

        GetIO().Fonts->AddFontFromMemoryTTF(
            (void*)fa_solid_900, sizeof(fa_solid_900),
            font_size, &icons_config, icons_ranges);
    }
}

#endif // IMGUI_NOTIFY