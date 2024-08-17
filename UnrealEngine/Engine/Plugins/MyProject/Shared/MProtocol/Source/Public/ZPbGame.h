#pragma once
#include "ZFmt.h"
#include "ZPbDefines.h"
#include "ZPbCommon.h"
#include "ZPbGame.generated.h"


namespace idlepb {
class Ping;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPing
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 req_ticks;


    FPbPing();
    FPbPing(const idlepb::Ping& Right);
    void FromPb(const idlepb::Ping& Right);
    void ToPb(idlepb::Ping* Out) const;
    void Reset();
    void operator=(const idlepb::Ping& Right);
    bool operator==(const FPbPing& Right) const;
    bool operator!=(const FPbPing& Right) const;
     
};

namespace idlepb {
class Pong;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPong
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 req_ticks;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 rsp_ticks;


    FPbPong();
    FPbPong(const idlepb::Pong& Right);
    void FromPb(const idlepb::Pong& Right);
    void ToPb(idlepb::Pong* Out) const;
    void Reset();
    void operator=(const idlepb::Pong& Right);
    bool operator==(const FPbPong& Right) const;
    bool operator!=(const FPbPong& Right) const;
     
};

namespace idlepb {
class DoGmCommand;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoGmCommand
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString command;


    FPbDoGmCommand();
    FPbDoGmCommand(const idlepb::DoGmCommand& Right);
    void FromPb(const idlepb::DoGmCommand& Right);
    void ToPb(idlepb::DoGmCommand* Out) const;
    void Reset();
    void operator=(const idlepb::DoGmCommand& Right);
    bool operator==(const FPbDoGmCommand& Right) const;
    bool operator!=(const FPbDoGmCommand& Right) const;
     
};

namespace idlepb {
class ReportError;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReportError
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString text;


    FPbReportError();
    FPbReportError(const idlepb::ReportError& Right);
    void FromPb(const idlepb::ReportError& Right);
    void ToPb(idlepb::ReportError* Out) const;
    void Reset();
    void operator=(const idlepb::ReportError& Right);
    bool operator==(const FPbReportError& Right) const;
    bool operator!=(const FPbReportError& Right) const;
     
};

namespace idlepb {
class LoginGameReq;
}  // namespace idlepb

/**
 * 登录游戏
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginGameReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString account;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString client_version;


    FPbLoginGameReq();
    FPbLoginGameReq(const idlepb::LoginGameReq& Right);
    void FromPb(const idlepb::LoginGameReq& Right);
    void ToPb(idlepb::LoginGameReq* Out) const;
    void Reset();
    void operator=(const idlepb::LoginGameReq& Right);
    bool operator==(const FPbLoginGameReq& Right) const;
    bool operator!=(const FPbLoginGameReq& Right) const;
     
};

namespace idlepb {
class LoginGameAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginGameAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbLoginGameRetCode ret;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleData role_data;

    /** 是否为重连 (即服务器上玩家对象已经存在) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_relogin;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbOfflineAwardSummary offline_award_summary;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSelfSeptInfo sept_info;


    FPbLoginGameAck();
    FPbLoginGameAck(const idlepb::LoginGameAck& Right);
    void FromPb(const idlepb::LoginGameAck& Right);
    void ToPb(idlepb::LoginGameAck* Out) const;
    void Reset();
    void operator=(const idlepb::LoginGameAck& Right);
    bool operator==(const FPbLoginGameAck& Right) const;
    bool operator!=(const FPbLoginGameAck& Right) const;
     
};

namespace idlepb {
class RefreshInventoryData;
}  // namespace idlepb

/**
 * 刷新包裹数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshInventoryData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> items;


    FPbRefreshInventoryData();
    FPbRefreshInventoryData(const idlepb::RefreshInventoryData& Right);
    void FromPb(const idlepb::RefreshInventoryData& Right);
    void ToPb(idlepb::RefreshInventoryData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshInventoryData& Right);
    bool operator==(const FPbRefreshInventoryData& Right) const;
    bool operator!=(const FPbRefreshInventoryData& Right) const;
     
};

namespace idlepb {
class SetCurrentCultivationDirectionReq;
}  // namespace idlepb

/**
 * 设置当前修炼方向
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetCurrentCultivationDirectionReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;


    FPbSetCurrentCultivationDirectionReq();
    FPbSetCurrentCultivationDirectionReq(const idlepb::SetCurrentCultivationDirectionReq& Right);
    void FromPb(const idlepb::SetCurrentCultivationDirectionReq& Right);
    void ToPb(idlepb::SetCurrentCultivationDirectionReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetCurrentCultivationDirectionReq& Right);
    bool operator==(const FPbSetCurrentCultivationDirectionReq& Right) const;
    bool operator!=(const FPbSetCurrentCultivationDirectionReq& Right) const;
     
};

namespace idlepb {
class SetCurrentCultivationDirectionAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetCurrentCultivationDirectionAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;


    FPbSetCurrentCultivationDirectionAck();
    FPbSetCurrentCultivationDirectionAck(const idlepb::SetCurrentCultivationDirectionAck& Right);
    void FromPb(const idlepb::SetCurrentCultivationDirectionAck& Right);
    void ToPb(idlepb::SetCurrentCultivationDirectionAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetCurrentCultivationDirectionAck& Right);
    bool operator==(const FPbSetCurrentCultivationDirectionAck& Right) const;
    bool operator!=(const FPbSetCurrentCultivationDirectionAck& Right) const;
     
};

namespace idlepb {
class RefreshCurrentCultivationDirection;
}  // namespace idlepb

/**
 * 刷新当前修炼方向
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshCurrentCultivationDirection
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;


    FPbRefreshCurrentCultivationDirection();
    FPbRefreshCurrentCultivationDirection(const idlepb::RefreshCurrentCultivationDirection& Right);
    void FromPb(const idlepb::RefreshCurrentCultivationDirection& Right);
    void ToPb(idlepb::RefreshCurrentCultivationDirection* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshCurrentCultivationDirection& Right);
    bool operator==(const FPbRefreshCurrentCultivationDirection& Right) const;
    bool operator!=(const FPbRefreshCurrentCultivationDirection& Right) const;
     
};

namespace idlepb {
class RefreshCultivationRankData;
}  // namespace idlepb

/**
 * 刷新等级经验数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshCultivationRankData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRankData rank_data;

    /** 修炼方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;

    /** 最后一次 ExpTick 的时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_exp_cycle_timestamp;


    FPbRefreshCultivationRankData();
    FPbRefreshCultivationRankData(const idlepb::RefreshCultivationRankData& Right);
    void FromPb(const idlepb::RefreshCultivationRankData& Right);
    void ToPb(idlepb::RefreshCultivationRankData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshCultivationRankData& Right);
    bool operator==(const FPbRefreshCultivationRankData& Right) const;
    bool operator!=(const FPbRefreshCultivationRankData& Right) const;
     
};

namespace idlepb {
class RefreshCultivationData;
}  // namespace idlepb

/**
 * 刷新修炼数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshCultivationData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCultivationData cultivation_data;

    /** 修炼方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;


    FPbRefreshCultivationData();
    FPbRefreshCultivationData(const idlepb::RefreshCultivationData& Right);
    void FromPb(const idlepb::RefreshCultivationData& Right);
    void ToPb(idlepb::RefreshCultivationData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshCultivationData& Right);
    bool operator==(const FPbRefreshCultivationData& Right) const;
    bool operator!=(const FPbRefreshCultivationData& Right) const;
     
};

namespace idlepb {
class RefreshCurrencyData;
}  // namespace idlepb

/**
 * 刷新货币
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshCurrencyData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCurrencyData data;


    FPbRefreshCurrencyData();
    FPbRefreshCurrencyData(const idlepb::RefreshCurrencyData& Right);
    void FromPb(const idlepb::RefreshCurrencyData& Right);
    void ToPb(idlepb::RefreshCurrencyData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshCurrencyData& Right);
    bool operator==(const FPbRefreshCurrencyData& Right) const;
    bool operator!=(const FPbRefreshCurrencyData& Right) const;
     
};

namespace idlepb {
class RefreshDailyCounterData;
}  // namespace idlepb

/**
 * 刷新今日计数
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshDailyCounterData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDailyCounter daily_counter;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleWeeklyCounter weekly_counter;


    FPbRefreshDailyCounterData();
    FPbRefreshDailyCounterData(const idlepb::RefreshDailyCounterData& Right);
    void FromPb(const idlepb::RefreshDailyCounterData& Right);
    void ToPb(idlepb::RefreshDailyCounterData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshDailyCounterData& Right);
    bool operator==(const FPbRefreshDailyCounterData& Right) const;
    bool operator!=(const FPbRefreshDailyCounterData& Right) const;
     
};

namespace idlepb {
class RefreshLastUnlockArenaId;
}  // namespace idlepb

/**
 * 刷新最后解锁到的秘境地图
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshLastUnlockArenaId
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_unlock_arena_id;


    FPbRefreshLastUnlockArenaId();
    FPbRefreshLastUnlockArenaId(const idlepb::RefreshLastUnlockArenaId& Right);
    void FromPb(const idlepb::RefreshLastUnlockArenaId& Right);
    void ToPb(idlepb::RefreshLastUnlockArenaId* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshLastUnlockArenaId& Right);
    bool operator==(const FPbRefreshLastUnlockArenaId& Right) const;
    bool operator!=(const FPbRefreshLastUnlockArenaId& Right) const;
     
};

namespace idlepb {
class RefreshUnlockedEquipmentSlots;
}  // namespace idlepb

/**
 * 刷新已解锁装备槽位列表
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshUnlockedEquipmentSlots
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> slots;


    FPbRefreshUnlockedEquipmentSlots();
    FPbRefreshUnlockedEquipmentSlots(const idlepb::RefreshUnlockedEquipmentSlots& Right);
    void FromPb(const idlepb::RefreshUnlockedEquipmentSlots& Right);
    void ToPb(idlepb::RefreshUnlockedEquipmentSlots* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshUnlockedEquipmentSlots& Right);
    bool operator==(const FPbRefreshUnlockedEquipmentSlots& Right) const;
    bool operator!=(const FPbRefreshUnlockedEquipmentSlots& Right) const;
     
};

namespace idlepb {
class UnlockEquipmentSlotReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockEquipmentSlotReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbUnlockEquipmentSlotReq();
    FPbUnlockEquipmentSlotReq(const idlepb::UnlockEquipmentSlotReq& Right);
    void FromPb(const idlepb::UnlockEquipmentSlotReq& Right);
    void ToPb(idlepb::UnlockEquipmentSlotReq* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockEquipmentSlotReq& Right);
    bool operator==(const FPbUnlockEquipmentSlotReq& Right) const;
    bool operator!=(const FPbUnlockEquipmentSlotReq& Right) const;
     
};

namespace idlepb {
class UnlockEquipmentSlotAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockEquipmentSlotAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUnlockEquipmentSlotAck();
    FPbUnlockEquipmentSlotAck(const idlepb::UnlockEquipmentSlotAck& Right);
    void FromPb(const idlepb::UnlockEquipmentSlotAck& Right);
    void ToPb(idlepb::UnlockEquipmentSlotAck* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockEquipmentSlotAck& Right);
    bool operator==(const FPbUnlockEquipmentSlotAck& Right) const;
    bool operator!=(const FPbUnlockEquipmentSlotAck& Right) const;
     
};

namespace idlepb {
class ThunderTestRoundData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbThunderTestRoundData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 round;

    /** 本轮伤害量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float damage;

    /** 剩余血量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hp;

    /** 剩余蓝量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mp;


    FPbThunderTestRoundData();
    FPbThunderTestRoundData(const idlepb::ThunderTestRoundData& Right);
    void FromPb(const idlepb::ThunderTestRoundData& Right);
    void ToPb(idlepb::ThunderTestRoundData* Out) const;
    void Reset();
    void operator=(const idlepb::ThunderTestRoundData& Right);
    bool operator==(const FPbThunderTestRoundData& Right) const;
    bool operator!=(const FPbThunderTestRoundData& Right) const;
     
};

namespace idlepb {
class ThunderTestData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbThunderTestData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbThunderTestRoundData> rounds;


    FPbThunderTestData();
    FPbThunderTestData(const idlepb::ThunderTestData& Right);
    void FromPb(const idlepb::ThunderTestData& Right);
    void ToPb(idlepb::ThunderTestData* Out) const;
    void Reset();
    void operator=(const idlepb::ThunderTestData& Right);
    bool operator==(const FPbThunderTestData& Right) const;
    bool operator!=(const FPbThunderTestData& Right) const;
     
};

namespace idlepb {
class DoBreakthroughReq;
}  // namespace idlepb

/**
 * 进行突破
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoBreakthroughReq
{
    GENERATED_BODY();

    /** 突破丹药的道具id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 item_id;


    FPbDoBreakthroughReq();
    FPbDoBreakthroughReq(const idlepb::DoBreakthroughReq& Right);
    void FromPb(const idlepb::DoBreakthroughReq& Right);
    void ToPb(idlepb::DoBreakthroughReq* Out) const;
    void Reset();
    void operator=(const idlepb::DoBreakthroughReq& Right);
    bool operator==(const FPbDoBreakthroughReq& Right) const;
    bool operator!=(const FPbDoBreakthroughReq& Right) const;
     
};

namespace idlepb {
class DoBreakthroughAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoBreakthroughAck
{
    GENERATED_BODY();

    /** 突破是否成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool success;

    /** 本次突破类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbBreakthroughType old_type;

    /** 下次突破类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbBreakthroughType new_type;

    /** 本次是否为突破瓶颈(已含特殊处理) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_bottleneck;

    /** 雷劫数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbThunderTestData thunder_test_data;


    FPbDoBreakthroughAck();
    FPbDoBreakthroughAck(const idlepb::DoBreakthroughAck& Right);
    void FromPb(const idlepb::DoBreakthroughAck& Right);
    void ToPb(idlepb::DoBreakthroughAck* Out) const;
    void Reset();
    void operator=(const idlepb::DoBreakthroughAck& Right);
    bool operator==(const FPbDoBreakthroughAck& Right) const;
    bool operator!=(const FPbDoBreakthroughAck& Right) const;
     
};

namespace idlepb {
class RefreshItems;
}  // namespace idlepb

/**
 * 刷新包裹数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshItems
{
    GENERATED_BODY();

    /** 添加或更改 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> items;

    /** 待删除道具列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> junks;

    /** 添加的其它不进包裹的道具(如：Token) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> others;

    /** 静默添加或更改道具(不弹提示) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> quiet_items;


    FPbRefreshItems();
    FPbRefreshItems(const idlepb::RefreshItems& Right);
    void FromPb(const idlepb::RefreshItems& Right);
    void ToPb(idlepb::RefreshItems* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshItems& Right);
    bool operator==(const FPbRefreshItems& Right) const;
    bool operator!=(const FPbRefreshItems& Right) const;
     
};

namespace idlepb {
class RefreshTemporaryPackageItems;
}  // namespace idlepb

/**
 * 刷新临时包裹中的道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshTemporaryPackageItems
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbTemporaryPackageItem> items;

    /** 包裹中道具总数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_extract_time;


    FPbRefreshTemporaryPackageItems();
    FPbRefreshTemporaryPackageItems(const idlepb::RefreshTemporaryPackageItems& Right);
    void FromPb(const idlepb::RefreshTemporaryPackageItems& Right);
    void ToPb(idlepb::RefreshTemporaryPackageItems* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshTemporaryPackageItems& Right);
    bool operator==(const FPbRefreshTemporaryPackageItems& Right) const;
    bool operator!=(const FPbRefreshTemporaryPackageItems& Right) const;
     
};

namespace idlepb {
class ExtractTemporaryPackageItemsReq;
}  // namespace idlepb

/**
 * 提取临时包裹中所有道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbExtractTemporaryPackageItemsReq
{
    GENERATED_BODY();


    FPbExtractTemporaryPackageItemsReq();
    FPbExtractTemporaryPackageItemsReq(const idlepb::ExtractTemporaryPackageItemsReq& Right);
    void FromPb(const idlepb::ExtractTemporaryPackageItemsReq& Right);
    void ToPb(idlepb::ExtractTemporaryPackageItemsReq* Out) const;
    void Reset();
    void operator=(const idlepb::ExtractTemporaryPackageItemsReq& Right);
    bool operator==(const FPbExtractTemporaryPackageItemsReq& Right) const;
    bool operator!=(const FPbExtractTemporaryPackageItemsReq& Right) const;
     
};

namespace idlepb {
class ExtractTemporaryPackageItemsAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbExtractTemporaryPackageItemsAck
{
    GENERATED_BODY();


    FPbExtractTemporaryPackageItemsAck();
    FPbExtractTemporaryPackageItemsAck(const idlepb::ExtractTemporaryPackageItemsAck& Right);
    void FromPb(const idlepb::ExtractTemporaryPackageItemsAck& Right);
    void ToPb(idlepb::ExtractTemporaryPackageItemsAck* Out) const;
    void Reset();
    void operator=(const idlepb::ExtractTemporaryPackageItemsAck& Right);
    bool operator==(const FPbExtractTemporaryPackageItemsAck& Right) const;
    bool operator!=(const FPbExtractTemporaryPackageItemsAck& Right) const;
     
};

namespace idlepb {
class GetTemporaryPackageDataReq;
}  // namespace idlepb

/**
 * 获取临时包裹数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetTemporaryPackageDataReq
{
    GENERATED_BODY();


    FPbGetTemporaryPackageDataReq();
    FPbGetTemporaryPackageDataReq(const idlepb::GetTemporaryPackageDataReq& Right);
    void FromPb(const idlepb::GetTemporaryPackageDataReq& Right);
    void ToPb(idlepb::GetTemporaryPackageDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetTemporaryPackageDataReq& Right);
    bool operator==(const FPbGetTemporaryPackageDataReq& Right) const;
    bool operator!=(const FPbGetTemporaryPackageDataReq& Right) const;
     
};

namespace idlepb {
class GetTemporaryPackageDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetTemporaryPackageDataAck
{
    GENERATED_BODY();

    /** 道具列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbTemporaryPackageItem> items;

    /** 最后一次提取的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_extract_time;


    FPbGetTemporaryPackageDataAck();
    FPbGetTemporaryPackageDataAck(const idlepb::GetTemporaryPackageDataAck& Right);
    void FromPb(const idlepb::GetTemporaryPackageDataAck& Right);
    void ToPb(idlepb::GetTemporaryPackageDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetTemporaryPackageDataAck& Right);
    bool operator==(const FPbGetTemporaryPackageDataAck& Right) const;
    bool operator!=(const FPbGetTemporaryPackageDataAck& Right) const;
     
};

namespace idlepb {
class GetArenaExplorationStatisticalDataReq;
}  // namespace idlepb

/**
 * 获取秘境探索统计数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetArenaExplorationStatisticalDataReq
{
    GENERATED_BODY();


    FPbGetArenaExplorationStatisticalDataReq();
    FPbGetArenaExplorationStatisticalDataReq(const idlepb::GetArenaExplorationStatisticalDataReq& Right);
    void FromPb(const idlepb::GetArenaExplorationStatisticalDataReq& Right);
    void ToPb(idlepb::GetArenaExplorationStatisticalDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetArenaExplorationStatisticalDataReq& Right);
    bool operator==(const FPbGetArenaExplorationStatisticalDataReq& Right) const;
    bool operator!=(const FPbGetArenaExplorationStatisticalDataReq& Right) const;
     
};

namespace idlepb {
class GetArenaExplorationStatisticalDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetArenaExplorationStatisticalDataAck
{
    GENERATED_BODY();

    /** 统计数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleArenaExplorationStatisticalData data;


    FPbGetArenaExplorationStatisticalDataAck();
    FPbGetArenaExplorationStatisticalDataAck(const idlepb::GetArenaExplorationStatisticalDataAck& Right);
    void FromPb(const idlepb::GetArenaExplorationStatisticalDataAck& Right);
    void ToPb(idlepb::GetArenaExplorationStatisticalDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetArenaExplorationStatisticalDataAck& Right);
    bool operator==(const FPbGetArenaExplorationStatisticalDataAck& Right) const;
    bool operator!=(const FPbGetArenaExplorationStatisticalDataAck& Right) const;
     
};

namespace idlepb {
class DoBreathingExerciseReq;
}  // namespace idlepb

/**
 * 吐纳
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoBreathingExerciseReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float percet;


    FPbDoBreathingExerciseReq();
    FPbDoBreathingExerciseReq(const idlepb::DoBreathingExerciseReq& Right);
    void FromPb(const idlepb::DoBreathingExerciseReq& Right);
    void ToPb(idlepb::DoBreathingExerciseReq* Out) const;
    void Reset();
    void operator=(const idlepb::DoBreathingExerciseReq& Right);
    bool operator==(const FPbDoBreathingExerciseReq& Right) const;
    bool operator!=(const FPbDoBreathingExerciseReq& Right) const;
     
};

namespace idlepb {
class DoBreathingExerciseAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoBreathingExerciseAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbDoBreathingExerciseResult result;


    FPbDoBreathingExerciseAck();
    FPbDoBreathingExerciseAck(const idlepb::DoBreathingExerciseAck& Right);
    void FromPb(const idlepb::DoBreathingExerciseAck& Right);
    void ToPb(idlepb::DoBreathingExerciseAck* Out) const;
    void Reset();
    void operator=(const idlepb::DoBreathingExerciseAck& Right);
    bool operator==(const FPbDoBreathingExerciseAck& Right) const;
    bool operator!=(const FPbDoBreathingExerciseAck& Right) const;
     
};

namespace idlepb {
class OneClickMergeBreathingReq;
}  // namespace idlepb

/**
 * 一键合并吐纳
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickMergeBreathingReq
{
    GENERATED_BODY();


    FPbOneClickMergeBreathingReq();
    FPbOneClickMergeBreathingReq(const idlepb::OneClickMergeBreathingReq& Right);
    void FromPb(const idlepb::OneClickMergeBreathingReq& Right);
    void ToPb(idlepb::OneClickMergeBreathingReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickMergeBreathingReq& Right);
    bool operator==(const FPbOneClickMergeBreathingReq& Right) const;
    bool operator!=(const FPbOneClickMergeBreathingReq& Right) const;
     
};

namespace idlepb {
class OneClickMergeBreathingAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickMergeBreathingAck
{
    GENERATED_BODY();

    /** 每次吐纳经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<float> exp;

    /** 每次吐纳倍率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<float> ret;


    FPbOneClickMergeBreathingAck();
    FPbOneClickMergeBreathingAck(const idlepb::OneClickMergeBreathingAck& Right);
    void FromPb(const idlepb::OneClickMergeBreathingAck& Right);
    void ToPb(idlepb::OneClickMergeBreathingAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickMergeBreathingAck& Right);
    bool operator==(const FPbOneClickMergeBreathingAck& Right) const;
    bool operator!=(const FPbOneClickMergeBreathingAck& Right) const;
     
};

namespace idlepb {
class RequestCommonCultivationDataReq;
}  // namespace idlepb

/**
 * 请求公共修炼数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestCommonCultivationDataReq
{
    GENERATED_BODY();


    FPbRequestCommonCultivationDataReq();
    FPbRequestCommonCultivationDataReq(const idlepb::RequestCommonCultivationDataReq& Right);
    void FromPb(const idlepb::RequestCommonCultivationDataReq& Right);
    void ToPb(idlepb::RequestCommonCultivationDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestCommonCultivationDataReq& Right);
    bool operator==(const FPbRequestCommonCultivationDataReq& Right) const;
    bool operator!=(const FPbRequestCommonCultivationDataReq& Right) const;
     
};

namespace idlepb {
class RequestCommonCultivationDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestCommonCultivationDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCommonCultivationData data;


    FPbRequestCommonCultivationDataAck();
    FPbRequestCommonCultivationDataAck(const idlepb::RequestCommonCultivationDataAck& Right);
    void FromPb(const idlepb::RequestCommonCultivationDataAck& Right);
    void ToPb(idlepb::RequestCommonCultivationDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestCommonCultivationDataAck& Right);
    bool operator==(const FPbRequestCommonCultivationDataAck& Right) const;
    bool operator!=(const FPbRequestCommonCultivationDataAck& Right) const;
     
};

namespace idlepb {
class ReceiveBreathingExerciseRewardReq;
}  // namespace idlepb

/**
 * 请求领取吐纳奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBreathingExerciseRewardReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbReceiveBreathingExerciseRewardReq();
    FPbReceiveBreathingExerciseRewardReq(const idlepb::ReceiveBreathingExerciseRewardReq& Right);
    void FromPb(const idlepb::ReceiveBreathingExerciseRewardReq& Right);
    void ToPb(idlepb::ReceiveBreathingExerciseRewardReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBreathingExerciseRewardReq& Right);
    bool operator==(const FPbReceiveBreathingExerciseRewardReq& Right) const;
    bool operator!=(const FPbReceiveBreathingExerciseRewardReq& Right) const;
     
};

namespace idlepb {
class ReceiveBreathingExerciseRewardAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBreathingExerciseRewardAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbReceiveBreathingExerciseRewardAck();
    FPbReceiveBreathingExerciseRewardAck(const idlepb::ReceiveBreathingExerciseRewardAck& Right);
    void FromPb(const idlepb::ReceiveBreathingExerciseRewardAck& Right);
    void ToPb(idlepb::ReceiveBreathingExerciseRewardAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBreathingExerciseRewardAck& Right);
    bool operator==(const FPbReceiveBreathingExerciseRewardAck& Right) const;
    bool operator!=(const FPbReceiveBreathingExerciseRewardAck& Right) const;
     
};


/**
 * 道具类型
*/
UENUM(BlueprintType)
enum class EPbUseItemResult : uint8
{
    UIR_Success = 0 UMETA(DisplayName="成功"),
    UIR_UnKnown = 1 UMETA(DisplayName="未知"),
    UIR_BadParam = 2 UMETA(DisplayName="参数非法"),
    UIR_NotEnoughNum = 3 UMETA(DisplayName="数量不足"),
    UIR_InventoryIsFull = 4 UMETA(DisplayName="背包已满"),
    UIR_LowRank = 5 UMETA(DisplayName="境界不足"),
    UIR_BadDir = 6 UMETA(DisplayName="修炼方向不对"),
    UIR_BadConfig = 7 UMETA(DisplayName="配置出错"),
    UIR_UseNumIsFull = 8 UMETA(DisplayName="达到上限"),
    UIR_BadTime = 9 UMETA(DisplayName="时机不对"),
    UIR_BadData = 10 UMETA(DisplayName="内存错误"),
    UIR_BadType = 11 UMETA(DisplayName="类型不对"),
};
constexpr EPbUseItemResult EPbUseItemResult_Min = EPbUseItemResult::UIR_Success;
constexpr EPbUseItemResult EPbUseItemResult_Max = EPbUseItemResult::UIR_BadType;
constexpr int32 EPbUseItemResult_ArraySize = static_cast<int32>(EPbUseItemResult_Max) + 1;
MPROTOCOL_API bool CheckEPbUseItemResultValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbUseItemResultDescription(EPbUseItemResult Val);

template <typename Char>
struct fmt::formatter<EPbUseItemResult, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbUseItemResult& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class UseItemReq;
}  // namespace idlepb

/**
 * 使用道具 (参数 id 和 cfg_id 二选一)
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseItemReq
{
    GENERATED_BODY();

    /** 通过道具唯一ID，使用指定道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;

    /** 通过道具配置ID，使用一类道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 使用数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbUseItemReq();
    FPbUseItemReq(const idlepb::UseItemReq& Right);
    void FromPb(const idlepb::UseItemReq& Right);
    void ToPb(idlepb::UseItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::UseItemReq& Right);
    bool operator==(const FPbUseItemReq& Right) const;
    bool operator!=(const FPbUseItemReq& Right) const;
     
};

namespace idlepb {
class UseItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbUseItemResult error_code;


    FPbUseItemAck();
    FPbUseItemAck(const idlepb::UseItemAck& Right);
    void FromPb(const idlepb::UseItemAck& Right);
    void ToPb(idlepb::UseItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::UseItemAck& Right);
    bool operator==(const FPbUseItemAck& Right) const;
    bool operator!=(const FPbUseItemAck& Right) const;
     
};

namespace idlepb {
class UseSelectGiftReq;
}  // namespace idlepb

/**
 * 使用自选宝箱
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseSelectGiftReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 uid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 choose_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbUseSelectGiftReq();
    FPbUseSelectGiftReq(const idlepb::UseSelectGiftReq& Right);
    void FromPb(const idlepb::UseSelectGiftReq& Right);
    void ToPb(idlepb::UseSelectGiftReq* Out) const;
    void Reset();
    void operator=(const idlepb::UseSelectGiftReq& Right);
    bool operator==(const FPbUseSelectGiftReq& Right) const;
    bool operator!=(const FPbUseSelectGiftReq& Right) const;
     
};

namespace idlepb {
class UseSelectGiftAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseSelectGiftAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbUseItemResult error_code;


    FPbUseSelectGiftAck();
    FPbUseSelectGiftAck(const idlepb::UseSelectGiftAck& Right);
    void FromPb(const idlepb::UseSelectGiftAck& Right);
    void ToPb(idlepb::UseSelectGiftAck* Out) const;
    void Reset();
    void operator=(const idlepb::UseSelectGiftAck& Right);
    bool operator==(const FPbUseSelectGiftAck& Right) const;
    bool operator!=(const FPbUseSelectGiftAck& Right) const;
     
};

namespace idlepb {
class SellItemInfo;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSellItemInfo
{
    GENERATED_BODY();

    /** 道具唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 item_id;

    /** 数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 用于返回售卖是否成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSellItemInfo();
    FPbSellItemInfo(const idlepb::SellItemInfo& Right);
    void FromPb(const idlepb::SellItemInfo& Right);
    void ToPb(idlepb::SellItemInfo* Out) const;
    void Reset();
    void operator=(const idlepb::SellItemInfo& Right);
    bool operator==(const FPbSellItemInfo& Right) const;
    bool operator!=(const FPbSellItemInfo& Right) const;
     
};

namespace idlepb {
class SellItemReq;
}  // namespace idlepb

/**
 * 出售道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSellItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSellItemInfo> items;


    FPbSellItemReq();
    FPbSellItemReq(const idlepb::SellItemReq& Right);
    void FromPb(const idlepb::SellItemReq& Right);
    void ToPb(idlepb::SellItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::SellItemReq& Right);
    bool operator==(const FPbSellItemReq& Right) const;
    bool operator!=(const FPbSellItemReq& Right) const;
     
};

namespace idlepb {
class SellItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSellItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSellItemInfo> items;


    FPbSellItemAck();
    FPbSellItemAck(const idlepb::SellItemAck& Right);
    void FromPb(const idlepb::SellItemAck& Right);
    void ToPb(idlepb::SellItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::SellItemAck& Right);
    bool operator==(const FPbSellItemAck& Right) const;
    bool operator!=(const FPbSellItemAck& Right) const;
     
};

namespace idlepb {
class RefreshAlchemyData;
}  // namespace idlepb

/**
 * 刷新炼丹数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshAlchemyData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAlchemyData data;


    FPbRefreshAlchemyData();
    FPbRefreshAlchemyData(const idlepb::RefreshAlchemyData& Right);
    void FromPb(const idlepb::RefreshAlchemyData& Right);
    void ToPb(idlepb::RefreshAlchemyData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshAlchemyData& Right);
    bool operator==(const FPbRefreshAlchemyData& Right) const;
    bool operator!=(const FPbRefreshAlchemyData& Right) const;
     
};

namespace idlepb {
class NotifyAlchemyRefineResult;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyAlchemyRefineResult
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 exp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAlchemyChanceType chance_type;


    FPbNotifyAlchemyRefineResult();
    FPbNotifyAlchemyRefineResult(const idlepb::NotifyAlchemyRefineResult& Right);
    void FromPb(const idlepb::NotifyAlchemyRefineResult& Right);
    void ToPb(idlepb::NotifyAlchemyRefineResult* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyAlchemyRefineResult& Right);
    bool operator==(const FPbNotifyAlchemyRefineResult& Right) const;
    bool operator!=(const FPbNotifyAlchemyRefineResult& Right) const;
     
};

namespace idlepb {
class RefreshForgeData;
}  // namespace idlepb

/**
 * 刷新炼器数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshForgeData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleForgeData data;


    FPbRefreshForgeData();
    FPbRefreshForgeData(const idlepb::RefreshForgeData& Right);
    void FromPb(const idlepb::RefreshForgeData& Right);
    void ToPb(idlepb::RefreshForgeData* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshForgeData& Right);
    bool operator==(const FPbRefreshForgeData& Right) const;
    bool operator!=(const FPbRefreshForgeData& Right) const;
     
};

namespace idlepb {
class NotifyForgeRefineResult;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyForgeRefineResult
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 exp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbForgeChanceType chance_type;


    FPbNotifyForgeRefineResult();
    FPbNotifyForgeRefineResult(const idlepb::NotifyForgeRefineResult& Right);
    void FromPb(const idlepb::NotifyForgeRefineResult& Right);
    void ToPb(idlepb::NotifyForgeRefineResult* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyForgeRefineResult& Right);
    bool operator==(const FPbNotifyForgeRefineResult& Right) const;
    bool operator!=(const FPbNotifyForgeRefineResult& Right) const;
     
};

namespace idlepb {
class EquipmentPutOnReq;
}  // namespace idlepb

/**
 * 穿装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipmentPutOnReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 slot_idx;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 item_id;


    FPbEquipmentPutOnReq();
    FPbEquipmentPutOnReq(const idlepb::EquipmentPutOnReq& Right);
    void FromPb(const idlepb::EquipmentPutOnReq& Right);
    void ToPb(idlepb::EquipmentPutOnReq* Out) const;
    void Reset();
    void operator=(const idlepb::EquipmentPutOnReq& Right);
    bool operator==(const FPbEquipmentPutOnReq& Right) const;
    bool operator!=(const FPbEquipmentPutOnReq& Right) const;
     
};

namespace idlepb {
class EquipmentPutOnAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipmentPutOnAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbEquipmentPutOnAck();
    FPbEquipmentPutOnAck(const idlepb::EquipmentPutOnAck& Right);
    void FromPb(const idlepb::EquipmentPutOnAck& Right);
    void ToPb(idlepb::EquipmentPutOnAck* Out) const;
    void Reset();
    void operator=(const idlepb::EquipmentPutOnAck& Right);
    bool operator==(const FPbEquipmentPutOnAck& Right) const;
    bool operator!=(const FPbEquipmentPutOnAck& Right) const;
     
};

namespace idlepb {
class EquipmentTakeOffReq;
}  // namespace idlepb

/**
 * 脱装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipmentTakeOffReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 slot_idx;


    FPbEquipmentTakeOffReq();
    FPbEquipmentTakeOffReq(const idlepb::EquipmentTakeOffReq& Right);
    void FromPb(const idlepb::EquipmentTakeOffReq& Right);
    void ToPb(idlepb::EquipmentTakeOffReq* Out) const;
    void Reset();
    void operator=(const idlepb::EquipmentTakeOffReq& Right);
    bool operator==(const FPbEquipmentTakeOffReq& Right) const;
    bool operator!=(const FPbEquipmentTakeOffReq& Right) const;
     
};

namespace idlepb {
class EquipmentTakeOffAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipmentTakeOffAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbEquipmentTakeOffAck();
    FPbEquipmentTakeOffAck(const idlepb::EquipmentTakeOffAck& Right);
    void FromPb(const idlepb::EquipmentTakeOffAck& Right);
    void ToPb(idlepb::EquipmentTakeOffAck* Out) const;
    void Reset();
    void operator=(const idlepb::EquipmentTakeOffAck& Right);
    bool operator==(const FPbEquipmentTakeOffAck& Right) const;
    bool operator!=(const FPbEquipmentTakeOffAck& Right) const;
     
};

namespace idlepb {
class GetInventoryDataReq;
}  // namespace idlepb

/**
 * 请求包裹
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetInventoryDataReq
{
    GENERATED_BODY();


    FPbGetInventoryDataReq();
    FPbGetInventoryDataReq(const idlepb::GetInventoryDataReq& Right);
    void FromPb(const idlepb::GetInventoryDataReq& Right);
    void ToPb(idlepb::GetInventoryDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetInventoryDataReq& Right);
    bool operator==(const FPbGetInventoryDataReq& Right) const;
    bool operator!=(const FPbGetInventoryDataReq& Right) const;
     
};

namespace idlepb {
class GetInventoryDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetInventoryDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> items;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlocked_equipment_slots;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 inventory_space_num;


    FPbGetInventoryDataAck();
    FPbGetInventoryDataAck(const idlepb::GetInventoryDataAck& Right);
    void FromPb(const idlepb::GetInventoryDataAck& Right);
    void ToPb(idlepb::GetInventoryDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetInventoryDataAck& Right);
    bool operator==(const FPbGetInventoryDataAck& Right) const;
    bool operator!=(const FPbGetInventoryDataAck& Right) const;
     
};

namespace idlepb {
class AlchemyRefineStartReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineStartReq
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 材料ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 material_id;

    /** 目标数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_num;


    FPbAlchemyRefineStartReq();
    FPbAlchemyRefineStartReq(const idlepb::AlchemyRefineStartReq& Right);
    void FromPb(const idlepb::AlchemyRefineStartReq& Right);
    void ToPb(idlepb::AlchemyRefineStartReq* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineStartReq& Right);
    bool operator==(const FPbAlchemyRefineStartReq& Right) const;
    bool operator!=(const FPbAlchemyRefineStartReq& Right) const;
     
};

namespace idlepb {
class AlchemyRefineStartAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineStartAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAlchemyRefineStartAck();
    FPbAlchemyRefineStartAck(const idlepb::AlchemyRefineStartAck& Right);
    void FromPb(const idlepb::AlchemyRefineStartAck& Right);
    void ToPb(idlepb::AlchemyRefineStartAck* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineStartAck& Right);
    bool operator==(const FPbAlchemyRefineStartAck& Right) const;
    bool operator!=(const FPbAlchemyRefineStartAck& Right) const;
     
};

namespace idlepb {
class AlchemyRefineCancelReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineCancelReq
{
    GENERATED_BODY();


    FPbAlchemyRefineCancelReq();
    FPbAlchemyRefineCancelReq(const idlepb::AlchemyRefineCancelReq& Right);
    void FromPb(const idlepb::AlchemyRefineCancelReq& Right);
    void ToPb(idlepb::AlchemyRefineCancelReq* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineCancelReq& Right);
    bool operator==(const FPbAlchemyRefineCancelReq& Right) const;
    bool operator!=(const FPbAlchemyRefineCancelReq& Right) const;
     
};

namespace idlepb {
class AlchemyRefineCancelAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineCancelAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAlchemyRefineCancelAck();
    FPbAlchemyRefineCancelAck(const idlepb::AlchemyRefineCancelAck& Right);
    void FromPb(const idlepb::AlchemyRefineCancelAck& Right);
    void ToPb(idlepb::AlchemyRefineCancelAck* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineCancelAck& Right);
    bool operator==(const FPbAlchemyRefineCancelAck& Right) const;
    bool operator!=(const FPbAlchemyRefineCancelAck& Right) const;
     
};

namespace idlepb {
class AlchemyRefineExtractReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineExtractReq
{
    GENERATED_BODY();


    FPbAlchemyRefineExtractReq();
    FPbAlchemyRefineExtractReq(const idlepb::AlchemyRefineExtractReq& Right);
    void FromPb(const idlepb::AlchemyRefineExtractReq& Right);
    void ToPb(idlepb::AlchemyRefineExtractReq* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineExtractReq& Right);
    bool operator==(const FPbAlchemyRefineExtractReq& Right) const;
    bool operator!=(const FPbAlchemyRefineExtractReq& Right) const;
     
};

namespace idlepb {
class AlchemyRefineExtractAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRefineExtractAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAlchemyRefineExtractAck();
    FPbAlchemyRefineExtractAck(const idlepb::AlchemyRefineExtractAck& Right);
    void FromPb(const idlepb::AlchemyRefineExtractAck& Right);
    void ToPb(idlepb::AlchemyRefineExtractAck* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRefineExtractAck& Right);
    bool operator==(const FPbAlchemyRefineExtractAck& Right) const;
    bool operator!=(const FPbAlchemyRefineExtractAck& Right) const;
     
};

namespace idlepb {
class CreateCharacterReq;
}  // namespace idlepb

/**
 * 创建角色
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCreateCharacterReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString hero_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 skeleton_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> model_data;


    FPbCreateCharacterReq();
    FPbCreateCharacterReq(const idlepb::CreateCharacterReq& Right);
    void FromPb(const idlepb::CreateCharacterReq& Right);
    void ToPb(idlepb::CreateCharacterReq* Out) const;
    void Reset();
    void operator=(const idlepb::CreateCharacterReq& Right);
    bool operator==(const FPbCreateCharacterReq& Right) const;
    bool operator!=(const FPbCreateCharacterReq& Right) const;
     
};

namespace idlepb {
class CreateCharacterAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCreateCharacterAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbCreateCharacterAck();
    FPbCreateCharacterAck(const idlepb::CreateCharacterAck& Right);
    void FromPb(const idlepb::CreateCharacterAck& Right);
    void ToPb(idlepb::CreateCharacterAck* Out) const;
    void Reset();
    void operator=(const idlepb::CreateCharacterAck& Right);
    bool operator==(const FPbCreateCharacterAck& Right) const;
    bool operator!=(const FPbCreateCharacterAck& Right) const;
     
};

namespace idlepb {
class SystemNotice;
}  // namespace idlepb

/**
 * 系统提示信息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSystemNotice
{
    GENERATED_BODY();

    /** 系统信息显示样式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 style;

    /** 文本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString text;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString s1;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString s2;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString s3;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n1;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n2;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n3;


    FPbSystemNotice();
    FPbSystemNotice(const idlepb::SystemNotice& Right);
    void FromPb(const idlepb::SystemNotice& Right);
    void ToPb(idlepb::SystemNotice* Out) const;
    void Reset();
    void operator=(const idlepb::SystemNotice& Right);
    bool operator==(const FPbSystemNotice& Right) const;
    bool operator!=(const FPbSystemNotice& Right) const;
     
};

namespace idlepb {
class GetRoleShopDataReq;
}  // namespace idlepb

/**
 * 坊市 - 请求数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleShopDataReq
{
    GENERATED_BODY();


    FPbGetRoleShopDataReq();
    FPbGetRoleShopDataReq(const idlepb::GetRoleShopDataReq& Right);
    void FromPb(const idlepb::GetRoleShopDataReq& Right);
    void ToPb(idlepb::GetRoleShopDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleShopDataReq& Right);
    bool operator==(const FPbGetRoleShopDataReq& Right) const;
    bool operator!=(const FPbGetRoleShopDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleShopDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleShopDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleShopData data;


    FPbGetRoleShopDataAck();
    FPbGetRoleShopDataAck(const idlepb::GetRoleShopDataAck& Right);
    void FromPb(const idlepb::GetRoleShopDataAck& Right);
    void ToPb(idlepb::GetRoleShopDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleShopDataAck& Right);
    bool operator==(const FPbGetRoleShopDataAck& Right) const;
    bool operator!=(const FPbGetRoleShopDataAck& Right) const;
     
};

namespace idlepb {
class RefreshShopReq;
}  // namespace idlepb

/**
 * 坊市 - 手动进货
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshShopReq
{
    GENERATED_BODY();


    FPbRefreshShopReq();
    FPbRefreshShopReq(const idlepb::RefreshShopReq& Right);
    void FromPb(const idlepb::RefreshShopReq& Right);
    void ToPb(idlepb::RefreshShopReq* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshShopReq& Right);
    bool operator==(const FPbRefreshShopReq& Right) const;
    bool operator!=(const FPbRefreshShopReq& Right) const;
     
};

namespace idlepb {
class RefreshShopAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshShopAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleShopData data;


    FPbRefreshShopAck();
    FPbRefreshShopAck(const idlepb::RefreshShopAck& Right);
    void FromPb(const idlepb::RefreshShopAck& Right);
    void ToPb(idlepb::RefreshShopAck* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshShopAck& Right);
    bool operator==(const FPbRefreshShopAck& Right) const;
    bool operator!=(const FPbRefreshShopAck& Right) const;
     
};

namespace idlepb {
class BuyShopItemReq;
}  // namespace idlepb

/**
 * 坊市 - 购买
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuyShopItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbBuyShopItemReq();
    FPbBuyShopItemReq(const idlepb::BuyShopItemReq& Right);
    void FromPb(const idlepb::BuyShopItemReq& Right);
    void ToPb(idlepb::BuyShopItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::BuyShopItemReq& Right);
    bool operator==(const FPbBuyShopItemReq& Right) const;
    bool operator!=(const FPbBuyShopItemReq& Right) const;
     
};

namespace idlepb {
class BuyShopItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuyShopItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbBuyShopItemAck();
    FPbBuyShopItemAck(const idlepb::BuyShopItemAck& Right);
    void FromPb(const idlepb::BuyShopItemAck& Right);
    void ToPb(idlepb::BuyShopItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::BuyShopItemAck& Right);
    bool operator==(const FPbBuyShopItemAck& Right) const;
    bool operator!=(const FPbBuyShopItemAck& Right) const;
     
};

namespace idlepb {
class GetRoleDeluxeShopDataReq;
}  // namespace idlepb

/**
 * 天机阁 - 请求数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleDeluxeShopDataReq
{
    GENERATED_BODY();


    FPbGetRoleDeluxeShopDataReq();
    FPbGetRoleDeluxeShopDataReq(const idlepb::GetRoleDeluxeShopDataReq& Right);
    void FromPb(const idlepb::GetRoleDeluxeShopDataReq& Right);
    void ToPb(idlepb::GetRoleDeluxeShopDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleDeluxeShopDataReq& Right);
    bool operator==(const FPbGetRoleDeluxeShopDataReq& Right) const;
    bool operator!=(const FPbGetRoleDeluxeShopDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleDeluxeShopDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleDeluxeShopDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDeluxeShopData data;


    FPbGetRoleDeluxeShopDataAck();
    FPbGetRoleDeluxeShopDataAck(const idlepb::GetRoleDeluxeShopDataAck& Right);
    void FromPb(const idlepb::GetRoleDeluxeShopDataAck& Right);
    void ToPb(idlepb::GetRoleDeluxeShopDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleDeluxeShopDataAck& Right);
    bool operator==(const FPbGetRoleDeluxeShopDataAck& Right) const;
    bool operator!=(const FPbGetRoleDeluxeShopDataAck& Right) const;
     
};

namespace idlepb {
class RefreshDeluxeShopReq;
}  // namespace idlepb

/**
 * 天机阁 - 手动进货
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshDeluxeShopReq
{
    GENERATED_BODY();


    FPbRefreshDeluxeShopReq();
    FPbRefreshDeluxeShopReq(const idlepb::RefreshDeluxeShopReq& Right);
    void FromPb(const idlepb::RefreshDeluxeShopReq& Right);
    void ToPb(idlepb::RefreshDeluxeShopReq* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshDeluxeShopReq& Right);
    bool operator==(const FPbRefreshDeluxeShopReq& Right) const;
    bool operator!=(const FPbRefreshDeluxeShopReq& Right) const;
     
};

namespace idlepb {
class RefreshDeluxeShopAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshDeluxeShopAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDeluxeShopData data;


    FPbRefreshDeluxeShopAck();
    FPbRefreshDeluxeShopAck(const idlepb::RefreshDeluxeShopAck& Right);
    void FromPb(const idlepb::RefreshDeluxeShopAck& Right);
    void ToPb(idlepb::RefreshDeluxeShopAck* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshDeluxeShopAck& Right);
    bool operator==(const FPbRefreshDeluxeShopAck& Right) const;
    bool operator!=(const FPbRefreshDeluxeShopAck& Right) const;
     
};

namespace idlepb {
class BuyDeluxeShopItemReq;
}  // namespace idlepb

/**
 * 天机阁 - 购买
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuyDeluxeShopItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbBuyDeluxeShopItemReq();
    FPbBuyDeluxeShopItemReq(const idlepb::BuyDeluxeShopItemReq& Right);
    void FromPb(const idlepb::BuyDeluxeShopItemReq& Right);
    void ToPb(idlepb::BuyDeluxeShopItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::BuyDeluxeShopItemReq& Right);
    bool operator==(const FPbBuyDeluxeShopItemReq& Right) const;
    bool operator!=(const FPbBuyDeluxeShopItemReq& Right) const;
     
};

namespace idlepb {
class BuyDeluxeShopItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuyDeluxeShopItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbBuyDeluxeShopItemAck();
    FPbBuyDeluxeShopItemAck(const idlepb::BuyDeluxeShopItemAck& Right);
    void FromPb(const idlepb::BuyDeluxeShopItemAck& Right);
    void ToPb(idlepb::BuyDeluxeShopItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::BuyDeluxeShopItemAck& Right);
    bool operator==(const FPbBuyDeluxeShopItemAck& Right) const;
    bool operator!=(const FPbBuyDeluxeShopItemAck& Right) const;
     
};

namespace idlepb {
class UnlockDeluxeShopReq;
}  // namespace idlepb

/**
 * 天机阁 - 解锁
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockDeluxeShopReq
{
    GENERATED_BODY();


    FPbUnlockDeluxeShopReq();
    FPbUnlockDeluxeShopReq(const idlepb::UnlockDeluxeShopReq& Right);
    void FromPb(const idlepb::UnlockDeluxeShopReq& Right);
    void ToPb(idlepb::UnlockDeluxeShopReq* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockDeluxeShopReq& Right);
    bool operator==(const FPbUnlockDeluxeShopReq& Right) const;
    bool operator!=(const FPbUnlockDeluxeShopReq& Right) const;
     
};

namespace idlepb {
class UnlockDeluxeShopAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockDeluxeShopAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDeluxeShopData data;


    FPbUnlockDeluxeShopAck();
    FPbUnlockDeluxeShopAck(const idlepb::UnlockDeluxeShopAck& Right);
    void FromPb(const idlepb::UnlockDeluxeShopAck& Right);
    void ToPb(idlepb::UnlockDeluxeShopAck* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockDeluxeShopAck& Right);
    bool operator==(const FPbUnlockDeluxeShopAck& Right) const;
    bool operator!=(const FPbUnlockDeluxeShopAck& Right) const;
     
};

namespace idlepb {
class RefreshDeluxeShopUnlocked;
}  // namespace idlepb

/**
 * 通知天机阁解锁状态
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshDeluxeShopUnlocked
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_unlocked;


    FPbRefreshDeluxeShopUnlocked();
    FPbRefreshDeluxeShopUnlocked(const idlepb::RefreshDeluxeShopUnlocked& Right);
    void FromPb(const idlepb::RefreshDeluxeShopUnlocked& Right);
    void ToPb(idlepb::RefreshDeluxeShopUnlocked* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshDeluxeShopUnlocked& Right);
    bool operator==(const FPbRefreshDeluxeShopUnlocked& Right) const;
    bool operator!=(const FPbRefreshDeluxeShopUnlocked& Right) const;
     
};

namespace idlepb {
class UnlockArenaReq;
}  // namespace idlepb

/**
 * 尝试解锁指定秘境
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockArenaReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 arena_id;


    FPbUnlockArenaReq();
    FPbUnlockArenaReq(const idlepb::UnlockArenaReq& Right);
    void FromPb(const idlepb::UnlockArenaReq& Right);
    void ToPb(idlepb::UnlockArenaReq* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockArenaReq& Right);
    bool operator==(const FPbUnlockArenaReq& Right) const;
    bool operator!=(const FPbUnlockArenaReq& Right) const;
     
};

namespace idlepb {
class UnlockArenaAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockArenaAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUnlockArenaAck();
    FPbUnlockArenaAck(const idlepb::UnlockArenaAck& Right);
    void FromPb(const idlepb::UnlockArenaAck& Right);
    void ToPb(idlepb::UnlockArenaAck* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockArenaAck& Right);
    bool operator==(const FPbUnlockArenaAck& Right) const;
    bool operator!=(const FPbUnlockArenaAck& Right) const;
     
};

namespace idlepb {
class NotifyUnlockArenaChallengeResult;
}  // namespace idlepb

/**
 * 通知解锁挑战结果
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyUnlockArenaChallengeResult
{
    GENERATED_BODY();

    /** 解锁目标秘境CfgId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 arena_id;

    /** 传送门场景CfgId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 door_id;

    /** 是否成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbNotifyUnlockArenaChallengeResult();
    FPbNotifyUnlockArenaChallengeResult(const idlepb::NotifyUnlockArenaChallengeResult& Right);
    void FromPb(const idlepb::NotifyUnlockArenaChallengeResult& Right);
    void ToPb(idlepb::NotifyUnlockArenaChallengeResult* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyUnlockArenaChallengeResult& Right);
    bool operator==(const FPbNotifyUnlockArenaChallengeResult& Right) const;
    bool operator!=(const FPbNotifyUnlockArenaChallengeResult& Right) const;
     
};

namespace idlepb {
class RequestRefreshRoleCombatPower;
}  // namespace idlepb

/**
 * 请求刷新角色战力
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestRefreshRoleCombatPower
{
    GENERATED_BODY();


    FPbRequestRefreshRoleCombatPower();
    FPbRequestRefreshRoleCombatPower(const idlepb::RequestRefreshRoleCombatPower& Right);
    void FromPb(const idlepb::RequestRefreshRoleCombatPower& Right);
    void ToPb(idlepb::RequestRefreshRoleCombatPower* Out) const;
    void Reset();
    void operator=(const idlepb::RequestRefreshRoleCombatPower& Right);
    bool operator==(const FPbRequestRefreshRoleCombatPower& Right) const;
    bool operator!=(const FPbRequestRefreshRoleCombatPower& Right) const;
     
};

namespace idlepb {
class NotifyRoleCombatPower;
}  // namespace idlepb

/**
 * 刷新角色战力
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyRoleCombatPower
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 old_value;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 new_value;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool show_notice;


    FPbNotifyRoleCombatPower();
    FPbNotifyRoleCombatPower(const idlepb::NotifyRoleCombatPower& Right);
    void FromPb(const idlepb::NotifyRoleCombatPower& Right);
    void ToPb(idlepb::NotifyRoleCombatPower* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyRoleCombatPower& Right);
    bool operator==(const FPbNotifyRoleCombatPower& Right) const;
    bool operator!=(const FPbNotifyRoleCombatPower& Right) const;
     
};

namespace idlepb {
class GameSystemChatMessage;
}  // namespace idlepb

/**
 * 系统聊天/战斗日志
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameSystemChatMessage
{
    GENERATED_BODY();

    /** 类型(目前只有战斗日志) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 chat_type;

    /** 聊天内容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<uint8> chat_content;


    FPbGameSystemChatMessage();
    FPbGameSystemChatMessage(const idlepb::GameSystemChatMessage& Right);
    void FromPb(const idlepb::GameSystemChatMessage& Right);
    void ToPb(idlepb::GameSystemChatMessage* Out) const;
    void Reset();
    void operator=(const idlepb::GameSystemChatMessage& Right);
    bool operator==(const FPbGameSystemChatMessage& Right) const;
    bool operator!=(const FPbGameSystemChatMessage& Right) const;
     
};

namespace idlepb {
class ReplicateQuestProgressChange;
}  // namespace idlepb

/**
 * 任务进度同步
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReplicateQuestProgressChange
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quest_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbQuestRequirementType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 amount;


    FPbReplicateQuestProgressChange();
    FPbReplicateQuestProgressChange(const idlepb::ReplicateQuestProgressChange& Right);
    void FromPb(const idlepb::ReplicateQuestProgressChange& Right);
    void ToPb(idlepb::ReplicateQuestProgressChange* Out) const;
    void Reset();
    void operator=(const idlepb::ReplicateQuestProgressChange& Right);
    bool operator==(const FPbReplicateQuestProgressChange& Right) const;
    bool operator!=(const FPbReplicateQuestProgressChange& Right) const;
     
};

namespace idlepb {
class QuestOpReq;
}  // namespace idlepb

/**
 * 请求任务操作
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQuestOpReq
{
    GENERATED_BODY();

    /** 操作类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbQuestOpType operation;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quest_id;


    FPbQuestOpReq();
    FPbQuestOpReq(const idlepb::QuestOpReq& Right);
    void FromPb(const idlepb::QuestOpReq& Right);
    void ToPb(idlepb::QuestOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::QuestOpReq& Right);
    bool operator==(const FPbQuestOpReq& Right) const;
    bool operator!=(const FPbQuestOpReq& Right) const;
     
};

namespace idlepb {
class QuestOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQuestOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 接受任务后初始化任务进度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbReplicateQuestProgressChange> init_progress;


    FPbQuestOpAck();
    FPbQuestOpAck(const idlepb::QuestOpAck& Right);
    void FromPb(const idlepb::QuestOpAck& Right);
    void ToPb(idlepb::QuestOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::QuestOpAck& Right);
    bool operator==(const FPbQuestOpAck& Right) const;
    bool operator!=(const FPbQuestOpAck& Right) const;
     
};

namespace idlepb {
class GetQuestDataReq;
}  // namespace idlepb

/**
 * 请求任务数据 -- 客户端任务存档
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetQuestDataReq
{
    GENERATED_BODY();


    FPbGetQuestDataReq();
    FPbGetQuestDataReq(const idlepb::GetQuestDataReq& Right);
    void FromPb(const idlepb::GetQuestDataReq& Right);
    void ToPb(idlepb::GetQuestDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetQuestDataReq& Right);
    bool operator==(const FPbGetQuestDataReq& Right) const;
    bool operator!=(const FPbGetQuestDataReq& Right) const;
     
};

namespace idlepb {
class GetQuestDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetQuestDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleQuestData data;


    FPbGetQuestDataAck();
    FPbGetQuestDataAck(const idlepb::GetQuestDataAck& Right);
    void FromPb(const idlepb::GetQuestDataAck& Right);
    void ToPb(idlepb::GetQuestDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetQuestDataAck& Right);
    bool operator==(const FPbGetQuestDataAck& Right) const;
    bool operator!=(const FPbGetQuestDataAck& Right) const;
     
};

namespace idlepb {
class GetRoleLeaderboardDataReq;
}  // namespace idlepb

/**
 * 排行榜 - 请求玩家排行榜个人信息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleLeaderboardDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;


    FPbGetRoleLeaderboardDataReq();
    FPbGetRoleLeaderboardDataReq(const idlepb::GetRoleLeaderboardDataReq& Right);
    void FromPb(const idlepb::GetRoleLeaderboardDataReq& Right);
    void ToPb(idlepb::GetRoleLeaderboardDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleLeaderboardDataReq& Right);
    bool operator==(const FPbGetRoleLeaderboardDataReq& Right) const;
    bool operator!=(const FPbGetRoleLeaderboardDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleLeaderboardDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleLeaderboardDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLeaderboardData data;

    /** 修为榜福泽排名修为均值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 param_n1;

    /** 各个榜单上的排名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> leaderboard_rank;


    FPbGetRoleLeaderboardDataAck();
    FPbGetRoleLeaderboardDataAck(const idlepb::GetRoleLeaderboardDataAck& Right);
    void FromPb(const idlepb::GetRoleLeaderboardDataAck& Right);
    void ToPb(idlepb::GetRoleLeaderboardDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleLeaderboardDataAck& Right);
    bool operator==(const FPbGetRoleLeaderboardDataAck& Right) const;
    bool operator!=(const FPbGetRoleLeaderboardDataAck& Right) const;
     
};

namespace idlepb {
class GetLeaderboardPreviewReq;
}  // namespace idlepb

/**
 * 请求排行榜预览，每个榜的榜一数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLeaderboardPreviewReq
{
    GENERATED_BODY();


    FPbGetLeaderboardPreviewReq();
    FPbGetLeaderboardPreviewReq(const idlepb::GetLeaderboardPreviewReq& Right);
    void FromPb(const idlepb::GetLeaderboardPreviewReq& Right);
    void ToPb(idlepb::GetLeaderboardPreviewReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetLeaderboardPreviewReq& Right);
    bool operator==(const FPbGetLeaderboardPreviewReq& Right) const;
    bool operator!=(const FPbGetLeaderboardPreviewReq& Right) const;
     
};

namespace idlepb {
class GetLeaderboardPreviewAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLeaderboardPreviewAck
{
    GENERATED_BODY();

    /** 角色属性榜第一 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardListItem> champions;

    /** 角色头像，如果需要 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCharacterModelConfig> role_model_configs;

    /** 玩家自己的配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLeaderboardData my_data;

    /** 装备榜，需要预览数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> equipments;

    /** 宗门榜第一 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSeptDataOnLeaderboard sept;

    /** 上次榜单刷新时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_refresh_time;


    FPbGetLeaderboardPreviewAck();
    FPbGetLeaderboardPreviewAck(const idlepb::GetLeaderboardPreviewAck& Right);
    void FromPb(const idlepb::GetLeaderboardPreviewAck& Right);
    void ToPb(idlepb::GetLeaderboardPreviewAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetLeaderboardPreviewAck& Right);
    bool operator==(const FPbGetLeaderboardPreviewAck& Right) const;
    bool operator!=(const FPbGetLeaderboardPreviewAck& Right) const;
     
};

namespace idlepb {
class GetLeaderboardDataReq;
}  // namespace idlepb

/**
 * 请求排行榜数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLeaderboardDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbLeaderboardType type;


    FPbGetLeaderboardDataReq();
    FPbGetLeaderboardDataReq(const idlepb::GetLeaderboardDataReq& Right);
    void FromPb(const idlepb::GetLeaderboardDataReq& Right);
    void ToPb(idlepb::GetLeaderboardDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetLeaderboardDataReq& Right);
    bool operator==(const FPbGetLeaderboardDataReq& Right) const;
    bool operator!=(const FPbGetLeaderboardDataReq& Right) const;
     
};

namespace idlepb {
class GetLeaderboardDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLeaderboardDataAck
{
    GENERATED_BODY();

    /** 上次榜单刷新时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_refresh_time;

    /** 前100名，如果玩家自己未进榜，则发送101个 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardListItem> data;

    /** 玩家自己的排名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 my_rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString rank1_message;

    /** 榜单玩家模型配置，头像 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCharacterModelConfig> role_model_configs;

    /** 对应装备榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> equipments;

    /** 山河图记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShanhetuRecord> shanghetu_records;

    /** 排行榜前三名点赞数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> top3_clicklike_num;

    /** 对应宗门榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSeptDataOnLeaderboard> septs;


    FPbGetLeaderboardDataAck();
    FPbGetLeaderboardDataAck(const idlepb::GetLeaderboardDataAck& Right);
    void FromPb(const idlepb::GetLeaderboardDataAck& Right);
    void ToPb(idlepb::GetLeaderboardDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetLeaderboardDataAck& Right);
    bool operator==(const FPbGetLeaderboardDataAck& Right) const;
    bool operator!=(const FPbGetLeaderboardDataAck& Right) const;
     
};

namespace idlepb {
class LeaderboardClickLikeReq;
}  // namespace idlepb

/**
 * 请求排行榜点赞
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardClickLikeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbLeaderboardType type;


    FPbLeaderboardClickLikeReq();
    FPbLeaderboardClickLikeReq(const idlepb::LeaderboardClickLikeReq& Right);
    void FromPb(const idlepb::LeaderboardClickLikeReq& Right);
    void ToPb(idlepb::LeaderboardClickLikeReq* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardClickLikeReq& Right);
    bool operator==(const FPbLeaderboardClickLikeReq& Right) const;
    bool operator!=(const FPbLeaderboardClickLikeReq& Right) const;
     
};

namespace idlepb {
class LeaderboardClickLikeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardClickLikeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbLeaderboardClickLikeAck();
    FPbLeaderboardClickLikeAck(const idlepb::LeaderboardClickLikeAck& Right);
    void FromPb(const idlepb::LeaderboardClickLikeAck& Right);
    void ToPb(idlepb::LeaderboardClickLikeAck* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardClickLikeAck& Right);
    bool operator==(const FPbLeaderboardClickLikeAck& Right) const;
    bool operator!=(const FPbLeaderboardClickLikeAck& Right) const;
     
};

namespace idlepb {
class LeaderboardUpdateMessageReq;
}  // namespace idlepb

/**
 * 请求更新排行榜留言
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardUpdateMessageReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString new_message;


    FPbLeaderboardUpdateMessageReq();
    FPbLeaderboardUpdateMessageReq(const idlepb::LeaderboardUpdateMessageReq& Right);
    void FromPb(const idlepb::LeaderboardUpdateMessageReq& Right);
    void ToPb(idlepb::LeaderboardUpdateMessageReq* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardUpdateMessageReq& Right);
    bool operator==(const FPbLeaderboardUpdateMessageReq& Right) const;
    bool operator!=(const FPbLeaderboardUpdateMessageReq& Right) const;
     
};

namespace idlepb {
class LeaderboardUpdateMessageAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardUpdateMessageAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbLeaderboardUpdateMessageAck();
    FPbLeaderboardUpdateMessageAck(const idlepb::LeaderboardUpdateMessageAck& Right);
    void FromPb(const idlepb::LeaderboardUpdateMessageAck& Right);
    void ToPb(idlepb::LeaderboardUpdateMessageAck* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardUpdateMessageAck& Right);
    bool operator==(const FPbLeaderboardUpdateMessageAck& Right) const;
    bool operator!=(const FPbLeaderboardUpdateMessageAck& Right) const;
     
};

namespace idlepb {
class GetMonsterTowerChallengeListReq;
}  // namespace idlepb

/**
 * 请求镇妖塔挑战榜数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMonsterTowerChallengeListReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 challenge_id;


    FPbGetMonsterTowerChallengeListReq();
    FPbGetMonsterTowerChallengeListReq(const idlepb::GetMonsterTowerChallengeListReq& Right);
    void FromPb(const idlepb::GetMonsterTowerChallengeListReq& Right);
    void ToPb(idlepb::GetMonsterTowerChallengeListReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetMonsterTowerChallengeListReq& Right);
    bool operator==(const FPbGetMonsterTowerChallengeListReq& Right) const;
    bool operator!=(const FPbGetMonsterTowerChallengeListReq& Right) const;
     
};

namespace idlepb {
class GetMonsterTowerChallengeListAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMonsterTowerChallengeListAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardListItem> data;

    /** 榜单玩家模型配置，头像 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCharacterModelConfig> model_configs;

    /** 所有榜单的完成进度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> all_list_progress;


    FPbGetMonsterTowerChallengeListAck();
    FPbGetMonsterTowerChallengeListAck(const idlepb::GetMonsterTowerChallengeListAck& Right);
    void FromPb(const idlepb::GetMonsterTowerChallengeListAck& Right);
    void ToPb(idlepb::GetMonsterTowerChallengeListAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetMonsterTowerChallengeListAck& Right);
    bool operator==(const FPbGetMonsterTowerChallengeListAck& Right) const;
    bool operator!=(const FPbGetMonsterTowerChallengeListAck& Right) const;
     
};

namespace idlepb {
class GetMonsterTowerChallengeRewardReq;
}  // namespace idlepb

/**
 * 请求镇妖塔挑战榜奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMonsterTowerChallengeRewardReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 challenge_id;


    FPbGetMonsterTowerChallengeRewardReq();
    FPbGetMonsterTowerChallengeRewardReq(const idlepb::GetMonsterTowerChallengeRewardReq& Right);
    void FromPb(const idlepb::GetMonsterTowerChallengeRewardReq& Right);
    void ToPb(idlepb::GetMonsterTowerChallengeRewardReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetMonsterTowerChallengeRewardReq& Right);
    bool operator==(const FPbGetMonsterTowerChallengeRewardReq& Right) const;
    bool operator!=(const FPbGetMonsterTowerChallengeRewardReq& Right) const;
     
};

namespace idlepb {
class GetMonsterTowerChallengeRewardAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMonsterTowerChallengeRewardAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbGetMonsterTowerChallengeRewardAck();
    FPbGetMonsterTowerChallengeRewardAck(const idlepb::GetMonsterTowerChallengeRewardAck& Right);
    void FromPb(const idlepb::GetMonsterTowerChallengeRewardAck& Right);
    void ToPb(idlepb::GetMonsterTowerChallengeRewardAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetMonsterTowerChallengeRewardAck& Right);
    bool operator==(const FPbGetMonsterTowerChallengeRewardAck& Right) const;
    bool operator!=(const FPbGetMonsterTowerChallengeRewardAck& Right) const;
     
};

namespace idlepb {
class GetFuZeRewardReq;
}  // namespace idlepb

/**
 * 请求福泽奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetFuZeRewardReq
{
    GENERATED_BODY();


    FPbGetFuZeRewardReq();
    FPbGetFuZeRewardReq(const idlepb::GetFuZeRewardReq& Right);
    void FromPb(const idlepb::GetFuZeRewardReq& Right);
    void ToPb(idlepb::GetFuZeRewardReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetFuZeRewardReq& Right);
    bool operator==(const FPbGetFuZeRewardReq& Right) const;
    bool operator!=(const FPbGetFuZeRewardReq& Right) const;
     
};

namespace idlepb {
class GetFuZeRewardAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetFuZeRewardAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbGetFuZeRewardAck();
    FPbGetFuZeRewardAck(const idlepb::GetFuZeRewardAck& Right);
    void FromPb(const idlepb::GetFuZeRewardAck& Right);
    void ToPb(idlepb::GetFuZeRewardAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetFuZeRewardAck& Right);
    bool operator==(const FPbGetFuZeRewardAck& Right) const;
    bool operator!=(const FPbGetFuZeRewardAck& Right) const;
     
};

namespace idlepb {
class GetRoleMailDataReq;
}  // namespace idlepb

/**
 * 请求邮箱数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleMailDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool only_num;


    FPbGetRoleMailDataReq();
    FPbGetRoleMailDataReq(const idlepb::GetRoleMailDataReq& Right);
    void FromPb(const idlepb::GetRoleMailDataReq& Right);
    void ToPb(idlepb::GetRoleMailDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleMailDataReq& Right);
    bool operator==(const FPbGetRoleMailDataReq& Right) const;
    bool operator!=(const FPbGetRoleMailDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleMailDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleMailDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 unread_mail_num;

    /** 邮件预览，不包含体积大的道具和装备数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMail> mail_box;


    FPbGetRoleMailDataAck();
    FPbGetRoleMailDataAck(const idlepb::GetRoleMailDataAck& Right);
    void FromPb(const idlepb::GetRoleMailDataAck& Right);
    void ToPb(idlepb::GetRoleMailDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleMailDataAck& Right);
    bool operator==(const FPbGetRoleMailDataAck& Right) const;
    bool operator!=(const FPbGetRoleMailDataAck& Right) const;
     
};

namespace idlepb {
class UpdateRoleMail;
}  // namespace idlepb

/**
 * 邮箱数据更新
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateRoleMail
{
    GENERATED_BODY();


    FPbUpdateRoleMail();
    FPbUpdateRoleMail(const idlepb::UpdateRoleMail& Right);
    void FromPb(const idlepb::UpdateRoleMail& Right);
    void ToPb(idlepb::UpdateRoleMail* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateRoleMail& Right);
    bool operator==(const FPbUpdateRoleMail& Right) const;
    bool operator!=(const FPbUpdateRoleMail& Right) const;
     
};

namespace idlepb {
class ReadMailReq;
}  // namespace idlepb

/**
 * 请求邮件已读，同时发送邮件真实内容
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReadMailReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbReadMailReq();
    FPbReadMailReq(const idlepb::ReadMailReq& Right);
    void FromPb(const idlepb::ReadMailReq& Right);
    void ToPb(idlepb::ReadMailReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReadMailReq& Right);
    bool operator==(const FPbReadMailReq& Right) const;
    bool operator!=(const FPbReadMailReq& Right) const;
     
};

namespace idlepb {
class ReadMailAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReadMailAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 包含了装备和道具数据的邮件全部内容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbMail mail;


    FPbReadMailAck();
    FPbReadMailAck(const idlepb::ReadMailAck& Right);
    void FromPb(const idlepb::ReadMailAck& Right);
    void ToPb(idlepb::ReadMailAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReadMailAck& Right);
    bool operator==(const FPbReadMailAck& Right) const;
    bool operator!=(const FPbReadMailAck& Right) const;
     
};

namespace idlepb {
class GetMailAttachmentReq;
}  // namespace idlepb

/**
 * 请求邮件领取
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMailAttachmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbGetMailAttachmentReq();
    FPbGetMailAttachmentReq(const idlepb::GetMailAttachmentReq& Right);
    void FromPb(const idlepb::GetMailAttachmentReq& Right);
    void ToPb(idlepb::GetMailAttachmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetMailAttachmentReq& Right);
    bool operator==(const FPbGetMailAttachmentReq& Right) const;
    bool operator!=(const FPbGetMailAttachmentReq& Right) const;
     
};

namespace idlepb {
class GetMailAttachmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetMailAttachmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbMailOperation result;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbMail mail_data;


    FPbGetMailAttachmentAck();
    FPbGetMailAttachmentAck(const idlepb::GetMailAttachmentAck& Right);
    void FromPb(const idlepb::GetMailAttachmentAck& Right);
    void ToPb(idlepb::GetMailAttachmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetMailAttachmentAck& Right);
    bool operator==(const FPbGetMailAttachmentAck& Right) const;
    bool operator!=(const FPbGetMailAttachmentAck& Right) const;
     
};

namespace idlepb {
class DeleteMailReq;
}  // namespace idlepb

/**
 * 请求邮件删除
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDeleteMailReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbDeleteMailReq();
    FPbDeleteMailReq(const idlepb::DeleteMailReq& Right);
    void FromPb(const idlepb::DeleteMailReq& Right);
    void ToPb(idlepb::DeleteMailReq* Out) const;
    void Reset();
    void operator=(const idlepb::DeleteMailReq& Right);
    bool operator==(const FPbDeleteMailReq& Right) const;
    bool operator!=(const FPbDeleteMailReq& Right) const;
     
};

namespace idlepb {
class DeleteMailAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDeleteMailAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDeleteMailAck();
    FPbDeleteMailAck(const idlepb::DeleteMailAck& Right);
    void FromPb(const idlepb::DeleteMailAck& Right);
    void ToPb(idlepb::DeleteMailAck* Out) const;
    void Reset();
    void operator=(const idlepb::DeleteMailAck& Right);
    bool operator==(const FPbDeleteMailAck& Right) const;
    bool operator!=(const FPbDeleteMailAck& Right) const;
     
};

namespace idlepb {
class OneClickGetMailAttachmentReq;
}  // namespace idlepb

/**
 * 请求邮件一键领取
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickGetMailAttachmentReq
{
    GENERATED_BODY();


    FPbOneClickGetMailAttachmentReq();
    FPbOneClickGetMailAttachmentReq(const idlepb::OneClickGetMailAttachmentReq& Right);
    void FromPb(const idlepb::OneClickGetMailAttachmentReq& Right);
    void ToPb(idlepb::OneClickGetMailAttachmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickGetMailAttachmentReq& Right);
    bool operator==(const FPbOneClickGetMailAttachmentReq& Right) const;
    bool operator!=(const FPbOneClickGetMailAttachmentReq& Right) const;
     
};

namespace idlepb {
class OneClickGetMailAttachmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickGetMailAttachmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbMailOperation result;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 unread_mail_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMail> mail_box;


    FPbOneClickGetMailAttachmentAck();
    FPbOneClickGetMailAttachmentAck(const idlepb::OneClickGetMailAttachmentAck& Right);
    void FromPb(const idlepb::OneClickGetMailAttachmentAck& Right);
    void ToPb(idlepb::OneClickGetMailAttachmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickGetMailAttachmentAck& Right);
    bool operator==(const FPbOneClickGetMailAttachmentAck& Right) const;
    bool operator!=(const FPbOneClickGetMailAttachmentAck& Right) const;
     
};

namespace idlepb {
class OneClickReadMailReq;
}  // namespace idlepb

/**
 * 请求邮件一键已读
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickReadMailReq
{
    GENERATED_BODY();


    FPbOneClickReadMailReq();
    FPbOneClickReadMailReq(const idlepb::OneClickReadMailReq& Right);
    void FromPb(const idlepb::OneClickReadMailReq& Right);
    void ToPb(idlepb::OneClickReadMailReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickReadMailReq& Right);
    bool operator==(const FPbOneClickReadMailReq& Right) const;
    bool operator!=(const FPbOneClickReadMailReq& Right) const;
     
};

namespace idlepb {
class OneClickReadMailAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickReadMailAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbOneClickReadMailAck();
    FPbOneClickReadMailAck(const idlepb::OneClickReadMailAck& Right);
    void FromPb(const idlepb::OneClickReadMailAck& Right);
    void ToPb(idlepb::OneClickReadMailAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickReadMailAck& Right);
    bool operator==(const FPbOneClickReadMailAck& Right) const;
    bool operator!=(const FPbOneClickReadMailAck& Right) const;
     
};

namespace idlepb {
class OneClickDeleteMailReq;
}  // namespace idlepb

/**
 * 请求邮件一键删除
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickDeleteMailReq
{
    GENERATED_BODY();


    FPbOneClickDeleteMailReq();
    FPbOneClickDeleteMailReq(const idlepb::OneClickDeleteMailReq& Right);
    void FromPb(const idlepb::OneClickDeleteMailReq& Right);
    void ToPb(idlepb::OneClickDeleteMailReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickDeleteMailReq& Right);
    bool operator==(const FPbOneClickDeleteMailReq& Right) const;
    bool operator!=(const FPbOneClickDeleteMailReq& Right) const;
     
};

namespace idlepb {
class OneClickDeleteMailAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickDeleteMailAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> deleted_index;


    FPbOneClickDeleteMailAck();
    FPbOneClickDeleteMailAck(const idlepb::OneClickDeleteMailAck& Right);
    void FromPb(const idlepb::OneClickDeleteMailAck& Right);
    void ToPb(idlepb::OneClickDeleteMailAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickDeleteMailAck& Right);
    bool operator==(const FPbOneClickDeleteMailAck& Right) const;
    bool operator!=(const FPbOneClickDeleteMailAck& Right) const;
     
};

namespace idlepb {
class UnlockFunctionModuleReq;
}  // namespace idlepb

/**
 * 解锁指定模块
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockFunctionModuleReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFunctionModuleType type;


    FPbUnlockFunctionModuleReq();
    FPbUnlockFunctionModuleReq(const idlepb::UnlockFunctionModuleReq& Right);
    void FromPb(const idlepb::UnlockFunctionModuleReq& Right);
    void ToPb(idlepb::UnlockFunctionModuleReq* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockFunctionModuleReq& Right);
    bool operator==(const FPbUnlockFunctionModuleReq& Right) const;
    bool operator!=(const FPbUnlockFunctionModuleReq& Right) const;
     
};

namespace idlepb {
class UnlockFunctionModuleAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUnlockFunctionModuleAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUnlockFunctionModuleAck();
    FPbUnlockFunctionModuleAck(const idlepb::UnlockFunctionModuleAck& Right);
    void FromPb(const idlepb::UnlockFunctionModuleAck& Right);
    void ToPb(idlepb::UnlockFunctionModuleAck* Out) const;
    void Reset();
    void operator=(const idlepb::UnlockFunctionModuleAck& Right);
    bool operator==(const FPbUnlockFunctionModuleAck& Right) const;
    bool operator!=(const FPbUnlockFunctionModuleAck& Right) const;
     
};

namespace idlepb {
class NotifyUnlockedModuels;
}  // namespace idlepb

/**
 * 刷新已经解锁模块列表
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyUnlockedModuels
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlocked_modules;


    FPbNotifyUnlockedModuels();
    FPbNotifyUnlockedModuels(const idlepb::NotifyUnlockedModuels& Right);
    void FromPb(const idlepb::NotifyUnlockedModuels& Right);
    void ToPb(idlepb::NotifyUnlockedModuels* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyUnlockedModuels& Right);
    bool operator==(const FPbNotifyUnlockedModuels& Right) const;
    bool operator!=(const FPbNotifyUnlockedModuels& Right) const;
     
};

namespace idlepb {
class UpdateChat;
}  // namespace idlepb

/**
 * 聊天消息更新
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateChat
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbChatMessageChannel channel;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbChatMessage chat_message;


    FPbUpdateChat();
    FPbUpdateChat(const idlepb::UpdateChat& Right);
    void FromPb(const idlepb::UpdateChat& Right);
    void ToPb(idlepb::UpdateChat* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateChat& Right);
    bool operator==(const FPbUpdateChat& Right) const;
    bool operator!=(const FPbUpdateChat& Right) const;
     
};

namespace idlepb {
class SendChatMessageReq;
}  // namespace idlepb

/**
 * 发送聊天消息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSendChatMessageReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbChatMessageChannel channel;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString text;

    /** 消息类型，这会允许客户端命令服务器广播公告，这并不合适 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbChatMessageType type;


    FPbSendChatMessageReq();
    FPbSendChatMessageReq(const idlepb::SendChatMessageReq& Right);
    void FromPb(const idlepb::SendChatMessageReq& Right);
    void ToPb(idlepb::SendChatMessageReq* Out) const;
    void Reset();
    void operator=(const idlepb::SendChatMessageReq& Right);
    bool operator==(const FPbSendChatMessageReq& Right) const;
    bool operator!=(const FPbSendChatMessageReq& Right) const;
     
};

namespace idlepb {
class SendChatMessageAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSendChatMessageAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSendChatMessageAck();
    FPbSendChatMessageAck(const idlepb::SendChatMessageAck& Right);
    void FromPb(const idlepb::SendChatMessageAck& Right);
    void ToPb(idlepb::SendChatMessageAck* Out) const;
    void Reset();
    void operator=(const idlepb::SendChatMessageAck& Right);
    bool operator==(const FPbSendChatMessageAck& Right) const;
    bool operator!=(const FPbSendChatMessageAck& Right) const;
     
};

namespace idlepb {
class GetChatRecordReq;
}  // namespace idlepb

/**
 * 请求聊天记录
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetChatRecordReq
{
    GENERATED_BODY();


    FPbGetChatRecordReq();
    FPbGetChatRecordReq(const idlepb::GetChatRecordReq& Right);
    void FromPb(const idlepb::GetChatRecordReq& Right);
    void ToPb(idlepb::GetChatRecordReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetChatRecordReq& Right);
    bool operator==(const FPbGetChatRecordReq& Right) const;
    bool operator!=(const FPbGetChatRecordReq& Right) const;
     
};

namespace idlepb {
class GetChatRecordAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetChatRecordAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbChatData public_chat_record;

    /** 私聊记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRolePrivateChatRecord private_chat_record;

    /** 宗门聊天记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbChatMessage> sept_record;


    FPbGetChatRecordAck();
    FPbGetChatRecordAck(const idlepb::GetChatRecordAck& Right);
    void FromPb(const idlepb::GetChatRecordAck& Right);
    void ToPb(idlepb::GetChatRecordAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetChatRecordAck& Right);
    bool operator==(const FPbGetChatRecordAck& Right) const;
    bool operator!=(const FPbGetChatRecordAck& Right) const;
     
};

namespace idlepb {
class DeletePrivateChatRecordReq;
}  // namespace idlepb

/**
 * 请求删除私聊记录
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDeletePrivateChatRecordReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;


    FPbDeletePrivateChatRecordReq();
    FPbDeletePrivateChatRecordReq(const idlepb::DeletePrivateChatRecordReq& Right);
    void FromPb(const idlepb::DeletePrivateChatRecordReq& Right);
    void ToPb(idlepb::DeletePrivateChatRecordReq* Out) const;
    void Reset();
    void operator=(const idlepb::DeletePrivateChatRecordReq& Right);
    bool operator==(const FPbDeletePrivateChatRecordReq& Right) const;
    bool operator!=(const FPbDeletePrivateChatRecordReq& Right) const;
     
};

namespace idlepb {
class DeletePrivateChatRecordAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDeletePrivateChatRecordAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDeletePrivateChatRecordAck();
    FPbDeletePrivateChatRecordAck(const idlepb::DeletePrivateChatRecordAck& Right);
    void FromPb(const idlepb::DeletePrivateChatRecordAck& Right);
    void ToPb(idlepb::DeletePrivateChatRecordAck* Out) const;
    void Reset();
    void operator=(const idlepb::DeletePrivateChatRecordAck& Right);
    bool operator==(const FPbDeletePrivateChatRecordAck& Right) const;
    bool operator!=(const FPbDeletePrivateChatRecordAck& Right) const;
     
};

namespace idlepb {
class ClearChatUnreadNumReq;
}  // namespace idlepb

/**
 * 请求聊天记录已读
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbClearChatUnreadNumReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;


    FPbClearChatUnreadNumReq();
    FPbClearChatUnreadNumReq(const idlepb::ClearChatUnreadNumReq& Right);
    void FromPb(const idlepb::ClearChatUnreadNumReq& Right);
    void ToPb(idlepb::ClearChatUnreadNumReq* Out) const;
    void Reset();
    void operator=(const idlepb::ClearChatUnreadNumReq& Right);
    bool operator==(const FPbClearChatUnreadNumReq& Right) const;
    bool operator!=(const FPbClearChatUnreadNumReq& Right) const;
     
};

namespace idlepb {
class ClearChatUnreadNumAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbClearChatUnreadNumAck
{
    GENERATED_BODY();


    FPbClearChatUnreadNumAck();
    FPbClearChatUnreadNumAck(const idlepb::ClearChatUnreadNumAck& Right);
    void FromPb(const idlepb::ClearChatUnreadNumAck& Right);
    void ToPb(idlepb::ClearChatUnreadNumAck* Out) const;
    void Reset();
    void operator=(const idlepb::ClearChatUnreadNumAck& Right);
    bool operator==(const FPbClearChatUnreadNumAck& Right) const;
    bool operator!=(const FPbClearChatUnreadNumAck& Right) const;
     
};

namespace idlepb {
class GetRoleInfoCacheReq;
}  // namespace idlepb

/**
 * 请求玩家数据缓存
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleInfoCacheReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> role_ids;


    FPbGetRoleInfoCacheReq();
    FPbGetRoleInfoCacheReq(const idlepb::GetRoleInfoCacheReq& Right);
    void FromPb(const idlepb::GetRoleInfoCacheReq& Right);
    void ToPb(idlepb::GetRoleInfoCacheReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleInfoCacheReq& Right);
    bool operator==(const FPbGetRoleInfoCacheReq& Right) const;
    bool operator!=(const FPbGetRoleInfoCacheReq& Right) const;
     
};

namespace idlepb {
class GetRoleInfoCacheAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleInfoCacheAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleRoleInfo> role_infos;


    FPbGetRoleInfoCacheAck();
    FPbGetRoleInfoCacheAck(const idlepb::GetRoleInfoCacheAck& Right);
    void FromPb(const idlepb::GetRoleInfoCacheAck& Right);
    void ToPb(idlepb::GetRoleInfoCacheAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleInfoCacheAck& Right);
    bool operator==(const FPbGetRoleInfoCacheAck& Right) const;
    bool operator!=(const FPbGetRoleInfoCacheAck& Right) const;
     
};

namespace idlepb {
class ForgeRefineStartReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineStartReq
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 材料ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 material_id;

    /** 辅材ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ext_material_id;

    /** 目标数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_num;

    /** 自动出售“下品” */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_sell_poor;

    /** 自动出售“中品” */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_sell_middle;


    FPbForgeRefineStartReq();
    FPbForgeRefineStartReq(const idlepb::ForgeRefineStartReq& Right);
    void FromPb(const idlepb::ForgeRefineStartReq& Right);
    void ToPb(idlepb::ForgeRefineStartReq* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineStartReq& Right);
    bool operator==(const FPbForgeRefineStartReq& Right) const;
    bool operator!=(const FPbForgeRefineStartReq& Right) const;
     
};

namespace idlepb {
class ForgeRefineStartAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineStartAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbForgeRefineStartAck();
    FPbForgeRefineStartAck(const idlepb::ForgeRefineStartAck& Right);
    void FromPb(const idlepb::ForgeRefineStartAck& Right);
    void ToPb(idlepb::ForgeRefineStartAck* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineStartAck& Right);
    bool operator==(const FPbForgeRefineStartAck& Right) const;
    bool operator!=(const FPbForgeRefineStartAck& Right) const;
     
};

namespace idlepb {
class ForgeRefineCancelReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineCancelReq
{
    GENERATED_BODY();


    FPbForgeRefineCancelReq();
    FPbForgeRefineCancelReq(const idlepb::ForgeRefineCancelReq& Right);
    void FromPb(const idlepb::ForgeRefineCancelReq& Right);
    void ToPb(idlepb::ForgeRefineCancelReq* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineCancelReq& Right);
    bool operator==(const FPbForgeRefineCancelReq& Right) const;
    bool operator!=(const FPbForgeRefineCancelReq& Right) const;
     
};

namespace idlepb {
class ForgeRefineCancelAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineCancelAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbForgeRefineCancelAck();
    FPbForgeRefineCancelAck(const idlepb::ForgeRefineCancelAck& Right);
    void FromPb(const idlepb::ForgeRefineCancelAck& Right);
    void ToPb(idlepb::ForgeRefineCancelAck* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineCancelAck& Right);
    bool operator==(const FPbForgeRefineCancelAck& Right) const;
    bool operator!=(const FPbForgeRefineCancelAck& Right) const;
     
};

namespace idlepb {
class ForgeRefineExtractReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineExtractReq
{
    GENERATED_BODY();


    FPbForgeRefineExtractReq();
    FPbForgeRefineExtractReq(const idlepb::ForgeRefineExtractReq& Right);
    void FromPb(const idlepb::ForgeRefineExtractReq& Right);
    void ToPb(idlepb::ForgeRefineExtractReq* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineExtractReq& Right);
    bool operator==(const FPbForgeRefineExtractReq& Right) const;
    bool operator!=(const FPbForgeRefineExtractReq& Right) const;
     
};

namespace idlepb {
class ForgeRefineExtractAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRefineExtractAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> items;


    FPbForgeRefineExtractAck();
    FPbForgeRefineExtractAck(const idlepb::ForgeRefineExtractAck& Right);
    void FromPb(const idlepb::ForgeRefineExtractAck& Right);
    void ToPb(idlepb::ForgeRefineExtractAck* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRefineExtractAck& Right);
    bool operator==(const FPbForgeRefineExtractAck& Right) const;
    bool operator!=(const FPbForgeRefineExtractAck& Right) const;
     
};

namespace idlepb {
class GetForgeLostEquipmentDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetForgeLostEquipmentDataReq
{
    GENERATED_BODY();


    FPbGetForgeLostEquipmentDataReq();
    FPbGetForgeLostEquipmentDataReq(const idlepb::GetForgeLostEquipmentDataReq& Right);
    void FromPb(const idlepb::GetForgeLostEquipmentDataReq& Right);
    void ToPb(idlepb::GetForgeLostEquipmentDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetForgeLostEquipmentDataReq& Right);
    bool operator==(const FPbGetForgeLostEquipmentDataReq& Right) const;
    bool operator!=(const FPbGetForgeLostEquipmentDataReq& Right) const;
     
};

namespace idlepb {
class GetForgeLostEquipmentDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetForgeLostEquipmentDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLostEquipmentData> data;


    FPbGetForgeLostEquipmentDataAck();
    FPbGetForgeLostEquipmentDataAck(const idlepb::GetForgeLostEquipmentDataAck& Right);
    void FromPb(const idlepb::GetForgeLostEquipmentDataAck& Right);
    void ToPb(idlepb::GetForgeLostEquipmentDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetForgeLostEquipmentDataAck& Right);
    bool operator==(const FPbGetForgeLostEquipmentDataAck& Right) const;
    bool operator!=(const FPbGetForgeLostEquipmentDataAck& Right) const;
     
};

namespace idlepb {
class ForgeDestroyReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeDestroyReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 uid;


    FPbForgeDestroyReq();
    FPbForgeDestroyReq(const idlepb::ForgeDestroyReq& Right);
    void FromPb(const idlepb::ForgeDestroyReq& Right);
    void ToPb(idlepb::ForgeDestroyReq* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeDestroyReq& Right);
    bool operator==(const FPbForgeDestroyReq& Right) const;
    bool operator!=(const FPbForgeDestroyReq& Right) const;
     
};

namespace idlepb {
class ForgeDestroyAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeDestroyAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbForgeDestroyAck();
    FPbForgeDestroyAck(const idlepb::ForgeDestroyAck& Right);
    void FromPb(const idlepb::ForgeDestroyAck& Right);
    void ToPb(idlepb::ForgeDestroyAck* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeDestroyAck& Right);
    bool operator==(const FPbForgeDestroyAck& Right) const;
    bool operator!=(const FPbForgeDestroyAck& Right) const;
     
};

namespace idlepb {
class ForgeFindBackReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeFindBackReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;


    FPbForgeFindBackReq();
    FPbForgeFindBackReq(const idlepb::ForgeFindBackReq& Right);
    void FromPb(const idlepb::ForgeFindBackReq& Right);
    void ToPb(idlepb::ForgeFindBackReq* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeFindBackReq& Right);
    bool operator==(const FPbForgeFindBackReq& Right) const;
    bool operator!=(const FPbForgeFindBackReq& Right) const;
     
};

namespace idlepb {
class ForgeFindBackAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeFindBackAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbForgeFindBackAck();
    FPbForgeFindBackAck(const idlepb::ForgeFindBackAck& Right);
    void FromPb(const idlepb::ForgeFindBackAck& Right);
    void ToPb(idlepb::ForgeFindBackAck* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeFindBackAck& Right);
    bool operator==(const FPbForgeFindBackAck& Right) const;
    bool operator!=(const FPbForgeFindBackAck& Right) const;
     
};

namespace idlepb {
class RequestPillElixirDataReq;
}  // namespace idlepb

/**
 * 请求秘药数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestPillElixirDataReq
{
    GENERATED_BODY();


    FPbRequestPillElixirDataReq();
    FPbRequestPillElixirDataReq(const idlepb::RequestPillElixirDataReq& Right);
    void FromPb(const idlepb::RequestPillElixirDataReq& Right);
    void ToPb(idlepb::RequestPillElixirDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestPillElixirDataReq& Right);
    bool operator==(const FPbRequestPillElixirDataReq& Right) const;
    bool operator!=(const FPbRequestPillElixirDataReq& Right) const;
     
};

namespace idlepb {
class RequestPillElixirDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestPillElixirDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRolePillElixirData data;


    FPbRequestPillElixirDataAck();
    FPbRequestPillElixirDataAck(const idlepb::RequestPillElixirDataAck& Right);
    void FromPb(const idlepb::RequestPillElixirDataAck& Right);
    void ToPb(idlepb::RequestPillElixirDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestPillElixirDataAck& Right);
    bool operator==(const FPbRequestPillElixirDataAck& Right) const;
    bool operator!=(const FPbRequestPillElixirDataAck& Right) const;
     
};

namespace idlepb {
class GetOnePillElixirDataReq;
}  // namespace idlepb

/**
 * 请求单种秘药数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetOnePillElixirDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_cfg_id;


    FPbGetOnePillElixirDataReq();
    FPbGetOnePillElixirDataReq(const idlepb::GetOnePillElixirDataReq& Right);
    void FromPb(const idlepb::GetOnePillElixirDataReq& Right);
    void ToPb(idlepb::GetOnePillElixirDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetOnePillElixirDataReq& Right);
    bool operator==(const FPbGetOnePillElixirDataReq& Right) const;
    bool operator!=(const FPbGetOnePillElixirDataReq& Right) const;
     
};

namespace idlepb {
class GetOnePillElixirDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetOnePillElixirDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPillElixirData data;


    FPbGetOnePillElixirDataAck();
    FPbGetOnePillElixirDataAck(const idlepb::GetOnePillElixirDataAck& Right);
    void FromPb(const idlepb::GetOnePillElixirDataAck& Right);
    void ToPb(idlepb::GetOnePillElixirDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetOnePillElixirDataAck& Right);
    bool operator==(const FPbGetOnePillElixirDataAck& Right) const;
    bool operator!=(const FPbGetOnePillElixirDataAck& Right) const;
     
};

namespace idlepb {
class RequestModifyPillElixirFilterReq;
}  // namespace idlepb

/**
 * 请求修改秘药过滤配置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestModifyPillElixirFilterReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_double;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_exp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_property;


    FPbRequestModifyPillElixirFilterReq();
    FPbRequestModifyPillElixirFilterReq(const idlepb::RequestModifyPillElixirFilterReq& Right);
    void FromPb(const idlepb::RequestModifyPillElixirFilterReq& Right);
    void ToPb(idlepb::RequestModifyPillElixirFilterReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestModifyPillElixirFilterReq& Right);
    bool operator==(const FPbRequestModifyPillElixirFilterReq& Right) const;
    bool operator!=(const FPbRequestModifyPillElixirFilterReq& Right) const;
     
};

namespace idlepb {
class RequestModifyPillElixirFilterAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestModifyPillElixirFilterAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbRequestModifyPillElixirFilterAck();
    FPbRequestModifyPillElixirFilterAck(const idlepb::RequestModifyPillElixirFilterAck& Right);
    void FromPb(const idlepb::RequestModifyPillElixirFilterAck& Right);
    void ToPb(idlepb::RequestModifyPillElixirFilterAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestModifyPillElixirFilterAck& Right);
    bool operator==(const FPbRequestModifyPillElixirFilterAck& Right) const;
    bool operator!=(const FPbRequestModifyPillElixirFilterAck& Right) const;
     
};

namespace idlepb {
class UsePillElixirReport;
}  // namespace idlepb

/**
 * 使用秘药报告
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUsePillElixirReport
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float property_num;


    FPbUsePillElixirReport();
    FPbUsePillElixirReport(const idlepb::UsePillElixirReport& Right);
    void FromPb(const idlepb::UsePillElixirReport& Right);
    void ToPb(idlepb::UsePillElixirReport* Out) const;
    void Reset();
    void operator=(const idlepb::UsePillElixirReport& Right);
    bool operator==(const FPbUsePillElixirReport& Right) const;
    bool operator!=(const FPbUsePillElixirReport& Right) const;
     
};

namespace idlepb {
class UsePillElixirReq;
}  // namespace idlepb

/**
 * 使用单颗秘药
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUsePillElixirReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;


    FPbUsePillElixirReq();
    FPbUsePillElixirReq(const idlepb::UsePillElixirReq& Right);
    void FromPb(const idlepb::UsePillElixirReq& Right);
    void ToPb(idlepb::UsePillElixirReq* Out) const;
    void Reset();
    void operator=(const idlepb::UsePillElixirReq& Right);
    bool operator==(const FPbUsePillElixirReq& Right) const;
    bool operator!=(const FPbUsePillElixirReq& Right) const;
     
};

namespace idlepb {
class UsePillElixirAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUsePillElixirAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUsePillElixirAck();
    FPbUsePillElixirAck(const idlepb::UsePillElixirAck& Right);
    void FromPb(const idlepb::UsePillElixirAck& Right);
    void ToPb(idlepb::UsePillElixirAck* Out) const;
    void Reset();
    void operator=(const idlepb::UsePillElixirAck& Right);
    bool operator==(const FPbUsePillElixirAck& Right) const;
    bool operator!=(const FPbUsePillElixirAck& Right) const;
     
};

namespace idlepb {
class OneClickUsePillElixirReq;
}  // namespace idlepb

/**
 * 一键使用秘药
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickUsePillElixirReq
{
    GENERATED_BODY();


    FPbOneClickUsePillElixirReq();
    FPbOneClickUsePillElixirReq(const idlepb::OneClickUsePillElixirReq& Right);
    void FromPb(const idlepb::OneClickUsePillElixirReq& Right);
    void ToPb(idlepb::OneClickUsePillElixirReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickUsePillElixirReq& Right);
    bool operator==(const FPbOneClickUsePillElixirReq& Right) const;
    bool operator!=(const FPbOneClickUsePillElixirReq& Right) const;
     
};

namespace idlepb {
class OneClickUsePillElixirAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickUsePillElixirAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbUsePillElixirReport> report;


    FPbOneClickUsePillElixirAck();
    FPbOneClickUsePillElixirAck(const idlepb::OneClickUsePillElixirAck& Right);
    void FromPb(const idlepb::OneClickUsePillElixirAck& Right);
    void ToPb(idlepb::OneClickUsePillElixirAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickUsePillElixirAck& Right);
    bool operator==(const FPbOneClickUsePillElixirAck& Right) const;
    bool operator!=(const FPbOneClickUsePillElixirAck& Right) const;
     
};

namespace idlepb {
class TradePillElixirReq;
}  // namespace idlepb

/**
 * 请求秘药兑换天机石
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTradePillElixirReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbTradePillElixirReq();
    FPbTradePillElixirReq(const idlepb::TradePillElixirReq& Right);
    void FromPb(const idlepb::TradePillElixirReq& Right);
    void ToPb(idlepb::TradePillElixirReq* Out) const;
    void Reset();
    void operator=(const idlepb::TradePillElixirReq& Right);
    bool operator==(const FPbTradePillElixirReq& Right) const;
    bool operator!=(const FPbTradePillElixirReq& Right) const;
     
};

namespace idlepb {
class TradePillElixirAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTradePillElixirAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbTradePillElixirAck();
    FPbTradePillElixirAck(const idlepb::TradePillElixirAck& Right);
    void FromPb(const idlepb::TradePillElixirAck& Right);
    void ToPb(idlepb::TradePillElixirAck* Out) const;
    void Reset();
    void operator=(const idlepb::TradePillElixirAck& Right);
    bool operator==(const FPbTradePillElixirAck& Right) const;
    bool operator!=(const FPbTradePillElixirAck& Right) const;
     
};

namespace idlepb {
class NotifyAutoModeStatus;
}  // namespace idlepb

/**
 * 更新自动模式状态
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyAutoModeStatus
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool enable;


    FPbNotifyAutoModeStatus();
    FPbNotifyAutoModeStatus(const idlepb::NotifyAutoModeStatus& Right);
    void FromPb(const idlepb::NotifyAutoModeStatus& Right);
    void ToPb(idlepb::NotifyAutoModeStatus* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyAutoModeStatus& Right);
    bool operator==(const FPbNotifyAutoModeStatus& Right) const;
    bool operator!=(const FPbNotifyAutoModeStatus& Right) const;
     
};

namespace idlepb {
class SetAutoMode;
}  // namespace idlepb

/**
 * 设置自动模块状态
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetAutoMode
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool enable;


    FPbSetAutoMode();
    FPbSetAutoMode(const idlepb::SetAutoMode& Right);
    void FromPb(const idlepb::SetAutoMode& Right);
    void ToPb(idlepb::SetAutoMode* Out) const;
    void Reset();
    void operator=(const idlepb::SetAutoMode& Right);
    bool operator==(const FPbSetAutoMode& Right) const;
    bool operator!=(const FPbSetAutoMode& Right) const;
     
};

namespace idlepb {
class ReinforceEquipmentReq;
}  // namespace idlepb

/**
 * 请求强化装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReinforceEquipmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;


    FPbReinforceEquipmentReq();
    FPbReinforceEquipmentReq(const idlepb::ReinforceEquipmentReq& Right);
    void FromPb(const idlepb::ReinforceEquipmentReq& Right);
    void ToPb(idlepb::ReinforceEquipmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReinforceEquipmentReq& Right);
    bool operator==(const FPbReinforceEquipmentReq& Right) const;
    bool operator!=(const FPbReinforceEquipmentReq& Right) const;
     
};

namespace idlepb {
class ReinforceEquipmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReinforceEquipmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbReinforceEquipmentAck();
    FPbReinforceEquipmentAck(const idlepb::ReinforceEquipmentAck& Right);
    void FromPb(const idlepb::ReinforceEquipmentAck& Right);
    void ToPb(idlepb::ReinforceEquipmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReinforceEquipmentAck& Right);
    bool operator==(const FPbReinforceEquipmentAck& Right) const;
    bool operator!=(const FPbReinforceEquipmentAck& Right) const;
     
};

namespace idlepb {
class QiWenEquipmentReq;
}  // namespace idlepb

/**
 * 请求装备器纹
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQiWenEquipmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 select_abc;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> commit_materials;


    FPbQiWenEquipmentReq();
    FPbQiWenEquipmentReq(const idlepb::QiWenEquipmentReq& Right);
    void FromPb(const idlepb::QiWenEquipmentReq& Right);
    void ToPb(idlepb::QiWenEquipmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::QiWenEquipmentReq& Right);
    bool operator==(const FPbQiWenEquipmentReq& Right) const;
    bool operator!=(const FPbQiWenEquipmentReq& Right) const;
     
};

namespace idlepb {
class QiWenEquipmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQiWenEquipmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbQiWenEquipmentAck();
    FPbQiWenEquipmentAck(const idlepb::QiWenEquipmentAck& Right);
    void FromPb(const idlepb::QiWenEquipmentAck& Right);
    void ToPb(idlepb::QiWenEquipmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::QiWenEquipmentAck& Right);
    bool operator==(const FPbQiWenEquipmentAck& Right) const;
    bool operator!=(const FPbQiWenEquipmentAck& Right) const;
     
};

namespace idlepb {
class RefineEquipmentReq;
}  // namespace idlepb

/**
 * 请求精炼装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefineEquipmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;


    FPbRefineEquipmentReq();
    FPbRefineEquipmentReq(const idlepb::RefineEquipmentReq& Right);
    void FromPb(const idlepb::RefineEquipmentReq& Right);
    void ToPb(idlepb::RefineEquipmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::RefineEquipmentReq& Right);
    bool operator==(const FPbRefineEquipmentReq& Right) const;
    bool operator!=(const FPbRefineEquipmentReq& Right) const;
     
};

namespace idlepb {
class RefineEquipmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefineEquipmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbRefineEquipmentAck();
    FPbRefineEquipmentAck(const idlepb::RefineEquipmentAck& Right);
    void FromPb(const idlepb::RefineEquipmentAck& Right);
    void ToPb(idlepb::RefineEquipmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::RefineEquipmentAck& Right);
    bool operator==(const FPbRefineEquipmentAck& Right) const;
    bool operator!=(const FPbRefineEquipmentAck& Right) const;
     
};

namespace idlepb {
class ResetEquipmentReq;
}  // namespace idlepb

/**
 * 请求还原装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbResetEquipmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> id;


    FPbResetEquipmentReq();
    FPbResetEquipmentReq(const idlepb::ResetEquipmentReq& Right);
    void FromPb(const idlepb::ResetEquipmentReq& Right);
    void ToPb(idlepb::ResetEquipmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::ResetEquipmentReq& Right);
    bool operator==(const FPbResetEquipmentReq& Right) const;
    bool operator!=(const FPbResetEquipmentReq& Right) const;
     
};

namespace idlepb {
class ResetEquipmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbResetEquipmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;


    FPbResetEquipmentAck();
    FPbResetEquipmentAck(const idlepb::ResetEquipmentAck& Right);
    void FromPb(const idlepb::ResetEquipmentAck& Right);
    void ToPb(idlepb::ResetEquipmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::ResetEquipmentAck& Right);
    bool operator==(const FPbResetEquipmentAck& Right) const;
    bool operator!=(const FPbResetEquipmentAck& Right) const;
     
};

namespace idlepb {
class InheritEquipmentReq;
}  // namespace idlepb

/**
 * 请求继承装备
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInheritEquipmentReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 equipment_from;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 equipment_to;


    FPbInheritEquipmentReq();
    FPbInheritEquipmentReq(const idlepb::InheritEquipmentReq& Right);
    void FromPb(const idlepb::InheritEquipmentReq& Right);
    void ToPb(idlepb::InheritEquipmentReq* Out) const;
    void Reset();
    void operator=(const idlepb::InheritEquipmentReq& Right);
    bool operator==(const FPbInheritEquipmentReq& Right) const;
    bool operator!=(const FPbInheritEquipmentReq& Right) const;
     
};

namespace idlepb {
class InheritEquipmentAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInheritEquipmentAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;


    FPbInheritEquipmentAck();
    FPbInheritEquipmentAck(const idlepb::InheritEquipmentAck& Right);
    void FromPb(const idlepb::InheritEquipmentAck& Right);
    void ToPb(idlepb::InheritEquipmentAck* Out) const;
    void Reset();
    void operator=(const idlepb::InheritEquipmentAck& Right);
    bool operator==(const FPbInheritEquipmentAck& Right) const;
    bool operator!=(const FPbInheritEquipmentAck& Right) const;
     
};

namespace idlepb {
class LockItemReq;
}  // namespace idlepb

/**
 * 请求锁定/解锁道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLockItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;


    FPbLockItemReq();
    FPbLockItemReq(const idlepb::LockItemReq& Right);
    void FromPb(const idlepb::LockItemReq& Right);
    void ToPb(idlepb::LockItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::LockItemReq& Right);
    bool operator==(const FPbLockItemReq& Right) const;
    bool operator!=(const FPbLockItemReq& Right) const;
     
};

namespace idlepb {
class LockItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLockItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbLockItemAck();
    FPbLockItemAck(const idlepb::LockItemAck& Right);
    void FromPb(const idlepb::LockItemAck& Right);
    void ToPb(idlepb::LockItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::LockItemAck& Right);
    bool operator==(const FPbLockItemAck& Right) const;
    bool operator!=(const FPbLockItemAck& Right) const;
     
};

namespace idlepb {
class CollectionActivatedSuit;
}  // namespace idlepb

/**
 * 古宝已激活套装
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionActivatedSuit
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float combat_power;


    FPbCollectionActivatedSuit();
    FPbCollectionActivatedSuit(const idlepb::CollectionActivatedSuit& Right);
    void FromPb(const idlepb::CollectionActivatedSuit& Right);
    void ToPb(idlepb::CollectionActivatedSuit* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionActivatedSuit& Right);
    bool operator==(const FPbCollectionActivatedSuit& Right) const;
    bool operator!=(const FPbCollectionActivatedSuit& Right) const;
     
};

namespace idlepb {
class GetRoleCollectionDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleCollectionDataReq
{
    GENERATED_BODY();


    FPbGetRoleCollectionDataReq();
    FPbGetRoleCollectionDataReq(const idlepb::GetRoleCollectionDataReq& Right);
    void FromPb(const idlepb::GetRoleCollectionDataReq& Right);
    void ToPb(idlepb::GetRoleCollectionDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleCollectionDataReq& Right);
    bool operator==(const FPbGetRoleCollectionDataReq& Right) const;
    bool operator!=(const FPbGetRoleCollectionDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleCollectionDataRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleCollectionDataRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionEntry> entries;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCommonCollectionPieceData> common_pieces;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionActivatedSuit> actived_suite;

    /** 已领取奖励的渊源 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> draw_award_done_histories;

    /** 可领取奖励的渊源 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> can_award_histories;

    /** 已领取到累计收集奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionZoneActiveAwardData> zone_active_awards;

    /** 下次可重置强化的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_reset_enhance_ticks;


    FPbGetRoleCollectionDataRsp();
    FPbGetRoleCollectionDataRsp(const idlepb::GetRoleCollectionDataRsp& Right);
    void FromPb(const idlepb::GetRoleCollectionDataRsp& Right);
    void ToPb(idlepb::GetRoleCollectionDataRsp* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleCollectionDataRsp& Right);
    bool operator==(const FPbGetRoleCollectionDataRsp& Right) const;
    bool operator!=(const FPbGetRoleCollectionDataRsp& Right) const;
     
};


/**
 * 古宝操作
*/
UENUM(BlueprintType)
enum class EPbRoleCollectionOpType : uint8
{
    RCOT_PieceFusion = 0 UMETA(DisplayName="碎片合成"),
    RCOT_UpgradeLevel = 1 UMETA(DisplayName="注灵"),
    RCOT_UpgradeStar = 2 UMETA(DisplayName="升星"),
    RCOT_DrawHistoryAward = 3 UMETA(DisplayName="领取渊源奖励"),
    RCOT_DrawZoneActiveAward = 4 UMETA(DisplayName="领取累计收集奖励"),
    RCOT_ResetEnhance = 5 UMETA(DisplayName="重置强化"),
};
constexpr EPbRoleCollectionOpType EPbRoleCollectionOpType_Min = EPbRoleCollectionOpType::RCOT_PieceFusion;
constexpr EPbRoleCollectionOpType EPbRoleCollectionOpType_Max = EPbRoleCollectionOpType::RCOT_ResetEnhance;
constexpr int32 EPbRoleCollectionOpType_ArraySize = static_cast<int32>(EPbRoleCollectionOpType_Max) + 1;
MPROTOCOL_API bool CheckEPbRoleCollectionOpTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRoleCollectionOpTypeDescription(EPbRoleCollectionOpType Val);

template <typename Char>
struct fmt::formatter<EPbRoleCollectionOpType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRoleCollectionOpType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class RoleCollectionOpReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleCollectionOpReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRoleCollectionOpType op_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMapValueInt32> consume_list;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_preview;


    FPbRoleCollectionOpReq();
    FPbRoleCollectionOpReq(const idlepb::RoleCollectionOpReq& Right);
    void FromPb(const idlepb::RoleCollectionOpReq& Right);
    void ToPb(idlepb::RoleCollectionOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::RoleCollectionOpReq& Right);
    bool operator==(const FPbRoleCollectionOpReq& Right) const;
    bool operator!=(const FPbRoleCollectionOpReq& Right) const;
     
};

namespace idlepb {
class RoleCollectionOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleCollectionOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRoleCollectionOpType op_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;


    FPbRoleCollectionOpAck();
    FPbRoleCollectionOpAck(const idlepb::RoleCollectionOpAck& Right);
    void FromPb(const idlepb::RoleCollectionOpAck& Right);
    void ToPb(idlepb::RoleCollectionOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::RoleCollectionOpAck& Right);
    bool operator==(const FPbRoleCollectionOpAck& Right) const;
    bool operator!=(const FPbRoleCollectionOpAck& Right) const;
     
};

namespace idlepb {
class NotifyRoleCollectionData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyRoleCollectionData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCollectionEntry entry;


    FPbNotifyRoleCollectionData();
    FPbNotifyRoleCollectionData(const idlepb::NotifyRoleCollectionData& Right);
    void FromPb(const idlepb::NotifyRoleCollectionData& Right);
    void ToPb(idlepb::NotifyRoleCollectionData* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyRoleCollectionData& Right);
    bool operator==(const FPbNotifyRoleCollectionData& Right) const;
    bool operator!=(const FPbNotifyRoleCollectionData& Right) const;
     
};

namespace idlepb {
class NotifyCommonCollectionPieceData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyCommonCollectionPieceData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCommonCollectionPieceData> common_pieces;


    FPbNotifyCommonCollectionPieceData();
    FPbNotifyCommonCollectionPieceData(const idlepb::NotifyCommonCollectionPieceData& Right);
    void FromPb(const idlepb::NotifyCommonCollectionPieceData& Right);
    void ToPb(idlepb::NotifyCommonCollectionPieceData* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyCommonCollectionPieceData& Right);
    bool operator==(const FPbNotifyCommonCollectionPieceData& Right) const;
    bool operator!=(const FPbNotifyCommonCollectionPieceData& Right) const;
     
};

namespace idlepb {
class NotifyCollectionActivatedSuit;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyCollectionActivatedSuit
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionActivatedSuit> actived_suite;


    FPbNotifyCollectionActivatedSuit();
    FPbNotifyCollectionActivatedSuit(const idlepb::NotifyCollectionActivatedSuit& Right);
    void FromPb(const idlepb::NotifyCollectionActivatedSuit& Right);
    void ToPb(idlepb::NotifyCollectionActivatedSuit* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyCollectionActivatedSuit& Right);
    bool operator==(const FPbNotifyCollectionActivatedSuit& Right) const;
    bool operator!=(const FPbNotifyCollectionActivatedSuit& Right) const;
     
};

namespace idlepb {
class ShareSelfRoleCollectionReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfRoleCollectionReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;


    FPbShareSelfRoleCollectionReq();
    FPbShareSelfRoleCollectionReq(const idlepb::ShareSelfRoleCollectionReq& Right);
    void FromPb(const idlepb::ShareSelfRoleCollectionReq& Right);
    void ToPb(idlepb::ShareSelfRoleCollectionReq* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfRoleCollectionReq& Right);
    bool operator==(const FPbShareSelfRoleCollectionReq& Right) const;
    bool operator!=(const FPbShareSelfRoleCollectionReq& Right) const;
     
};

namespace idlepb {
class ShareSelfRoleCollectionRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfRoleCollectionRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 share_id;


    FPbShareSelfRoleCollectionRsp();
    FPbShareSelfRoleCollectionRsp(const idlepb::ShareSelfRoleCollectionRsp& Right);
    void FromPb(const idlepb::ShareSelfRoleCollectionRsp& Right);
    void ToPb(idlepb::ShareSelfRoleCollectionRsp* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfRoleCollectionRsp& Right);
    bool operator==(const FPbShareSelfRoleCollectionRsp& Right) const;
    bool operator!=(const FPbShareSelfRoleCollectionRsp& Right) const;
     
};

namespace idlepb {
class GetShareRoleCollectionDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShareRoleCollectionDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 share_id;


    FPbGetShareRoleCollectionDataReq();
    FPbGetShareRoleCollectionDataReq(const idlepb::GetShareRoleCollectionDataReq& Right);
    void FromPb(const idlepb::GetShareRoleCollectionDataReq& Right);
    void ToPb(idlepb::GetShareRoleCollectionDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetShareRoleCollectionDataReq& Right);
    bool operator==(const FPbGetShareRoleCollectionDataReq& Right) const;
    bool operator!=(const FPbGetShareRoleCollectionDataReq& Right) const;
     
};

namespace idlepb {
class GetShareRoleCollectionDataRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShareRoleCollectionDataRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCollectionEntry collection_data;


    FPbGetShareRoleCollectionDataRsp();
    FPbGetShareRoleCollectionDataRsp(const idlepb::GetShareRoleCollectionDataRsp& Right);
    void FromPb(const idlepb::GetShareRoleCollectionDataRsp& Right);
    void ToPb(idlepb::GetShareRoleCollectionDataRsp* Out) const;
    void Reset();
    void operator=(const idlepb::GetShareRoleCollectionDataRsp& Right);
    bool operator==(const FPbGetShareRoleCollectionDataRsp& Right) const;
    bool operator!=(const FPbGetShareRoleCollectionDataRsp& Right) const;
     
};

namespace idlepb {
class NotifyRoleCollectionHistories;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyRoleCollectionHistories
{
    GENERATED_BODY();

    /** 已领取奖励的渊源 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> draw_award_done_histories;

    /** 可领取奖励的渊源 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> can_award_histories;


    FPbNotifyRoleCollectionHistories();
    FPbNotifyRoleCollectionHistories(const idlepb::NotifyRoleCollectionHistories& Right);
    void FromPb(const idlepb::NotifyRoleCollectionHistories& Right);
    void ToPb(idlepb::NotifyRoleCollectionHistories* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyRoleCollectionHistories& Right);
    bool operator==(const FPbNotifyRoleCollectionHistories& Right) const;
    bool operator!=(const FPbNotifyRoleCollectionHistories& Right) const;
     
};

namespace idlepb {
class NotifyCollectionZoneActiveAwards;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyCollectionZoneActiveAwards
{
    GENERATED_BODY();

    /** 已领取到累计收集奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionZoneActiveAwardData> zone_active_awards;


    FPbNotifyCollectionZoneActiveAwards();
    FPbNotifyCollectionZoneActiveAwards(const idlepb::NotifyCollectionZoneActiveAwards& Right);
    void FromPb(const idlepb::NotifyCollectionZoneActiveAwards& Right);
    void ToPb(idlepb::NotifyCollectionZoneActiveAwards* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyCollectionZoneActiveAwards& Right);
    bool operator==(const FPbNotifyCollectionZoneActiveAwards& Right) const;
    bool operator!=(const FPbNotifyCollectionZoneActiveAwards& Right) const;
     
};

namespace idlepb {
class NotifyRoleCollectionNextResetEnhanceTicks;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyRoleCollectionNextResetEnhanceTicks
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_reset_enhance_ticks;


    FPbNotifyRoleCollectionNextResetEnhanceTicks();
    FPbNotifyRoleCollectionNextResetEnhanceTicks(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right);
    void FromPb(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right);
    void ToPb(idlepb::NotifyRoleCollectionNextResetEnhanceTicks* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyRoleCollectionNextResetEnhanceTicks& Right);
    bool operator==(const FPbNotifyRoleCollectionNextResetEnhanceTicks& Right) const;
    bool operator!=(const FPbNotifyRoleCollectionNextResetEnhanceTicks& Right) const;
     
};

namespace idlepb {
class RoleBattleHistoryList;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleBattleHistoryList
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleBattleInfo> entries;


    FPbRoleBattleHistoryList();
    FPbRoleBattleHistoryList(const idlepb::RoleBattleHistoryList& Right);
    void FromPb(const idlepb::RoleBattleHistoryList& Right);
    void ToPb(idlepb::RoleBattleHistoryList* Out) const;
    void Reset();
    void operator=(const idlepb::RoleBattleHistoryList& Right);
    bool operator==(const FPbRoleBattleHistoryList& Right) const;
    bool operator!=(const FPbRoleBattleHistoryList& Right) const;
     
};

namespace idlepb {
class NotifySoloArenaChallengeOver;
}  // namespace idlepb

/**
 * 切磋挑战结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifySoloArenaChallengeOver
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool win;

    /** 对战信息提要 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleInfo info;


    FPbNotifySoloArenaChallengeOver();
    FPbNotifySoloArenaChallengeOver(const idlepb::NotifySoloArenaChallengeOver& Right);
    void FromPb(const idlepb::NotifySoloArenaChallengeOver& Right);
    void ToPb(idlepb::NotifySoloArenaChallengeOver* Out) const;
    void Reset();
    void operator=(const idlepb::NotifySoloArenaChallengeOver& Right);
    bool operator==(const FPbNotifySoloArenaChallengeOver& Right) const;
    bool operator!=(const FPbNotifySoloArenaChallengeOver& Right) const;
     
};

namespace idlepb {
class SoloArenaChallengeReq;
}  // namespace idlepb

/**
 * 发起切磋
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSoloArenaChallengeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 target_role_id;


    FPbSoloArenaChallengeReq();
    FPbSoloArenaChallengeReq(const idlepb::SoloArenaChallengeReq& Right);
    void FromPb(const idlepb::SoloArenaChallengeReq& Right);
    void ToPb(idlepb::SoloArenaChallengeReq* Out) const;
    void Reset();
    void operator=(const idlepb::SoloArenaChallengeReq& Right);
    bool operator==(const FPbSoloArenaChallengeReq& Right) const;
    bool operator!=(const FPbSoloArenaChallengeReq& Right) const;
     
};

namespace idlepb {
class SoloArenaChallengeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSoloArenaChallengeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSoloArenaChallengeAck();
    FPbSoloArenaChallengeAck(const idlepb::SoloArenaChallengeAck& Right);
    void FromPb(const idlepb::SoloArenaChallengeAck& Right);
    void ToPb(idlepb::SoloArenaChallengeAck* Out) const;
    void Reset();
    void operator=(const idlepb::SoloArenaChallengeAck& Right);
    bool operator==(const FPbSoloArenaChallengeAck& Right) const;
    bool operator!=(const FPbSoloArenaChallengeAck& Right) const;
     
};

namespace idlepb {
class SoloArenaQuickEndReq;
}  // namespace idlepb

/**
 * 请求快速结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSoloArenaQuickEndReq
{
    GENERATED_BODY();

    /** 是否为中途退出 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_exit;


    FPbSoloArenaQuickEndReq();
    FPbSoloArenaQuickEndReq(const idlepb::SoloArenaQuickEndReq& Right);
    void FromPb(const idlepb::SoloArenaQuickEndReq& Right);
    void ToPb(idlepb::SoloArenaQuickEndReq* Out) const;
    void Reset();
    void operator=(const idlepb::SoloArenaQuickEndReq& Right);
    bool operator==(const FPbSoloArenaQuickEndReq& Right) const;
    bool operator!=(const FPbSoloArenaQuickEndReq& Right) const;
     
};

namespace idlepb {
class SoloArenaQuickEndAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSoloArenaQuickEndAck
{
    GENERATED_BODY();


    FPbSoloArenaQuickEndAck();
    FPbSoloArenaQuickEndAck(const idlepb::SoloArenaQuickEndAck& Right);
    void FromPb(const idlepb::SoloArenaQuickEndAck& Right);
    void ToPb(idlepb::SoloArenaQuickEndAck* Out) const;
    void Reset();
    void operator=(const idlepb::SoloArenaQuickEndAck& Right);
    bool operator==(const FPbSoloArenaQuickEndAck& Right) const;
    bool operator!=(const FPbSoloArenaQuickEndAck& Right) const;
     
};

namespace idlepb {
class GetSoloArenaHistoryListReq;
}  // namespace idlepb

/**
 * 获取切磋历史列表
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetSoloArenaHistoryListReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSoloType type;


    FPbGetSoloArenaHistoryListReq();
    FPbGetSoloArenaHistoryListReq(const idlepb::GetSoloArenaHistoryListReq& Right);
    void FromPb(const idlepb::GetSoloArenaHistoryListReq& Right);
    void ToPb(idlepb::GetSoloArenaHistoryListReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetSoloArenaHistoryListReq& Right);
    bool operator==(const FPbGetSoloArenaHistoryListReq& Right) const;
    bool operator!=(const FPbGetSoloArenaHistoryListReq& Right) const;
     
};

namespace idlepb {
class GetSoloArenaHistoryListAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetSoloArenaHistoryListAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleBattleHistoryList data;


    FPbGetSoloArenaHistoryListAck();
    FPbGetSoloArenaHistoryListAck(const idlepb::GetSoloArenaHistoryListAck& Right);
    void FromPb(const idlepb::GetSoloArenaHistoryListAck& Right);
    void ToPb(idlepb::GetSoloArenaHistoryListAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetSoloArenaHistoryListAck& Right);
    bool operator==(const FPbGetSoloArenaHistoryListAck& Right) const;
    bool operator!=(const FPbGetSoloArenaHistoryListAck& Right) const;
     
};

namespace idlepb {
class ReplaySoloArenaHistoryReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReplaySoloArenaHistoryReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 history_world_id;


    FPbReplaySoloArenaHistoryReq();
    FPbReplaySoloArenaHistoryReq(const idlepb::ReplaySoloArenaHistoryReq& Right);
    void FromPb(const idlepb::ReplaySoloArenaHistoryReq& Right);
    void ToPb(idlepb::ReplaySoloArenaHistoryReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReplaySoloArenaHistoryReq& Right);
    bool operator==(const FPbReplaySoloArenaHistoryReq& Right) const;
    bool operator!=(const FPbReplaySoloArenaHistoryReq& Right) const;
     
};

namespace idlepb {
class ReplaySoloArenaHistoryAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReplaySoloArenaHistoryAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCompressedData data;


    FPbReplaySoloArenaHistoryAck();
    FPbReplaySoloArenaHistoryAck(const idlepb::ReplaySoloArenaHistoryAck& Right);
    void FromPb(const idlepb::ReplaySoloArenaHistoryAck& Right);
    void ToPb(idlepb::ReplaySoloArenaHistoryAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReplaySoloArenaHistoryAck& Right);
    bool operator==(const FPbReplaySoloArenaHistoryAck& Right) const;
    bool operator!=(const FPbReplaySoloArenaHistoryAck& Right) const;
     
};

namespace idlepb {
class GetBattleHistoryInfoReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetBattleHistoryInfoReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 history_world_id;


    FPbGetBattleHistoryInfoReq();
    FPbGetBattleHistoryInfoReq(const idlepb::GetBattleHistoryInfoReq& Right);
    void FromPb(const idlepb::GetBattleHistoryInfoReq& Right);
    void ToPb(idlepb::GetBattleHistoryInfoReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetBattleHistoryInfoReq& Right);
    bool operator==(const FPbGetBattleHistoryInfoReq& Right) const;
    bool operator!=(const FPbGetBattleHistoryInfoReq& Right) const;
     
};

namespace idlepb {
class GetBattleHistoryInfoAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetBattleHistoryInfoAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleInfo info;


    FPbGetBattleHistoryInfoAck();
    FPbGetBattleHistoryInfoAck(const idlepb::GetBattleHistoryInfoAck& Right);
    void FromPb(const idlepb::GetBattleHistoryInfoAck& Right);
    void ToPb(idlepb::GetBattleHistoryInfoAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetBattleHistoryInfoAck& Right);
    bool operator==(const FPbGetBattleHistoryInfoAck& Right) const;
    bool operator!=(const FPbGetBattleHistoryInfoAck& Right) const;
     
};

namespace idlepb {
class NotifyEnterOpenClientWorld;
}  // namespace idlepb

/**
 * 请求客户端进入客户端地图
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyEnterOpenClientWorld
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 world_cfg_id;


    FPbNotifyEnterOpenClientWorld();
    FPbNotifyEnterOpenClientWorld(const idlepb::NotifyEnterOpenClientWorld& Right);
    void FromPb(const idlepb::NotifyEnterOpenClientWorld& Right);
    void ToPb(idlepb::NotifyEnterOpenClientWorld* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyEnterOpenClientWorld& Right);
    bool operator==(const FPbNotifyEnterOpenClientWorld& Right) const;
    bool operator!=(const FPbNotifyEnterOpenClientWorld& Right) const;
     
};

namespace idlepb {
class NotifyMonsterTowerData;
}  // namespace idlepb

/**
 * 更新镇妖塔数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyMonsterTowerData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleMonsterTowerData data;


    FPbNotifyMonsterTowerData();
    FPbNotifyMonsterTowerData(const idlepb::NotifyMonsterTowerData& Right);
    void FromPb(const idlepb::NotifyMonsterTowerData& Right);
    void ToPb(idlepb::NotifyMonsterTowerData* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyMonsterTowerData& Right);
    bool operator==(const FPbNotifyMonsterTowerData& Right) const;
    bool operator!=(const FPbNotifyMonsterTowerData& Right) const;
     
};

namespace idlepb {
class NotifyMonsterTowerChallengeOver;
}  // namespace idlepb

/**
 * 镇妖塔挑战结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyMonsterTowerChallengeOver
{
    GENERATED_BODY();

    /** 挑战层数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 floor;

    /** 是否挑战成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool win;


    FPbNotifyMonsterTowerChallengeOver();
    FPbNotifyMonsterTowerChallengeOver(const idlepb::NotifyMonsterTowerChallengeOver& Right);
    void FromPb(const idlepb::NotifyMonsterTowerChallengeOver& Right);
    void ToPb(idlepb::NotifyMonsterTowerChallengeOver* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyMonsterTowerChallengeOver& Right);
    bool operator==(const FPbNotifyMonsterTowerChallengeOver& Right) const;
    bool operator!=(const FPbNotifyMonsterTowerChallengeOver& Right) const;
     
};

namespace idlepb {
class MonsterTowerChallengeReq;
}  // namespace idlepb

/**
 * 挑战镇妖塔
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerChallengeReq
{
    GENERATED_BODY();


    FPbMonsterTowerChallengeReq();
    FPbMonsterTowerChallengeReq(const idlepb::MonsterTowerChallengeReq& Right);
    void FromPb(const idlepb::MonsterTowerChallengeReq& Right);
    void ToPb(idlepb::MonsterTowerChallengeReq* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerChallengeReq& Right);
    bool operator==(const FPbMonsterTowerChallengeReq& Right) const;
    bool operator!=(const FPbMonsterTowerChallengeReq& Right) const;
     
};

namespace idlepb {
class MonsterTowerChallengeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerChallengeAck
{
    GENERATED_BODY();


    FPbMonsterTowerChallengeAck();
    FPbMonsterTowerChallengeAck(const idlepb::MonsterTowerChallengeAck& Right);
    void FromPb(const idlepb::MonsterTowerChallengeAck& Right);
    void ToPb(idlepb::MonsterTowerChallengeAck* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerChallengeAck& Right);
    bool operator==(const FPbMonsterTowerChallengeAck& Right) const;
    bool operator!=(const FPbMonsterTowerChallengeAck& Right) const;
     
};

namespace idlepb {
class MonsterTowerDrawIdleAwardReq;
}  // namespace idlepb

/**
 * 领取镇妖塔挂机奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerDrawIdleAwardReq
{
    GENERATED_BODY();


    FPbMonsterTowerDrawIdleAwardReq();
    FPbMonsterTowerDrawIdleAwardReq(const idlepb::MonsterTowerDrawIdleAwardReq& Right);
    void FromPb(const idlepb::MonsterTowerDrawIdleAwardReq& Right);
    void ToPb(idlepb::MonsterTowerDrawIdleAwardReq* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerDrawIdleAwardReq& Right);
    bool operator==(const FPbMonsterTowerDrawIdleAwardReq& Right) const;
    bool operator!=(const FPbMonsterTowerDrawIdleAwardReq& Right) const;
     
};

namespace idlepb {
class MonsterTowerDrawIdleAwardAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerDrawIdleAwardAck
{
    GENERATED_BODY();


    FPbMonsterTowerDrawIdleAwardAck();
    FPbMonsterTowerDrawIdleAwardAck(const idlepb::MonsterTowerDrawIdleAwardAck& Right);
    void FromPb(const idlepb::MonsterTowerDrawIdleAwardAck& Right);
    void ToPb(idlepb::MonsterTowerDrawIdleAwardAck* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerDrawIdleAwardAck& Right);
    bool operator==(const FPbMonsterTowerDrawIdleAwardAck& Right) const;
    bool operator!=(const FPbMonsterTowerDrawIdleAwardAck& Right) const;
     
};

namespace idlepb {
class MonsterTowerClosedDoorTrainingReq;
}  // namespace idlepb

/**
 * 镇妖塔闭关
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerClosedDoorTrainingReq
{
    GENERATED_BODY();


    FPbMonsterTowerClosedDoorTrainingReq();
    FPbMonsterTowerClosedDoorTrainingReq(const idlepb::MonsterTowerClosedDoorTrainingReq& Right);
    void FromPb(const idlepb::MonsterTowerClosedDoorTrainingReq& Right);
    void ToPb(idlepb::MonsterTowerClosedDoorTrainingReq* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerClosedDoorTrainingReq& Right);
    bool operator==(const FPbMonsterTowerClosedDoorTrainingReq& Right) const;
    bool operator!=(const FPbMonsterTowerClosedDoorTrainingReq& Right) const;
     
};

namespace idlepb {
class MonsterTowerClosedDoorTrainingAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerClosedDoorTrainingAck
{
    GENERATED_BODY();


    FPbMonsterTowerClosedDoorTrainingAck();
    FPbMonsterTowerClosedDoorTrainingAck(const idlepb::MonsterTowerClosedDoorTrainingAck& Right);
    void FromPb(const idlepb::MonsterTowerClosedDoorTrainingAck& Right);
    void ToPb(idlepb::MonsterTowerClosedDoorTrainingAck* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerClosedDoorTrainingAck& Right);
    bool operator==(const FPbMonsterTowerClosedDoorTrainingAck& Right) const;
    bool operator!=(const FPbMonsterTowerClosedDoorTrainingAck& Right) const;
     
};

namespace idlepb {
class MonsterTowerQuickEndReq;
}  // namespace idlepb

/**
 * 请求快速结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerQuickEndReq
{
    GENERATED_BODY();

    /** 是否为中途退出 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_exit;


    FPbMonsterTowerQuickEndReq();
    FPbMonsterTowerQuickEndReq(const idlepb::MonsterTowerQuickEndReq& Right);
    void FromPb(const idlepb::MonsterTowerQuickEndReq& Right);
    void ToPb(idlepb::MonsterTowerQuickEndReq* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerQuickEndReq& Right);
    bool operator==(const FPbMonsterTowerQuickEndReq& Right) const;
    bool operator!=(const FPbMonsterTowerQuickEndReq& Right) const;
     
};

namespace idlepb {
class MonsterTowerQuickEndAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMonsterTowerQuickEndAck
{
    GENERATED_BODY();


    FPbMonsterTowerQuickEndAck();
    FPbMonsterTowerQuickEndAck(const idlepb::MonsterTowerQuickEndAck& Right);
    void FromPb(const idlepb::MonsterTowerQuickEndAck& Right);
    void ToPb(idlepb::MonsterTowerQuickEndAck* Out) const;
    void Reset();
    void operator=(const idlepb::MonsterTowerQuickEndAck& Right);
    bool operator==(const FPbMonsterTowerQuickEndAck& Right) const;
    bool operator!=(const FPbMonsterTowerQuickEndAck& Right) const;
     
};

namespace idlepb {
class NotifyFightModeData;
}  // namespace idlepb

/**
 * 更新战斗模式数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyFightModeData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFightModeData data;


    FPbNotifyFightModeData();
    FPbNotifyFightModeData(const idlepb::NotifyFightModeData& Right);
    void FromPb(const idlepb::NotifyFightModeData& Right);
    void ToPb(idlepb::NotifyFightModeData* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyFightModeData& Right);
    bool operator==(const FPbNotifyFightModeData& Right) const;
    bool operator!=(const FPbNotifyFightModeData& Right) const;
     
};


/**
*/
UENUM(BlueprintType)
enum class EPbSetFightModeAckErrorCode : uint8
{
    SetFightModeAckErrorCode_Ok = 0 UMETA(DisplayName="成功"),
    SetFightModeAckErrorCode_Other = 1 UMETA(DisplayName="其它错误"),
    SetFightModeAckErrorCode_RankInvalid = 2 UMETA(DisplayName="等级错误"),
    SetFightModeAckErrorCode_ModeInvalid = 3 UMETA(DisplayName="该模式不能在当前地图使用"),
    SetFightModeAckErrorCode_FightTime = 4 UMETA(DisplayName="战斗时间错误"),
};
constexpr EPbSetFightModeAckErrorCode EPbSetFightModeAckErrorCode_Min = EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_Ok;
constexpr EPbSetFightModeAckErrorCode EPbSetFightModeAckErrorCode_Max = EPbSetFightModeAckErrorCode::SetFightModeAckErrorCode_FightTime;
constexpr int32 EPbSetFightModeAckErrorCode_ArraySize = static_cast<int32>(EPbSetFightModeAckErrorCode_Max) + 1;
MPROTOCOL_API bool CheckEPbSetFightModeAckErrorCodeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSetFightModeAckErrorCodeDescription(EPbSetFightModeAckErrorCode Val);

template <typename Char>
struct fmt::formatter<EPbSetFightModeAckErrorCode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSetFightModeAckErrorCode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class SetFightModeReq;
}  // namespace idlepb

/**
 * 设置战斗模式
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetFightModeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFightMode mode;


    FPbSetFightModeReq();
    FPbSetFightModeReq(const idlepb::SetFightModeReq& Right);
    void FromPb(const idlepb::SetFightModeReq& Right);
    void ToPb(idlepb::SetFightModeReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetFightModeReq& Right);
    bool operator==(const FPbSetFightModeReq& Right) const;
    bool operator!=(const FPbSetFightModeReq& Right) const;
     
};

namespace idlepb {
class SetFightModeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetFightModeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSetFightModeAckErrorCode error_code;


    FPbSetFightModeAck();
    FPbSetFightModeAck(const idlepb::SetFightModeAck& Right);
    void FromPb(const idlepb::SetFightModeAck& Right);
    void ToPb(idlepb::SetFightModeAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetFightModeAck& Right);
    bool operator==(const FPbSetFightModeAck& Right) const;
    bool operator!=(const FPbSetFightModeAck& Right) const;
     
};

namespace idlepb {
class NotifyInventorySpaceNum;
}  // namespace idlepb

/**
 * 更新背包空间
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyInventorySpaceNum
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbNotifyInventorySpaceNum();
    FPbNotifyInventorySpaceNum(const idlepb::NotifyInventorySpaceNum& Right);
    void FromPb(const idlepb::NotifyInventorySpaceNum& Right);
    void ToPb(idlepb::NotifyInventorySpaceNum* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyInventorySpaceNum& Right);
    bool operator==(const FPbNotifyInventorySpaceNum& Right) const;
    bool operator!=(const FPbNotifyInventorySpaceNum& Right) const;
     
};

namespace idlepb {
class NotifyInventoryFullMailItem;
}  // namespace idlepb

/**
 * 通知背包已经满，道具经邮件发送
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyInventoryFullMailItem
{
    GENERATED_BODY();


    FPbNotifyInventoryFullMailItem();
    FPbNotifyInventoryFullMailItem(const idlepb::NotifyInventoryFullMailItem& Right);
    void FromPb(const idlepb::NotifyInventoryFullMailItem& Right);
    void ToPb(idlepb::NotifyInventoryFullMailItem* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyInventoryFullMailItem& Right);
    bool operator==(const FPbNotifyInventoryFullMailItem& Right) const;
    bool operator!=(const FPbNotifyInventoryFullMailItem& Right) const;
     
};

namespace idlepb {
class NotifyQiCollectorRank;
}  // namespace idlepb

/**
 * 更新聚灵阵等级
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyQiCollectorRank
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;


    FPbNotifyQiCollectorRank();
    FPbNotifyQiCollectorRank(const idlepb::NotifyQiCollectorRank& Right);
    void FromPb(const idlepb::NotifyQiCollectorRank& Right);
    void ToPb(idlepb::NotifyQiCollectorRank* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyQiCollectorRank& Right);
    bool operator==(const FPbNotifyQiCollectorRank& Right) const;
    bool operator!=(const FPbNotifyQiCollectorRank& Right) const;
     
};

namespace idlepb {
class UpgradeQiCollectorReq;
}  // namespace idlepb

/**
 * 升级聚灵阵
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpgradeQiCollectorReq
{
    GENERATED_BODY();


    FPbUpgradeQiCollectorReq();
    FPbUpgradeQiCollectorReq(const idlepb::UpgradeQiCollectorReq& Right);
    void FromPb(const idlepb::UpgradeQiCollectorReq& Right);
    void ToPb(idlepb::UpgradeQiCollectorReq* Out) const;
    void Reset();
    void operator=(const idlepb::UpgradeQiCollectorReq& Right);
    bool operator==(const FPbUpgradeQiCollectorReq& Right) const;
    bool operator!=(const FPbUpgradeQiCollectorReq& Right) const;
     
};

namespace idlepb {
class UpgradeQiCollectorAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpgradeQiCollectorAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUpgradeQiCollectorAck();
    FPbUpgradeQiCollectorAck(const idlepb::UpgradeQiCollectorAck& Right);
    void FromPb(const idlepb::UpgradeQiCollectorAck& Right);
    void ToPb(idlepb::UpgradeQiCollectorAck* Out) const;
    void Reset();
    void operator=(const idlepb::UpgradeQiCollectorAck& Right);
    bool operator==(const FPbUpgradeQiCollectorAck& Right) const;
    bool operator!=(const FPbUpgradeQiCollectorAck& Right) const;
     
};

namespace idlepb {
class GetRoleAllStatsReq;
}  // namespace idlepb

/**
 * 请求玩家的游戏数值数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleAllStatsReq
{
    GENERATED_BODY();


    FPbGetRoleAllStatsReq();
    FPbGetRoleAllStatsReq(const idlepb::GetRoleAllStatsReq& Right);
    void FromPb(const idlepb::GetRoleAllStatsReq& Right);
    void ToPb(idlepb::GetRoleAllStatsReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleAllStatsReq& Right);
    bool operator==(const FPbGetRoleAllStatsReq& Right) const;
    bool operator!=(const FPbGetRoleAllStatsReq& Right) const;
     
};

namespace idlepb {
class GetRoleAllStatsAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleAllStatsAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsAllModuleData all_stats_data;


    FPbGetRoleAllStatsAck();
    FPbGetRoleAllStatsAck(const idlepb::GetRoleAllStatsAck& Right);
    void FromPb(const idlepb::GetRoleAllStatsAck& Right);
    void ToPb(idlepb::GetRoleAllStatsAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleAllStatsAck& Right);
    bool operator==(const FPbGetRoleAllStatsAck& Right) const;
    bool operator!=(const FPbGetRoleAllStatsAck& Right) const;
     
};

namespace idlepb {
class GetShanhetuDataReq;
}  // namespace idlepb

/**
 * 请求玩家山河图数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShanhetuDataReq
{
    GENERATED_BODY();


    FPbGetShanhetuDataReq();
    FPbGetShanhetuDataReq(const idlepb::GetShanhetuDataReq& Right);
    void FromPb(const idlepb::GetShanhetuDataReq& Right);
    void ToPb(idlepb::GetShanhetuDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetShanhetuDataReq& Right);
    bool operator==(const FPbGetShanhetuDataReq& Right) const;
    bool operator!=(const FPbGetShanhetuDataReq& Right) const;
     
};

namespace idlepb {
class GetShanhetuDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShanhetuDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleShanhetuData data;


    FPbGetShanhetuDataAck();
    FPbGetShanhetuDataAck(const idlepb::GetShanhetuDataAck& Right);
    void FromPb(const idlepb::GetShanhetuDataAck& Right);
    void ToPb(idlepb::GetShanhetuDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetShanhetuDataAck& Right);
    bool operator==(const FPbGetShanhetuDataAck& Right) const;
    bool operator!=(const FPbGetShanhetuDataAck& Right) const;
     
};

namespace idlepb {
class SetShanhetuUseConfigReq;
}  // namespace idlepb

/**
 * 请求修改山河图使用配置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetShanhetuUseConfigReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_skip_green;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_skip_blue;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_skip_perpo;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_skip_gold;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_skip_red;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 auto_select;


    FPbSetShanhetuUseConfigReq();
    FPbSetShanhetuUseConfigReq(const idlepb::SetShanhetuUseConfigReq& Right);
    void FromPb(const idlepb::SetShanhetuUseConfigReq& Right);
    void ToPb(idlepb::SetShanhetuUseConfigReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetShanhetuUseConfigReq& Right);
    bool operator==(const FPbSetShanhetuUseConfigReq& Right) const;
    bool operator!=(const FPbSetShanhetuUseConfigReq& Right) const;
     
};

namespace idlepb {
class SetShanhetuUseConfigAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetShanhetuUseConfigAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSetShanhetuUseConfigAck();
    FPbSetShanhetuUseConfigAck(const idlepb::SetShanhetuUseConfigAck& Right);
    void FromPb(const idlepb::SetShanhetuUseConfigAck& Right);
    void ToPb(idlepb::SetShanhetuUseConfigAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetShanhetuUseConfigAck& Right);
    bool operator==(const FPbSetShanhetuUseConfigAck& Right) const;
    bool operator!=(const FPbSetShanhetuUseConfigAck& Right) const;
     
};

namespace idlepb {
class UseShanhetuReq;
}  // namespace idlepb

/**
 * 请求使用山河图
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseShanhetuReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool skip;

    /** 使用数量大于1时判定为批量使用 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbUseShanhetuReq();
    FPbUseShanhetuReq(const idlepb::UseShanhetuReq& Right);
    void FromPb(const idlepb::UseShanhetuReq& Right);
    void ToPb(idlepb::UseShanhetuReq* Out) const;
    void Reset();
    void operator=(const idlepb::UseShanhetuReq& Right);
    bool operator==(const FPbUseShanhetuReq& Right) const;
    bool operator!=(const FPbUseShanhetuReq& Right) const;
     
};

namespace idlepb {
class UseShanhetuAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUseShanhetuAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 跳过使用，直接返回一组道具结果 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;

    /** 不跳过使用，则返回一张地图 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuMap map;


    FPbUseShanhetuAck();
    FPbUseShanhetuAck(const idlepb::UseShanhetuAck& Right);
    void FromPb(const idlepb::UseShanhetuAck& Right);
    void ToPb(idlepb::UseShanhetuAck* Out) const;
    void Reset();
    void operator=(const idlepb::UseShanhetuAck& Right);
    bool operator==(const FPbUseShanhetuAck& Right) const;
    bool operator!=(const FPbUseShanhetuAck& Right) const;
     
};

namespace idlepb {
class StepShanhetuReq;
}  // namespace idlepb

/**
 * 探索山河图
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbStepShanhetuReq
{
    GENERATED_BODY();

    /** 如果有事件，选择 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 choose_event;


    FPbStepShanhetuReq();
    FPbStepShanhetuReq(const idlepb::StepShanhetuReq& Right);
    void FromPb(const idlepb::StepShanhetuReq& Right);
    void ToPb(idlepb::StepShanhetuReq* Out) const;
    void Reset();
    void operator=(const idlepb::StepShanhetuReq& Right);
    bool operator==(const FPbStepShanhetuReq& Right) const;
    bool operator!=(const FPbStepShanhetuReq& Right) const;
     
};

namespace idlepb {
class StepShanhetuAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbStepShanhetuAck
{
    GENERATED_BODY();

    /** 是否探索完成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool done;

    /** 当前进度，如果进度不变则代表探索失败 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 current_row;

    /** 当前记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuRecord record;


    FPbStepShanhetuAck();
    FPbStepShanhetuAck(const idlepb::StepShanhetuAck& Right);
    void FromPb(const idlepb::StepShanhetuAck& Right);
    void ToPb(idlepb::StepShanhetuAck* Out) const;
    void Reset();
    void operator=(const idlepb::StepShanhetuAck& Right);
    bool operator==(const FPbStepShanhetuAck& Right) const;
    bool operator!=(const FPbStepShanhetuAck& Right) const;
     
};

namespace idlepb {
class GetShanhetuUseRecordReq;
}  // namespace idlepb

/**
 * 请求山河图记录
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShanhetuUseRecordReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 uid;


    FPbGetShanhetuUseRecordReq();
    FPbGetShanhetuUseRecordReq(const idlepb::GetShanhetuUseRecordReq& Right);
    void FromPb(const idlepb::GetShanhetuUseRecordReq& Right);
    void ToPb(idlepb::GetShanhetuUseRecordReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetShanhetuUseRecordReq& Right);
    bool operator==(const FPbGetShanhetuUseRecordReq& Right) const;
    bool operator!=(const FPbGetShanhetuUseRecordReq& Right) const;
     
};

namespace idlepb {
class GetShanhetuUseRecordAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShanhetuUseRecordAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuRecord record;


    FPbGetShanhetuUseRecordAck();
    FPbGetShanhetuUseRecordAck(const idlepb::GetShanhetuUseRecordAck& Right);
    void FromPb(const idlepb::GetShanhetuUseRecordAck& Right);
    void ToPb(idlepb::GetShanhetuUseRecordAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetShanhetuUseRecordAck& Right);
    bool operator==(const FPbGetShanhetuUseRecordAck& Right) const;
    bool operator!=(const FPbGetShanhetuUseRecordAck& Right) const;
     
};

namespace idlepb {
class SetAttackLockTypeReq;
}  // namespace idlepb

/**
 * 设置锁定方式
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetAttackLockTypeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAttackLockType type;


    FPbSetAttackLockTypeReq();
    FPbSetAttackLockTypeReq(const idlepb::SetAttackLockTypeReq& Right);
    void FromPb(const idlepb::SetAttackLockTypeReq& Right);
    void ToPb(idlepb::SetAttackLockTypeReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetAttackLockTypeReq& Right);
    bool operator==(const FPbSetAttackLockTypeReq& Right) const;
    bool operator!=(const FPbSetAttackLockTypeReq& Right) const;
     
};

namespace idlepb {
class SetAttackLockTypeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetAttackLockTypeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSetAttackLockTypeAck();
    FPbSetAttackLockTypeAck(const idlepb::SetAttackLockTypeAck& Right);
    void FromPb(const idlepb::SetAttackLockTypeAck& Right);
    void ToPb(idlepb::SetAttackLockTypeAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetAttackLockTypeAck& Right);
    bool operator==(const FPbSetAttackLockTypeAck& Right) const;
    bool operator!=(const FPbSetAttackLockTypeAck& Right) const;
     
};

namespace idlepb {
class SetAttackUnlockTypeReq;
}  // namespace idlepb

/**
 * 设置取消锁定方式
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetAttackUnlockTypeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAttackUnlockType type;


    FPbSetAttackUnlockTypeReq();
    FPbSetAttackUnlockTypeReq(const idlepb::SetAttackUnlockTypeReq& Right);
    void FromPb(const idlepb::SetAttackUnlockTypeReq& Right);
    void ToPb(idlepb::SetAttackUnlockTypeReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetAttackUnlockTypeReq& Right);
    bool operator==(const FPbSetAttackUnlockTypeReq& Right) const;
    bool operator!=(const FPbSetAttackUnlockTypeReq& Right) const;
     
};

namespace idlepb {
class SetAttackUnlockTypeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetAttackUnlockTypeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSetAttackUnlockTypeAck();
    FPbSetAttackUnlockTypeAck(const idlepb::SetAttackUnlockTypeAck& Right);
    void FromPb(const idlepb::SetAttackUnlockTypeAck& Right);
    void ToPb(idlepb::SetAttackUnlockTypeAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetAttackUnlockTypeAck& Right);
    bool operator==(const FPbSetAttackUnlockTypeAck& Right) const;
    bool operator!=(const FPbSetAttackUnlockTypeAck& Right) const;
     
};

namespace idlepb {
class SetShowUnlockButtonReq;
}  // namespace idlepb

/**
 * 设置是否显示解锁按钮
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetShowUnlockButtonReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool enable;


    FPbSetShowUnlockButtonReq();
    FPbSetShowUnlockButtonReq(const idlepb::SetShowUnlockButtonReq& Right);
    void FromPb(const idlepb::SetShowUnlockButtonReq& Right);
    void ToPb(idlepb::SetShowUnlockButtonReq* Out) const;
    void Reset();
    void operator=(const idlepb::SetShowUnlockButtonReq& Right);
    bool operator==(const FPbSetShowUnlockButtonReq& Right) const;
    bool operator!=(const FPbSetShowUnlockButtonReq& Right) const;
     
};

namespace idlepb {
class SetShowUnlockButtonAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetShowUnlockButtonAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSetShowUnlockButtonAck();
    FPbSetShowUnlockButtonAck(const idlepb::SetShowUnlockButtonAck& Right);
    void FromPb(const idlepb::SetShowUnlockButtonAck& Right);
    void ToPb(idlepb::SetShowUnlockButtonAck* Out) const;
    void Reset();
    void operator=(const idlepb::SetShowUnlockButtonAck& Right);
    bool operator==(const FPbSetShowUnlockButtonAck& Right) const;
    bool operator!=(const FPbSetShowUnlockButtonAck& Right) const;
     
};

namespace idlepb {
class RefreshRoleNormalSetting;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshRoleNormalSetting
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleNormalSettings settings;


    FPbRefreshRoleNormalSetting();
    FPbRefreshRoleNormalSetting(const idlepb::RefreshRoleNormalSetting& Right);
    void FromPb(const idlepb::RefreshRoleNormalSetting& Right);
    void ToPb(idlepb::RefreshRoleNormalSetting* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshRoleNormalSetting& Right);
    bool operator==(const FPbRefreshRoleNormalSetting& Right) const;
    bool operator!=(const FPbRefreshRoleNormalSetting& Right) const;
     
};

namespace idlepb {
class GetUserVarReq;
}  // namespace idlepb

/**
 * 获取用户变量内容
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetUserVarReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString var_name;


    FPbGetUserVarReq();
    FPbGetUserVarReq(const idlepb::GetUserVarReq& Right);
    void FromPb(const idlepb::GetUserVarReq& Right);
    void ToPb(idlepb::GetUserVarReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetUserVarReq& Right);
    bool operator==(const FPbGetUserVarReq& Right) const;
    bool operator!=(const FPbGetUserVarReq& Right) const;
     
};

namespace idlepb {
class GetUserVarRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetUserVarRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 var_value;


    FPbGetUserVarRsp();
    FPbGetUserVarRsp(const idlepb::GetUserVarRsp& Right);
    void FromPb(const idlepb::GetUserVarRsp& Right);
    void ToPb(idlepb::GetUserVarRsp* Out) const;
    void Reset();
    void operator=(const idlepb::GetUserVarRsp& Right);
    bool operator==(const FPbGetUserVarRsp& Right) const;
    bool operator!=(const FPbGetUserVarRsp& Right) const;
     
};

namespace idlepb {
class GetUserVarsReq;
}  // namespace idlepb

/**
 * 获取多个用户变量内容
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetUserVarsReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FString> var_name;


    FPbGetUserVarsReq();
    FPbGetUserVarsReq(const idlepb::GetUserVarsReq& Right);
    void FromPb(const idlepb::GetUserVarsReq& Right);
    void ToPb(idlepb::GetUserVarsReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetUserVarsReq& Right);
    bool operator==(const FPbGetUserVarsReq& Right) const;
    bool operator!=(const FPbGetUserVarsReq& Right) const;
     
};

namespace idlepb {
class GetUserVarsRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetUserVarsRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbStringKeyInt32ValueEntry> data;


    FPbGetUserVarsRsp();
    FPbGetUserVarsRsp(const idlepb::GetUserVarsRsp& Right);
    void FromPb(const idlepb::GetUserVarsRsp& Right);
    void ToPb(idlepb::GetUserVarsRsp* Out) const;
    void Reset();
    void operator=(const idlepb::GetUserVarsRsp& Right);
    bool operator==(const FPbGetUserVarsRsp& Right) const;
    bool operator!=(const FPbGetUserVarsRsp& Right) const;
     
};

namespace idlepb {
class SetUserVar;
}  // namespace idlepb

/**
 * 设置用户变量值
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSetUserVar
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString var_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 var_value;


    FPbSetUserVar();
    FPbSetUserVar(const idlepb::SetUserVar& Right);
    void FromPb(const idlepb::SetUserVar& Right);
    void ToPb(idlepb::SetUserVar* Out) const;
    void Reset();
    void operator=(const idlepb::SetUserVar& Right);
    bool operator==(const FPbSetUserVar& Right) const;
    bool operator!=(const FPbSetUserVar& Right) const;
     
};

namespace idlepb {
class DelUserVar;
}  // namespace idlepb

/**
 * 删除用户变量
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDelUserVar
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString var_name;


    FPbDelUserVar();
    FPbDelUserVar(const idlepb::DelUserVar& Right);
    void FromPb(const idlepb::DelUserVar& Right);
    void ToPb(idlepb::DelUserVar* Out) const;
    void Reset();
    void operator=(const idlepb::DelUserVar& Right);
    bool operator==(const FPbDelUserVar& Right) const;
    bool operator!=(const FPbDelUserVar& Right) const;
     
};

namespace idlepb {
class ShareSelfItemReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 item_id;


    FPbShareSelfItemReq();
    FPbShareSelfItemReq(const idlepb::ShareSelfItemReq& Right);
    void FromPb(const idlepb::ShareSelfItemReq& Right);
    void ToPb(idlepb::ShareSelfItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfItemReq& Right);
    bool operator==(const FPbShareSelfItemReq& Right) const;
    bool operator!=(const FPbShareSelfItemReq& Right) const;
     
};

namespace idlepb {
class ShareSelfItemRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfItemRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 share_id;


    FPbShareSelfItemRsp();
    FPbShareSelfItemRsp(const idlepb::ShareSelfItemRsp& Right);
    void FromPb(const idlepb::ShareSelfItemRsp& Right);
    void ToPb(idlepb::ShareSelfItemRsp* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfItemRsp& Right);
    bool operator==(const FPbShareSelfItemRsp& Right) const;
    bool operator!=(const FPbShareSelfItemRsp& Right) const;
     
};

namespace idlepb {
class ShareSelfItemsReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfItemsReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> item_id;


    FPbShareSelfItemsReq();
    FPbShareSelfItemsReq(const idlepb::ShareSelfItemsReq& Right);
    void FromPb(const idlepb::ShareSelfItemsReq& Right);
    void ToPb(idlepb::ShareSelfItemsReq* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfItemsReq& Right);
    bool operator==(const FPbShareSelfItemsReq& Right) const;
    bool operator!=(const FPbShareSelfItemsReq& Right) const;
     
};

namespace idlepb {
class ShareSelfItemsRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShareSelfItemsRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbInt64Pair> share_id;


    FPbShareSelfItemsRsp();
    FPbShareSelfItemsRsp(const idlepb::ShareSelfItemsRsp& Right);
    void FromPb(const idlepb::ShareSelfItemsRsp& Right);
    void ToPb(idlepb::ShareSelfItemsRsp* Out) const;
    void Reset();
    void operator=(const idlepb::ShareSelfItemsRsp& Right);
    bool operator==(const FPbShareSelfItemsRsp& Right) const;
    bool operator!=(const FPbShareSelfItemsRsp& Right) const;
     
};

namespace idlepb {
class GetShareItemDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShareItemDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 share_id;


    FPbGetShareItemDataReq();
    FPbGetShareItemDataReq(const idlepb::GetShareItemDataReq& Right);
    void FromPb(const idlepb::GetShareItemDataReq& Right);
    void ToPb(idlepb::GetShareItemDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetShareItemDataReq& Right);
    bool operator==(const FPbGetShareItemDataReq& Right) const;
    bool operator!=(const FPbGetShareItemDataReq& Right) const;
     
};

namespace idlepb {
class GetShareItemDataRsp;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetShareItemDataRsp
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData item_data;


    FPbGetShareItemDataRsp();
    FPbGetShareItemDataRsp(const idlepb::GetShareItemDataRsp& Right);
    void FromPb(const idlepb::GetShareItemDataRsp& Right);
    void ToPb(idlepb::GetShareItemDataRsp* Out) const;
    void Reset();
    void operator=(const idlepb::GetShareItemDataRsp& Right);
    bool operator==(const FPbGetShareItemDataRsp& Right) const;
    bool operator!=(const FPbGetShareItemDataRsp& Right) const;
     
};

namespace idlepb {
class GetChecklistDataReq;
}  // namespace idlepb

/**
 * 请求玩家福缘数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetChecklistDataReq
{
    GENERATED_BODY();


    FPbGetChecklistDataReq();
    FPbGetChecklistDataReq(const idlepb::GetChecklistDataReq& Right);
    void FromPb(const idlepb::GetChecklistDataReq& Right);
    void ToPb(idlepb::GetChecklistDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetChecklistDataReq& Right);
    bool operator==(const FPbGetChecklistDataReq& Right) const;
    bool operator!=(const FPbGetChecklistDataReq& Right) const;
     
};

namespace idlepb {
class GetChecklistDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetChecklistDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleChecklistData data;


    FPbGetChecklistDataAck();
    FPbGetChecklistDataAck(const idlepb::GetChecklistDataAck& Right);
    void FromPb(const idlepb::GetChecklistDataAck& Right);
    void ToPb(idlepb::GetChecklistDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetChecklistDataAck& Right);
    bool operator==(const FPbGetChecklistDataAck& Right) const;
    bool operator!=(const FPbGetChecklistDataAck& Right) const;
     
};

namespace idlepb {
class NotifyChecklist;
}  // namespace idlepb

/**
 * 通知福缘功能，有任务完成
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyChecklist
{
    GENERATED_BODY();


    FPbNotifyChecklist();
    FPbNotifyChecklist(const idlepb::NotifyChecklist& Right);
    void FromPb(const idlepb::NotifyChecklist& Right);
    void ToPb(idlepb::NotifyChecklist* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyChecklist& Right);
    bool operator==(const FPbNotifyChecklist& Right) const;
    bool operator!=(const FPbNotifyChecklist& Right) const;
     
};

namespace idlepb {
class ChecklistOpReq;
}  // namespace idlepb

/**
 * 福缘操作，提交任务、领取奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbChecklistOpReq
{
    GENERATED_BODY();

    /** 提交任务或领取奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool sumbmit_or_receive;

    /** 仅在领取奖励时有效 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool day_or_week;


    FPbChecklistOpReq();
    FPbChecklistOpReq(const idlepb::ChecklistOpReq& Right);
    void FromPb(const idlepb::ChecklistOpReq& Right);
    void ToPb(idlepb::ChecklistOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::ChecklistOpReq& Right);
    bool operator==(const FPbChecklistOpReq& Right) const;
    bool operator!=(const FPbChecklistOpReq& Right) const;
     
};

namespace idlepb {
class ChecklistOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbChecklistOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleChecklistData data;


    FPbChecklistOpAck();
    FPbChecklistOpAck(const idlepb::ChecklistOpAck& Right);
    void FromPb(const idlepb::ChecklistOpAck& Right);
    void ToPb(idlepb::ChecklistOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::ChecklistOpAck& Right);
    bool operator==(const FPbChecklistOpAck& Right) const;
    bool operator!=(const FPbChecklistOpAck& Right) const;
     
};

namespace idlepb {
class UpdateChecklistReq;
}  // namespace idlepb

/**
 * 福缘操作，提交任务进度、领取奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateChecklistReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;


    FPbUpdateChecklistReq();
    FPbUpdateChecklistReq(const idlepb::UpdateChecklistReq& Right);
    void FromPb(const idlepb::UpdateChecklistReq& Right);
    void ToPb(idlepb::UpdateChecklistReq* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateChecklistReq& Right);
    bool operator==(const FPbUpdateChecklistReq& Right) const;
    bool operator!=(const FPbUpdateChecklistReq& Right) const;
     
};

namespace idlepb {
class UpdateChecklistAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateChecklistAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbUpdateChecklistAck();
    FPbUpdateChecklistAck(const idlepb::UpdateChecklistAck& Right);
    void FromPb(const idlepb::UpdateChecklistAck& Right);
    void ToPb(idlepb::UpdateChecklistAck* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateChecklistAck& Right);
    bool operator==(const FPbUpdateChecklistAck& Right) const;
    bool operator!=(const FPbUpdateChecklistAck& Right) const;
     
};

namespace idlepb {
class GetCommonItemExchangeDataReq;
}  // namespace idlepb

/**
 * 获取玩家通用道具兑换数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetCommonItemExchangeDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;


    FPbGetCommonItemExchangeDataReq();
    FPbGetCommonItemExchangeDataReq(const idlepb::GetCommonItemExchangeDataReq& Right);
    void FromPb(const idlepb::GetCommonItemExchangeDataReq& Right);
    void ToPb(idlepb::GetCommonItemExchangeDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetCommonItemExchangeDataReq& Right);
    bool operator==(const FPbGetCommonItemExchangeDataReq& Right) const;
    bool operator!=(const FPbGetCommonItemExchangeDataReq& Right) const;
     
};

namespace idlepb {
class GetCommonItemExchangeDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetCommonItemExchangeDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_exchange_num;


    FPbGetCommonItemExchangeDataAck();
    FPbGetCommonItemExchangeDataAck(const idlepb::GetCommonItemExchangeDataAck& Right);
    void FromPb(const idlepb::GetCommonItemExchangeDataAck& Right);
    void ToPb(idlepb::GetCommonItemExchangeDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetCommonItemExchangeDataAck& Right);
    bool operator==(const FPbGetCommonItemExchangeDataAck& Right) const;
    bool operator!=(const FPbGetCommonItemExchangeDataAck& Right) const;
     
};

namespace idlepb {
class ExchangeCommonItemReq;
}  // namespace idlepb

/**
 * 请求兑换通用道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbExchangeCommonItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbExchangeCommonItemReq();
    FPbExchangeCommonItemReq(const idlepb::ExchangeCommonItemReq& Right);
    void FromPb(const idlepb::ExchangeCommonItemReq& Right);
    void ToPb(idlepb::ExchangeCommonItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::ExchangeCommonItemReq& Right);
    bool operator==(const FPbExchangeCommonItemReq& Right) const;
    bool operator!=(const FPbExchangeCommonItemReq& Right) const;
     
};

namespace idlepb {
class ExchangeCommonItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbExchangeCommonItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 out_num;


    FPbExchangeCommonItemAck();
    FPbExchangeCommonItemAck(const idlepb::ExchangeCommonItemAck& Right);
    void FromPb(const idlepb::ExchangeCommonItemAck& Right);
    void ToPb(idlepb::ExchangeCommonItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::ExchangeCommonItemAck& Right);
    bool operator==(const FPbExchangeCommonItemAck& Right) const;
    bool operator!=(const FPbExchangeCommonItemAck& Right) const;
     
};

namespace idlepb {
class SynthesisCommonItemReq;
}  // namespace idlepb

/**
 * 请求合成通用道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSynthesisCommonItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbSynthesisCommonItemReq();
    FPbSynthesisCommonItemReq(const idlepb::SynthesisCommonItemReq& Right);
    void FromPb(const idlepb::SynthesisCommonItemReq& Right);
    void ToPb(idlepb::SynthesisCommonItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::SynthesisCommonItemReq& Right);
    bool operator==(const FPbSynthesisCommonItemReq& Right) const;
    bool operator!=(const FPbSynthesisCommonItemReq& Right) const;
     
};

namespace idlepb {
class SynthesisCommonItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSynthesisCommonItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbSynthesisCommonItemAck();
    FPbSynthesisCommonItemAck(const idlepb::SynthesisCommonItemAck& Right);
    void FromPb(const idlepb::SynthesisCommonItemAck& Right);
    void ToPb(idlepb::SynthesisCommonItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::SynthesisCommonItemAck& Right);
    bool operator==(const FPbSynthesisCommonItemAck& Right) const;
    bool operator!=(const FPbSynthesisCommonItemAck& Right) const;
     
};

namespace idlepb {
class GetRoleSeptShopDataReq;
}  // namespace idlepb

/**
 * 请求玩家宗门商店数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleSeptShopDataReq
{
    GENERATED_BODY();


    FPbGetRoleSeptShopDataReq();
    FPbGetRoleSeptShopDataReq(const idlepb::GetRoleSeptShopDataReq& Right);
    void FromPb(const idlepb::GetRoleSeptShopDataReq& Right);
    void ToPb(idlepb::GetRoleSeptShopDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleSeptShopDataReq& Right);
    bool operator==(const FPbGetRoleSeptShopDataReq& Right) const;
    bool operator!=(const FPbGetRoleSeptShopDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleSeptShopDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleSeptShopDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptShopData data;


    FPbGetRoleSeptShopDataAck();
    FPbGetRoleSeptShopDataAck(const idlepb::GetRoleSeptShopDataAck& Right);
    void FromPb(const idlepb::GetRoleSeptShopDataAck& Right);
    void ToPb(idlepb::GetRoleSeptShopDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleSeptShopDataAck& Right);
    bool operator==(const FPbGetRoleSeptShopDataAck& Right) const;
    bool operator!=(const FPbGetRoleSeptShopDataAck& Right) const;
     
};

namespace idlepb {
class GetRoleSeptQuestDataReq;
}  // namespace idlepb

/**
 * 请求玩家宗门事务数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleSeptQuestDataReq
{
    GENERATED_BODY();


    FPbGetRoleSeptQuestDataReq();
    FPbGetRoleSeptQuestDataReq(const idlepb::GetRoleSeptQuestDataReq& Right);
    void FromPb(const idlepb::GetRoleSeptQuestDataReq& Right);
    void ToPb(idlepb::GetRoleSeptQuestDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleSeptQuestDataReq& Right);
    bool operator==(const FPbGetRoleSeptQuestDataReq& Right) const;
    bool operator!=(const FPbGetRoleSeptQuestDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleSeptQuestDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleSeptQuestDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptQuestData data;


    FPbGetRoleSeptQuestDataAck();
    FPbGetRoleSeptQuestDataAck(const idlepb::GetRoleSeptQuestDataAck& Right);
    void FromPb(const idlepb::GetRoleSeptQuestDataAck& Right);
    void ToPb(idlepb::GetRoleSeptQuestDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleSeptQuestDataAck& Right);
    bool operator==(const FPbGetRoleSeptQuestDataAck& Right) const;
    bool operator!=(const FPbGetRoleSeptQuestDataAck& Right) const;
     
};

namespace idlepb {
class BuySeptShopItemReq;
}  // namespace idlepb

/**
 * 请求兑换宗门商店道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuySeptShopItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbBuySeptShopItemReq();
    FPbBuySeptShopItemReq(const idlepb::BuySeptShopItemReq& Right);
    void FromPb(const idlepb::BuySeptShopItemReq& Right);
    void ToPb(idlepb::BuySeptShopItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::BuySeptShopItemReq& Right);
    bool operator==(const FPbBuySeptShopItemReq& Right) const;
    bool operator!=(const FPbBuySeptShopItemReq& Right) const;
     
};

namespace idlepb {
class BuySeptShopItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBuySeptShopItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbBuySeptShopItemAck();
    FPbBuySeptShopItemAck(const idlepb::BuySeptShopItemAck& Right);
    void FromPb(const idlepb::BuySeptShopItemAck& Right);
    void ToPb(idlepb::BuySeptShopItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::BuySeptShopItemAck& Right);
    bool operator==(const FPbBuySeptShopItemAck& Right) const;
    bool operator!=(const FPbBuySeptShopItemAck& Right) const;
     
};

namespace idlepb {
class ReqRoleSeptQuestOpReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReqRoleSeptQuestOpReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;


    FPbReqRoleSeptQuestOpReq();
    FPbReqRoleSeptQuestOpReq(const idlepb::ReqRoleSeptQuestOpReq& Right);
    void FromPb(const idlepb::ReqRoleSeptQuestOpReq& Right);
    void ToPb(idlepb::ReqRoleSeptQuestOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReqRoleSeptQuestOpReq& Right);
    bool operator==(const FPbReqRoleSeptQuestOpReq& Right) const;
    bool operator!=(const FPbReqRoleSeptQuestOpReq& Right) const;
     
};

namespace idlepb {
class ReqRoleSeptQuestOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReqRoleSeptQuestOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbReqRoleSeptQuestOpAck();
    FPbReqRoleSeptQuestOpAck(const idlepb::ReqRoleSeptQuestOpAck& Right);
    void FromPb(const idlepb::ReqRoleSeptQuestOpAck& Right);
    void ToPb(idlepb::ReqRoleSeptQuestOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReqRoleSeptQuestOpAck& Right);
    bool operator==(const FPbReqRoleSeptQuestOpAck& Right) const;
    bool operator!=(const FPbReqRoleSeptQuestOpAck& Right) const;
     
};

namespace idlepb {
class RefreshSeptQuestReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshSeptQuestReq
{
    GENERATED_BODY();


    FPbRefreshSeptQuestReq();
    FPbRefreshSeptQuestReq(const idlepb::RefreshSeptQuestReq& Right);
    void FromPb(const idlepb::RefreshSeptQuestReq& Right);
    void ToPb(idlepb::RefreshSeptQuestReq* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshSeptQuestReq& Right);
    bool operator==(const FPbRefreshSeptQuestReq& Right) const;
    bool operator!=(const FPbRefreshSeptQuestReq& Right) const;
     
};

namespace idlepb {
class RefreshSeptQuestAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshSeptQuestAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptQuestData data;


    FPbRefreshSeptQuestAck();
    FPbRefreshSeptQuestAck(const idlepb::RefreshSeptQuestAck& Right);
    void FromPb(const idlepb::RefreshSeptQuestAck& Right);
    void ToPb(idlepb::RefreshSeptQuestAck* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshSeptQuestAck& Right);
    bool operator==(const FPbRefreshSeptQuestAck& Right) const;
    bool operator!=(const FPbRefreshSeptQuestAck& Right) const;
     
};

namespace idlepb {
class ReqSeptQuestRankUpReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReqSeptQuestRankUpReq
{
    GENERATED_BODY();


    FPbReqSeptQuestRankUpReq();
    FPbReqSeptQuestRankUpReq(const idlepb::ReqSeptQuestRankUpReq& Right);
    void FromPb(const idlepb::ReqSeptQuestRankUpReq& Right);
    void ToPb(idlepb::ReqSeptQuestRankUpReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReqSeptQuestRankUpReq& Right);
    bool operator==(const FPbReqSeptQuestRankUpReq& Right) const;
    bool operator!=(const FPbReqSeptQuestRankUpReq& Right) const;
     
};

namespace idlepb {
class ReqSeptQuestRankUpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReqSeptQuestRankUpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbReqSeptQuestRankUpAck();
    FPbReqSeptQuestRankUpAck(const idlepb::ReqSeptQuestRankUpAck& Right);
    void FromPb(const idlepb::ReqSeptQuestRankUpAck& Right);
    void ToPb(idlepb::ReqSeptQuestRankUpAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReqSeptQuestRankUpAck& Right);
    bool operator==(const FPbReqSeptQuestRankUpAck& Right) const;
    bool operator!=(const FPbReqSeptQuestRankUpAck& Right) const;
     
};

namespace idlepb {
class GetGongFaDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetGongFaDataReq
{
    GENERATED_BODY();


    FPbGetGongFaDataReq();
    FPbGetGongFaDataReq(const idlepb::GetGongFaDataReq& Right);
    void FromPb(const idlepb::GetGongFaDataReq& Right);
    void ToPb(idlepb::GetGongFaDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetGongFaDataReq& Right);
    bool operator==(const FPbGetGongFaDataReq& Right) const;
    bool operator!=(const FPbGetGongFaDataReq& Right) const;
     
};

namespace idlepb {
class GetGongFaDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetGongFaDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleGongFaData data;


    FPbGetGongFaDataAck();
    FPbGetGongFaDataAck(const idlepb::GetGongFaDataAck& Right);
    void FromPb(const idlepb::GetGongFaDataAck& Right);
    void ToPb(idlepb::GetGongFaDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetGongFaDataAck& Right);
    bool operator==(const FPbGetGongFaDataAck& Right) const;
    bool operator!=(const FPbGetGongFaDataAck& Right) const;
     
};

namespace idlepb {
class GongFaOpReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGongFaOpReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 gongfa_id;


    FPbGongFaOpReq();
    FPbGongFaOpReq(const idlepb::GongFaOpReq& Right);
    void FromPb(const idlepb::GongFaOpReq& Right);
    void ToPb(idlepb::GongFaOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::GongFaOpReq& Right);
    bool operator==(const FPbGongFaOpReq& Right) const;
    bool operator!=(const FPbGongFaOpReq& Right) const;
     
};

namespace idlepb {
class GongFaOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGongFaOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGongFaData gongfa_data;


    FPbGongFaOpAck();
    FPbGongFaOpAck(const idlepb::GongFaOpAck& Right);
    void FromPb(const idlepb::GongFaOpAck& Right);
    void ToPb(idlepb::GongFaOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::GongFaOpAck& Right);
    bool operator==(const FPbGongFaOpAck& Right) const;
    bool operator!=(const FPbGongFaOpAck& Right) const;
     
};

namespace idlepb {
class ActivateGongFaMaxEffectReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbActivateGongFaMaxEffectReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;


    FPbActivateGongFaMaxEffectReq();
    FPbActivateGongFaMaxEffectReq(const idlepb::ActivateGongFaMaxEffectReq& Right);
    void FromPb(const idlepb::ActivateGongFaMaxEffectReq& Right);
    void ToPb(idlepb::ActivateGongFaMaxEffectReq* Out) const;
    void Reset();
    void operator=(const idlepb::ActivateGongFaMaxEffectReq& Right);
    bool operator==(const FPbActivateGongFaMaxEffectReq& Right) const;
    bool operator!=(const FPbActivateGongFaMaxEffectReq& Right) const;
     
};

namespace idlepb {
class ActivateGongFaMaxEffectAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbActivateGongFaMaxEffectAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbActivateGongFaMaxEffectAck();
    FPbActivateGongFaMaxEffectAck(const idlepb::ActivateGongFaMaxEffectAck& Right);
    void FromPb(const idlepb::ActivateGongFaMaxEffectAck& Right);
    void ToPb(idlepb::ActivateGongFaMaxEffectAck* Out) const;
    void Reset();
    void operator=(const idlepb::ActivateGongFaMaxEffectAck& Right);
    bool operator==(const FPbActivateGongFaMaxEffectAck& Right) const;
    bool operator!=(const FPbActivateGongFaMaxEffectAck& Right) const;
     
};

namespace idlepb {
class ReceiveFuZengRewardsReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveFuZengRewardsReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFuZengType type;


    FPbReceiveFuZengRewardsReq();
    FPbReceiveFuZengRewardsReq(const idlepb::ReceiveFuZengRewardsReq& Right);
    void FromPb(const idlepb::ReceiveFuZengRewardsReq& Right);
    void ToPb(idlepb::ReceiveFuZengRewardsReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveFuZengRewardsReq& Right);
    bool operator==(const FPbReceiveFuZengRewardsReq& Right) const;
    bool operator!=(const FPbReceiveFuZengRewardsReq& Right) const;
     
};

namespace idlepb {
class ReceiveFuZengRewardsAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveFuZengRewardsAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbFuZengData data;


    FPbReceiveFuZengRewardsAck();
    FPbReceiveFuZengRewardsAck(const idlepb::ReceiveFuZengRewardsAck& Right);
    void FromPb(const idlepb::ReceiveFuZengRewardsAck& Right);
    void ToPb(idlepb::ReceiveFuZengRewardsAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveFuZengRewardsAck& Right);
    bool operator==(const FPbReceiveFuZengRewardsAck& Right) const;
    bool operator!=(const FPbReceiveFuZengRewardsAck& Right) const;
     
};

namespace idlepb {
class GetRoleFuZengDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleFuZengDataReq
{
    GENERATED_BODY();


    FPbGetRoleFuZengDataReq();
    FPbGetRoleFuZengDataReq(const idlepb::GetRoleFuZengDataReq& Right);
    void FromPb(const idlepb::GetRoleFuZengDataReq& Right);
    void ToPb(idlepb::GetRoleFuZengDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleFuZengDataReq& Right);
    bool operator==(const FPbGetRoleFuZengDataReq& Right) const;
    bool operator!=(const FPbGetRoleFuZengDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleFuZengDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleFuZengDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFuZengData data;


    FPbGetRoleFuZengDataAck();
    FPbGetRoleFuZengDataAck(const idlepb::GetRoleFuZengDataAck& Right);
    void FromPb(const idlepb::GetRoleFuZengDataAck& Right);
    void ToPb(idlepb::GetRoleFuZengDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleFuZengDataAck& Right);
    bool operator==(const FPbGetRoleFuZengDataAck& Right) const;
    bool operator!=(const FPbGetRoleFuZengDataAck& Right) const;
     
};

namespace idlepb {
class NotifyFuZeng;
}  // namespace idlepb

/**
 * 通知福缘功能，有任务完成
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyFuZeng
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFuZengType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;


    FPbNotifyFuZeng();
    FPbNotifyFuZeng(const idlepb::NotifyFuZeng& Right);
    void FromPb(const idlepb::NotifyFuZeng& Right);
    void ToPb(idlepb::NotifyFuZeng* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyFuZeng& Right);
    bool operator==(const FPbNotifyFuZeng& Right) const;
    bool operator!=(const FPbNotifyFuZeng& Right) const;
     
};

namespace idlepb {
class GetRoleTreasuryDataReq;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleTreasuryDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool dirty_flag;


    FPbGetRoleTreasuryDataReq();
    FPbGetRoleTreasuryDataReq(const idlepb::GetRoleTreasuryDataReq& Right);
    void FromPb(const idlepb::GetRoleTreasuryDataReq& Right);
    void ToPb(idlepb::GetRoleTreasuryDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleTreasuryDataReq& Right);
    bool operator==(const FPbGetRoleTreasuryDataReq& Right) const;
    bool operator!=(const FPbGetRoleTreasuryDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleTreasuryDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleTreasuryDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTreasurySaveData data;


    FPbGetRoleTreasuryDataAck();
    FPbGetRoleTreasuryDataAck(const idlepb::GetRoleTreasuryDataAck& Right);
    void FromPb(const idlepb::GetRoleTreasuryDataAck& Right);
    void ToPb(idlepb::GetRoleTreasuryDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleTreasuryDataAck& Right);
    bool operator==(const FPbGetRoleTreasuryDataAck& Right) const;
    bool operator!=(const FPbGetRoleTreasuryDataAck& Right) const;
     
};

namespace idlepb {
class OpenTreasuryChestReq;
}  // namespace idlepb

/**
 * 请求开箱
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOpenTreasuryChestReq
{
    GENERATED_BODY();

    /** 开箱类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 chest_type;

    /** 开启次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbOpenTreasuryChestReq();
    FPbOpenTreasuryChestReq(const idlepb::OpenTreasuryChestReq& Right);
    void FromPb(const idlepb::OpenTreasuryChestReq& Right);
    void ToPb(idlepb::OpenTreasuryChestReq* Out) const;
    void Reset();
    void operator=(const idlepb::OpenTreasuryChestReq& Right);
    bool operator==(const FPbOpenTreasuryChestReq& Right) const;
    bool operator!=(const FPbOpenTreasuryChestReq& Right) const;
     
};

namespace idlepb {
class OpenTreasuryChestAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOpenTreasuryChestAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbOpenTreasuryChestAck();
    FPbOpenTreasuryChestAck(const idlepb::OpenTreasuryChestAck& Right);
    void FromPb(const idlepb::OpenTreasuryChestAck& Right);
    void ToPb(idlepb::OpenTreasuryChestAck* Out) const;
    void Reset();
    void operator=(const idlepb::OpenTreasuryChestAck& Right);
    bool operator==(const FPbOpenTreasuryChestAck& Right) const;
    bool operator!=(const FPbOpenTreasuryChestAck& Right) const;
     
};

namespace idlepb {
class OneClickOpenTreasuryChestReq;
}  // namespace idlepb

/**
 * 请求一键全开箱
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickOpenTreasuryChestReq
{
    GENERATED_BODY();


    FPbOneClickOpenTreasuryChestReq();
    FPbOneClickOpenTreasuryChestReq(const idlepb::OneClickOpenTreasuryChestReq& Right);
    void FromPb(const idlepb::OneClickOpenTreasuryChestReq& Right);
    void ToPb(idlepb::OneClickOpenTreasuryChestReq* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickOpenTreasuryChestReq& Right);
    bool operator==(const FPbOneClickOpenTreasuryChestReq& Right) const;
    bool operator!=(const FPbOneClickOpenTreasuryChestReq& Right) const;
     
};

namespace idlepb {
class OneClickOpenTreasuryChestAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOneClickOpenTreasuryChestAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 返回新的开箱次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> today_open_times;

    /** 返回新的保底次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> guarantee_count;


    FPbOneClickOpenTreasuryChestAck();
    FPbOneClickOpenTreasuryChestAck(const idlepb::OneClickOpenTreasuryChestAck& Right);
    void FromPb(const idlepb::OneClickOpenTreasuryChestAck& Right);
    void ToPb(idlepb::OneClickOpenTreasuryChestAck* Out) const;
    void Reset();
    void operator=(const idlepb::OneClickOpenTreasuryChestAck& Right);
    bool operator==(const FPbOneClickOpenTreasuryChestAck& Right) const;
    bool operator!=(const FPbOneClickOpenTreasuryChestAck& Right) const;
     
};

namespace idlepb {
class OpenTreasuryGachaReq;
}  // namespace idlepb

/**
 * 请求探索卡池
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOpenTreasuryGachaReq
{
    GENERATED_BODY();

    /** 卡池类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 gacha_type;

    /** 探索次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbOpenTreasuryGachaReq();
    FPbOpenTreasuryGachaReq(const idlepb::OpenTreasuryGachaReq& Right);
    void FromPb(const idlepb::OpenTreasuryGachaReq& Right);
    void ToPb(idlepb::OpenTreasuryGachaReq* Out) const;
    void Reset();
    void operator=(const idlepb::OpenTreasuryGachaReq& Right);
    bool operator==(const FPbOpenTreasuryGachaReq& Right) const;
    bool operator!=(const FPbOpenTreasuryGachaReq& Right) const;
     
};

namespace idlepb {
class OpenTreasuryGachaAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOpenTreasuryGachaAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 是否消耗的免费次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool free;


    FPbOpenTreasuryGachaAck();
    FPbOpenTreasuryGachaAck(const idlepb::OpenTreasuryGachaAck& Right);
    void FromPb(const idlepb::OpenTreasuryGachaAck& Right);
    void ToPb(idlepb::OpenTreasuryGachaAck* Out) const;
    void Reset();
    void operator=(const idlepb::OpenTreasuryGachaAck& Right);
    bool operator==(const FPbOpenTreasuryGachaAck& Right) const;
    bool operator!=(const FPbOpenTreasuryGachaAck& Right) const;
     
};

namespace idlepb {
class RefreshTreasuryShopReq;
}  // namespace idlepb

/**
 * 请求刷新古修商店
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshTreasuryShopReq
{
    GENERATED_BODY();


    FPbRefreshTreasuryShopReq();
    FPbRefreshTreasuryShopReq(const idlepb::RefreshTreasuryShopReq& Right);
    void FromPb(const idlepb::RefreshTreasuryShopReq& Right);
    void ToPb(idlepb::RefreshTreasuryShopReq* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshTreasuryShopReq& Right);
    bool operator==(const FPbRefreshTreasuryShopReq& Right) const;
    bool operator!=(const FPbRefreshTreasuryShopReq& Right) const;
     
};

namespace idlepb {
class RefreshTreasuryShopAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRefreshTreasuryShopAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbTreasuryShopItem> items;


    FPbRefreshTreasuryShopAck();
    FPbRefreshTreasuryShopAck(const idlepb::RefreshTreasuryShopAck& Right);
    void FromPb(const idlepb::RefreshTreasuryShopAck& Right);
    void ToPb(idlepb::RefreshTreasuryShopAck* Out) const;
    void Reset();
    void operator=(const idlepb::RefreshTreasuryShopAck& Right);
    bool operator==(const FPbRefreshTreasuryShopAck& Right) const;
    bool operator!=(const FPbRefreshTreasuryShopAck& Right) const;
     
};

namespace idlepb {
class TreasuryShopBuyReq;
}  // namespace idlepb

/**
 * 请求古修商店中购买
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTreasuryShopBuyReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;


    FPbTreasuryShopBuyReq();
    FPbTreasuryShopBuyReq(const idlepb::TreasuryShopBuyReq& Right);
    void FromPb(const idlepb::TreasuryShopBuyReq& Right);
    void ToPb(idlepb::TreasuryShopBuyReq* Out) const;
    void Reset();
    void operator=(const idlepb::TreasuryShopBuyReq& Right);
    bool operator==(const FPbTreasuryShopBuyReq& Right) const;
    bool operator!=(const FPbTreasuryShopBuyReq& Right) const;
     
};

namespace idlepb {
class TreasuryShopBuyAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTreasuryShopBuyAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbTreasuryShopBuyAck();
    FPbTreasuryShopBuyAck(const idlepb::TreasuryShopBuyAck& Right);
    void FromPb(const idlepb::TreasuryShopBuyAck& Right);
    void ToPb(idlepb::TreasuryShopBuyAck* Out) const;
    void Reset();
    void operator=(const idlepb::TreasuryShopBuyAck& Right);
    bool operator==(const FPbTreasuryShopBuyAck& Right) const;
    bool operator!=(const FPbTreasuryShopBuyAck& Right) const;
     
};

namespace idlepb {
class GetLifeCounterDataReq;
}  // namespace idlepb

/**
 * 获取生涯计数器数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLifeCounterDataReq
{
    GENERATED_BODY();


    FPbGetLifeCounterDataReq();
    FPbGetLifeCounterDataReq(const idlepb::GetLifeCounterDataReq& Right);
    void FromPb(const idlepb::GetLifeCounterDataReq& Right);
    void ToPb(idlepb::GetLifeCounterDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetLifeCounterDataReq& Right);
    bool operator==(const FPbGetLifeCounterDataReq& Right) const;
    bool operator!=(const FPbGetLifeCounterDataReq& Right) const;
     
};

namespace idlepb {
class GetLifeCounterDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetLifeCounterDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLifeCounterData data;


    FPbGetLifeCounterDataAck();
    FPbGetLifeCounterDataAck(const idlepb::GetLifeCounterDataAck& Right);
    void FromPb(const idlepb::GetLifeCounterDataAck& Right);
    void ToPb(idlepb::GetLifeCounterDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetLifeCounterDataAck& Right);
    bool operator==(const FPbGetLifeCounterDataAck& Right) const;
    bool operator!=(const FPbGetLifeCounterDataAck& Right) const;
     
};

namespace idlepb {
class UpdateLifeCounter;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateLifeCounter
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 function_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 new_num;


    FPbUpdateLifeCounter();
    FPbUpdateLifeCounter(const idlepb::UpdateLifeCounter& Right);
    void FromPb(const idlepb::UpdateLifeCounter& Right);
    void ToPb(idlepb::UpdateLifeCounter* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateLifeCounter& Right);
    bool operator==(const FPbUpdateLifeCounter& Right) const;
    bool operator!=(const FPbUpdateLifeCounter& Right) const;
     
};

namespace idlepb {
class DoQuestFightReq;
}  // namespace idlepb

/**
 * 进行任务对战
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoQuestFightReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quest_id;


    FPbDoQuestFightReq();
    FPbDoQuestFightReq(const idlepb::DoQuestFightReq& Right);
    void FromPb(const idlepb::DoQuestFightReq& Right);
    void ToPb(idlepb::DoQuestFightReq* Out) const;
    void Reset();
    void operator=(const idlepb::DoQuestFightReq& Right);
    bool operator==(const FPbDoQuestFightReq& Right) const;
    bool operator!=(const FPbDoQuestFightReq& Right) const;
     
};

namespace idlepb {
class DoQuestFightAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoQuestFightAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDoQuestFightAck();
    FPbDoQuestFightAck(const idlepb::DoQuestFightAck& Right);
    void FromPb(const idlepb::DoQuestFightAck& Right);
    void ToPb(idlepb::DoQuestFightAck* Out) const;
    void Reset();
    void operator=(const idlepb::DoQuestFightAck& Right);
    bool operator==(const FPbDoQuestFightAck& Right) const;
    bool operator!=(const FPbDoQuestFightAck& Right) const;
     
};

namespace idlepb {
class QuestFightQuickEndReq;
}  // namespace idlepb

/**
 * 请求任务对战快速结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQuestFightQuickEndReq
{
    GENERATED_BODY();

    /** 是否为中途退出 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_exit;


    FPbQuestFightQuickEndReq();
    FPbQuestFightQuickEndReq(const idlepb::QuestFightQuickEndReq& Right);
    void FromPb(const idlepb::QuestFightQuickEndReq& Right);
    void ToPb(idlepb::QuestFightQuickEndReq* Out) const;
    void Reset();
    void operator=(const idlepb::QuestFightQuickEndReq& Right);
    bool operator==(const FPbQuestFightQuickEndReq& Right) const;
    bool operator!=(const FPbQuestFightQuickEndReq& Right) const;
     
};

namespace idlepb {
class QuestFightQuickEndAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQuestFightQuickEndAck
{
    GENERATED_BODY();


    FPbQuestFightQuickEndAck();
    FPbQuestFightQuickEndAck(const idlepb::QuestFightQuickEndAck& Right);
    void FromPb(const idlepb::QuestFightQuickEndAck& Right);
    void ToPb(idlepb::QuestFightQuickEndAck* Out) const;
    void Reset();
    void operator=(const idlepb::QuestFightQuickEndAck& Right);
    bool operator==(const FPbQuestFightQuickEndAck& Right) const;
    bool operator!=(const FPbQuestFightQuickEndAck& Right) const;
     
};

namespace idlepb {
class NotifyQuestFightChallengeOver;
}  // namespace idlepb

/**
 * 任务对战挑战结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyQuestFightChallengeOver
{
    GENERATED_BODY();

    /** 挑战任务id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quest_id;

    /** 是否挑战成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool win;


    FPbNotifyQuestFightChallengeOver();
    FPbNotifyQuestFightChallengeOver(const idlepb::NotifyQuestFightChallengeOver& Right);
    void FromPb(const idlepb::NotifyQuestFightChallengeOver& Right);
    void ToPb(idlepb::NotifyQuestFightChallengeOver* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyQuestFightChallengeOver& Right);
    bool operator==(const FPbNotifyQuestFightChallengeOver& Right) const;
    bool operator!=(const FPbNotifyQuestFightChallengeOver& Right) const;
     
};

namespace idlepb {
class GetAppearanceDataReq;
}  // namespace idlepb

/**
 * 请求外观数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetAppearanceDataReq
{
    GENERATED_BODY();


    FPbGetAppearanceDataReq();
    FPbGetAppearanceDataReq(const idlepb::GetAppearanceDataReq& Right);
    void FromPb(const idlepb::GetAppearanceDataReq& Right);
    void ToPb(idlepb::GetAppearanceDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetAppearanceDataReq& Right);
    bool operator==(const FPbGetAppearanceDataReq& Right) const;
    bool operator!=(const FPbGetAppearanceDataReq& Right) const;
     
};

namespace idlepb {
class GetAppearanceDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetAppearanceDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAppearanceData data;


    FPbGetAppearanceDataAck();
    FPbGetAppearanceDataAck(const idlepb::GetAppearanceDataAck& Right);
    void FromPb(const idlepb::GetAppearanceDataAck& Right);
    void ToPb(idlepb::GetAppearanceDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetAppearanceDataAck& Right);
    bool operator==(const FPbGetAppearanceDataAck& Right) const;
    bool operator!=(const FPbGetAppearanceDataAck& Right) const;
     
};

namespace idlepb {
class AppearanceAddReq;
}  // namespace idlepb

/**
 * 请求添加外观（使用包含外观的礼包道具）
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceAddReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;


    FPbAppearanceAddReq();
    FPbAppearanceAddReq(const idlepb::AppearanceAddReq& Right);
    void FromPb(const idlepb::AppearanceAddReq& Right);
    void ToPb(idlepb::AppearanceAddReq* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceAddReq& Right);
    bool operator==(const FPbAppearanceAddReq& Right) const;
    bool operator!=(const FPbAppearanceAddReq& Right) const;
     
};

namespace idlepb {
class AppearanceAddAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceAddAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAppearanceAddAck();
    FPbAppearanceAddAck(const idlepb::AppearanceAddAck& Right);
    void FromPb(const idlepb::AppearanceAddAck& Right);
    void ToPb(idlepb::AppearanceAddAck* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceAddAck& Right);
    bool operator==(const FPbAppearanceAddAck& Right) const;
    bool operator!=(const FPbAppearanceAddAck& Right) const;
     
};

namespace idlepb {
class AppearanceActiveReq;
}  // namespace idlepb

/**
 * 请求激活外观
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceActiveReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 group_id;


    FPbAppearanceActiveReq();
    FPbAppearanceActiveReq(const idlepb::AppearanceActiveReq& Right);
    void FromPb(const idlepb::AppearanceActiveReq& Right);
    void ToPb(idlepb::AppearanceActiveReq* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceActiveReq& Right);
    bool operator==(const FPbAppearanceActiveReq& Right) const;
    bool operator!=(const FPbAppearanceActiveReq& Right) const;
     
};

namespace idlepb {
class AppearanceActiveAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceActiveAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAppearanceActiveAck();
    FPbAppearanceActiveAck(const idlepb::AppearanceActiveAck& Right);
    void FromPb(const idlepb::AppearanceActiveAck& Right);
    void ToPb(idlepb::AppearanceActiveAck* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceActiveAck& Right);
    bool operator==(const FPbAppearanceActiveAck& Right) const;
    bool operator!=(const FPbAppearanceActiveAck& Right) const;
     
};

namespace idlepb {
class AppearanceWearReq;
}  // namespace idlepb

/**
 * 请求穿戴外观
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceWearReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 group_id;


    FPbAppearanceWearReq();
    FPbAppearanceWearReq(const idlepb::AppearanceWearReq& Right);
    void FromPb(const idlepb::AppearanceWearReq& Right);
    void ToPb(idlepb::AppearanceWearReq* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceWearReq& Right);
    bool operator==(const FPbAppearanceWearReq& Right) const;
    bool operator!=(const FPbAppearanceWearReq& Right) const;
     
};

namespace idlepb {
class AppearanceWearAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceWearAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAppearanceWearAck();
    FPbAppearanceWearAck(const idlepb::AppearanceWearAck& Right);
    void FromPb(const idlepb::AppearanceWearAck& Right);
    void ToPb(idlepb::AppearanceWearAck* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceWearAck& Right);
    bool operator==(const FPbAppearanceWearAck& Right) const;
    bool operator!=(const FPbAppearanceWearAck& Right) const;
     
};

namespace idlepb {
class AppearanceChangeSkTypeReq;
}  // namespace idlepb

/**
 * 请求修改外形
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceChangeSkTypeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sk_type;


    FPbAppearanceChangeSkTypeReq();
    FPbAppearanceChangeSkTypeReq(const idlepb::AppearanceChangeSkTypeReq& Right);
    void FromPb(const idlepb::AppearanceChangeSkTypeReq& Right);
    void ToPb(idlepb::AppearanceChangeSkTypeReq* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceChangeSkTypeReq& Right);
    bool operator==(const FPbAppearanceChangeSkTypeReq& Right) const;
    bool operator!=(const FPbAppearanceChangeSkTypeReq& Right) const;
     
};

namespace idlepb {
class AppearanceChangeSkTypeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceChangeSkTypeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAppearanceChangeSkTypeAck();
    FPbAppearanceChangeSkTypeAck(const idlepb::AppearanceChangeSkTypeAck& Right);
    void FromPb(const idlepb::AppearanceChangeSkTypeAck& Right);
    void ToPb(idlepb::AppearanceChangeSkTypeAck* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceChangeSkTypeAck& Right);
    bool operator==(const FPbAppearanceChangeSkTypeAck& Right) const;
    bool operator!=(const FPbAppearanceChangeSkTypeAck& Right) const;
     
};

namespace idlepb {
class AppearanceBuyReq;
}  // namespace idlepb

/**
 * 请求外观商店购买
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceBuyReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 shop_index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_index;


    FPbAppearanceBuyReq();
    FPbAppearanceBuyReq(const idlepb::AppearanceBuyReq& Right);
    void FromPb(const idlepb::AppearanceBuyReq& Right);
    void ToPb(idlepb::AppearanceBuyReq* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceBuyReq& Right);
    bool operator==(const FPbAppearanceBuyReq& Right) const;
    bool operator!=(const FPbAppearanceBuyReq& Right) const;
     
};

namespace idlepb {
class AppearanceBuyAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceBuyAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbAppearanceBuyAck();
    FPbAppearanceBuyAck(const idlepb::AppearanceBuyAck& Right);
    void FromPb(const idlepb::AppearanceBuyAck& Right);
    void ToPb(idlepb::AppearanceBuyAck* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceBuyAck& Right);
    bool operator==(const FPbAppearanceBuyAck& Right) const;
    bool operator!=(const FPbAppearanceBuyAck& Right) const;
     
};

namespace idlepb {
class GetArenaCheckListDataReq;
}  // namespace idlepb

/**
 * 请求秘境探索数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetArenaCheckListDataReq
{
    GENERATED_BODY();


    FPbGetArenaCheckListDataReq();
    FPbGetArenaCheckListDataReq(const idlepb::GetArenaCheckListDataReq& Right);
    void FromPb(const idlepb::GetArenaCheckListDataReq& Right);
    void ToPb(idlepb::GetArenaCheckListDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetArenaCheckListDataReq& Right);
    bool operator==(const FPbGetArenaCheckListDataReq& Right) const;
    bool operator!=(const FPbGetArenaCheckListDataReq& Right) const;
     
};

namespace idlepb {
class GetArenaCheckListDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetArenaCheckListDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleArenaCheckListData data;


    FPbGetArenaCheckListDataAck();
    FPbGetArenaCheckListDataAck(const idlepb::GetArenaCheckListDataAck& Right);
    void FromPb(const idlepb::GetArenaCheckListDataAck& Right);
    void ToPb(idlepb::GetArenaCheckListDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetArenaCheckListDataAck& Right);
    bool operator==(const FPbGetArenaCheckListDataAck& Right) const;
    bool operator!=(const FPbGetArenaCheckListDataAck& Right) const;
     
};

namespace idlepb {
class ArenaCheckListSubmitReq;
}  // namespace idlepb

/**
 * 请求提交秘境探索事件
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListSubmitReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 check_list_id;


    FPbArenaCheckListSubmitReq();
    FPbArenaCheckListSubmitReq(const idlepb::ArenaCheckListSubmitReq& Right);
    void FromPb(const idlepb::ArenaCheckListSubmitReq& Right);
    void ToPb(idlepb::ArenaCheckListSubmitReq* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListSubmitReq& Right);
    bool operator==(const FPbArenaCheckListSubmitReq& Right) const;
    bool operator!=(const FPbArenaCheckListSubmitReq& Right) const;
     
};

namespace idlepb {
class ArenaCheckListSubmitAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListSubmitAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbArenaCheckListData data;


    FPbArenaCheckListSubmitAck();
    FPbArenaCheckListSubmitAck(const idlepb::ArenaCheckListSubmitAck& Right);
    void FromPb(const idlepb::ArenaCheckListSubmitAck& Right);
    void ToPb(idlepb::ArenaCheckListSubmitAck* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListSubmitAck& Right);
    bool operator==(const FPbArenaCheckListSubmitAck& Right) const;
    bool operator!=(const FPbArenaCheckListSubmitAck& Right) const;
     
};

namespace idlepb {
class ArenaCheckListRewardSubmitReq;
}  // namespace idlepb

/**
 * 请求提交秘境探索奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListRewardSubmitReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 reward_id;


    FPbArenaCheckListRewardSubmitReq();
    FPbArenaCheckListRewardSubmitReq(const idlepb::ArenaCheckListRewardSubmitReq& Right);
    void FromPb(const idlepb::ArenaCheckListRewardSubmitReq& Right);
    void ToPb(idlepb::ArenaCheckListRewardSubmitReq* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListRewardSubmitReq& Right);
    bool operator==(const FPbArenaCheckListRewardSubmitReq& Right) const;
    bool operator!=(const FPbArenaCheckListRewardSubmitReq& Right) const;
     
};

namespace idlepb {
class ArenaCheckListRewardSubmitAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListRewardSubmitAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbArenaCheckListRewardData data;


    FPbArenaCheckListRewardSubmitAck();
    FPbArenaCheckListRewardSubmitAck(const idlepb::ArenaCheckListRewardSubmitAck& Right);
    void FromPb(const idlepb::ArenaCheckListRewardSubmitAck& Right);
    void ToPb(idlepb::ArenaCheckListRewardSubmitAck* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListRewardSubmitAck& Right);
    bool operator==(const FPbArenaCheckListRewardSubmitAck& Right) const;
    bool operator!=(const FPbArenaCheckListRewardSubmitAck& Right) const;
     
};

namespace idlepb {
class DungeonKillAllChallengeReq;
}  // namespace idlepb

/**
 * 发起剿灭副本
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllChallengeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 dungeon_uid_id;


    FPbDungeonKillAllChallengeReq();
    FPbDungeonKillAllChallengeReq(const idlepb::DungeonKillAllChallengeReq& Right);
    void FromPb(const idlepb::DungeonKillAllChallengeReq& Right);
    void ToPb(idlepb::DungeonKillAllChallengeReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllChallengeReq& Right);
    bool operator==(const FPbDungeonKillAllChallengeReq& Right) const;
    bool operator!=(const FPbDungeonKillAllChallengeReq& Right) const;
     
};

namespace idlepb {
class DungeonKillAllChallengeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllChallengeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonKillAllChallengeAck();
    FPbDungeonKillAllChallengeAck(const idlepb::DungeonKillAllChallengeAck& Right);
    void FromPb(const idlepb::DungeonKillAllChallengeAck& Right);
    void ToPb(idlepb::DungeonKillAllChallengeAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllChallengeAck& Right);
    bool operator==(const FPbDungeonKillAllChallengeAck& Right) const;
    bool operator!=(const FPbDungeonKillAllChallengeAck& Right) const;
     
};

namespace idlepb {
class DungeonKillAllQuickEndReq;
}  // namespace idlepb

/**
 * 请求剿灭副本快速结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllQuickEndReq
{
    GENERATED_BODY();

    /** 是否为中途退出 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_exit;


    FPbDungeonKillAllQuickEndReq();
    FPbDungeonKillAllQuickEndReq(const idlepb::DungeonKillAllQuickEndReq& Right);
    void FromPb(const idlepb::DungeonKillAllQuickEndReq& Right);
    void ToPb(idlepb::DungeonKillAllQuickEndReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllQuickEndReq& Right);
    bool operator==(const FPbDungeonKillAllQuickEndReq& Right) const;
    bool operator!=(const FPbDungeonKillAllQuickEndReq& Right) const;
     
};

namespace idlepb {
class DungeonKillAllQuickEndAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllQuickEndAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonKillAllQuickEndAck();
    FPbDungeonKillAllQuickEndAck(const idlepb::DungeonKillAllQuickEndAck& Right);
    void FromPb(const idlepb::DungeonKillAllQuickEndAck& Right);
    void ToPb(idlepb::DungeonKillAllQuickEndAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllQuickEndAck& Right);
    bool operator==(const FPbDungeonKillAllQuickEndAck& Right) const;
    bool operator!=(const FPbDungeonKillAllQuickEndAck& Right) const;
     
};

namespace idlepb {
class NotifyDungeonKillAllChallengeOver;
}  // namespace idlepb

/**
 * 剿灭副本挑战结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyDungeonKillAllChallengeOver
{
    GENERATED_BODY();

    /** 挑战Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /** 是否挑战成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool win;


    FPbNotifyDungeonKillAllChallengeOver();
    FPbNotifyDungeonKillAllChallengeOver(const idlepb::NotifyDungeonKillAllChallengeOver& Right);
    void FromPb(const idlepb::NotifyDungeonKillAllChallengeOver& Right);
    void ToPb(idlepb::NotifyDungeonKillAllChallengeOver* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyDungeonKillAllChallengeOver& Right);
    bool operator==(const FPbNotifyDungeonKillAllChallengeOver& Right) const;
    bool operator!=(const FPbNotifyDungeonKillAllChallengeOver& Right) const;
     
};

namespace idlepb {
class NotifyDungeonKillAllChallengeCurWaveNum;
}  // namespace idlepb

/**
 * 通知剿灭副本当前第几波
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyDungeonKillAllChallengeCurWaveNum
{
    GENERATED_BODY();

    /** 挑战Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /** 当前波数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 curnum;

    /** 最大波数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 maxnum;


    FPbNotifyDungeonKillAllChallengeCurWaveNum();
    FPbNotifyDungeonKillAllChallengeCurWaveNum(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right);
    void FromPb(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right);
    void ToPb(idlepb::NotifyDungeonKillAllChallengeCurWaveNum* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyDungeonKillAllChallengeCurWaveNum& Right);
    bool operator==(const FPbNotifyDungeonKillAllChallengeCurWaveNum& Right) const;
    bool operator!=(const FPbNotifyDungeonKillAllChallengeCurWaveNum& Right) const;
     
};

namespace idlepb {
class DungeonKillAllDataReq;
}  // namespace idlepb

/**
 * 询问剿灭副本是否完成
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ask_uid;


    FPbDungeonKillAllDataReq();
    FPbDungeonKillAllDataReq(const idlepb::DungeonKillAllDataReq& Right);
    void FromPb(const idlepb::DungeonKillAllDataReq& Right);
    void ToPb(idlepb::DungeonKillAllDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllDataReq& Right);
    bool operator==(const FPbDungeonKillAllDataReq& Right) const;
    bool operator!=(const FPbDungeonKillAllDataReq& Right) const;
     
};

namespace idlepb {
class DungeonKillAllDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonKillAllDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonKillAllDataAck();
    FPbDungeonKillAllDataAck(const idlepb::DungeonKillAllDataAck& Right);
    void FromPb(const idlepb::DungeonKillAllDataAck& Right);
    void ToPb(idlepb::DungeonKillAllDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonKillAllDataAck& Right);
    bool operator==(const FPbDungeonKillAllDataAck& Right) const;
    bool operator!=(const FPbDungeonKillAllDataAck& Right) const;
     
};

namespace idlepb {
class DungeonSurviveChallengeReq;
}  // namespace idlepb

/**
 * 发起生存副本
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveChallengeReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 dungeon_uid;


    FPbDungeonSurviveChallengeReq();
    FPbDungeonSurviveChallengeReq(const idlepb::DungeonSurviveChallengeReq& Right);
    void FromPb(const idlepb::DungeonSurviveChallengeReq& Right);
    void ToPb(idlepb::DungeonSurviveChallengeReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveChallengeReq& Right);
    bool operator==(const FPbDungeonSurviveChallengeReq& Right) const;
    bool operator!=(const FPbDungeonSurviveChallengeReq& Right) const;
     
};

namespace idlepb {
class DungeonSurviveChallengeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveChallengeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonSurviveChallengeAck();
    FPbDungeonSurviveChallengeAck(const idlepb::DungeonSurviveChallengeAck& Right);
    void FromPb(const idlepb::DungeonSurviveChallengeAck& Right);
    void ToPb(idlepb::DungeonSurviveChallengeAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveChallengeAck& Right);
    bool operator==(const FPbDungeonSurviveChallengeAck& Right) const;
    bool operator!=(const FPbDungeonSurviveChallengeAck& Right) const;
     
};

namespace idlepb {
class DungeonSurviveQuickEndReq;
}  // namespace idlepb

/**
 * 请求生存副本快速结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveQuickEndReq
{
    GENERATED_BODY();

    /** 是否为中途退出 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_exit;


    FPbDungeonSurviveQuickEndReq();
    FPbDungeonSurviveQuickEndReq(const idlepb::DungeonSurviveQuickEndReq& Right);
    void FromPb(const idlepb::DungeonSurviveQuickEndReq& Right);
    void ToPb(idlepb::DungeonSurviveQuickEndReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveQuickEndReq& Right);
    bool operator==(const FPbDungeonSurviveQuickEndReq& Right) const;
    bool operator!=(const FPbDungeonSurviveQuickEndReq& Right) const;
     
};

namespace idlepb {
class DungeonSurviveQuickEndAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveQuickEndAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonSurviveQuickEndAck();
    FPbDungeonSurviveQuickEndAck(const idlepb::DungeonSurviveQuickEndAck& Right);
    void FromPb(const idlepb::DungeonSurviveQuickEndAck& Right);
    void ToPb(idlepb::DungeonSurviveQuickEndAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveQuickEndAck& Right);
    bool operator==(const FPbDungeonSurviveQuickEndAck& Right) const;
    bool operator!=(const FPbDungeonSurviveQuickEndAck& Right) const;
     
};

namespace idlepb {
class NotifyDungeonSurviveChallengeOver;
}  // namespace idlepb

/**
 * 生存副本挑战结束
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyDungeonSurviveChallengeOver
{
    GENERATED_BODY();

    /** 挑战Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /** 是否挑战成功 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool win;


    FPbNotifyDungeonSurviveChallengeOver();
    FPbNotifyDungeonSurviveChallengeOver(const idlepb::NotifyDungeonSurviveChallengeOver& Right);
    void FromPb(const idlepb::NotifyDungeonSurviveChallengeOver& Right);
    void ToPb(idlepb::NotifyDungeonSurviveChallengeOver* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyDungeonSurviveChallengeOver& Right);
    bool operator==(const FPbNotifyDungeonSurviveChallengeOver& Right) const;
    bool operator!=(const FPbNotifyDungeonSurviveChallengeOver& Right) const;
     
};

namespace idlepb {
class NotifyDungeonSurviveChallengeCurWaveNum;
}  // namespace idlepb

/**
 * 通知生存副本当前第几波
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyDungeonSurviveChallengeCurWaveNum
{
    GENERATED_BODY();

    /** 挑战Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /** 当前波数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 curnum;

    /** 最大波数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 maxnum;


    FPbNotifyDungeonSurviveChallengeCurWaveNum();
    FPbNotifyDungeonSurviveChallengeCurWaveNum(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right);
    void FromPb(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right);
    void ToPb(idlepb::NotifyDungeonSurviveChallengeCurWaveNum* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyDungeonSurviveChallengeCurWaveNum& Right);
    bool operator==(const FPbNotifyDungeonSurviveChallengeCurWaveNum& Right) const;
    bool operator!=(const FPbNotifyDungeonSurviveChallengeCurWaveNum& Right) const;
     
};

namespace idlepb {
class DungeonSurviveDataReq;
}  // namespace idlepb

/**
 * 询问生存副本是否完成
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ask_uid;


    FPbDungeonSurviveDataReq();
    FPbDungeonSurviveDataReq(const idlepb::DungeonSurviveDataReq& Right);
    void FromPb(const idlepb::DungeonSurviveDataReq& Right);
    void ToPb(idlepb::DungeonSurviveDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveDataReq& Right);
    bool operator==(const FPbDungeonSurviveDataReq& Right) const;
    bool operator!=(const FPbDungeonSurviveDataReq& Right) const;
     
};

namespace idlepb {
class DungeonSurviveDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDungeonSurviveDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbDungeonSurviveDataAck();
    FPbDungeonSurviveDataAck(const idlepb::DungeonSurviveDataAck& Right);
    void FromPb(const idlepb::DungeonSurviveDataAck& Right);
    void ToPb(idlepb::DungeonSurviveDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::DungeonSurviveDataAck& Right);
    bool operator==(const FPbDungeonSurviveDataAck& Right) const;
    bool operator!=(const FPbDungeonSurviveDataAck& Right) const;
     
};

namespace idlepb {
class RequestEnterSeptDemonWorldReq;
}  // namespace idlepb

/**
 * 请求进入镇魔深渊
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestEnterSeptDemonWorldReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;


    FPbRequestEnterSeptDemonWorldReq();
    FPbRequestEnterSeptDemonWorldReq(const idlepb::RequestEnterSeptDemonWorldReq& Right);
    void FromPb(const idlepb::RequestEnterSeptDemonWorldReq& Right);
    void ToPb(idlepb::RequestEnterSeptDemonWorldReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestEnterSeptDemonWorldReq& Right);
    bool operator==(const FPbRequestEnterSeptDemonWorldReq& Right) const;
    bool operator!=(const FPbRequestEnterSeptDemonWorldReq& Right) const;
     
};

namespace idlepb {
class RequestEnterSeptDemonWorldAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestEnterSeptDemonWorldAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbRequestEnterSeptDemonWorldAck();
    FPbRequestEnterSeptDemonWorldAck(const idlepb::RequestEnterSeptDemonWorldAck& Right);
    void FromPb(const idlepb::RequestEnterSeptDemonWorldAck& Right);
    void ToPb(idlepb::RequestEnterSeptDemonWorldAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestEnterSeptDemonWorldAck& Right);
    bool operator==(const FPbRequestEnterSeptDemonWorldAck& Right) const;
    bool operator!=(const FPbRequestEnterSeptDemonWorldAck& Right) const;
     
};

namespace idlepb {
class RequestLeaveSeptDemonWorldReq;
}  // namespace idlepb

/**
 * 请求退出镇魔深渊
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestLeaveSeptDemonWorldReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;


    FPbRequestLeaveSeptDemonWorldReq();
    FPbRequestLeaveSeptDemonWorldReq(const idlepb::RequestLeaveSeptDemonWorldReq& Right);
    void FromPb(const idlepb::RequestLeaveSeptDemonWorldReq& Right);
    void ToPb(idlepb::RequestLeaveSeptDemonWorldReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestLeaveSeptDemonWorldReq& Right);
    bool operator==(const FPbRequestLeaveSeptDemonWorldReq& Right) const;
    bool operator!=(const FPbRequestLeaveSeptDemonWorldReq& Right) const;
     
};

namespace idlepb {
class RequestLeaveSeptDemonWorldAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestLeaveSeptDemonWorldAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbRequestLeaveSeptDemonWorldAck();
    FPbRequestLeaveSeptDemonWorldAck(const idlepb::RequestLeaveSeptDemonWorldAck& Right);
    void FromPb(const idlepb::RequestLeaveSeptDemonWorldAck& Right);
    void ToPb(idlepb::RequestLeaveSeptDemonWorldAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestLeaveSeptDemonWorldAck& Right);
    bool operator==(const FPbRequestLeaveSeptDemonWorldAck& Right) const;
    bool operator!=(const FPbRequestLeaveSeptDemonWorldAck& Right) const;
     
};

namespace idlepb {
class RequestSeptDemonWorldDataReq;
}  // namespace idlepb

/**
 * 请求镇魔深渊相关数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestSeptDemonWorldDataReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;


    FPbRequestSeptDemonWorldDataReq();
    FPbRequestSeptDemonWorldDataReq(const idlepb::RequestSeptDemonWorldDataReq& Right);
    void FromPb(const idlepb::RequestSeptDemonWorldDataReq& Right);
    void ToPb(idlepb::RequestSeptDemonWorldDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestSeptDemonWorldDataReq& Right);
    bool operator==(const FPbRequestSeptDemonWorldDataReq& Right) const;
    bool operator!=(const FPbRequestSeptDemonWorldDataReq& Right) const;
     
};

namespace idlepb {
class RequestSeptDemonWorldDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestSeptDemonWorldDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSeptDemonWorldData data;


    FPbRequestSeptDemonWorldDataAck();
    FPbRequestSeptDemonWorldDataAck(const idlepb::RequestSeptDemonWorldDataAck& Right);
    void FromPb(const idlepb::RequestSeptDemonWorldDataAck& Right);
    void ToPb(idlepb::RequestSeptDemonWorldDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestSeptDemonWorldDataAck& Right);
    bool operator==(const FPbRequestSeptDemonWorldDataAck& Right) const;
    bool operator!=(const FPbRequestSeptDemonWorldDataAck& Right) const;
     
};

namespace idlepb {
class RequestInSeptDemonWorldEndTimeReq;
}  // namespace idlepb

/**
 * 请求在镇魔深渊待的最后时间点
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestInSeptDemonWorldEndTimeReq
{
    GENERATED_BODY();


    FPbRequestInSeptDemonWorldEndTimeReq();
    FPbRequestInSeptDemonWorldEndTimeReq(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right);
    void FromPb(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right);
    void ToPb(idlepb::RequestInSeptDemonWorldEndTimeReq* Out) const;
    void Reset();
    void operator=(const idlepb::RequestInSeptDemonWorldEndTimeReq& Right);
    bool operator==(const FPbRequestInSeptDemonWorldEndTimeReq& Right) const;
    bool operator!=(const FPbRequestInSeptDemonWorldEndTimeReq& Right) const;
     
};

namespace idlepb {
class RequestInSeptDemonWorldEndTimeAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRequestInSeptDemonWorldEndTimeAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 end_time;


    FPbRequestInSeptDemonWorldEndTimeAck();
    FPbRequestInSeptDemonWorldEndTimeAck(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right);
    void FromPb(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right);
    void ToPb(idlepb::RequestInSeptDemonWorldEndTimeAck* Out) const;
    void Reset();
    void operator=(const idlepb::RequestInSeptDemonWorldEndTimeAck& Right);
    bool operator==(const FPbRequestInSeptDemonWorldEndTimeAck& Right) const;
    bool operator!=(const FPbRequestInSeptDemonWorldEndTimeAck& Right) const;
     
};

namespace idlepb {
class GetFarmlandDataReq;
}  // namespace idlepb

/**
 * 药园数据请求
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetFarmlandDataReq
{
    GENERATED_BODY();


    FPbGetFarmlandDataReq();
    FPbGetFarmlandDataReq(const idlepb::GetFarmlandDataReq& Right);
    void FromPb(const idlepb::GetFarmlandDataReq& Right);
    void ToPb(idlepb::GetFarmlandDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetFarmlandDataReq& Right);
    bool operator==(const FPbGetFarmlandDataReq& Right) const;
    bool operator!=(const FPbGetFarmlandDataReq& Right) const;
     
};

namespace idlepb {
class GetFarmlandDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetFarmlandDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFarmlandData data;


    FPbGetFarmlandDataAck();
    FPbGetFarmlandDataAck(const idlepb::GetFarmlandDataAck& Right);
    void FromPb(const idlepb::GetFarmlandDataAck& Right);
    void ToPb(idlepb::GetFarmlandDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetFarmlandDataAck& Right);
    bool operator==(const FPbGetFarmlandDataAck& Right) const;
    bool operator!=(const FPbGetFarmlandDataAck& Right) const;
     
};

namespace idlepb {
class FarmlandUnlockBlockReq;
}  // namespace idlepb

/**
 * 药园地块解锁
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandUnlockBlockReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 y;


    FPbFarmlandUnlockBlockReq();
    FPbFarmlandUnlockBlockReq(const idlepb::FarmlandUnlockBlockReq& Right);
    void FromPb(const idlepb::FarmlandUnlockBlockReq& Right);
    void ToPb(idlepb::FarmlandUnlockBlockReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandUnlockBlockReq& Right);
    bool operator==(const FPbFarmlandUnlockBlockReq& Right) const;
    bool operator!=(const FPbFarmlandUnlockBlockReq& Right) const;
     
};

namespace idlepb {
class FarmlandUnlockBlockAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandUnlockBlockAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbFarmlandUnlockBlockAck();
    FPbFarmlandUnlockBlockAck(const idlepb::FarmlandUnlockBlockAck& Right);
    void FromPb(const idlepb::FarmlandUnlockBlockAck& Right);
    void ToPb(idlepb::FarmlandUnlockBlockAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandUnlockBlockAck& Right);
    bool operator==(const FPbFarmlandUnlockBlockAck& Right) const;
    bool operator!=(const FPbFarmlandUnlockBlockAck& Right) const;
     
};

namespace idlepb {
class FarmlandPlantSeedReq;
}  // namespace idlepb

/**
 * 药园种植或铲除
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandPlantSeedReq
{
    GENERATED_BODY();

    /** 道具配置id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /** 种植坐标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 y;

    /** 旋转摆放 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rotation;

    /** 改为铲除药植请求 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_delete;


    FPbFarmlandPlantSeedReq();
    FPbFarmlandPlantSeedReq(const idlepb::FarmlandPlantSeedReq& Right);
    void FromPb(const idlepb::FarmlandPlantSeedReq& Right);
    void ToPb(idlepb::FarmlandPlantSeedReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandPlantSeedReq& Right);
    bool operator==(const FPbFarmlandPlantSeedReq& Right) const;
    bool operator!=(const FPbFarmlandPlantSeedReq& Right) const;
     
};

namespace idlepb {
class FarmlandPlantSeedAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandPlantSeedAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbFarmlandPlantData plant_data;


    FPbFarmlandPlantSeedAck();
    FPbFarmlandPlantSeedAck(const idlepb::FarmlandPlantSeedAck& Right);
    void FromPb(const idlepb::FarmlandPlantSeedAck& Right);
    void ToPb(idlepb::FarmlandPlantSeedAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandPlantSeedAck& Right);
    bool operator==(const FPbFarmlandPlantSeedAck& Right) const;
    bool operator!=(const FPbFarmlandPlantSeedAck& Right) const;
     
};

namespace idlepb {
class FarmlandWateringReq;
}  // namespace idlepb

/**
 * 药园浇灌
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandWateringReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbFarmlandWateringReq();
    FPbFarmlandWateringReq(const idlepb::FarmlandWateringReq& Right);
    void FromPb(const idlepb::FarmlandWateringReq& Right);
    void ToPb(idlepb::FarmlandWateringReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandWateringReq& Right);
    bool operator==(const FPbFarmlandWateringReq& Right) const;
    bool operator!=(const FPbFarmlandWateringReq& Right) const;
     
};

namespace idlepb {
class FarmlandWateringAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandWateringAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 totaol_speed_up;


    FPbFarmlandWateringAck();
    FPbFarmlandWateringAck(const idlepb::FarmlandWateringAck& Right);
    void FromPb(const idlepb::FarmlandWateringAck& Right);
    void ToPb(idlepb::FarmlandWateringAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandWateringAck& Right);
    bool operator==(const FPbFarmlandWateringAck& Right) const;
    bool operator!=(const FPbFarmlandWateringAck& Right) const;
     
};

namespace idlepb {
class FarmlandRipeningReq;
}  // namespace idlepb

/**
 * 药园催熟
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandRipeningReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 plant_uid;

    /** 催熟道具id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /** 使用数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 一键催熟 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 one_click;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandManagementInfo> one_click_plants;


    FPbFarmlandRipeningReq();
    FPbFarmlandRipeningReq(const idlepb::FarmlandRipeningReq& Right);
    void FromPb(const idlepb::FarmlandRipeningReq& Right);
    void ToPb(idlepb::FarmlandRipeningReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandRipeningReq& Right);
    bool operator==(const FPbFarmlandRipeningReq& Right) const;
    bool operator!=(const FPbFarmlandRipeningReq& Right) const;
     
};

namespace idlepb {
class FarmlandRipeningAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandRipeningAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandPlantData> result;

    /** 用掉的催熟道具情况 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> used_ripe_items;


    FPbFarmlandRipeningAck();
    FPbFarmlandRipeningAck(const idlepb::FarmlandRipeningAck& Right);
    void FromPb(const idlepb::FarmlandRipeningAck& Right);
    void ToPb(idlepb::FarmlandRipeningAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandRipeningAck& Right);
    bool operator==(const FPbFarmlandRipeningAck& Right) const;
    bool operator!=(const FPbFarmlandRipeningAck& Right) const;
     
};

namespace idlepb {
class FarmlandHarvestReq;
}  // namespace idlepb

/**
 * 药园收获
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandHarvestReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> plant_ids;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_harvest_same_class;


    FPbFarmlandHarvestReq();
    FPbFarmlandHarvestReq(const idlepb::FarmlandHarvestReq& Right);
    void FromPb(const idlepb::FarmlandHarvestReq& Right);
    void ToPb(idlepb::FarmlandHarvestReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandHarvestReq& Right);
    bool operator==(const FPbFarmlandHarvestReq& Right) const;
    bool operator!=(const FPbFarmlandHarvestReq& Right) const;
     
};

namespace idlepb {
class FarmlandHarvestAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandHarvestAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;

    /** 收获成功的药株id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> op_success_plant_id;

    /** 续种的新数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandPlantData> continue_seeds;


    FPbFarmlandHarvestAck();
    FPbFarmlandHarvestAck(const idlepb::FarmlandHarvestAck& Right);
    void FromPb(const idlepb::FarmlandHarvestAck& Right);
    void ToPb(idlepb::FarmlandHarvestAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandHarvestAck& Right);
    bool operator==(const FPbFarmlandHarvestAck& Right) const;
    bool operator!=(const FPbFarmlandHarvestAck& Right) const;
     
};

namespace idlepb {
class FarmerRankUpReq;
}  // namespace idlepb

/**
 * 药园药童升级
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmerRankUpReq
{
    GENERATED_BODY();


    FPbFarmerRankUpReq();
    FPbFarmerRankUpReq(const idlepb::FarmerRankUpReq& Right);
    void FromPb(const idlepb::FarmerRankUpReq& Right);
    void ToPb(idlepb::FarmerRankUpReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmerRankUpReq& Right);
    bool operator==(const FPbFarmerRankUpReq& Right) const;
    bool operator!=(const FPbFarmerRankUpReq& Right) const;
     
};

namespace idlepb {
class FarmerRankUpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmerRankUpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbFarmerRankUpAck();
    FPbFarmerRankUpAck(const idlepb::FarmerRankUpAck& Right);
    void FromPb(const idlepb::FarmerRankUpAck& Right);
    void ToPb(idlepb::FarmerRankUpAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmerRankUpAck& Right);
    bool operator==(const FPbFarmerRankUpAck& Right) const;
    bool operator!=(const FPbFarmerRankUpAck& Right) const;
     
};

namespace idlepb {
class FarmlandSetManagementReq;
}  // namespace idlepb

/**
 * 药园打理
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandSetManagementReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandManagementInfo> plans;


    FPbFarmlandSetManagementReq();
    FPbFarmlandSetManagementReq(const idlepb::FarmlandSetManagementReq& Right);
    void FromPb(const idlepb::FarmlandSetManagementReq& Right);
    void ToPb(idlepb::FarmlandSetManagementReq* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandSetManagementReq& Right);
    bool operator==(const FPbFarmlandSetManagementReq& Right) const;
    bool operator!=(const FPbFarmlandSetManagementReq& Right) const;
     
};

namespace idlepb {
class FarmlandSetManagementAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandSetManagementAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbFarmlandSetManagementAck();
    FPbFarmlandSetManagementAck(const idlepb::FarmlandSetManagementAck& Right);
    void FromPb(const idlepb::FarmlandSetManagementAck& Right);
    void ToPb(idlepb::FarmlandSetManagementAck* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandSetManagementAck& Right);
    bool operator==(const FPbFarmlandSetManagementAck& Right) const;
    bool operator!=(const FPbFarmlandSetManagementAck& Right) const;
     
};

namespace idlepb {
class UpdateFarmlandStateReq;
}  // namespace idlepb

/**
 * 药园状态数据，自动收获，药童好感动刷新
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateFarmlandStateReq
{
    GENERATED_BODY();


    FPbUpdateFarmlandStateReq();
    FPbUpdateFarmlandStateReq(const idlepb::UpdateFarmlandStateReq& Right);
    void FromPb(const idlepb::UpdateFarmlandStateReq& Right);
    void ToPb(idlepb::UpdateFarmlandStateReq* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateFarmlandStateReq& Right);
    bool operator==(const FPbUpdateFarmlandStateReq& Right) const;
    bool operator!=(const FPbUpdateFarmlandStateReq& Right) const;
     
};

namespace idlepb {
class UpdateFarmlandStateAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbUpdateFarmlandStateAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 farmer_friendship_exp;

    /** 自动收获的植株 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> auto_harvest_plants;

    /** 续种的新数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandPlantData> continue_seeds;

    /** 自动收获的道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> harvest_items;


    FPbUpdateFarmlandStateAck();
    FPbUpdateFarmlandStateAck(const idlepb::UpdateFarmlandStateAck& Right);
    void FromPb(const idlepb::UpdateFarmlandStateAck& Right);
    void ToPb(idlepb::UpdateFarmlandStateAck* Out) const;
    void Reset();
    void operator=(const idlepb::UpdateFarmlandStateAck& Right);
    bool operator==(const FPbUpdateFarmlandStateAck& Right) const;
    bool operator!=(const FPbUpdateFarmlandStateAck& Right) const;
     
};

namespace idlepb {
class GetRoleInfoReq;
}  // namespace idlepb

/**
 * 请求玩家个人信息 Todo 旧接口
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleInfoReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;


    FPbGetRoleInfoReq();
    FPbGetRoleInfoReq(const idlepb::GetRoleInfoReq& Right);
    void FromPb(const idlepb::GetRoleInfoReq& Right);
    void ToPb(idlepb::GetRoleInfoReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleInfoReq& Right);
    bool operator==(const FPbGetRoleInfoReq& Right) const;
    bool operator!=(const FPbGetRoleInfoReq& Right) const;
     
};

namespace idlepb {
class GetRoleInfoAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleInfoAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleInfo role_info;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbGetRoleInfoAck();
    FPbGetRoleInfoAck(const idlepb::GetRoleInfoAck& Right);
    void FromPb(const idlepb::GetRoleInfoAck& Right);
    void ToPb(idlepb::GetRoleInfoAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleInfoAck& Right);
    bool operator==(const FPbGetRoleInfoAck& Right) const;
    bool operator!=(const FPbGetRoleInfoAck& Right) const;
     
};

namespace idlepb {
class GetRoleFriendDataReq;
}  // namespace idlepb

/**
 * 获取道友功能数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleFriendDataReq
{
    GENERATED_BODY();


    FPbGetRoleFriendDataReq();
    FPbGetRoleFriendDataReq(const idlepb::GetRoleFriendDataReq& Right);
    void FromPb(const idlepb::GetRoleFriendDataReq& Right);
    void ToPb(idlepb::GetRoleFriendDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleFriendDataReq& Right);
    bool operator==(const FPbGetRoleFriendDataReq& Right) const;
    bool operator!=(const FPbGetRoleFriendDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleFriendDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleFriendDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFriendData data;

    /** 用于显示的角色数据缓存 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleRoleInfo> role_infos;


    FPbGetRoleFriendDataAck();
    FPbGetRoleFriendDataAck(const idlepb::GetRoleFriendDataAck& Right);
    void FromPb(const idlepb::GetRoleFriendDataAck& Right);
    void ToPb(idlepb::GetRoleFriendDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleFriendDataAck& Right);
    bool operator==(const FPbGetRoleFriendDataAck& Right) const;
    bool operator!=(const FPbGetRoleFriendDataAck& Right) const;
     
};

namespace idlepb {
class FriendOpReq;
}  // namespace idlepb

/**
 * 发起 好友申请/或移除好友 拉黑/或移除拉黑 成为道侣或解除道侣
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendOpReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /** 枚举值除了None无用处，其它分别为发起好友请求，拉黑，结义操作 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFriendRelationshipType op_type;

    /** 是否为反向操作, 即删除好友，解除拉黑，解除结义 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool reverse_op;


    FPbFriendOpReq();
    FPbFriendOpReq(const idlepb::FriendOpReq& Right);
    void FromPb(const idlepb::FriendOpReq& Right);
    void ToPb(idlepb::FriendOpReq* Out) const;
    void Reset();
    void operator=(const idlepb::FriendOpReq& Right);
    bool operator==(const FPbFriendOpReq& Right) const;
    bool operator!=(const FPbFriendOpReq& Right) const;
     
};

namespace idlepb {
class FriendOpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendOpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFriendRelationshipType relationship_ab;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFriendRelationshipType relationship_ba;


    FPbFriendOpAck();
    FPbFriendOpAck(const idlepb::FriendOpAck& Right);
    void FromPb(const idlepb::FriendOpAck& Right);
    void ToPb(idlepb::FriendOpAck* Out) const;
    void Reset();
    void operator=(const idlepb::FriendOpAck& Right);
    bool operator==(const FPbFriendOpAck& Right) const;
    bool operator!=(const FPbFriendOpAck& Right) const;
     
};

namespace idlepb {
class ReplyFriendRequestReq;
}  // namespace idlepb

/**
 * 发起 好友申请/或移除好友 拉黑/或移除拉黑 成为道侣或解除道侣
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReplyFriendRequestReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool agree;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool one_click;


    FPbReplyFriendRequestReq();
    FPbReplyFriendRequestReq(const idlepb::ReplyFriendRequestReq& Right);
    void FromPb(const idlepb::ReplyFriendRequestReq& Right);
    void ToPb(idlepb::ReplyFriendRequestReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReplyFriendRequestReq& Right);
    bool operator==(const FPbReplyFriendRequestReq& Right) const;
    bool operator!=(const FPbReplyFriendRequestReq& Right) const;
     
};

namespace idlepb {
class ReplyFriendRequestAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReplyFriendRequestAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /** 操作失败的角色与玩家的关系 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> relationship_ba;

    /** 操作失败的角色id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> failed_ids;


    FPbReplyFriendRequestAck();
    FPbReplyFriendRequestAck(const idlepb::ReplyFriendRequestAck& Right);
    void FromPb(const idlepb::ReplyFriendRequestAck& Right);
    void ToPb(idlepb::ReplyFriendRequestAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReplyFriendRequestAck& Right);
    bool operator==(const FPbReplyFriendRequestAck& Right) const;
    bool operator!=(const FPbReplyFriendRequestAck& Right) const;
     
};

namespace idlepb {
class FriendSearchRoleInfoReq;
}  // namespace idlepb

/**
 * 查找玩家（道友功能）
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendSearchRoleInfoReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;


    FPbFriendSearchRoleInfoReq();
    FPbFriendSearchRoleInfoReq(const idlepb::FriendSearchRoleInfoReq& Right);
    void FromPb(const idlepb::FriendSearchRoleInfoReq& Right);
    void ToPb(idlepb::FriendSearchRoleInfoReq* Out) const;
    void Reset();
    void operator=(const idlepb::FriendSearchRoleInfoReq& Right);
    bool operator==(const FPbFriendSearchRoleInfoReq& Right) const;
    bool operator!=(const FPbFriendSearchRoleInfoReq& Right) const;
     
};

namespace idlepb {
class FriendSearchRoleInfoAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendSearchRoleInfoAck
{
    GENERATED_BODY();

    /** 用于显示的角色数据缓存 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleRoleInfo> role_infos;


    FPbFriendSearchRoleInfoAck();
    FPbFriendSearchRoleInfoAck(const idlepb::FriendSearchRoleInfoAck& Right);
    void FromPb(const idlepb::FriendSearchRoleInfoAck& Right);
    void ToPb(idlepb::FriendSearchRoleInfoAck* Out) const;
    void Reset();
    void operator=(const idlepb::FriendSearchRoleInfoAck& Right);
    bool operator==(const FPbFriendSearchRoleInfoAck& Right) const;
    bool operator!=(const FPbFriendSearchRoleInfoAck& Right) const;
     
};

namespace idlepb {
class NotifyFriendMessage;
}  // namespace idlepb

/**
 * 通知道友功能消息,视玩家数据进行解析
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyFriendMessage
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSimpleRoleInfo role_info;

    /** 1.上线状态更新消息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_update_state;

    /** 2.好友申请被拒绝消息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool b_refused;

    /** 3.好友关系变更事件消息（收到请求|绝交、被拉黑、成为好友|道缘值更新） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbFriendListItem friend_event;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool online;


    FPbNotifyFriendMessage();
    FPbNotifyFriendMessage(const idlepb::NotifyFriendMessage& Right);
    void FromPb(const idlepb::NotifyFriendMessage& Right);
    void ToPb(idlepb::NotifyFriendMessage* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyFriendMessage& Right);
    bool operator==(const FPbNotifyFriendMessage& Right) const;
    bool operator!=(const FPbNotifyFriendMessage& Right) const;
     
};

namespace idlepb {
class GetRoleAvatarDataReq;
}  // namespace idlepb

/**
 * 获取化身数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleAvatarDataReq
{
    GENERATED_BODY();

    /** 触发时间间隔进行物资抽取结算 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool draw_this_time;


    FPbGetRoleAvatarDataReq();
    FPbGetRoleAvatarDataReq(const idlepb::GetRoleAvatarDataReq& Right);
    void FromPb(const idlepb::GetRoleAvatarDataReq& Right);
    void ToPb(idlepb::GetRoleAvatarDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleAvatarDataReq& Right);
    bool operator==(const FPbGetRoleAvatarDataReq& Right) const;
    bool operator!=(const FPbGetRoleAvatarDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleAvatarDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleAvatarDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAvatarData data;


    FPbGetRoleAvatarDataAck();
    FPbGetRoleAvatarDataAck(const idlepb::GetRoleAvatarDataAck& Right);
    void FromPb(const idlepb::GetRoleAvatarDataAck& Right);
    void ToPb(idlepb::GetRoleAvatarDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleAvatarDataAck& Right);
    bool operator==(const FPbGetRoleAvatarDataAck& Right) const;
    bool operator!=(const FPbGetRoleAvatarDataAck& Right) const;
     
};

namespace idlepb {
class DispatchAvatarReq;
}  // namespace idlepb

/**
 * 派遣化身
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDispatchAvatarReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 world_index;


    FPbDispatchAvatarReq();
    FPbDispatchAvatarReq(const idlepb::DispatchAvatarReq& Right);
    void FromPb(const idlepb::DispatchAvatarReq& Right);
    void ToPb(idlepb::DispatchAvatarReq* Out) const;
    void Reset();
    void operator=(const idlepb::DispatchAvatarReq& Right);
    bool operator==(const FPbDispatchAvatarReq& Right) const;
    bool operator!=(const FPbDispatchAvatarReq& Right) const;
     
};

namespace idlepb {
class DispatchAvatarAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDispatchAvatarAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAvatarData data;


    FPbDispatchAvatarAck();
    FPbDispatchAvatarAck(const idlepb::DispatchAvatarAck& Right);
    void FromPb(const idlepb::DispatchAvatarAck& Right);
    void ToPb(idlepb::DispatchAvatarAck* Out) const;
    void Reset();
    void operator=(const idlepb::DispatchAvatarAck& Right);
    bool operator==(const FPbDispatchAvatarAck& Right) const;
    bool operator!=(const FPbDispatchAvatarAck& Right) const;
     
};

namespace idlepb {
class AvatarRankUpReq;
}  // namespace idlepb

/**
 * 化身升级
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAvatarRankUpReq
{
    GENERATED_BODY();


    FPbAvatarRankUpReq();
    FPbAvatarRankUpReq(const idlepb::AvatarRankUpReq& Right);
    void FromPb(const idlepb::AvatarRankUpReq& Right);
    void ToPb(idlepb::AvatarRankUpReq* Out) const;
    void Reset();
    void operator=(const idlepb::AvatarRankUpReq& Right);
    bool operator==(const FPbAvatarRankUpReq& Right) const;
    bool operator!=(const FPbAvatarRankUpReq& Right) const;
     
};

namespace idlepb {
class AvatarRankUpAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAvatarRankUpAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAvatarData data;


    FPbAvatarRankUpAck();
    FPbAvatarRankUpAck(const idlepb::AvatarRankUpAck& Right);
    void FromPb(const idlepb::AvatarRankUpAck& Right);
    void ToPb(idlepb::AvatarRankUpAck* Out) const;
    void Reset();
    void operator=(const idlepb::AvatarRankUpAck& Right);
    bool operator==(const FPbAvatarRankUpAck& Right) const;
    bool operator!=(const FPbAvatarRankUpAck& Right) const;
     
};

namespace idlepb {
class ReceiveAvatarTempPackageReq;
}  // namespace idlepb

/**
 * 收获化身包裹道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveAvatarTempPackageReq
{
    GENERATED_BODY();


    FPbReceiveAvatarTempPackageReq();
    FPbReceiveAvatarTempPackageReq(const idlepb::ReceiveAvatarTempPackageReq& Right);
    void FromPb(const idlepb::ReceiveAvatarTempPackageReq& Right);
    void ToPb(idlepb::ReceiveAvatarTempPackageReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveAvatarTempPackageReq& Right);
    bool operator==(const FPbReceiveAvatarTempPackageReq& Right) const;
    bool operator!=(const FPbReceiveAvatarTempPackageReq& Right) const;
     
};

namespace idlepb {
class ReceiveAvatarTempPackageAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveAvatarTempPackageAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAvatarData data;


    FPbReceiveAvatarTempPackageAck();
    FPbReceiveAvatarTempPackageAck(const idlepb::ReceiveAvatarTempPackageAck& Right);
    void FromPb(const idlepb::ReceiveAvatarTempPackageAck& Right);
    void ToPb(idlepb::ReceiveAvatarTempPackageAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveAvatarTempPackageAck& Right);
    bool operator==(const FPbReceiveAvatarTempPackageAck& Right) const;
    bool operator!=(const FPbReceiveAvatarTempPackageAck& Right) const;
     
};

namespace idlepb {
class GetRoleBiographyDataReq;
}  // namespace idlepb

/**
 * 请求角色传记数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleBiographyDataReq
{
    GENERATED_BODY();


    FPbGetRoleBiographyDataReq();
    FPbGetRoleBiographyDataReq(const idlepb::GetRoleBiographyDataReq& Right);
    void FromPb(const idlepb::GetRoleBiographyDataReq& Right);
    void ToPb(idlepb::GetRoleBiographyDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleBiographyDataReq& Right);
    bool operator==(const FPbGetRoleBiographyDataReq& Right) const;
    bool operator!=(const FPbGetRoleBiographyDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleBiographyDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleBiographyDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleBiographyData data;


    FPbGetRoleBiographyDataAck();
    FPbGetRoleBiographyDataAck(const idlepb::GetRoleBiographyDataAck& Right);
    void FromPb(const idlepb::GetRoleBiographyDataAck& Right);
    void ToPb(idlepb::GetRoleBiographyDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleBiographyDataAck& Right);
    bool operator==(const FPbGetRoleBiographyDataAck& Right) const;
    bool operator!=(const FPbGetRoleBiographyDataAck& Right) const;
     
};

namespace idlepb {
class ReceiveBiographyItemReq;
}  // namespace idlepb

/**
 * 请求领取传记奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBiographyItemReq
{
    GENERATED_BODY();

    /** 一个章节下的Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> cfg_ids;


    FPbReceiveBiographyItemReq();
    FPbReceiveBiographyItemReq(const idlepb::ReceiveBiographyItemReq& Right);
    void FromPb(const idlepb::ReceiveBiographyItemReq& Right);
    void ToPb(idlepb::ReceiveBiographyItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBiographyItemReq& Right);
    bool operator==(const FPbReceiveBiographyItemReq& Right) const;
    bool operator!=(const FPbReceiveBiographyItemReq& Right) const;
     
};

namespace idlepb {
class ReceiveBiographyItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBiographyItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbReceiveBiographyItemAck();
    FPbReceiveBiographyItemAck(const idlepb::ReceiveBiographyItemAck& Right);
    void FromPb(const idlepb::ReceiveBiographyItemAck& Right);
    void ToPb(idlepb::ReceiveBiographyItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBiographyItemAck& Right);
    bool operator==(const FPbReceiveBiographyItemAck& Right) const;
    bool operator!=(const FPbReceiveBiographyItemAck& Right) const;
     
};

namespace idlepb {
class NotifyBiographyMessage;
}  // namespace idlepb

/**
 * 通知传记功能消息（史记或纪念）
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyBiographyMessage
{
    GENERATED_BODY();


    FPbNotifyBiographyMessage();
    FPbNotifyBiographyMessage(const idlepb::NotifyBiographyMessage& Right);
    void FromPb(const idlepb::NotifyBiographyMessage& Right);
    void ToPb(idlepb::NotifyBiographyMessage* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyBiographyMessage& Right);
    bool operator==(const FPbNotifyBiographyMessage& Right) const;
    bool operator!=(const FPbNotifyBiographyMessage& Right) const;
     
};

namespace idlepb {
class GetBiographyEventDataReq;
}  // namespace idlepb

/**
 * 请求史记记数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetBiographyEventDataReq
{
    GENERATED_BODY();


    FPbGetBiographyEventDataReq();
    FPbGetBiographyEventDataReq(const idlepb::GetBiographyEventDataReq& Right);
    void FromPb(const idlepb::GetBiographyEventDataReq& Right);
    void ToPb(idlepb::GetBiographyEventDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetBiographyEventDataReq& Right);
    bool operator==(const FPbGetBiographyEventDataReq& Right) const;
    bool operator!=(const FPbGetBiographyEventDataReq& Right) const;
     
};

namespace idlepb {
class GetBiographyEventDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetBiographyEventDataAck
{
    GENERATED_BODY();

    /** 史记排行榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBiographyEventLeaderboardList> biography_lists;

    /** 服务器计数器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbServerCounterData server_counter_data;


    FPbGetBiographyEventDataAck();
    FPbGetBiographyEventDataAck(const idlepb::GetBiographyEventDataAck& Right);
    void FromPb(const idlepb::GetBiographyEventDataAck& Right);
    void ToPb(idlepb::GetBiographyEventDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetBiographyEventDataAck& Right);
    bool operator==(const FPbGetBiographyEventDataAck& Right) const;
    bool operator!=(const FPbGetBiographyEventDataAck& Right) const;
     
};

namespace idlepb {
class ReceiveBiographyEventItemReq;
}  // namespace idlepb

/**
 * 请求领取史记奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBiographyEventItemReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;


    FPbReceiveBiographyEventItemReq();
    FPbReceiveBiographyEventItemReq(const idlepb::ReceiveBiographyEventItemReq& Right);
    void FromPb(const idlepb::ReceiveBiographyEventItemReq& Right);
    void ToPb(idlepb::ReceiveBiographyEventItemReq* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBiographyEventItemReq& Right);
    bool operator==(const FPbReceiveBiographyEventItemReq& Right) const;
    bool operator!=(const FPbReceiveBiographyEventItemReq& Right) const;
     
};

namespace idlepb {
class ReceiveBiographyEventItemAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReceiveBiographyEventItemAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;


    FPbReceiveBiographyEventItemAck();
    FPbReceiveBiographyEventItemAck(const idlepb::ReceiveBiographyEventItemAck& Right);
    void FromPb(const idlepb::ReceiveBiographyEventItemAck& Right);
    void ToPb(idlepb::ReceiveBiographyEventItemAck* Out) const;
    void Reset();
    void operator=(const idlepb::ReceiveBiographyEventItemAck& Right);
    bool operator==(const FPbReceiveBiographyEventItemAck& Right) const;
    bool operator!=(const FPbReceiveBiographyEventItemAck& Right) const;
     
};

namespace idlepb {
class AddBiographyRoleLogReq;
}  // namespace idlepb

/**
 * 请求上传纪念日志
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAddBiographyRoleLogReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBiographyRoleLog log;


    FPbAddBiographyRoleLogReq();
    FPbAddBiographyRoleLogReq(const idlepb::AddBiographyRoleLogReq& Right);
    void FromPb(const idlepb::AddBiographyRoleLogReq& Right);
    void ToPb(idlepb::AddBiographyRoleLogReq* Out) const;
    void Reset();
    void operator=(const idlepb::AddBiographyRoleLogReq& Right);
    bool operator==(const FPbAddBiographyRoleLogReq& Right) const;
    bool operator!=(const FPbAddBiographyRoleLogReq& Right) const;
     
};

namespace idlepb {
class AddBiographyRoleLogAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAddBiographyRoleLogAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBiographyRoleLog log;


    FPbAddBiographyRoleLogAck();
    FPbAddBiographyRoleLogAck(const idlepb::AddBiographyRoleLogAck& Right);
    void FromPb(const idlepb::AddBiographyRoleLogAck& Right);
    void ToPb(idlepb::AddBiographyRoleLogAck* Out) const;
    void Reset();
    void operator=(const idlepb::AddBiographyRoleLogAck& Right);
    bool operator==(const FPbAddBiographyRoleLogAck& Right) const;
    bool operator!=(const FPbAddBiographyRoleLogAck& Right) const;
     
};

namespace idlepb {
class GetRoleVipShopDataReq;
}  // namespace idlepb

/**
 * 请求仙阁商店数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleVipShopDataReq
{
    GENERATED_BODY();


    FPbGetRoleVipShopDataReq();
    FPbGetRoleVipShopDataReq(const idlepb::GetRoleVipShopDataReq& Right);
    void FromPb(const idlepb::GetRoleVipShopDataReq& Right);
    void ToPb(idlepb::GetRoleVipShopDataReq* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleVipShopDataReq& Right);
    bool operator==(const FPbGetRoleVipShopDataReq& Right) const;
    bool operator!=(const FPbGetRoleVipShopDataReq& Right) const;
     
};

namespace idlepb {
class GetRoleVipShopDataAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGetRoleVipShopDataAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleVipShopData data;


    FPbGetRoleVipShopDataAck();
    FPbGetRoleVipShopDataAck(const idlepb::GetRoleVipShopDataAck& Right);
    void FromPb(const idlepb::GetRoleVipShopDataAck& Right);
    void ToPb(idlepb::GetRoleVipShopDataAck* Out) const;
    void Reset();
    void operator=(const idlepb::GetRoleVipShopDataAck& Right);
    bool operator==(const FPbGetRoleVipShopDataAck& Right) const;
    bool operator!=(const FPbGetRoleVipShopDataAck& Right) const;
     
};

namespace idlepb {
class VipShopBuyReq;
}  // namespace idlepb

/**
 * 请求仙阁商店购买
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbVipShopBuyReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbVipShopBuyReq();
    FPbVipShopBuyReq(const idlepb::VipShopBuyReq& Right);
    void FromPb(const idlepb::VipShopBuyReq& Right);
    void ToPb(idlepb::VipShopBuyReq* Out) const;
    void Reset();
    void operator=(const idlepb::VipShopBuyReq& Right);
    bool operator==(const FPbVipShopBuyReq& Right) const;
    bool operator!=(const FPbVipShopBuyReq& Right) const;
     
};

namespace idlepb {
class VipShopBuyAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbVipShopBuyAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbVipShopBuyAck();
    FPbVipShopBuyAck(const idlepb::VipShopBuyAck& Right);
    void FromPb(const idlepb::VipShopBuyAck& Right);
    void ToPb(idlepb::VipShopBuyAck* Out) const;
    void Reset();
    void operator=(const idlepb::VipShopBuyAck& Right);
    bool operator==(const FPbVipShopBuyAck& Right) const;
    bool operator!=(const FPbVipShopBuyAck& Right) const;
     
};
