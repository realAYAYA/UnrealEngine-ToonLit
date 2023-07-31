// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ACHIEVEMENT_H
#define RAIL_SDK_RAIL_ACHIEVEMENT_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/base/rail_string.h"
#include "rail/sdk/rail_event.h"

// @desc The classes here provides reliable services for game achievements.
// To use the APIs, you will first need to configure meta data for achivements on developer
// portal. The APIs can only be used for achievements already submitted on developer portal.
// Below are 3 main scenarios
// 1) Unlock achievements for the current player
// 2) Check the unlock status for achievements for the current player or another
// 3) Get the percentage for players who have unlocked an achievement.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailPlayerAchievement;
class IRailGlobalAchievement;

class IRailAchievementHelper {
  public:
    // @desc Create an object to use interfaces in IRailPlayerAchievement
    // @param player Player's RailID. Use 0 for the current player
    // @return Pointer to the object of type IRailPlayerAchievement
    virtual IRailPlayerAchievement* CreatePlayerAchievement(const RailID& player) = 0;

    // @desc Get an object to check general achievement unlock status later, such as
    // the percentage of all players who have unlocked a specified achievement
    // @return Pointer to the object of type IRailGlobalAchievement
    virtual IRailGlobalAchievement* GetGlobalAchievement() = 0;
};

class IRailPlayerAchievement : public IRailComponent {
  public:
    // @desc Get the unique Rail ID for this IRailPlayerAchievement object
    // @return The Rail ID for the player. Should be the same RailID as the parameter in
    // CreatePlayerAchievement(const RailID& player)
    virtual RailID GetRailID() = 0;

    // @desc Asynchronously download a player's achivement info to local
    // This will trigger the event PlayerAchievementReceived
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncRequestAchievement(const RailString& user_data) = 0;

    // @desc Check whether the achievement of 'name' is unlocked
    // @param name Name of the specified achievement
    // @param achieved Indicates whether the specified achievement is unlocked
    virtual RailResult HasAchieved(const RailString& name, bool* achieved) = 0;

    // @desc Retrieve info of JSON format for the achievement of 'name'
    // The info contains below key-value pairs. More key-value pairs could be added in future.
    // {
    //  "name": "name",
    //  "description": "desc",
    //  "display_name": "display name",
    //  "achieved": 1,
    //  "achieved_time":123456, // seconds since January 1 1970
    //  "icon_index": 1,
    //  "icon_url": "http://...",
    //  "unachieved_icon_url": "http://...",
    //  "is_process" : true,
    //  "hidden" : false,
    //  "cur_value" : 100,
    //  "unlock_value" : 100
    //  }
    // @param name The name of the achievement
    // @param achievement_info The game info retrieved if the call succeeds
    // @return Returns kSuccess on success
    virtual RailResult GetAchievementInfo(const RailString& name, RailString* achievement_info) = 0;

    // @desc Set the progress for an achievement. If 'current_value' is equal to or larger than
    // 'max_value', the achievement will be unlocked. Asynchronously, this will trigger the event
    // PlayerAchievementStored. It can only be used for the current player. If the object was
    // created with another player's RailID, kErrorAchievementNotMyAchievement will be returned.
    // @param current_value The current progress to unlock the achievement of 'name'
    // @param max_value Defaults to 0. If 0, the value configured on developer portal will be
    // used, otherwise 'max_value' will be used for comparison rather than the one configured on
    // the server. Usually, it is recommended to configure 'max_value' on developer portal.
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success. If another player's RailID was used to create the
    // object, kErrorAchievementNotMyAchievement will be returned.
    virtual RailResult AsyncTriggerAchievementProgress(const RailString& name,
                        uint32_t current_value,
                        uint32_t max_value = 0,
                        const RailString& user_data = "") = 0;

    // @desc Unlock the achievement locally. On the server, the unlock status does not change.
    // To update the unlock status to the server, call AsyncStoreAchievement
    // @param name Name of achievement
    // @return Returns kSuccess on success. If another player's RailID was used to create the
    // object, kErrorAchievementNotMyAchievement will be returned.
    virtual RailResult MakeAchievement(const RailString& name) = 0;

    // @desc Clear achievement info locally. The achievement status on server will not change.
    // @param name Name of achievement
    // @return Returns kSuccess on success. If another player's RailID was used to create the
    // object, kErrorAchievementNotMyAchievement will be returned.
    virtual RailResult CancelAchievement(const RailString& name) = 0;

    // @desc Upload the updated local achievement info to server
    // The event PlayerAchievementStored will be triggered after the call.
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncStoreAchievement(const RailString& user_data) = 0;

    // @desc Reset all the achievements to initial status for the current player. Please be
    // cautious. Usually this is only used for development purposes.
    // @return Returns kSuccess on success. If another player's RailID was used to create the
    // object, kErrorAchievementNotMyAchievement will be returned.
    virtual RailResult ResetAllAchievements() = 0;

    // @desc Retrieve an array of names for all the achievements, whether unlocked or not
    // @param names The array of achievement names
    // @return Returns kSuccess on success
    virtual RailResult GetAllAchievementsName(RailArray<RailString>* names) = 0;
};

class IRailGlobalAchievement : public IRailComponent {
  public:
    // @desc Asynchronously retrieve the games stats data to local so that
    // the other two synchronous interfaces can use later.
    // The struct GlobalAchievementReceived contains the number of achievements.
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncRequestAchievement(const RailString& user_data) = 0;

    // @desc Check the percentage of players who have unlocked the achivement of 'name'
    // AsyncRequestAchievement must be called before this interface is used
    // @param name Name of achievement
    // @return Returns kSuccess on success
    virtual RailResult GetGlobalAchievedPercent(const RailString& name, double* percent) = 0;

    // @desc For achievements sorted in descending order according to the number of players who
    // have successfully unlocked, this will retrieve info for the one that ranks 'index'
    // The 'index' starts from 0
    // @param index The range should be in [0, GlobalAchievementReceived::count)
    // For index as 0, info for the achievement most players unlocked will be retrieved
    // @param name The achievement name retrieved if the call succeeds
    // @param percent The percentage of all players who have unlocked the achievement
    // @return Returns kErrorAchievementOutofRange if 'index' is out of the
    // range [0, GlobalAchievementReceived::count). Returns kSuccess on success.
    virtual RailResult GetGlobalAchievedPercentDescending(int32_t index,
                        RailString* name,
                        double* percent) = 0;
};

namespace rail_event {

struct PlayerAchievementReceived
    : public RailEvent<kRailEventAchievementPlayerAchievementReceived> {
    PlayerAchievementReceived() {}
};

struct PlayerAchievementStored : public RailEvent<kRailEventAchievementPlayerAchievementStored> {
    PlayerAchievementStored() {
        group_achievement = false;
        current_progress = 0;
        max_progress = 0;
    }

    bool group_achievement;
    RailString achievement_name;
    uint32_t current_progress;
    uint32_t max_progress;
};

struct GlobalAchievementReceived
    : public RailEvent<kRailEventAchievementGlobalAchievementReceived> {
    GlobalAchievementReceived() { count = 0; }

    int32_t count;  // number of all achievements
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_ACHIEVEMENT_H
