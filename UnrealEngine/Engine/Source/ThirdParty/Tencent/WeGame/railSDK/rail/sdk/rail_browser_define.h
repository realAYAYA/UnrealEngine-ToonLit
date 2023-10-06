// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_BROWSER_DEFINE_H
#define RAIL_SDK_RAIL_BROWSER_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailMouseActionType {
    kRailMouseButtonActionUnknown = 0,

    kRailMouseButtonActionLeftButtonDown = 1,
    kRailMouseButtonActionLeftButtonUp = 2,
    kRailMouseButtonActionLeftButtonDoubleClick = 3,

    kRailMouseButtonActionRightButtonDown = 4,
    kRailMouseButtonActionRightButtonUp = 5,
    kRailMouseButtonActionRightButtonDoubleClick = 6,

    kRailMouseButtonActionMiddleButtonDown = 7,
    kRailMouseButtonActionMiddleButtonUp = 8,
    kRailMouseButtonActionMiddleButtonDoubleClick = 9,

    kRailMouseButtonActionMove = 10,
};

struct CreateBrowserOptions {
    CreateBrowserOptions() {
        has_maximum_button = true;
        has_minimum_button = true;
        has_border = true;
        is_movable = true;
        margin_top = 0;
        margin_left = 0;
    }

    bool has_maximum_button;
    bool has_minimum_button;
    bool has_border;
    bool is_movable;
    int32_t margin_top;
    int32_t margin_left;
};

struct CreateCustomerDrawBrowserOptions {
    CreateCustomerDrawBrowserOptions() {
        content_offset_x = 0;
        content_offset_y = 0;
        content_window_width = 600;
        content_window_height = 400;
        has_scroll = false;
    }

    int32_t content_offset_x;       // html content offset x in screen, in pixel
    int32_t content_offset_y;       // html content offset y in screen, in pixel
    uint32_t content_window_width;  // width of window which can display the content, in pixel
    uint32_t content_window_height;  // height of window which can display the content, in pixel
    bool has_scroll;
};

namespace rail_event {
// triggered create a IRailBrowser web browser result
struct CreateBrowserResult : public RailEvent<kRailEventBrowserCreateResult> {
    CreateBrowserResult() {
        result = kFailure;
        user_data = "";
    }
};

// triggered reload IRailBrowser web result
struct ReloadBrowserResult : public RailEvent<kRailEventBrowserReloadResult> {
    ReloadBrowserResult() {
        result = kFailure;
        user_data = "";
    }
};

// triggered close a IRailBrowser web browser result
struct CloseBrowserResult : public RailEvent<kRailEventBrowserCloseResult> {
    CloseBrowserResult() {
        result = kFailure;
        user_data = "";
    }
};

// triggered javascript event notify for both IRailBrowser and IRailBrowserRender browser types
struct JavascriptEventResult : public RailEvent<kRailEventBrowserJavascriptEvent> {
    JavascriptEventResult() {
        result = kFailure;
        user_data = "";
    }

    RailString event_name;
    RailString event_value;
};

// for osrPaint: file mapping of bitmap rendered by IRailBrowserRender browser
struct BrowserNeedsPaintRequest : public RailEvent<kRailEventBrowserPaint> {
    BrowserNeedsPaintRequest() {
        bgra_data = NULL;
        offset_x = 0;
        offset_y = 0;
        bgra_width = 0;
        bgra_height = 0;

        scroll_x_pos = 0;
        scroll_y_pos = 0;
        page_scale_factor = 1.0f;
        user_data = "";
    }

    const char* bgra_data;  // B8G8R8A8 data for browser surface
    int32_t offset_x;       // content offset in screen, pixels
    int32_t offset_y;       // content offset in screen, pixels
    uint32_t bgra_width;    // total width
    uint32_t bgra_height;   // total height

    uint32_t scroll_x_pos;
    uint32_t scroll_y_pos;
    float page_scale_factor;
};

// for osrPaint: part of window needs to paint
// file mapping of bitmap rendered by IRailBrowserRender browser
struct BrowserDamageRectNeedsPaintRequest : public RailEvent<kRailEventBrowserDamageRectPaint> {
    BrowserDamageRectNeedsPaintRequest() {
        bgra_data = NULL;
        offset_x = 0;
        offset_y = 0;
        bgra_width = 0;
        bgra_height = 0;

        update_offset_x = 0;
        update_offset_y = 0;
        update_bgra_width = 0;
        update_bgra_height = 0;

        scroll_x_pos = 0;
        scroll_y_pos = 0;
        page_scale_factor = 1.0f;

        user_data = "";
    }

    const char* bgra_data;  // B8G8R8A8 data for browser surface

    int32_t offset_x;      // content offset in screen, pixels
    int32_t offset_y;      // content offset in screen, pixels
    uint32_t bgra_width;   // total width
    uint32_t bgra_height;  // total height

    int32_t update_offset_x;  // content offset in screen, pixels
    int32_t update_offset_y;  // content offset in screen, pixels
    uint32_t update_bgra_width;
    uint32_t update_bgra_height;

    uint32_t scroll_x_pos;
    uint32_t scroll_y_pos;
    float page_scale_factor;
};

// posted when IRailBrowserRender web navigate completed
struct BrowserRenderNavigateResult : public RailEvent<kRailEventBrowserNavigeteResult> {
    BrowserRenderNavigateResult() {
        result = kFailure;
        user_data = "";
    }

    RailString url;
};

// posted when IRailBrowserRender web render state changed
struct BrowserRenderStateChanged : public RailEvent<kRailEventBrowserStateChanged> {
    BrowserRenderStateChanged() {
        result = kSuccess;
        user_data = "";
        can_go_back = true;
        can_go_forward = true;
    }

    bool can_go_back;
    bool can_go_forward;
};

// posted when IRailBrowserRender web title changed
struct BrowserRenderTitleChanged : public RailEvent<kRailEventBrowserTitleChanged> {
    BrowserRenderTitleChanged() {
        result = kSuccess;
        user_data = "";
        new_title = "";
    }

    RailString new_title;
};

// posted when a browser tries to navigate a new web page
struct BrowserTryNavigateNewPageRequest
    : public RailEvent<kRailEventBrowserTryNavigateNewPageRequest> {
    BrowserTryNavigateNewPageRequest() {
        is_redirect_request = false;
        user_data = "";
    }

    RailString url;            // new url
    RailString target_type;    // _blank or _self
    bool is_redirect_request;  // is a redirect html request
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_BROWSER_DEFINE_H
