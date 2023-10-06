// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_TEXT_INPUT_DEFINE_H
#define RAIL_SDK_RAIL_TEXT_INPUT_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailTextInputWindowOption {
    RailTextInputWindowOption() {
        show_password_input = false;
        enable_multi_line_edit = false;
        auto_cancel = false;
        is_min_window = false;
        position_left = 0;
        position_top = 0;
    }
    // for normal text input window
    bool show_password_input;     // true: show as password
    bool enable_multi_line_edit;  // false: single line, now always false
    bool auto_cancel;             // true: if user press escape or X button, auto hide the text pad
    RailString caption_text;
    RailString description;
    RailString content_placeholder;

    // for min text input window
    bool is_min_window;
    uint32_t position_left;  // x position relative to foreground window left
    uint32_t position_top;   // y position relative to foreground window top
};

namespace rail_event {

struct RailTextInputResult : public RailEvent<kRailEventTextInputShowTextInputWindowResult> {
    RailTextInputResult() {
        result = kFailure;
        user_data = "";
    }

    RailString content;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_TEXT_INPUT_DEFINE_H
