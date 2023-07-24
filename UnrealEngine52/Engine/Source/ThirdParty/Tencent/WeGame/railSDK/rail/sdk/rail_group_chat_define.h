// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GROUP_CHAT_DEFINE_H
#define RAIL_SDK_RAIL_GROUP_CHAT_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailGroupInfo {
    RailGroupInfo() {
    }

    RailString group_id;
    RailString group_name;
    RailString group_icon_url;
};

namespace rail_event {

struct RailQueryGroupsInfoResult : public RailEvent<kRailEventGroupChatQueryGroupsInfoResult> {
    RailQueryGroupsInfoResult() {
        result = kFailure;
    }

    RailArray<RailString> group_ids;
};

struct RailOpenGroupChatResult : public RailEvent<kRailEventGroupChatOpenGroupChatResult> {
    RailOpenGroupChatResult() {
        result = kFailure;
    }
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GROUP_CHAT_DEFINE_H
