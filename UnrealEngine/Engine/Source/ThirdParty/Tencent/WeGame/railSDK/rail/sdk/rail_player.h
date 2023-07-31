// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_PLAYER_H
#define RAIL_SDK_RAIL_PLAYER_H

#include "rail/sdk/rail_player_define.h"

namespace rail {

#pragma pack(push, RAIL_SDK_PACKING)

class IRailPlayer {
  public:
    // To check if a user has successfully logged in to the Rail platform.
    virtual bool AlreadyLoggedIn() = 0;

    virtual const RailID GetRailID() = 0;

    virtual RailResult GetPlayerDataPath(RailString* path) = 0;

    // trigger event AcquireSessionTicketResponse
    virtual RailResult AsyncAcquireSessionTicket(const RailString& user_data) = 0;

    // trigger event StartAuthSessionTicketResponse
    virtual RailResult AsyncStartSessionWithPlayer(const RailSessionTicket& player_ticket,
                        RailID player_rail_id,
                        const RailString& user_data) = 0;

    virtual void TerminateSessionOfPlayer(RailID player_rail_id) = 0;

    virtual void AbandonSessionTicket(const RailSessionTicket& session_ticket) = 0;

    virtual RailResult GetPlayerName(RailString* name) = 0;

    virtual EnumRailPlayerOwnershipType GetPlayerOwnershipType() = 0;

    // request get purchase key(such as cdkey) of yourself
    // event name: kRailEventPlayerGetGamePurchaseKey with data PlayerGetGamePurchaseKeyResult
    virtual RailResult AsyncGetGamePurchaseKey(const RailString& user_data) = 0;

    // if player's age is less than 18 years in China, the game revenue returned to player is
    // limited. If the returned value is true, call GetRateOfGameRevenue interface to get the
    // rate of game revenue.
    virtual bool IsGameRevenueLimited() = 0;

    // if IsGameRevenueLimited interface return true, it is need to count game on-line time.
    // if the game on-line time is more than 3 hours, the rate of game revenue will be half of.
    // else if the on-line time is more than 5 hours, it will be zero.
    //
    // for example, when killed some enemies:
    //     uint32_t enemies = GetKilledEnemies();
    //     uint32_t experience_points = enemies * KILL_EACH_ENEMY_POINTS;
    //     uint32_t real_experience_points = experience_points * GetRateOfGameRevenue();
    virtual float GetRateOfGameRevenue() = 0;

    // query current player's banned status for anti cheat
    virtual RailResult AsyncQueryPlayerBannedStatus(const RailString& user_data) = 0;

    // get an authenticate URL for the specified URL.
    // Callback is GetAuthenticateURLResult.
    virtual RailResult AsyncGetAuthenticateURL(const RailGetAuthenticateURLOptions& options,
                        const RailString& user_data) = 0;

    // Callback is RailGetPlayerMetadataResult.
    virtual RailResult AsyncGetPlayerMetadata(const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // callback is RailGetEncryptedGameTicketResult
    virtual RailResult AsyncGetEncryptedGameTicket(const RailString& set_metadata,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_PLAYER_H
