// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_USER_SPACE_DEFINE_H
#define RAIL_SDK_RAIL_USER_SPACE_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class SpaceWorkID {
  public:
    SpaceWorkID() : id_(0) {}
    explicit SpaceWorkID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ > 0; }

    bool operator==(const SpaceWorkID& r) const { return id_ == r.id_; }
    bool operator!=(const SpaceWorkID& r) const { return !(*this == r); }

  private:
    uint64_t id_;
};

// space work type
enum EnumRailSpaceWorkType {
    kRailSpaceWorkTypeMod = 0,            // mod
    kRailSpaceWorkTypeScreenShot = 1,     // screenshot
    kRailSpaceWorkTypeModGroup = 2,       // mod group
    kRailSpaceWorkTypeRemoteStorage = 3,  // remote storage
    kRailSpaceWorkTypeGameInternal = 5,   // not show in shop and only allow public share level
};

// space work visible, they are incompatible
enum EnumRailSpaceWorkShareLevel {
    kRailSpaceWorkShareLevelPrivate = 0,  // only visible to owner
    kRailSpaceWorkShareLevelFriend = 1,   // visible to friends
    kRailSpaceWorkShareLevelPublic = 2,   // visible to everyone
};

// order by options of query result, they are incompatible
enum EnumRailSpaceWorkOrderBy {
    kRailSpaceWorkOrderByLastUpdateTime = 0,   // ordered by last update time
    kRailSpaceWorkOrderByCreateTime = 1,       // ordered by create time
    kRailSpaceWorkOrderByDownloadCount = 2,    // ordered by download count
    kRailSpaceWorkOrderByScore = 3,            // deprecated, use kRailSpaceWorkOrderByRate
                                               // instead. ordered by score of this space work
    kRailSpaceWorkOrderBySubscribedCount = 4,  // ordered by subscribed count
    kRailSpaceWorkOrderByRate = 10,            // ordered by score
    kRailSpaceWorkOrderByBestMatch = 11,       // only supported in search
};

// deprecated, use EnumRailSpaceWorkRateValue instead.
// the score can be used to vote a space work.
enum EnumRailSpaceWorkVoteValue {
    kRailSpaceWorkVoteZero = 0,
    kRailSpaceWorkVoteOne = 1,
    kRailSpaceWorkVoteTwo = 2,
    kRailSpaceWorkVoteThree = 3,
    kRailSpaceWorkVoteFour = 4,
    kRailSpaceWorkVoteFive = 5,
};

enum EnumRailSpaceWorkRateValue {
    kRailSpaceWorkVoteDown = 1,
    kRailSpaceWorkVoteUp = 5,
};

enum EnumRailWorkFileClass {
    kRailWorkFileClassPopular = 1,
};

enum EnumRailSpaceWorkSyncState {
    kRailSpaceWorkNoSync = 0,
    kRailSpaceWorkDownloading = 1,
    kRailSpaceWorkUploading = 2,
};

enum EnumRailModifyFavoritesSpaceWorkType {
    kRailModifyFavoritesSpaceWorkTypeAdd = 1,
    kRailModifyFavoritesSpaceWorkTypeRemove = 2,
};

enum EnumRailSpaceworkQueryType {
    kRailSpaceworkQueryMySubscribed = 1,
    kRailSpaceworkQueryMyFavorite = 2,
    kRailSpaceworkQueryByUsers = 3,
};

// the filter used by space work query
// subscribed_ids, creator_ids and favorite_ids can only use one of them each query
struct RailSpaceWorkFilter {
    RailArray<RailID> subscriber_list;      // query space works subscribed by one of those users
    RailArray<RailID> creator_list;         // query space works created by one of those users
    RailArray<EnumRailSpaceWorkType> type;  // query space works is one of those types
    RailArray<RailID> collector_list;  // query space works which are one of those users' favorite
    RailArray<EnumRailWorkFileClass> classes;  // query space works from one of those classes
    RailSpaceWorkFilter() {}
};

// the query options
struct RailQueryWorkFileOptions {
    bool with_url;  // whether return url which can be used to show space work detail web page
    bool with_description;  // whether return description of the space work
    // just return total in call back result, any other options will be disabled when it is true
    bool query_total_only;
    bool with_uploader_ids;  // get uploader rail id list, it may not the real author
    bool with_preview_url;   // get primary preview url
    bool with_vote_detail;   // get vote detail
    int preview_scaling_rate;   // set to 1~100 to return a url of preview whose size is scaled.
    RailQueryWorkFileOptions() {
        with_url = false;
        with_description = false;
        query_total_only = false;
        with_uploader_ids = false;
        with_preview_url = false;
        preview_scaling_rate = 100;
        with_vote_detail = false;
    }
};

// progress of space work synchronization
struct RailSpaceWorkSyncProgress {
    uint64_t finished_bytes;  // bytes had been synced
    uint64_t total_bytes;     // total bytes
    float progress;           // percent of progress, rang from 0.0 to 1.0
    EnumRailSpaceWorkSyncState current_state;
    RailSpaceWorkSyncProgress() {
        finished_bytes = 0;
        total_bytes = 0;
        progress = 0.0f;
        current_state = kRailSpaceWorkNoSync;
    }
};

// We support several vote score.
// If you only allow user to voteup and votedown, you can use special score to stand for them.
// For example, use 1 to stand for votedown and 5 for voteup, it will return number of 1 and 5.
// But if game allows user to vote other scores, it will also return number those scores.
// How to calculate the final score is totally denpend on game itself.
struct RailSpaceWorkVoteDetail {
    EnumRailSpaceWorkRateValue vote_value;
    uint32_t voted_players;
    RailSpaceWorkVoteDetail() {
        vote_value = kRailSpaceWorkVoteDown;
        voted_players = 0;
    }
};

struct RailSpaceWorkDescriptor {
    SpaceWorkID id;
    RailString name;
    RailString description;
    RailString detail_url;
    uint32_t create_time;
    RailArray<RailID> uploader_ids;
    RailString preview_url;
    RailString preview_scaling_url;
    RailArray<RailSpaceWorkVoteDetail> vote_details;
    RailSpaceWorkDescriptor() {
        create_time = 0;
    }
};

struct RailUserSpaceDownloadProgress {
    SpaceWorkID id;
    uint32_t progress;  // 0~100
    uint64_t total;     // bytes
    uint64_t finidshed; // bytes
    uint32_t speed;     // kbps
};


struct RailUserSpaceDownloadResult {
    SpaceWorkID id;
    uint32_t err_code;          //  成功情况下为0， 失败情况下为具体错误码
    uint64_t total_bytes;
    uint64_t finished_bytes;
    uint32_t total_files;
    uint32_t finished_files;
    RailString err_msg;
};

namespace rail_event {

struct AsyncGetMySubscribedWorksResult
    : public RailEvent<kRailEventUserSpaceGetMySubscribedWorksResult> {
    RailArray<RailSpaceWorkDescriptor> spacework_descriptors;
    uint32_t total_available_works;  // total count could be queried in this condition
    AsyncGetMySubscribedWorksResult() { total_available_works = 0; }
};

struct AsyncGetMyFavoritesWorksResult
    : public RailEvent<kRailEventUserSpaceGetMyFavoritesWorksResult> {
    RailArray<RailSpaceWorkDescriptor> spacework_descriptors;
    uint32_t total_available_works;  // total count could be queried in this condition
    AsyncGetMyFavoritesWorksResult() { total_available_works = 0; }
};

struct AsyncQuerySpaceWorksResult : public RailEvent<kRailEventUserSpaceQuerySpaceWorksResult> {
    RailArray<RailSpaceWorkDescriptor> spacework_descriptors;
    uint32_t total_available_works;  // total count could be queried in this condition
    AsyncQuerySpaceWorksResult() { total_available_works = 0; }
};

struct AsyncUpdateMetadataResult : public RailEvent<kRailEventUserSpaceUpdateMetadataResult> {
    SpaceWorkID id;
    EnumRailSpaceWorkType type;
    AsyncUpdateMetadataResult() { type = kRailSpaceWorkTypeMod; }
};

struct SyncSpaceWorkResult : public RailEvent<kRailEventUserSpaceSyncResult> {
    SpaceWorkID id;
    SyncSpaceWorkResult() {}
};

struct AsyncSubscribeSpaceWorksResult : public RailEvent<kRailEventUserSpaceSubscribeResult> {
    RailArray<SpaceWorkID> success_ids;
    RailArray<SpaceWorkID> failure_ids;
    bool subscribe;
    AsyncSubscribeSpaceWorksResult() { subscribe = true; }
};

struct AsyncModifyFavoritesWorksResult
    : public RailEvent<kRailEventUserSpaceModifyFavoritesWorksResult> {
    RailArray<SpaceWorkID> success_ids;
    RailArray<SpaceWorkID> failure_ids;
    EnumRailModifyFavoritesSpaceWorkType modify_flag;
    AsyncModifyFavoritesWorksResult() { modify_flag = kRailModifyFavoritesSpaceWorkTypeAdd; }
};

struct AsyncRemoveSpaceWorkResult : public RailEvent<kRailEventUserSpaceRemoveSpaceWorkResult> {
    SpaceWorkID id;
    AsyncRemoveSpaceWorkResult() {}
};

struct AsyncVoteSpaceWorkResult : public RailEvent<kRailEventUserSpaceVoteSpaceWorkResult> {
    SpaceWorkID id;
    AsyncVoteSpaceWorkResult() {}
};

struct AsyncRateSpaceWorkResult : public RailEvent<kRailEventUserSpaceRateSpaceWorkResult> {
    SpaceWorkID id;
    AsyncRateSpaceWorkResult() {}
};

struct AsyncSearchSpaceWorksResult : public RailEvent<kRailEventUserSpaceSearchSpaceWorkResult> {
    RailArray<RailSpaceWorkDescriptor> spacework_descriptors;
    uint32_t total_available_works;  // total count could be queried in this condition
    AsyncSearchSpaceWorksResult() { total_available_works = 0; }
};

struct UserSpaceDownloadProgress : public RailEvent<kRailEventUserSpaceDownloadProgress> {
    RailArray<RailUserSpaceDownloadProgress> progress;
    uint32_t total_progress;
    UserSpaceDownloadProgress() {
        total_progress = 0;
    }
};

struct UserSpaceDownloadResult : public RailEvent<kRailEventUserSpaceDownloadResult> {
    RailArray<RailUserSpaceDownloadResult> results;
    uint32_t total_results;
    UserSpaceDownloadResult() {
        total_results = 0;
    }
};

}  // namespace rail_event

struct QueryMySubscribedSpaceWorksResult {
    // Only spacework id in RailSpaceWorkDescriptor is available currently
    RailArray<RailSpaceWorkDescriptor> spacework_descriptors;
    EnumRailSpaceWorkType spacework_type;
    uint32_t total_available_works;  // total count could be queried in this condition
    QueryMySubscribedSpaceWorksResult() {
        total_available_works = 0;
        spacework_type = kRailSpaceWorkTypeMod;
    }
};

// AsyncUpdateMetadata's query options
// Any options set to ture will make AsyncUpdateMetadata cost more time to get data
// Attention: use default options ususally much faster
struct RailSpaceWorkUpdateOptions {
    // GetDescription and GetAdditionalPreviewUrls will return empty data if set to false
    bool with_detail;
    // GetMetadata, GetAllMetadata and GetLanguages won't return data if set to false
    bool with_metadata;
    // IsSubscribed will always return false if set to false
    bool check_has_subscribed;
    // IsFavorite will always return false if set to false
    bool check_has_favorited;
    // GetMyVote will always return kRailSpaceWorkVoteZero if set to false
    bool with_my_vote;
    // GetVoteDetail will always return empty data if set to false
    bool with_vote_detail;
    RailSpaceWorkUpdateOptions() {
        with_detail = true;            // get detail by default
        with_metadata = false;         // only use it when needed
        check_has_subscribed = false;  // do not suggest to use it
        check_has_favorited = false;   // do not suggest to use it
        with_my_vote = false;          // only use it when really need to show user's vote
        with_vote_detail = false;
    }
};

// Used by GetStatistic
enum EnumRailSpaceWorkStatistic {
    kRailSpaceWorkStatisticSubscriptionCount = 1,
    kRailSpaceWorkStatisticFavoriteCount = 2,
    kRailSpaceWorkStatisticDownloadCount = 3,
    kRailSpaceWorkStatisticScore = 4,  // total score
};

struct RailSpaceWorkSearchFilter {
    RailSpaceWorkSearchFilter() {
        match_all_required_metadata = false;
        match_all_required_tags = false;
    }
    RailString search_text;  // search text in spaceworks' data like name, max length is 128
    // let following array to emtpy to make it has no effect
    // max length of array is 10
    RailArray<RailString> required_tags;  // only return spaceworks have one of those tags
    RailArray<RailString> excluded_tags;  // only return spaceworks have none of those tags
    // only return spaceworks have one of those metadata
    RailArray<RailKeyValue> required_metadata;
    // only return spaceworks have none of those metadata
    RailArray<RailKeyValue> excluded_metadata;
    // set to true to return spacework match all meta in required_metadata
    bool match_all_required_metadata;
    // set to true to return spacework match all tags in required_tags
    bool match_all_required_tags;
};

// Used by GetState
enum EnumRailSpaceWorkState {
    kRailSpaceWorkStateNone = 0,          // No state infomation of this spacework
    kRailSpaceWorkStateDownloaded = 0x1,  // current spacework is downloaded (may need update)
    kRailSpaceWorkStateNeedsSync = 0x2,  // current spacework haven't been downloaded or need update
    kRailSpaceWorkStateDownloading = 0x4,  // current spacework is downloading
    kRailSpaceWorkStateUploading = 0x8,    // current spacework is uploading
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_USER_SPACE_DEFINE_H
