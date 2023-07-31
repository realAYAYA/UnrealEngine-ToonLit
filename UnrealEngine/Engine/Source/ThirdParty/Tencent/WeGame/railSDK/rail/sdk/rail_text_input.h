// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_TEXT_INPUT_H
#define RAIL_SDK_RAIL_TEXT_INPUT_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_text_input_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailTextInputHelper {
  public:
    virtual RailResult ShowTextInputWindow(const RailTextInputWindowOption& options) = 0;

    virtual void GetTextInputContent(RailString* content) = 0;

    virtual RailResult HideTextInputWindow() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_TEXT_INPUT_H
