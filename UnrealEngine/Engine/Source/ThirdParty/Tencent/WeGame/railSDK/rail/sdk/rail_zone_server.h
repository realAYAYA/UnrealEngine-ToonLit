// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ZONE_SERVER_H
#define RAIL_SDK_RAIL_ZONE_SERVER_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_zone_server_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailZoneServer;

class IRailZoneServerHelper {
  public:
    virtual RailZoneID GetPlayerSelectedZoneID() = 0;

    virtual RailZoneID GetRootZoneID() = 0;

    virtual IRailZoneServer* OpenZoneServer(const RailZoneID& zone_id, RailResult* result) = 0;

    virtual RailResult AsyncSwitchPlayerSelectedZone(const RailZoneID& zone_id) = 0;
};

class IRailZoneServer : public IRailComponent {
  public:
    virtual RailZoneID GetZoneID() = 0;

    virtual RailResult GetZoneNameLanguages(RailArray<RailString>* languages) = 0;

    virtual RailResult GetZoneName(const RailString& language_filter, RailString* zone_name) = 0;

    virtual RailResult GetZoneDescriptionLanguages(RailArray<RailString>* languages) = 0;

    virtual RailResult GetZoneDescription(const RailString& language_filter,
                        RailString* zone_description) = 0;

    virtual RailResult GetGameServerAddresses(RailArray<RailString>* server_addresses) = 0;

    virtual RailResult GetZoneMetadatas(RailArray<RailKeyValue>* key_values) = 0;

    virtual RailResult GetChildrenZoneIDs(RailArray<RailZoneID>* zone_ids) = 0;

    virtual bool IsZoneVisiable() = 0;

    virtual bool IsZoneJoinable() = 0;

    virtual uint32_t GetZoneEnableStartTime() = 0;

    virtual uint32_t GetZoneEnableEndTime() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_ZONE_SERVER_H
