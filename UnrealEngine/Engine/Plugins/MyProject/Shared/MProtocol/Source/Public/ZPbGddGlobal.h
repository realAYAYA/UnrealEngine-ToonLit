#pragma once
#include "ZFmt.h"
#include "ZPbDefines.h"
#include "ZPbCommon.h"
#include "ZPbGddGlobal.generated.h"


namespace idlepb {
class CommonGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCommonGlobalConfig
{
    GENERATED_BODY();

    /** TsRpc 超时时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float ts_rpc_max_seconds;


    FPbCommonGlobalConfig();
    FPbCommonGlobalConfig(const idlepb::CommonGlobalConfig& Right);
    void FromPb(const idlepb::CommonGlobalConfig& Right);
    void ToPb(idlepb::CommonGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::CommonGlobalConfig& Right);
    bool operator==(const FPbCommonGlobalConfig& Right) const;
    bool operator!=(const FPbCommonGlobalConfig& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfigLevelUpEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfigLevelUpEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree_limit;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 stage_limit;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_item_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_money;


    FPbCollectionGlobalConfigLevelUpEntry();
    FPbCollectionGlobalConfigLevelUpEntry(const idlepb::CollectionGlobalConfigLevelUpEntry& Right);
    void FromPb(const idlepb::CollectionGlobalConfigLevelUpEntry& Right);
    void ToPb(idlepb::CollectionGlobalConfigLevelUpEntry* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfigLevelUpEntry& Right);
    bool operator==(const FPbCollectionGlobalConfigLevelUpEntry& Right) const;
    bool operator!=(const FPbCollectionGlobalConfigLevelUpEntry& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfigUpgradeStarCostRequestEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfigUpgradeStarCostRequestEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_item_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_self_piece_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> cost_common_piece_num;


    FPbCollectionGlobalConfigUpgradeStarCostRequestEntry();
    FPbCollectionGlobalConfigUpgradeStarCostRequestEntry(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right);
    void FromPb(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right);
    void ToPb(idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfigUpgradeStarCostRequestEntry& Right);
    bool operator==(const FPbCollectionGlobalConfigUpgradeStarCostRequestEntry& Right) const;
    bool operator!=(const FPbCollectionGlobalConfigUpgradeStarCostRequestEntry& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfigUpgradeStarCostEntry;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfigUpgradeStarCostEntry
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionGlobalConfigUpgradeStarCostRequestEntry> request;


    FPbCollectionGlobalConfigUpgradeStarCostEntry();
    FPbCollectionGlobalConfigUpgradeStarCostEntry(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right);
    void FromPb(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right);
    void ToPb(idlepb::CollectionGlobalConfigUpgradeStarCostEntry* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfigUpgradeStarCostEntry& Right);
    bool operator==(const FPbCollectionGlobalConfigUpgradeStarCostEntry& Right) const;
    bool operator!=(const FPbCollectionGlobalConfigUpgradeStarCostEntry& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfigUpgradeStar;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfigUpgradeStar
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> common_piece_by_quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionGlobalConfigUpgradeStarCostEntry> cost_by_quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionGlobalConfigUpgradeStarCostEntry> cost_by_quality_skill;


    FPbCollectionGlobalConfigUpgradeStar();
    FPbCollectionGlobalConfigUpgradeStar(const idlepb::CollectionGlobalConfigUpgradeStar& Right);
    void FromPb(const idlepb::CollectionGlobalConfigUpgradeStar& Right);
    void ToPb(idlepb::CollectionGlobalConfigUpgradeStar* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfigUpgradeStar& Right);
    bool operator==(const FPbCollectionGlobalConfigUpgradeStar& Right) const;
    bool operator!=(const FPbCollectionGlobalConfigUpgradeStar& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfigReset;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfigReset
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cost_ji_yuan;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 cold_time_seconds;


    FPbCollectionGlobalConfigReset();
    FPbCollectionGlobalConfigReset(const idlepb::CollectionGlobalConfigReset& Right);
    void FromPb(const idlepb::CollectionGlobalConfigReset& Right);
    void ToPb(idlepb::CollectionGlobalConfigReset* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfigReset& Right);
    bool operator==(const FPbCollectionGlobalConfigReset& Right) const;
    bool operator!=(const FPbCollectionGlobalConfigReset& Right) const;
     
};

namespace idlepb {
class CollectionGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCollectionGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> active_by_quality;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCollectionGlobalConfigLevelUpEntry> levelup;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCollectionGlobalConfigUpgradeStar upgrade_star;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbCollectionGlobalConfigReset reset;


    FPbCollectionGlobalConfig();
    FPbCollectionGlobalConfig(const idlepb::CollectionGlobalConfig& Right);
    void FromPb(const idlepb::CollectionGlobalConfig& Right);
    void ToPb(idlepb::CollectionGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::CollectionGlobalConfig& Right);
    bool operator==(const FPbCollectionGlobalConfig& Right) const;
    bool operator!=(const FPbCollectionGlobalConfig& Right) const;
     
};

namespace idlepb {
class CurrencyGlobalConfigItem2Currency;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCurrencyGlobalConfigItem2Currency
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_cfg_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbCurrencyType currency_type;


    FPbCurrencyGlobalConfigItem2Currency();
    FPbCurrencyGlobalConfigItem2Currency(const idlepb::CurrencyGlobalConfigItem2Currency& Right);
    void FromPb(const idlepb::CurrencyGlobalConfigItem2Currency& Right);
    void ToPb(idlepb::CurrencyGlobalConfigItem2Currency* Out) const;
    void Reset();
    void operator=(const idlepb::CurrencyGlobalConfigItem2Currency& Right);
    bool operator==(const FPbCurrencyGlobalConfigItem2Currency& Right) const;
    bool operator!=(const FPbCurrencyGlobalConfigItem2Currency& Right) const;
     
};

namespace idlepb {
class CurrencyGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCurrencyGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbCurrencyGlobalConfigItem2Currency> item2currency;


    FPbCurrencyGlobalConfig();
    FPbCurrencyGlobalConfig(const idlepb::CurrencyGlobalConfig& Right);
    void FromPb(const idlepb::CurrencyGlobalConfig& Right);
    void ToPb(idlepb::CurrencyGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::CurrencyGlobalConfig& Right);
    bool operator==(const FPbCurrencyGlobalConfig& Right) const;
    bool operator!=(const FPbCurrencyGlobalConfig& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigRoleInitAttributes;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigRoleInitAttributes
{
    GENERATED_BODY();

    /** 气血 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hp;

    /** 真元 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mp;

    /** 物攻 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_att;

    /** 物防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_def;

    /** 魔攻 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_att;

    /** 魔防 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_def;

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

    /** 神识 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mind;


    FPbPlayerGlobalConfigRoleInitAttributes();
    FPbPlayerGlobalConfigRoleInitAttributes(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right);
    void FromPb(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right);
    void ToPb(idlepb::PlayerGlobalConfigRoleInitAttributes* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigRoleInitAttributes& Right);
    bool operator==(const FPbPlayerGlobalConfigRoleInitAttributes& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigRoleInitAttributes& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigConstants;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigConstants
{
    GENERATED_BODY();

    /** 初始体积半径 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float init_radius;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_move_stop_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_move_walk_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 move_speed;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float attack_interval_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_heal_trigger_ratio_hp;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_heal_trigger_ratio_mp;

    /** 手动锁定距离S1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_lock_distance_hand;

    /** 手动锁定距离S2 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_lock_distance_auto;

    /** 手动锁定距离S3 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_lock_distance_max;

    /** 近距离取消锁定 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_lock_distance_near;

    /** 脱离屏幕视线取消 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_lock_distance_screen;

    /** 2D 进入缩放物体大小的距离 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float enter_scale_size_distance_2d;

    /** 重生倒计时 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_reborn_time;

    /** 每次点击减少倒计时的时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_reduce_time;

    /** 减少时间次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_reduce_num_max;

    /** 脱战时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_escape_time;

    /** 脱离激战时间（秒）-修士间战斗 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float battle_status_seconds;

    /** 最大探索时长(单位秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_explore_time;

    /** 传送CD (单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_teleport_cooldown;

    /** 切换修炼方向的最小Rank需求 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 switch_cultivation_direction_min_rank;

    /** 角色空闲多长时间，服务器接手角色控制权 (单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float server_take_control_idle_seconds;

    /** 角色坐标补正距离 近 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_location_correction_distance_near;

    /** 角色坐标补正距离 远 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_location_correction_distance_far;

    /** 角色补正速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float player_correction_cost_speed;

    /** mini Map Width */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mini_map_world_width;

    /** mini Map Height */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mini_map_world_height;

    /** 客户端管理Entity的范围，距离主角的长度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float activate_entities_distance;

    /** 选中框自动消失时间 (单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float select_box_auto_disappear_time;

    /** 干预模式空闲多久，变为自动模式 (单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float intervene_to_auto_seconds;

    /** 手动变为自动模式延迟时间 (单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float set_pause_move_function_time;


    FPbPlayerGlobalConfigConstants();
    FPbPlayerGlobalConfigConstants(const idlepb::PlayerGlobalConfigConstants& Right);
    void FromPb(const idlepb::PlayerGlobalConfigConstants& Right);
    void ToPb(idlepb::PlayerGlobalConfigConstants* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigConstants& Right);
    bool operator==(const FPbPlayerGlobalConfigConstants& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigConstants& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigAbility;
}  // namespace idlepb

/**
 * 角色神通相关全局参数配置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigAbility
{
    GENERATED_BODY();

    /** 神通系统开启的主修等级限制 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 open_rank;

    /** 开启第二神通树的修炼等级限制 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 open_secondary_rank;

    /** 神通槽位开放修炼等级限制 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> slots_unlock_rank;


    FPbPlayerGlobalConfigAbility();
    FPbPlayerGlobalConfigAbility(const idlepb::PlayerGlobalConfigAbility& Right);
    void FromPb(const idlepb::PlayerGlobalConfigAbility& Right);
    void ToPb(idlepb::PlayerGlobalConfigAbility* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigAbility& Right);
    bool operator==(const FPbPlayerGlobalConfigAbility& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigAbility& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigBreathingExercise;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigBreathingExercise
{
    GENERATED_BODY();

    /** 吐纳速度（单位：秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float speed;

    /** 取消阈值 (例 0.5 代表 50%) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float cancel_pct;

    /** 高级吐纳开始阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float high_min_pct;

    /** 完美吐纳开始阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float perfect_min_pct;

    /** 完美吐纳结束阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float perfect_max_pct;

    /** 2倍吐纳权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rate2;

    /** 5倍吐纳权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rate5;

    /** 10倍吐纳权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rate10;

    /** 基础灵气 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float basic_ling_qi;


    FPbPlayerGlobalConfigBreathingExercise();
    FPbPlayerGlobalConfigBreathingExercise(const idlepb::PlayerGlobalConfigBreathingExercise& Right);
    void FromPb(const idlepb::PlayerGlobalConfigBreathingExercise& Right);
    void ToPb(idlepb::PlayerGlobalConfigBreathingExercise* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigBreathingExercise& Right);
    bool operator==(const FPbPlayerGlobalConfigBreathingExercise& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigBreathingExercise& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigThunderTestDegreeConfig;
}  // namespace idlepb

/**
 * 雷劫境界配置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigThunderTestDegreeConfig
{
    GENERATED_BODY();

    /** 境界 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;

    /** 基础伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float val;


    FPbPlayerGlobalConfigThunderTestDegreeConfig();
    FPbPlayerGlobalConfigThunderTestDegreeConfig(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right);
    void FromPb(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right);
    void ToPb(idlepb::PlayerGlobalConfigThunderTestDegreeConfig* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigThunderTestDegreeConfig& Right);
    bool operator==(const FPbPlayerGlobalConfigThunderTestDegreeConfig& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigThunderTestDegreeConfig& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigThunderTest;
}  // namespace idlepb

/**
 * 雷劫配置
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigThunderTest
{
    GENERATED_BODY();

    /** 雷劫系数 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<float> damage_coef;

    /** 基础伤害 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPlayerGlobalConfigThunderTestDegreeConfig> damage_base;


    FPbPlayerGlobalConfigThunderTest();
    FPbPlayerGlobalConfigThunderTest(const idlepb::PlayerGlobalConfigThunderTest& Right);
    void FromPb(const idlepb::PlayerGlobalConfigThunderTest& Right);
    void ToPb(idlepb::PlayerGlobalConfigThunderTest* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigThunderTest& Right);
    bool operator==(const FPbPlayerGlobalConfigThunderTest& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigThunderTest& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigAlchemy;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigAlchemy
{
    GENERATED_BODY();

    /** 单次炼丹耗时（单位：秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float each_refining_seconds;

    /** 触发大保底阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 big_chance_value;

    /** 触发大保底阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 small_chance_value;

    /** 每日炼丹次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_daily_count;


    FPbPlayerGlobalConfigAlchemy();
    FPbPlayerGlobalConfigAlchemy(const idlepb::PlayerGlobalConfigAlchemy& Right);
    void FromPb(const idlepb::PlayerGlobalConfigAlchemy& Right);
    void ToPb(idlepb::PlayerGlobalConfigAlchemy* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigAlchemy& Right);
    bool operator==(const FPbPlayerGlobalConfigAlchemy& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigAlchemy& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigForgeDestroyBackItemConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigForgeDestroyBackItemConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_num;


    FPbPlayerGlobalConfigForgeDestroyBackItemConfig();
    FPbPlayerGlobalConfigForgeDestroyBackItemConfig(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right);
    void FromPb(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right);
    void ToPb(idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigForgeDestroyBackItemConfig& Right);
    bool operator==(const FPbPlayerGlobalConfigForgeDestroyBackItemConfig& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigForgeDestroyBackItemConfig& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigForge;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigForge
{
    GENERATED_BODY();

    /** 单次炼丹耗时（单位：秒） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float each_refining_seconds;

    /** 触发大保底阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 big_chance_value;

    /** 触发大保底阈值 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 small_chance_value;

    /** 每日炼器次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_daily_count;

    /** 每日使用辅材次数上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_daily_extra_materials_use_count;

    /** 装备还原的机缘单价 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 revert_cost_gold;

    /** 熔炼的机缘单价 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 destroy_cost_gold;

    /** 熔炉装备返还的资源数量 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbPlayerGlobalConfigForgeDestroyBackItemConfig> destroy_get_item_num;

    /** 找回的机缘单机 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 found_cost_gold;

    /** 可找回装备的最大时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 found_time;


    FPbPlayerGlobalConfigForge();
    FPbPlayerGlobalConfigForge(const idlepb::PlayerGlobalConfigForge& Right);
    void FromPb(const idlepb::PlayerGlobalConfigForge& Right);
    void ToPb(idlepb::PlayerGlobalConfigForge* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigForge& Right);
    bool operator==(const FPbPlayerGlobalConfigForge& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigForge& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigFightMode;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigFightMode
{
    GENERATED_BODY();

    /** 切换模式所需等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 all_mode_require_rank;

    /** `全体模式`切换到`和平模式`所需未攻击时长(单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float all_mode_to_peace_mode_need_seconds;

    /** 战斗模式切换冷却时长(单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mode_change_need_seconds;

    /** 仇恨持续时长(单位：秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float hate_sustain_seconds;

    /** 倍速功能解锁等级 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 world_speed_unlock_rank;


    FPbPlayerGlobalConfigFightMode();
    FPbPlayerGlobalConfigFightMode(const idlepb::PlayerGlobalConfigFightMode& Right);
    void FromPb(const idlepb::PlayerGlobalConfigFightMode& Right);
    void ToPb(idlepb::PlayerGlobalConfigFightMode* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigFightMode& Right);
    bool operator==(const FPbPlayerGlobalConfigFightMode& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigFightMode& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfigInventory;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfigInventory
{
    GENERATED_BODY();

    /** 初始背包空间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 init_space;

    /** 境界等级提升后扩容背包空间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 stage_up_add_space;

    /** 背包已满默认发邮件ID */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 full_mail_id;


    FPbPlayerGlobalConfigInventory();
    FPbPlayerGlobalConfigInventory(const idlepb::PlayerGlobalConfigInventory& Right);
    void FromPb(const idlepb::PlayerGlobalConfigInventory& Right);
    void ToPb(idlepb::PlayerGlobalConfigInventory* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfigInventory& Right);
    bool operator==(const FPbPlayerGlobalConfigInventory& Right) const;
    bool operator!=(const FPbPlayerGlobalConfigInventory& Right) const;
     
};

namespace idlepb {
class PlayerGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigConstants constants;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigRoleInitAttributes new_role_init_attrs;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigAbility ability;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigBreathingExercise breathing_exercise;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigThunderTest thunder_test;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigAlchemy alchemy;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigForge forge;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigFightMode fight_mode;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerGlobalConfigInventory inventory;


    FPbPlayerGlobalConfig();
    FPbPlayerGlobalConfig(const idlepb::PlayerGlobalConfig& Right);
    void FromPb(const idlepb::PlayerGlobalConfig& Right);
    void ToPb(idlepb::PlayerGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerGlobalConfig& Right);
    bool operator==(const FPbPlayerGlobalConfig& Right) const;
    bool operator!=(const FPbPlayerGlobalConfig& Right) const;
     
};

namespace idlepb {
class NpcGlobalConfigConstants;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNpcGlobalConfigConstants
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_move_stop_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float auto_move_walk_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 walk_speed;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float attack_interval_time;

    /** 自动锁定距离S2 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float npc_lock_distance_auto;

    /** 锁定距离上限S3 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float npc_lock_distance_max;

    /** 怪物坐标补正距离 近 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float monster_location_correction_distance_near;

    /** 怪物坐标补正距离 远 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float monster_location_correction_distance_far;

    /** 怪物补正速度 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float monster_correction_cost_speed;

    /** 体修默认技能 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float phy_default_ability_fullid;

    /** 法修默认技能 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float mag_default_ability_fullid;

    /** 默认技能权重 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 default_ability_weight;


    FPbNpcGlobalConfigConstants();
    FPbNpcGlobalConfigConstants(const idlepb::NpcGlobalConfigConstants& Right);
    void FromPb(const idlepb::NpcGlobalConfigConstants& Right);
    void ToPb(idlepb::NpcGlobalConfigConstants* Out) const;
    void Reset();
    void operator=(const idlepb::NpcGlobalConfigConstants& Right);
    bool operator==(const FPbNpcGlobalConfigConstants& Right) const;
    bool operator!=(const FPbNpcGlobalConfigConstants& Right) const;
     
};

namespace idlepb {
class NpcGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbNpcGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbNpcGlobalConfigConstants constants;


    FPbNpcGlobalConfig();
    FPbNpcGlobalConfig(const idlepb::NpcGlobalConfig& Right);
    void FromPb(const idlepb::NpcGlobalConfig& Right);
    void ToPb(idlepb::NpcGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::NpcGlobalConfig& Right);
    bool operator==(const FPbNpcGlobalConfig& Right) const;
    bool operator!=(const FPbNpcGlobalConfig& Right) const;
     
};

namespace idlepb {
class WorldGlobalConfigDungeonCommon;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbWorldGlobalConfigDungeonCommon
{
    GENERATED_BODY();

    /** 战斗开始延迟(秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float begin_delay_seconds;

    /** 战斗结束延迟(秒) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float end_delay_seconds;


    FPbWorldGlobalConfigDungeonCommon();
    FPbWorldGlobalConfigDungeonCommon(const idlepb::WorldGlobalConfigDungeonCommon& Right);
    void FromPb(const idlepb::WorldGlobalConfigDungeonCommon& Right);
    void ToPb(idlepb::WorldGlobalConfigDungeonCommon* Out) const;
    void Reset();
    void operator=(const idlepb::WorldGlobalConfigDungeonCommon& Right);
    bool operator==(const FPbWorldGlobalConfigDungeonCommon& Right) const;
    bool operator!=(const FPbWorldGlobalConfigDungeonCommon& Right) const;
     
};

namespace idlepb {
class WorldGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbWorldGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbWorldGlobalConfigDungeonCommon dungeon_common;


    FPbWorldGlobalConfig();
    FPbWorldGlobalConfig(const idlepb::WorldGlobalConfig& Right);
    void FromPb(const idlepb::WorldGlobalConfig& Right);
    void ToPb(idlepb::WorldGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::WorldGlobalConfig& Right);
    bool operator==(const FPbWorldGlobalConfig& Right) const;
    bool operator!=(const FPbWorldGlobalConfig& Right) const;
     
};

namespace idlepb {
class AppearanceGlobalConfigShopRefreshRule;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceGlobalConfigShopRefreshRule
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> weight;


    FPbAppearanceGlobalConfigShopRefreshRule();
    FPbAppearanceGlobalConfigShopRefreshRule(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right);
    void FromPb(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right);
    void ToPb(idlepb::AppearanceGlobalConfigShopRefreshRule* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceGlobalConfigShopRefreshRule& Right);
    bool operator==(const FPbAppearanceGlobalConfigShopRefreshRule& Right) const;
    bool operator!=(const FPbAppearanceGlobalConfigShopRefreshRule& Right) const;
     
};

namespace idlepb {
class AppearanceGlobalConfigModelTypeInfo;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceGlobalConfigModelTypeInfo
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 model_type;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString name_english;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString name_chinese;


    FPbAppearanceGlobalConfigModelTypeInfo();
    FPbAppearanceGlobalConfigModelTypeInfo(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right);
    void FromPb(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right);
    void ToPb(idlepb::AppearanceGlobalConfigModelTypeInfo* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceGlobalConfigModelTypeInfo& Right);
    bool operator==(const FPbAppearanceGlobalConfigModelTypeInfo& Right) const;
    bool operator!=(const FPbAppearanceGlobalConfigModelTypeInfo& Right) const;
     
};

namespace idlepb {
class AppearanceGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAppearanceGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 refund_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 shop_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 shop_refresh_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sk_type_change_cd;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 sk_type_change_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAppearanceGlobalConfigShopRefreshRule> shop_refresh_rule;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAppearanceGlobalConfigModelTypeInfo> model_type_info;


    FPbAppearanceGlobalConfig();
    FPbAppearanceGlobalConfig(const idlepb::AppearanceGlobalConfig& Right);
    void FromPb(const idlepb::AppearanceGlobalConfig& Right);
    void ToPb(idlepb::AppearanceGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::AppearanceGlobalConfig& Right);
    bool operator==(const FPbAppearanceGlobalConfig& Right) const;
    bool operator!=(const FPbAppearanceGlobalConfig& Right) const;
     
};

namespace idlepb {
class FarmGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFarmGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString farmland_shape;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> default_unlock_farmland_index;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 unlock_farmland_cost_item_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> unlock_farmland_cost_item_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString farmland_seed_shape;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 watering_times_per_day;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<int32> cost_per_watering;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 add_speed_per_watering;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString add_speed_from_item;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float min_seed_plant_time_percent;


    FPbFarmGlobalConfig();
    FPbFarmGlobalConfig(const idlepb::FarmGlobalConfig& Right);
    void FromPb(const idlepb::FarmGlobalConfig& Right);
    void ToPb(idlepb::FarmGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::FarmGlobalConfig& Right);
    bool operator==(const FPbFarmGlobalConfig& Right) const;
    bool operator!=(const FPbFarmGlobalConfig& Right) const;
     
};

namespace idlepb {
class FriendsGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFriendsGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_friend_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 daily_search_count;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_apply_count;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 max_block_list;


    FPbFriendsGlobalConfig();
    FPbFriendsGlobalConfig(const idlepb::FriendsGlobalConfig& Right);
    void FromPb(const idlepb::FriendsGlobalConfig& Right);
    void ToPb(idlepb::FriendsGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::FriendsGlobalConfig& Right);
    bool operator==(const FPbFriendsGlobalConfig& Right) const;
    bool operator!=(const FPbFriendsGlobalConfig& Right) const;
     
};

namespace idlepb {
class AvatarStageCountDiff;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAvatarStageCountDiff
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 diff_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float coef;


    FPbAvatarStageCountDiff();
    FPbAvatarStageCountDiff(const idlepb::AvatarStageCountDiff& Right);
    void FromPb(const idlepb::AvatarStageCountDiff& Right);
    void ToPb(idlepb::AvatarStageCountDiff* Out) const;
    void Reset();
    void operator=(const idlepb::AvatarStageCountDiff& Right);
    bool operator==(const FPbAvatarStageCountDiff& Right) const;
    bool operator!=(const FPbAvatarStageCountDiff& Right) const;
     
};

namespace idlepb {
class AnotherMeGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbAnotherMeGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 temp_package_max;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 idle_time_max;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 idle_time_duration;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 default_draw_time;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbAvatarStageCountDiff> stage_count_diff;


    FPbAnotherMeGlobalConfig();
    FPbAnotherMeGlobalConfig(const idlepb::AnotherMeGlobalConfig& Right);
    void FromPb(const idlepb::AnotherMeGlobalConfig& Right);
    void ToPb(idlepb::AnotherMeGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::AnotherMeGlobalConfig& Right);
    bool operator==(const FPbAnotherMeGlobalConfig& Right) const;
    bool operator!=(const FPbAnotherMeGlobalConfig& Right) const;
     
};

namespace idlepb {
class FuZeRewardNum;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFuZeRewardNum
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 rank;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 num;


    FPbFuZeRewardNum();
    FPbFuZeRewardNum(const idlepb::FuZeRewardNum& Right);
    void FromPb(const idlepb::FuZeRewardNum& Right);
    void ToPb(idlepb::FuZeRewardNum* Out) const;
    void Reset();
    void operator=(const idlepb::FuZeRewardNum& Right);
    bool operator==(const FPbFuZeRewardNum& Right) const;
    bool operator!=(const FPbFuZeRewardNum& Right) const;
     
};

namespace idlepb {
class FuZeRewardItemId;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFuZeRewardItemId
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 degree;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 item_id;


    FPbFuZeRewardItemId();
    FPbFuZeRewardItemId(const idlepb::FuZeRewardItemId& Right);
    void FromPb(const idlepb::FuZeRewardItemId& Right);
    void ToPb(idlepb::FuZeRewardItemId* Out) const;
    void Reset();
    void operator=(const idlepb::FuZeRewardItemId& Right);
    bool operator==(const FPbFuZeRewardItemId& Right) const;
    bool operator!=(const FPbFuZeRewardItemId& Right) const;
     
};

namespace idlepb {
class FuZeGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbFuZeGlobalConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFuZeRewardNum> reward_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<FPbFuZeRewardItemId> reward_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 fuze_rank_num;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float fuze_rank_ratio;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float fenqi_exp_ratio;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 fuze_rank_min;


    FPbFuZeGlobalConfig();
    FPbFuZeGlobalConfig(const idlepb::FuZeGlobalConfig& Right);
    void FromPb(const idlepb::FuZeGlobalConfig& Right);
    void ToPb(idlepb::FuZeGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::FuZeGlobalConfig& Right);
    bool operator==(const FPbFuZeGlobalConfig& Right) const;
    bool operator!=(const FPbFuZeGlobalConfig& Right) const;
     
};
