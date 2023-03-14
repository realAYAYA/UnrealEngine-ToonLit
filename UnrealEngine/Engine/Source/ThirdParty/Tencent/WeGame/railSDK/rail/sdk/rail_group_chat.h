// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_GROUP_CHAT_H
#define RAIL_SDK_RAIL_GROUP_CHAT_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_group_chat_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailGroupChat;
class IRailGroupChatHelper {
  public:
    // query current player's group information. You can get a list of group ids from
    // the RailQueryGroupsInfoResult callback.
    virtual RailResult AsyncQueryGroupsInfo(const RailString& user_data) = 0;

    // get a group chat object. You can't use the IRailGroupChat pointer until you receive
    // the RailOpenGroupChatResult callback and the result parameter is kSuccess.
    virtual IRailGroupChat* AsyncOpenGroupChat(const RailString& group_id,
                                const RailString& user_data) = 0;
};

class IRailGroupChat : public IRailComponent {
  public:
    virtual RailResult GetGroupInfo(RailGroupInfo* group_info) = 0;

    virtual RailResult OpenGroupWindow() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_GROUP_CHAT_H
