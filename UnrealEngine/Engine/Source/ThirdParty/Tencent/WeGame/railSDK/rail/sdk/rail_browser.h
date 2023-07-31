// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_BROWSER_H
#define RAIL_SDK_RAIL_BROWSER_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_browser_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailBrowser;
class IRailBrowserRender;
class IRailBrowserHelper {
  public:
    // window_width, window_height in pixel
    // event kRailEventBrowserCreateResult will callback when it triggered
    virtual IRailBrowser* AsyncCreateBrowser(const RailString& url,
                            uint32_t window_width,
                            uint32_t window_height,
                            const RailString& user_data,
                            const CreateBrowserOptions& options = CreateBrowserOptions(),
                            RailResult* result = NULL) = 0;

    // create customer draw browser
    virtual IRailBrowserRender* CreateCustomerDrawBrowser(const RailString& url,
                                    const RailString& user_data,
                                    const CreateCustomerDrawBrowserOptions& options =
                                        CreateCustomerDrawBrowserOptions(),
                                    RailResult* result = NULL) = 0;

    // navigate a specified url with web browser directly
    virtual RailResult NavigateWebPage(const RailString& url, bool display_in_new_tab) = 0;
};

class IRailBrowser : public IRailComponent {
  public:
    virtual bool GetCurrentUrl(RailString* url) = 0;
    // event kRailEventBrowserReloadResult will callback when it triggered
    virtual bool ReloadWithUrl(const RailString& new_url = "") = 0;
    virtual void StopLoad() = 0;

    // event kRailEventBrowserJavascriptEvent will callback when it triggered
    virtual bool AddJavascriptEventListener(const RailString& event_name) = 0;
    virtual bool RemoveAllJavascriptEventListener() = 0;

    // event kRailEventBrowserTryNavigateNewPageRequest will callback,
    // before browser navigate new url
    virtual void AllowNavigateNewPage(bool allow) = 0;

    // event kRailEventBrowserCloseResult will callback when it triggered
    virtual void Close() = 0;
};

class IRailBrowserRender : public IRailComponent {
  public:
    virtual bool GetCurrentUrl(RailString* url) = 0;
    virtual bool ReloadWithUrl(const RailString& new_url = "") = 0;
    virtual void StopLoad() = 0;

    // event kRailEventBrowserJavascriptEvent will callback when it triggered
    virtual bool AddJavascriptEventListener(const RailString& event_name) = 0;
    virtual bool RemoveAllJavascriptEventListener() = 0;

    // event kRailEventBrowserTryNavigateNewPageRequest will callback,
    // before browser navigate a new url
    virtual void AllowNavigateNewPage(bool allow) = 0;

    // close the browser render
    virtual void Close() = 0;

    // following apis only for IRailBrowserRender
    virtual void UpdateCustomDrawWindowPos(int32_t content_offset_x,
                    int32_t content_offset_y,
                    uint32_t content_window_width,
                    uint32_t content_window_height) = 0;

    // browser active - mean the window has got focus
    virtual void SetBrowserActive(bool active) = 0;

    // navigate back
    virtual void GoBack() = 0;
    // navigate forward
    virtual void GoForward() = 0;

    // execute this javascript in the web page
    virtual bool ExecuteJavascript(const RailString& event_name, const RailString& event_value) = 0;

    // only for windows platform
    virtual void DispatchWindowsMessage(uint32_t window_msg, WPARAM w_param, LPARAM l_param) = 0;

    virtual void DispatchMouseMessage(EnumRailMouseActionType button_action,
                    uint32_t user_define_mouse_key,
                    uint32_t x_pos,
                    uint32_t y_pos) = 0;

    virtual void MouseWheel(int32_t delta,
                    uint32_t user_define_mouse_key,
                    uint32_t x_pos,
                    uint32_t y_pos) = 0;

    // when the window control get focus or not
    virtual void SetFocus(bool has_focus) = 0;

    virtual void KeyDown(uint32_t key_code) = 0;
    virtual void KeyUp(uint32_t key_code) = 0;
    virtual void KeyChar(uint32_t key_code, bool is_uinchar) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_BROWSER_H
