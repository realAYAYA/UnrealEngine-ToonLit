// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_COMPONENT_H
#define RAIL_SDK_RAIL_COMPONENT_H

#include "rail/sdk/base/rail_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailComponent {
  public:
    virtual uint64_t GetComponentVersion() = 0;
    virtual void Release() = 0;

  protected:
    virtual ~IRailComponent() {}
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_COMPONENT_H
