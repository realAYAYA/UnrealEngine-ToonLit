#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbNet.h"
#include "PbCommon.generated.h"


namespace idlepb {
class Int64Data;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInt64Data
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 value;


    FPbInt64Data();
    FPbInt64Data(const idlepb::Int64Data& Right);
    void FromPb(const idlepb::Int64Data& Right);
    void ToPb(idlepb::Int64Data* Out) const;
    void Reset();
    void operator=(const idlepb::Int64Data& Right);
    bool operator==(const FPbInt64Data& Right) const;
    bool operator!=(const FPbInt64Data& Right) const;
     
};

namespace idlepb {
class Vector2;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbVector2
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float y;


    FPbVector2();
    FPbVector2(const idlepb::Vector2& Right);
    void FromPb(const idlepb::Vector2& Right);
    void ToPb(idlepb::Vector2* Out) const;
    void Reset();
    void operator=(const idlepb::Vector2& Right);
    bool operator==(const FPbVector2& Right) const;
    bool operator!=(const FPbVector2& Right) const;
     
};

namespace idlepb {
class Vector3;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbVector3
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float y;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float z;


    FPbVector3();
    FPbVector3(const idlepb::Vector3& Right);
    void FromPb(const idlepb::Vector3& Right);
    void ToPb(idlepb::Vector3* Out) const;
    void Reset();
    void operator=(const idlepb::Vector3& Right);
    bool operator==(const FPbVector3& Right) const;
    bool operator!=(const FPbVector3& Right) const;
     
};

namespace idlepb {
class Color;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbColor
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float r;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float g;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float b;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float a;


    FPbColor();
    FPbColor(const idlepb::Color& Right);
    void FromPb(const idlepb::Color& Right);
    void ToPb(idlepb::Color* Out) const;
    void Reset();
    void operator=(const idlepb::Color& Right);
    bool operator==(const FPbColor& Right) const;
    bool operator!=(const FPbColor& Right) const;
     
};

namespace idlepb {
class Int64Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInt64Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 key;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 value;


    FPbInt64Pair();
    FPbInt64Pair(const idlepb::Int64Pair& Right);
    void FromPb(const idlepb::Int64Pair& Right);
    void ToPb(idlepb::Int64Pair* Out) const;
    void Reset();
    void operator=(const idlepb::Int64Pair& Right);
    bool operator==(const FPbInt64Pair& Right) const;
    bool operator!=(const FPbInt64Pair& Right) const;
     
};

namespace idlepb {
class StringKeyInt32ValueEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbStringKeyInt32ValueEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString key;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 value;


    FPbStringKeyInt32ValueEntry();
    FPbStringKeyInt32ValueEntry(const idlepb::StringKeyInt32ValueEntry& Right);
    void FromPb(const idlepb::StringKeyInt32ValueEntry& Right);
    void ToPb(idlepb::StringKeyInt32ValueEntry* Out) const;
    void Reset();
    void operator=(const idlepb::StringKeyInt32ValueEntry& Right);
    bool operator==(const FPbStringKeyInt32ValueEntry& Right) const;
    bool operator!=(const FPbStringKeyInt32ValueEntry& Right) const;
     
};

namespace idlepb {
class MapValueInt32;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMapValueInt32
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 key;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 value;


    FPbMapValueInt32();
    FPbMapValueInt32(const idlepb::MapValueInt32& Right);
    void FromPb(const idlepb::MapValueInt32& Right);
    void ToPb(idlepb::MapValueInt32* Out) const;
    void Reset();
    void operator=(const idlepb::MapValueInt32& Right);
    bool operator==(const FPbMapValueInt32& Right) const;
    bool operator!=(const FPbMapValueInt32& Right) const;
     
};

namespace idlepb {
class StringInt64Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbStringInt64Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString str;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 value;


    FPbStringInt64Pair();
    FPbStringInt64Pair(const idlepb::StringInt64Pair& Right);
    void FromPb(const idlepb::StringInt64Pair& Right);
    void ToPb(idlepb::StringInt64Pair* Out) const;
    void Reset();
    void operator=(const idlepb::StringInt64Pair& Right);
    bool operator==(const FPbStringInt64Pair& Right) const;
    bool operator!=(const FPbStringInt64Pair& Right) const;
     
};

namespace idlepb {
class AbilityEffectData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAbilityEffectData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float duration;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float period;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 duration_policy;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float starttime_world;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 starttime_utc;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float y;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float z;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float m;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float n;


    FPbAbilityEffectData();
    FPbAbilityEffectData(const idlepb::AbilityEffectData& Right);
    void FromPb(const idlepb::AbilityEffectData& Right);
    void ToPb(idlepb::AbilityEffectData* Out) const;
    void Reset();
    void operator=(const idlepb::AbilityEffectData& Right);
    bool operator==(const FPbAbilityEffectData& Right) const;
    bool operator!=(const FPbAbilityEffectData& Right) const;
     
};


/**
 * Replication Target Type
*/
UENUM(BlueprintType)
enum class EPbReplicationTargetType : uint8
{
    RTT_Self = 0 UMETA(DisplayName="自己所在客户端"),
    RTT_World = 1 UMETA(DisplayName="当前场景"),
};
constexpr EPbReplicationTargetType EPbReplicationTargetType_Min = EPbReplicationTargetType::RTT_Self;
constexpr EPbReplicationTargetType EPbReplicationTargetType_Max = EPbReplicationTargetType::RTT_World;
constexpr int32 EPbReplicationTargetType_ArraySize = static_cast<int32>(EPbReplicationTargetType_Max) + 1;
MPROTOCOL_API bool CheckEPbReplicationTargetTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbReplicationTargetTypeDescription(EPbReplicationTargetType Val);

template <typename Char>
struct fmt::formatter<EPbReplicationTargetType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbReplicationTargetType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 系统信息显示样式
*/
UENUM(BlueprintType)
enum class EPbSystemNoticeStyle : uint8
{
    SystemNoticeStyle_None = 0 UMETA(DisplayName="未知"),
    SystemNoticeStyle_Dialog = 1 UMETA(DisplayName="弹框提示"),
    SystemNoticeStyle_ScreenCenter = 2 UMETA(DisplayName="屏幕中央提示"),
};
constexpr EPbSystemNoticeStyle EPbSystemNoticeStyle_Min = EPbSystemNoticeStyle::SystemNoticeStyle_None;
constexpr EPbSystemNoticeStyle EPbSystemNoticeStyle_Max = EPbSystemNoticeStyle::SystemNoticeStyle_ScreenCenter;
constexpr int32 EPbSystemNoticeStyle_ArraySize = static_cast<int32>(EPbSystemNoticeStyle_Max) + 1;
MPROTOCOL_API bool CheckEPbSystemNoticeStyleValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSystemNoticeStyleDescription(EPbSystemNoticeStyle Val);

template <typename Char>
struct fmt::formatter<EPbSystemNoticeStyle, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSystemNoticeStyle& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbSystemNoticeId : uint8
{
    SystemNoticeId_None = 0 UMETA(DisplayName="未知"),
    SystemNoticeId_AddItem = 7 UMETA(DisplayName="添加道具"),
};
constexpr EPbSystemNoticeId EPbSystemNoticeId_Min = EPbSystemNoticeId::SystemNoticeId_None;
constexpr EPbSystemNoticeId EPbSystemNoticeId_Max = EPbSystemNoticeId::SystemNoticeId_AddItem;
constexpr int32 EPbSystemNoticeId_ArraySize = static_cast<int32>(EPbSystemNoticeId_Max) + 1;
MPROTOCOL_API bool CheckEPbSystemNoticeIdValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSystemNoticeIdDescription(EPbSystemNoticeId Val);

template <typename Char>
struct fmt::formatter<EPbSystemNoticeId, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSystemNoticeId& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class GameStatData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameStatData
{
    GENERATED_BODY();

    /** 属性类型 GameStatType */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /** 属性数值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float value;


    FPbGameStatData();
    FPbGameStatData(const idlepb::GameStatData& Right);
    void FromPb(const idlepb::GameStatData& Right);
    void ToPb(idlepb::GameStatData* Out) const;
    void Reset();
    void operator=(const idlepb::GameStatData& Right);
    bool operator==(const FPbGameStatData& Right) const;
    bool operator!=(const FPbGameStatData& Right) const;
     
};

namespace idlepb {
class GameStatsData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameStatsData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbGameStatData> stats;


    FPbGameStatsData();
    FPbGameStatsData(const idlepb::GameStatsData& Right);
    void FromPb(const idlepb::GameStatsData& Right);
    void ToPb(idlepb::GameStatsData* Out) const;
    void Reset();
    void operator=(const idlepb::GameStatsData& Right);
    bool operator==(const FPbGameStatsData& Right) const;
    bool operator!=(const FPbGameStatsData& Right) const;
     
};

namespace idlepb {
class GameStatsModuleData;
}  // namespace idlepb

/**
 * 属性集
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameStatsModuleData
{
    GENERATED_BODY();

    /** 模块类型 (GameStatsModuleType or ItemStatsModuleType) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /** 模块属性 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData stats;


    FPbGameStatsModuleData();
    FPbGameStatsModuleData(const idlepb::GameStatsModuleData& Right);
    void FromPb(const idlepb::GameStatsModuleData& Right);
    void ToPb(idlepb::GameStatsModuleData* Out) const;
    void Reset();
    void operator=(const idlepb::GameStatsModuleData& Right);
    bool operator==(const FPbGameStatsModuleData& Right) const;
    bool operator!=(const FPbGameStatsModuleData& Right) const;
     
};

namespace idlepb {
class GameStatsAllModuleData;
}  // namespace idlepb

/**
 * 属性集模块
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameStatsAllModuleData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbGameStatsModuleData> all_module;


    FPbGameStatsAllModuleData();
    FPbGameStatsAllModuleData(const idlepb::GameStatsAllModuleData& Right);
    void FromPb(const idlepb::GameStatsAllModuleData& Right);
    void ToPb(idlepb::GameStatsAllModuleData* Out) const;
    void Reset();
    void operator=(const idlepb::GameStatsAllModuleData& Right);
    bool operator==(const FPbGameStatsAllModuleData& Right) const;
    bool operator!=(const FPbGameStatsAllModuleData& Right) const;
     
};

namespace idlepb {
class RoleAttribute;
}  // namespace idlepb

/**
 * 角色属性 -- 本结构仅只为兼容性存在，新版请使用 GameStatsData
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleAttribute
{
    GENERATED_BODY();

    /** 气血 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float health;

    /** 气血上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float max_health;

    /** 真元 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mana;

    /** 真元上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float max_mana;

    /** 神识 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mind;

    /** 体魄 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float strength;

    /** 内息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float intellect;

    /** 身法 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float agility;

    /** 移动速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float move_speed;

    /** 物攻 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_att;

    /** 物防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_def;

    /** 法攻 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_att;

    /** 法防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_def;

    /** 物理闪避 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_dodge;

    /** 魔法闪避 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_dodge;

    /** 物理命中 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_hit;

    /** 魔法命中 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_hit;

    /** 会心 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float crit;

    /** 会心抵抗 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float crit_def;

    /** 气血回复百分比 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hp_recover_percent;

    /** 真元回复百分比 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mp_recover_percent;

    /** 会心倍率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float crit_coef;

    /** 会心格挡 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float crit_block;

    /** 会心附伤 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float crit_additional_damage;

    /** 秘境获得灵石加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float arena_money_add_percent;

    /** 法术破防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_break;

    /** 物理破防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_break;

    /** 法术格挡 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_block;

    /** 物理格挡 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_block;

    /** 奋起 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float fen_qi;

    /** 每日吐纳次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float tuna_num;

    /** 吐纳加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float tuna_add_percent;

    /** 每日修为丹药服用次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float medicine_num;

    /** 修为丹药服用加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float medicine_add_percent;

    /** 洞府灵气加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float baseqi_add_percent;

    /** 攻击修士神通伤害加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float shen_tong_damage_to_player_add_percent;

    /** 受到修士神通伤害减免 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float shen_tong_damage_to_player_reduce_percent;

    /** 攻击修士法宝伤害加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float fa_bao_damage_to_player_add_percent;

    /** 受到修士法宝伤害减免 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float fa_bao_damage_to_player_reduce_percent;

    /** 造成物理伤害增加 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_damage_add_percent;

    /** 造成法术伤害增加 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_damage_add_percent;

    /** 受到物理伤害减少 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_damage_reduce_percent;

    /** 受到法术伤害减少 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_damage_reduce_percent;

    /** 攻击怪物伤害加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float attack_monster_damage_add_percent;

    /** 受怪物伤害减免 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float take_monster_damage_reduce_percent;

    /** 基础气血加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_hp_add_percent;

    /** 基础真元加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_mp_add_percent;

    /** 基础物攻加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_phy_att_add_percent;

    /** 基础法攻加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_mag_att_add_percent;

    /** 基础物防加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_phy_def_add_percent;

    /** 基础法防加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_mag_def_add_percent;

    /** 基础物理命中加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_phy_hit_add_percent;

    /** 基础法术命中加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_mag_hit_add_percent;

    /** 基础物理闪避加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_phy_dodge_add_percent;

    /** 基础法术闪避加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_mag_dodge_add_percent;

    /** 基础会心值加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_crit_add_percent;

    /** 基础会心抗性值加成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float base_crit_def_add_percent;


    FPbRoleAttribute();
    FPbRoleAttribute(const idlepb::RoleAttribute& Right);
    void FromPb(const idlepb::RoleAttribute& Right);
    void ToPb(idlepb::RoleAttribute* Out) const;
    void Reset();
    void operator=(const idlepb::RoleAttribute& Right);
    bool operator==(const FPbRoleAttribute& Right) const;
    bool operator!=(const FPbRoleAttribute& Right) const;
    void* GetMemberPtrByIndex(int32 Index);
    const void* GetMemberPtrByIndex(int32 Index) const;
    const char* GetMemberTypeNameByIndex(int32 Index) const;
    void SimplePlus(const FPbRoleAttribute& Right);
     
};

namespace idlepb {
class RankData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRankData
{
    GENERATED_BODY();

    /** 等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float exp;

    /** 级别 - 重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 layer;

    /** 级别 - 期 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 stage;

    /** 级别 - 境 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;

    /** 需突破类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbBreakthroughType breakthrough_type;

    /** 突破失败增加的成功率 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 lose_add_probability;

    /** 突破失败恢复时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 lose_recover_timestamp;

    /** 加经验获得的攻击力（物/魔） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 stage_add_att;


    FPbRankData();
    FPbRankData(const idlepb::RankData& Right);
    void FromPb(const idlepb::RankData& Right);
    void ToPb(idlepb::RankData* Out) const;
    void Reset();
    void operator=(const idlepb::RankData& Right);
    bool operator==(const FPbRankData& Right) const;
    bool operator!=(const FPbRankData& Right) const;
     
};

namespace idlepb {
class BreathingReward;
}  // namespace idlepb

/**
 * 每日吐纳奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBreathingReward
{
    GENERATED_BODY();

    /** 生成奖励下标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /** 奖励道具id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> item_id;

    /** 奖励数目 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> num;

    /** 生成时的修炼方向，用于颜色显示 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 dir;

    /** 已领取 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool received;


    FPbBreathingReward();
    FPbBreathingReward(const idlepb::BreathingReward& Right);
    void FromPb(const idlepb::BreathingReward& Right);
    void ToPb(idlepb::BreathingReward* Out) const;
    void Reset();
    void operator=(const idlepb::BreathingReward& Right);
    bool operator==(const FPbBreathingReward& Right) const;
    bool operator!=(const FPbBreathingReward& Right) const;
     
};

namespace idlepb {
class CommonCultivationData;
}  // namespace idlepb

/**
 * 公共修炼数据，独立于修炼方向
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCommonCultivationData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBreathingReward> breathing_rewards;

    /** 是否合并吐纳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool merge_breathing;


    FPbCommonCultivationData();
    FPbCommonCultivationData(const idlepb::CommonCultivationData& Right);
    void FromPb(const idlepb::CommonCultivationData& Right);
    void ToPb(idlepb::CommonCultivationData* Out) const;
    void Reset();
    void operator=(const idlepb::CommonCultivationData& Right);
    bool operator==(const FPbCommonCultivationData& Right) const;
    bool operator!=(const FPbCommonCultivationData& Right) const;
     
};

namespace idlepb {
class CultivationData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCultivationData
{
    GENERATED_BODY();

    /** 等级经验数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRankData rank_data;


    FPbCultivationData();
    FPbCultivationData(const idlepb::CultivationData& Right);
    void FromPb(const idlepb::CultivationData& Right);
    void ToPb(idlepb::CultivationData* Out) const;
    void Reset();
    void operator=(const idlepb::CultivationData& Right);
    bool operator==(const FPbCultivationData& Right) const;
    bool operator!=(const FPbCultivationData& Right) const;
     
};


/**
 * 本日计数
*/
UENUM(BlueprintType)
enum class EPbRoleDailyCounterType : uint8
{
    RDCT_BreathingExerciseTimes = 0 UMETA(DisplayName="今日吐纳次数"),
    RDCT_TakeMedicineTimes = 1 UMETA(DisplayName="今日服药次数"),
    RDCT_LeaderboardClickLikeNum = 2 UMETA(DisplayName="排行榜今日已点赞次数"),
    RDCT_AlchemyTimes = 3 UMETA(DisplayName="今日炼丹次数"),
    RDCT_ForgeTimes = 4 UMETA(DisplayName="今日炼器次数"),
    RDCT_UseExtraMaterialsTimes = 5 UMETA(DisplayName="今日炼器使用辅材次数"),
    RDCT_TotalBreathingExerciseTimes = 6 UMETA(DisplayName="今日总共吐纳次数"),
    RDCT_ForgeProduceQuality_None = 7 UMETA(DisplayName="今日炼制产出品质统计-其他"),
    RDCT_ForgeProduceQuality_White = 8 UMETA(DisplayName="今日炼制产出品质统计-白"),
    RDCT_ForgeProduceQuality_Green = 9 UMETA(DisplayName="今日炼制产出品质统计-绿"),
    RDCT_ForgeProduceQuality_Blue = 10 UMETA(DisplayName="今日炼制产出品质统计-蓝"),
    RDCT_ForgeProduceQuality_Purple = 11 UMETA(DisplayName="今日炼制产出品质统计-紫"),
    RDCT_ForgeProduceQuality_Orange = 12 UMETA(DisplayName="今日炼制产出品质统计-橙"),
    RDCT_ForgeProduceQuality_Red = 13 UMETA(DisplayName="今日炼制产出品质统计-红"),
    RDCT_GiftPackage_Other = 14 UMETA(DisplayName="今日礼包打开数目统计-其它"),
    RDCT_GiftPackage_Phy = 15 UMETA(DisplayName="今日礼包打开数目统计-炼体福袋"),
    RDCT_GiftPackage_Magic = 16 UMETA(DisplayName="今日礼包打开数目统计-修法福袋"),
    RDCT_GiftPackage_Money = 17 UMETA(DisplayName="今日礼包打开数目统计-灵石宝箱"),
    RDCT_GiftPackage_Weapon = 18 UMETA(DisplayName="今日礼包打开数目统计-仙品武器"),
    RDCT_GiftPackage_Treasure = 19 UMETA(DisplayName="今日礼包打开数目统计-仙品法宝"),
    RDCT_GiftPackage_Materials = 20 UMETA(DisplayName="今日礼包打开数目统计-古宝注灵材料"),
    RDCT_GiftPackage_GrabBag = 21 UMETA(DisplayName="今日礼包打开数目统计-福袋"),
    RDCT_GiftPackage_MonsterInvasion = 22 UMETA(DisplayName="今日礼包打开数目统计-神兽入侵宝箱"),
    RDCT_GiftPackage_StorageBag = 23 UMETA(DisplayName="今日礼包打开数目统计-储物袋"),
    RDCT_GiftPackage_Select = 24 UMETA(DisplayName="今日礼包打开数目统计-自选礼包"),
    RDCT_MonsterTowerChallengeTimes = 25 UMETA(DisplayName="今日挑战镇妖塔次数"),
    RDCT_MonsterTowerClosedDoorTrainingTimes = 26 UMETA(DisplayName="今日镇妖塔闭关次数"),
    RDCT_FriendlySoloTimes = 27 UMETA(DisplayName="今日切磋次数"),
    RDCT_SwordPkTimes = 28 UMETA(DisplayName="今日论剑台次数"),
    RDCT_ExchangeHeroCard = 29 UMETA(DisplayName="今日兑换英雄帖次数"),
    RDCT_TodaySeptConstructTimes = 30 UMETA(DisplayName="今日宗门建设次数"),
    RDCT_TodaySearchSeptByNameTimes = 31 UMETA(DisplayName="今日使用名字搜索宗门次数"),
    RDCT_GatherSeptStoneSeconds = 32 UMETA(DisplayName="本日采集矿脉时长(秒)"),
    RDCT_MonsterTowerClickLikeNum = 33 UMETA(DisplayName="镇妖塔排行榜今日已点赞次数"),
    RDCT_FarmlandWatering = 34 UMETA(DisplayName="药园本日浇灌次数"),
    RDCT_FriendRequestNum = 35 UMETA(DisplayName="本日好友请求次数"),
    RDCT_FriendSearchNum = 36 UMETA(DisplayName="本日道友查找次数"),
    RDCT_FuZeReward = 37 UMETA(DisplayName="本日领取福泽"),
};
constexpr EPbRoleDailyCounterType EPbRoleDailyCounterType_Min = EPbRoleDailyCounterType::RDCT_BreathingExerciseTimes;
constexpr EPbRoleDailyCounterType EPbRoleDailyCounterType_Max = EPbRoleDailyCounterType::RDCT_FuZeReward;
constexpr int32 EPbRoleDailyCounterType_ArraySize = static_cast<int32>(EPbRoleDailyCounterType_Max) + 1;
MPROTOCOL_API bool CheckEPbRoleDailyCounterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRoleDailyCounterTypeDescription(EPbRoleDailyCounterType Val);

template <typename Char>
struct fmt::formatter<EPbRoleDailyCounterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRoleDailyCounterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class RoleDailyCounterEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleDailyCounterEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRoleDailyCounterType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbRoleDailyCounterEntry();
    FPbRoleDailyCounterEntry(const idlepb::RoleDailyCounterEntry& Right);
    void FromPb(const idlepb::RoleDailyCounterEntry& Right);
    void ToPb(idlepb::RoleDailyCounterEntry* Out) const;
    void Reset();
    void operator=(const idlepb::RoleDailyCounterEntry& Right);
    bool operator==(const FPbRoleDailyCounterEntry& Right) const;
    bool operator!=(const FPbRoleDailyCounterEntry& Right) const;
     
};

namespace idlepb {
class RoleDailyCounter;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleDailyCounter
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleDailyCounterEntry> entries;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;


    FPbRoleDailyCounter();
    FPbRoleDailyCounter(const idlepb::RoleDailyCounter& Right);
    void FromPb(const idlepb::RoleDailyCounter& Right);
    void ToPb(idlepb::RoleDailyCounter* Out) const;
    void Reset();
    void operator=(const idlepb::RoleDailyCounter& Right);
    bool operator==(const FPbRoleDailyCounter& Right) const;
    bool operator!=(const FPbRoleDailyCounter& Right) const;
     
};


/**
 * 本周计数
*/
UENUM(BlueprintType)
enum class EPbRoleWeeklyCounterType : uint8
{
    RWCT_GatherSeptStoneSeconds = 0 UMETA(DisplayName="本周采集矿脉时长(秒)"),
};
constexpr EPbRoleWeeklyCounterType EPbRoleWeeklyCounterType_Min = EPbRoleWeeklyCounterType::RWCT_GatherSeptStoneSeconds;
constexpr EPbRoleWeeklyCounterType EPbRoleWeeklyCounterType_Max = EPbRoleWeeklyCounterType::RWCT_GatherSeptStoneSeconds;
constexpr int32 EPbRoleWeeklyCounterType_ArraySize = static_cast<int32>(EPbRoleWeeklyCounterType_Max) + 1;
MPROTOCOL_API bool CheckEPbRoleWeeklyCounterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRoleWeeklyCounterTypeDescription(EPbRoleWeeklyCounterType Val);

template <typename Char>
struct fmt::formatter<EPbRoleWeeklyCounterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRoleWeeklyCounterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class RoleWeeklyCounterEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleWeeklyCounterEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRoleWeeklyCounterType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbRoleWeeklyCounterEntry();
    FPbRoleWeeklyCounterEntry(const idlepb::RoleWeeklyCounterEntry& Right);
    void FromPb(const idlepb::RoleWeeklyCounterEntry& Right);
    void ToPb(idlepb::RoleWeeklyCounterEntry* Out) const;
    void Reset();
    void operator=(const idlepb::RoleWeeklyCounterEntry& Right);
    bool operator==(const FPbRoleWeeklyCounterEntry& Right) const;
    bool operator!=(const FPbRoleWeeklyCounterEntry& Right) const;
     
};

namespace idlepb {
class RoleWeeklyCounter;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleWeeklyCounter
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleWeeklyCounterEntry> entries;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;


    FPbRoleWeeklyCounter();
    FPbRoleWeeklyCounter(const idlepb::RoleWeeklyCounter& Right);
    void FromPb(const idlepb::RoleWeeklyCounter& Right);
    void ToPb(idlepb::RoleWeeklyCounter* Out) const;
    void Reset();
    void operator=(const idlepb::RoleWeeklyCounter& Right);
    bool operator==(const FPbRoleWeeklyCounter& Right) const;
    bool operator!=(const FPbRoleWeeklyCounter& Right) const;
     
};

namespace idlepb {
class CurrencyEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCurrencyEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCurrencyType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 num;


    FPbCurrencyEntry();
    FPbCurrencyEntry(const idlepb::CurrencyEntry& Right);
    void FromPb(const idlepb::CurrencyEntry& Right);
    void ToPb(idlepb::CurrencyEntry* Out) const;
    void Reset();
    void operator=(const idlepb::CurrencyEntry& Right);
    bool operator==(const FPbCurrencyEntry& Right) const;
    bool operator!=(const FPbCurrencyEntry& Right) const;
     
};

namespace idlepb {
class CurrencyData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCurrencyData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCurrencyEntry> currencies;


    FPbCurrencyData();
    FPbCurrencyData(const idlepb::CurrencyData& Right);
    void FromPb(const idlepb::CurrencyData& Right);
    void ToPb(idlepb::CurrencyData* Out) const;
    void Reset();
    void operator=(const idlepb::CurrencyData& Right);
    bool operator==(const FPbCurrencyData& Right) const;
    bool operator!=(const FPbCurrencyData& Right) const;
     
};

namespace idlepb {
class EquipPerkEntry;
}  // namespace idlepb

/**
 * 装备词条
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipPerkEntry
{
    GENERATED_BODY();

    /** 词条ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /** 词条品质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality quality;

    /** 词条数值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbEquipPerkEntry();
    FPbEquipPerkEntry(const idlepb::EquipPerkEntry& Right);
    void FromPb(const idlepb::EquipPerkEntry& Right);
    void ToPb(idlepb::EquipPerkEntry* Out) const;
    void Reset();
    void operator=(const idlepb::EquipPerkEntry& Right);
    bool operator==(const FPbEquipPerkEntry& Right) const;
    bool operator!=(const FPbEquipPerkEntry& Right) const;
     
};

namespace idlepb {
class SkillEquipmentAttributes;
}  // namespace idlepb

/**
 * 法宝属性
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSkillEquipmentAttributes
{
    GENERATED_BODY();

    /** 神通冷却 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float cool_down;

    /** 目标数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_num;

    /** 攻击次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 attack_count;

    /** 每次攻击物理百分比加成系数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_coefficient;

    /** 每次攻击固定物理伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_damage;

    /** 每次攻击法术百分比加成系数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_coefficient;

    /** 每次攻击固定法术伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_damage;

    /** 伤害上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float max_damage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAbilityEffectData> effects;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAbilityEffectData> shield_effects;


    FPbSkillEquipmentAttributes();
    FPbSkillEquipmentAttributes(const idlepb::SkillEquipmentAttributes& Right);
    void FromPb(const idlepb::SkillEquipmentAttributes& Right);
    void ToPb(idlepb::SkillEquipmentAttributes* Out) const;
    void Reset();
    void operator=(const idlepb::SkillEquipmentAttributes& Right);
    bool operator==(const FPbSkillEquipmentAttributes& Right) const;
    bool operator!=(const FPbSkillEquipmentAttributes& Right) const;
     
};

namespace idlepb {
class SkillEquipmentData;
}  // namespace idlepb

/**
 * 法宝专属数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSkillEquipmentData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSkillEquipmentAttributes attributes;

    /** 强化属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSkillEquipmentAttributes reinforce_attributes;

    /** 器纹属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSkillEquipmentAttributes qiwen_attributes;

    /** 精炼属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSkillEquipmentAttributes jinglian_attributes;


    FPbSkillEquipmentData();
    FPbSkillEquipmentData(const idlepb::SkillEquipmentData& Right);
    void FromPb(const idlepb::SkillEquipmentData& Right);
    void ToPb(idlepb::SkillEquipmentData* Out) const;
    void Reset();
    void operator=(const idlepb::SkillEquipmentData& Right);
    bool operator==(const FPbSkillEquipmentData& Right) const;
    bool operator!=(const FPbSkillEquipmentData& Right) const;
     
};

namespace idlepb {
class CollectionEntry;
}  // namespace idlepb

/**
 * 古宝数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionEntry
{
    GENERATED_BODY();

    /** 古宝道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /** 注灵等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /** 星级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 star;

    /** 是否已激活 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_activated;

    /** 碎片数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 piece_num;

    /** 生涯计数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 life_num;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float combat_power;


    FPbCollectionEntry();
    FPbCollectionEntry(const idlepb::CollectionEntry& Right);
    void FromPb(const idlepb::CollectionEntry& Right);
    void ToPb(idlepb::CollectionEntry* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionEntry& Right);
    bool operator==(const FPbCollectionEntry& Right) const;
    bool operator!=(const FPbCollectionEntry& Right) const;
     
};

namespace idlepb {
class EquipmentData;
}  // namespace idlepb

/**
 * 装备数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEquipmentData
{
    GENERATED_BODY();

    /** 穿戴格子编号，0表示没有穿戴，取值参见 EquipmentGrid.xlsx */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 slot_index;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;

    /** 铸器师名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString maker_name;

    /** 铸器师ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 maker_roleid;

    /** 词条 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbEquipPerkEntry> perks;

    /** 法宝数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSkillEquipmentData skill_equipment_data;

    /** 古宝数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCollectionEntry collection_data;

    /** 强化等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 reinforce_level;

    /** 精炼等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 refine_level;

    /** 甲字器纹等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_a_level;

    /** 乙字器纹等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_b_level;

    /** 丙字器纹等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_c_level;

    /** 器纹额外属性激活数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_extra_effect_num;

    /** 器纹花费累计 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_moneycast;

    /** 甲字器纹当前经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_current_exp_a;

    /** 乙字器纹当前经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_current_exp_b;

    /** 丙字器纹当前经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_current_exp_c;

    /** 甲字器纹总经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_total_exp_a;

    /** 乙字器纹总经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_total_exp_b;

    /** 丙字器纹总经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qiwen_total_exp_c;

    /** 基础属性 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData base_stats;

    /** 强化属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData reinforce_stats;

    /** 器纹属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData qiwen_stats;

    /** 精炼属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData refine_stats;

    /** 词条属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData perk_stats;

    /** 器纹共鸣属性提升 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData qiwen_resonance_stats;


    FPbEquipmentData();
    FPbEquipmentData(const idlepb::EquipmentData& Right);
    void FromPb(const idlepb::EquipmentData& Right);
    void ToPb(idlepb::EquipmentData* Out) const;
    void Reset();
    void operator=(const idlepb::EquipmentData& Right);
    bool operator==(const FPbEquipmentData& Right) const;
    bool operator!=(const FPbEquipmentData& Right) const;
     
};

namespace idlepb {
class ItemData;
}  // namespace idlepb

/**
 * 道具数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbItemData
{
    GENERATED_BODY();

    /** 道具唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 锁定状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool locked;

    /** 装备数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbEquipmentData equipment_data;


    FPbItemData();
    FPbItemData(const idlepb::ItemData& Right);
    void FromPb(const idlepb::ItemData& Right);
    void ToPb(idlepb::ItemData* Out) const;
    void Reset();
    void operator=(const idlepb::ItemData& Right);
    bool operator==(const FPbItemData& Right) const;
    bool operator!=(const FPbItemData& Right) const;
     
};

namespace idlepb {
class SimpleItemData;
}  // namespace idlepb

/**
 * 简易道具数据，用于多种数据组织
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSimpleItemData
{
    GENERATED_BODY();

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbSimpleItemData();
    FPbSimpleItemData(const idlepb::SimpleItemData& Right);
    void FromPb(const idlepb::SimpleItemData& Right);
    void ToPb(idlepb::SimpleItemData* Out) const;
    void Reset();
    void operator=(const idlepb::SimpleItemData& Right);
    bool operator==(const FPbSimpleItemData& Right) const;
    bool operator!=(const FPbSimpleItemData& Right) const;
     
};

namespace idlepb {
class TemporaryPackageItem;
}  // namespace idlepb

/**
 * 临时包裹中的道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTemporaryPackageItem
{
    GENERATED_BODY();

    /** 唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 id;

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbTemporaryPackageItem();
    FPbTemporaryPackageItem(const idlepb::TemporaryPackageItem& Right);
    void FromPb(const idlepb::TemporaryPackageItem& Right);
    void ToPb(idlepb::TemporaryPackageItem* Out) const;
    void Reset();
    void operator=(const idlepb::TemporaryPackageItem& Right);
    bool operator==(const FPbTemporaryPackageItem& Right) const;
    bool operator!=(const FPbTemporaryPackageItem& Right) const;
     
};

namespace idlepb {
class ArenaExplorationStatisticalItem;
}  // namespace idlepb

/**
 * 秘境探索统计数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaExplorationStatisticalItem
{
    GENERATED_BODY();

    /** 记录时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 time;

    /** 地图名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString mapname;

    /** 击杀数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 killnum;

    /** 重伤数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 deathnum;

    /** 非灵石道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 itemnum;

    /** 灵石数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 moneynum;


    FPbArenaExplorationStatisticalItem();
    FPbArenaExplorationStatisticalItem(const idlepb::ArenaExplorationStatisticalItem& Right);
    void FromPb(const idlepb::ArenaExplorationStatisticalItem& Right);
    void ToPb(idlepb::ArenaExplorationStatisticalItem* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaExplorationStatisticalItem& Right);
    bool operator==(const FPbArenaExplorationStatisticalItem& Right) const;
    bool operator!=(const FPbArenaExplorationStatisticalItem& Right) const;
     
};

namespace idlepb {
class ShopItemBase;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShopItemBase
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 价格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 price;

    /** 库存 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 count;

    /** 已买次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 bought_count;

    /** 配置id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 推荐必买 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool must_buy;

    /** 折扣 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float discount;


    FPbShopItemBase();
    FPbShopItemBase(const idlepb::ShopItemBase& Right);
    void FromPb(const idlepb::ShopItemBase& Right);
    void ToPb(idlepb::ShopItemBase* Out) const;
    void Reset();
    void operator=(const idlepb::ShopItemBase& Right);
    bool operator==(const FPbShopItemBase& Right) const;
    bool operator!=(const FPbShopItemBase& Right) const;
     
};

namespace idlepb {
class ShopItem;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShopItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 价格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 money;

    /** 是否售空 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_sold_out;

    /** 道具数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData item_data;


    FPbShopItem();
    FPbShopItem(const idlepb::ShopItem& Right);
    void FromPb(const idlepb::ShopItem& Right);
    void ToPb(idlepb::ShopItem* Out) const;
    void Reset();
    void operator=(const idlepb::ShopItem& Right);
    bool operator==(const FPbShopItem& Right) const;
    bool operator!=(const FPbShopItem& Right) const;
     
};

namespace idlepb {
class DeluxeShopItem;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDeluxeShopItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 购买次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sellcount;

    /** 折扣 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 discount;

    /** 价格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 money;

    /** 是否售空 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_sold_out;

    /** 道具数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData item_data;

    /** 推荐必买 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool must_buy;


    FPbDeluxeShopItem();
    FPbDeluxeShopItem(const idlepb::DeluxeShopItem& Right);
    void FromPb(const idlepb::DeluxeShopItem& Right);
    void ToPb(idlepb::DeluxeShopItem* Out) const;
    void Reset();
    void operator=(const idlepb::DeluxeShopItem& Right);
    bool operator==(const FPbDeluxeShopItem& Right) const;
    bool operator!=(const FPbDeluxeShopItem& Right) const;
     
};

namespace idlepb {
class RoleVipShopData;
}  // namespace idlepb

/**
 * 角色仙阁商店数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleVipShopData
{
    GENERATED_BODY();

    /** 货架道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShopItemBase> shop_items;

    /** 昨日自动刷新时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_day_refresh_time;

    /** 上周自动刷新时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_week_refresh_time;


    FPbRoleVipShopData();
    FPbRoleVipShopData(const idlepb::RoleVipShopData& Right);
    void FromPb(const idlepb::RoleVipShopData& Right);
    void ToPb(idlepb::RoleVipShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleVipShopData& Right);
    bool operator==(const FPbRoleVipShopData& Right) const;
    bool operator!=(const FPbRoleVipShopData& Right) const;
     
};

namespace idlepb {
class CharacterModelConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCharacterModelConfig
{
    GENERATED_BODY();

    /** 当前体型，上面的字段日后会被弃用 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 skeleton_type;

    /** 外观插槽 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> model_slots;


    FPbCharacterModelConfig();
    FPbCharacterModelConfig(const idlepb::CharacterModelConfig& Right);
    void FromPb(const idlepb::CharacterModelConfig& Right);
    void ToPb(idlepb::CharacterModelConfig* Out) const;
    void Reset();
    void operator=(const idlepb::CharacterModelConfig& Right);
    bool operator==(const FPbCharacterModelConfig& Right) const;
    bool operator!=(const FPbCharacterModelConfig& Right) const;
     
};

namespace idlepb {
class RoleAppearanceShopData;
}  // namespace idlepb

/**
 * 角色外观商店
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleAppearanceShopData
{
    GENERATED_BODY();

    /** 货架1道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShopItemBase> goods1;

    /** 最后一次自动刷新时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_auto_refresh_time;


    FPbRoleAppearanceShopData();
    FPbRoleAppearanceShopData(const idlepb::RoleAppearanceShopData& Right);
    void FromPb(const idlepb::RoleAppearanceShopData& Right);
    void ToPb(idlepb::RoleAppearanceShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleAppearanceShopData& Right);
    bool operator==(const FPbRoleAppearanceShopData& Right) const;
    bool operator!=(const FPbRoleAppearanceShopData& Right) const;
     
};

namespace idlepb {
class AppearanceCollection;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceCollection
{
    GENERATED_BODY();

    /** 当前体型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 group_id;

    /** 持续时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 duration;

    /** 激活时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_date;


    FPbAppearanceCollection();
    FPbAppearanceCollection(const idlepb::AppearanceCollection& Right);
    void FromPb(const idlepb::AppearanceCollection& Right);
    void ToPb(idlepb::AppearanceCollection* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceCollection& Right);
    bool operator==(const FPbAppearanceCollection& Right) const;
    bool operator!=(const FPbAppearanceCollection& Right) const;
     
};

namespace idlepb {
class RoleAppearanceData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleAppearanceData
{
    GENERATED_BODY();

    /** 上次使用化形丹的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_change_skeleton_time;

    /** 装扮收集 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAppearanceCollection> collection;

    /** 当前穿戴外观 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig current_model;

    /** 外观商店 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAppearanceShopData shop_data;


    FPbRoleAppearanceData();
    FPbRoleAppearanceData(const idlepb::RoleAppearanceData& Right);
    void FromPb(const idlepb::RoleAppearanceData& Right);
    void ToPb(idlepb::RoleAppearanceData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleAppearanceData& Right);
    bool operator==(const FPbRoleAppearanceData& Right) const;
    bool operator!=(const FPbRoleAppearanceData& Right) const;
     
};

namespace idlepb {
class AlchemyPackageItem;
}  // namespace idlepb

/**
 * 炼金包裹中的道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyPackageItem
{
    GENERATED_BODY();

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbAlchemyPackageItem();
    FPbAlchemyPackageItem(const idlepb::AlchemyPackageItem& Right);
    void FromPb(const idlepb::AlchemyPackageItem& Right);
    void ToPb(idlepb::AlchemyPackageItem* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyPackageItem& Right);
    bool operator==(const FPbAlchemyPackageItem& Right) const;
    bool operator!=(const FPbAlchemyPackageItem& Right) const;
     
};

namespace idlepb {
class AlchemyMakeData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyMakeData
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 配方等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_degree;

    /** 材料ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 material_id;

    /** 材料品质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality material_quality;

    /** 目标数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_num;

    /** 已炼制数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_num;

    /** 下次完成时间戳(单次) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_done_time;

    /** 已经炼出的道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAlchemyPackageItem> items;

    /** 最近一次产出的道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_produce_item_cfg_id;

    /** 最近一次产出的道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_produce_item_num;

    /** 总开始时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_start_time;

    /** 预计总完成时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_done_time;

    /** 本次炼丹合计获得经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 add_exp;

    /** 已炼制成功数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_successed_num;

    /** 已炼制失败数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_failed_num;


    FPbAlchemyMakeData();
    FPbAlchemyMakeData(const idlepb::AlchemyMakeData& Right);
    void FromPb(const idlepb::AlchemyMakeData& Right);
    void ToPb(idlepb::AlchemyMakeData* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyMakeData& Right);
    bool operator==(const FPbAlchemyMakeData& Right) const;
    bool operator!=(const FPbAlchemyMakeData& Right) const;
     
};

namespace idlepb {
class AlchemyRecipeData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAlchemyRecipeData
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 大保底 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 big_chance;

    /** 小保底 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 small_chance;


    FPbAlchemyRecipeData();
    FPbAlchemyRecipeData(const idlepb::AlchemyRecipeData& Right);
    void FromPb(const idlepb::AlchemyRecipeData& Right);
    void ToPb(idlepb::AlchemyRecipeData* Out) const;
    void Reset();
    void operator=(const idlepb::AlchemyRecipeData& Right);
    bool operator==(const FPbAlchemyRecipeData& Right) const;
    bool operator!=(const FPbAlchemyRecipeData& Right) const;
     
};

namespace idlepb {
class RoleAlchemyData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleAlchemyData
{
    GENERATED_BODY();

    /** 丹师等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 丹师经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 exp;

    /** 正在制造的丹药数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbAlchemyMakeData cur_make_data;

    /** 累计炼丹数量(终身累计) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_refine_num;

    /** 产出品质统计 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> produce_quality_stats;

    /** 学会的配方 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAlchemyRecipeData> recipes;


    FPbRoleAlchemyData();
    FPbRoleAlchemyData(const idlepb::RoleAlchemyData& Right);
    void FromPb(const idlepb::RoleAlchemyData& Right);
    void ToPb(idlepb::RoleAlchemyData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleAlchemyData& Right);
    bool operator==(const FPbRoleAlchemyData& Right) const;
    bool operator!=(const FPbRoleAlchemyData& Right) const;
     
};

namespace idlepb {
class ForgePackageItem;
}  // namespace idlepb

/**
 * 炼器包裹中的道具
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgePackageItem
{
    GENERATED_BODY();

    /** 道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbForgePackageItem();
    FPbForgePackageItem(const idlepb::ForgePackageItem& Right);
    void FromPb(const idlepb::ForgePackageItem& Right);
    void ToPb(idlepb::ForgePackageItem* Out) const;
    void Reset();
    void operator=(const idlepb::ForgePackageItem& Right);
    bool operator==(const FPbForgePackageItem& Right) const;
    bool operator!=(const FPbForgePackageItem& Right) const;
     
};

namespace idlepb {
class ForgeMakeData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeMakeData
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 配方等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_degree;

    /** 材料ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 material_id;

    /** 材料品质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality material_quality;

    /** 目标数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_num;

    /** 已炼制数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_num;

    /** 下次完成时间戳(单次) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_done_time;

    /** 已经炼出的道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbForgePackageItem> items;

    /** 最近一次产出的道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_produce_item_cfg_id;

    /** 最近一次产出的道具数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_produce_item_num;

    /** 总开始时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_start_time;

    /** 预计总完成时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_done_time;

    /** 本次炼丹合计获得经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 add_exp;

    /** 已炼制成功数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_successed_num;

    /** 已炼制失败数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_failed_num;

    /** 辅助材料ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ext_material_id;

    /** 自动出售“下品”道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_sell_poor;

    /** 自动出售“中品”道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_sell_middle;


    FPbForgeMakeData();
    FPbForgeMakeData(const idlepb::ForgeMakeData& Right);
    void FromPb(const idlepb::ForgeMakeData& Right);
    void ToPb(idlepb::ForgeMakeData* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeMakeData& Right);
    bool operator==(const FPbForgeMakeData& Right) const;
    bool operator!=(const FPbForgeMakeData& Right) const;
     
};

namespace idlepb {
class ForgeRecipeData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbForgeRecipeData
{
    GENERATED_BODY();

    /** 配方ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 recipe_id;

    /** 大保底 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 big_chance;

    /** 小保底 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 small_chance;


    FPbForgeRecipeData();
    FPbForgeRecipeData(const idlepb::ForgeRecipeData& Right);
    void FromPb(const idlepb::ForgeRecipeData& Right);
    void ToPb(idlepb::ForgeRecipeData* Out) const;
    void Reset();
    void operator=(const idlepb::ForgeRecipeData& Right);
    bool operator==(const FPbForgeRecipeData& Right) const;
    bool operator!=(const FPbForgeRecipeData& Right) const;
     
};

namespace idlepb {
class LostEquipmentData;
}  // namespace idlepb

/**
 * 消失装备数据（出售、熔炼）
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLostEquipmentData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /** 销毁缘由标记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 tag;

    /** 销毁时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 lost_date;

    /** 装备数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData item_data;


    FPbLostEquipmentData();
    FPbLostEquipmentData(const idlepb::LostEquipmentData& Right);
    void FromPb(const idlepb::LostEquipmentData& Right);
    void ToPb(idlepb::LostEquipmentData* Out) const;
    void Reset();
    void operator=(const idlepb::LostEquipmentData& Right);
    bool operator==(const FPbLostEquipmentData& Right) const;
    bool operator!=(const FPbLostEquipmentData& Right) const;
     
};

namespace idlepb {
class RoleForgeData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleForgeData
{
    GENERATED_BODY();

    /** 器师等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 器师经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 exp;

    /** 正在制造的装备数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbForgeMakeData cur_make_data;

    /** 累计炼器数量(终身累计) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_refine_num;

    /** 产出装备品质统计 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> produce_equip_quality_stats;

    /** 产出法宝品质统计 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> produce_skillequip_quality_stats;

    /** 学会的配方 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbForgeRecipeData> recipes;

    /** 装备找回数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLostEquipmentData> lost_equipment_data;

    /** 用于摧毁道具计数，生成uid */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 destroy_num;


    FPbRoleForgeData();
    FPbRoleForgeData(const idlepb::RoleForgeData& Right);
    void FromPb(const idlepb::RoleForgeData& Right);
    void ToPb(idlepb::RoleForgeData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleForgeData& Right);
    bool operator==(const FPbRoleForgeData& Right) const;
    bool operator!=(const FPbRoleForgeData& Right) const;
     
};

namespace idlepb {
class PillPropertyData;
}  // namespace idlepb

/**
 * 属性丹数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPillPropertyData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 consumed_num;


    FPbPillPropertyData();
    FPbPillPropertyData(const idlepb::PillPropertyData& Right);
    void FromPb(const idlepb::PillPropertyData& Right);
    void ToPb(idlepb::PillPropertyData* Out) const;
    void Reset();
    void operator=(const idlepb::PillPropertyData& Right);
    bool operator==(const FPbPillPropertyData& Right) const;
    bool operator!=(const FPbPillPropertyData& Right) const;
     
};

namespace idlepb {
class GongFaData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGongFaData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbGongFaState state;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float combat_power;


    FPbGongFaData();
    FPbGongFaData(const idlepb::GongFaData& Right);
    void FromPb(const idlepb::GongFaData& Right);
    void ToPb(idlepb::GongFaData* Out) const;
    void Reset();
    void operator=(const idlepb::GongFaData& Right);
    bool operator==(const FPbGongFaData& Right) const;
    bool operator!=(const FPbGongFaData& Right) const;
     
};

namespace idlepb {
class RoleGongFaData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleGongFaData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbGongFaData> data;

    /** 已经激活的功法圆满效果 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> active_max_effect;

    /** 功法点生涯使用计数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 gongfa_point_use_num;


    FPbRoleGongFaData();
    FPbRoleGongFaData(const idlepb::RoleGongFaData& Right);
    void FromPb(const idlepb::RoleGongFaData& Right);
    void ToPb(idlepb::RoleGongFaData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleGongFaData& Right);
    bool operator==(const FPbRoleGongFaData& Right) const;
    bool operator!=(const FPbRoleGongFaData& Right) const;
     
};

namespace idlepb {
class CollectionEntrySaveData;
}  // namespace idlepb

/**
 * 古宝数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionEntrySaveData
{
    GENERATED_BODY();

    /** 古宝道具配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /** 注灵等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /** 星级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 star;

    /** 是否已激活 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_activated;

    /** 碎片数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 piece_num;


    FPbCollectionEntrySaveData();
    FPbCollectionEntrySaveData(const idlepb::CollectionEntrySaveData& Right);
    void FromPb(const idlepb::CollectionEntrySaveData& Right);
    void ToPb(idlepb::CollectionEntrySaveData* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionEntrySaveData& Right);
    bool operator==(const FPbCollectionEntrySaveData& Right) const;
    bool operator!=(const FPbCollectionEntrySaveData& Right) const;
     
};

namespace idlepb {
class CommonCollectionPieceData;
}  // namespace idlepb

/**
 * 古宝通用碎片
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCommonCollectionPieceData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbItemQuality quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 piece_num;


    FPbCommonCollectionPieceData();
    FPbCommonCollectionPieceData(const idlepb::CommonCollectionPieceData& Right);
    void FromPb(const idlepb::CommonCollectionPieceData& Right);
    void ToPb(idlepb::CommonCollectionPieceData* Out) const;
    void Reset();
    void operator=(const idlepb::CommonCollectionPieceData& Right);
    bool operator==(const FPbCommonCollectionPieceData& Right) const;
    bool operator!=(const FPbCommonCollectionPieceData& Right) const;
     
};

namespace idlepb {
class CollectionZoneActiveAwardData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionZoneActiveAwardData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCollectionZoneType zone_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbCollectionZoneActiveAwardData();
    FPbCollectionZoneActiveAwardData(const idlepb::CollectionZoneActiveAwardData& Right);
    void FromPb(const idlepb::CollectionZoneActiveAwardData& Right);
    void ToPb(idlepb::CollectionZoneActiveAwardData* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionZoneActiveAwardData& Right);
    bool operator==(const FPbCollectionZoneActiveAwardData& Right) const;
    bool operator!=(const FPbCollectionZoneActiveAwardData& Right) const;
     
};

namespace idlepb {
class RoleCollectionSaveData;
}  // namespace idlepb

/**
 * 角色古宝存档数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleCollectionSaveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionEntrySaveData> all_entries;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCommonCollectionPieceData> common_pieces;

    /** 已领取完毕奖励的渊源ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> draw_award_done_histories;

    /** 已领取到累计收集奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionZoneActiveAwardData> zone_active_awards;

    /** 下次可重置强化的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_reset_enhance_ticks;


    FPbRoleCollectionSaveData();
    FPbRoleCollectionSaveData(const idlepb::RoleCollectionSaveData& Right);
    void FromPb(const idlepb::RoleCollectionSaveData& Right);
    void ToPb(idlepb::RoleCollectionSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleCollectionSaveData& Right);
    bool operator==(const FPbRoleCollectionSaveData& Right) const;
    bool operator!=(const FPbRoleCollectionSaveData& Right) const;
     
};

namespace idlepb {
class FuZengTuple;
}  // namespace idlepb

/**
 * 福赠领取记录元组
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFuZengTuple
{
    GENERATED_BODY();

    /** 配置id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 领取过的数目 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> num;


    FPbFuZengTuple();
    FPbFuZengTuple(const idlepb::FuZengTuple& Right);
    void FromPb(const idlepb::FuZengTuple& Right);
    void ToPb(idlepb::FuZengTuple* Out) const;
    void Reset();
    void operator=(const idlepb::FuZengTuple& Right);
    bool operator==(const FPbFuZengTuple& Right) const;
    bool operator!=(const FPbFuZengTuple& Right) const;
     
};

namespace idlepb {
class FuZengData;
}  // namespace idlepb

/**
 * 福赠单种功能类数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFuZengData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFuZengType type;

    /** 领取记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFuZengTuple> received_record;

    /** 记录生涯最高值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 max_num;


    FPbFuZengData();
    FPbFuZengData(const idlepb::FuZengData& Right);
    void FromPb(const idlepb::FuZengData& Right);
    void ToPb(idlepb::FuZengData* Out) const;
    void Reset();
    void operator=(const idlepb::FuZengData& Right);
    bool operator==(const FPbFuZengData& Right) const;
    bool operator!=(const FPbFuZengData& Right) const;
     
};

namespace idlepb {
class RoleFuZengData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleFuZengData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFuZengData> data;


    FPbRoleFuZengData();
    FPbRoleFuZengData(const idlepb::RoleFuZengData& Right);
    void FromPb(const idlepb::RoleFuZengData& Right);
    void ToPb(idlepb::RoleFuZengData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleFuZengData& Right);
    bool operator==(const FPbRoleFuZengData& Right) const;
    bool operator!=(const FPbRoleFuZengData& Right) const;
     
};

namespace idlepb {
class RoleFightModeData;
}  // namespace idlepb

/**
 * 角色战斗模式
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleFightModeData
{
    GENERATED_BODY();

    /** 当前战斗模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFightMode cur_mode;

    /** 最后一次攻击时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_attack_ticks;

    /** 最后一次受击时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_defence_ticks;


    FPbRoleFightModeData();
    FPbRoleFightModeData(const idlepb::RoleFightModeData& Right);
    void FromPb(const idlepb::RoleFightModeData& Right);
    void ToPb(idlepb::RoleFightModeData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleFightModeData& Right);
    bool operator==(const FPbRoleFightModeData& Right) const;
    bool operator!=(const FPbRoleFightModeData& Right) const;
     
};

namespace idlepb {
class RoleNormalSettings;
}  // namespace idlepb

/**
 * 角色普通设置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleNormalSettings
{
    GENERATED_BODY();

    /** 攻击锁定方式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAttackLockType attack_lock_type;

    /** 攻击解锁方式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAttackUnlockType attack_unlock_type;

    /** 是否显示取消锁定按钮 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool show_unlock_button;


    FPbRoleNormalSettings();
    FPbRoleNormalSettings(const idlepb::RoleNormalSettings& Right);
    void FromPb(const idlepb::RoleNormalSettings& Right);
    void ToPb(idlepb::RoleNormalSettings* Out) const;
    void Reset();
    void operator=(const idlepb::RoleNormalSettings& Right);
    bool operator==(const FPbRoleNormalSettings& Right) const;
    bool operator!=(const FPbRoleNormalSettings& Right) const;
     
};

namespace idlepb {
class RoleData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 user_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /** 货币 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCurrencyData currency_data;

    /** 炼体数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCultivationData physics_data;

    /** 修法数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCultivationData magic_data;

    /** 当前修炼方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection cultivation_dir;

    /** 最后一次 ExpTick 的时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_exp_cycle_timestamp;

    /** 每日计数器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDailyCounter daily_counter;

    /** 外观 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig model_config;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_world_cfgid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbVector3 last_world_pos;

    /** 下次可传送时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_teleport_time;

    /** 最后解锁到的秘境地图CfgId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_unlock_arena_id;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;

    /** 登录次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 login_count;

    /** 已解锁模块列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlocked_modules;

    /** 角色创建时间(UTC) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 create_time;

    /** 属性丹数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPillPropertyData> pill_property_data;

    /** 战斗模式 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFightModeData fight_mode;

    /** 聚灵阵等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 qi_collector_rank;

    /** 普通设置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleNormalSettings normal_settings;

    /** 角色最后一次离线时间点(UTC) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 offline_time;

    /** 每周计数器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleWeeklyCounter weekly_counter;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_arena_world_cfgid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbVector3 last_arena_world_pos;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsData game_stats;

    /** 普通秘境 + 中立秘境 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_all_arena_world_cfgid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbVector3 last_all_arena_world_pos;


    FPbRoleData();
    FPbRoleData(const idlepb::RoleData& Right);
    void FromPb(const idlepb::RoleData& Right);
    void ToPb(idlepb::RoleData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleData& Right);
    bool operator==(const FPbRoleData& Right) const;
    bool operator!=(const FPbRoleData& Right) const;
     
};

namespace idlepb {
class SimpleAbilityData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSimpleAbilityData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 grade;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 study_grade;


    FPbSimpleAbilityData();
    FPbSimpleAbilityData(const idlepb::SimpleAbilityData& Right);
    void FromPb(const idlepb::SimpleAbilityData& Right);
    void ToPb(idlepb::SimpleAbilityData* Out) const;
    void Reset();
    void operator=(const idlepb::SimpleAbilityData& Right);
    bool operator==(const FPbSimpleAbilityData& Right) const;
    bool operator!=(const FPbSimpleAbilityData& Right) const;
     
};

namespace idlepb {
class SimpleGongFaData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSimpleGongFaData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_full;


    FPbSimpleGongFaData();
    FPbSimpleGongFaData(const idlepb::SimpleGongFaData& Right);
    void FromPb(const idlepb::SimpleGongFaData& Right);
    void ToPb(idlepb::SimpleGongFaData* Out) const;
    void Reset();
    void operator=(const idlepb::SimpleGongFaData& Right);
    bool operator==(const FPbSimpleGongFaData& Right) const;
    bool operator!=(const FPbSimpleGongFaData& Right) const;
     
};

namespace idlepb {
class RoleInfo;
}  // namespace idlepb

/**
 * 玩家预览信息，RoleData的简化
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 user_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 create_time;

    /** 主修方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection cultivation_main_dir;

    /** 主修境界 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cultivation_main_rank;

    /** 辅修方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection cultivation_second_dir;

    /** 主修境界 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cultivation_second_rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig character_model;

    /** 称号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> title;

    /** 总战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;

    /** 当前装备 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> equipments;

    /** 已解锁装备格子 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlocked_equipment_slots;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSeptPosition sept_position;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString sept_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sept_logo;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsAllModuleData all_stats_data;

    /** 已装配技能列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleAbilityData> slotted_abilities;

    /** 未装配技能列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleAbilityData> unslotted_abilities;

    /** 功法 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleGongFaData> gong_fa_entries;


    FPbRoleInfo();
    FPbRoleInfo(const idlepb::RoleInfo& Right);
    void FromPb(const idlepb::RoleInfo& Right);
    void ToPb(idlepb::RoleInfo* Out) const;
    void Reset();
    void operator=(const idlepb::RoleInfo& Right);
    bool operator==(const FPbRoleInfo& Right) const;
    bool operator!=(const FPbRoleInfo& Right) const;
     
};

namespace idlepb {
class RoleInventoryData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleInventoryData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> items;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 hp_pill_cooldown_expire_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 mp_pill_cooldown_expire_time;

    /** 已解锁装备格子 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlocked_equipment_slots;

    /** 背包空间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 inventory_space_num;


    FPbRoleInventoryData();
    FPbRoleInventoryData(const idlepb::RoleInventoryData& Right);
    void FromPb(const idlepb::RoleInventoryData& Right);
    void ToPb(idlepb::RoleInventoryData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleInventoryData& Right);
    bool operator==(const FPbRoleInventoryData& Right) const;
    bool operator!=(const FPbRoleInventoryData& Right) const;
     
};

namespace idlepb {
class RoleTemporaryPackageData;
}  // namespace idlepb

/**
 * 临时包裹
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleTemporaryPackageData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbTemporaryPackageItem> items;

    /** 最后一次提取的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_extract_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_item_id;


    FPbRoleTemporaryPackageData();
    FPbRoleTemporaryPackageData(const idlepb::RoleTemporaryPackageData& Right);
    void FromPb(const idlepb::RoleTemporaryPackageData& Right);
    void ToPb(idlepb::RoleTemporaryPackageData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleTemporaryPackageData& Right);
    bool operator==(const FPbRoleTemporaryPackageData& Right) const;
    bool operator!=(const FPbRoleTemporaryPackageData& Right) const;
     
};

namespace idlepb {
class RoleArenaExplorationStatisticalData;
}  // namespace idlepb

/**
 * 角色秘境探索统计数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleArenaExplorationStatisticalData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbArenaExplorationStatisticalItem> items;


    FPbRoleArenaExplorationStatisticalData();
    FPbRoleArenaExplorationStatisticalData(const idlepb::RoleArenaExplorationStatisticalData& Right);
    void FromPb(const idlepb::RoleArenaExplorationStatisticalData& Right);
    void ToPb(idlepb::RoleArenaExplorationStatisticalData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleArenaExplorationStatisticalData& Right);
    bool operator==(const FPbRoleArenaExplorationStatisticalData& Right) const;
    bool operator!=(const FPbRoleArenaExplorationStatisticalData& Right) const;
     
};

namespace idlepb {
class QuestProgress;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbQuestProgress
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> progress;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 state;


    FPbQuestProgress();
    FPbQuestProgress(const idlepb::QuestProgress& Right);
    void FromPb(const idlepb::QuestProgress& Right);
    void ToPb(idlepb::QuestProgress* Out) const;
    void Reset();
    void operator=(const idlepb::QuestProgress& Right);
    bool operator==(const FPbQuestProgress& Right) const;
    bool operator!=(const FPbQuestProgress& Right) const;
     
};

namespace idlepb {
class RoleQuestData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleQuestData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> accepted_quests;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> finished_quests;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbQuestProgress> quests_progress;


    FPbRoleQuestData();
    FPbRoleQuestData(const idlepb::RoleQuestData& Right);
    void FromPb(const idlepb::RoleQuestData& Right);
    void ToPb(idlepb::RoleQuestData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleQuestData& Right);
    bool operator==(const FPbRoleQuestData& Right) const;
    bool operator!=(const FPbRoleQuestData& Right) const;
     
};

namespace idlepb {
class RoleShopData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleShopData
{
    GENERATED_BODY();

    /** 坊市货架道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShopItem> items;

    /** 今日已手动刷新次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_manual_refresh_num;

    /** 最后一次自动刷新时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_auto_refresh_time;

    /** 最后一次重置时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;

    /** 保底计数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 guarantee_refresh_num;


    FPbRoleShopData();
    FPbRoleShopData(const idlepb::RoleShopData& Right);
    void FromPb(const idlepb::RoleShopData& Right);
    void ToPb(idlepb::RoleShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleShopData& Right);
    bool operator==(const FPbRoleShopData& Right) const;
    bool operator!=(const FPbRoleShopData& Right) const;
     
};

namespace idlepb {
class RoleDeluxeShopData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleDeluxeShopData
{
    GENERATED_BODY();

    /** 天机阁货架道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbDeluxeShopItem> items;

    /** 今日已手动刷新次数，通过天机令 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_manual_refresh_num_item;

    /** 今日已手动刷新次数，通过机缘 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_manual_refresh_num_gold;

    /** 最后一次自动刷新时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_auto_refresh_time;

    /** 最后一次重置时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;


    FPbRoleDeluxeShopData();
    FPbRoleDeluxeShopData(const idlepb::RoleDeluxeShopData& Right);
    void FromPb(const idlepb::RoleDeluxeShopData& Right);
    void ToPb(idlepb::RoleDeluxeShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleDeluxeShopData& Right);
    bool operator==(const FPbRoleDeluxeShopData& Right) const;
    bool operator!=(const FPbRoleDeluxeShopData& Right) const;
     
};

namespace idlepb {
class MailAttachment;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMailAttachment
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool received;


    FPbMailAttachment();
    FPbMailAttachment(const idlepb::MailAttachment& Right);
    void FromPb(const idlepb::MailAttachment& Right);
    void ToPb(idlepb::MailAttachment* Out) const;
    void Reset();
    void operator=(const idlepb::MailAttachment& Right);
    bool operator==(const FPbMailAttachment& Right) const;
    bool operator!=(const FPbMailAttachment& Right) const;
     
};

namespace idlepb {
class Mail;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbMail
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbMailType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString title;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString subtitle;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString body_text;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString sender;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMailAttachment> attachments;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_date;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 keep_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_read;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_received;

    /** 装备数据类附件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbItemData> equipments;


    FPbMail();
    FPbMail(const idlepb::Mail& Right);
    void FromPb(const idlepb::Mail& Right);
    void ToPb(idlepb::Mail* Out) const;
    void Reset();
    void operator=(const idlepb::Mail& Right);
    bool operator==(const FPbMail& Right) const;
    bool operator!=(const FPbMail& Right) const;
     
};

namespace idlepb {
class RoleMailData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleMailData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMail> mail_box;

    /** 生涯收到的邮件数，用于生产邮件唯一id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_num;

    /** 计数领取过的系统邮件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMapValueInt32> system_mail_counter;


    FPbRoleMailData();
    FPbRoleMailData(const idlepb::RoleMailData& Right);
    void FromPb(const idlepb::RoleMailData& Right);
    void ToPb(idlepb::RoleMailData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleMailData& Right);
    bool operator==(const FPbRoleMailData& Right) const;
    bool operator!=(const FPbRoleMailData& Right) const;
     
};

namespace idlepb {
class OfflineAwardSummary;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbOfflineAwardSummary
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;

    /** 离线累计时长 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 time_during;

    /** 离线累计经验 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 add_exp;

    /** 离线累计属性 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 add_attr;


    FPbOfflineAwardSummary();
    FPbOfflineAwardSummary(const idlepb::OfflineAwardSummary& Right);
    void FromPb(const idlepb::OfflineAwardSummary& Right);
    void ToPb(idlepb::OfflineAwardSummary* Out) const;
    void Reset();
    void operator=(const idlepb::OfflineAwardSummary& Right);
    bool operator==(const FPbOfflineAwardSummary& Right) const;
    bool operator!=(const FPbOfflineAwardSummary& Right) const;
     
};

namespace idlepb {
class RoleOfflineData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleOfflineData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_exp_value;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_attr_value;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbOfflineAwardSummary last_award_summary;


    FPbRoleOfflineData();
    FPbRoleOfflineData(const idlepb::RoleOfflineData& Right);
    void FromPb(const idlepb::RoleOfflineData& Right);
    void ToPb(idlepb::RoleOfflineData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleOfflineData& Right);
    bool operator==(const FPbRoleOfflineData& Right) const;
    bool operator!=(const FPbRoleOfflineData& Right) const;
     
};

namespace idlepb {
class PillElixirData;
}  // namespace idlepb

/**
 * 单种秘药数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPillElixirData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 holding_num;


    FPbPillElixirData();
    FPbPillElixirData(const idlepb::PillElixirData& Right);
    void FromPb(const idlepb::PillElixirData& Right);
    void ToPb(idlepb::PillElixirData* Out) const;
    void Reset();
    void operator=(const idlepb::PillElixirData& Right);
    bool operator==(const FPbPillElixirData& Right) const;
    bool operator!=(const FPbPillElixirData& Right) const;
     
};

namespace idlepb {
class RolePillElixirData;
}  // namespace idlepb

/**
 * 秘药/属性丹 数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRolePillElixirData
{
    GENERATED_BODY();

    /** 秘药数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPillElixirData> pill_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_double;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_exp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 limit_property;


    FPbRolePillElixirData();
    FPbRolePillElixirData(const idlepb::RolePillElixirData& Right);
    void FromPb(const idlepb::RolePillElixirData& Right);
    void ToPb(idlepb::RolePillElixirData* Out) const;
    void Reset();
    void operator=(const idlepb::RolePillElixirData& Right);
    bool operator==(const FPbRolePillElixirData& Right) const;
    bool operator!=(const FPbRolePillElixirData& Right) const;
     
};

namespace idlepb {
class AbilityEffectDefData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAbilityEffectDefData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float duration;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float period;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 duration_policy;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float y;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float z;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float m;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float n;


    FPbAbilityEffectDefData();
    FPbAbilityEffectDefData(const idlepb::AbilityEffectDefData& Right);
    void FromPb(const idlepb::AbilityEffectDefData& Right);
    void ToPb(idlepb::AbilityEffectDefData* Out) const;
    void Reset();
    void operator=(const idlepb::AbilityEffectDefData& Right);
    bool operator==(const FPbAbilityEffectDefData& Right) const;
    bool operator!=(const FPbAbilityEffectDefData& Right) const;
     
};

namespace idlepb {
class AbilityData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAbilityData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 grade;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 activetime_utc;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float activetime_world;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 unique_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 study_grade;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float cooldown;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float target_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float target_distance;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float target_catchdistance;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float attack_count;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_coefficient;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_damage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mana_coefficient;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mana_damage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float item_maxdamage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_cfgid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAbilityEffectDefData> effect_defs;


    FPbAbilityData();
    FPbAbilityData(const idlepb::AbilityData& Right);
    void FromPb(const idlepb::AbilityData& Right);
    void ToPb(idlepb::AbilityData* Out) const;
    void Reset();
    void operator=(const idlepb::AbilityData& Right);
    bool operator==(const FPbAbilityData& Right) const;
    bool operator!=(const FPbAbilityData& Right) const;
     
};

namespace idlepb {
class PlayerAbilityData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerAbilityData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAbilityData> abilities;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMapValueInt32> slotted_abilites;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> active_queue;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_shiled_first;

    /** 神通一键重置冷却时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 revert_all_skill_cooldown;


    FPbPlayerAbilityData();
    FPbPlayerAbilityData(const idlepb::PlayerAbilityData& Right);
    void FromPb(const idlepb::PlayerAbilityData& Right);
    void ToPb(idlepb::PlayerAbilityData* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerAbilityData& Right);
    bool operator==(const FPbPlayerAbilityData& Right) const;
    bool operator!=(const FPbPlayerAbilityData& Right) const;
     
};

namespace idlepb {
class RoleZasData;
}  // namespace idlepb

/**
 * ZAS数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleZasData
{
    GENERATED_BODY();

    /** 技能系统数据版本号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 zas_version;

    /** 角色技能数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerAbilityData zas_ability;

    /** 角色神通心得生涯使用数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 shentong_upgrade_point_use_num;


    FPbRoleZasData();
    FPbRoleZasData(const idlepb::RoleZasData& Right);
    void FromPb(const idlepb::RoleZasData& Right);
    void ToPb(idlepb::RoleZasData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleZasData& Right);
    bool operator==(const FPbRoleZasData& Right) const;
    bool operator!=(const FPbRoleZasData& Right) const;
     
};

namespace idlepb {
class AbilityPKResult;
}  // namespace idlepb

/**
 * 技能一次结算的结果(FZPKResult)
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAbilityPKResult
{
    GENERATED_BODY();

    /** 攻方 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 instigator;

    /** 守方 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 target;

    /** 伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float damage;

    /** 附加伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float additional_damage;

    /** 是否命中 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_hit;

    /** 是否会心 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_critical;

    /** 是否反击 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_countered;

    /** 是否极限伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_extremedamage;

    /** 反击伤害值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float countereddamage;

    /** 当前攻击次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 currentattackcount;

    /** 护盾吸收的伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float shield_suckdamage;

    /** 是否反击会心 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_countered_critical;

    /** 反击护盾吸收伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float countered_shield_suckdamage;


    FPbAbilityPKResult();
    FPbAbilityPKResult(const idlepb::AbilityPKResult& Right);
    void FromPb(const idlepb::AbilityPKResult& Right);
    void ToPb(idlepb::AbilityPKResult* Out) const;
    void Reset();
    void operator=(const idlepb::AbilityPKResult& Right);
    bool operator==(const FPbAbilityPKResult& Right) const;
    bool operator!=(const FPbAbilityPKResult& Right) const;
     
};


/**
 * 释放技能结果码
*/
UENUM(BlueprintType)
enum class EPbAbilityActiveErrorCode : uint8
{
    AbilityActiveErrorCode_Success = 0 UMETA(DisplayName="正常"),
    AbilityActiveErrorCode_Timeout = 1 UMETA(DisplayName="超时"),
    AbilityActiveErrorCode_InvalidAbility = 2 UMETA(DisplayName="无效技能"),
    AbilityActiveErrorCode_Cooldown = 11 UMETA(DisplayName="CD不满足"),
    AbilityActiveErrorCode_CostNotEnough = 12 UMETA(DisplayName="消耗不够"),
    AbilityActiveErrorCode_Silent = 13 UMETA(DisplayName="沉默状态"),
    AbilityActiveErrorCode_Freezing = 14 UMETA(DisplayName="冰冻状态"),
    AbilityActiveErrorCode_Death = 15 UMETA(DisplayName="死亡状态"),
    AbilityActiveErrorCode_OwnerCheck = 16 UMETA(DisplayName="Owner非法"),
    AbilityActiveErrorCode_CommonCooldown = 17 UMETA(DisplayName="公共CD不满足"),
};
constexpr EPbAbilityActiveErrorCode EPbAbilityActiveErrorCode_Min = EPbAbilityActiveErrorCode::AbilityActiveErrorCode_Success;
constexpr EPbAbilityActiveErrorCode EPbAbilityActiveErrorCode_Max = EPbAbilityActiveErrorCode::AbilityActiveErrorCode_CommonCooldown;
constexpr int32 EPbAbilityActiveErrorCode_ArraySize = static_cast<int32>(EPbAbilityActiveErrorCode_Max) + 1;
MPROTOCOL_API bool CheckEPbAbilityActiveErrorCodeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbAbilityActiveErrorCodeDescription(EPbAbilityActiveErrorCode Val);

template <typename Char>
struct fmt::formatter<EPbAbilityActiveErrorCode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbAbilityActiveErrorCode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class AbilityActiveResult;
}  // namespace idlepb

/**
 * 释放技能的结果
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAbilityActiveResult
{
    GENERATED_BODY();

    /** 技能释放者 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 eid;

    /** 技能 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ability;

    /** 技能唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 ability_unique_id;

    /** 错误码 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbAbilityActiveErrorCode error;

    /** 伤害结果 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAbilityPKResult> results;

    /** 添加上的EffectID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> effects;


    FPbAbilityActiveResult();
    FPbAbilityActiveResult(const idlepb::AbilityActiveResult& Right);
    void FromPb(const idlepb::AbilityActiveResult& Right);
    void ToPb(idlepb::AbilityActiveResult* Out) const;
    void Reset();
    void operator=(const idlepb::AbilityActiveResult& Right);
    bool operator==(const FPbAbilityActiveResult& Right) const;
    bool operator!=(const FPbAbilityActiveResult& Right) const;
     
};

namespace idlepb {
class ShanhetuItem;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShanhetuItem
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
    int32 weight;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 score;


    FPbShanhetuItem();
    FPbShanhetuItem(const idlepb::ShanhetuItem& Right);
    void FromPb(const idlepb::ShanhetuItem& Right);
    void ToPb(idlepb::ShanhetuItem* Out) const;
    void Reset();
    void operator=(const idlepb::ShanhetuItem& Right);
    bool operator==(const FPbShanhetuItem& Right) const;
    bool operator!=(const FPbShanhetuItem& Right) const;
     
};

namespace idlepb {
class ShanhetuRecord;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShanhetuRecord
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 uid;

    /** 图纸道具id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /** 规模 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 scale_id;

    /** 收获评分->评分称号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 score;

    /** 产出道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShanhetuItem> items;

    /** 使用时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 use_time;


    FPbShanhetuRecord();
    FPbShanhetuRecord(const idlepb::ShanhetuRecord& Right);
    void FromPb(const idlepb::ShanhetuRecord& Right);
    void ToPb(idlepb::ShanhetuRecord* Out) const;
    void Reset();
    void operator=(const idlepb::ShanhetuRecord& Right);
    bool operator==(const FPbShanhetuRecord& Right) const;
    bool operator!=(const FPbShanhetuRecord& Right) const;
     
};

namespace idlepb {
class ShanhetuBlock;
}  // namespace idlepb

/**
 * 山河图地块
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShanhetuBlock
{
    GENERATED_BODY();

    /** 地块类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /** 地块品质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quality;

    /** 产出道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuItem item;

    /** 地块事件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 event_cfg_id;


    FPbShanhetuBlock();
    FPbShanhetuBlock(const idlepb::ShanhetuBlock& Right);
    void FromPb(const idlepb::ShanhetuBlock& Right);
    void ToPb(idlepb::ShanhetuBlock* Out) const;
    void Reset();
    void operator=(const idlepb::ShanhetuBlock& Right);
    bool operator==(const FPbShanhetuBlock& Right) const;
    bool operator!=(const FPbShanhetuBlock& Right) const;
     
};

namespace idlepb {
class ShanhetuBlockRow;
}  // namespace idlepb

/**
 * 一行山河图地块
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShanhetuBlockRow
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShanhetuBlock> blocks;


    FPbShanhetuBlockRow();
    FPbShanhetuBlockRow(const idlepb::ShanhetuBlockRow& Right);
    void FromPb(const idlepb::ShanhetuBlockRow& Right);
    void ToPb(idlepb::ShanhetuBlockRow* Out) const;
    void Reset();
    void operator=(const idlepb::ShanhetuBlockRow& Right);
    bool operator==(const FPbShanhetuBlockRow& Right) const;
    bool operator!=(const FPbShanhetuBlockRow& Right) const;
     
};

namespace idlepb {
class ShanhetuMap;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbShanhetuMap
{
    GENERATED_BODY();

    /** 是否探索完成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool done;

    /** 当前进度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 current_row;

    /** 当前记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuRecord record;

    /** 生成地图 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShanhetuBlockRow> map;


    FPbShanhetuMap();
    FPbShanhetuMap(const idlepb::ShanhetuMap& Right);
    void FromPb(const idlepb::ShanhetuMap& Right);
    void ToPb(idlepb::ShanhetuMap* Out) const;
    void Reset();
    void operator=(const idlepb::ShanhetuMap& Right);
    bool operator==(const FPbShanhetuMap& Right) const;
    bool operator!=(const FPbShanhetuMap& Right) const;
     
};

namespace idlepb {
class RoleShanhetuData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleShanhetuData
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

    /** 当前开包内容，及其进度，如果异常下线，则下次上限通过邮件发送 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuMap current_map;

    /** 生涯开包次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbShanhetuRecord> records;

    /** 清空每周记录时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_update_date;


    FPbRoleShanhetuData();
    FPbRoleShanhetuData(const idlepb::RoleShanhetuData& Right);
    void FromPb(const idlepb::RoleShanhetuData& Right);
    void ToPb(idlepb::RoleShanhetuData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleShanhetuData& Right);
    bool operator==(const FPbRoleShanhetuData& Right) const;
    bool operator!=(const FPbRoleShanhetuData& Right) const;
     
};

namespace idlepb {
class RoleLeaderboardData;
}  // namespace idlepb

/**
 * 排行榜玩家功能数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleLeaderboardData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /** 获赞次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 blike_num;

    /** 最后一次重置时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;

    /** 上榜留言 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString rank_message;

    /** 最强武器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData weapon;

    /** 最强防具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData ammor;

    /** 最强饰品 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData jewlery;

    /** 最强法宝 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbItemData skill_weapon;

    /** 历史最高 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuRecord shanhetu_history;

    /** 本周最高 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbShanhetuRecord shanhetu_week;

    /** 镇妖塔获赞次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 monster_tower_blike_num;

    /** 已经领取的挑战奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> has_received_challange_reward;

    /** 福泽定格时的玩家主修方向等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 fuze_rank;

    /** 累计福泽天数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 fuze_days;

    /** 福泽定格时的玩家主修方向经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 fuze_exp;

    /** 当天服务器计数的福泽排名 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 fuze_leaderboard_rank;


    FPbRoleLeaderboardData();
    FPbRoleLeaderboardData(const idlepb::RoleLeaderboardData& Right);
    void FromPb(const idlepb::RoleLeaderboardData& Right);
    void ToPb(idlepb::RoleLeaderboardData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleLeaderboardData& Right);
    bool operator==(const FPbRoleLeaderboardData& Right) const;
    bool operator!=(const FPbRoleLeaderboardData& Right) const;
     
};

namespace idlepb {
class RoleMonsterTowerData;
}  // namespace idlepb

/**
 * 镇妖塔数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleMonsterTowerData
{
    GENERATED_BODY();

    /** 最后通关的镇妖塔层数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_floor;

    /** 挂机 - 已持续时间，领取后清零 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 idle_during_ticks;


    FPbRoleMonsterTowerData();
    FPbRoleMonsterTowerData(const idlepb::RoleMonsterTowerData& Right);
    void FromPb(const idlepb::RoleMonsterTowerData& Right);
    void ToPb(idlepb::RoleMonsterTowerData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleMonsterTowerData& Right);
    bool operator==(const FPbRoleMonsterTowerData& Right) const;
    bool operator!=(const FPbRoleMonsterTowerData& Right) const;
     
};

namespace idlepb {
class RoleDungeonKillAllData;
}  // namespace idlepb

/**
 * 剿灭副本数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleDungeonKillAllData
{
    GENERATED_BODY();

    /** 最后通关的剿灭副本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> done_uid;


    FPbRoleDungeonKillAllData();
    FPbRoleDungeonKillAllData(const idlepb::RoleDungeonKillAllData& Right);
    void FromPb(const idlepb::RoleDungeonKillAllData& Right);
    void ToPb(idlepb::RoleDungeonKillAllData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleDungeonKillAllData& Right);
    bool operator==(const FPbRoleDungeonKillAllData& Right) const;
    bool operator!=(const FPbRoleDungeonKillAllData& Right) const;
     
};

namespace idlepb {
class RoleDungeonSurviveData;
}  // namespace idlepb

/**
 * 生存副本数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleDungeonSurviveData
{
    GENERATED_BODY();

    /** 最后通关的剿灭副本 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> done_uid;


    FPbRoleDungeonSurviveData();
    FPbRoleDungeonSurviveData(const idlepb::RoleDungeonSurviveData& Right);
    void FromPb(const idlepb::RoleDungeonSurviveData& Right);
    void ToPb(idlepb::RoleDungeonSurviveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleDungeonSurviveData& Right);
    bool operator==(const FPbRoleDungeonSurviveData& Right) const;
    bool operator!=(const FPbRoleDungeonSurviveData& Right) const;
     
};

namespace idlepb {
class BossInvasionRewardEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBossInvasionRewardEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 unique_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 arena_cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;


    FPbBossInvasionRewardEntry();
    FPbBossInvasionRewardEntry(const idlepb::BossInvasionRewardEntry& Right);
    void FromPb(const idlepb::BossInvasionRewardEntry& Right);
    void ToPb(idlepb::BossInvasionRewardEntry* Out) const;
    void Reset();
    void operator=(const idlepb::BossInvasionRewardEntry& Right);
    bool operator==(const FPbBossInvasionRewardEntry& Right) const;
    bool operator!=(const FPbBossInvasionRewardEntry& Right) const;
     
};

namespace idlepb {
class BossInvasionKillRewardData;
}  // namespace idlepb

/**
 * 尾刀奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBossInvasionKillRewardData
{
    GENERATED_BODY();

    /** 本轮入侵开始时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 start_ticks;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBossInvasionRewardEntry> rewards;

    /** 奖励是否已领取 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_draw_done;

    /** 已领取的奖励 UniqueId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 drawed_unique_id;


    FPbBossInvasionKillRewardData();
    FPbBossInvasionKillRewardData(const idlepb::BossInvasionKillRewardData& Right);
    void FromPb(const idlepb::BossInvasionKillRewardData& Right);
    void ToPb(idlepb::BossInvasionKillRewardData* Out) const;
    void Reset();
    void operator=(const idlepb::BossInvasionKillRewardData& Right);
    bool operator==(const FPbBossInvasionKillRewardData& Right) const;
    bool operator!=(const FPbBossInvasionKillRewardData& Right) const;
     
};

namespace idlepb {
class BossInvasionDamageRewardData;
}  // namespace idlepb

/**
 * 伤害排行奖励
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBossInvasionDamageRewardData
{
    GENERATED_BODY();

    /** 入侵日期 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 start_ticks;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBossInvasionRewardEntry> rewards;


    FPbBossInvasionDamageRewardData();
    FPbBossInvasionDamageRewardData(const idlepb::BossInvasionDamageRewardData& Right);
    void FromPb(const idlepb::BossInvasionDamageRewardData& Right);
    void ToPb(idlepb::BossInvasionDamageRewardData* Out) const;
    void Reset();
    void operator=(const idlepb::BossInvasionDamageRewardData& Right);
    bool operator==(const FPbBossInvasionDamageRewardData& Right) const;
    bool operator!=(const FPbBossInvasionDamageRewardData& Right) const;
     
};

namespace idlepb {
class RoleBossInvasionData;
}  // namespace idlepb

/**
 * BOSS入侵数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleBossInvasionData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_ticks;

    /** 尾刀奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBossInvasionKillRewardData kill_reward;

    /** 伤害排行奖励 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBossInvasionDamageRewardData> damage_reward;

    /** 已领取的伤害排行奖励 UniqueId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 drawed_unique_id;


    FPbRoleBossInvasionData();
    FPbRoleBossInvasionData(const idlepb::RoleBossInvasionData& Right);
    void FromPb(const idlepb::RoleBossInvasionData& Right);
    void ToPb(idlepb::RoleBossInvasionData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleBossInvasionData& Right);
    bool operator==(const FPbRoleBossInvasionData& Right) const;
    bool operator!=(const FPbRoleBossInvasionData& Right) const;
     
};

namespace idlepb {
class RoleMasiveData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleMasiveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbStringKeyInt32ValueEntry> user_vars;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_self_unique_id;


    FPbRoleMasiveData();
    FPbRoleMasiveData(const idlepb::RoleMasiveData& Right);
    void FromPb(const idlepb::RoleMasiveData& Right);
    void ToPb(idlepb::RoleMasiveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleMasiveData& Right);
    bool operator==(const FPbRoleMasiveData& Right) const;
    bool operator!=(const FPbRoleMasiveData& Right) const;
     
};

namespace idlepb {
class CheckTask;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCheckTask
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 task_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 need_num;

    /** 提供的活跃点数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 point;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 progress;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_submitted;


    FPbCheckTask();
    FPbCheckTask(const idlepb::CheckTask& Right);
    void FromPb(const idlepb::CheckTask& Right);
    void ToPb(idlepb::CheckTask* Out) const;
    void Reset();
    void operator=(const idlepb::CheckTask& Right);
    bool operator==(const FPbCheckTask& Right) const;
    bool operator!=(const FPbCheckTask& Right) const;
     
};

namespace idlepb {
class RoleChecklistData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleChecklistData
{
    GENERATED_BODY();

    /** 活跃点数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 day_point;

    /** 活跃点数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 week_point;

    /** 任务 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCheckTask> day_tasks;

    /** 任务 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCheckTask> week_tasks;

    /** 领取次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 day_received_time;

    /** 领取次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 week_received_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_day_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_week_time;

    /** 本日参与神兽入侵时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 boss_invasion_time;

    /** 锁定用于生成本日奖励的Degree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree_locked_day;

    /** 锁定用于生成本周奖励的Degree */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree_locked_week;


    FPbRoleChecklistData();
    FPbRoleChecklistData(const idlepb::RoleChecklistData& Right);
    void FromPb(const idlepb::RoleChecklistData& Right);
    void ToPb(idlepb::RoleChecklistData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleChecklistData& Right);
    bool operator==(const FPbRoleChecklistData& Right) const;
    bool operator!=(const FPbRoleChecklistData& Right) const;
     
};

namespace idlepb {
class RoleCommonItemExchangeData;
}  // namespace idlepb

/**
 * 角色道具兑换功能数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleCommonItemExchangeData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_day;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_week;

    /** 每日兑换 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMapValueInt32> item_exchange_day;

    /** 每周兑换 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMapValueInt32> item_exchange_week;


    FPbRoleCommonItemExchangeData();
    FPbRoleCommonItemExchangeData(const idlepb::RoleCommonItemExchangeData& Right);
    void FromPb(const idlepb::RoleCommonItemExchangeData& Right);
    void ToPb(idlepb::RoleCommonItemExchangeData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleCommonItemExchangeData& Right);
    bool operator==(const FPbRoleCommonItemExchangeData& Right) const;
    bool operator!=(const FPbRoleCommonItemExchangeData& Right) const;
     
};

namespace idlepb {
class RoleTreasuryChestData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleTreasuryChestData
{
    GENERATED_BODY();

    /** 每种宝箱的今日开箱次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> today_open_times;

    /** 保底计数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> guarantee_count;


    FPbRoleTreasuryChestData();
    FPbRoleTreasuryChestData(const idlepb::RoleTreasuryChestData& Right);
    void FromPb(const idlepb::RoleTreasuryChestData& Right);
    void ToPb(idlepb::RoleTreasuryChestData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleTreasuryChestData& Right);
    bool operator==(const FPbRoleTreasuryChestData& Right) const;
    bool operator!=(const FPbRoleTreasuryChestData& Right) const;
     
};

namespace idlepb {
class RoleTreasuryGachaData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleTreasuryGachaData
{
    GENERATED_BODY();

    /** 每种卡池的今日开奖次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> today_open_times;

    /** 免费单开次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> free_open_times;

    /** 保底计数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> guarantee_count;

    /** 生涯开启次数，用于福赠计数，仅限Type == 1的宝箱 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_open_time;


    FPbRoleTreasuryGachaData();
    FPbRoleTreasuryGachaData(const idlepb::RoleTreasuryGachaData& Right);
    void FromPb(const idlepb::RoleTreasuryGachaData& Right);
    void ToPb(idlepb::RoleTreasuryGachaData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleTreasuryGachaData& Right);
    bool operator==(const FPbRoleTreasuryGachaData& Right) const;
    bool operator!=(const FPbRoleTreasuryGachaData& Right) const;
     
};

namespace idlepb {
class TreasuryShopItem;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbTreasuryShopItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;

    /** 价格 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 price;

    /** 库存 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 count;

    /** 已买次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 bought_count;

    /** 配置id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;


    FPbTreasuryShopItem();
    FPbTreasuryShopItem(const idlepb::TreasuryShopItem& Right);
    void FromPb(const idlepb::TreasuryShopItem& Right);
    void ToPb(idlepb::TreasuryShopItem* Out) const;
    void Reset();
    void operator=(const idlepb::TreasuryShopItem& Right);
    bool operator==(const FPbTreasuryShopItem& Right) const;
    bool operator!=(const FPbTreasuryShopItem& Right) const;
     
};

namespace idlepb {
class RoleTreasuryShopData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleTreasuryShopData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbTreasuryShopItem> shop_items;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_refresh_time;

    /** 刷新标记，用于功能红点提示 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool shop_refresh_flag;


    FPbRoleTreasuryShopData();
    FPbRoleTreasuryShopData(const idlepb::RoleTreasuryShopData& Right);
    void FromPb(const idlepb::RoleTreasuryShopData& Right);
    void ToPb(idlepb::RoleTreasuryShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleTreasuryShopData& Right);
    bool operator==(const FPbRoleTreasuryShopData& Right) const;
    bool operator!=(const FPbRoleTreasuryShopData& Right) const;
     
};

namespace idlepb {
class RoleTreasurySaveData;
}  // namespace idlepb

/**
 * 宝藏阁功能整体数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleTreasurySaveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTreasuryChestData treasury_chest_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTreasuryGachaData treasury_gacha_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTreasuryShopData treasury_shop_data;

    /** 重置时间戳，设计宝藏阁多个功能 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time;


    FPbRoleTreasurySaveData();
    FPbRoleTreasurySaveData(const idlepb::RoleTreasurySaveData& Right);
    void FromPb(const idlepb::RoleTreasurySaveData& Right);
    void ToPb(idlepb::RoleTreasurySaveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleTreasurySaveData& Right);
    bool operator==(const FPbRoleTreasurySaveData& Right) const;
    bool operator!=(const FPbRoleTreasurySaveData& Right) const;
     
};

namespace idlepb {
class ArenaCheckListData;
}  // namespace idlepb

/**
 * 秘境探索事件相关 Begin
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 checklist_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 checklist_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbArenaCheckListState checklist_state;


    FPbArenaCheckListData();
    FPbArenaCheckListData(const idlepb::ArenaCheckListData& Right);
    void FromPb(const idlepb::ArenaCheckListData& Right);
    void ToPb(idlepb::ArenaCheckListData* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListData& Right);
    bool operator==(const FPbArenaCheckListData& Right) const;
    bool operator!=(const FPbArenaCheckListData& Right) const;
     
};

namespace idlepb {
class ArenaCheckListRewardData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbArenaCheckListRewardData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 reward_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbArenaCheckListRewardState reward_state;


    FPbArenaCheckListRewardData();
    FPbArenaCheckListRewardData(const idlepb::ArenaCheckListRewardData& Right);
    void FromPb(const idlepb::ArenaCheckListRewardData& Right);
    void ToPb(idlepb::ArenaCheckListRewardData* Out) const;
    void Reset();
    void operator=(const idlepb::ArenaCheckListRewardData& Right);
    bool operator==(const FPbArenaCheckListRewardData& Right) const;
    bool operator!=(const FPbArenaCheckListRewardData& Right) const;
     
};

namespace idlepb {
class RoleArenaCheckListData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleArenaCheckListData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbArenaCheckListData> arena_check_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbArenaCheckListRewardData> check_reward_data;


    FPbRoleArenaCheckListData();
    FPbRoleArenaCheckListData(const idlepb::RoleArenaCheckListData& Right);
    void FromPb(const idlepb::RoleArenaCheckListData& Right);
    void ToPb(idlepb::RoleArenaCheckListData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleArenaCheckListData& Right);
    bool operator==(const FPbRoleArenaCheckListData& Right) const;
    bool operator!=(const FPbRoleArenaCheckListData& Right) const;
     
};

namespace idlepb {
class RoleSeptInviteEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleSeptInviteEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSeptPosition position;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbRoleSeptInviteEntry();
    FPbRoleSeptInviteEntry(const idlepb::RoleSeptInviteEntry& Right);
    void FromPb(const idlepb::RoleSeptInviteEntry& Right);
    void ToPb(idlepb::RoleSeptInviteEntry* Out) const;
    void Reset();
    void operator=(const idlepb::RoleSeptInviteEntry& Right);
    bool operator==(const FPbRoleSeptInviteEntry& Right) const;
    bool operator!=(const FPbRoleSeptInviteEntry& Right) const;
     
};

namespace idlepb {
class SeptQuest;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSeptQuest
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 uid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 quest_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool received;

    /** 接取任务时等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /** 任务生成时就确定灵石数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 money_num;


    FPbSeptQuest();
    FPbSeptQuest(const idlepb::SeptQuest& Right);
    void FromPb(const idlepb::SeptQuest& Right);
    void ToPb(idlepb::SeptQuest* Out) const;
    void Reset();
    void operator=(const idlepb::SeptQuest& Right);
    bool operator==(const FPbSeptQuest& Right) const;
    bool operator!=(const FPbSeptQuest& Right) const;
     
};

namespace idlepb {
class RoleSeptQuestData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleSeptQuestData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSeptQuest> quests;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 today_manual_refresh_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 level;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 current_exp;

    /** 用于产生唯一id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 total_num;


    FPbRoleSeptQuestData();
    FPbRoleSeptQuestData(const idlepb::RoleSeptQuestData& Right);
    void FromPb(const idlepb::RoleSeptQuestData& Right);
    void ToPb(idlepb::RoleSeptQuestData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleSeptQuestData& Right);
    bool operator==(const FPbRoleSeptQuestData& Right) const;
    bool operator!=(const FPbRoleSeptQuestData& Right) const;
     
};

namespace idlepb {
class RoleSeptShopData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleSeptShopData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_time_sept_shop;

    /** 宗门商店兑换记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> exchange_history;


    FPbRoleSeptShopData();
    FPbRoleSeptShopData(const idlepb::RoleSeptShopData& Right);
    void FromPb(const idlepb::RoleSeptShopData& Right);
    void ToPb(idlepb::RoleSeptShopData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleSeptShopData& Right);
    bool operator==(const FPbRoleSeptShopData& Right) const;
    bool operator!=(const FPbRoleSeptShopData& Right) const;
     
};

namespace idlepb {
class RoleSeptData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleSeptData
{
    GENERATED_BODY();

    /** 下次能加入宗门的时间点 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_join_ticks;

    /** 本日招募次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleSeptInviteEntry> daily_invite_entries;

    /** 藏经阁 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptShopData sept_shop_data;

    /** 宗门事务 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptQuestData sept_quest_data;

    /** 镇魔深渊累积时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sept_demon_cumulative_time;

    /** 镇魔深渊宝库奖励剩余开启次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sept_demon_stage_reward_num;

    /** 镇魔深渊宝库奖励已开启次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sept_demon_stage_reward_use_num;

    /** 镇魔深渊挑战奖励已领取 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> sept_demon_damage_reward_received;

    /** 镇魔深渊挑战奖励已完成未领取 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> sept_demon_damage_reward_finished;


    FPbRoleSeptData();
    FPbRoleSeptData(const idlepb::RoleSeptData& Right);
    void FromPb(const idlepb::RoleSeptData& Right);
    void ToPb(idlepb::RoleSeptData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleSeptData& Right);
    bool operator==(const FPbRoleSeptData& Right) const;
    bool operator!=(const FPbRoleSeptData& Right) const;
     
};

namespace idlepb {
class SeptDemonWorldData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSeptDemonWorldData
{
    GENERATED_BODY();

    /** 当前是否开启 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_started;

    /** 本次结束时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 cur_end_ticks;

    /** 下次开启时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_open_ticks;

    /** 当前所在重数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cur_stage;

    /** 本重剩余血量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float cur_stage_hp;

    /** 本重最大血量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float cur_stage_maxhp;

    /** 上次活动的重数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_stage;

    /** 此时在镇魔深渊中的玩家id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> player_ids;


    FPbSeptDemonWorldData();
    FPbSeptDemonWorldData(const idlepb::SeptDemonWorldData& Right);
    void FromPb(const idlepb::SeptDemonWorldData& Right);
    void ToPb(idlepb::SeptDemonWorldData* Out) const;
    void Reset();
    void operator=(const idlepb::SeptDemonWorldData& Right);
    bool operator==(const FPbSeptDemonWorldData& Right) const;
    bool operator!=(const FPbSeptDemonWorldData& Right) const;
     
};

namespace idlepb {
class SimpleCounter;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSimpleCounter
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 num;


    FPbSimpleCounter();
    FPbSimpleCounter(const idlepb::SimpleCounter& Right);
    void FromPb(const idlepb::SimpleCounter& Right);
    void ToPb(idlepb::SimpleCounter* Out) const;
    void Reset();
    void operator=(const idlepb::SimpleCounter& Right);
    bool operator==(const FPbSimpleCounter& Right) const;
    bool operator!=(const FPbSimpleCounter& Right) const;
     
};

namespace idlepb {
class FunctionCounter;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFunctionCounter
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 function_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleCounter> counters;


    FPbFunctionCounter();
    FPbFunctionCounter(const idlepb::FunctionCounter& Right);
    void FromPb(const idlepb::FunctionCounter& Right);
    void ToPb(idlepb::FunctionCounter* Out) const;
    void Reset();
    void operator=(const idlepb::FunctionCounter& Right);
    bool operator==(const FPbFunctionCounter& Right) const;
    bool operator!=(const FPbFunctionCounter& Right) const;
     
};

namespace idlepb {
class RoleLifeCounterData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleLifeCounterData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFunctionCounter> function_counter;


    FPbRoleLifeCounterData();
    FPbRoleLifeCounterData(const idlepb::RoleLifeCounterData& Right);
    void FromPb(const idlepb::RoleLifeCounterData& Right);
    void ToPb(idlepb::RoleLifeCounterData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleLifeCounterData& Right);
    bool operator==(const FPbRoleLifeCounterData& Right) const;
    bool operator!=(const FPbRoleLifeCounterData& Right) const;
     
};

namespace idlepb {
class FarmlandManagementInfo;
}  // namespace idlepb

/**
 * 药园药株计划信息，用于一件催熟和药园打理
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandManagementInfo
{
    GENERATED_BODY();

    /** 对应药园打理功能上，这里当作种子的configId使用 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 plant_uid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 target_stage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_seed;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool auto_harvest;


    FPbFarmlandManagementInfo();
    FPbFarmlandManagementInfo(const idlepb::FarmlandManagementInfo& Right);
    void FromPb(const idlepb::FarmlandManagementInfo& Right);
    void ToPb(idlepb::FarmlandManagementInfo* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandManagementInfo& Right);
    bool operator==(const FPbFarmlandManagementInfo& Right) const;
    bool operator!=(const FPbFarmlandManagementInfo& Right) const;
     
};

namespace idlepb {
class FarmlandPlantData;
}  // namespace idlepb

/**
 * 药园药株状态数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmlandPlantData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 plant_uid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 x;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 y;

    /** 种植旋转设置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rotation;

    /** 种子配置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 config_id;

    /** 升灵次数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 shenling;

    /** 种植时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_date;

    /** 催熟加速时长 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 speed_up;


    FPbFarmlandPlantData();
    FPbFarmlandPlantData(const idlepb::FarmlandPlantData& Right);
    void FromPb(const idlepb::FarmlandPlantData& Right);
    void ToPb(idlepb::FarmlandPlantData* Out) const;
    void Reset();
    void operator=(const idlepb::FarmlandPlantData& Right);
    bool operator==(const FPbFarmlandPlantData& Right) const;
    bool operator!=(const FPbFarmlandPlantData& Right) const;
     
};

namespace idlepb {
class RoleFarmlandData;
}  // namespace idlepb

/**
 * 玩家药园数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleFarmlandData
{
    GENERATED_BODY();

    /** 当前药株状态 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandPlantData> current_plants;

    /** 打理设置 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFarmlandManagementInfo> managment_plan;

    /** 已解锁地块 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbVector2> unlock_blocks;

    /** 药童等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 farmer_grade;

    /** 药童好感 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 farmer_friendship_exp;

    /** 角色持有催熟道具 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> ripe_items;


    FPbRoleFarmlandData();
    FPbRoleFarmlandData(const idlepb::RoleFarmlandData& Right);
    void FromPb(const idlepb::RoleFarmlandData& Right);
    void ToPb(idlepb::RoleFarmlandData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleFarmlandData& Right);
    bool operator==(const FPbRoleFarmlandData& Right) const;
    bool operator!=(const FPbRoleFarmlandData& Right) const;
     
};

namespace idlepb {
class RoleAvatarData;
}  // namespace idlepb

/**
 * 玩家化身数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleAvatarData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 current_world_index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_time;

    /** 上一次物资抽取结算时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_draw_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> temp_package;

    /** 上次化身所在秘境Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 last_wrold_index;


    FPbRoleAvatarData();
    FPbRoleAvatarData(const idlepb::RoleAvatarData& Right);
    void FromPb(const idlepb::RoleAvatarData& Right);
    void ToPb(idlepb::RoleAvatarData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleAvatarData& Right);
    bool operator==(const FPbRoleAvatarData& Right) const;
    bool operator!=(const FPbRoleAvatarData& Right) const;
     
};

namespace idlepb {
class BiographyRoleLog;
}  // namespace idlepb

/**
 * 纪念功能-角色日志
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBiographyRoleLog
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 dao_year;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 log_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 poem_seed;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString zone_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString content;


    FPbBiographyRoleLog();
    FPbBiographyRoleLog(const idlepb::BiographyRoleLog& Right);
    void FromPb(const idlepb::BiographyRoleLog& Right);
    void ToPb(idlepb::BiographyRoleLog* Out) const;
    void Reset();
    void operator=(const idlepb::BiographyRoleLog& Right);
    bool operator==(const FPbBiographyRoleLog& Right) const;
    bool operator!=(const FPbBiographyRoleLog& Right) const;
     
};

namespace idlepb {
class RoleBiographyData;
}  // namespace idlepb

/**
 * 玩家传记数据(包括史记，纪念)
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleBiographyData
{
    GENERATED_BODY();

    /** 已经领取的传记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> received_cfg_ids;

    /** 已经领取的史记 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> received_event_cfg_ids;

    /** 纪念记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBiographyRoleLog> role_logs;


    FPbRoleBiographyData();
    FPbRoleBiographyData(const idlepb::RoleBiographyData& Right);
    void FromPb(const idlepb::RoleBiographyData& Right);
    void ToPb(idlepb::RoleBiographyData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleBiographyData& Right);
    bool operator==(const FPbRoleBiographyData& Right) const;
    bool operator!=(const FPbRoleBiographyData& Right) const;
     
};

namespace idlepb {
class SimpleRoleInfo;
}  // namespace idlepb

/**
 * 聊天玩家信息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSimpleRoleInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig model_config;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection role_cultivation_direction;

    /** 所属宗门 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString sept_name;

    /** 宗门职位 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSeptPosition sept_position;

    /** 上次在线时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_online_timespan;

    /** 区服号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 server_id;


    FPbSimpleRoleInfo();
    FPbSimpleRoleInfo(const idlepb::SimpleRoleInfo& Right);
    void FromPb(const idlepb::SimpleRoleInfo& Right);
    void ToPb(idlepb::SimpleRoleInfo* Out) const;
    void Reset();
    void operator=(const idlepb::SimpleRoleInfo& Right);
    bool operator==(const FPbSimpleRoleInfo& Right) const;
    bool operator!=(const FPbSimpleRoleInfo& Right) const;
     
};

namespace idlepb {
class ChatMessage;
}  // namespace idlepb

/**
 * 单个聊天消息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbChatMessage
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString text;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSimpleRoleInfo role_info;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbChatMessageType type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 time;


    FPbChatMessage();
    FPbChatMessage(const idlepb::ChatMessage& Right);
    void FromPb(const idlepb::ChatMessage& Right);
    void ToPb(idlepb::ChatMessage* Out) const;
    void Reset();
    void operator=(const idlepb::ChatMessage& Right);
    bool operator==(const FPbChatMessage& Right) const;
    bool operator!=(const FPbChatMessage& Right) const;
     
};

namespace idlepb {
class PrivateChatRecord;
}  // namespace idlepb

/**
 * 私聊记录，与单个玩家
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPrivateChatRecord
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbChatMessage> chat_record;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 unread_num;


    FPbPrivateChatRecord();
    FPbPrivateChatRecord(const idlepb::PrivateChatRecord& Right);
    void FromPb(const idlepb::PrivateChatRecord& Right);
    void ToPb(idlepb::PrivateChatRecord* Out) const;
    void Reset();
    void operator=(const idlepb::PrivateChatRecord& Right);
    bool operator==(const FPbPrivateChatRecord& Right) const;
    bool operator!=(const FPbPrivateChatRecord& Right) const;
     
};

namespace idlepb {
class RolePrivateChatRecord;
}  // namespace idlepb

/**
 * 私聊记录，一个玩家的与多个玩家的，用于玩家Rpc
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRolePrivateChatRecord
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPrivateChatRecord> data;


    FPbRolePrivateChatRecord();
    FPbRolePrivateChatRecord(const idlepb::RolePrivateChatRecord& Right);
    void FromPb(const idlepb::RolePrivateChatRecord& Right);
    void ToPb(idlepb::RolePrivateChatRecord* Out) const;
    void Reset();
    void operator=(const idlepb::RolePrivateChatRecord& Right);
    bool operator==(const FPbRolePrivateChatRecord& Right) const;
    bool operator!=(const FPbRolePrivateChatRecord& Right) const;
     
};

namespace idlepb {
class ChatData;
}  // namespace idlepb

/**
 * 公频聊天数据，仅用于服务器存储
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbChatData
{
    GENERATED_BODY();

    /** 集群区服聊天记录（万仙） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbChatMessage> colony_servers;

    /** 较小区服聊天记录（异界） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbChatMessage> quad_servers;

    /** 本区聊天记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbChatMessage> local_server;


    FPbChatData();
    FPbChatData(const idlepb::ChatData& Right);
    void FromPb(const idlepb::ChatData& Right);
    void ToPb(idlepb::ChatData* Out) const;
    void Reset();
    void operator=(const idlepb::ChatData& Right);
    bool operator==(const FPbChatData& Right) const;
    bool operator!=(const FPbChatData& Right) const;
     
};


/**
 * 好友关系类型
*/
UENUM(BlueprintType)
enum class EPbFriendRelationshipType : uint8
{
    FRT_None = 0 UMETA(DisplayName="无关系"),
    FRT_Friend = 1 UMETA(DisplayName="一般好友关系"),
    FRT_Partner = 2 UMETA(DisplayName="道侣关系"),
    FRT_Blocked = 3 UMETA(DisplayName="被对方拉黑"),
};
constexpr EPbFriendRelationshipType EPbFriendRelationshipType_Min = EPbFriendRelationshipType::FRT_None;
constexpr EPbFriendRelationshipType EPbFriendRelationshipType_Max = EPbFriendRelationshipType::FRT_Blocked;
constexpr int32 EPbFriendRelationshipType_ArraySize = static_cast<int32>(EPbFriendRelationshipType_Max) + 1;
MPROTOCOL_API bool CheckEPbFriendRelationshipTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFriendRelationshipTypeDescription(EPbFriendRelationshipType Val);

template <typename Char>
struct fmt::formatter<EPbFriendRelationshipType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFriendRelationshipType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class FriendListItem;
}  // namespace idlepb

/**
 * 好友名单元素
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendListItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /** 关系值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 relationship;

    /** 关系类型(道侣、结义) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFriendRelationshipType type;


    FPbFriendListItem();
    FPbFriendListItem(const idlepb::FriendListItem& Right);
    void FromPb(const idlepb::FriendListItem& Right);
    void ToPb(idlepb::FriendListItem* Out) const;
    void Reset();
    void operator=(const idlepb::FriendListItem& Right);
    bool operator==(const FPbFriendListItem& Right) const;
    bool operator!=(const FPbFriendListItem& Right) const;
     
};

namespace idlepb {
class RoleFriendData;
}  // namespace idlepb

/**
 * 道友功能
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleFriendData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFriendListItem> friend_list;

    /** 申请列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> request_list;

    /** 黑名单,离线数据的该字段用于通知被玩家拉黑 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> block_list;

    /** 玩家自己的申请列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> my_request;

    /** 绝交后仍保存关系值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFriendListItem> history_list;


    FPbRoleFriendData();
    FPbRoleFriendData(const idlepb::RoleFriendData& Right);
    void FromPb(const idlepb::RoleFriendData& Right);
    void ToPb(idlepb::RoleFriendData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleFriendData& Right);
    bool operator==(const FPbRoleFriendData& Right) const;
    bool operator!=(const FPbRoleFriendData& Right) const;
     
};

namespace idlepb {
class RoleOfflineFunctionData;
}  // namespace idlepb

/**
 * 角色离线功能点数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleOfflineFunctionData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /** 离线邮件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbMail> mails;

    /** 离线好友功能数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFriendData friend_data;

    /** 私聊聊天记录 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPrivateChatRecord> private_chat_data;

    /** 离线纪念日志 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBiographyRoleLog> role_logs;

    /** 离线排行榜数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLeaderboardData leaderboard_data;

    /** 玩家名字，用于模糊查找 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;

    /** 主修方向总经验值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 total_exp;


    FPbRoleOfflineFunctionData();
    FPbRoleOfflineFunctionData(const idlepb::RoleOfflineFunctionData& Right);
    void FromPb(const idlepb::RoleOfflineFunctionData& Right);
    void ToPb(idlepb::RoleOfflineFunctionData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleOfflineFunctionData& Right);
    bool operator==(const FPbRoleOfflineFunctionData& Right) const;
    bool operator!=(const FPbRoleOfflineFunctionData& Right) const;
     
};

namespace idlepb {
class ServerCounterData;
}  // namespace idlepb

/**
 * 服务器计数器（史记功能等）
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbServerCounterData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFunctionCounter> function_counter;


    FPbServerCounterData();
    FPbServerCounterData(const idlepb::ServerCounterData& Right);
    void FromPb(const idlepb::ServerCounterData& Right);
    void ToPb(idlepb::ServerCounterData* Out) const;
    void Reset();
    void operator=(const idlepb::ServerCounterData& Right);
    bool operator==(const FPbServerCounterData& Right) const;
    bool operator!=(const FPbServerCounterData& Right) const;
     
};

namespace idlepb {
class SocialFunctionCommonSaveData;
}  // namespace idlepb

/**
 * 社交服务器公共数据中继，存储一些玩家离线数据或请求
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSocialFunctionCommonSaveData
{
    GENERATED_BODY();

    /** 角色离线功能点数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleOfflineFunctionData> offline_role_datas;

    /** 服务器计数器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbServerCounterData server_counter_data;

    /** 玩家名册 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbStringInt64Pair> role_list;


    FPbSocialFunctionCommonSaveData();
    FPbSocialFunctionCommonSaveData(const idlepb::SocialFunctionCommonSaveData& Right);
    void FromPb(const idlepb::SocialFunctionCommonSaveData& Right);
    void ToPb(idlepb::SocialFunctionCommonSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::SocialFunctionCommonSaveData& Right);
    bool operator==(const FPbSocialFunctionCommonSaveData& Right) const;
    bool operator!=(const FPbSocialFunctionCommonSaveData& Right) const;
     
};

namespace idlepb {
class RoleSaveData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleSaveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleData role_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsAllModuleData all_stats_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mp;

    /** 任务系统数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleQuestData quest;

    /** 坊市系统数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleShopData shop;

    /** 临时包裹 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTemporaryPackageData temporary_package;

    /** 包裹 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleInventoryData inventory;

    /** 最近离线数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleOfflineData offline_data;

    /** 炼丹系统数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAlchemyData alchemy_data;

    /** 天机阁数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDeluxeShopData deluxe_shop;

    /** 排行功能榜数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLeaderboardData leaderboard_data;

    /** 邮箱数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleMailData mail_data;

    /** 炼器系统数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleForgeData forge_data;

    /** 秘药数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRolePillElixirData pillelixir_data;

    /** 公共修炼数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCommonCultivationData common_cultivation_data;

    /** 技能数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleZasData zas_data;

    /** 镇妖塔数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleMonsterTowerData monster_tower_data;

    /** 山河图数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleShanhetuData shanhetu_data;

    /** BOSS入侵数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleBossInvasionData boss_invasion_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleMasiveData massive_data;

    /** 福缘数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleChecklistData checklist_data;

    /** 通用道具兑换数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleCommonItemExchangeData common_item_exchange_data;

    /** 宗门个人数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleSeptData sept_data;

    /** 宝藏阁功能数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleTreasurySaveData treasury_chest_data;

    /** 功法 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleGongFaData gongfa_data;

    /** 福赠 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFuZengData fuzeng_data;

    /** 古宝 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleCollectionSaveData collection_data;

    /** 角色生涯计数器 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleLifeCounterData life_counter_data;

    /** 角色外观 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAppearanceData appearance_data;

    /** 角色秘境探索事件 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleArenaCheckListData arena_check_list_data;

    /** 剿灭副本的数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDungeonKillAllData dungeon_kill_all_data;

    /** 药园数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFarmlandData farmland_data;

    /** 生存副本的数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleDungeonSurviveData dungeon_survive_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleFriendData friend_data;

    /** 化身数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleAvatarData avatar_data;

    /** 秘境探索统计数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleArenaExplorationStatisticalData arena_statistical_data;

    /** 传记数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleBiographyData biography_data;

    /** 仙阁数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleVipShopData vip_shop_data;


    FPbRoleSaveData();
    FPbRoleSaveData(const idlepb::RoleSaveData& Right);
    void FromPb(const idlepb::RoleSaveData& Right);
    void ToPb(idlepb::RoleSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleSaveData& Right);
    bool operator==(const FPbRoleSaveData& Right) const;
    bool operator!=(const FPbRoleSaveData& Right) const;
     
};

namespace idlepb {
class BattleHistoryRecord;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBattleHistoryRecord
{
    GENERATED_BODY();

    /** 序号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 index;

    /** 发送时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float world_seconds;

    /** 源 EntityId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 source_entity_id;

    /** 目标 EntityId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 target_entity_id;

    /** 消息 TypeId */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 message_type_id;

    /** 消息内容 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<uint8> message_body;


    FPbBattleHistoryRecord();
    FPbBattleHistoryRecord(const idlepb::BattleHistoryRecord& Right);
    void FromPb(const idlepb::BattleHistoryRecord& Right);
    void ToPb(idlepb::BattleHistoryRecord* Out) const;
    void Reset();
    void operator=(const idlepb::BattleHistoryRecord& Right);
    bool operator==(const FPbBattleHistoryRecord& Right) const;
    bool operator!=(const FPbBattleHistoryRecord& Right) const;
     
};

namespace idlepb {
class BattleRoleInfo;
}  // namespace idlepb

/**
 * 战斗角色信息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBattleRoleInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 zone_id;

    /** 分数变动值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 score_delta;

    /** 最终分数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 score;

    /** 名次变化 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank_delta;

    /** 最终名次 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;

    /** 角色外形数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig model_config;

    /** 主修方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection cultivation_main_dir;

    /** 主修方向等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cultivation_main_rank;

    /** NPC配置ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 npc_cfg_id;


    FPbBattleRoleInfo();
    FPbBattleRoleInfo(const idlepb::BattleRoleInfo& Right);
    void FromPb(const idlepb::BattleRoleInfo& Right);
    void ToPb(idlepb::BattleRoleInfo* Out) const;
    void Reset();
    void operator=(const idlepb::BattleRoleInfo& Right);
    bool operator==(const FPbBattleRoleInfo& Right) const;
    bool operator!=(const FPbBattleRoleInfo& Right) const;
     
};

namespace idlepb {
class BattleInfo;
}  // namespace idlepb

/**
 * 战斗信息提要
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBattleInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 world_id;

    /** 攻击方是否取胜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool attacker_win;

    /** 攻击方信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleRoleInfo attacker;

    /** 防守方信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleRoleInfo defender;

    /** 开始时间 (ticks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_ticks;

    /** 结束时间 (ticks) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 end_ticks;

    /** 对战类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSoloType solo_type;


    FPbBattleInfo();
    FPbBattleInfo(const idlepb::BattleInfo& Right);
    void FromPb(const idlepb::BattleInfo& Right);
    void ToPb(idlepb::BattleInfo* Out) const;
    void Reset();
    void operator=(const idlepb::BattleInfo& Right);
    bool operator==(const FPbBattleInfo& Right) const;
    bool operator!=(const FPbBattleInfo& Right) const;
     
};

namespace idlepb {
class BattleHistory;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBattleHistory
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleInfo info;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBattleHistoryRecord> records;


    FPbBattleHistory();
    FPbBattleHistory(const idlepb::BattleHistory& Right);
    void FromPb(const idlepb::BattleHistory& Right);
    void ToPb(idlepb::BattleHistory* Out) const;
    void Reset();
    void operator=(const idlepb::BattleHistory& Right);
    bool operator==(const FPbBattleHistory& Right) const;
    bool operator!=(const FPbBattleHistory& Right) const;
     
};

namespace idlepb {
class RoleBattleInfo;
}  // namespace idlepb

/**
 * 角色的战斗信息提要
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleBattleInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbBattleInfo base;

    /** 本场战斗是否可复仇 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool can_revenge;

    /** 所属赛季 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 round_num;


    FPbRoleBattleInfo();
    FPbRoleBattleInfo(const idlepb::RoleBattleInfo& Right);
    void FromPb(const idlepb::RoleBattleInfo& Right);
    void ToPb(idlepb::RoleBattleInfo* Out) const;
    void Reset();
    void operator=(const idlepb::RoleBattleInfo& Right);
    bool operator==(const FPbRoleBattleInfo& Right) const;
    bool operator!=(const FPbRoleBattleInfo& Right) const;
     
};

namespace idlepb {
class RoleBattleHistorySaveData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbRoleBattleHistorySaveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbRoleBattleInfo> histories;


    FPbRoleBattleHistorySaveData();
    FPbRoleBattleHistorySaveData(const idlepb::RoleBattleHistorySaveData& Right);
    void FromPb(const idlepb::RoleBattleHistorySaveData& Right);
    void ToPb(idlepb::RoleBattleHistorySaveData* Out) const;
    void Reset();
    void operator=(const idlepb::RoleBattleHistorySaveData& Right);
    bool operator==(const FPbRoleBattleHistorySaveData& Right) const;
    bool operator!=(const FPbRoleBattleHistorySaveData& Right) const;
     
};

namespace idlepb {
class CompressedData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCompressedData
{
    GENERATED_BODY();

    /** 原始数据大小 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 original_size;

    /** 压缩后数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<uint8> compressed_data;


    FPbCompressedData();
    FPbCompressedData(const idlepb::CompressedData& Right);
    void FromPb(const idlepb::CompressedData& Right);
    void ToPb(idlepb::CompressedData* Out) const;
    void Reset();
    void operator=(const idlepb::CompressedData& Right);
    bool operator==(const FPbCompressedData& Right) const;
    bool operator!=(const FPbCompressedData& Right) const;
     
};

namespace idlepb {
class DoBreathingExerciseResult;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoBreathingExerciseResult
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool perfect;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float exp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rate;


    FPbDoBreathingExerciseResult();
    FPbDoBreathingExerciseResult(const idlepb::DoBreathingExerciseResult& Right);
    void FromPb(const idlepb::DoBreathingExerciseResult& Right);
    void ToPb(idlepb::DoBreathingExerciseResult* Out) const;
    void Reset();
    void operator=(const idlepb::DoBreathingExerciseResult& Right);
    bool operator==(const FPbDoBreathingExerciseResult& Right) const;
    bool operator!=(const FPbDoBreathingExerciseResult& Right) const;
     
};


/**
*/
UENUM(BlueprintType)
enum class EPbLoginGameRetCode : uint8
{
    LoginGameRetCode_Ok = 0 UMETA(DisplayName="正常登陆"),
    LoginGameRetCode_Unknown = 1 UMETA(DisplayName="未知错误"),
    LoginGameRetCode_NoRole = 2 UMETA(DisplayName="没有角色"),
    LoginGameRetCode_DuplicateLogin = 3 UMETA(DisplayName="已经在线"),
    LoginGameRetCode_AccountInvalid = 4 UMETA(DisplayName="帐号非法"),
    LoginGameRetCode_VersionError = 5 UMETA(DisplayName="版本错误"),
};
constexpr EPbLoginGameRetCode EPbLoginGameRetCode_Min = EPbLoginGameRetCode::LoginGameRetCode_Ok;
constexpr EPbLoginGameRetCode EPbLoginGameRetCode_Max = EPbLoginGameRetCode::LoginGameRetCode_VersionError;
constexpr int32 EPbLoginGameRetCode_ArraySize = static_cast<int32>(EPbLoginGameRetCode_Max) + 1;
MPROTOCOL_API bool CheckEPbLoginGameRetCodeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbLoginGameRetCodeDescription(EPbLoginGameRetCode Val);

template <typename Char>
struct fmt::formatter<EPbLoginGameRetCode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbLoginGameRetCode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbGotoType : uint8
{
    GotoType_None = 0 UMETA(DisplayName="未知类型"),
    GotoType_Relive = 1 UMETA(DisplayName="复活"),
    GotoType_Teleport = 2 UMETA(DisplayName="传送"),
};
constexpr EPbGotoType EPbGotoType_Min = EPbGotoType::GotoType_None;
constexpr EPbGotoType EPbGotoType_Max = EPbGotoType::GotoType_Teleport;
constexpr int32 EPbGotoType_ArraySize = static_cast<int32>(EPbGotoType_Max) + 1;
MPROTOCOL_API bool CheckEPbGotoTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbGotoTypeDescription(EPbGotoType Val);

template <typename Char>
struct fmt::formatter<EPbGotoType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbGotoType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class SystemNoticeParams;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSystemNoticeParams
{
    GENERATED_BODY();

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
    FString s4;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n1;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n2;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n3;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 n4;


    FPbSystemNoticeParams();
    FPbSystemNoticeParams(const idlepb::SystemNoticeParams& Right);
    void FromPb(const idlepb::SystemNoticeParams& Right);
    void ToPb(idlepb::SystemNoticeParams* Out) const;
    void Reset();
    void operator=(const idlepb::SystemNoticeParams& Right);
    bool operator==(const FPbSystemNoticeParams& Right) const;
    bool operator!=(const FPbSystemNoticeParams& Right) const;
     
};

namespace idlepb {
class DropItem;
}  // namespace idlepb

/**
 * 掉落数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDropItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_num;


    FPbDropItem();
    FPbDropItem(const idlepb::DropItem& Right);
    void FromPb(const idlepb::DropItem& Right);
    void ToPb(idlepb::DropItem* Out) const;
    void Reset();
    void operator=(const idlepb::DropItem& Right);
    bool operator==(const FPbDropItem& Right) const;
    bool operator!=(const FPbDropItem& Right) const;
     
};


/**
 * 传送类型
*/
UENUM(BlueprintType)
enum class EPbTravelWorldType : uint8
{
    TravelWorldType_Normal = 0 UMETA(DisplayName="普通"),
    TravelWorldType_Force = 1 UMETA(DisplayName="强制传送(客户端必须切换关卡)"),
    TravelWorldType_ClientNoOpen = 2 UMETA(DisplayName="客户端不要切换关卡"),
    TravelWorldType_ClientCityNoOpen = 3 UMETA(DisplayName="客户端在主城的话就不切换，其它强制切换"),
};
constexpr EPbTravelWorldType EPbTravelWorldType_Min = EPbTravelWorldType::TravelWorldType_Normal;
constexpr EPbTravelWorldType EPbTravelWorldType_Max = EPbTravelWorldType::TravelWorldType_ClientCityNoOpen;
constexpr int32 EPbTravelWorldType_ArraySize = static_cast<int32>(EPbTravelWorldType_Max) + 1;
MPROTOCOL_API bool CheckEPbTravelWorldTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbTravelWorldTypeDescription(EPbTravelWorldType Val);

template <typename Char>
struct fmt::formatter<EPbTravelWorldType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbTravelWorldType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 史记事件类型
*/
UENUM(BlueprintType)
enum class EPbBiographyEventType : uint8
{
    BET_Degree = 0 UMETA(DisplayName="境界排名"),
    BET_FullDegree = 1 UMETA(DisplayName="境界圆满排名"),
    BET_KillMonster = 2 UMETA(DisplayName="击杀妖兽数量"),
    BET_SeptDonation = 3 UMETA(DisplayName="限时宗门建设值"),
    BET_CombatPower = 4 UMETA(DisplayName="限时玩家战力排行"),
    BET_ImmortalRoad = 5 UMETA(DisplayName="飞升之路"),
};
constexpr EPbBiographyEventType EPbBiographyEventType_Min = EPbBiographyEventType::BET_Degree;
constexpr EPbBiographyEventType EPbBiographyEventType_Max = EPbBiographyEventType::BET_ImmortalRoad;
constexpr int32 EPbBiographyEventType_ArraySize = static_cast<int32>(EPbBiographyEventType_Max) + 1;
MPROTOCOL_API bool CheckEPbBiographyEventTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbBiographyEventTypeDescription(EPbBiographyEventType Val);

template <typename Char>
struct fmt::formatter<EPbBiographyEventType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbBiographyEventType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class BiographyEventLeaderboardItem;
}  // namespace idlepb

/**
 * 史记榜单，用于各种事件榜单数据，参数以通用风格命名，用于多种事件情形
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBiographyEventLeaderboardItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 uid;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString name;

    /** 宗门Logo、玩家外观数据（骨骼类型） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 param_d1;

    /** 时间戳、战力值、建设值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 param_n1;

    /** 宗门上榜时保存当时成员列表 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> params_n1;

    /** 玩家外观数据（ModelType） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> params_d1;


    FPbBiographyEventLeaderboardItem();
    FPbBiographyEventLeaderboardItem(const idlepb::BiographyEventLeaderboardItem& Right);
    void FromPb(const idlepb::BiographyEventLeaderboardItem& Right);
    void ToPb(idlepb::BiographyEventLeaderboardItem* Out) const;
    void Reset();
    void operator=(const idlepb::BiographyEventLeaderboardItem& Right);
    bool operator==(const FPbBiographyEventLeaderboardItem& Right) const;
    bool operator!=(const FPbBiographyEventLeaderboardItem& Right) const;
     
};

namespace idlepb {
class BiographyEventLeaderboardList;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbBiographyEventLeaderboardList
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBiographyEventLeaderboardItem> list_data;

    /** 榜单配置id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cfg_id;

    /** 起始日期，限时榜单 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_date;

    /** 是否已经完成 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool finished;


    FPbBiographyEventLeaderboardList();
    FPbBiographyEventLeaderboardList(const idlepb::BiographyEventLeaderboardList& Right);
    void FromPb(const idlepb::BiographyEventLeaderboardList& Right);
    void ToPb(idlepb::BiographyEventLeaderboardList* Out) const;
    void Reset();
    void operator=(const idlepb::BiographyEventLeaderboardList& Right);
    bool operator==(const FPbBiographyEventLeaderboardList& Right) const;
    bool operator!=(const FPbBiographyEventLeaderboardList& Right) const;
     
};

namespace idlepb {
class LeaderboardListItem;
}  // namespace idlepb

/**
 * 排行榜单个玩家数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardListItem
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString user_name;

    /** 属性值：战斗力、财富值等等 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 property_num;

    /** 上榜时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 time;

    /** 自定义参数，宗门logo等 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 d1;


    FPbLeaderboardListItem();
    FPbLeaderboardListItem(const idlepb::LeaderboardListItem& Right);
    void FromPb(const idlepb::LeaderboardListItem& Right);
    void ToPb(idlepb::LeaderboardListItem* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardListItem& Right);
    bool operator==(const FPbLeaderboardListItem& Right) const;
    bool operator!=(const FPbLeaderboardListItem& Right) const;
     
};

namespace idlepb {
class SeptDataOnLeaderboard;
}  // namespace idlepb

/**
 * 排行榜宗门数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSeptDataOnLeaderboard
{
    GENERATED_BODY();

    /** 宗门唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;

    /** 宗门名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString sept_name;

    /** 宗门logo */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 logo_index;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 property_num;


    FPbSeptDataOnLeaderboard();
    FPbSeptDataOnLeaderboard(const idlepb::SeptDataOnLeaderboard& Right);
    void FromPb(const idlepb::SeptDataOnLeaderboard& Right);
    void ToPb(idlepb::SeptDataOnLeaderboard* Out) const;
    void Reset();
    void operator=(const idlepb::SeptDataOnLeaderboard& Right);
    bool operator==(const FPbSeptDataOnLeaderboard& Right) const;
    bool operator!=(const FPbSeptDataOnLeaderboard& Right) const;
     
};

namespace idlepb {
class LeaderboardList;
}  // namespace idlepb

/**
 * 单个排行榜单数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardList
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardListItem> list_data;

    /** 榜单类型，或配置或其它id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type_id;


    FPbLeaderboardList();
    FPbLeaderboardList(const idlepb::LeaderboardList& Right);
    void FromPb(const idlepb::LeaderboardList& Right);
    void ToPb(idlepb::LeaderboardList* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardList& Right);
    bool operator==(const FPbLeaderboardList& Right) const;
    bool operator!=(const FPbLeaderboardList& Right) const;
     
};

namespace idlepb {
class LeaderboardSaveData;
}  // namespace idlepb

/**
 * 排行榜存档
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLeaderboardSaveData
{
    GENERATED_BODY();

    /** 多榜单数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardList> lists_data;

    /** 宗门榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSeptDataOnLeaderboard> sept_list;

    /** 镇妖塔挑战榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbLeaderboardList> monster_tower_challange;

    /** 周刷新计时 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_week_time;

    /** 日刷新计时 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_reset_day_time;

    /** 史记排行榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbBiographyEventLeaderboardList> biography_lists;

    /** 福泽功能计算后的经验均值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 fuze_exp;

    /** 离线人员福泽奖励补发邮件清单 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int64> fuze_mail_list;


    FPbLeaderboardSaveData();
    FPbLeaderboardSaveData(const idlepb::LeaderboardSaveData& Right);
    void FromPb(const idlepb::LeaderboardSaveData& Right);
    void ToPb(idlepb::LeaderboardSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::LeaderboardSaveData& Right);
    bool operator==(const FPbLeaderboardSaveData& Right) const;
    bool operator!=(const FPbLeaderboardSaveData& Right) const;
     
};


/**
 * 角色脏标记
*/
UENUM(BlueprintType)
enum class EPbRoleDirtyFlag : uint8
{
    RoleDirtyFlag_Save = 0 UMETA(DisplayName="存档"),
};
constexpr EPbRoleDirtyFlag EPbRoleDirtyFlag_Min = EPbRoleDirtyFlag::RoleDirtyFlag_Save;
constexpr EPbRoleDirtyFlag EPbRoleDirtyFlag_Max = EPbRoleDirtyFlag::RoleDirtyFlag_Save;
constexpr int32 EPbRoleDirtyFlag_ArraySize = static_cast<int32>(EPbRoleDirtyFlag_Max) + 1;
MPROTOCOL_API bool CheckEPbRoleDirtyFlagValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRoleDirtyFlagDescription(EPbRoleDirtyFlag Val);

template <typename Char>
struct fmt::formatter<EPbRoleDirtyFlag, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRoleDirtyFlag& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class SeptDemonDamageHistoryEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSeptDemonDamageHistoryEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float total_damage;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig role_model;


    FPbSeptDemonDamageHistoryEntry();
    FPbSeptDemonDamageHistoryEntry(const idlepb::SeptDemonDamageHistoryEntry& Right);
    void FromPb(const idlepb::SeptDemonDamageHistoryEntry& Right);
    void ToPb(idlepb::SeptDemonDamageHistoryEntry* Out) const;
    void Reset();
    void operator=(const idlepb::SeptDemonDamageHistoryEntry& Right);
    bool operator==(const FPbSeptDemonDamageHistoryEntry& Right) const;
    bool operator!=(const FPbSeptDemonDamageHistoryEntry& Right) const;
     
};

namespace idlepb {
class SeptDemonDamageHistoryData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSeptDemonDamageHistoryData
{
    GENERATED_BODY();

    /** 伤害排行榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSeptDemonDamageHistoryEntry> all_entries;


    FPbSeptDemonDamageHistoryData();
    FPbSeptDemonDamageHistoryData(const idlepb::SeptDemonDamageHistoryData& Right);
    void FromPb(const idlepb::SeptDemonDamageHistoryData& Right);
    void ToPb(idlepb::SeptDemonDamageHistoryData* Out) const;
    void Reset();
    void operator=(const idlepb::SeptDemonDamageHistoryData& Right);
    bool operator==(const FPbSeptDemonDamageHistoryData& Right) const;
    bool operator!=(const FPbSeptDemonDamageHistoryData& Right) const;
     
};

namespace idlepb {
class SelfSeptInfo;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSelfSeptInfo
{
    GENERATED_BODY();

    /** 宗门唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sept_id;

    /** 宗门名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString sept_name;

    /** 宗门职位 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbSeptPosition sept_position;

    /** 下次可加入宗门的时间戳 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_join_ticks;

    /** 宗门领地是否被攻击 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool land_fighting;


    FPbSelfSeptInfo();
    FPbSelfSeptInfo(const idlepb::SelfSeptInfo& Right);
    void FromPb(const idlepb::SelfSeptInfo& Right);
    void ToPb(idlepb::SelfSeptInfo* Out) const;
    void Reset();
    void operator=(const idlepb::SelfSeptInfo& Right);
    bool operator==(const FPbSelfSeptInfo& Right) const;
    bool operator!=(const FPbSelfSeptInfo& Right) const;
     
};

namespace idlepb {
class CreatePlayerParams;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCreatePlayerParams
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /** 炼体 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRankData physics_rank_data;

    /** 修法 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRankData magic_rank_data;

    /** 外观 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig model_config;

    /** 玩家技能相关数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerAbilityData ability_data;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbFightMode fight_mode;

    /** 是否创建为替身 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_dummy;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbRoleNormalSettings normal_settings;

    /** 宗门信息 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbSelfSeptInfo self_sept_info;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbGameStatsAllModuleData all_stats_data;


    FPbCreatePlayerParams();
    FPbCreatePlayerParams(const idlepb::CreatePlayerParams& Right);
    void FromPb(const idlepb::CreatePlayerParams& Right);
    void ToPb(idlepb::CreatePlayerParams* Out) const;
    void Reset();
    void operator=(const idlepb::CreatePlayerParams& Right);
    bool operator==(const FPbCreatePlayerParams& Right) const;
    bool operator!=(const FPbCreatePlayerParams& Right) const;
     
};

namespace idlepb {
class WorldRuntimeData;
}  // namespace idlepb

/**
 * 地图运行时数据
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbWorldRuntimeData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 world_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float world_seconds;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float time_dilation;


    FPbWorldRuntimeData();
    FPbWorldRuntimeData(const idlepb::WorldRuntimeData& Right);
    void FromPb(const idlepb::WorldRuntimeData& Right);
    void ToPb(idlepb::WorldRuntimeData* Out) const;
    void Reset();
    void operator=(const idlepb::WorldRuntimeData& Right);
    bool operator==(const FPbWorldRuntimeData& Right) const;
    bool operator!=(const FPbWorldRuntimeData& Right) const;
     
};

namespace idlepb {
class NotifyGiftPackageResult;
}  // namespace idlepb

/**
 * 礼包结果通知
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyGiftPackageResult
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSimpleItemData> items;

    /** 礼包道具id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 gift_item_id;

    /** 礼包配置id，如果有 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> config_id;


    FPbNotifyGiftPackageResult();
    FPbNotifyGiftPackageResult(const idlepb::NotifyGiftPackageResult& Right);
    void FromPb(const idlepb::NotifyGiftPackageResult& Right);
    void ToPb(idlepb::NotifyGiftPackageResult* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyGiftPackageResult& Right);
    bool operator==(const FPbNotifyGiftPackageResult& Right) const;
    bool operator!=(const FPbNotifyGiftPackageResult& Right) const;
     
};

namespace idlepb {
class NotifyUsePillProperty;
}  // namespace idlepb

/**
 * 使用属性丹药通知
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNotifyUsePillProperty
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
    int32 property_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float property_num;


    FPbNotifyUsePillProperty();
    FPbNotifyUsePillProperty(const idlepb::NotifyUsePillProperty& Right);
    void FromPb(const idlepb::NotifyUsePillProperty& Right);
    void ToPb(idlepb::NotifyUsePillProperty* Out) const;
    void Reset();
    void operator=(const idlepb::NotifyUsePillProperty& Right);
    bool operator==(const FPbNotifyUsePillProperty& Right) const;
    bool operator!=(const FPbNotifyUsePillProperty& Right) const;
     
};

namespace idlepb {
class EntityCultivationDirData;
}  // namespace idlepb

/**
 * 修炼信息
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEntityCultivationDirData
{
    GENERATED_BODY();

    /** 修炼方向 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCultivationDirection dir;

    /** 等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 级别 - 重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 layer;

    /** 级别 - 期 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 stage;

    /** 级别 - 境 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;


    FPbEntityCultivationDirData();
    FPbEntityCultivationDirData(const idlepb::EntityCultivationDirData& Right);
    void FromPb(const idlepb::EntityCultivationDirData& Right);
    void ToPb(idlepb::EntityCultivationDirData* Out) const;
    void Reset();
    void operator=(const idlepb::EntityCultivationDirData& Right);
    bool operator==(const FPbEntityCultivationDirData& Right) const;
    bool operator!=(const FPbEntityCultivationDirData& Right) const;
     
};

namespace idlepb {
class EntityCultivationData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEntityCultivationData
{
    GENERATED_BODY();

    /** 主修 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbEntityCultivationDirData major;

    /** 辅修 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbEntityCultivationDirData minor;


    FPbEntityCultivationData();
    FPbEntityCultivationData(const idlepb::EntityCultivationData& Right);
    void FromPb(const idlepb::EntityCultivationData& Right);
    void ToPb(idlepb::EntityCultivationData* Out) const;
    void Reset();
    void operator=(const idlepb::EntityCultivationData& Right);
    bool operator==(const FPbEntityCultivationData& Right) const;
    bool operator!=(const FPbEntityCultivationData& Right) const;
     
};

namespace idlepb {
class SwordPkTopListEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSwordPkTopListEntry
{
    GENERATED_BODY();

    /** 角色唯一ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 role_id;

    /** 角色名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString role_name;

    /** 角色外形数据 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCharacterModelConfig role_model;

    /** 分数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 score;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 score_update_ticks;

    /** 名次 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /** 战力 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 combat_power;


    FPbSwordPkTopListEntry();
    FPbSwordPkTopListEntry(const idlepb::SwordPkTopListEntry& Right);
    void FromPb(const idlepb::SwordPkTopListEntry& Right);
    void ToPb(idlepb::SwordPkTopListEntry* Out) const;
    void Reset();
    void operator=(const idlepb::SwordPkTopListEntry& Right);
    bool operator==(const FPbSwordPkTopListEntry& Right) const;
    bool operator!=(const FPbSwordPkTopListEntry& Right) const;
     
};

namespace idlepb {
class SwordPkGlobalSaveData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbSwordPkGlobalSaveData
{
    GENERATED_BODY();

    /** 序号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 round_num;

    /** 开启时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 begin_local_ticks;

    /** 结束时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 end_local_ticks;

    /** 是否结束 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_over;

    /** 下轮每日奖励时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 next_daily_reward_local_ticks;

    /** 排行榜 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbSwordPkTopListEntry> top_list;


    FPbSwordPkGlobalSaveData();
    FPbSwordPkGlobalSaveData(const idlepb::SwordPkGlobalSaveData& Right);
    void FromPb(const idlepb::SwordPkGlobalSaveData& Right);
    void ToPb(idlepb::SwordPkGlobalSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::SwordPkGlobalSaveData& Right);
    bool operator==(const FPbSwordPkGlobalSaveData& Right) const;
    bool operator!=(const FPbSwordPkGlobalSaveData& Right) const;
     
};
