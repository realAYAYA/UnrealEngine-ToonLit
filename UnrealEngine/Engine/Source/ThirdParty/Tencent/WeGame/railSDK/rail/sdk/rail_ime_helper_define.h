// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_IME_HELPER_DEFINE_H
#define RAIL_SDK_RAIL_IME_HELPER_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailWindowPosition {
    RailWindowPosition() {
        position_left = 0;
        position_top = 0;
    }
    uint32_t position_left;  // x position relative to foreground window left
    uint32_t position_top;   // y position relative to foreground window top
};

struct RailTextInputImeWindowOption {
    RailTextInputImeWindowOption() {
        show_rail_ime_window = true;
    }
    RailWindowPosition position;
    bool show_rail_ime_window;
};

enum RailIMETextInputCompositionState {
    kTextInputCompositionStateNone = 0,
    kTextInputCompositionStateStart = 1,
    kTextInputCompositionStateUpdate = 2,
    kTextInputCompositionStateEnd = 3,
};

namespace rail_event {
struct RailIMEHelperTextInputCompositionState
    : public RailEvent<kRailEventIMEHelperTextInputCompositionStateChanged> {
    RailIMEHelperTextInputCompositionState() {
        result = kSuccess;
        user_data = "";
        composition_text = "";
        composition_state = kTextInputCompositionStateNone;
    }
    // composition_text only has value when composition_state is kTextInputCompositionStateUpdate
    // composition_text has a candidate list and the letters that player typed
    // composition_text is a json data, the format is like below
    // {
    // "candidate_list": [
    //    {
    //        "candidate_text": "让他 "
    //    },
    //    {
    //        "candidate_text": "让她 "
    //    },
    //    {
    //        "candidate_text": "人体 "
    //    },
    //    {
    //        "candidate_text": "如图 "
    //    }
    //    ],
    // "composition_text": "rt"
    // }
    RailString composition_text;
    RailIMETextInputCompositionState composition_state;
};

struct RailIMEHelperTextInputSelectedResult
    : public RailEvent<kRailEventIMEHelperTextInputSelectedResult> {
    RailIMEHelperTextInputSelectedResult() {
        result = kFailure;
        user_data = "";
    }

    RailString content;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_IME_HELPER_DEFINE_H
