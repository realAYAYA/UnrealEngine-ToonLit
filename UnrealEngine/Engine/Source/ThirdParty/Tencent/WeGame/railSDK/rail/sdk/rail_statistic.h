// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_STATISTIC_H
#define RAIL_SDK_RAIL_STATISTIC_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailPlayerStats;
class IRailGlobalStats;
class IRailStatisticHelper {
  public:
    virtual IRailPlayerStats* CreatePlayerStats(const RailID& player) = 0;

    virtual IRailGlobalStats* GetGlobalStats() = 0;

    // trigger event NumberOfPlayerReceived
    virtual RailResult AsyncGetNumberOfPlayer(const RailString& user_data) = 0;
};

class IRailPlayerStats : public IRailComponent {
  public:
    virtual RailID GetRailID() = 0;

    // trigger event PlayerStatsReceived
    virtual RailResult AsyncRequestStats(const RailString& user_data) = 0;

    virtual RailResult GetStatValue(const RailString& name, int32_t* data) = 0;
    virtual RailResult GetStatValue(const RailString& name, double* data) = 0;

    // just set my stat, otherwise return kErrorStatsDontSetOtherPlayerStat.
    virtual RailResult SetStatValue(const RailString& name, int32_t data) = 0;
    virtual RailResult SetStatValue(const RailString& name, double data) = 0;
    virtual RailResult UpdateAverageStatValue(const RailString& name, double data) = 0;

    // trigger event PlayerStatsStored, just store my states.
    virtual RailResult AsyncStoreStats(const RailString& user_data) = 0;

    // just reset my stat, otherwise return kErrorStatsDontSetOtherPlayerStat.
    virtual RailResult ResetAllStats() = 0;
};

class IRailGlobalStats : public IRailComponent {
  public:
    // trigger event GlobalStatsReceived
    virtual RailResult AsyncRequestGlobalStats(const RailString& user_data) = 0;

    virtual RailResult GetGlobalStatValue(const RailString& name, int64_t* data) = 0;
    virtual RailResult GetGlobalStatValue(const RailString& name, double* data) = 0;

    // Gets history value
    // global_stats_data will be filled with daily values, starting with today.
    // data_size is the size of the global_stats_data array
    // num_global_stats will be filled with the number of GlobalStats stored in back-end server.
    virtual RailResult GetGlobalStatValueHistory(const RailString& name,
                        int64_t* global_stats_data,
                        uint32_t data_size,
                        int32_t* num_global_stats) = 0;

    virtual RailResult GetGlobalStatValueHistory(const RailString& name,
                        double* global_stats_data,
                        uint32_t data_size,
                        int32_t* num_global_stats) = 0;
};

namespace rail_event {

struct PlayerStatsReceived : public RailEvent<kRailEventStatsPlayerStatsReceived> {
    PlayerStatsReceived() {}
};

struct PlayerStatsStored : public RailEvent<kRailEventStatsPlayerStatsStored> {
    PlayerStatsStored() {}
};

struct NumberOfPlayerReceived : public RailEvent<kRailEventStatsNumOfPlayerReceived> {
    NumberOfPlayerReceived() {
        online_number = 0;
        offline_number = 0;
    }
    int32_t online_number;   // number of player online
    int32_t offline_number;  // number of player offline
};

struct GlobalStatsRequestReceived : public RailEvent<kRailEventStatsGlobalStatsReceived> {
    GlobalStatsRequestReceived() {}
};

}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_STATISTIC_H
