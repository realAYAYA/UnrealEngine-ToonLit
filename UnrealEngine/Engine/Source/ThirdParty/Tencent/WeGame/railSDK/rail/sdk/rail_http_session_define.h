// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_HTTP_SESSION_DEFINE_H
#define RAIL_SDK_RAIL_HTTP_SESSION_DEFINE_H

#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum RailHttpSessionMethod {
    kRailHttpSessionMethodGet = 0,
    kRailHttpSessionMethodPost = 1,
};

namespace rail_event {
struct RailHttpSessionResponse : public RailEvent<kRailEventHttpSessionResponseResult> {
    RailHttpSessionResponse() {}
    RailString http_response_data;
};
}  // namespace rail_event
#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_HTTP_SESSION_DEFINE_H
