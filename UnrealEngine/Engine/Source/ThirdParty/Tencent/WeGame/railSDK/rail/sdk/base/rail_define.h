// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_DEFINE_H
#define RAIL_SDK_RAIL_DEFINE_H

#include "rail/sdk/base/stdint.h"
#include "rail/sdk/base/rail_string.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailPlatformType {
    kRailPlatformTGP = 1,
    kRailPlatformQQGame = 2,
};

enum EnumCommonLimit {
    kRailCommonMaxRepeatedKeys = 50,
    kRailCommonUsersInviteMaxUsersOnce = 128,
    kRailCommonMaxKeyLength = 256,
    kRailCommonMaxValueLength = 1024,
    kRailCommonUtilsCheckDirtyWordsStringMaxLength = 16384,
};

enum EnumRailIDValue {
    kInvalidRailId = 0,
    kInvalidGameId = kInvalidRailId,
    kInvalidDlcId = kInvalidGameId,
};

enum EnumRailIDDomain {
    kRailIDDomainInvalid = 0,
    kRailIDDomainPublic = 1,
};

class RailID {
  public:
    RailID() : id_(kInvalidRailId) {}
    RailID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ != kInvalidRailId; }

    EnumRailIDDomain GetDomain() const {
        int32_t domain_type = id_ >> 56;
        if (domain_type == kRailIDDomainPublic) {
            return kRailIDDomainPublic;
        }
        return kRailIDDomainInvalid;
    }

    bool operator==(const RailID& r) const { return id_ == r.id_; }
    bool operator!=(const RailID& r) const { return !(*this == r); }

  private:
    uint64_t id_;
};

class RailGameID {
  public:
    RailGameID() : id_(kInvalidRailId) {}
    RailGameID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ != kInvalidRailId; }

    bool operator==(const RailGameID& r) const { return id_ == r.id_; }
    bool operator!=(const RailGameID& r) const { return !(*this == r); }

  private:
    uint64_t id_;
};

class RailDlcID {
  public:
    RailDlcID() : id_(kInvalidRailId) {}
    RailDlcID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ != kInvalidRailId; }

    bool operator==(const RailDlcID& r) const { return id_ == r.id_; }
    bool operator!=(const RailDlcID& r) const { return !(*this == r); }

  private:
    uint64_t id_;
};

class RailVoiceChannelID {
  public:
    RailVoiceChannelID() : id_(kInvalidRailId) {}
    RailVoiceChannelID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ != kInvalidRailId; }

    bool operator==(const RailVoiceChannelID& r) const { return id_ == r.id_; }
    bool operator!=(const RailVoiceChannelID& r) const { return !(*this == r); }

  private:
    uint64_t id_;
};

class RailZoneID {
public:
    RailZoneID() : id_(kInvalidRailId) {}
    RailZoneID(uint64_t id) : id_(id) {}

    void set_id(uint64_t id) { id_ = id; }
    uint64_t get_id() const { return id_; }

    bool IsValid() const { return id_ != kInvalidRailId; }

    bool operator==(const RailZoneID& r) const { return id_ == r.id_; }
    bool operator!=(const RailZoneID& r) const { return !(*this == r); }

private:
    uint64_t id_;
};

// Key-Value
struct RailKeyValue {
    RailString key;
    RailString value;
};

// session ticket
struct RailSessionTicket {
    RailSessionTicket() {}
    RailString ticket;
};

// property for value of some key
enum EnumRailPropertyValueType {
    kRailPropertyValueTypeString = 1,
    kRailPropertyValueTypeInt = 2,
    kRailPropertyValueTypeDouble = 3,
};

enum EnumRailComparisonType {
    kRailComparisonTypeEqualToOrLessThan = 1,     // <=
    kRailComparisonTypeLessThan = 2,              // <
    kRailComparisonTypeEqual = 3,                 // ==
    kRailComparisonTypeGreaterThan = 4,           // >
    kRailComparisonTypeEqualToOrGreaterThan = 5,  // >=
    kRailComparisonTypeNotEqual = 6,              // !=
    kRailComparisonTypeIn = 7,                    // in, delimited by ";"
    kRailComparisonTypeNotIn = 8,                 // not in, delimited by ";"
    kRailComparisonTypeFuzzyMatch = 9,            // ~=, only kRailPropertyValueTypeString
    kRailComparisonTypeContain = 10,              // contain, delimited by ','
    kRailComparisonTypeNotContain = 11,           // not contain, delimited by ','
};

// sort type
enum EnumRailSortType {
    kRailSortTypeAsc = 1,
    kRailSortTypeDesc = 2,
    kRailSortTypeCloseTo = 3,
    kRailSortTypeDistanceNear = 4,
};

enum EnumRailOptionalValue {
    kRailOptionalNo = 0,
    kRailOptionalYes = 1,
    kRailOptionalAny = 2,
};

enum EnumRailGameRefundState {
    kRailGameRefundStateUnknown = 0,
    kRailGameRefundStateApplyReceived = 1000,
    kRailGameRefundStateUserCancelApply = 1100,
    kRailGameRefundStateAdminCancelApply = 1101,
    kRailGameRefundStateRefundApproved = 1150,
    kRailGameRefundStateRefundSuccess = 1200,
    kRailGameRefundStateRefundFailed = 1201,
};

typedef uint32_t RailProductID;
typedef uint64_t RailAssetID;

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_DEFINE_H
