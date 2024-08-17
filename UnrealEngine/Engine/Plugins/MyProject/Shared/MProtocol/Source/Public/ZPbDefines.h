#pragma once
#include "ZFmt.h"
#include "ZPbDefines.generated.h"



/**
 * 道具品质
*/
UENUM(BlueprintType)
enum class EPbItemQuality : uint8
{
    EQ_None = 0 UMETA(DisplayName="其他"),
    EQ_White = 1 UMETA(DisplayName="白"),
    EQ_Green = 2 UMETA(DisplayName="绿"),
    EQ_Blue = 3 UMETA(DisplayName="蓝"),
    EQ_Purple = 4 UMETA(DisplayName="紫"),
    EQ_Orange = 5 UMETA(DisplayName="橙"),
    EQ_Red = 6 UMETA(DisplayName="红"),
};
constexpr EPbItemQuality EPbItemQuality_Min = EPbItemQuality::EQ_None;
constexpr EPbItemQuality EPbItemQuality_Max = EPbItemQuality::EQ_Red;
constexpr int32 EPbItemQuality_ArraySize = static_cast<int32>(EPbItemQuality_Max) + 1;
MPROTOCOL_API bool CheckEPbItemQualityValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbItemQualityDescription(EPbItemQuality Val);

template <typename Char>
struct fmt::formatter<EPbItemQuality, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbItemQuality& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 道具显示分类
*/
UENUM(BlueprintType)
enum class EPbItemShowType : uint8
{
    ItemShowType_None = 0 UMETA(DisplayName="其它"),
    ItemShowType_Equipment = 1 UMETA(DisplayName="装备"),
    ItemShowType_Pill = 2 UMETA(DisplayName="丹药"),
    ItemShowType_Material = 3 UMETA(DisplayName="材料"),
    ItemShowType_Special = 4 UMETA(DisplayName="特殊"),
};
constexpr EPbItemShowType EPbItemShowType_Min = EPbItemShowType::ItemShowType_None;
constexpr EPbItemShowType EPbItemShowType_Max = EPbItemShowType::ItemShowType_Special;
constexpr int32 EPbItemShowType_ArraySize = static_cast<int32>(EPbItemShowType_Max) + 1;
MPROTOCOL_API bool CheckEPbItemShowTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbItemShowTypeDescription(EPbItemShowType Val);

template <typename Char>
struct fmt::formatter<EPbItemShowType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbItemShowType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 道具类型
*/
UENUM(BlueprintType)
enum class EPbItemType : uint8
{
    ItemType_None = 0 UMETA(DisplayName="其它"),
    ItemType_ExpPill = 1 UMETA(DisplayName="修为丹药"),
    ItemType_Weapon = 2 UMETA(DisplayName="武器"),
    ItemType_CLOTHING = 3 UMETA(DisplayName="防具"),
    ItemType_JEWELRY = 4 UMETA(DisplayName="饰品"),
    ItemType_SkillEquipment = 5 UMETA(DisplayName="法宝"),
    ItemType_RecoverPill = 6 UMETA(DisplayName="回复丹药"),
    ItemType_SkillBook = 7 UMETA(DisplayName="技能书(神通残篇)"),
    ItemType_SecretPill = 8 UMETA(DisplayName="秘药"),
    ItemType_AttrPill = 9 UMETA(DisplayName="属性丹药"),
    ItemType_BreakthroughPill = 10 UMETA(DisplayName="突破丹药"),
    ItemType_PillMaterial = 11 UMETA(DisplayName="炼丹材料"),
    ItemType_WeaponMaterial = 12 UMETA(DisplayName="铸器材料"),
    ItemType_PillRecipe = 13 UMETA(DisplayName="炼丹配方"),
    ItemType_EquipRecipe = 14 UMETA(DisplayName="炼器配方"),
    ItemType_ForgeMaterial = 15 UMETA(DisplayName="炼器辅材"),
    ItemType_GiftPackage = 16 UMETA(DisplayName="礼包"),
    ItemType_SpaceMaterial = 17 UMETA(DisplayName="空间材料"),
    ItemType_Seed = 18 UMETA(DisplayName="种子"),
    ItemType_ShanHeTu = 20 UMETA(DisplayName="山河图道具"),
    ItemType_QiWen = 21 UMETA(DisplayName="器纹材料"),
    ItemType_QiLing = 22 UMETA(DisplayName="装备器灵材料"),
    ItemType_GuBao = 23 UMETA(DisplayName="古宝"),
    ItemType_GuBaoPiece = 24 UMETA(DisplayName="古宝碎片"),
    ItemType_QiLingSkill = 25 UMETA(DisplayName="法宝器灵材料"),
    ItemType_ZhuLingMeterial = 28 UMETA(DisplayName="古宝注灵材料"),
    ItemType_FarmRipe = 36 UMETA(DisplayName="药园催熟"),
    ItemType_Token = 100 UMETA(DisplayName="数值型道具"),
};
constexpr EPbItemType EPbItemType_Min = EPbItemType::ItemType_None;
constexpr EPbItemType EPbItemType_Max = EPbItemType::ItemType_Token;
constexpr int32 EPbItemType_ArraySize = static_cast<int32>(EPbItemType_Max) + 1;
MPROTOCOL_API bool CheckEPbItemTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbItemTypeDescription(EPbItemType Val);

template <typename Char>
struct fmt::formatter<EPbItemType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbItemType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 装备主类型
*/
UENUM(BlueprintType)
enum class EPbEquipmentMainType : uint8
{
    EquipmentMainType_None = 0 UMETA(DisplayName="其它"),
    EquipmentMainType_Weapon = 1 UMETA(DisplayName="武器"),
    EquipmentMainType_CLOTHING = 2 UMETA(DisplayName="防具"),
    EquipmentMainType_JEWELRY = 3 UMETA(DisplayName="饰品"),
    EquipmentMainType_AttSkillEquipment = 4 UMETA(DisplayName="进攻类法宝"),
    EquipmentMainType_DefSkillEquipment = 5 UMETA(DisplayName="防御类法宝"),
};
constexpr EPbEquipmentMainType EPbEquipmentMainType_Min = EPbEquipmentMainType::EquipmentMainType_None;
constexpr EPbEquipmentMainType EPbEquipmentMainType_Max = EPbEquipmentMainType::EquipmentMainType_DefSkillEquipment;
constexpr int32 EPbEquipmentMainType_ArraySize = static_cast<int32>(EPbEquipmentMainType_Max) + 1;
MPROTOCOL_API bool CheckEPbEquipmentMainTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbEquipmentMainTypeDescription(EPbEquipmentMainType Val);

template <typename Char>
struct fmt::formatter<EPbEquipmentMainType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbEquipmentMainType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 装备词条 - 效果增减类型
*/
UENUM(BlueprintType)
enum class EPbPerkValueAddType : uint8
{
    PerkValueAddType_None = 0 UMETA(DisplayName="其他"),
    PerkValueAddType_Add = 1 UMETA(DisplayName="增加"),
    PerkValueAddType_Sub = 2 UMETA(DisplayName="减少"),
};
constexpr EPbPerkValueAddType EPbPerkValueAddType_Min = EPbPerkValueAddType::PerkValueAddType_None;
constexpr EPbPerkValueAddType EPbPerkValueAddType_Max = EPbPerkValueAddType::PerkValueAddType_Sub;
constexpr int32 EPbPerkValueAddType_ArraySize = static_cast<int32>(EPbPerkValueAddType_Max) + 1;
MPROTOCOL_API bool CheckEPbPerkValueAddTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPerkValueAddTypeDescription(EPbPerkValueAddType Val);

template <typename Char>
struct fmt::formatter<EPbPerkValueAddType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPerkValueAddType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 装备词条 - 效果数值类型
*/
UENUM(BlueprintType)
enum class EPbPerkValueEffectType : uint8
{
    PerkValueEffectType_None = 0 UMETA(DisplayName="其他"),
    PerkValueEffectType_EquipmentBasicAttribute = 1 UMETA(DisplayName="装备属性百分比"),
    PerkValueEffectType_Attack = 2 UMETA(DisplayName="攻击"),
    PerkValueEffectType_HpMp = 3 UMETA(DisplayName="气血&真元"),
    PerkValueEffectType_Defence = 4 UMETA(DisplayName="防御"),
    PerkValueEffectType_CritAndCritDef = 5 UMETA(DisplayName="会心和会心抗性"),
    PerkValueEffectType_CritCoeff = 6 UMETA(DisplayName="会心倍率"),
    PerkValueEffectType_StrengthIntellect = 7 UMETA(DisplayName="体魄内息"),
    PerkValueEffectType_RecoverPercent = 8 UMETA(DisplayName="气血真元回复"),
    PerkValueEffectType_Agility = 9 UMETA(DisplayName="身法"),
    PerkValueEffectType_DodgeHit = 10 UMETA(DisplayName="闪避命中"),
    PerkValueEffectType_MoveSpeed = 11 UMETA(DisplayName="移动速度"),
};
constexpr EPbPerkValueEffectType EPbPerkValueEffectType_Min = EPbPerkValueEffectType::PerkValueEffectType_None;
constexpr EPbPerkValueEffectType EPbPerkValueEffectType_Max = EPbPerkValueEffectType::PerkValueEffectType_MoveSpeed;
constexpr int32 EPbPerkValueEffectType_ArraySize = static_cast<int32>(EPbPerkValueEffectType_Max) + 1;
MPROTOCOL_API bool CheckEPbPerkValueEffectTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPerkValueEffectTypeDescription(EPbPerkValueEffectType Val);

template <typename Char>
struct fmt::formatter<EPbPerkValueEffectType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPerkValueEffectType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 词条ID常量
*/
UENUM(BlueprintType)
enum class EPbPerkIdConsts : uint8
{
    PerkIdConsts_None = 0 UMETA(DisplayName="其它"),
    PerkIdConsts_EquipmentBasicAttribute = 1 UMETA(DisplayName="装备属性百分比"),
    PerkIdConsts_MagAttack = 2 UMETA(DisplayName="人物法攻"),
    PerkIdConsts_PhyAttack = 3 UMETA(DisplayName="任务物攻"),
    PerkIdConsts_Hp = 4 UMETA(DisplayName="人物气血"),
    PerkIdConsts_Mp = 5 UMETA(DisplayName="人物真元"),
    PerkIdConsts_PhyDefence = 6 UMETA(DisplayName="人物物防"),
    PerkIdConsts_MagDefence = 7 UMETA(DisplayName="人物法防"),
    PerkIdConsts_Crit = 8 UMETA(DisplayName="人物会心"),
    PerkIdConsts_CritCoeff = 9 UMETA(DisplayName="人物会心倍率"),
    PerkIdConsts_Strength = 11 UMETA(DisplayName="人物体魄"),
    PerkIdConsts_Intellect = 12 UMETA(DisplayName="人物内息"),
    PerkIdConsts_MpRecoverPercent = 13 UMETA(DisplayName="人物真元回复"),
    PerkIdConsts_HpRecoverPercent = 14 UMETA(DisplayName="人物气血回复"),
    PerkIdConsts_Agility = 15 UMETA(DisplayName="人物身法"),
    PerkIdConsts_MagDodge = 16 UMETA(DisplayName="人物法术闪避"),
    PerkIdConsts_PhyDodge = 17 UMETA(DisplayName="人物物理闪避"),
    PerkIdConsts_CritDef = 18 UMETA(DisplayName="人物会心抗性"),
    PerkIdConsts_PhyHit = 19 UMETA(DisplayName="人物物理命中"),
    PerkIdConsts_MagHit = 20 UMETA(DisplayName="人物法术命中"),
    PerkIdConsts_MoveSpeed = 25 UMETA(DisplayName="人物移动速度"),
};
constexpr EPbPerkIdConsts EPbPerkIdConsts_Min = EPbPerkIdConsts::PerkIdConsts_None;
constexpr EPbPerkIdConsts EPbPerkIdConsts_Max = EPbPerkIdConsts::PerkIdConsts_MoveSpeed;
constexpr int32 EPbPerkIdConsts_ArraySize = static_cast<int32>(EPbPerkIdConsts_Max) + 1;
MPROTOCOL_API bool CheckEPbPerkIdConstsValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPerkIdConstsDescription(EPbPerkIdConsts Val);

template <typename Char>
struct fmt::formatter<EPbPerkIdConsts, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPerkIdConsts& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 装备格子状态
*/
UENUM(BlueprintType)
enum class EPbEquipmentSlotState : uint8
{
    EquipmentSlotState_Locked = 0 UMETA(DisplayName="锁定"),
    EquipmentSlotState_ToUnlock = 1 UMETA(DisplayName="待解锁"),
    EquipmentSlotState_UnlockNoEquip = 2 UMETA(DisplayName="已解锁 - 无可用装备"),
    EquipmentSlotState_UnlockEquipInBag = 3 UMETA(DisplayName="已解锁 - 有可用装备"),
    EquipmentSlotState_Slotted = 4 UMETA(DisplayName="已装备"),
};
constexpr EPbEquipmentSlotState EPbEquipmentSlotState_Min = EPbEquipmentSlotState::EquipmentSlotState_Locked;
constexpr EPbEquipmentSlotState EPbEquipmentSlotState_Max = EPbEquipmentSlotState::EquipmentSlotState_Slotted;
constexpr int32 EPbEquipmentSlotState_ArraySize = static_cast<int32>(EPbEquipmentSlotState_Max) + 1;
MPROTOCOL_API bool CheckEPbEquipmentSlotStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbEquipmentSlotStateDescription(EPbEquipmentSlotState Val);

template <typename Char>
struct fmt::formatter<EPbEquipmentSlotState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbEquipmentSlotState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 装备格子可穿戴类型
*/
UENUM(BlueprintType)
enum class EPbEquipmentSlotWearType : uint8
{
    ESWT_Equipment = 0 UMETA(DisplayName="只允许装备道具"),
    ESWT_Collection = 1 UMETA(DisplayName="只允许主动古宝"),
};
constexpr EPbEquipmentSlotWearType EPbEquipmentSlotWearType_Min = EPbEquipmentSlotWearType::ESWT_Equipment;
constexpr EPbEquipmentSlotWearType EPbEquipmentSlotWearType_Max = EPbEquipmentSlotWearType::ESWT_Collection;
constexpr int32 EPbEquipmentSlotWearType_ArraySize = static_cast<int32>(EPbEquipmentSlotWearType_Max) + 1;
MPROTOCOL_API bool CheckEPbEquipmentSlotWearTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbEquipmentSlotWearTypeDescription(EPbEquipmentSlotWearType Val);

template <typename Char>
struct fmt::formatter<EPbEquipmentSlotWearType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbEquipmentSlotWearType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 炼丹-保底类型
*/
UENUM(BlueprintType)
enum class EPbAlchemyChanceType : uint8
{
    AlchemyChanceType_Normal = 0 UMETA(DisplayName="普通-无保底"),
    AlchemyChanceType_Small = 1 UMETA(DisplayName="小保底"),
    AlchemyChanceType_Big = 2 UMETA(DisplayName="大保底"),
};
constexpr EPbAlchemyChanceType EPbAlchemyChanceType_Min = EPbAlchemyChanceType::AlchemyChanceType_Normal;
constexpr EPbAlchemyChanceType EPbAlchemyChanceType_Max = EPbAlchemyChanceType::AlchemyChanceType_Big;
constexpr int32 EPbAlchemyChanceType_ArraySize = static_cast<int32>(EPbAlchemyChanceType_Max) + 1;
MPROTOCOL_API bool CheckEPbAlchemyChanceTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbAlchemyChanceTypeDescription(EPbAlchemyChanceType Val);

template <typename Char>
struct fmt::formatter<EPbAlchemyChanceType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbAlchemyChanceType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 服药类型
*/
UENUM(BlueprintType)
enum class EPbPillType : uint8
{
    PillType_None = 0 UMETA(DisplayName="未知"),
    PillType_Hp = 1 UMETA(DisplayName="气血"),
    PillType_Mp = 2 UMETA(DisplayName="真元"),
};
constexpr EPbPillType EPbPillType_Min = EPbPillType::PillType_None;
constexpr EPbPillType EPbPillType_Max = EPbPillType::PillType_Mp;
constexpr int32 EPbPillType_ArraySize = static_cast<int32>(EPbPillType_Max) + 1;
MPROTOCOL_API bool CheckEPbPillTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPillTypeDescription(EPbPillType Val);

template <typename Char>
struct fmt::formatter<EPbPillType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPillType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 修炼方向
*/
UENUM(BlueprintType)
enum class EPbCultivationDirection : uint8
{
    CD_None = 0 UMETA(DisplayName="通用"),
    CD_Physic = 1 UMETA(DisplayName="炼体"),
    CD_Magic = 2 UMETA(DisplayName="修法"),
};
constexpr EPbCultivationDirection EPbCultivationDirection_Min = EPbCultivationDirection::CD_None;
constexpr EPbCultivationDirection EPbCultivationDirection_Max = EPbCultivationDirection::CD_Magic;
constexpr int32 EPbCultivationDirection_ArraySize = static_cast<int32>(EPbCultivationDirection_Max) + 1;
MPROTOCOL_API bool CheckEPbCultivationDirectionValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCultivationDirectionDescription(EPbCultivationDirection Val);

template <typename Char>
struct fmt::formatter<EPbCultivationDirection, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCultivationDirection& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 突破类型
*/
UENUM(BlueprintType)
enum class EPbBreakthroughType : uint8
{
    BT_None = 0 UMETA(DisplayName="无需突破"),
    BT_Layer = 1 UMETA(DisplayName="瓶颈"),
    BT_Stage = 2 UMETA(DisplayName="破镜"),
    BT_Degree = 3 UMETA(DisplayName="渡劫"),
};
constexpr EPbBreakthroughType EPbBreakthroughType_Min = EPbBreakthroughType::BT_None;
constexpr EPbBreakthroughType EPbBreakthroughType_Max = EPbBreakthroughType::BT_Degree;
constexpr int32 EPbBreakthroughType_ArraySize = static_cast<int32>(EPbBreakthroughType_Max) + 1;
MPROTOCOL_API bool CheckEPbBreakthroughTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbBreakthroughTypeDescription(EPbBreakthroughType Val);

template <typename Char>
struct fmt::formatter<EPbBreakthroughType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbBreakthroughType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 地图类型(LevelType)
*/
UENUM(BlueprintType)
enum class EPbWorldType : uint8
{
    WT_None = 0 UMETA(DisplayName="未知"),
    WT_ClientOnly = 1 UMETA(DisplayName="客户端专用"),
    WT_Arena = 2 UMETA(DisplayName="秘境地图"),
    WT_Door = 3 UMETA(DisplayName="传送门地图"),
    WT_MonsterTower = 4 UMETA(DisplayName="镇妖塔地图"),
    WT_SoloArena = 5 UMETA(DisplayName="切磋地图"),
    WT_SeptArena = 6 UMETA(DisplayName="中立秘境地图"),
    WT_QuestFight = 7 UMETA(DisplayName="任务对战地图"),
    WT_DungeonKillAll = 8 UMETA(DisplayName="剿灭型副本地图"),
    WT_DungeonSurvive = 9 UMETA(DisplayName="生存型副本地图"),
    WT_SeptDemon = 10 UMETA(DisplayName="镇魔深渊地图"),
};
constexpr EPbWorldType EPbWorldType_Min = EPbWorldType::WT_None;
constexpr EPbWorldType EPbWorldType_Max = EPbWorldType::WT_SeptDemon;
constexpr int32 EPbWorldType_ArraySize = static_cast<int32>(EPbWorldType_Max) + 1;
MPROTOCOL_API bool CheckEPbWorldTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbWorldTypeDescription(EPbWorldType Val);

template <typename Char>
struct fmt::formatter<EPbWorldType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbWorldType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 物件类型
*/
UENUM(BlueprintType)
enum class EPbEntityType : uint8
{
    ET_Unknown = 0 UMETA(DisplayName="未知"),
    ET_Player = 1 UMETA(DisplayName="玩家"),
    ET_Npc = 2 UMETA(DisplayName="NPC"),
};
constexpr EPbEntityType EPbEntityType_Min = EPbEntityType::ET_Unknown;
constexpr EPbEntityType EPbEntityType_Max = EPbEntityType::ET_Npc;
constexpr int32 EPbEntityType_ArraySize = static_cast<int32>(EPbEntityType_Max) + 1;
MPROTOCOL_API bool CheckEPbEntityTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbEntityTypeDescription(EPbEntityType Val);

template <typename Char>
struct fmt::formatter<EPbEntityType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbEntityType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 物件状态
*/
UENUM(BlueprintType)
enum class EPbEntityState : uint8
{
    ES_None = 0 UMETA(DisplayName="未知"),
    ES_Init = 1 UMETA(DisplayName="初始"),
    ES_Normal = 2 UMETA(DisplayName="正常"),
    ES_Death = 3 UMETA(DisplayName="死亡"),
    ES_Recycle = 4 UMETA(DisplayName="收回"),
};
constexpr EPbEntityState EPbEntityState_Min = EPbEntityState::ES_None;
constexpr EPbEntityState EPbEntityState_Max = EPbEntityState::ES_Recycle;
constexpr int32 EPbEntityState_ArraySize = static_cast<int32>(EPbEntityState_Max) + 1;
MPROTOCOL_API bool CheckEPbEntityStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbEntityStateDescription(EPbEntityState Val);

template <typename Char>
struct fmt::formatter<EPbEntityState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbEntityState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * NP类型
*/
UENUM(BlueprintType)
enum class EPbNpcType : uint8
{
    NpcType_None = 0 UMETA(DisplayName="未知"),
    NpcType_Function = 1 UMETA(DisplayName="功能NPC"),
    NpcType_Monster = 2 UMETA(DisplayName="怪物"),
    NpcType_SeptStone = 3 UMETA(DisplayName="中立秘境矿点"),
    NpcType_SeptLand = 4 UMETA(DisplayName="中立秘境宗门领地"),
};
constexpr EPbNpcType EPbNpcType_Min = EPbNpcType::NpcType_None;
constexpr EPbNpcType EPbNpcType_Max = EPbNpcType::NpcType_SeptLand;
constexpr int32 EPbNpcType_ArraySize = static_cast<int32>(EPbNpcType_Max) + 1;
MPROTOCOL_API bool CheckEPbNpcTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbNpcTypeDescription(EPbNpcType Val);

template <typename Char>
struct fmt::formatter<EPbNpcType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbNpcType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 怪物类型
*/
UENUM(BlueprintType)
enum class EPbMonsterType : uint8
{
    MonsterType_None = 0 UMETA(DisplayName="未知"),
    MonsterType_Normal = 1 UMETA(DisplayName="普通"),
    MonsterType_Elite = 2 UMETA(DisplayName="精英"),
    MonsterType_Chief = 3 UMETA(DisplayName="首领"),
    MonsterType_SuperBoss = 4 UMETA(DisplayName="神兽"),
    MonsterType_SeptDemon = 5 UMETA(DisplayName="镇魔深渊Npc"),
};
constexpr EPbMonsterType EPbMonsterType_Min = EPbMonsterType::MonsterType_None;
constexpr EPbMonsterType EPbMonsterType_Max = EPbMonsterType::MonsterType_SeptDemon;
constexpr int32 EPbMonsterType_ArraySize = static_cast<int32>(EPbMonsterType_Max) + 1;
MPROTOCOL_API bool CheckEPbMonsterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbMonsterTypeDescription(EPbMonsterType Val);

template <typename Char>
struct fmt::formatter<EPbMonsterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbMonsterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 货币类型
*/
UENUM(BlueprintType)
enum class EPbCurrencyType : uint8
{
    CurrencyType_None = 0 UMETA(DisplayName="未知类型"),
    CurrencyType_Money = 1 UMETA(DisplayName="灵石"),
    CurrencyType_Soul = 2 UMETA(DisplayName="天命"),
    CurrencyType_Gold = 3 UMETA(DisplayName="机缘"),
    CurrencyType_Item = 4 UMETA(DisplayName="天机令"),
    CurrencyType_AbilityActivePoint = 5 UMETA(DisplayName="神通要诀"),
    CurrencyType_AbilityUpgradePoint = 6 UMETA(DisplayName="神通心得"),
    CurrencyType_KungfuPoint = 7 UMETA(DisplayName="功法点"),
    CurrencyType_TreasureToken = 8 UMETA(DisplayName="天机石"),
    CurrencyType_ChaosStone = 9 UMETA(DisplayName="混沌聚灵石"),
    CurrencyType_StudyPoint = 10 UMETA(DisplayName="研习心得"),
    CurrencyType_NingWenGem = 11 UMETA(DisplayName="凝纹宝玉"),
    CurrencyType_HeroCard = 12 UMETA(DisplayName="英雄帖"),
    CurrencyType_SeptDonation = 13 UMETA(DisplayName="宗门贡献"),
    CurrencyType_SeptStone = 14 UMETA(DisplayName="宗门玄晶石"),
    CurrencyType_SeptQuestExp = 15 UMETA(DisplayName="事务经验"),
    CurrencyType_SeptQuestToken = 16 UMETA(DisplayName="事务令"),
    CurrencyType_GongFaToken = 17 UMETA(DisplayName="功法要诀"),
    CurrencyType_GachaTokenL1 = 18 UMETA(DisplayName="探宝令"),
    CurrencyType_GachaTokenL2 = 19 UMETA(DisplayName="灵域探宝令"),
    CurrencyType_GachaTokenL3 = 20 UMETA(DisplayName="仙魔令"),
    CurrencyType_GachaTokenL4 = 21 UMETA(DisplayName="领域探宝令"),
    CurrencyType_GachaTokenL5 = 22 UMETA(DisplayName="探宝灯"),
    CurrencyType_TreasuryChest01 = 23 UMETA(DisplayName="流云玉简道具ID"),
    CurrencyType_TreasuryChest02 = 24 UMETA(DisplayName="玄光玉简道具ID"),
    CurrencyType_TreasuryChest03 = 25 UMETA(DisplayName="上古玉简道具ID"),
    CurrencyType_TreasuryChest04 = 26 UMETA(DisplayName="灵兽玉简道具ID"),
    CurrencyType_TreasuryChest05 = 27 UMETA(DisplayName="本命玉简道具ID"),
    CurrencyType_TreasuryChest06 = 28 UMETA(DisplayName="仙魔玉简道具ID"),
    CurrencyType_TreasuryChest07 = 29 UMETA(DisplayName="乾坤玉简道具ID"),
    CurrencyType_TreasuryChest08 = 30 UMETA(DisplayName="造化玉简道具ID"),
    CurrencyType_AppearanceMoney = 31 UMETA(DisplayName="外观商店货币道具ID"),
};
constexpr EPbCurrencyType EPbCurrencyType_Min = EPbCurrencyType::CurrencyType_None;
constexpr EPbCurrencyType EPbCurrencyType_Max = EPbCurrencyType::CurrencyType_AppearanceMoney;
constexpr int32 EPbCurrencyType_ArraySize = static_cast<int32>(EPbCurrencyType_Max) + 1;
MPROTOCOL_API bool CheckEPbCurrencyTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCurrencyTypeDescription(EPbCurrencyType Val);

template <typename Char>
struct fmt::formatter<EPbCurrencyType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCurrencyType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 1v1对战类型
*/
UENUM(BlueprintType)
enum class EPbSoloType : uint8
{
    SoloType_None = 0 UMETA(DisplayName="未知类型"),
    SoloType_FriendlyPk = 1 UMETA(DisplayName="切磋"),
    SoloType_SwordPk = 2 UMETA(DisplayName="论剑台挑战"),
    SoloType_SwordPkRevenge = 3 UMETA(DisplayName="论剑台复仇"),
    SoloType_RobberySeptStone = 4 UMETA(DisplayName="抢夺中立秘镜矿脉"),
};
constexpr EPbSoloType EPbSoloType_Min = EPbSoloType::SoloType_None;
constexpr EPbSoloType EPbSoloType_Max = EPbSoloType::SoloType_RobberySeptStone;
constexpr int32 EPbSoloType_ArraySize = static_cast<int32>(EPbSoloType_Max) + 1;
MPROTOCOL_API bool CheckEPbSoloTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSoloTypeDescription(EPbSoloType Val);

template <typename Char>
struct fmt::formatter<EPbSoloType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSoloType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 任务要求类型
*/
UENUM(BlueprintType)
enum class EPbQuestRequirementType : uint8
{
    QT_Kill = 0 UMETA(DisplayName="击杀"),
    QT_Get = 1 UMETA(DisplayName="获得道具"),
    QT_Submit = 2 UMETA(DisplayName="提交道具"),
    QT_Rank = 3 UMETA(DisplayName="等级提升"),
    QT_Event = 4 UMETA(DisplayName="特殊事件"),
    QT_Alchemy = 5 UMETA(DisplayName="炼丹"),
    QT_AlchemyRank = 6 UMETA(DisplayName="丹师等级"),
    QT_SkillRank = 7 UMETA(DisplayName="神通等级"),
    QT_Forge = 8 UMETA(DisplayName="炼器"),
    QT_ForgeRank = 9 UMETA(DisplayName="器室等级"),
    QT_ArenaDoor = 10 UMETA(DisplayName="秘境传送阵"),
    QT_MonsterTower = 11 UMETA(DisplayName="镇妖塔"),
    QT_QiCollector = 12 UMETA(DisplayName="聚灵阵"),
    QT_GongFa = 13 UMETA(DisplayName="功法"),
    QT_QuestFight = 14 UMETA(DisplayName="任务对战"),
    QT_SkillDegree = 15 UMETA(DisplayName="神通品阶"),
    QT_GongFaDegree = 16 UMETA(DisplayName="功法品阶"),
    QT_CollectorQuality = 17 UMETA(DisplayName="古宝品质"),
    QT_JoinSept = 18 UMETA(DisplayName="加入宗门"),
    QT_FarmlandSeed = 19 UMETA(DisplayName="药园播种"),
};
constexpr EPbQuestRequirementType EPbQuestRequirementType_Min = EPbQuestRequirementType::QT_Kill;
constexpr EPbQuestRequirementType EPbQuestRequirementType_Max = EPbQuestRequirementType::QT_FarmlandSeed;
constexpr int32 EPbQuestRequirementType_ArraySize = static_cast<int32>(EPbQuestRequirementType_Max) + 1;
MPROTOCOL_API bool CheckEPbQuestRequirementTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbQuestRequirementTypeDescription(EPbQuestRequirementType Val);

template <typename Char>
struct fmt::formatter<EPbQuestRequirementType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbQuestRequirementType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 任务操作类型
*/
UENUM(BlueprintType)
enum class EPbQuestOpType : uint8
{
    QOp_Accept = 0 UMETA(DisplayName="接受"),
    QOp_Finish = 1 UMETA(DisplayName="提交"),
    QOp_GiveUp = 2 UMETA(DisplayName="放弃"),
};
constexpr EPbQuestOpType EPbQuestOpType_Min = EPbQuestOpType::QOp_Accept;
constexpr EPbQuestOpType EPbQuestOpType_Max = EPbQuestOpType::QOp_GiveUp;
constexpr int32 EPbQuestOpType_ArraySize = static_cast<int32>(EPbQuestOpType_Max) + 1;
MPROTOCOL_API bool CheckEPbQuestOpTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbQuestOpTypeDescription(EPbQuestOpType Val);

template <typename Char>
struct fmt::formatter<EPbQuestOpType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbQuestOpType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 任务特殊奖励
*/
UENUM(BlueprintType)
enum class EPbQuestSpecialRewardType : uint8
{
    QSRT_FarmerFriendShip = 0 UMETA(DisplayName="药童好感"),
};
constexpr EPbQuestSpecialRewardType EPbQuestSpecialRewardType_Min = EPbQuestSpecialRewardType::QSRT_FarmerFriendShip;
constexpr EPbQuestSpecialRewardType EPbQuestSpecialRewardType_Max = EPbQuestSpecialRewardType::QSRT_FarmerFriendShip;
constexpr int32 EPbQuestSpecialRewardType_ArraySize = static_cast<int32>(EPbQuestSpecialRewardType_Max) + 1;
MPROTOCOL_API bool CheckEPbQuestSpecialRewardTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbQuestSpecialRewardTypeDescription(EPbQuestSpecialRewardType Val);

template <typename Char>
struct fmt::formatter<EPbQuestSpecialRewardType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbQuestSpecialRewardType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 排行榜
*/
UENUM(BlueprintType)
enum class EPbLeaderboardType : uint8
{
    LBT_Combat = 0 UMETA(DisplayName="战力榜"),
    LBT_Magic = 1 UMETA(DisplayName="修法榜"),
    LBT_Phy = 2 UMETA(DisplayName="炼体榜"),
    LBT_Rich = 3 UMETA(DisplayName="财富榜"),
    LBT_Pet = 4 UMETA(DisplayName="灵兽榜"),
    LBT_Sect = 5 UMETA(DisplayName="宗门榜"),
    LBT_Weapon = 6 UMETA(DisplayName="武器榜"),
    LBT_Armor = 7 UMETA(DisplayName="防具榜"),
    LBT_Jewelry = 8 UMETA(DisplayName="饰品榜"),
    LBT_Treasure = 9 UMETA(DisplayName="法宝榜"),
    LBT_Shanhetu = 10 UMETA(DisplayName="山河图开包榜"),
    LBT_Shanhetu_Week = 11 UMETA(DisplayName="山河图开周榜"),
    LBT_MonsterTower = 12 UMETA(DisplayName="镇妖塔"),
    LBT_MainExp = 13 UMETA(DisplayName="修为榜"),
    LBT_MaxNum = 14 UMETA(DisplayName="最大种类"),
};
constexpr EPbLeaderboardType EPbLeaderboardType_Min = EPbLeaderboardType::LBT_Combat;
constexpr EPbLeaderboardType EPbLeaderboardType_Max = EPbLeaderboardType::LBT_MaxNum;
constexpr int32 EPbLeaderboardType_ArraySize = static_cast<int32>(EPbLeaderboardType_Max) + 1;
MPROTOCOL_API bool CheckEPbLeaderboardTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbLeaderboardTypeDescription(EPbLeaderboardType Val);

template <typename Char>
struct fmt::formatter<EPbLeaderboardType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbLeaderboardType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 炼丹状态
*/
UENUM(BlueprintType)
enum class EPbAlchemyState : uint8
{
    AlchemyState_None = 0 UMETA(DisplayName="未开始"),
    AlchemyState_Running = 1 UMETA(DisplayName="进行中"),
    AlchemyState_Done = 2 UMETA(DisplayName="已经完成（待领取）"),
};
constexpr EPbAlchemyState EPbAlchemyState_Min = EPbAlchemyState::AlchemyState_None;
constexpr EPbAlchemyState EPbAlchemyState_Max = EPbAlchemyState::AlchemyState_Done;
constexpr int32 EPbAlchemyState_ArraySize = static_cast<int32>(EPbAlchemyState_Max) + 1;
MPROTOCOL_API bool CheckEPbAlchemyStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbAlchemyStateDescription(EPbAlchemyState Val);

template <typename Char>
struct fmt::formatter<EPbAlchemyState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbAlchemyState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 地图状态
*/
UENUM(BlueprintType)
enum class EPbWorldState : uint8
{
    WS_None = 0 UMETA(DisplayName="未知"),
    WS_Init = 1 UMETA(DisplayName="初始"),
    WS_Normal = 2 UMETA(DisplayName="正常"),
    WS_Closing = 3 UMETA(DisplayName="关闭中"),
    WS_Recycle = 4 UMETA(DisplayName="收回"),
};
constexpr EPbWorldState EPbWorldState_Min = EPbWorldState::WS_None;
constexpr EPbWorldState EPbWorldState_Max = EPbWorldState::WS_Recycle;
constexpr int32 EPbWorldState_ArraySize = static_cast<int32>(EPbWorldState_Max) + 1;
MPROTOCOL_API bool CheckEPbWorldStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbWorldStateDescription(EPbWorldState Val);

template <typename Char>
struct fmt::formatter<EPbWorldState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbWorldState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 属性模块类型 (编号 < GSMT_Debug 的为汇总模块，编号 >= GSMT_Debug 的为基础模块)
*/
UENUM(BlueprintType)
enum class EPbGameStatsModuleType : uint8
{
    GSMT_Summary = 0 UMETA(DisplayName="全汇总"),
    GSMT_Base = 1 UMETA(DisplayName="基础属性汇总"),
    GSMT_CollectionBlue = 2 UMETA(DisplayName="古宝汇总 - 普通"),
    GSMT_CollectionPurple = 3 UMETA(DisplayName="古宝汇总 - 珍宝"),
    GSMT_CollectionOrange = 4 UMETA(DisplayName="古宝汇总 - 灵宝"),
    GSMT_CollectionRed = 5 UMETA(DisplayName="古宝汇总 - 至宝"),
    GSMT_CollectionSuitBlue = 6 UMETA(DisplayName="古宝套装汇总 - 普通"),
    GSMT_CollectionSuitPurple = 7 UMETA(DisplayName="古宝套装汇总 - 珍宝"),
    GSMT_CollectionSuitOrange = 8 UMETA(DisplayName="古宝套装汇总 - 灵宝"),
    GSMT_CollectionSuitRed = 9 UMETA(DisplayName="古宝套装汇总 - 至宝"),
    GSMT_EquipmentBase = 31 UMETA(DisplayName="装备道具 - 基础属性"),
    GSMT_EquipmentReinforce = 32 UMETA(DisplayName="装备道具 - 强化属性"),
    GSMT_EquipmentQiWen = 33 UMETA(DisplayName="装备道具 - 器纹属性"),
    GSMT_EquipmentRefine = 34 UMETA(DisplayName="装备道具 - 精炼属性"),
    GSMT_Debug = 50 UMETA(DisplayName="调试"),
    GSMT_RankBreakthrough = 51 UMETA(DisplayName="境界突破"),
    GSMT_RankPractice = 52 UMETA(DisplayName="境界修炼"),
    GSMT_Collection = 53 UMETA(DisplayName="古宝"),
    GSMT_CollectionSuit = 54 UMETA(DisplayName="古宝套装"),
    GSMT_PillElixir = 55 UMETA(DisplayName="秘药"),
    GSMT_PillProperty = 56 UMETA(DisplayName="丹药"),
    GSMT_GongFa = 57 UMETA(DisplayName="功法"),
    GSMT_QiCollector = 58 UMETA(DisplayName="聚灵阵"),
    GSMT_Sept = 59 UMETA(DisplayName="宗门"),
    GSMT_Equipment = 60 UMETA(DisplayName="装备"),
    GSMT_Ability = 61 UMETA(DisplayName="技能"),
    GSMT_EquipmentReinforceResonance = 62 UMETA(DisplayName="装备道具 - 祭炼共鸣属性"),
    GSMT_EquipmentQiWenResonance = 63 UMETA(DisplayName="装备道具 - 器纹共鸣属性"),
    GSMT_Leaderboard = 91 UMETA(DisplayName="来自排行榜的属性增益（福泽）"),
};
constexpr EPbGameStatsModuleType EPbGameStatsModuleType_Min = EPbGameStatsModuleType::GSMT_Summary;
constexpr EPbGameStatsModuleType EPbGameStatsModuleType_Max = EPbGameStatsModuleType::GSMT_Leaderboard;
constexpr int32 EPbGameStatsModuleType_ArraySize = static_cast<int32>(EPbGameStatsModuleType_Max) + 1;
MPROTOCOL_API bool CheckEPbGameStatsModuleTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbGameStatsModuleTypeDescription(EPbGameStatsModuleType Val);

template <typename Char>
struct fmt::formatter<EPbGameStatsModuleType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbGameStatsModuleType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 道具属性模块类型
*/
UENUM(BlueprintType)
enum class EPbItemStatsModuleType : uint8
{
    ISMT_Equipment = 0 UMETA(DisplayName="装备属性"),
    ISMT_Reinforce = 2 UMETA(DisplayName="强化"),
    ISMT_QiWen = 3 UMETA(DisplayName="器纹"),
    ISMT_Refine = 4 UMETA(DisplayName="精炼"),
};
constexpr EPbItemStatsModuleType EPbItemStatsModuleType_Min = EPbItemStatsModuleType::ISMT_Equipment;
constexpr EPbItemStatsModuleType EPbItemStatsModuleType_Max = EPbItemStatsModuleType::ISMT_Refine;
constexpr int32 EPbItemStatsModuleType_ArraySize = static_cast<int32>(EPbItemStatsModuleType_Max) + 1;
MPROTOCOL_API bool CheckEPbItemStatsModuleTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbItemStatsModuleTypeDescription(EPbItemStatsModuleType Val);

template <typename Char>
struct fmt::formatter<EPbItemStatsModuleType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbItemStatsModuleType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 功能模块状态
*/
UENUM(BlueprintType)
enum class EPbFunctionModuleState : uint8
{
    FMS_Hide = 0 UMETA(DisplayName="隐藏，未达到显示等级"),
    FMS_Show = 1 UMETA(DisplayName="显示，未达到解锁等级"),
    FMS_CanUnlock = 2 UMETA(DisplayName="显示，已达解锁等级还未解锁"),
    FMS_Unlocked = 3 UMETA(DisplayName="已解锁"),
};
constexpr EPbFunctionModuleState EPbFunctionModuleState_Min = EPbFunctionModuleState::FMS_Hide;
constexpr EPbFunctionModuleState EPbFunctionModuleState_Max = EPbFunctionModuleState::FMS_Unlocked;
constexpr int32 EPbFunctionModuleState_ArraySize = static_cast<int32>(EPbFunctionModuleState_Max) + 1;
MPROTOCOL_API bool CheckEPbFunctionModuleStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFunctionModuleStateDescription(EPbFunctionModuleState Val);

template <typename Char>
struct fmt::formatter<EPbFunctionModuleState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFunctionModuleState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 功能模块类型
*/
UENUM(BlueprintType)
enum class EPbFunctionModuleType : uint8
{
    FMT_None = 0 UMETA(DisplayName="未知"),
    FMT_Alchemy = 1 UMETA(DisplayName="丹房 (炼丹)"),
    FMT_Shop = 2 UMETA(DisplayName="坊市"),
    FMT_DeluxShop = 3 UMETA(DisplayName="天机阁"),
    FMT_Leaderboard = 4 UMETA(DisplayName="排行榜"),
    FMT_Mail = 5 UMETA(DisplayName="邮件"),
    FMT_Forge = 6 UMETA(DisplayName="器室（炼器）"),
    FMT_MonsterTower = 7 UMETA(DisplayName="镇妖塔"),
    FMT_PillElixir = 8 UMETA(DisplayName="秘药"),
    FMT_Ability = 9 UMETA(DisplayName="神通"),
    FMT_GuBao = 10 UMETA(DisplayName="古宝"),
    FMT_GongFa = 11 UMETA(DisplayName="功法"),
    FMT_Share = 12 UMETA(DisplayName="分享"),
    FMT_HuangZhuang = 13 UMETA(DisplayName="换装"),
    FMT_ZhuangPei = 14 UMETA(DisplayName="装配"),
    FMT_QiCollector = 15 UMETA(DisplayName="聚灵阵"),
    FMT_Checklist = 16 UMETA(DisplayName="福缘"),
    FMT_SwordPk = 17 UMETA(DisplayName="论剑台"),
    FMT_TreasuryChest = 18 UMETA(DisplayName="宝藏阁"),
    FMT_Appearance = 19 UMETA(DisplayName="外观"),
    FMT_Farm = 20 UMETA(DisplayName="药园"),
    FMT_Avatar = 21 UMETA(DisplayName="化身"),
    FMT_Biography = 22 UMETA(DisplayName="传记"),
    FMT_BiographyEvent = 23 UMETA(DisplayName="史记"),
    FMT_VipShop = 24 UMETA(DisplayName="仙阁商店"),
};
constexpr EPbFunctionModuleType EPbFunctionModuleType_Min = EPbFunctionModuleType::FMT_None;
constexpr EPbFunctionModuleType EPbFunctionModuleType_Max = EPbFunctionModuleType::FMT_VipShop;
constexpr int32 EPbFunctionModuleType_ArraySize = static_cast<int32>(EPbFunctionModuleType_Max) + 1;
MPROTOCOL_API bool CheckEPbFunctionModuleTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFunctionModuleTypeDescription(EPbFunctionModuleType Val);

template <typename Char>
struct fmt::formatter<EPbFunctionModuleType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFunctionModuleType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 功能模块解锁方式
*/
UENUM(BlueprintType)
enum class EPbFunctionModuleUnlockType : uint8
{
    FMUT_Click = 0 UMETA(DisplayName="需要玩家点击解锁"),
    FMUT_Auto = 1 UMETA(DisplayName="到级别自动解锁"),
};
constexpr EPbFunctionModuleUnlockType EPbFunctionModuleUnlockType_Min = EPbFunctionModuleUnlockType::FMUT_Click;
constexpr EPbFunctionModuleUnlockType EPbFunctionModuleUnlockType_Max = EPbFunctionModuleUnlockType::FMUT_Auto;
constexpr int32 EPbFunctionModuleUnlockType_ArraySize = static_cast<int32>(EPbFunctionModuleUnlockType_Max) + 1;
MPROTOCOL_API bool CheckEPbFunctionModuleUnlockTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFunctionModuleUnlockTypeDescription(EPbFunctionModuleUnlockType Val);

template <typename Char>
struct fmt::formatter<EPbFunctionModuleUnlockType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFunctionModuleUnlockType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 邮件类型
*/
UENUM(BlueprintType)
enum class EPbMailType : uint8
{
    MT_System = 0 UMETA(DisplayName="系统公告"),
    MT_Maintenance = 1 UMETA(DisplayName="维护公告"),
    MT_Official = 2 UMETA(DisplayName="天道（官方）公告"),
};
constexpr EPbMailType EPbMailType_Min = EPbMailType::MT_System;
constexpr EPbMailType EPbMailType_Max = EPbMailType::MT_Official;
constexpr int32 EPbMailType_ArraySize = static_cast<int32>(EPbMailType_Max) + 1;
MPROTOCOL_API bool CheckEPbMailTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbMailTypeDescription(EPbMailType Val);

template <typename Char>
struct fmt::formatter<EPbMailType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbMailType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 邮箱操作结构
*/
UENUM(BlueprintType)
enum class EPbMailOperation : uint8
{
    MOP_Fail = 0 UMETA(DisplayName="失败"),
    MOP_Done = 1 UMETA(DisplayName="完成"),
    MOP_InventoryIsFull = 2 UMETA(DisplayName="背包满"),
};
constexpr EPbMailOperation EPbMailOperation_Min = EPbMailOperation::MOP_Fail;
constexpr EPbMailOperation EPbMailOperation_Max = EPbMailOperation::MOP_InventoryIsFull;
constexpr int32 EPbMailOperation_ArraySize = static_cast<int32>(EPbMailOperation_Max) + 1;
MPROTOCOL_API bool CheckEPbMailOperationValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbMailOperationDescription(EPbMailOperation Val);

template <typename Char>
struct fmt::formatter<EPbMailOperation, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbMailOperation& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 聊天消息类型
*/
UENUM(BlueprintType)
enum class EPbChatMessageType : uint8
{
    CMT_Normal = 0 UMETA(DisplayName="普通"),
    CMT_SystemNotice = 1 UMETA(DisplayName="公告"),
};
constexpr EPbChatMessageType EPbChatMessageType_Min = EPbChatMessageType::CMT_Normal;
constexpr EPbChatMessageType EPbChatMessageType_Max = EPbChatMessageType::CMT_SystemNotice;
constexpr int32 EPbChatMessageType_ArraySize = static_cast<int32>(EPbChatMessageType_Max) + 1;
MPROTOCOL_API bool CheckEPbChatMessageTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbChatMessageTypeDescription(EPbChatMessageType Val);

template <typename Char>
struct fmt::formatter<EPbChatMessageType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbChatMessageType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 聊天频道
*/
UENUM(BlueprintType)
enum class EPbChatMessageChannel : uint8
{
    CMC_ColonyServers = 0 UMETA(DisplayName="大区（万仙）"),
    CMC_QuadServers = 1 UMETA(DisplayName="小区（异界）"),
    CMC_LocalServer = 2 UMETA(DisplayName="本区"),
    CMC_Organization = 3 UMETA(DisplayName="宗门"),
    CMC_Private = 4 UMETA(DisplayName="私聊"),
};
constexpr EPbChatMessageChannel EPbChatMessageChannel_Min = EPbChatMessageChannel::CMC_ColonyServers;
constexpr EPbChatMessageChannel EPbChatMessageChannel_Max = EPbChatMessageChannel::CMC_Private;
constexpr int32 EPbChatMessageChannel_ArraySize = static_cast<int32>(EPbChatMessageChannel_Max) + 1;
MPROTOCOL_API bool CheckEPbChatMessageChannelValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbChatMessageChannelDescription(EPbChatMessageChannel Val);

template <typename Char>
struct fmt::formatter<EPbChatMessageChannel, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbChatMessageChannel& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 炼器-保底类型
*/
UENUM(BlueprintType)
enum class EPbForgeChanceType : uint8
{
    ForgeChanceType_Normal = 0 UMETA(DisplayName="普通-无保底"),
    ForgeChanceType_Small = 1 UMETA(DisplayName="小保底"),
    ForgeChanceType_Big = 2 UMETA(DisplayName="大保底"),
};
constexpr EPbForgeChanceType EPbForgeChanceType_Min = EPbForgeChanceType::ForgeChanceType_Normal;
constexpr EPbForgeChanceType EPbForgeChanceType_Max = EPbForgeChanceType::ForgeChanceType_Big;
constexpr int32 EPbForgeChanceType_ArraySize = static_cast<int32>(EPbForgeChanceType_Max) + 1;
MPROTOCOL_API bool CheckEPbForgeChanceTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbForgeChanceTypeDescription(EPbForgeChanceType Val);

template <typename Char>
struct fmt::formatter<EPbForgeChanceType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbForgeChanceType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 炼器状态
*/
UENUM(BlueprintType)
enum class EPbForgeState : uint8
{
    ForgeState_None = 0 UMETA(DisplayName="未开始"),
    ForgeState_Running = 1 UMETA(DisplayName="进行中"),
    ForgeState_Done = 2 UMETA(DisplayName="已经完成（待领取）"),
};
constexpr EPbForgeState EPbForgeState_Min = EPbForgeState::ForgeState_None;
constexpr EPbForgeState EPbForgeState_Max = EPbForgeState::ForgeState_Done;
constexpr int32 EPbForgeState_ArraySize = static_cast<int32>(EPbForgeState_Max) + 1;
MPROTOCOL_API bool CheckEPbForgeStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbForgeStateDescription(EPbForgeState Val);

template <typename Char>
struct fmt::formatter<EPbForgeState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbForgeState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 秘药类型
*/
UENUM(BlueprintType)
enum class EPbPillElixirType : uint8
{
    PillElixirType_Other = 0 UMETA(DisplayName="其它"),
    PillElixirType_Exp = 1 UMETA(DisplayName="修为秘药"),
    PillElixirType_Property = 2 UMETA(DisplayName="属性秘药"),
    PillElixirType_Double = 3 UMETA(DisplayName="双修秘药"),
};
constexpr EPbPillElixirType EPbPillElixirType_Min = EPbPillElixirType::PillElixirType_Other;
constexpr EPbPillElixirType EPbPillElixirType_Max = EPbPillElixirType::PillElixirType_Double;
constexpr int32 EPbPillElixirType_ArraySize = static_cast<int32>(EPbPillElixirType_Max) + 1;
MPROTOCOL_API bool CheckEPbPillElixirTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPillElixirTypeDescription(EPbPillElixirType Val);

template <typename Char>
struct fmt::formatter<EPbPillElixirType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPillElixirType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 秘药效果类型
*/
UENUM(BlueprintType)
enum class EPbPillElixrEffectType : uint8
{
    PillElixrEffectType_None = 0 UMETA(DisplayName="其它"),
    PillElixrEffectType_PhyExp = 1 UMETA(DisplayName="炼体修为"),
    PillElixrEffectType_MagicExp = 2 UMETA(DisplayName="练法修为"),
    PillElixrEffectType_CriticalResist = 3 UMETA(DisplayName="会心抗性"),
    PillElixrEffectType_Critical = 4 UMETA(DisplayName="会心"),
    PillElixrEffectType_MagicDodge = 5 UMETA(DisplayName="法术闪避"),
    PillElixrEffectType_MagicAccuraccy = 6 UMETA(DisplayName="法术命中"),
    PillElixrEffectType_PhyDodge = 7 UMETA(DisplayName="物理闪避"),
    PillElixrEffectType_PhyAccuraccy = 8 UMETA(DisplayName="物理命中"),
};
constexpr EPbPillElixrEffectType EPbPillElixrEffectType_Min = EPbPillElixrEffectType::PillElixrEffectType_None;
constexpr EPbPillElixrEffectType EPbPillElixrEffectType_Max = EPbPillElixrEffectType::PillElixrEffectType_PhyAccuraccy;
constexpr int32 EPbPillElixrEffectType_ArraySize = static_cast<int32>(EPbPillElixrEffectType_Max) + 1;
MPROTOCOL_API bool CheckEPbPillElixrEffectTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPillElixrEffectTypeDescription(EPbPillElixrEffectType Val);

template <typename Char>
struct fmt::formatter<EPbPillElixrEffectType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPillElixrEffectType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 神通操作返回类型
*/
UENUM(BlueprintType)
enum class EPbPlayerAbilityActionResult : uint8
{
    PlayerAbilityAction_Success = 0 UMETA(DisplayName="成功"),
    PlayerAbilityAction_Timeout = 1 UMETA(DisplayName="超时"),
    PlayerAbilityAction_InvalidAbility = 2 UMETA(DisplayName="无效神通ID"),
    PlayerAbilityAction_GiveFailed_ExistAlready = 10 UMETA(DisplayName="习得神通-神通已经存在了"),
    PlayerAbilityAction_GiveFailed_OwnerFailed = 11 UMETA(DisplayName="习得神通-Owner条件不满足"),
    PlayerAbilityAction_UpgradeFailed_NonExist = 20 UMETA(DisplayName="升级失败-神通不存在"),
    PlayerAbilityAction_UpgradeFailed_MaxGrade = 21 UMETA(DisplayName="升级失败-已到达最大值"),
    PlayerAbilityAction_UpgradeFailed_OwnerFailed = 22 UMETA(DisplayName="升级失败-Owner条件不满足"),
    PlayerAbilityAction_ResetAll_CoolDown = 50 UMETA(DisplayName="一键重置神通冷却中"),
};
constexpr EPbPlayerAbilityActionResult EPbPlayerAbilityActionResult_Min = EPbPlayerAbilityActionResult::PlayerAbilityAction_Success;
constexpr EPbPlayerAbilityActionResult EPbPlayerAbilityActionResult_Max = EPbPlayerAbilityActionResult::PlayerAbilityAction_ResetAll_CoolDown;
constexpr int32 EPbPlayerAbilityActionResult_ArraySize = static_cast<int32>(EPbPlayerAbilityActionResult_Max) + 1;
MPROTOCOL_API bool CheckEPbPlayerAbilityActionResultValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPlayerAbilityActionResultDescription(EPbPlayerAbilityActionResult Val);

template <typename Char>
struct fmt::formatter<EPbPlayerAbilityActionResult, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPlayerAbilityActionResult& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 战斗模式
*/
UENUM(BlueprintType)
enum class EPbFightMode : uint8
{
    FightMode_Peace = 0 UMETA(DisplayName="和平 (无法攻击其他修士，无法被其他修士攻击)"),
    FightMode_All = 1 UMETA(DisplayName="全体 (可以攻击其他全体模式下的玩家)"),
    FightMode_Sept = 2 UMETA(DisplayName="宗门"),
    FightMode_Neutral = 3 UMETA(DisplayName="中立"),
};
constexpr EPbFightMode EPbFightMode_Min = EPbFightMode::FightMode_Peace;
constexpr EPbFightMode EPbFightMode_Max = EPbFightMode::FightMode_Neutral;
constexpr int32 EPbFightMode_ArraySize = static_cast<int32>(EPbFightMode_Max) + 1;
MPROTOCOL_API bool CheckEPbFightModeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFightModeDescription(EPbFightMode Val);

template <typename Char>
struct fmt::formatter<EPbFightMode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFightMode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 攻击锁定方式
*/
UENUM(BlueprintType)
enum class EPbAttackLockType : uint8
{
    AttackLockType_MinDistance = 0 UMETA(DisplayName="最近距离"),
    AttackLockType_LowHpPct = 1 UMETA(DisplayName="剩余血量比例最低"),
    AttackLockType_LowHpMpValue = 2 UMETA(DisplayName="气血真元绝对值最低"),
};
constexpr EPbAttackLockType EPbAttackLockType_Min = EPbAttackLockType::AttackLockType_MinDistance;
constexpr EPbAttackLockType EPbAttackLockType_Max = EPbAttackLockType::AttackLockType_LowHpMpValue;
constexpr int32 EPbAttackLockType_ArraySize = static_cast<int32>(EPbAttackLockType_Max) + 1;
MPROTOCOL_API bool CheckEPbAttackLockTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbAttackLockTypeDescription(EPbAttackLockType Val);

template <typename Char>
struct fmt::formatter<EPbAttackLockType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbAttackLockType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 攻击取消锁定方式
*/
UENUM(BlueprintType)
enum class EPbAttackUnlockType : uint8
{
    AttackUnlockType_MaxDistance = 0 UMETA(DisplayName="远距离"),
    AttackUnlockType_NearDistance = 1 UMETA(DisplayName="近距离"),
    AttackUnlockType_Screen = 2 UMETA(DisplayName="脱离屏幕"),
};
constexpr EPbAttackUnlockType EPbAttackUnlockType_Min = EPbAttackUnlockType::AttackUnlockType_MaxDistance;
constexpr EPbAttackUnlockType EPbAttackUnlockType_Max = EPbAttackUnlockType::AttackUnlockType_Screen;
constexpr int32 EPbAttackUnlockType_ArraySize = static_cast<int32>(EPbAttackUnlockType_Max) + 1;
MPROTOCOL_API bool CheckEPbAttackUnlockTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbAttackUnlockTypeDescription(EPbAttackUnlockType Val);

template <typename Char>
struct fmt::formatter<EPbAttackUnlockType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbAttackUnlockType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * BOSS入侵
*/
UENUM(BlueprintType)
enum class EPbBossInvasionState : uint8
{
    BossInvasionState_None = 0 UMETA(DisplayName="未知"),
    BossInvasionState_Wait = 1 UMETA(DisplayName="等待开始"),
    BossInvasionState_Prepare = 2 UMETA(DisplayName="准备阶段"),
    BossInvasionState_Fight = 3 UMETA(DisplayName="战斗阶段"),
    BossInvasionState_End = 4 UMETA(DisplayName="战斗结束"),
};
constexpr EPbBossInvasionState EPbBossInvasionState_Min = EPbBossInvasionState::BossInvasionState_None;
constexpr EPbBossInvasionState EPbBossInvasionState_Max = EPbBossInvasionState::BossInvasionState_End;
constexpr int32 EPbBossInvasionState_ArraySize = static_cast<int32>(EPbBossInvasionState_Max) + 1;
MPROTOCOL_API bool CheckEPbBossInvasionStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbBossInvasionStateDescription(EPbBossInvasionState Val);

template <typename Char>
struct fmt::formatter<EPbBossInvasionState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbBossInvasionState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 论剑台
*/
UENUM(BlueprintType)
enum class EPbSwordPkState : uint8
{
    SwordPkState_None = 0 UMETA(DisplayName="未知"),
    SwordPkState_Init = 1 UMETA(DisplayName="初始化"),
    SwordPkState_SeasonBegin = 2 UMETA(DisplayName="赛季开始"),
    SwordPkState_SeasonDuring = 3 UMETA(DisplayName="赛季进行中"),
    SwordPkState_SeasonEnd = 4 UMETA(DisplayName="赛季结束"),
};
constexpr EPbSwordPkState EPbSwordPkState_Min = EPbSwordPkState::SwordPkState_None;
constexpr EPbSwordPkState EPbSwordPkState_Max = EPbSwordPkState::SwordPkState_SeasonEnd;
constexpr int32 EPbSwordPkState_ArraySize = static_cast<int32>(EPbSwordPkState_Max) + 1;
MPROTOCOL_API bool CheckEPbSwordPkStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSwordPkStateDescription(EPbSwordPkState Val);

template <typename Char>
struct fmt::formatter<EPbSwordPkState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSwordPkState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 福缘功能相关
*/
UENUM(BlueprintType)
enum class EPbCheckListTaskType : uint8
{
    CLTT_None = 0 UMETA(DisplayName=""),
    CLTT_Login = 1 UMETA(DisplayName="登录游戏"),
    CLTT_EatPill = 2 UMETA(DisplayName="服用道行丹"),
    CLTT_Practice = 3 UMETA(DisplayName="吐纳"),
    CLTT_BuyInMarket = 4 UMETA(DisplayName="坊市购买"),
    CLTT_MakePill = 5 UMETA(DisplayName="炼丹"),
    CLTT_GongFa = 6 UMETA(DisplayName="修炼功法"),
    CLTT_MonsterTower = 7 UMETA(DisplayName="挑战镇妖塔"),
    CLTT_Portal = 8 UMETA(DisplayName="挑战秘境传送阵"),
    CLTT_Plant = 9 UMETA(DisplayName="药园种植"),
    CLTT_Forge = 10 UMETA(DisplayName="锻造装备"),
    CLTT_Ad = 11 UMETA(DisplayName="观影"),
    CLTT_Ability = 12 UMETA(DisplayName="升级神通"),
    CLTT_LocalPk = 13 UMETA(DisplayName="论剑台"),
    CLTT_Bounty = 14 UMETA(DisplayName="完成悬赏令"),
    CLTT_Reinforce = 15 UMETA(DisplayName="祭炼装备"),
    CLTT_CommonPk = 16 UMETA(DisplayName="跨界论道"),
    CLTT_BuyInDeluxShop = 17 UMETA(DisplayName="天机阁购买"),
    CLTT_MonsterInvasion = 18 UMETA(DisplayName="神兽入侵"),
    CLTT_WorldInvasion = 19 UMETA(DisplayName="异界入侵"),
    CLTT_Mine = 20 UMETA(DisplayName="灵脉采集"),
    CLTT_SectContribution = 21 UMETA(DisplayName="宗门贡献点"),
    CLTT_Quiz = 22 UMETA(DisplayName="八荒竞猜"),
    CLTT_Share = 23 UMETA(DisplayName="分享"),
    CLTT_MineTime = 24 UMETA(DisplayName="灵脉采集时长"),
};
constexpr EPbCheckListTaskType EPbCheckListTaskType_Min = EPbCheckListTaskType::CLTT_None;
constexpr EPbCheckListTaskType EPbCheckListTaskType_Max = EPbCheckListTaskType::CLTT_MineTime;
constexpr int32 EPbCheckListTaskType_ArraySize = static_cast<int32>(EPbCheckListTaskType_Max) + 1;
MPROTOCOL_API bool CheckEPbCheckListTaskTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCheckListTaskTypeDescription(EPbCheckListTaskType Val);

template <typename Char>
struct fmt::formatter<EPbCheckListTaskType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCheckListTaskType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 宗门职位
*/
UENUM(BlueprintType)
enum class EPbSeptPosition : uint8
{
    SeptPosition_None = 0 UMETA(DisplayName="未加入"),
    SeptPosition_Normal = 1 UMETA(DisplayName="成员"),
    SeptPosition_Manager = 2 UMETA(DisplayName="主事"),
    SeptPosition_Senator = 3 UMETA(DisplayName="长老"),
    SeptPosition_ViceChairman = 4 UMETA(DisplayName="副宗主"),
    SeptPosition_Chairman = 5 UMETA(DisplayName="宗主"),
};
constexpr EPbSeptPosition EPbSeptPosition_Min = EPbSeptPosition::SeptPosition_None;
constexpr EPbSeptPosition EPbSeptPosition_Max = EPbSeptPosition::SeptPosition_Chairman;
constexpr int32 EPbSeptPosition_ArraySize = static_cast<int32>(EPbSeptPosition_Max) + 1;
MPROTOCOL_API bool CheckEPbSeptPositionValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSeptPositionDescription(EPbSeptPosition Val);

template <typename Char>
struct fmt::formatter<EPbSeptPosition, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSeptPosition& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbSeptLogType : uint8
{
    SLT_None = 0 UMETA(DisplayName="未知类型"),
    SLT_Join = 1 UMETA(DisplayName="加入宗门"),
    SLT_Leave = 2 UMETA(DisplayName="离开宗门"),
    SLT_ChangeUp = 3 UMETA(DisplayName="升职"),
    SLT_ChangeDown = 4 UMETA(DisplayName="降职"),
};
constexpr EPbSeptLogType EPbSeptLogType_Min = EPbSeptLogType::SLT_None;
constexpr EPbSeptLogType EPbSeptLogType_Max = EPbSeptLogType::SLT_ChangeDown;
constexpr int32 EPbSeptLogType_ArraySize = static_cast<int32>(EPbSeptLogType_Max) + 1;
MPROTOCOL_API bool CheckEPbSeptLogTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSeptLogTypeDescription(EPbSeptLogType Val);

template <typename Char>
struct fmt::formatter<EPbSeptLogType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSeptLogType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 中立秘镜矿脉类型
*/
UENUM(BlueprintType)
enum class EPbSeptArenaStoneType : uint8
{
    SeptArenaStoneType_None = 0 UMETA(DisplayName="未知类型"),
    SeptArenaStoneType_Primary = 1 UMETA(DisplayName="初级"),
    SeptArenaStoneType_Intermediate = 2 UMETA(DisplayName="中级"),
    SeptArenaStoneType_Advanced = 3 UMETA(DisplayName="高级"),
    SeptArenaStoneType_Top = 4 UMETA(DisplayName="顶级"),
};
constexpr EPbSeptArenaStoneType EPbSeptArenaStoneType_Min = EPbSeptArenaStoneType::SeptArenaStoneType_None;
constexpr EPbSeptArenaStoneType EPbSeptArenaStoneType_Max = EPbSeptArenaStoneType::SeptArenaStoneType_Top;
constexpr int32 EPbSeptArenaStoneType_ArraySize = static_cast<int32>(EPbSeptArenaStoneType_Max) + 1;
MPROTOCOL_API bool CheckEPbSeptArenaStoneTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSeptArenaStoneTypeDescription(EPbSeptArenaStoneType Val);

template <typename Char>
struct fmt::formatter<EPbSeptArenaStoneType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSeptArenaStoneType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 功法状态
*/
UENUM(BlueprintType)
enum class EPbGongFaState : uint8
{
    Unable = 0 UMETA(DisplayName="未达到领悟条件"),
    ReadyToLearn = 1 UMETA(DisplayName="待领悟"),
    Training = 2 UMETA(DisplayName="领悟中"),
    Standby = 3 UMETA(DisplayName="待激活"),
    Activated = 4 UMETA(DisplayName="已经激活"),
};
constexpr EPbGongFaState EPbGongFaState_Min = EPbGongFaState::Unable;
constexpr EPbGongFaState EPbGongFaState_Max = EPbGongFaState::Activated;
constexpr int32 EPbGongFaState_ArraySize = static_cast<int32>(EPbGongFaState_Max) + 1;
MPROTOCOL_API bool CheckEPbGongFaStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbGongFaStateDescription(EPbGongFaState Val);

template <typename Char>
struct fmt::formatter<EPbGongFaState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbGongFaState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 学习功法特殊条件
*/
UENUM(BlueprintType)
enum class EPbGongFaCondition : uint8
{
    GFC_None = 0 UMETA(DisplayName="无条件"),
    GFC_HerbalBlock = 1 UMETA(DisplayName="解锁药园土地的数量"),
    GFC_CostItem = 2 UMETA(DisplayName="生涯消耗道具数量"),
    GFC_GuBao = 3 UMETA(DisplayName="持有古宝且星级达到n"),
    GFC_AvatarLevel = 4 UMETA(DisplayName="分身等级达到n"),
    GFC_OrangeAlchemy = 5 UMETA(DisplayName="炼制橙色丹药"),
    GFC_OrangeForge = 6 UMETA(DisplayName="炼制橙色装备"),
    GFC_MaxHP = 7 UMETA(DisplayName="最大气血"),
    GFC_MaxMP = 8 UMETA(DisplayName="最大真元"),
    GFC_PetLevel = 9 UMETA(DisplayName="拥有灵兽且等级达到n"),
    GFC_SkillLevel = 10 UMETA(DisplayName="对应神通等级达到n"),
    GFC_ImmortalRoad = 11 UMETA(DisplayName="完成全部飞升之路"),
    GFC_Crit = 12 UMETA(DisplayName="会心"),
    GFC_CritCoff = 13 UMETA(DisplayName="会心倍率"),
    GFC_MagBreak = 14 UMETA(DisplayName="法术破防"),
    GFC_PhyBreak = 15 UMETA(DisplayName="物理破防"),
    GFC_MagBlock = 16 UMETA(DisplayName="法术格挡"),
    GFC_PhyBlock = 17 UMETA(DisplayName="物理格挡"),
};
constexpr EPbGongFaCondition EPbGongFaCondition_Min = EPbGongFaCondition::GFC_None;
constexpr EPbGongFaCondition EPbGongFaCondition_Max = EPbGongFaCondition::GFC_PhyBlock;
constexpr int32 EPbGongFaCondition_ArraySize = static_cast<int32>(EPbGongFaCondition_Max) + 1;
MPROTOCOL_API bool CheckEPbGongFaConditionValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbGongFaConditionDescription(EPbGongFaCondition Val);

template <typename Char>
struct fmt::formatter<EPbGongFaCondition, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbGongFaCondition& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 功法效果
*/
UENUM(BlueprintType)
enum class EPbGongFaEffectType : uint8
{
    GFET_None = 0 UMETA(DisplayName="无条件"),
    GFET_BaseAtt = 1 UMETA(DisplayName="功法全属性提升"),
    GFET_DongFu = 2 UMETA(DisplayName="洞府灵气"),
    GFET_PracticeNum = 3 UMETA(DisplayName="吐纳次数"),
    GFET_PracticeUp = 4 UMETA(DisplayName="吐纳效果"),
    GFET_MergePractive = 5 UMETA(DisplayName="将所有吐纳合一次完成"),
    GFET_PillUp = 6 UMETA(DisplayName="修为丹药服用效果"),
    GFET_PillUseNum = 7 UMETA(DisplayName="每日修为丹药服用次数提升"),
    GFET_MoneyUp = 8 UMETA(DisplayName="秘境灵石产出"),
    GFET_AttackMonsterDamageAddPercent = 9 UMETA(DisplayName="攻击怪物伤害加成"),
    GFET_TakeMonsterDamageReducePercent = 10 UMETA(DisplayName="受怪物伤害减免"),
    GFET_FaBaoDamageToPlayerAddPercent = 11 UMETA(DisplayName="攻击修士法宝伤害"),
    GFET_FaBaoDamageToPlayerReducePercent = 12 UMETA(DisplayName="受到法宝伤害减免"),
    GFET_ShenTongDamageToPlayerAddPercent = 13 UMETA(DisplayName="攻击修士神通伤害"),
    GFET_ShenTongDamageToPlayerReducePercent = 14 UMETA(DisplayName="受到神通伤害减免"),
    GFET_Mind = 15 UMETA(DisplayName="神识"),
    GFET_MpRecoverPercent = 16 UMETA(DisplayName="真元回复"),
    GFET_Mp = 17 UMETA(DisplayName="真元"),
    GFET_BaseMp = 18 UMETA(DisplayName="基础真元"),
    GFET_HpRecoverPercent = 19 UMETA(DisplayName="气血回复"),
    GFET_Hp = 20 UMETA(DisplayName="气血"),
    GFET_BaseHp = 21 UMETA(DisplayName="基础气血"),
    GFET_MagAtt = 22 UMETA(DisplayName="法术攻击"),
    GFET_PhyAtt = 23 UMETA(DisplayName="物理攻击"),
    GFET_Crit = 24 UMETA(DisplayName="会心"),
    GFET_CritCoeff = 25 UMETA(DisplayName="会心倍率"),
    GFET_CritBlock = 26 UMETA(DisplayName="会心格挡"),
    GFET_ControlRateAtt = 27 UMETA(DisplayName="控制概率强化"),
    GFET_ControlRateDef = 28 UMETA(DisplayName="控制概率抗性"),
    GFET_ControlTimeAtt = 29 UMETA(DisplayName="控制时间强化"),
    GFET_ControlTimeDef = 30 UMETA(DisplayName="控制时间抗性"),
    GFET_MoveSpeed = 31 UMETA(DisplayName="移动速度"),
    GFET_Intellect = 32 UMETA(DisplayName="内息"),
    GFET_Strength = 33 UMETA(DisplayName="体魄"),
};
constexpr EPbGongFaEffectType EPbGongFaEffectType_Min = EPbGongFaEffectType::GFET_None;
constexpr EPbGongFaEffectType EPbGongFaEffectType_Max = EPbGongFaEffectType::GFET_Strength;
constexpr int32 EPbGongFaEffectType_ArraySize = static_cast<int32>(EPbGongFaEffectType_Max) + 1;
MPROTOCOL_API bool CheckEPbGongFaEffectTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbGongFaEffectTypeDescription(EPbGongFaEffectType Val);

template <typename Char>
struct fmt::formatter<EPbGongFaEffectType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbGongFaEffectType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 福赠类型
*/
UENUM(BlueprintType)
enum class EPbFuZengType : uint8
{
    FZT_None = 0 UMETA(DisplayName="无"),
    FZT_CombatPower = 1 UMETA(DisplayName="战力"),
    FZT_MonsterTower = 2 UMETA(DisplayName="镇妖塔"),
    FZT_Ability = 3 UMETA(DisplayName="神通"),
    FZT_Pill = 4 UMETA(DisplayName="秘药"),
    FZT_GongFa = 5 UMETA(DisplayName="功法"),
    FZT_GuBaoCollection = 6 UMETA(DisplayName="古宝收集"),
    FZT_GuBaoLevel = 7 UMETA(DisplayName="古宝星级"),
    FZT_GaCha = 8 UMETA(DisplayName="外域探宝"),
    FZT_MaxNum = 9 UMETA(DisplayName="最大值"),
};
constexpr EPbFuZengType EPbFuZengType_Min = EPbFuZengType::FZT_None;
constexpr EPbFuZengType EPbFuZengType_Max = EPbFuZengType::FZT_MaxNum;
constexpr int32 EPbFuZengType_ArraySize = static_cast<int32>(EPbFuZengType_Max) + 1;
MPROTOCOL_API bool CheckEPbFuZengTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFuZengTypeDescription(EPbFuZengType Val);

template <typename Char>
struct fmt::formatter<EPbFuZengType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFuZengType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 福赠状态
*/
UENUM(BlueprintType)
enum class EPbFuZengState : uint8
{
    FZS_UnFinished = 0 UMETA(DisplayName="未完成"),
    FZS_Finished = 1 UMETA(DisplayName="已完成待领取"),
    FZS_Received = 2 UMETA(DisplayName="已领取"),
};
constexpr EPbFuZengState EPbFuZengState_Min = EPbFuZengState::FZS_UnFinished;
constexpr EPbFuZengState EPbFuZengState_Max = EPbFuZengState::FZS_Received;
constexpr int32 EPbFuZengState_ArraySize = static_cast<int32>(EPbFuZengState_Max) + 1;
MPROTOCOL_API bool CheckEPbFuZengStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFuZengStateDescription(EPbFuZengState Val);

template <typename Char>
struct fmt::formatter<EPbFuZengState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFuZengState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝升星加成方式
*/
UENUM(BlueprintType)
enum class EPbCollectionStarAdditionType : uint8
{
    CSAT_Times = 0 UMETA(DisplayName="乘"),
    CSAT_Plus = 1 UMETA(DisplayName="加"),
};
constexpr EPbCollectionStarAdditionType EPbCollectionStarAdditionType_Min = EPbCollectionStarAdditionType::CSAT_Times;
constexpr EPbCollectionStarAdditionType EPbCollectionStarAdditionType_Max = EPbCollectionStarAdditionType::CSAT_Plus;
constexpr int32 EPbCollectionStarAdditionType_ArraySize = static_cast<int32>(EPbCollectionStarAdditionType_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionStarAdditionTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionStarAdditionTypeDescription(EPbCollectionStarAdditionType Val);

template <typename Char>
struct fmt::formatter<EPbCollectionStarAdditionType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionStarAdditionType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝使用类型
*/
UENUM(BlueprintType)
enum class EPbCollectionUseType : uint8
{
    CUT_Active = 0 UMETA(DisplayName="主动"),
    CUT_Passive = 1 UMETA(DisplayName="被动"),
    CUT_Functional = 2 UMETA(DisplayName="功能"),
};
constexpr EPbCollectionUseType EPbCollectionUseType_Min = EPbCollectionUseType::CUT_Active;
constexpr EPbCollectionUseType EPbCollectionUseType_Max = EPbCollectionUseType::CUT_Functional;
constexpr int32 EPbCollectionUseType_ArraySize = static_cast<int32>(EPbCollectionUseType_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionUseTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionUseTypeDescription(EPbCollectionUseType Val);

template <typename Char>
struct fmt::formatter<EPbCollectionUseType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionUseType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝区域类型
*/
UENUM(BlueprintType)
enum class EPbCollectionZoneType : uint8
{
    CZT_Unknown = 0 UMETA(DisplayName="未定义"),
    CZT_RenJie = 1 UMETA(DisplayName="人界"),
    CZT_LingJie = 2 UMETA(DisplayName="灵界"),
    CZT_XianMo = 3 UMETA(DisplayName="仙魔"),
    CZT_LiuYu = 4 UMETA(DisplayName="领域"),
    CZT_QiTa = 5 UMETA(DisplayName="其他"),
};
constexpr EPbCollectionZoneType EPbCollectionZoneType_Min = EPbCollectionZoneType::CZT_Unknown;
constexpr EPbCollectionZoneType EPbCollectionZoneType_Max = EPbCollectionZoneType::CZT_QiTa;
constexpr int32 EPbCollectionZoneType_ArraySize = static_cast<int32>(EPbCollectionZoneType_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionZoneTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionZoneTypeDescription(EPbCollectionZoneType Val);

template <typename Char>
struct fmt::formatter<EPbCollectionZoneType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionZoneType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝筛选类型
*/
UENUM(BlueprintType)
enum class EPbCollectionFilterType : uint8
{
    CFT_Owned = 0 UMETA(DisplayName="已拥有"),
    CFT_All = 1 UMETA(DisplayName="图鉴"),
    CFT_Source = 2 UMETA(DisplayName="渊源"),
    CFT_Suit = 3 UMETA(DisplayName="套装"),
    CFT_CanUp = 4 UMETA(DisplayName="可升星"),
};
constexpr EPbCollectionFilterType EPbCollectionFilterType_Min = EPbCollectionFilterType::CFT_Owned;
constexpr EPbCollectionFilterType EPbCollectionFilterType_Max = EPbCollectionFilterType::CFT_CanUp;
constexpr int32 EPbCollectionFilterType_ArraySize = static_cast<int32>(EPbCollectionFilterType_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionFilterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionFilterTypeDescription(EPbCollectionFilterType Val);

template <typename Char>
struct fmt::formatter<EPbCollectionFilterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionFilterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝排序类型
*/
UENUM(BlueprintType)
enum class EPbCollectionOrderType : uint8
{
    COT_Quality = 0 UMETA(DisplayName="品质"),
    COT_Property = 1 UMETA(DisplayName="属性"),
    COT_Level = 2 UMETA(DisplayName="注灵"),
    COT_Active = 3 UMETA(DisplayName="活动"),
};
constexpr EPbCollectionOrderType EPbCollectionOrderType_Min = EPbCollectionOrderType::COT_Quality;
constexpr EPbCollectionOrderType EPbCollectionOrderType_Max = EPbCollectionOrderType::COT_Active;
constexpr int32 EPbCollectionOrderType_ArraySize = static_cast<int32>(EPbCollectionOrderType_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionOrderTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionOrderTypeDescription(EPbCollectionOrderType Val);

template <typename Char>
struct fmt::formatter<EPbCollectionOrderType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionOrderType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝渊源状态
*/
UENUM(BlueprintType)
enum class EPbCollectionHistoryState : uint8
{
    CHS_None = 0 UMETA(DisplayName="没有渊源"),
    CHS_NotActive = 1 UMETA(DisplayName="还未激活"),
    CHS_CanDraw = 2 UMETA(DisplayName="可以领取"),
    CHS_DrawDone = 3 UMETA(DisplayName="已领取"),
};
constexpr EPbCollectionHistoryState EPbCollectionHistoryState_Min = EPbCollectionHistoryState::CHS_None;
constexpr EPbCollectionHistoryState EPbCollectionHistoryState_Max = EPbCollectionHistoryState::CHS_DrawDone;
constexpr int32 EPbCollectionHistoryState_ArraySize = static_cast<int32>(EPbCollectionHistoryState_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionHistoryStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionHistoryStateDescription(EPbCollectionHistoryState Val);

template <typename Char>
struct fmt::formatter<EPbCollectionHistoryState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionHistoryState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 古宝收集奖励状态
*/
UENUM(BlueprintType)
enum class EPbCollectionZoneActiveAwardState : uint8
{
    CZAAS_None = 0 UMETA(DisplayName="无法领取"),
    CZAAS_CanDraw = 1 UMETA(DisplayName="可以领取"),
    CZAAS_DrawDone = 2 UMETA(DisplayName="已经领取"),
};
constexpr EPbCollectionZoneActiveAwardState EPbCollectionZoneActiveAwardState_Min = EPbCollectionZoneActiveAwardState::CZAAS_None;
constexpr EPbCollectionZoneActiveAwardState EPbCollectionZoneActiveAwardState_Max = EPbCollectionZoneActiveAwardState::CZAAS_DrawDone;
constexpr int32 EPbCollectionZoneActiveAwardState_ArraySize = static_cast<int32>(EPbCollectionZoneActiveAwardState_Max) + 1;
MPROTOCOL_API bool CheckEPbCollectionZoneActiveAwardStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbCollectionZoneActiveAwardStateDescription(EPbCollectionZoneActiveAwardState Val);

template <typename Char>
struct fmt::formatter<EPbCollectionZoneActiveAwardState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbCollectionZoneActiveAwardState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 生涯计数类型
*/
UENUM(BlueprintType)
enum class EPbLifeCounterType : uint8
{
    LCT_Unknown = 0 UMETA(DisplayName="未知"),
    LCT_AlchemyQuality = 1 UMETA(DisplayName="炼制对应品质丹药"),
    LCT_ForgeEquipmentQuality = 2 UMETA(DisplayName="炼制对应品质装备"),
    LCT_ForgeSkillEquipmentQuality = 3 UMETA(DisplayName="炼制对应品质法宝"),
    LCT_AbilityLevelPhy = 4 UMETA(DisplayName="体修神通升级"),
    LCT_AbilityLevelMag = 5 UMETA(DisplayName="法修神通升级"),
    LCT_KillCommonMonster = 6 UMETA(DisplayName="击杀普通小怪"),
    LCT_KillEliteMonster = 7 UMETA(DisplayName="击杀精英怪"),
    LCT_KillBoss = 8 UMETA(DisplayName="击杀首领"),
    LCT_ActiveGongFaPoint = 9 UMETA(DisplayName="激活功法节点"),
    LCT_FinishingGongFa = 10 UMETA(DisplayName="功法圆满"),
    LCT_PillElixirUsed = 11 UMETA(DisplayName="秘药使用"),
    LCT_InventoryItemUsed = 12 UMETA(DisplayName="背包道具使用按ID"),
    LCT_InventoryItemUsedType = 13 UMETA(DisplayName="背包道具使用按类型"),
    LCT_LoginGame = 14 UMETA(DisplayName="登录游戏每日"),
    LCT_FuYuanPoint = 15 UMETA(DisplayName="福缘活跃度"),
    LCT_SeptQuest = 16 UMETA(DisplayName="完成宗门事务"),
    LCT_MonsterTower = 17 UMETA(DisplayName="镇妖塔层数"),
    LCT_CollectionStar = 18 UMETA(DisplayName="古宝星级"),
    LCT_CollectionNum = 19 UMETA(DisplayName="古宝收集"),
    LCT_GaChaOpen = 20 UMETA(DisplayName="探宝计数"),
    LCT_Shanhetu = 21 UMETA(DisplayName="山河图计数"),
    LCT_PropertyPillUse = 22 UMETA(DisplayName="属性丹药按id计数"),
    LCT_FarmBlockNum = 23 UMETA(DisplayName="药园地块解锁数"),
    LCT_AlchemyType = 24 UMETA(DisplayName="炼制对应类型丹药"),
};
constexpr EPbLifeCounterType EPbLifeCounterType_Min = EPbLifeCounterType::LCT_Unknown;
constexpr EPbLifeCounterType EPbLifeCounterType_Max = EPbLifeCounterType::LCT_AlchemyType;
constexpr int32 EPbLifeCounterType_ArraySize = static_cast<int32>(EPbLifeCounterType_Max) + 1;
MPROTOCOL_API bool CheckEPbLifeCounterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbLifeCounterTypeDescription(EPbLifeCounterType Val);

template <typename Char>
struct fmt::formatter<EPbLifeCounterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbLifeCounterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbModelType : uint8
{
    MT_Unknown = 0 UMETA(DisplayName="未知"),
    MT_HeadIcon = 1 UMETA(DisplayName="头像"),
    MT_HeadFrame = 2 UMETA(DisplayName="头像框"),
    MT_Body = 3 UMETA(DisplayName="衣服"),
    MT_Hair = 4 UMETA(DisplayName="发型"),
    MT_Hat = 5 UMETA(DisplayName="头饰"),
    MT_HeadDeco = 6 UMETA(DisplayName="额饰"),
    MT_FaceDeco = 7 UMETA(DisplayName="面饰"),
    MT_Mask = 8 UMETA(DisplayName="面具"),
    MT_EarDeco = 9 UMETA(DisplayName="耳饰"),
    MT_EyeBrows = 10 UMETA(DisplayName="眉眼"),
    MT_Makeup = 11 UMETA(DisplayName="妆容"),
    MT_FacePaint = 12 UMETA(DisplayName="面绘"),
    MT_FacePrint = 13 UMETA(DisplayName="印容"),
    MT_BackLight = 14 UMETA(DisplayName="灵光"),
    MT_MaxNum = 15 UMETA(DisplayName="枚举容量"),
};
constexpr EPbModelType EPbModelType_Min = EPbModelType::MT_Unknown;
constexpr EPbModelType EPbModelType_Max = EPbModelType::MT_MaxNum;
constexpr int32 EPbModelType_ArraySize = static_cast<int32>(EPbModelType_Max) + 1;
MPROTOCOL_API bool CheckEPbModelTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbModelTypeDescription(EPbModelType Val);

template <typename Char>
struct fmt::formatter<EPbModelType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbModelType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 道具变化类型
*/
UENUM(BlueprintType)
enum class EPbInventoryItemChangedType : uint8
{
    IICT_Modify = 0 UMETA(DisplayName="修改"),
    IICT_Add = 1 UMETA(DisplayName="新增"),
    IICT_Del = 2 UMETA(DisplayName="删除"),
};
constexpr EPbInventoryItemChangedType EPbInventoryItemChangedType_Min = EPbInventoryItemChangedType::IICT_Modify;
constexpr EPbInventoryItemChangedType EPbInventoryItemChangedType_Max = EPbInventoryItemChangedType::IICT_Del;
constexpr int32 EPbInventoryItemChangedType_ArraySize = static_cast<int32>(EPbInventoryItemChangedType_Max) + 1;
MPROTOCOL_API bool CheckEPbInventoryItemChangedTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbInventoryItemChangedTypeDescription(EPbInventoryItemChangedType Val);

template <typename Char>
struct fmt::formatter<EPbInventoryItemChangedType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbInventoryItemChangedType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 秘境探索事件状态
*/
UENUM(BlueprintType)
enum class EPbArenaCheckListState : uint8
{
    ACLS_None = 0 UMETA(DisplayName="未知"),
    ACLS_UnFinished = 1 UMETA(DisplayName="未完成"),
    ACLS_Finished = 2 UMETA(DisplayName="已完成待领取"),
    ACLS_Received = 3 UMETA(DisplayName="已领取"),
};
constexpr EPbArenaCheckListState EPbArenaCheckListState_Min = EPbArenaCheckListState::ACLS_None;
constexpr EPbArenaCheckListState EPbArenaCheckListState_Max = EPbArenaCheckListState::ACLS_Received;
constexpr int32 EPbArenaCheckListState_ArraySize = static_cast<int32>(EPbArenaCheckListState_Max) + 1;
MPROTOCOL_API bool CheckEPbArenaCheckListStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbArenaCheckListStateDescription(EPbArenaCheckListState Val);

template <typename Char>
struct fmt::formatter<EPbArenaCheckListState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbArenaCheckListState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 秘境探索事件奖励领取状态
*/
UENUM(BlueprintType)
enum class EPbArenaCheckListRewardState : uint8
{
    ACLRS_UnFinished = 0 UMETA(DisplayName="未完成"),
    ACLRS_UnReceived = 1 UMETA(DisplayName="已完成待领取"),
    ACLRS_Received = 2 UMETA(DisplayName="已领取"),
};
constexpr EPbArenaCheckListRewardState EPbArenaCheckListRewardState_Min = EPbArenaCheckListRewardState::ACLRS_UnFinished;
constexpr EPbArenaCheckListRewardState EPbArenaCheckListRewardState_Max = EPbArenaCheckListRewardState::ACLRS_Received;
constexpr int32 EPbArenaCheckListRewardState_ArraySize = static_cast<int32>(EPbArenaCheckListRewardState_Max) + 1;
MPROTOCOL_API bool CheckEPbArenaCheckListRewardStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbArenaCheckListRewardStateDescription(EPbArenaCheckListRewardState Val);

template <typename Char>
struct fmt::formatter<EPbArenaCheckListRewardState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbArenaCheckListRewardState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 药童效果类型
*/
UENUM(BlueprintType)
enum class EPbFarmLandState : uint8
{
    FLS_None = 0 UMETA(DisplayName="未解锁"),
    FLS_Free = 1 UMETA(DisplayName="所有灵植生长周期减少"),
    FLS_InUse = 2 UMETA(DisplayName="正在使用"),
};
constexpr EPbFarmLandState EPbFarmLandState_Min = EPbFarmLandState::FLS_None;
constexpr EPbFarmLandState EPbFarmLandState_Max = EPbFarmLandState::FLS_InUse;
constexpr int32 EPbFarmLandState_ArraySize = static_cast<int32>(EPbFarmLandState_Max) + 1;
MPROTOCOL_API bool CheckEPbFarmLandStateValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFarmLandStateDescription(EPbFarmLandState Val);

template <typename Char>
struct fmt::formatter<EPbFarmLandState, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFarmLandState& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 药童效果类型
*/
UENUM(BlueprintType)
enum class EPbFarmerEffectType : uint8
{
    FET_None = 0 UMETA(DisplayName="未完成"),
    FET_ReduceGrowthTime = 1 UMETA(DisplayName="所有灵植生长周期减少"),
    FET_RipeningUp = 2 UMETA(DisplayName="催熟效果增加"),
    FET_WateringUp = 3 UMETA(DisplayName="浇灌效果增加"),
    FET_ExtraOneHarvest = 4 UMETA(DisplayName="收获灵植时有几率额外获得1株"),
    FET_WateringNumUp = 5 UMETA(DisplayName="浇灌次数增加"),
    FET_ReduceGrowthPeriod = 6 UMETA(DisplayName="所有灵植生长周期减少百分比"),
    FET_MaxNum = 7 UMETA(DisplayName="最大效果数"),
};
constexpr EPbFarmerEffectType EPbFarmerEffectType_Min = EPbFarmerEffectType::FET_None;
constexpr EPbFarmerEffectType EPbFarmerEffectType_Max = EPbFarmerEffectType::FET_MaxNum;
constexpr int32 EPbFarmerEffectType_ArraySize = static_cast<int32>(EPbFarmerEffectType_Max) + 1;
MPROTOCOL_API bool CheckEPbFarmerEffectTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbFarmerEffectTypeDescription(EPbFarmerEffectType Val);

template <typename Char>
struct fmt::formatter<EPbFarmerEffectType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbFarmerEffectType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 服务器计数器类型
*/
UENUM(BlueprintType)
enum class EPbServerCounterType : uint8
{
    SCT_None = 0 UMETA(DisplayName="None"),
    SCT_BiographyEventKillNpcWithHighDegree = 1 UMETA(DisplayName="史记-高等级怪物击杀"),
    SCT_KillNpcWithHighDegree = 2 UMETA(DisplayName="高等级怪物击杀"),
    SCT_DegreeUpPhy = 3 UMETA(DisplayName="玩家炼体升级计数"),
    SCT_DegreeUpMag = 4 UMETA(DisplayName="玩家修法升级计数"),
};
constexpr EPbServerCounterType EPbServerCounterType_Min = EPbServerCounterType::SCT_None;
constexpr EPbServerCounterType EPbServerCounterType_Max = EPbServerCounterType::SCT_DegreeUpMag;
constexpr int32 EPbServerCounterType_ArraySize = static_cast<int32>(EPbServerCounterType_Max) + 1;
MPROTOCOL_API bool CheckEPbServerCounterTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbServerCounterTypeDescription(EPbServerCounterType Val);

template <typename Char>
struct fmt::formatter<EPbServerCounterType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbServerCounterType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 刷新周期类型
*/
UENUM(BlueprintType)
enum class EPbPeriodType : uint8
{
    PT_None = 0 UMETA(DisplayName="未知"),
    PT_Day = 1 UMETA(DisplayName="日"),
    PT_Week = 2 UMETA(DisplayName="周"),
    PT_Mouth = 3 UMETA(DisplayName="月"),
    PT_Year = 4 UMETA(DisplayName="年"),
};
constexpr EPbPeriodType EPbPeriodType_Min = EPbPeriodType::PT_None;
constexpr EPbPeriodType EPbPeriodType_Max = EPbPeriodType::PT_Year;
constexpr int32 EPbPeriodType_ArraySize = static_cast<int32>(EPbPeriodType_Max) + 1;
MPROTOCOL_API bool CheckEPbPeriodTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPeriodTypeDescription(EPbPeriodType Val);

template <typename Char>
struct fmt::formatter<EPbPeriodType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPeriodType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};
