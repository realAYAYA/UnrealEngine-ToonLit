#include "PbDefines.h"
#include "defines.pb.h"



bool CheckEPbItemQualityValid(int32 Val)
{
    return idlepb::ItemQuality_IsValid(Val);
}

const TCHAR* GetEPbItemQualityDescription(EPbItemQuality Val)
{
    switch (Val)
    {
        case EPbItemQuality::EQ_None: return TEXT("其他");
        case EPbItemQuality::EQ_White: return TEXT("白");
        case EPbItemQuality::EQ_Green: return TEXT("绿");
        case EPbItemQuality::EQ_Blue: return TEXT("蓝");
        case EPbItemQuality::EQ_Purple: return TEXT("紫");
        case EPbItemQuality::EQ_Orange: return TEXT("橙");
        case EPbItemQuality::EQ_Red: return TEXT("红");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbItemShowTypeValid(int32 Val)
{
    return idlepb::ItemShowType_IsValid(Val);
}

const TCHAR* GetEPbItemShowTypeDescription(EPbItemShowType Val)
{
    switch (Val)
    {
        case EPbItemShowType::ItemShowType_None: return TEXT("其它");
        case EPbItemShowType::ItemShowType_Equipment: return TEXT("装备");
        case EPbItemShowType::ItemShowType_Pill: return TEXT("丹药");
        case EPbItemShowType::ItemShowType_Material: return TEXT("材料");
        case EPbItemShowType::ItemShowType_Special: return TEXT("特殊");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbItemTypeValid(int32 Val)
{
    return idlepb::ItemType_IsValid(Val);
}

const TCHAR* GetEPbItemTypeDescription(EPbItemType Val)
{
    switch (Val)
    {
        case EPbItemType::ItemType_None: return TEXT("其它");
        case EPbItemType::ItemType_ExpPill: return TEXT("修为丹药");
        case EPbItemType::ItemType_Weapon: return TEXT("武器");
        case EPbItemType::ItemType_CLOTHING: return TEXT("防具");
        case EPbItemType::ItemType_JEWELRY: return TEXT("饰品");
        case EPbItemType::ItemType_SkillEquipment: return TEXT("法宝");
        case EPbItemType::ItemType_RecoverPill: return TEXT("回复丹药");
        case EPbItemType::ItemType_SkillBook: return TEXT("技能书(神通残篇)");
        case EPbItemType::ItemType_SecretPill: return TEXT("秘药");
        case EPbItemType::ItemType_AttrPill: return TEXT("属性丹药");
        case EPbItemType::ItemType_BreakthroughPill: return TEXT("突破丹药");
        case EPbItemType::ItemType_PillMaterial: return TEXT("炼丹材料");
        case EPbItemType::ItemType_WeaponMaterial: return TEXT("铸器材料");
        case EPbItemType::ItemType_PillRecipe: return TEXT("炼丹配方");
        case EPbItemType::ItemType_EquipRecipe: return TEXT("炼器配方");
        case EPbItemType::ItemType_ForgeMaterial: return TEXT("炼器辅材");
        case EPbItemType::ItemType_GiftPackage: return TEXT("礼包");
        case EPbItemType::ItemType_SpaceMaterial: return TEXT("空间材料");
        case EPbItemType::ItemType_Seed: return TEXT("种子");
        case EPbItemType::ItemType_ShanHeTu: return TEXT("山河图道具");
        case EPbItemType::ItemType_QiWen: return TEXT("器纹材料");
        case EPbItemType::ItemType_QiLing: return TEXT("装备器灵材料");
        case EPbItemType::ItemType_GuBao: return TEXT("古宝");
        case EPbItemType::ItemType_GuBaoPiece: return TEXT("古宝碎片");
        case EPbItemType::ItemType_QiLingSkill: return TEXT("法宝器灵材料");
        case EPbItemType::ItemType_ZhuLingMeterial: return TEXT("古宝注灵材料");
        case EPbItemType::ItemType_FarmRipe: return TEXT("药园催熟");
        case EPbItemType::ItemType_Token: return TEXT("数值型道具");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbEquipmentMainTypeValid(int32 Val)
{
    return idlepb::EquipmentMainType_IsValid(Val);
}

const TCHAR* GetEPbEquipmentMainTypeDescription(EPbEquipmentMainType Val)
{
    switch (Val)
    {
        case EPbEquipmentMainType::EquipmentMainType_None: return TEXT("其它");
        case EPbEquipmentMainType::EquipmentMainType_Weapon: return TEXT("武器");
        case EPbEquipmentMainType::EquipmentMainType_CLOTHING: return TEXT("防具");
        case EPbEquipmentMainType::EquipmentMainType_JEWELRY: return TEXT("饰品");
        case EPbEquipmentMainType::EquipmentMainType_AttSkillEquipment: return TEXT("进攻类法宝");
        case EPbEquipmentMainType::EquipmentMainType_DefSkillEquipment: return TEXT("防御类法宝");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPerkValueAddTypeValid(int32 Val)
{
    return idlepb::PerkValueAddType_IsValid(Val);
}

const TCHAR* GetEPbPerkValueAddTypeDescription(EPbPerkValueAddType Val)
{
    switch (Val)
    {
        case EPbPerkValueAddType::PerkValueAddType_None: return TEXT("其他");
        case EPbPerkValueAddType::PerkValueAddType_Add: return TEXT("增加");
        case EPbPerkValueAddType::PerkValueAddType_Sub: return TEXT("减少");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPerkValueEffectTypeValid(int32 Val)
{
    return idlepb::PerkValueEffectType_IsValid(Val);
}

const TCHAR* GetEPbPerkValueEffectTypeDescription(EPbPerkValueEffectType Val)
{
    switch (Val)
    {
        case EPbPerkValueEffectType::PerkValueEffectType_None: return TEXT("其他");
        case EPbPerkValueEffectType::PerkValueEffectType_EquipmentBasicAttribute: return TEXT("装备属性百分比");
        case EPbPerkValueEffectType::PerkValueEffectType_Attack: return TEXT("攻击");
        case EPbPerkValueEffectType::PerkValueEffectType_HpMp: return TEXT("气血&真元");
        case EPbPerkValueEffectType::PerkValueEffectType_Defence: return TEXT("防御");
        case EPbPerkValueEffectType::PerkValueEffectType_CritAndCritDef: return TEXT("会心和会心抗性");
        case EPbPerkValueEffectType::PerkValueEffectType_CritCoeff: return TEXT("会心倍率");
        case EPbPerkValueEffectType::PerkValueEffectType_StrengthIntellect: return TEXT("体魄内息");
        case EPbPerkValueEffectType::PerkValueEffectType_RecoverPercent: return TEXT("气血真元回复");
        case EPbPerkValueEffectType::PerkValueEffectType_Agility: return TEXT("身法");
        case EPbPerkValueEffectType::PerkValueEffectType_DodgeHit: return TEXT("闪避命中");
        case EPbPerkValueEffectType::PerkValueEffectType_MoveSpeed: return TEXT("移动速度");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPerkIdConstsValid(int32 Val)
{
    return idlepb::PerkIdConsts_IsValid(Val);
}

const TCHAR* GetEPbPerkIdConstsDescription(EPbPerkIdConsts Val)
{
    switch (Val)
    {
        case EPbPerkIdConsts::PerkIdConsts_None: return TEXT("其它");
        case EPbPerkIdConsts::PerkIdConsts_EquipmentBasicAttribute: return TEXT("装备属性百分比");
        case EPbPerkIdConsts::PerkIdConsts_MagAttack: return TEXT("人物法攻");
        case EPbPerkIdConsts::PerkIdConsts_PhyAttack: return TEXT("任务物攻");
        case EPbPerkIdConsts::PerkIdConsts_Hp: return TEXT("人物气血");
        case EPbPerkIdConsts::PerkIdConsts_Mp: return TEXT("人物真元");
        case EPbPerkIdConsts::PerkIdConsts_PhyDefence: return TEXT("人物物防");
        case EPbPerkIdConsts::PerkIdConsts_MagDefence: return TEXT("人物法防");
        case EPbPerkIdConsts::PerkIdConsts_Crit: return TEXT("人物会心");
        case EPbPerkIdConsts::PerkIdConsts_CritCoeff: return TEXT("人物会心倍率");
        case EPbPerkIdConsts::PerkIdConsts_Strength: return TEXT("人物体魄");
        case EPbPerkIdConsts::PerkIdConsts_Intellect: return TEXT("人物内息");
        case EPbPerkIdConsts::PerkIdConsts_MpRecoverPercent: return TEXT("人物真元回复");
        case EPbPerkIdConsts::PerkIdConsts_HpRecoverPercent: return TEXT("人物气血回复");
        case EPbPerkIdConsts::PerkIdConsts_Agility: return TEXT("人物身法");
        case EPbPerkIdConsts::PerkIdConsts_MagDodge: return TEXT("人物法术闪避");
        case EPbPerkIdConsts::PerkIdConsts_PhyDodge: return TEXT("人物物理闪避");
        case EPbPerkIdConsts::PerkIdConsts_CritDef: return TEXT("人物会心抗性");
        case EPbPerkIdConsts::PerkIdConsts_PhyHit: return TEXT("人物物理命中");
        case EPbPerkIdConsts::PerkIdConsts_MagHit: return TEXT("人物法术命中");
        case EPbPerkIdConsts::PerkIdConsts_MoveSpeed: return TEXT("人物移动速度");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbEquipmentSlotStateValid(int32 Val)
{
    return idlepb::EquipmentSlotState_IsValid(Val);
}

const TCHAR* GetEPbEquipmentSlotStateDescription(EPbEquipmentSlotState Val)
{
    switch (Val)
    {
        case EPbEquipmentSlotState::EquipmentSlotState_Locked: return TEXT("锁定");
        case EPbEquipmentSlotState::EquipmentSlotState_ToUnlock: return TEXT("待解锁");
        case EPbEquipmentSlotState::EquipmentSlotState_UnlockNoEquip: return TEXT("已解锁 - 无可用装备");
        case EPbEquipmentSlotState::EquipmentSlotState_UnlockEquipInBag: return TEXT("已解锁 - 有可用装备");
        case EPbEquipmentSlotState::EquipmentSlotState_Slotted: return TEXT("已装备");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbEquipmentSlotWearTypeValid(int32 Val)
{
    return idlepb::EquipmentSlotWearType_IsValid(Val);
}

const TCHAR* GetEPbEquipmentSlotWearTypeDescription(EPbEquipmentSlotWearType Val)
{
    switch (Val)
    {
        case EPbEquipmentSlotWearType::ESWT_Equipment: return TEXT("只允许装备道具");
        case EPbEquipmentSlotWearType::ESWT_Collection: return TEXT("只允许主动古宝");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbAlchemyChanceTypeValid(int32 Val)
{
    return idlepb::AlchemyChanceType_IsValid(Val);
}

const TCHAR* GetEPbAlchemyChanceTypeDescription(EPbAlchemyChanceType Val)
{
    switch (Val)
    {
        case EPbAlchemyChanceType::AlchemyChanceType_Normal: return TEXT("普通-无保底");
        case EPbAlchemyChanceType::AlchemyChanceType_Small: return TEXT("小保底");
        case EPbAlchemyChanceType::AlchemyChanceType_Big: return TEXT("大保底");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPillTypeValid(int32 Val)
{
    return idlepb::PillType_IsValid(Val);
}

const TCHAR* GetEPbPillTypeDescription(EPbPillType Val)
{
    switch (Val)
    {
        case EPbPillType::PillType_None: return TEXT("未知");
        case EPbPillType::PillType_Hp: return TEXT("气血");
        case EPbPillType::PillType_Mp: return TEXT("真元");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCultivationDirectionValid(int32 Val)
{
    return idlepb::CultivationDirection_IsValid(Val);
}

const TCHAR* GetEPbCultivationDirectionDescription(EPbCultivationDirection Val)
{
    switch (Val)
    {
        case EPbCultivationDirection::CD_None: return TEXT("通用");
        case EPbCultivationDirection::CD_Physic: return TEXT("炼体");
        case EPbCultivationDirection::CD_Magic: return TEXT("修法");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbBreakthroughTypeValid(int32 Val)
{
    return idlepb::BreakthroughType_IsValid(Val);
}

const TCHAR* GetEPbBreakthroughTypeDescription(EPbBreakthroughType Val)
{
    switch (Val)
    {
        case EPbBreakthroughType::BT_None: return TEXT("无需突破");
        case EPbBreakthroughType::BT_Layer: return TEXT("瓶颈");
        case EPbBreakthroughType::BT_Stage: return TEXT("破镜");
        case EPbBreakthroughType::BT_Degree: return TEXT("渡劫");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbWorldTypeValid(int32 Val)
{
    return idlepb::WorldType_IsValid(Val);
}

const TCHAR* GetEPbWorldTypeDescription(EPbWorldType Val)
{
    switch (Val)
    {
        case EPbWorldType::WT_None: return TEXT("未知");
        case EPbWorldType::WT_ClientOnly: return TEXT("客户端专用");
        case EPbWorldType::WT_Arena: return TEXT("秘境地图");
        case EPbWorldType::WT_Door: return TEXT("传送门地图");
        case EPbWorldType::WT_MonsterTower: return TEXT("镇妖塔地图");
        case EPbWorldType::WT_SoloArena: return TEXT("切磋地图");
        case EPbWorldType::WT_SeptArena: return TEXT("中立秘境地图");
        case EPbWorldType::WT_QuestFight: return TEXT("任务对战地图");
        case EPbWorldType::WT_DungeonKillAll: return TEXT("剿灭型副本地图");
        case EPbWorldType::WT_DungeonSurvive: return TEXT("生存型副本地图");
        case EPbWorldType::WT_SeptDemon: return TEXT("镇魔深渊地图");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbEntityTypeValid(int32 Val)
{
    return idlepb::EntityType_IsValid(Val);
}

const TCHAR* GetEPbEntityTypeDescription(EPbEntityType Val)
{
    switch (Val)
    {
        case EPbEntityType::ET_Unknown: return TEXT("未知");
        case EPbEntityType::ET_Player: return TEXT("玩家");
        case EPbEntityType::ET_Npc: return TEXT("NPC");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbEntityStateValid(int32 Val)
{
    return idlepb::EntityState_IsValid(Val);
}

const TCHAR* GetEPbEntityStateDescription(EPbEntityState Val)
{
    switch (Val)
    {
        case EPbEntityState::ES_None: return TEXT("未知");
        case EPbEntityState::ES_Init: return TEXT("初始");
        case EPbEntityState::ES_Normal: return TEXT("正常");
        case EPbEntityState::ES_Death: return TEXT("死亡");
        case EPbEntityState::ES_Recycle: return TEXT("收回");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbNpcTypeValid(int32 Val)
{
    return idlepb::NpcType_IsValid(Val);
}

const TCHAR* GetEPbNpcTypeDescription(EPbNpcType Val)
{
    switch (Val)
    {
        case EPbNpcType::NpcType_None: return TEXT("未知");
        case EPbNpcType::NpcType_Function: return TEXT("功能NPC");
        case EPbNpcType::NpcType_Monster: return TEXT("怪物");
        case EPbNpcType::NpcType_SeptStone: return TEXT("中立秘境矿点");
        case EPbNpcType::NpcType_SeptLand: return TEXT("中立秘境宗门领地");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbMonsterTypeValid(int32 Val)
{
    return idlepb::MonsterType_IsValid(Val);
}

const TCHAR* GetEPbMonsterTypeDescription(EPbMonsterType Val)
{
    switch (Val)
    {
        case EPbMonsterType::MonsterType_None: return TEXT("未知");
        case EPbMonsterType::MonsterType_Normal: return TEXT("普通");
        case EPbMonsterType::MonsterType_Elite: return TEXT("精英");
        case EPbMonsterType::MonsterType_Chief: return TEXT("首领");
        case EPbMonsterType::MonsterType_SuperBoss: return TEXT("神兽");
        case EPbMonsterType::MonsterType_SeptDemon: return TEXT("镇魔深渊Npc");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCurrencyTypeValid(int32 Val)
{
    return idlepb::CurrencyType_IsValid(Val);
}

const TCHAR* GetEPbCurrencyTypeDescription(EPbCurrencyType Val)
{
    switch (Val)
    {
        case EPbCurrencyType::CurrencyType_None: return TEXT("未知类型");
        case EPbCurrencyType::CurrencyType_Money: return TEXT("灵石");
        case EPbCurrencyType::CurrencyType_Soul: return TEXT("天命");
        case EPbCurrencyType::CurrencyType_Gold: return TEXT("机缘");
        case EPbCurrencyType::CurrencyType_Item: return TEXT("天机令");
        case EPbCurrencyType::CurrencyType_AbilityActivePoint: return TEXT("神通要诀");
        case EPbCurrencyType::CurrencyType_AbilityUpgradePoint: return TEXT("神通心得");
        case EPbCurrencyType::CurrencyType_KungfuPoint: return TEXT("功法点");
        case EPbCurrencyType::CurrencyType_TreasureToken: return TEXT("天机石");
        case EPbCurrencyType::CurrencyType_ChaosStone: return TEXT("混沌聚灵石");
        case EPbCurrencyType::CurrencyType_StudyPoint: return TEXT("研习心得");
        case EPbCurrencyType::CurrencyType_NingWenGem: return TEXT("凝纹宝玉");
        case EPbCurrencyType::CurrencyType_HeroCard: return TEXT("英雄帖");
        case EPbCurrencyType::CurrencyType_SeptDonation: return TEXT("宗门贡献");
        case EPbCurrencyType::CurrencyType_SeptStone: return TEXT("宗门玄晶石");
        case EPbCurrencyType::CurrencyType_SeptQuestExp: return TEXT("事务经验");
        case EPbCurrencyType::CurrencyType_SeptQuestToken: return TEXT("事务令");
        case EPbCurrencyType::CurrencyType_GongFaToken: return TEXT("功法要诀");
        case EPbCurrencyType::CurrencyType_GachaTokenL1: return TEXT("探宝令");
        case EPbCurrencyType::CurrencyType_GachaTokenL2: return TEXT("灵域探宝令");
        case EPbCurrencyType::CurrencyType_GachaTokenL3: return TEXT("仙魔令");
        case EPbCurrencyType::CurrencyType_GachaTokenL4: return TEXT("领域探宝令");
        case EPbCurrencyType::CurrencyType_GachaTokenL5: return TEXT("探宝灯");
        case EPbCurrencyType::CurrencyType_TreasuryChest01: return TEXT("流云玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest02: return TEXT("玄光玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest03: return TEXT("上古玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest04: return TEXT("灵兽玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest05: return TEXT("本命玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest06: return TEXT("仙魔玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest07: return TEXT("乾坤玉简道具ID");
        case EPbCurrencyType::CurrencyType_TreasuryChest08: return TEXT("造化玉简道具ID");
        case EPbCurrencyType::CurrencyType_AppearanceMoney: return TEXT("外观商店货币道具ID");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSoloTypeValid(int32 Val)
{
    return idlepb::SoloType_IsValid(Val);
}

const TCHAR* GetEPbSoloTypeDescription(EPbSoloType Val)
{
    switch (Val)
    {
        case EPbSoloType::SoloType_None: return TEXT("未知类型");
        case EPbSoloType::SoloType_FriendlyPk: return TEXT("切磋");
        case EPbSoloType::SoloType_SwordPk: return TEXT("论剑台挑战");
        case EPbSoloType::SoloType_SwordPkRevenge: return TEXT("论剑台复仇");
        case EPbSoloType::SoloType_RobberySeptStone: return TEXT("抢夺中立秘镜矿脉");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbQuestRequirementTypeValid(int32 Val)
{
    return idlepb::QuestRequirementType_IsValid(Val);
}

const TCHAR* GetEPbQuestRequirementTypeDescription(EPbQuestRequirementType Val)
{
    switch (Val)
    {
        case EPbQuestRequirementType::QT_Kill: return TEXT("击杀");
        case EPbQuestRequirementType::QT_Get: return TEXT("获得道具");
        case EPbQuestRequirementType::QT_Submit: return TEXT("提交道具");
        case EPbQuestRequirementType::QT_Rank: return TEXT("等级提升");
        case EPbQuestRequirementType::QT_Event: return TEXT("特殊事件");
        case EPbQuestRequirementType::QT_Alchemy: return TEXT("炼丹");
        case EPbQuestRequirementType::QT_AlchemyRank: return TEXT("丹师等级");
        case EPbQuestRequirementType::QT_SkillRank: return TEXT("神通等级");
        case EPbQuestRequirementType::QT_Forge: return TEXT("炼器");
        case EPbQuestRequirementType::QT_ForgeRank: return TEXT("器室等级");
        case EPbQuestRequirementType::QT_ArenaDoor: return TEXT("秘境传送阵");
        case EPbQuestRequirementType::QT_MonsterTower: return TEXT("镇妖塔");
        case EPbQuestRequirementType::QT_QiCollector: return TEXT("聚灵阵");
        case EPbQuestRequirementType::QT_GongFa: return TEXT("功法");
        case EPbQuestRequirementType::QT_QuestFight: return TEXT("任务对战");
        case EPbQuestRequirementType::QT_SkillDegree: return TEXT("神通品阶");
        case EPbQuestRequirementType::QT_GongFaDegree: return TEXT("功法品阶");
        case EPbQuestRequirementType::QT_CollectorQuality: return TEXT("古宝品质");
        case EPbQuestRequirementType::QT_JoinSept: return TEXT("加入宗门");
        case EPbQuestRequirementType::QT_FarmlandSeed: return TEXT("药园播种");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbQuestOpTypeValid(int32 Val)
{
    return idlepb::QuestOpType_IsValid(Val);
}

const TCHAR* GetEPbQuestOpTypeDescription(EPbQuestOpType Val)
{
    switch (Val)
    {
        case EPbQuestOpType::QOp_Accept: return TEXT("接受");
        case EPbQuestOpType::QOp_Finish: return TEXT("提交");
        case EPbQuestOpType::QOp_GiveUp: return TEXT("放弃");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbQuestSpecialRewardTypeValid(int32 Val)
{
    return idlepb::QuestSpecialRewardType_IsValid(Val);
}

const TCHAR* GetEPbQuestSpecialRewardTypeDescription(EPbQuestSpecialRewardType Val)
{
    switch (Val)
    {
        case EPbQuestSpecialRewardType::QSRT_FarmerFriendShip: return TEXT("药童好感");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbLeaderboardTypeValid(int32 Val)
{
    return idlepb::LeaderboardType_IsValid(Val);
}

const TCHAR* GetEPbLeaderboardTypeDescription(EPbLeaderboardType Val)
{
    switch (Val)
    {
        case EPbLeaderboardType::LBT_Combat: return TEXT("战力榜");
        case EPbLeaderboardType::LBT_Magic: return TEXT("修法榜");
        case EPbLeaderboardType::LBT_Phy: return TEXT("炼体榜");
        case EPbLeaderboardType::LBT_Rich: return TEXT("财富榜");
        case EPbLeaderboardType::LBT_Pet: return TEXT("灵兽榜");
        case EPbLeaderboardType::LBT_Sect: return TEXT("宗门榜");
        case EPbLeaderboardType::LBT_Weapon: return TEXT("武器榜");
        case EPbLeaderboardType::LBT_Armor: return TEXT("防具榜");
        case EPbLeaderboardType::LBT_Jewelry: return TEXT("饰品榜");
        case EPbLeaderboardType::LBT_Treasure: return TEXT("法宝榜");
        case EPbLeaderboardType::LBT_Shanhetu: return TEXT("山河图开包榜");
        case EPbLeaderboardType::LBT_Shanhetu_Week: return TEXT("山河图开周榜");
        case EPbLeaderboardType::LBT_MonsterTower: return TEXT("镇妖塔");
        case EPbLeaderboardType::LBT_MainExp: return TEXT("修为榜");
        case EPbLeaderboardType::LBT_MaxNum: return TEXT("最大种类");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbAlchemyStateValid(int32 Val)
{
    return idlepb::AlchemyState_IsValid(Val);
}

const TCHAR* GetEPbAlchemyStateDescription(EPbAlchemyState Val)
{
    switch (Val)
    {
        case EPbAlchemyState::AlchemyState_None: return TEXT("未开始");
        case EPbAlchemyState::AlchemyState_Running: return TEXT("进行中");
        case EPbAlchemyState::AlchemyState_Done: return TEXT("已经完成（待领取）");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbWorldStateValid(int32 Val)
{
    return idlepb::WorldState_IsValid(Val);
}

const TCHAR* GetEPbWorldStateDescription(EPbWorldState Val)
{
    switch (Val)
    {
        case EPbWorldState::WS_None: return TEXT("未知");
        case EPbWorldState::WS_Init: return TEXT("初始");
        case EPbWorldState::WS_Normal: return TEXT("正常");
        case EPbWorldState::WS_Closing: return TEXT("关闭中");
        case EPbWorldState::WS_Recycle: return TEXT("收回");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbGameStatsModuleTypeValid(int32 Val)
{
    return idlepb::GameStatsModuleType_IsValid(Val);
}

const TCHAR* GetEPbGameStatsModuleTypeDescription(EPbGameStatsModuleType Val)
{
    switch (Val)
    {
        case EPbGameStatsModuleType::GSMT_Summary: return TEXT("全汇总");
        case EPbGameStatsModuleType::GSMT_Base: return TEXT("基础属性汇总");
        case EPbGameStatsModuleType::GSMT_CollectionBlue: return TEXT("古宝汇总 - 普通");
        case EPbGameStatsModuleType::GSMT_CollectionPurple: return TEXT("古宝汇总 - 珍宝");
        case EPbGameStatsModuleType::GSMT_CollectionOrange: return TEXT("古宝汇总 - 灵宝");
        case EPbGameStatsModuleType::GSMT_CollectionRed: return TEXT("古宝汇总 - 至宝");
        case EPbGameStatsModuleType::GSMT_CollectionSuitBlue: return TEXT("古宝套装汇总 - 普通");
        case EPbGameStatsModuleType::GSMT_CollectionSuitPurple: return TEXT("古宝套装汇总 - 珍宝");
        case EPbGameStatsModuleType::GSMT_CollectionSuitOrange: return TEXT("古宝套装汇总 - 灵宝");
        case EPbGameStatsModuleType::GSMT_CollectionSuitRed: return TEXT("古宝套装汇总 - 至宝");
        case EPbGameStatsModuleType::GSMT_EquipmentBase: return TEXT("装备道具 - 基础属性");
        case EPbGameStatsModuleType::GSMT_EquipmentReinforce: return TEXT("装备道具 - 强化属性");
        case EPbGameStatsModuleType::GSMT_EquipmentQiWen: return TEXT("装备道具 - 器纹属性");
        case EPbGameStatsModuleType::GSMT_EquipmentRefine: return TEXT("装备道具 - 精炼属性");
        case EPbGameStatsModuleType::GSMT_Debug: return TEXT("调试");
        case EPbGameStatsModuleType::GSMT_RankBreakthrough: return TEXT("境界突破");
        case EPbGameStatsModuleType::GSMT_RankPractice: return TEXT("境界修炼");
        case EPbGameStatsModuleType::GSMT_Collection: return TEXT("古宝");
        case EPbGameStatsModuleType::GSMT_CollectionSuit: return TEXT("古宝套装");
        case EPbGameStatsModuleType::GSMT_PillElixir: return TEXT("秘药");
        case EPbGameStatsModuleType::GSMT_PillProperty: return TEXT("丹药");
        case EPbGameStatsModuleType::GSMT_GongFa: return TEXT("功法");
        case EPbGameStatsModuleType::GSMT_QiCollector: return TEXT("聚灵阵");
        case EPbGameStatsModuleType::GSMT_Sept: return TEXT("宗门");
        case EPbGameStatsModuleType::GSMT_Equipment: return TEXT("装备");
        case EPbGameStatsModuleType::GSMT_Ability: return TEXT("技能");
        case EPbGameStatsModuleType::GSMT_EquipmentReinforceResonance: return TEXT("装备道具 - 祭炼共鸣属性");
        case EPbGameStatsModuleType::GSMT_EquipmentQiWenResonance: return TEXT("装备道具 - 器纹共鸣属性");
        case EPbGameStatsModuleType::GSMT_Leaderboard: return TEXT("来自排行榜的属性增益（福泽）");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbItemStatsModuleTypeValid(int32 Val)
{
    return idlepb::ItemStatsModuleType_IsValid(Val);
}

const TCHAR* GetEPbItemStatsModuleTypeDescription(EPbItemStatsModuleType Val)
{
    switch (Val)
    {
        case EPbItemStatsModuleType::ISMT_Equipment: return TEXT("装备属性");
        case EPbItemStatsModuleType::ISMT_Reinforce: return TEXT("强化");
        case EPbItemStatsModuleType::ISMT_QiWen: return TEXT("器纹");
        case EPbItemStatsModuleType::ISMT_Refine: return TEXT("精炼");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFunctionModuleStateValid(int32 Val)
{
    return idlepb::FunctionModuleState_IsValid(Val);
}

const TCHAR* GetEPbFunctionModuleStateDescription(EPbFunctionModuleState Val)
{
    switch (Val)
    {
        case EPbFunctionModuleState::FMS_Hide: return TEXT("隐藏，未达到显示等级");
        case EPbFunctionModuleState::FMS_Show: return TEXT("显示，未达到解锁等级");
        case EPbFunctionModuleState::FMS_CanUnlock: return TEXT("显示，已达解锁等级还未解锁");
        case EPbFunctionModuleState::FMS_Unlocked: return TEXT("已解锁");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFunctionModuleTypeValid(int32 Val)
{
    return idlepb::FunctionModuleType_IsValid(Val);
}

const TCHAR* GetEPbFunctionModuleTypeDescription(EPbFunctionModuleType Val)
{
    switch (Val)
    {
        case EPbFunctionModuleType::FMT_None: return TEXT("未知");
        case EPbFunctionModuleType::FMT_Alchemy: return TEXT("丹房 (炼丹)");
        case EPbFunctionModuleType::FMT_Shop: return TEXT("坊市");
        case EPbFunctionModuleType::FMT_DeluxShop: return TEXT("天机阁");
        case EPbFunctionModuleType::FMT_Leaderboard: return TEXT("排行榜");
        case EPbFunctionModuleType::FMT_Mail: return TEXT("邮件");
        case EPbFunctionModuleType::FMT_Forge: return TEXT("器室（炼器）");
        case EPbFunctionModuleType::FMT_MonsterTower: return TEXT("镇妖塔");
        case EPbFunctionModuleType::FMT_PillElixir: return TEXT("秘药");
        case EPbFunctionModuleType::FMT_Ability: return TEXT("神通");
        case EPbFunctionModuleType::FMT_GuBao: return TEXT("古宝");
        case EPbFunctionModuleType::FMT_GongFa: return TEXT("功法");
        case EPbFunctionModuleType::FMT_Share: return TEXT("分享");
        case EPbFunctionModuleType::FMT_HuangZhuang: return TEXT("换装");
        case EPbFunctionModuleType::FMT_ZhuangPei: return TEXT("装配");
        case EPbFunctionModuleType::FMT_QiCollector: return TEXT("聚灵阵");
        case EPbFunctionModuleType::FMT_Checklist: return TEXT("福缘");
        case EPbFunctionModuleType::FMT_SwordPk: return TEXT("论剑台");
        case EPbFunctionModuleType::FMT_TreasuryChest: return TEXT("宝藏阁");
        case EPbFunctionModuleType::FMT_Appearance: return TEXT("外观");
        case EPbFunctionModuleType::FMT_Farm: return TEXT("药园");
        case EPbFunctionModuleType::FMT_Avatar: return TEXT("化身");
        case EPbFunctionModuleType::FMT_Biography: return TEXT("传记");
        case EPbFunctionModuleType::FMT_BiographyEvent: return TEXT("史记");
        case EPbFunctionModuleType::FMT_VipShop: return TEXT("仙阁商店");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFunctionModuleUnlockTypeValid(int32 Val)
{
    return idlepb::FunctionModuleUnlockType_IsValid(Val);
}

const TCHAR* GetEPbFunctionModuleUnlockTypeDescription(EPbFunctionModuleUnlockType Val)
{
    switch (Val)
    {
        case EPbFunctionModuleUnlockType::FMUT_Click: return TEXT("需要玩家点击解锁");
        case EPbFunctionModuleUnlockType::FMUT_Auto: return TEXT("到级别自动解锁");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbMailTypeValid(int32 Val)
{
    return idlepb::MailType_IsValid(Val);
}

const TCHAR* GetEPbMailTypeDescription(EPbMailType Val)
{
    switch (Val)
    {
        case EPbMailType::MT_System: return TEXT("系统公告");
        case EPbMailType::MT_Maintenance: return TEXT("维护公告");
        case EPbMailType::MT_Official: return TEXT("天道（官方）公告");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbMailOperationValid(int32 Val)
{
    return idlepb::MailOperation_IsValid(Val);
}

const TCHAR* GetEPbMailOperationDescription(EPbMailOperation Val)
{
    switch (Val)
    {
        case EPbMailOperation::MOP_Fail: return TEXT("失败");
        case EPbMailOperation::MOP_Done: return TEXT("完成");
        case EPbMailOperation::MOP_InventoryIsFull: return TEXT("背包满");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbChatMessageTypeValid(int32 Val)
{
    return idlepb::ChatMessageType_IsValid(Val);
}

const TCHAR* GetEPbChatMessageTypeDescription(EPbChatMessageType Val)
{
    switch (Val)
    {
        case EPbChatMessageType::CMT_Normal: return TEXT("普通");
        case EPbChatMessageType::CMT_SystemNotice: return TEXT("公告");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbChatMessageChannelValid(int32 Val)
{
    return idlepb::ChatMessageChannel_IsValid(Val);
}

const TCHAR* GetEPbChatMessageChannelDescription(EPbChatMessageChannel Val)
{
    switch (Val)
    {
        case EPbChatMessageChannel::CMC_ColonyServers: return TEXT("大区（万仙）");
        case EPbChatMessageChannel::CMC_QuadServers: return TEXT("小区（异界）");
        case EPbChatMessageChannel::CMC_LocalServer: return TEXT("本区");
        case EPbChatMessageChannel::CMC_Organization: return TEXT("宗门");
        case EPbChatMessageChannel::CMC_Private: return TEXT("私聊");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbForgeChanceTypeValid(int32 Val)
{
    return idlepb::ForgeChanceType_IsValid(Val);
}

const TCHAR* GetEPbForgeChanceTypeDescription(EPbForgeChanceType Val)
{
    switch (Val)
    {
        case EPbForgeChanceType::ForgeChanceType_Normal: return TEXT("普通-无保底");
        case EPbForgeChanceType::ForgeChanceType_Small: return TEXT("小保底");
        case EPbForgeChanceType::ForgeChanceType_Big: return TEXT("大保底");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbForgeStateValid(int32 Val)
{
    return idlepb::ForgeState_IsValid(Val);
}

const TCHAR* GetEPbForgeStateDescription(EPbForgeState Val)
{
    switch (Val)
    {
        case EPbForgeState::ForgeState_None: return TEXT("未开始");
        case EPbForgeState::ForgeState_Running: return TEXT("进行中");
        case EPbForgeState::ForgeState_Done: return TEXT("已经完成（待领取）");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPillElixirTypeValid(int32 Val)
{
    return idlepb::PillElixirType_IsValid(Val);
}

const TCHAR* GetEPbPillElixirTypeDescription(EPbPillElixirType Val)
{
    switch (Val)
    {
        case EPbPillElixirType::PillElixirType_Other: return TEXT("其它");
        case EPbPillElixirType::PillElixirType_Exp: return TEXT("修为秘药");
        case EPbPillElixirType::PillElixirType_Property: return TEXT("属性秘药");
        case EPbPillElixirType::PillElixirType_Double: return TEXT("双修秘药");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPillElixrEffectTypeValid(int32 Val)
{
    return idlepb::PillElixrEffectType_IsValid(Val);
}

const TCHAR* GetEPbPillElixrEffectTypeDescription(EPbPillElixrEffectType Val)
{
    switch (Val)
    {
        case EPbPillElixrEffectType::PillElixrEffectType_None: return TEXT("其它");
        case EPbPillElixrEffectType::PillElixrEffectType_PhyExp: return TEXT("炼体修为");
        case EPbPillElixrEffectType::PillElixrEffectType_MagicExp: return TEXT("练法修为");
        case EPbPillElixrEffectType::PillElixrEffectType_CriticalResist: return TEXT("会心抗性");
        case EPbPillElixrEffectType::PillElixrEffectType_Critical: return TEXT("会心");
        case EPbPillElixrEffectType::PillElixrEffectType_MagicDodge: return TEXT("法术闪避");
        case EPbPillElixrEffectType::PillElixrEffectType_MagicAccuraccy: return TEXT("法术命中");
        case EPbPillElixrEffectType::PillElixrEffectType_PhyDodge: return TEXT("物理闪避");
        case EPbPillElixrEffectType::PillElixrEffectType_PhyAccuraccy: return TEXT("物理命中");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPlayerAbilityActionResultValid(int32 Val)
{
    return idlepb::PlayerAbilityActionResult_IsValid(Val);
}

const TCHAR* GetEPbPlayerAbilityActionResultDescription(EPbPlayerAbilityActionResult Val)
{
    switch (Val)
    {
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_Success: return TEXT("成功");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_Timeout: return TEXT("超时");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_InvalidAbility: return TEXT("无效神通ID");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_GiveFailed_ExistAlready: return TEXT("习得神通-神通已经存在了");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_GiveFailed_OwnerFailed: return TEXT("习得神通-Owner条件不满足");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_UpgradeFailed_NonExist: return TEXT("升级失败-神通不存在");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_UpgradeFailed_MaxGrade: return TEXT("升级失败-已到达最大值");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_UpgradeFailed_OwnerFailed: return TEXT("升级失败-Owner条件不满足");
        case EPbPlayerAbilityActionResult::PlayerAbilityAction_ResetAll_CoolDown: return TEXT("一键重置神通冷却中");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFightModeValid(int32 Val)
{
    return idlepb::FightMode_IsValid(Val);
}

const TCHAR* GetEPbFightModeDescription(EPbFightMode Val)
{
    switch (Val)
    {
        case EPbFightMode::FightMode_Peace: return TEXT("和平 (无法攻击其他修士，无法被其他修士攻击)");
        case EPbFightMode::FightMode_All: return TEXT("全体 (可以攻击其他全体模式下的玩家)");
        case EPbFightMode::FightMode_Sept: return TEXT("宗门");
        case EPbFightMode::FightMode_Neutral: return TEXT("中立");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbAttackLockTypeValid(int32 Val)
{
    return idlepb::AttackLockType_IsValid(Val);
}

const TCHAR* GetEPbAttackLockTypeDescription(EPbAttackLockType Val)
{
    switch (Val)
    {
        case EPbAttackLockType::AttackLockType_MinDistance: return TEXT("最近距离");
        case EPbAttackLockType::AttackLockType_LowHpPct: return TEXT("剩余血量比例最低");
        case EPbAttackLockType::AttackLockType_LowHpMpValue: return TEXT("气血真元绝对值最低");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbAttackUnlockTypeValid(int32 Val)
{
    return idlepb::AttackUnlockType_IsValid(Val);
}

const TCHAR* GetEPbAttackUnlockTypeDescription(EPbAttackUnlockType Val)
{
    switch (Val)
    {
        case EPbAttackUnlockType::AttackUnlockType_MaxDistance: return TEXT("远距离");
        case EPbAttackUnlockType::AttackUnlockType_NearDistance: return TEXT("近距离");
        case EPbAttackUnlockType::AttackUnlockType_Screen: return TEXT("脱离屏幕");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbBossInvasionStateValid(int32 Val)
{
    return idlepb::BossInvasionState_IsValid(Val);
}

const TCHAR* GetEPbBossInvasionStateDescription(EPbBossInvasionState Val)
{
    switch (Val)
    {
        case EPbBossInvasionState::BossInvasionState_None: return TEXT("未知");
        case EPbBossInvasionState::BossInvasionState_Wait: return TEXT("等待开始");
        case EPbBossInvasionState::BossInvasionState_Prepare: return TEXT("准备阶段");
        case EPbBossInvasionState::BossInvasionState_Fight: return TEXT("战斗阶段");
        case EPbBossInvasionState::BossInvasionState_End: return TEXT("战斗结束");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSwordPkStateValid(int32 Val)
{
    return idlepb::SwordPkState_IsValid(Val);
}

const TCHAR* GetEPbSwordPkStateDescription(EPbSwordPkState Val)
{
    switch (Val)
    {
        case EPbSwordPkState::SwordPkState_None: return TEXT("未知");
        case EPbSwordPkState::SwordPkState_Init: return TEXT("初始化");
        case EPbSwordPkState::SwordPkState_SeasonBegin: return TEXT("赛季开始");
        case EPbSwordPkState::SwordPkState_SeasonDuring: return TEXT("赛季进行中");
        case EPbSwordPkState::SwordPkState_SeasonEnd: return TEXT("赛季结束");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCheckListTaskTypeValid(int32 Val)
{
    return idlepb::CheckListTaskType_IsValid(Val);
}

const TCHAR* GetEPbCheckListTaskTypeDescription(EPbCheckListTaskType Val)
{
    switch (Val)
    {
        case EPbCheckListTaskType::CLTT_None: return TEXT("");
        case EPbCheckListTaskType::CLTT_Login: return TEXT("登录游戏");
        case EPbCheckListTaskType::CLTT_EatPill: return TEXT("服用道行丹");
        case EPbCheckListTaskType::CLTT_Practice: return TEXT("吐纳");
        case EPbCheckListTaskType::CLTT_BuyInMarket: return TEXT("坊市购买");
        case EPbCheckListTaskType::CLTT_MakePill: return TEXT("炼丹");
        case EPbCheckListTaskType::CLTT_GongFa: return TEXT("修炼功法");
        case EPbCheckListTaskType::CLTT_MonsterTower: return TEXT("挑战镇妖塔");
        case EPbCheckListTaskType::CLTT_Portal: return TEXT("挑战秘境传送阵");
        case EPbCheckListTaskType::CLTT_Plant: return TEXT("药园种植");
        case EPbCheckListTaskType::CLTT_Forge: return TEXT("锻造装备");
        case EPbCheckListTaskType::CLTT_Ad: return TEXT("观影");
        case EPbCheckListTaskType::CLTT_Ability: return TEXT("升级神通");
        case EPbCheckListTaskType::CLTT_LocalPk: return TEXT("论剑台");
        case EPbCheckListTaskType::CLTT_Bounty: return TEXT("完成悬赏令");
        case EPbCheckListTaskType::CLTT_Reinforce: return TEXT("祭炼装备");
        case EPbCheckListTaskType::CLTT_CommonPk: return TEXT("跨界论道");
        case EPbCheckListTaskType::CLTT_BuyInDeluxShop: return TEXT("天机阁购买");
        case EPbCheckListTaskType::CLTT_MonsterInvasion: return TEXT("神兽入侵");
        case EPbCheckListTaskType::CLTT_WorldInvasion: return TEXT("异界入侵");
        case EPbCheckListTaskType::CLTT_Mine: return TEXT("灵脉采集");
        case EPbCheckListTaskType::CLTT_SectContribution: return TEXT("宗门贡献点");
        case EPbCheckListTaskType::CLTT_Quiz: return TEXT("八荒竞猜");
        case EPbCheckListTaskType::CLTT_Share: return TEXT("分享");
        case EPbCheckListTaskType::CLTT_MineTime: return TEXT("灵脉采集时长");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSeptPositionValid(int32 Val)
{
    return idlepb::SeptPosition_IsValid(Val);
}

const TCHAR* GetEPbSeptPositionDescription(EPbSeptPosition Val)
{
    switch (Val)
    {
        case EPbSeptPosition::SeptPosition_None: return TEXT("未加入");
        case EPbSeptPosition::SeptPosition_Normal: return TEXT("成员");
        case EPbSeptPosition::SeptPosition_Manager: return TEXT("主事");
        case EPbSeptPosition::SeptPosition_Senator: return TEXT("长老");
        case EPbSeptPosition::SeptPosition_ViceChairman: return TEXT("副宗主");
        case EPbSeptPosition::SeptPosition_Chairman: return TEXT("宗主");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSeptLogTypeValid(int32 Val)
{
    return idlepb::SeptLogType_IsValid(Val);
}

const TCHAR* GetEPbSeptLogTypeDescription(EPbSeptLogType Val)
{
    switch (Val)
    {
        case EPbSeptLogType::SLT_None: return TEXT("未知类型");
        case EPbSeptLogType::SLT_Join: return TEXT("加入宗门");
        case EPbSeptLogType::SLT_Leave: return TEXT("离开宗门");
        case EPbSeptLogType::SLT_ChangeUp: return TEXT("升职");
        case EPbSeptLogType::SLT_ChangeDown: return TEXT("降职");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSeptArenaStoneTypeValid(int32 Val)
{
    return idlepb::SeptArenaStoneType_IsValid(Val);
}

const TCHAR* GetEPbSeptArenaStoneTypeDescription(EPbSeptArenaStoneType Val)
{
    switch (Val)
    {
        case EPbSeptArenaStoneType::SeptArenaStoneType_None: return TEXT("未知类型");
        case EPbSeptArenaStoneType::SeptArenaStoneType_Primary: return TEXT("初级");
        case EPbSeptArenaStoneType::SeptArenaStoneType_Intermediate: return TEXT("中级");
        case EPbSeptArenaStoneType::SeptArenaStoneType_Advanced: return TEXT("高级");
        case EPbSeptArenaStoneType::SeptArenaStoneType_Top: return TEXT("顶级");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbGongFaStateValid(int32 Val)
{
    return idlepb::GongFaState_IsValid(Val);
}

const TCHAR* GetEPbGongFaStateDescription(EPbGongFaState Val)
{
    switch (Val)
    {
        case EPbGongFaState::Unable: return TEXT("未达到领悟条件");
        case EPbGongFaState::ReadyToLearn: return TEXT("待领悟");
        case EPbGongFaState::Training: return TEXT("领悟中");
        case EPbGongFaState::Standby: return TEXT("待激活");
        case EPbGongFaState::Activated: return TEXT("已经激活");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbGongFaConditionValid(int32 Val)
{
    return idlepb::GongFaCondition_IsValid(Val);
}

const TCHAR* GetEPbGongFaConditionDescription(EPbGongFaCondition Val)
{
    switch (Val)
    {
        case EPbGongFaCondition::GFC_None: return TEXT("无条件");
        case EPbGongFaCondition::GFC_HerbalBlock: return TEXT("解锁药园土地的数量");
        case EPbGongFaCondition::GFC_CostItem: return TEXT("生涯消耗道具数量");
        case EPbGongFaCondition::GFC_GuBao: return TEXT("持有古宝且星级达到n");
        case EPbGongFaCondition::GFC_AvatarLevel: return TEXT("分身等级达到n");
        case EPbGongFaCondition::GFC_OrangeAlchemy: return TEXT("炼制橙色丹药");
        case EPbGongFaCondition::GFC_OrangeForge: return TEXT("炼制橙色装备");
        case EPbGongFaCondition::GFC_MaxHP: return TEXT("最大气血");
        case EPbGongFaCondition::GFC_MaxMP: return TEXT("最大真元");
        case EPbGongFaCondition::GFC_PetLevel: return TEXT("拥有灵兽且等级达到n");
        case EPbGongFaCondition::GFC_SkillLevel: return TEXT("对应神通等级达到n");
        case EPbGongFaCondition::GFC_ImmortalRoad: return TEXT("完成全部飞升之路");
        case EPbGongFaCondition::GFC_Crit: return TEXT("会心");
        case EPbGongFaCondition::GFC_CritCoff: return TEXT("会心倍率");
        case EPbGongFaCondition::GFC_MagBreak: return TEXT("法术破防");
        case EPbGongFaCondition::GFC_PhyBreak: return TEXT("物理破防");
        case EPbGongFaCondition::GFC_MagBlock: return TEXT("法术格挡");
        case EPbGongFaCondition::GFC_PhyBlock: return TEXT("物理格挡");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbGongFaEffectTypeValid(int32 Val)
{
    return idlepb::GongFaEffectType_IsValid(Val);
}

const TCHAR* GetEPbGongFaEffectTypeDescription(EPbGongFaEffectType Val)
{
    switch (Val)
    {
        case EPbGongFaEffectType::GFET_None: return TEXT("无条件");
        case EPbGongFaEffectType::GFET_BaseAtt: return TEXT("功法全属性提升");
        case EPbGongFaEffectType::GFET_DongFu: return TEXT("洞府灵气");
        case EPbGongFaEffectType::GFET_PracticeNum: return TEXT("吐纳次数");
        case EPbGongFaEffectType::GFET_PracticeUp: return TEXT("吐纳效果");
        case EPbGongFaEffectType::GFET_MergePractive: return TEXT("将所有吐纳合一次完成");
        case EPbGongFaEffectType::GFET_PillUp: return TEXT("修为丹药服用效果");
        case EPbGongFaEffectType::GFET_PillUseNum: return TEXT("每日修为丹药服用次数提升");
        case EPbGongFaEffectType::GFET_MoneyUp: return TEXT("秘境灵石产出");
        case EPbGongFaEffectType::GFET_AttackMonsterDamageAddPercent: return TEXT("攻击怪物伤害加成");
        case EPbGongFaEffectType::GFET_TakeMonsterDamageReducePercent: return TEXT("受怪物伤害减免");
        case EPbGongFaEffectType::GFET_FaBaoDamageToPlayerAddPercent: return TEXT("攻击修士法宝伤害");
        case EPbGongFaEffectType::GFET_FaBaoDamageToPlayerReducePercent: return TEXT("受到法宝伤害减免");
        case EPbGongFaEffectType::GFET_ShenTongDamageToPlayerAddPercent: return TEXT("攻击修士神通伤害");
        case EPbGongFaEffectType::GFET_ShenTongDamageToPlayerReducePercent: return TEXT("受到神通伤害减免");
        case EPbGongFaEffectType::GFET_Mind: return TEXT("神识");
        case EPbGongFaEffectType::GFET_MpRecoverPercent: return TEXT("真元回复");
        case EPbGongFaEffectType::GFET_Mp: return TEXT("真元");
        case EPbGongFaEffectType::GFET_BaseMp: return TEXT("基础真元");
        case EPbGongFaEffectType::GFET_HpRecoverPercent: return TEXT("气血回复");
        case EPbGongFaEffectType::GFET_Hp: return TEXT("气血");
        case EPbGongFaEffectType::GFET_BaseHp: return TEXT("基础气血");
        case EPbGongFaEffectType::GFET_MagAtt: return TEXT("法术攻击");
        case EPbGongFaEffectType::GFET_PhyAtt: return TEXT("物理攻击");
        case EPbGongFaEffectType::GFET_Crit: return TEXT("会心");
        case EPbGongFaEffectType::GFET_CritCoeff: return TEXT("会心倍率");
        case EPbGongFaEffectType::GFET_CritBlock: return TEXT("会心格挡");
        case EPbGongFaEffectType::GFET_ControlRateAtt: return TEXT("控制概率强化");
        case EPbGongFaEffectType::GFET_ControlRateDef: return TEXT("控制概率抗性");
        case EPbGongFaEffectType::GFET_ControlTimeAtt: return TEXT("控制时间强化");
        case EPbGongFaEffectType::GFET_ControlTimeDef: return TEXT("控制时间抗性");
        case EPbGongFaEffectType::GFET_MoveSpeed: return TEXT("移动速度");
        case EPbGongFaEffectType::GFET_Intellect: return TEXT("内息");
        case EPbGongFaEffectType::GFET_Strength: return TEXT("体魄");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFuZengTypeValid(int32 Val)
{
    return idlepb::FuZengType_IsValid(Val);
}

const TCHAR* GetEPbFuZengTypeDescription(EPbFuZengType Val)
{
    switch (Val)
    {
        case EPbFuZengType::FZT_None: return TEXT("无");
        case EPbFuZengType::FZT_CombatPower: return TEXT("战力");
        case EPbFuZengType::FZT_MonsterTower: return TEXT("镇妖塔");
        case EPbFuZengType::FZT_Ability: return TEXT("神通");
        case EPbFuZengType::FZT_Pill: return TEXT("秘药");
        case EPbFuZengType::FZT_GongFa: return TEXT("功法");
        case EPbFuZengType::FZT_GuBaoCollection: return TEXT("古宝收集");
        case EPbFuZengType::FZT_GuBaoLevel: return TEXT("古宝星级");
        case EPbFuZengType::FZT_GaCha: return TEXT("外域探宝");
        case EPbFuZengType::FZT_MaxNum: return TEXT("最大值");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFuZengStateValid(int32 Val)
{
    return idlepb::FuZengState_IsValid(Val);
}

const TCHAR* GetEPbFuZengStateDescription(EPbFuZengState Val)
{
    switch (Val)
    {
        case EPbFuZengState::FZS_UnFinished: return TEXT("未完成");
        case EPbFuZengState::FZS_Finished: return TEXT("已完成待领取");
        case EPbFuZengState::FZS_Received: return TEXT("已领取");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionStarAdditionTypeValid(int32 Val)
{
    return idlepb::CollectionStarAdditionType_IsValid(Val);
}

const TCHAR* GetEPbCollectionStarAdditionTypeDescription(EPbCollectionStarAdditionType Val)
{
    switch (Val)
    {
        case EPbCollectionStarAdditionType::CSAT_Times: return TEXT("乘");
        case EPbCollectionStarAdditionType::CSAT_Plus: return TEXT("加");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionUseTypeValid(int32 Val)
{
    return idlepb::CollectionUseType_IsValid(Val);
}

const TCHAR* GetEPbCollectionUseTypeDescription(EPbCollectionUseType Val)
{
    switch (Val)
    {
        case EPbCollectionUseType::CUT_Active: return TEXT("主动");
        case EPbCollectionUseType::CUT_Passive: return TEXT("被动");
        case EPbCollectionUseType::CUT_Functional: return TEXT("功能");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionZoneTypeValid(int32 Val)
{
    return idlepb::CollectionZoneType_IsValid(Val);
}

const TCHAR* GetEPbCollectionZoneTypeDescription(EPbCollectionZoneType Val)
{
    switch (Val)
    {
        case EPbCollectionZoneType::CZT_Unknown: return TEXT("未定义");
        case EPbCollectionZoneType::CZT_RenJie: return TEXT("人界");
        case EPbCollectionZoneType::CZT_LingJie: return TEXT("灵界");
        case EPbCollectionZoneType::CZT_XianMo: return TEXT("仙魔");
        case EPbCollectionZoneType::CZT_LiuYu: return TEXT("领域");
        case EPbCollectionZoneType::CZT_QiTa: return TEXT("其他");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionFilterTypeValid(int32 Val)
{
    return idlepb::CollectionFilterType_IsValid(Val);
}

const TCHAR* GetEPbCollectionFilterTypeDescription(EPbCollectionFilterType Val)
{
    switch (Val)
    {
        case EPbCollectionFilterType::CFT_Owned: return TEXT("已拥有");
        case EPbCollectionFilterType::CFT_All: return TEXT("图鉴");
        case EPbCollectionFilterType::CFT_Source: return TEXT("渊源");
        case EPbCollectionFilterType::CFT_Suit: return TEXT("套装");
        case EPbCollectionFilterType::CFT_CanUp: return TEXT("可升星");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionOrderTypeValid(int32 Val)
{
    return idlepb::CollectionOrderType_IsValid(Val);
}

const TCHAR* GetEPbCollectionOrderTypeDescription(EPbCollectionOrderType Val)
{
    switch (Val)
    {
        case EPbCollectionOrderType::COT_Quality: return TEXT("品质");
        case EPbCollectionOrderType::COT_Property: return TEXT("属性");
        case EPbCollectionOrderType::COT_Level: return TEXT("注灵");
        case EPbCollectionOrderType::COT_Active: return TEXT("活动");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionHistoryStateValid(int32 Val)
{
    return idlepb::CollectionHistoryState_IsValid(Val);
}

const TCHAR* GetEPbCollectionHistoryStateDescription(EPbCollectionHistoryState Val)
{
    switch (Val)
    {
        case EPbCollectionHistoryState::CHS_None: return TEXT("没有渊源");
        case EPbCollectionHistoryState::CHS_NotActive: return TEXT("还未激活");
        case EPbCollectionHistoryState::CHS_CanDraw: return TEXT("可以领取");
        case EPbCollectionHistoryState::CHS_DrawDone: return TEXT("已领取");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbCollectionZoneActiveAwardStateValid(int32 Val)
{
    return idlepb::CollectionZoneActiveAwardState_IsValid(Val);
}

const TCHAR* GetEPbCollectionZoneActiveAwardStateDescription(EPbCollectionZoneActiveAwardState Val)
{
    switch (Val)
    {
        case EPbCollectionZoneActiveAwardState::CZAAS_None: return TEXT("无法领取");
        case EPbCollectionZoneActiveAwardState::CZAAS_CanDraw: return TEXT("可以领取");
        case EPbCollectionZoneActiveAwardState::CZAAS_DrawDone: return TEXT("已经领取");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbLifeCounterTypeValid(int32 Val)
{
    return idlepb::LifeCounterType_IsValid(Val);
}

const TCHAR* GetEPbLifeCounterTypeDescription(EPbLifeCounterType Val)
{
    switch (Val)
    {
        case EPbLifeCounterType::LCT_Unknown: return TEXT("未知");
        case EPbLifeCounterType::LCT_AlchemyQuality: return TEXT("炼制对应品质丹药");
        case EPbLifeCounterType::LCT_ForgeEquipmentQuality: return TEXT("炼制对应品质装备");
        case EPbLifeCounterType::LCT_ForgeSkillEquipmentQuality: return TEXT("炼制对应品质法宝");
        case EPbLifeCounterType::LCT_AbilityLevelPhy: return TEXT("体修神通升级");
        case EPbLifeCounterType::LCT_AbilityLevelMag: return TEXT("法修神通升级");
        case EPbLifeCounterType::LCT_KillCommonMonster: return TEXT("击杀普通小怪");
        case EPbLifeCounterType::LCT_KillEliteMonster: return TEXT("击杀精英怪");
        case EPbLifeCounterType::LCT_KillBoss: return TEXT("击杀首领");
        case EPbLifeCounterType::LCT_ActiveGongFaPoint: return TEXT("激活功法节点");
        case EPbLifeCounterType::LCT_FinishingGongFa: return TEXT("功法圆满");
        case EPbLifeCounterType::LCT_PillElixirUsed: return TEXT("秘药使用");
        case EPbLifeCounterType::LCT_InventoryItemUsed: return TEXT("背包道具使用按ID");
        case EPbLifeCounterType::LCT_InventoryItemUsedType: return TEXT("背包道具使用按类型");
        case EPbLifeCounterType::LCT_LoginGame: return TEXT("登录游戏每日");
        case EPbLifeCounterType::LCT_FuYuanPoint: return TEXT("福缘活跃度");
        case EPbLifeCounterType::LCT_SeptQuest: return TEXT("完成宗门事务");
        case EPbLifeCounterType::LCT_MonsterTower: return TEXT("镇妖塔层数");
        case EPbLifeCounterType::LCT_CollectionStar: return TEXT("古宝星级");
        case EPbLifeCounterType::LCT_CollectionNum: return TEXT("古宝收集");
        case EPbLifeCounterType::LCT_GaChaOpen: return TEXT("探宝计数");
        case EPbLifeCounterType::LCT_Shanhetu: return TEXT("山河图计数");
        case EPbLifeCounterType::LCT_PropertyPillUse: return TEXT("属性丹药按id计数");
        case EPbLifeCounterType::LCT_FarmBlockNum: return TEXT("药园地块解锁数");
        case EPbLifeCounterType::LCT_AlchemyType: return TEXT("炼制对应类型丹药");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbModelTypeValid(int32 Val)
{
    return idlepb::ModelType_IsValid(Val);
}

const TCHAR* GetEPbModelTypeDescription(EPbModelType Val)
{
    switch (Val)
    {
        case EPbModelType::MT_Unknown: return TEXT("未知");
        case EPbModelType::MT_HeadIcon: return TEXT("头像");
        case EPbModelType::MT_HeadFrame: return TEXT("头像框");
        case EPbModelType::MT_Body: return TEXT("衣服");
        case EPbModelType::MT_Hair: return TEXT("发型");
        case EPbModelType::MT_Hat: return TEXT("头饰");
        case EPbModelType::MT_HeadDeco: return TEXT("额饰");
        case EPbModelType::MT_FaceDeco: return TEXT("面饰");
        case EPbModelType::MT_Mask: return TEXT("面具");
        case EPbModelType::MT_EarDeco: return TEXT("耳饰");
        case EPbModelType::MT_EyeBrows: return TEXT("眉眼");
        case EPbModelType::MT_Makeup: return TEXT("妆容");
        case EPbModelType::MT_FacePaint: return TEXT("面绘");
        case EPbModelType::MT_FacePrint: return TEXT("印容");
        case EPbModelType::MT_BackLight: return TEXT("灵光");
        case EPbModelType::MT_MaxNum: return TEXT("枚举容量");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbInventoryItemChangedTypeValid(int32 Val)
{
    return idlepb::InventoryItemChangedType_IsValid(Val);
}

const TCHAR* GetEPbInventoryItemChangedTypeDescription(EPbInventoryItemChangedType Val)
{
    switch (Val)
    {
        case EPbInventoryItemChangedType::IICT_Modify: return TEXT("修改");
        case EPbInventoryItemChangedType::IICT_Add: return TEXT("新增");
        case EPbInventoryItemChangedType::IICT_Del: return TEXT("删除");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbArenaCheckListStateValid(int32 Val)
{
    return idlepb::ArenaCheckListState_IsValid(Val);
}

const TCHAR* GetEPbArenaCheckListStateDescription(EPbArenaCheckListState Val)
{
    switch (Val)
    {
        case EPbArenaCheckListState::ACLS_None: return TEXT("未知");
        case EPbArenaCheckListState::ACLS_UnFinished: return TEXT("未完成");
        case EPbArenaCheckListState::ACLS_Finished: return TEXT("已完成待领取");
        case EPbArenaCheckListState::ACLS_Received: return TEXT("已领取");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbArenaCheckListRewardStateValid(int32 Val)
{
    return idlepb::ArenaCheckListRewardState_IsValid(Val);
}

const TCHAR* GetEPbArenaCheckListRewardStateDescription(EPbArenaCheckListRewardState Val)
{
    switch (Val)
    {
        case EPbArenaCheckListRewardState::ACLRS_UnFinished: return TEXT("未完成");
        case EPbArenaCheckListRewardState::ACLRS_UnReceived: return TEXT("已完成待领取");
        case EPbArenaCheckListRewardState::ACLRS_Received: return TEXT("已领取");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFarmLandStateValid(int32 Val)
{
    return idlepb::FarmLandState_IsValid(Val);
}

const TCHAR* GetEPbFarmLandStateDescription(EPbFarmLandState Val)
{
    switch (Val)
    {
        case EPbFarmLandState::FLS_None: return TEXT("未解锁");
        case EPbFarmLandState::FLS_Free: return TEXT("所有灵植生长周期减少");
        case EPbFarmLandState::FLS_InUse: return TEXT("正在使用");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbFarmerEffectTypeValid(int32 Val)
{
    return idlepb::FarmerEffectType_IsValid(Val);
}

const TCHAR* GetEPbFarmerEffectTypeDescription(EPbFarmerEffectType Val)
{
    switch (Val)
    {
        case EPbFarmerEffectType::FET_None: return TEXT("未完成");
        case EPbFarmerEffectType::FET_ReduceGrowthTime: return TEXT("所有灵植生长周期减少");
        case EPbFarmerEffectType::FET_RipeningUp: return TEXT("催熟效果增加");
        case EPbFarmerEffectType::FET_WateringUp: return TEXT("浇灌效果增加");
        case EPbFarmerEffectType::FET_ExtraOneHarvest: return TEXT("收获灵植时有几率额外获得1株");
        case EPbFarmerEffectType::FET_WateringNumUp: return TEXT("浇灌次数增加");
        case EPbFarmerEffectType::FET_ReduceGrowthPeriod: return TEXT("所有灵植生长周期减少百分比");
        case EPbFarmerEffectType::FET_MaxNum: return TEXT("最大效果数");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbServerCounterTypeValid(int32 Val)
{
    return idlepb::ServerCounterType_IsValid(Val);
}

const TCHAR* GetEPbServerCounterTypeDescription(EPbServerCounterType Val)
{
    switch (Val)
    {
        case EPbServerCounterType::SCT_None: return TEXT("None");
        case EPbServerCounterType::SCT_BiographyEventKillNpcWithHighDegree: return TEXT("史记-高等级怪物击杀");
        case EPbServerCounterType::SCT_KillNpcWithHighDegree: return TEXT("高等级怪物击杀");
        case EPbServerCounterType::SCT_DegreeUpPhy: return TEXT("玩家炼体升级计数");
        case EPbServerCounterType::SCT_DegreeUpMag: return TEXT("玩家修法升级计数");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPeriodTypeValid(int32 Val)
{
    return idlepb::PeriodType_IsValid(Val);
}

const TCHAR* GetEPbPeriodTypeDescription(EPbPeriodType Val)
{
    switch (Val)
    {
        case EPbPeriodType::PT_None: return TEXT("未知");
        case EPbPeriodType::PT_Day: return TEXT("日");
        case EPbPeriodType::PT_Week: return TEXT("周");
        case EPbPeriodType::PT_Mouth: return TEXT("月");
        case EPbPeriodType::PT_Year: return TEXT("年");
    }
    return TEXT("UNKNOWN");
}