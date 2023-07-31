// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ROOM_DEFINE_H
#define RAIL_SDK_RAIL_ROOM_DEFINE_H

#include "rail/sdk/base/rail_array.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

// rename from RAIL_DEFAULT_MAX_ROOM_MEMBERS, 2018/11/27.
const uint32_t kRailRoomDefaultMaxMemberNumber = 2;

const uint32_t kRailRoomDataKeyValuePairsLimit = 50;

// the type of room.
enum EnumRoomType {
    kRailRoomTypePrivate = 0,
    kRailRoomTypeWithFriends = 1,
    kRailRoomTypePublic = 2,
    kRailRoomTypeHidden = 3,
};

// the reason for leaving room.
enum EnumLeaveRoomReason {
    kLeaveRoomReasonActive = 1,
    kLeaveRoomReasonTimeout = 2,
    kLeaveRoomReasonKick = 3,
};

// member status changed.
enum EnumRoomMemberActionStatus {
    kMemberEnteredRoom = 1,
    kMemberLeftRoom = 2,
    kMemberDisconnectServer = 4,
};

// the reason for owner changed.
enum EnumRoomOwnerChangeReason {
    kRoomOwnerActiveChange = 1,
    kRoomOwnerLeave = 2,
};

struct RoomOptions {
    RoomOptions() {
        type = kRailRoomTypePublic;
        max_members = kRailRoomDefaultMaxMemberNumber;
        enable_team_voice = true;
    }

    EnumRoomType type;
    uint32_t max_members;
    RailString password;
    RailString room_tag;
    bool enable_team_voice;
};

struct RoomInfoListSorter {
    RoomInfoListSorter() {
        property_value_type = kRailPropertyValueTypeString;
        property_sort_type = kRailSortTypeAsc;
        close_to_value = 0;
    }

    EnumRailPropertyValueType property_value_type;
    EnumRailSortType property_sort_type;
    RailString property_key;
    double close_to_value;  // this value is valid when property_sort_type is kRailSortTypeCloseTo.
};

struct RoomInfoListFilterKey {
    RoomInfoListFilterKey() {
        value_type = kRailPropertyValueTypeString;
        comparison_type = kRailComparisonTypeEqualToOrLessThan;
    }

    RailString key_name;                     // filter key name.
    EnumRailPropertyValueType value_type;    // value of 'key'(indicated by key_name).
    EnumRailComparisonType comparison_type;  // comparison type between value( value of 'key')
                                             // and filter_value.
    RailString filter_value;                 // defined filter value.
};

struct RoomInfoListFilter {
    RoomInfoListFilter() {
        filter_password = kRailOptionalAny;
        filter_friends_owned = kRailOptionalAny;
        filter_friends_in_room = kRailOptionalAny;
        available_slot_at_least = 0;
    }

    // all filters below are logic AND relationship
    // example:
    // filters AND
    //
    // filter_room_name(if room_name_contained is not empty) AND
    //
    // has_password_only(if filter_password = kRailOptionalYes) AND
    // not_has_password(if filter_password = kRailOptionalNo) AND
    // not_care_whether_password(if filter_password = kRailOptionalAny) AND
    //
    // friends_owned_only(if filter_friends_owned = kRailOptionalYes) AND
    // not_friends_owned(if filter_friends_owned = kRailOptionalNo) AND
    // not_care_whether_friends_owned(if filter_friends_owned = kRailOptionalAny) AND
    //
    // available_slot_at_least

    // defined filter condition
    RailArray<RoomInfoListFilterKey> key_filters;  // filter by all conditions in key_filters array,
                                                   // like key_filters[0] AND key_filters[1] AND ...
                                                   // AND key_filters[N].
    RailString room_name_contained;  // filter rooms containing the specified room name.
                                     // If the room_name_contained value is part of the room name,
                                     // this room will be returned from the server.
    RailString room_tag;             // filter rooms with specified room tag. The room can be
                                     // returned from the server ONLY when the room tag of this
                                     // room is the same with the room_tag value.
    EnumRailOptionalValue filter_password;         // filter rooms whether have password or not.
    EnumRailOptionalValue filter_friends_owned;    // filter rooms whether created by friends or
                                                   // not.
    EnumRailOptionalValue filter_friends_in_room;  // filter rooms whether the player's friends
                                                   // in or not.
    uint32_t available_slot_at_least;              // filter rooms where at least
                                                   // available_slot_at_least number slot left.
};

struct RoomInfo {
    RoomInfo() {
        room_id = 0;
        owner_id = 0;
        max_members = 0;
        current_members = 0;
        create_time = 0;
        has_password = false;
        is_joinable = true;
        type = kRailRoomTypePrivate;
        game_server_rail_id = 0;
    }

    uint64_t room_id;
    RailID owner_id;
    uint32_t max_members;
    uint32_t current_members;
    uint32_t create_time;
    RailString room_name;
    RailString room_tag;
    bool has_password;
    bool is_joinable;
    EnumRoomType type;
    RailID game_server_rail_id;
    RailArray<RailKeyValue> room_kvs;
};

struct RoomMemberInfo {
    RoomMemberInfo() {
        room_id = 0;
        member_id = 0;
        member_index = 0;
    }

    uint64_t room_id;
    RailID member_id;
    uint32_t member_index;
    RailString member_name;
    RailArray<RailKeyValue> member_kvs;
};

namespace rail_event {

// rename from CreateRoomInfo, 2018/11/27.
struct CreateRoomResult : public RailEvent<kRailEventRoomCreated> {
    CreateRoomResult() {
        room_id = 0;
    }

    uint64_t room_id;
};

struct OpenRoomResult : public RailEvent<kRailEventRoomOpenRoomResult> {
    OpenRoomResult() {
        result = kFailure;
        room_id = 0;
    }
    uint64_t room_id;
};

// rename from RoomInfoList, 2018/11/27.
struct GetRoomListResult : public RailEvent<kRailEventRoomGetRoomListResult> {
    GetRoomListResult() {
        begin_index = 0;
        end_index = 0;
        total_room_num = 0;
    }

    uint32_t begin_index;
    uint32_t end_index;
    uint32_t total_room_num;  // rename from total_room_num_in_zone, 2018/11/27.
    RailArray<RoomInfo> room_infos;
};

// rename from UserRoomListInfo, 2018/11/27.
struct GetUserRoomListResult : public RailEvent<kRailEventRoomGetUserRoomListResult> {
    GetUserRoomListResult() {
        result = kFailure;
    }

    RailArray<RoomInfo> room_info;
};

struct SetNewRoomOwnerResult : public RailEvent<kRailEventRoomSetNewRoomOwnerResult> {
    SetNewRoomOwnerResult() {
        result = kFailure;
    }
};

// rename from RoomMembersInfo, 2018/11/27.
struct GetRoomMembersResult : public RailEvent<kRailEventRoomGetRoomMembersResult> {
    GetRoomMembersResult() {
        room_id = 0;
        member_num = 0;
    }

    uint64_t room_id;
    uint32_t member_num;
    RailArray<RoomMemberInfo> member_infos;
};

// rename from LeaveRoomInfo, 2018/11/27.
struct LeaveRoomResult : public RailEvent<kRailEventRoomLeaveRoomResult> {
    LeaveRoomResult() {
        room_id = 0;
        reason = kLeaveRoomReasonActive;
    }

    uint64_t room_id;
    EnumLeaveRoomReason reason;
};

// rename from JoinRoomInfo, 2018/11/27.
struct JoinRoomResult : public RailEvent<kRailEventRoomJoinRoomResult> {
    JoinRoomResult() {
        room_id = 0;
    }

    uint64_t room_id;
};

// rename from RoomAllData, 2018/11/27.
struct GetAllRoomDataResult : public RailEvent<kRailEventRoomGetAllDataResult> {
    GetAllRoomDataResult() {}

    RoomInfo room_info;
};

// rename from KickOffMemberInfo, 2018/11/27.
struct KickOffMemberResult : public RailEvent<kRailEventRoomKickOffMemberResult> {
    KickOffMemberResult() {
        room_id = 0;
        kicked_id = 0;
    }

    uint64_t room_id;
    RailID kicked_id;
};

struct SetRoomTagResult : public RailEvent<kRailEventRoomSetRoomTagResult> {
    SetRoomTagResult() {
        result = kFailure;
    }
};

struct GetRoomTagResult : public RailEvent<kRailEventRoomGetRoomTagResult> {
    GetRoomTagResult() {
        result = kFailure;
    }

    RailString room_tag;
};

// rename from SetRoomMetadataInfo, 2018/11/27.
struct SetRoomMetadataResult : public RailEvent<kRailEventRoomSetRoomMetadataResult> {
    SetRoomMetadataResult() {
        room_id = 0;
    }

    uint64_t room_id;
};

// rename from GetRoomMetadataInfo, 2018/11/27.
struct GetRoomMetadataResult : public RailEvent<kRailEventRoomGetRoomMetadataResult> {
    GetRoomMetadataResult() {
        room_id = 0;
    }

    uint64_t room_id;
    RailArray<RailKeyValue> key_value;
};

// rename from ClearRoomMetadataInfo, 2018/11/27.
struct ClearRoomMetadataResult : public RailEvent<kRailEventRoomClearRoomMetadataResult> {
    ClearRoomMetadataResult() {
        room_id = 0;
    }

    uint64_t room_id;
};

// rename from SetMemberMetadataInfo, 2018/11/27.
struct SetMemberMetadataResult : public RailEvent<kRailEventRoomSetMemberMetadataResult> {
    SetMemberMetadataResult() {
        room_id = 0;
        member_id = 0;
    }

    uint64_t room_id;
    RailID member_id;
};

// rename from GetMemberMetadataInfo, 2018/11/27.
struct GetMemberMetadataResult : public RailEvent<kRailEventRoomGetMemberMetadataResult> {
    GetMemberMetadataResult() {
        room_id = 0;
        member_id = 0;
    }

    uint64_t room_id;
    RailID member_id;
    RailArray<RailKeyValue> key_value;
};

struct RoomDataReceived : public RailEvent<kRailEventRoomNotifyRoomDataReceived> {
    RoomDataReceived() {
        remote_peer = 0;
        message_type = 0;
        data_len = 0;
    }

    RailID remote_peer;
    uint32_t message_type;
    uint32_t data_len;
    RailString data_buf;
};

struct SetRoomTypeResult : public RailEvent<kRailEventRoomSetRoomTypeResult> {
    SetRoomTypeResult() {
        room_type = kRailRoomTypePublic;
    }

    EnumRoomType room_type;
};

struct SetRoomMaxMemberResult : public RailEvent<kRailEventRoomSetRoomMaxMemberResult> {
    SetRoomMaxMemberResult() {
        result = kFailure;
    }
};

// info of room property changed.
struct NotifyMetadataChange : public RailEvent<kRailEventRoomNotifyMetadataChanged> {
    NotifyMetadataChange() {
        room_id = 0;
        changer_id = 0;
    }

    uint64_t room_id;
    RailID changer_id;
};

// info of room member changed.
struct NotifyRoomMemberChange : public RailEvent<kRailEventRoomNotifyMemberChanged> {
    NotifyRoomMemberChange() {
        room_id = 0;
        changer_id = 0;
        id_for_making_change = 0;
        state_change = kMemberEnteredRoom;
    }

    uint64_t room_id;
    RailID changer_id;
    RailID id_for_making_change;
    EnumRoomMemberActionStatus state_change;
};

// info of member being kicked.
struct NotifyRoomMemberKicked : public RailEvent<kRailEventRoomNotifyMemberkicked> {
    NotifyRoomMemberKicked() {
        room_id = 0;
        id_for_making_kick = 0;
        kicked_id = 0;
        due_to_kicker_lost_connect = 0;
    }

    uint64_t room_id;
    RailID id_for_making_kick;
    RailID kicked_id;
    uint32_t due_to_kicker_lost_connect;
};

// info of room destroyed.
struct NotifyRoomDestroy : public RailEvent<kRailEventRoomNotifyRoomDestroyed> {
    NotifyRoomDestroy() { room_id = 0; }

    uint64_t room_id;
};


// info of room owner changed.
struct NotifyRoomOwnerChange : public RailEvent<kRailEventRoomNotifyRoomOwnerChanged> {
    NotifyRoomOwnerChange() {
        room_id = 0;
        old_owner_id = 0;
        new_owner_id = 0;
        reason = kRoomOwnerActiveChange;
    }
    uint64_t room_id;
    RailID old_owner_id;
    RailID new_owner_id;
    EnumRoomOwnerChangeReason reason;
};

// info of room game server changed.
struct NotifyRoomGameServerChange : public RailEvent<kRailEventRoomNotifyRoomGameServerChanged> {
    NotifyRoomGameServerChange() {
        room_id = 0;
        game_server_rail_id = 0;
        game_server_channel_id = 0;
    }

    uint64_t room_id;
    RailID game_server_rail_id;
    uint64_t game_server_channel_id;
};

}  // namespace rail_event
#pragma pack(pop)

}  // namespace rail

#endif  // RAIL_SDK_RAIL_ROOM_DEFINE_H
