// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_LEADERBOARD_H
#define RAIL_SDK_RAIL_LEADERBOARD_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_user_space_define.h"

// @desc classes and structures here provide simple and reliable services for game leaderboards.
// To update and retrieve leaderboards, usually the leaderboards need to be configured first
// on the Developer Portal. Leaderboards can also be created programmatically with
// AsyncCreateLeaderboard


namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum LeaderboardType {
    kLeaderboardUnknown = 0,   // invalid value
    kLeaderboardAllZone = 1,   // for global leaderboard
    kLeaderboardMyZone = 2,    // for global leaderboard, same as above
    kLeaderboardMyServer = 3,  // for global leaderboard, same as above
    kLeaderboardFriends = 4,   // for leaderboard of friends only
};

enum LeaderboardUploadType {
    kLeaderboardUploadInvalid = 0,     // invalid value
    kLeaderboardUploadRewrite = 1,     // rewrite unconditionally if an entry already exists
    kLeaderboardUploadChooseBest = 2,  // update only when the new score is better
};

enum LeaderboardSortType {
    kLeaderboardSortTypeNone = 0,  // invalid
    kLeaderboardSortTypeAsc = 1,   // ascending
    kLeaderboardSortTypeDesc = 2,  // descending
};

// used to properly display the leaderboard entries on game platform
enum LeaderboardDisplayType {
    kLeaderboardDisplayTypeNone = 0,          // invalid value
    kLeaderboardDisplayTypeDouble = 1,        // interpret number as a double value
    kLeaderboardDisplayTypeSeconds = 2,       // interpret number as seconds
    kLeaderboardDisplayTypeMilliSeconds = 3,  // interpret number as milliseconds
};

struct LeaderboardParameters {
    LeaderboardParameters() {}

    RailString param;  // of JSON format. Configured on Developer Portal
};

struct RequestLeaderboardEntryParam {
    RequestLeaderboardEntryParam() {
        type = kLeaderboardUnknown;
        range_start = 0;
        range_end = 0;
        user_coordinate = false;   // if true, current player's position will be added to the range
    }

    LeaderboardType type;  // either for friend leaderboards or global leaderboards
    // If range_end >= range_start, the range will be [param.range_start, param.range_end]
    // If range_end == -1, the range will be [param.range_start, index of the last entry]
    // Please note for global leaderboard, if user_coordinate is true, current player's position
    // will be added to the range
    int32_t range_start;   // could be less than 0 for global leaderboard if user_coordinate == true
    int32_t range_end;     // use -1 to retrieve till the last leaderboard entry
    bool user_coordinate;  // use true for relative coordinates range for global leaderboards
};

struct LeaderboardData {
    LeaderboardData() {
        score = 0.0;
        rank = 0;
    }

    double score;
    int32_t rank;
    SpaceWorkID spacework_id;
    RailString additional_infomation;
};

struct LeaderboardEntry {
    LeaderboardEntry() { player_id = 0; }

    RailID player_id;
    LeaderboardData data;
};

struct UploadLeaderboardParam {
    UploadLeaderboardParam() { type = kLeaderboardUploadInvalid; }

    LeaderboardUploadType type;
    LeaderboardData data;
};

class IRailLeaderboardEntries;
class IRailLeaderboard;

class IRailLeaderboardHelper {
  public:
    virtual ~IRailLeaderboardHelper() {}

    // @desc Open a leaderboard that was already configured on Developer Portal
    // @param leaderboard_name API name configured on the Developer Portal. Not the display name.
    // On the Developer Portal, you will find 'API name' in the leaderboard section.
    // @return The pointer to the leaderboard object
    virtual IRailLeaderboard* OpenLeaderboard(const RailString& leaderboard_name) = 0;

    // @desc If the leaderboard of the name 'leaderboard_name' was neither configured on the
    // Developer Portal nor created with this interface earlier, the call will create one.
    // Otherwise, the existing leaderboard will be opened just like using OpenLeaderboard.
    // The callback event is LeaderboardCreated
    // @param leaderboard_name Name of the leaderboard
    // @param sort_type How to sort the leaderboard. Could be ascending or decending.
    // @param display_type How the data in the leaderboard is displayed on the game platform
    // @param user_data Will be copied to the asynchronous result.
    // @return Pointer to the leaderboard created or the existing one of the same name
    virtual IRailLeaderboard* AsyncCreateLeaderboard(const RailString& leaderboard_name,
                                LeaderboardSortType sort_type,
                                LeaderboardDisplayType display_type,
                                const RailString& user_data,
                                RailResult* result) = 0;
};

class IRailLeaderboard : public IRailComponent {
  public:
    // @desc Get the name of the current leaderboard
    // @return Name of the current leaderboard
    virtual RailString GetLeaderboardName() = 0;

    // @desc Get the total number of entries in the leaderboard
    virtual int32_t GetTotalEntriesCount() = 0;

    // @desc Get meta data for leaderboard to check whether the leaderboard exists
    // The callback is LeaderboardReceived
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetLeaderboard(const RailString& user_data) = 0;

    // @desc Get customized parameter for the leaderboard
    // @param param Retrieved parameter
    // @return Returns kSuccess on success
    virtual RailResult GetLeaderboardParameters(LeaderboardParameters* param) = 0;

    // @desc Create an object to download leaderboard entries later
    // @return Pointer to the object to download info for leaderboard entries
    virtual IRailLeaderboardEntries* CreateLeaderboardEntries() = 0;

    // @desc Update the leaderboard
    // The callback event is LeaderboardUploaded
    // For leaderboard in a mod(player modification), use AsyncAttachSpaceWork to attach an MOD ID
    // @param update_param See definition of UploadLeaderboardParam for details
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncUploadLeaderboard(const UploadLeaderboardParam& update_param,
                        const RailString& user_data) = 0;

    // @desc Get the LeaderboardSortType
    // @param sort_type Usually ascending or decending
    // @return Returns kSuccess on success
    virtual RailResult GetLeaderboardSortType(LeaderboardSortType* sort_type) = 0;

    // @desc Get the LeaderboardDisplayType
    // @param display_type Type to show on the game platform
    // @return Returns kSuccess on success
    virtual RailResult GetLeaderboardDisplayType(LeaderboardDisplayType* display_type) = 0;

    // @desc Attach the leaderboard to a mod (player modification)
    // The callback is LeaderboardAttachSpaceWork
    // Only one spacework_id can be attached to the leaderboard
    // The new spacework_id will replace the old one if there is an existing one
    // @param spacework_id ID of the player mod
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncAttachSpaceWork(SpaceWorkID spacework_id,
                        const RailString& user_data) = 0;
};

class IRailLeaderboardEntries : public IRailComponent {
  public:
    // @desc Get the Rail ID used in AsyncRequestLeaderboardEntries last time
    // @return RailID used in AsyncRequestLeaderboardEntries last time
    virtual RailID GetRailID() = 0;

    // @desc Get the name of the current leaderboard
    // @return Name of the leaderboard
    virtual RailString GetLeaderboardName() = 0;

    // @desc Download leaderboard entries
    // 'range_start' can be less than zero.
    // For global leaderboard, if player != RailID(0) and param.user_coordinate == true,
    // the actual range is [player_pos + range_start, player_pos + range_end].
    // If user_coordinate == false, the actual range is
    // [max(1, player_pos) + range_start, max(0, player_pos) + range_end].
    // For example: if player_pos == 6, range_start == -2 and range_end == 2, the
    // actual range is [4, 8]. If player == RailID(0), range_start == -2 and range_end == 2, the
    // actual range is [1, 2]
    // The callback event is LeaderboardEntryReceived
    // @param player If player.IsValid() == false, a global leaderboard will be retrieved.
    // Otherwise, a leaderboard for friends only will be retrieved
    // @param param See definition of RequestLeaderboardEntryParam for details. Notice: If
    // type ==  kLeaderboardFriends, range_start and range_end will be ignored, and the
    // whole data of leaderboard for friends will be retrieved.
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncRequestLeaderboardEntries(const RailID& player,
                        const RequestLeaderboardEntryParam& param,
                        const RailString& user_data) = 0;

    // @desc Get the param that was used in AsyncRequestLeaderboardEntries
    // @return The param used in AsyncRequestLeaderboardEntries
    virtual RequestLeaderboardEntryParam GetEntriesParam() = 0;

    // @desc Get the number of the entries retrieved
    // @return Number of the entries retrieved
    virtual int32_t GetEntriesCount() = 0;

    // @desc Get an entry of 'index' in the retrieved entries
    // @param index Should be in the range [0, entries_count)
    // @param leaderboard_entry The retrieved entry
    // @return Returns kSuccess on success
    virtual RailResult GetLeaderboardEntry(int32_t index, LeaderboardEntry* leaderboard_entry) = 0;
};

namespace rail_event {

struct LeaderboardReceived : public RailEvent<kRailEventLeaderboardReceived> {
    LeaderboardReceived() { does_exist = false; }

    RailString leaderboard_name;
    bool does_exist;
};

struct LeaderboardCreated : public RailEvent<kRailEventLeaderboardAsyncCreated> {
    LeaderboardCreated() {}

    RailString leaderboard_name;
};

struct LeaderboardEntryReceived : public RailEvent<kRailEventLeaderboardEntryReceived> {
    LeaderboardEntryReceived() {}

    RailString leaderboard_name;
};

struct LeaderboardUploaded : public RailEvent<kRailEventLeaderboardUploaded> {
    LeaderboardUploaded() {
        score = 0.0;
        better_score = false;
        new_rank = 0;
        old_rank = 0;
    }

    RailString leaderboard_name;
    double score;
    bool better_score;
    int32_t new_rank;
    int32_t old_rank;
};

struct LeaderboardAttachSpaceWork : public RailEvent<kRailEventLeaderboardAttachSpaceWork> {
    LeaderboardAttachSpaceWork() {}

    RailString leaderboard_name;
    SpaceWorkID spacework_id;
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_LEADERBOARD_H
