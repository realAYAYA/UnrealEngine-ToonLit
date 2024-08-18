#pragma once

#include "ZPbCommon.h"
#include "ZPbGame.h"
#include "ZPbWorld.h"
#include "ZPbAbility.h"
#include "ZPbRelation.h"
#include "common.pb.h"
#include "game.pb.h"
#include "ability.pb.h"
#include "world.pb.h"
#include "relation.pb.h"

#include "ZTools.h"
#include "MRpcManager.h"

class ZRPC_API FZGameRpcInterface
{
public:

    FZGameRpcInterface(FMRpcManager* InManager);
    virtual ~FZGameRpcInterface();

    const TCHAR* GetName() const { return TEXT("GameRpc"); }  
    
    
    /**
     * 登录游戏
    */
    static constexpr uint64 LoginGame = 0xce6f5c055d80fbc0LL; 
    typedef TSharedPtr<idlezt::LoginGameReq> FZLoginGameReqPtr;
    typedef TSharedPtr<idlezt::LoginGameAck> FZLoginGameRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZLoginGameReqPtr&, const FZLoginGameRspPtr&)> FZLoginGameCallback;
    static void LoginGameRegister(FMRpcManager* InManager, const FZLoginGameCallback& InCallback);
    
    /**
     * 设置修炼方向
    */
    static constexpr uint64 SetCurrentCultivationDirection = 0x4d8bad2991c46375LL; 
    typedef TSharedPtr<idlezt::SetCurrentCultivationDirectionReq> FZSetCurrentCultivationDirectionReqPtr;
    typedef TSharedPtr<idlezt::SetCurrentCultivationDirectionAck> FZSetCurrentCultivationDirectionRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetCurrentCultivationDirectionReqPtr&, const FZSetCurrentCultivationDirectionRspPtr&)> FZSetCurrentCultivationDirectionCallback;
    static void SetCurrentCultivationDirectionRegister(FMRpcManager* InManager, const FZSetCurrentCultivationDirectionCallback& InCallback);
    
    /**
     * 突破
    */
    static constexpr uint64 DoBreakthrough = 0x9793802185c130c0LL; 
    typedef TSharedPtr<idlezt::DoBreakthroughReq> FZDoBreakthroughReqPtr;
    typedef TSharedPtr<idlezt::DoBreakthroughAck> FZDoBreakthroughRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDoBreakthroughReqPtr&, const FZDoBreakthroughRspPtr&)> FZDoBreakthroughCallback;
    static void DoBreakthroughRegister(FMRpcManager* InManager, const FZDoBreakthroughCallback& InCallback);
    
    /**
     * 请求公共修炼数据
    */
    static constexpr uint64 RequestCommonCultivationData = 0x6b72d469375741c9LL; 
    typedef TSharedPtr<idlezt::RequestCommonCultivationDataReq> FZRequestCommonCultivationDataReqPtr;
    typedef TSharedPtr<idlezt::RequestCommonCultivationDataAck> FZRequestCommonCultivationDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestCommonCultivationDataReqPtr&, const FZRequestCommonCultivationDataRspPtr&)> FZRequestCommonCultivationDataCallback;
    static void RequestCommonCultivationDataRegister(FMRpcManager* InManager, const FZRequestCommonCultivationDataCallback& InCallback);
    
    /**
     * 请求合并吐纳
    */
    static constexpr uint64 OneClickMergeBreathing = 0x88abe5f5230d907bLL; 
    typedef TSharedPtr<idlezt::OneClickMergeBreathingReq> FZOneClickMergeBreathingReqPtr;
    typedef TSharedPtr<idlezt::OneClickMergeBreathingAck> FZOneClickMergeBreathingRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickMergeBreathingReqPtr&, const FZOneClickMergeBreathingRspPtr&)> FZOneClickMergeBreathingCallback;
    static void OneClickMergeBreathingRegister(FMRpcManager* InManager, const FZOneClickMergeBreathingCallback& InCallback);
    
    /**
     * 请求领取吐纳奖励
    */
    static constexpr uint64 ReceiveBreathingExerciseReward = 0x45dead7fd8d4e9d1LL; 
    typedef TSharedPtr<idlezt::ReceiveBreathingExerciseRewardReq> FZReceiveBreathingExerciseRewardReqPtr;
    typedef TSharedPtr<idlezt::ReceiveBreathingExerciseRewardAck> FZReceiveBreathingExerciseRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReceiveBreathingExerciseRewardReqPtr&, const FZReceiveBreathingExerciseRewardRspPtr&)> FZReceiveBreathingExerciseRewardCallback;
    static void ReceiveBreathingExerciseRewardRegister(FMRpcManager* InManager, const FZReceiveBreathingExerciseRewardCallback& InCallback);
    
    /**
     * 请求包裹数据
    */
    static constexpr uint64 GetInventoryData = 0x6289e76a7763fa29LL; 
    typedef TSharedPtr<idlezt::GetInventoryDataReq> FZGetInventoryDataReqPtr;
    typedef TSharedPtr<idlezt::GetInventoryDataAck> FZGetInventoryDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetInventoryDataReqPtr&, const FZGetInventoryDataRspPtr&)> FZGetInventoryDataCallback;
    static void GetInventoryDataRegister(FMRpcManager* InManager, const FZGetInventoryDataCallback& InCallback);
    
    /**
     * 请求任务数据
    */
    static constexpr uint64 GetQuestData = 0xdcd8b03182e1e10fLL; 
    typedef TSharedPtr<idlezt::GetQuestDataReq> FZGetQuestDataReqPtr;
    typedef TSharedPtr<idlezt::GetQuestDataAck> FZGetQuestDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetQuestDataReqPtr&, const FZGetQuestDataRspPtr&)> FZGetQuestDataCallback;
    static void GetQuestDataRegister(FMRpcManager* InManager, const FZGetQuestDataCallback& InCallback);
    
    /**
     * 创建角色
    */
    static constexpr uint64 CreateCharacter = 0xa2b3cb26d675f7a8LL; 
    typedef TSharedPtr<idlezt::CreateCharacterReq> FZCreateCharacterReqPtr;
    typedef TSharedPtr<idlezt::CreateCharacterAck> FZCreateCharacterRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZCreateCharacterReqPtr&, const FZCreateCharacterRspPtr&)> FZCreateCharacterCallback;
    static void CreateCharacterRegister(FMRpcManager* InManager, const FZCreateCharacterCallback& InCallback);
    
    /**
     * 使用道具
    */
    static constexpr uint64 UseItem = 0x297bb199fc61c7cbLL; 
    typedef TSharedPtr<idlezt::UseItemReq> FZUseItemReqPtr;
    typedef TSharedPtr<idlezt::UseItemAck> FZUseItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUseItemReqPtr&, const FZUseItemRspPtr&)> FZUseItemCallback;
    static void UseItemRegister(FMRpcManager* InManager, const FZUseItemCallback& InCallback);
    
    /**
     * 使用自选宝箱
    */
    static constexpr uint64 UseSelectGift = 0x628b4953323db31aLL; 
    typedef TSharedPtr<idlezt::UseSelectGiftReq> FZUseSelectGiftReqPtr;
    typedef TSharedPtr<idlezt::UseSelectGiftAck> FZUseSelectGiftRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUseSelectGiftReqPtr&, const FZUseSelectGiftRspPtr&)> FZUseSelectGiftCallback;
    static void UseSelectGiftRegister(FMRpcManager* InManager, const FZUseSelectGiftCallback& InCallback);
    
    /**
     * 出售道具
    */
    static constexpr uint64 SellItem = 0x3b633e1152db2f46LL; 
    typedef TSharedPtr<idlezt::SellItemReq> FZSellItemReqPtr;
    typedef TSharedPtr<idlezt::SellItemAck> FZSellItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSellItemReqPtr&, const FZSellItemRspPtr&)> FZSellItemCallback;
    static void SellItemRegister(FMRpcManager* InManager, const FZSellItemCallback& InCallback);
    
    /**
     * 解锁装备槽位
    */
    static constexpr uint64 UnlockEquipmentSlot = 0x6a1e7510d613dabbLL; 
    typedef TSharedPtr<idlezt::UnlockEquipmentSlotReq> FZUnlockEquipmentSlotReqPtr;
    typedef TSharedPtr<idlezt::UnlockEquipmentSlotAck> FZUnlockEquipmentSlotRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUnlockEquipmentSlotReqPtr&, const FZUnlockEquipmentSlotRspPtr&)> FZUnlockEquipmentSlotCallback;
    static void UnlockEquipmentSlotRegister(FMRpcManager* InManager, const FZUnlockEquipmentSlotCallback& InCallback);
    
    /**
     * 开始炼丹
    */
    static constexpr uint64 AlchemyRefineStart = 0xabddac121e17959fLL; 
    typedef TSharedPtr<idlezt::AlchemyRefineStartReq> FZAlchemyRefineStartReqPtr;
    typedef TSharedPtr<idlezt::AlchemyRefineStartAck> FZAlchemyRefineStartRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAlchemyRefineStartReqPtr&, const FZAlchemyRefineStartRspPtr&)> FZAlchemyRefineStartCallback;
    static void AlchemyRefineStartRegister(FMRpcManager* InManager, const FZAlchemyRefineStartCallback& InCallback);
    
    /**
     * 终止炼丹
    */
    static constexpr uint64 AlchemyRefineCancel = 0xb2fb66060815a7bbLL; 
    typedef TSharedPtr<idlezt::AlchemyRefineCancelReq> FZAlchemyRefineCancelReqPtr;
    typedef TSharedPtr<idlezt::AlchemyRefineCancelAck> FZAlchemyRefineCancelRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAlchemyRefineCancelReqPtr&, const FZAlchemyRefineCancelRspPtr&)> FZAlchemyRefineCancelCallback;
    static void AlchemyRefineCancelRegister(FMRpcManager* InManager, const FZAlchemyRefineCancelCallback& InCallback);
    
    /**
     * 领取丹药
    */
    static constexpr uint64 AlchemyRefineExtract = 0x4cafe919a56c21feLL; 
    typedef TSharedPtr<idlezt::AlchemyRefineExtractReq> FZAlchemyRefineExtractReqPtr;
    typedef TSharedPtr<idlezt::AlchemyRefineExtractAck> FZAlchemyRefineExtractRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAlchemyRefineExtractReqPtr&, const FZAlchemyRefineExtractRspPtr&)> FZAlchemyRefineExtractCallback;
    static void AlchemyRefineExtractRegister(FMRpcManager* InManager, const FZAlchemyRefineExtractCallback& InCallback);
    
    /**
     * 获取坊市数据
    */
    static constexpr uint64 GetRoleShopData = 0x9fdf7ab06e85cf7LL; 
    typedef TSharedPtr<idlezt::GetRoleShopDataReq> FZGetRoleShopDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleShopDataAck> FZGetRoleShopDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleShopDataReqPtr&, const FZGetRoleShopDataRspPtr&)> FZGetRoleShopDataCallback;
    static void GetRoleShopDataRegister(FMRpcManager* InManager, const FZGetRoleShopDataCallback& InCallback);
    
    /**
     * 手动刷新坊市
    */
    static constexpr uint64 RefreshShop = 0xc90c61527f470ae8LL; 
    typedef TSharedPtr<idlezt::RefreshShopReq> FZRefreshShopReqPtr;
    typedef TSharedPtr<idlezt::RefreshShopAck> FZRefreshShopRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRefreshShopReqPtr&, const FZRefreshShopRspPtr&)> FZRefreshShopCallback;
    static void RefreshShopRegister(FMRpcManager* InManager, const FZRefreshShopCallback& InCallback);
    
    /**
     * 购买坊市道具
    */
    static constexpr uint64 BuyShopItem = 0x28d75a3fb3f22330LL; 
    typedef TSharedPtr<idlezt::BuyShopItemReq> FZBuyShopItemReqPtr;
    typedef TSharedPtr<idlezt::BuyShopItemAck> FZBuyShopItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZBuyShopItemReqPtr&, const FZBuyShopItemRspPtr&)> FZBuyShopItemCallback;
    static void BuyShopItemRegister(FMRpcManager* InManager, const FZBuyShopItemCallback& InCallback);
    
    /**
     * 获取天机阁数据
    */
    static constexpr uint64 GetRoleDeluxeShopData = 0x3006bc33cf275148LL; 
    typedef TSharedPtr<idlezt::GetRoleDeluxeShopDataReq> FZGetRoleDeluxeShopDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleDeluxeShopDataAck> FZGetRoleDeluxeShopDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleDeluxeShopDataReqPtr&, const FZGetRoleDeluxeShopDataRspPtr&)> FZGetRoleDeluxeShopDataCallback;
    static void GetRoleDeluxeShopDataRegister(FMRpcManager* InManager, const FZGetRoleDeluxeShopDataCallback& InCallback);
    
    /**
     * 手动刷新天机阁
    */
    static constexpr uint64 RefreshDeluxeShop = 0xa2c72a8fdf8c66cfLL; 
    typedef TSharedPtr<idlezt::RefreshDeluxeShopReq> FZRefreshDeluxeShopReqPtr;
    typedef TSharedPtr<idlezt::RefreshDeluxeShopAck> FZRefreshDeluxeShopRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRefreshDeluxeShopReqPtr&, const FZRefreshDeluxeShopRspPtr&)> FZRefreshDeluxeShopCallback;
    static void RefreshDeluxeShopRegister(FMRpcManager* InManager, const FZRefreshDeluxeShopCallback& InCallback);
    
    /**
     * 购买天机阁道具
    */
    static constexpr uint64 BuyDeluxeShopItem = 0x4c3788b66e924a37LL; 
    typedef TSharedPtr<idlezt::BuyDeluxeShopItemReq> FZBuyDeluxeShopItemReqPtr;
    typedef TSharedPtr<idlezt::BuyDeluxeShopItemAck> FZBuyDeluxeShopItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZBuyDeluxeShopItemReqPtr&, const FZBuyDeluxeShopItemRspPtr&)> FZBuyDeluxeShopItemCallback;
    static void BuyDeluxeShopItemRegister(FMRpcManager* InManager, const FZBuyDeluxeShopItemCallback& InCallback);
    
    /**
     * 获取临时包裹数据
    */
    static constexpr uint64 GetTemporaryPackageData = 0x8ff7c641e43f61b6LL; 
    typedef TSharedPtr<idlezt::GetTemporaryPackageDataReq> FZGetTemporaryPackageDataReqPtr;
    typedef TSharedPtr<idlezt::GetTemporaryPackageDataAck> FZGetTemporaryPackageDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetTemporaryPackageDataReqPtr&, const FZGetTemporaryPackageDataRspPtr&)> FZGetTemporaryPackageDataCallback;
    static void GetTemporaryPackageDataRegister(FMRpcManager* InManager, const FZGetTemporaryPackageDataCallback& InCallback);
    
    /**
     * 提取临时包裹中的道具
    */
    static constexpr uint64 ExtractTemporaryPackageItems = 0x130f7fbc2f49aa05LL; 
    typedef TSharedPtr<idlezt::ExtractTemporaryPackageItemsReq> FZExtractTemporaryPackageItemsReqPtr;
    typedef TSharedPtr<idlezt::ExtractTemporaryPackageItemsAck> FZExtractTemporaryPackageItemsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZExtractTemporaryPackageItemsReqPtr&, const FZExtractTemporaryPackageItemsRspPtr&)> FZExtractTemporaryPackageItemsCallback;
    static void ExtractTemporaryPackageItemsRegister(FMRpcManager* InManager, const FZExtractTemporaryPackageItemsCallback& InCallback);
    
    /**
     * 加速重生
    */
    static constexpr uint64 SpeedupRelive = 0xf60ce52062a3a7baLL; 
    typedef TSharedPtr<idlezt::SpeedupReliveReq> FZSpeedupReliveReqPtr;
    typedef TSharedPtr<idlezt::SpeedupReliveAck> FZSpeedupReliveRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSpeedupReliveReqPtr&, const FZSpeedupReliveRspPtr&)> FZSpeedupReliveCallback;
    static void SpeedupReliveRegister(FMRpcManager* InManager, const FZSpeedupReliveCallback& InCallback);
    
    /**
     * 获取地图信息
    */
    static constexpr uint64 GetMapInfo = 0x6469d9bc1c32f5f3LL; 
    typedef TSharedPtr<idlezt::GetMapInfoReq> FZGetMapInfoReqPtr;
    typedef TSharedPtr<idlezt::GetMapInfoAck> FZGetMapInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetMapInfoReqPtr&, const FZGetMapInfoRspPtr&)> FZGetMapInfoCallback;
    static void GetMapInfoRegister(FMRpcManager* InManager, const FZGetMapInfoCallback& InCallback);
    
    /**
     * 解锁指定秘境
    */
    static constexpr uint64 UnlockArena = 0x8222f4df05702318LL; 
    typedef TSharedPtr<idlezt::UnlockArenaReq> FZUnlockArenaReqPtr;
    typedef TSharedPtr<idlezt::UnlockArenaAck> FZUnlockArenaRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUnlockArenaReqPtr&, const FZUnlockArenaRspPtr&)> FZUnlockArenaCallback;
    static void UnlockArenaRegister(FMRpcManager* InManager, const FZUnlockArenaCallback& InCallback);
    
    /**
     * 请求任务操作
    */
    static constexpr uint64 QuestOp = 0xfbedb417707b2552LL; 
    typedef TSharedPtr<idlezt::QuestOpReq> FZQuestOpReqPtr;
    typedef TSharedPtr<idlezt::QuestOpAck> FZQuestOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZQuestOpReqPtr&, const FZQuestOpRspPtr&)> FZQuestOpCallback;
    static void QuestOpRegister(FMRpcManager* InManager, const FZQuestOpCallback& InCallback);
    
    /**
     * 穿装备
    */
    static constexpr uint64 EquipmentPutOn = 0xfeab0433ea90cb55LL; 
    typedef TSharedPtr<idlezt::EquipmentPutOnReq> FZEquipmentPutOnReqPtr;
    typedef TSharedPtr<idlezt::EquipmentPutOnAck> FZEquipmentPutOnRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZEquipmentPutOnReqPtr&, const FZEquipmentPutOnRspPtr&)> FZEquipmentPutOnCallback;
    static void EquipmentPutOnRegister(FMRpcManager* InManager, const FZEquipmentPutOnCallback& InCallback);
    
    /**
     * 脱装备
    */
    static constexpr uint64 EquipmentTakeOff = 0x35720ae84de31b19LL; 
    typedef TSharedPtr<idlezt::EquipmentTakeOffReq> FZEquipmentTakeOffReqPtr;
    typedef TSharedPtr<idlezt::EquipmentTakeOffAck> FZEquipmentTakeOffRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZEquipmentTakeOffReqPtr&, const FZEquipmentTakeOffRspPtr&)> FZEquipmentTakeOffCallback;
    static void EquipmentTakeOffRegister(FMRpcManager* InManager, const FZEquipmentTakeOffCallback& InCallback);
    
    /**
     * 请求排行榜预览，每个榜的榜一数据
    */
    static constexpr uint64 GetLeaderboardPreview = 0x2367bb66d63ff39eLL; 
    typedef TSharedPtr<idlezt::GetLeaderboardPreviewReq> FZGetLeaderboardPreviewReqPtr;
    typedef TSharedPtr<idlezt::GetLeaderboardPreviewAck> FZGetLeaderboardPreviewRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetLeaderboardPreviewReqPtr&, const FZGetLeaderboardPreviewRspPtr&)> FZGetLeaderboardPreviewCallback;
    static void GetLeaderboardPreviewRegister(FMRpcManager* InManager, const FZGetLeaderboardPreviewCallback& InCallback);
    
    /**
     * 请求排行榜数据
    */
    static constexpr uint64 GetLeaderboardData = 0x5135797fd164c32LL; 
    typedef TSharedPtr<idlezt::GetLeaderboardDataReq> FZGetLeaderboardDataReqPtr;
    typedef TSharedPtr<idlezt::GetLeaderboardDataAck> FZGetLeaderboardDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetLeaderboardDataReqPtr&, const FZGetLeaderboardDataRspPtr&)> FZGetLeaderboardDataCallback;
    static void GetLeaderboardDataRegister(FMRpcManager* InManager, const FZGetLeaderboardDataCallback& InCallback);
    
    /**
     * 请求单个玩家排行榜数据
    */
    static constexpr uint64 GetRoleLeaderboardData = 0x42b77f108e3e7366LL; 
    typedef TSharedPtr<idlezt::GetRoleLeaderboardDataReq> FZGetRoleLeaderboardDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleLeaderboardDataAck> FZGetRoleLeaderboardDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleLeaderboardDataReqPtr&, const FZGetRoleLeaderboardDataRspPtr&)> FZGetRoleLeaderboardDataCallback;
    static void GetRoleLeaderboardDataRegister(FMRpcManager* InManager, const FZGetRoleLeaderboardDataCallback& InCallback);
    
    /**
     * 请求排行榜点赞
    */
    static constexpr uint64 LeaderboardClickLike = 0x901f3761bad288e7LL; 
    typedef TSharedPtr<idlezt::LeaderboardClickLikeReq> FZLeaderboardClickLikeReqPtr;
    typedef TSharedPtr<idlezt::LeaderboardClickLikeAck> FZLeaderboardClickLikeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZLeaderboardClickLikeReqPtr&, const FZLeaderboardClickLikeRspPtr&)> FZLeaderboardClickLikeCallback;
    static void LeaderboardClickLikeRegister(FMRpcManager* InManager, const FZLeaderboardClickLikeCallback& InCallback);
    
    /**
     * 请求排行榜更新留言
    */
    static constexpr uint64 LeaderboardUpdateMessage = 0x7136c0c30375e6acLL; 
    typedef TSharedPtr<idlezt::LeaderboardUpdateMessageReq> FZLeaderboardUpdateMessageReqPtr;
    typedef TSharedPtr<idlezt::LeaderboardUpdateMessageAck> FZLeaderboardUpdateMessageRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZLeaderboardUpdateMessageReqPtr&, const FZLeaderboardUpdateMessageRspPtr&)> FZLeaderboardUpdateMessageCallback;
    static void LeaderboardUpdateMessageRegister(FMRpcManager* InManager, const FZLeaderboardUpdateMessageCallback& InCallback);
    
    /**
     * 请求领取福泽奖励
    */
    static constexpr uint64 GetFuZeReward = 0x4e1e612104aaae50LL; 
    typedef TSharedPtr<idlezt::GetFuZeRewardReq> FZGetFuZeRewardReqPtr;
    typedef TSharedPtr<idlezt::GetFuZeRewardAck> FZGetFuZeRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetFuZeRewardReqPtr&, const FZGetFuZeRewardRspPtr&)> FZGetFuZeRewardCallback;
    static void GetFuZeRewardRegister(FMRpcManager* InManager, const FZGetFuZeRewardCallback& InCallback);
    
    /**
     * 请求邮箱数据
    */
    static constexpr uint64 GetRoleMailData = 0xb445dca52f0f42b4LL; 
    typedef TSharedPtr<idlezt::GetRoleMailDataReq> FZGetRoleMailDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleMailDataAck> FZGetRoleMailDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleMailDataReqPtr&, const FZGetRoleMailDataRspPtr&)> FZGetRoleMailDataCallback;
    static void GetRoleMailDataRegister(FMRpcManager* InManager, const FZGetRoleMailDataCallback& InCallback);
    
    /**
     * 请求邮箱已读
    */
    static constexpr uint64 ReadMail = 0xa321d98496c078ceLL; 
    typedef TSharedPtr<idlezt::ReadMailReq> FZReadMailReqPtr;
    typedef TSharedPtr<idlezt::ReadMailAck> FZReadMailRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReadMailReqPtr&, const FZReadMailRspPtr&)> FZReadMailCallback;
    static void ReadMailRegister(FMRpcManager* InManager, const FZReadMailCallback& InCallback);
    
    /**
     * 请求邮箱领取
    */
    static constexpr uint64 GetMailAttachment = 0x6b4c471b8342afadLL; 
    typedef TSharedPtr<idlezt::GetMailAttachmentReq> FZGetMailAttachmentReqPtr;
    typedef TSharedPtr<idlezt::GetMailAttachmentAck> FZGetMailAttachmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetMailAttachmentReqPtr&, const FZGetMailAttachmentRspPtr&)> FZGetMailAttachmentCallback;
    static void GetMailAttachmentRegister(FMRpcManager* InManager, const FZGetMailAttachmentCallback& InCallback);
    
    /**
     * 请求删除邮件
    */
    static constexpr uint64 DeleteMail = 0xd95f1bf076785989LL; 
    typedef TSharedPtr<idlezt::DeleteMailReq> FZDeleteMailReqPtr;
    typedef TSharedPtr<idlezt::DeleteMailAck> FZDeleteMailRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDeleteMailReqPtr&, const FZDeleteMailRspPtr&)> FZDeleteMailCallback;
    static void DeleteMailRegister(FMRpcManager* InManager, const FZDeleteMailCallback& InCallback);
    
    /**
     * 请求邮件一键领取
    */
    static constexpr uint64 OneClickGetMailAttachment = 0x6fa0b907ea4a8bc9LL; 
    typedef TSharedPtr<idlezt::OneClickGetMailAttachmentReq> FZOneClickGetMailAttachmentReqPtr;
    typedef TSharedPtr<idlezt::OneClickGetMailAttachmentAck> FZOneClickGetMailAttachmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickGetMailAttachmentReqPtr&, const FZOneClickGetMailAttachmentRspPtr&)> FZOneClickGetMailAttachmentCallback;
    static void OneClickGetMailAttachmentRegister(FMRpcManager* InManager, const FZOneClickGetMailAttachmentCallback& InCallback);
    
    /**
     * 请求邮件一键已读
    */
    static constexpr uint64 OneClickReadMail = 0x9c9dac9a228cfd9aLL; 
    typedef TSharedPtr<idlezt::OneClickReadMailReq> FZOneClickReadMailReqPtr;
    typedef TSharedPtr<idlezt::OneClickReadMailAck> FZOneClickReadMailRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickReadMailReqPtr&, const FZOneClickReadMailRspPtr&)> FZOneClickReadMailCallback;
    static void OneClickReadMailRegister(FMRpcManager* InManager, const FZOneClickReadMailCallback& InCallback);
    
    /**
     * 请求邮件一键删除
    */
    static constexpr uint64 OneClickDeleteMail = 0xb4cf47cd8e057cb5LL; 
    typedef TSharedPtr<idlezt::OneClickDeleteMailReq> FZOneClickDeleteMailReqPtr;
    typedef TSharedPtr<idlezt::OneClickDeleteMailAck> FZOneClickDeleteMailRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickDeleteMailReqPtr&, const FZOneClickDeleteMailRspPtr&)> FZOneClickDeleteMailCallback;
    static void OneClickDeleteMailRegister(FMRpcManager* InManager, const FZOneClickDeleteMailCallback& InCallback);
    
    /**
     * 解锁指定模块
    */
    static constexpr uint64 UnlockFunctionModule = 0x4aed05aaa8042a61LL; 
    typedef TSharedPtr<idlezt::UnlockFunctionModuleReq> FZUnlockFunctionModuleReqPtr;
    typedef TSharedPtr<idlezt::UnlockFunctionModuleAck> FZUnlockFunctionModuleRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUnlockFunctionModuleReqPtr&, const FZUnlockFunctionModuleRspPtr&)> FZUnlockFunctionModuleCallback;
    static void UnlockFunctionModuleRegister(FMRpcManager* InManager, const FZUnlockFunctionModuleCallback& InCallback);
    
    /**
     * 请求聊天消息
    */
    static constexpr uint64 GetChatRecord = 0x451f04034bae2d98LL; 
    typedef TSharedPtr<idlezt::GetChatRecordReq> FZGetChatRecordReqPtr;
    typedef TSharedPtr<idlezt::GetChatRecordAck> FZGetChatRecordRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetChatRecordReqPtr&, const FZGetChatRecordRspPtr&)> FZGetChatRecordCallback;
    static void GetChatRecordRegister(FMRpcManager* InManager, const FZGetChatRecordCallback& InCallback);
    
    /**
     * 请求删除私聊消息
    */
    static constexpr uint64 DeletePrivateChatRecord = 0x778bfb6d543ec55eLL; 
    typedef TSharedPtr<idlezt::DeletePrivateChatRecordReq> FZDeletePrivateChatRecordReqPtr;
    typedef TSharedPtr<idlezt::DeletePrivateChatRecordAck> FZDeletePrivateChatRecordRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDeletePrivateChatRecordReqPtr&, const FZDeletePrivateChatRecordRspPtr&)> FZDeletePrivateChatRecordCallback;
    static void DeletePrivateChatRecordRegister(FMRpcManager* InManager, const FZDeletePrivateChatRecordCallback& InCallback);
    
    /**
     * 发送聊天消息
    */
    static constexpr uint64 SendChatMessage = 0x688de90d229c3d08LL; 
    typedef TSharedPtr<idlezt::SendChatMessageReq> FZSendChatMessageReqPtr;
    typedef TSharedPtr<idlezt::SendChatMessageAck> FZSendChatMessageRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSendChatMessageReqPtr&, const FZSendChatMessageRspPtr&)> FZSendChatMessageCallback;
    static void SendChatMessageRegister(FMRpcManager* InManager, const FZSendChatMessageCallback& InCallback);
    
    /**
     * 请求聊天记录已读
    */
    static constexpr uint64 ClearChatUnreadNum = 0x28d9b329e373bd31LL; 
    typedef TSharedPtr<idlezt::ClearChatUnreadNumReq> FZClearChatUnreadNumReqPtr;
    typedef TSharedPtr<idlezt::ClearChatUnreadNumAck> FZClearChatUnreadNumRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZClearChatUnreadNumReqPtr&, const FZClearChatUnreadNumRspPtr&)> FZClearChatUnreadNumCallback;
    static void ClearChatUnreadNumRegister(FMRpcManager* InManager, const FZClearChatUnreadNumCallback& InCallback);
    
    /**
     * 开始炼器
    */
    static constexpr uint64 ForgeRefineStart = 0xa150d9c40f25bd7bLL; 
    typedef TSharedPtr<idlezt::ForgeRefineStartReq> FZForgeRefineStartReqPtr;
    typedef TSharedPtr<idlezt::ForgeRefineStartAck> FZForgeRefineStartRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZForgeRefineStartReqPtr&, const FZForgeRefineStartRspPtr&)> FZForgeRefineStartCallback;
    static void ForgeRefineStartRegister(FMRpcManager* InManager, const FZForgeRefineStartCallback& InCallback);
    
    /**
     * 终止炼器
    */
    static constexpr uint64 ForgeRefineCancel = 0x245824dfda9cfdb7LL; 
    typedef TSharedPtr<idlezt::ForgeRefineCancelReq> FZForgeRefineCancelReqPtr;
    typedef TSharedPtr<idlezt::ForgeRefineCancelAck> FZForgeRefineCancelRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZForgeRefineCancelReqPtr&, const FZForgeRefineCancelRspPtr&)> FZForgeRefineCancelCallback;
    static void ForgeRefineCancelRegister(FMRpcManager* InManager, const FZForgeRefineCancelCallback& InCallback);
    
    /**
     * 领取炼器生成的道具
    */
    static constexpr uint64 ForgeRefineExtract = 0xff837301ca159ed2LL; 
    typedef TSharedPtr<idlezt::ForgeRefineExtractReq> FZForgeRefineExtractReqPtr;
    typedef TSharedPtr<idlezt::ForgeRefineExtractAck> FZForgeRefineExtractRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZForgeRefineExtractReqPtr&, const FZForgeRefineExtractRspPtr&)> FZForgeRefineExtractCallback;
    static void ForgeRefineExtractRegister(FMRpcManager* InManager, const FZForgeRefineExtractCallback& InCallback);
    
    /**
     * 请求找回装备数据
    */
    static constexpr uint64 GetForgeLostEquipmentData = 0xaf589a48f17e95fcLL; 
    typedef TSharedPtr<idlezt::GetForgeLostEquipmentDataReq> FZGetForgeLostEquipmentDataReqPtr;
    typedef TSharedPtr<idlezt::GetForgeLostEquipmentDataAck> FZGetForgeLostEquipmentDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetForgeLostEquipmentDataReqPtr&, const FZGetForgeLostEquipmentDataRspPtr&)> FZGetForgeLostEquipmentDataCallback;
    static void GetForgeLostEquipmentDataRegister(FMRpcManager* InManager, const FZGetForgeLostEquipmentDataCallback& InCallback);
    
    /**
     * 请求销毁装备
    */
    static constexpr uint64 ForgeDestroy = 0x8bb1dda6abb6140aLL; 
    typedef TSharedPtr<idlezt::ForgeDestroyReq> FZForgeDestroyReqPtr;
    typedef TSharedPtr<idlezt::ForgeDestroyAck> FZForgeDestroyRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZForgeDestroyReqPtr&, const FZForgeDestroyRspPtr&)> FZForgeDestroyCallback;
    static void ForgeDestroyRegister(FMRpcManager* InManager, const FZForgeDestroyCallback& InCallback);
    
    /**
     * 请求找回装备
    */
    static constexpr uint64 ForgeFindBack = 0xf6565d61f8e068a4LL; 
    typedef TSharedPtr<idlezt::ForgeFindBackReq> FZForgeFindBackReqPtr;
    typedef TSharedPtr<idlezt::ForgeFindBackAck> FZForgeFindBackRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZForgeFindBackReqPtr&, const FZForgeFindBackRspPtr&)> FZForgeFindBackCallback;
    static void ForgeFindBackRegister(FMRpcManager* InManager, const FZForgeFindBackCallback& InCallback);
    
    /**
     * 请求秘药数据
    */
    static constexpr uint64 RequestPillElixirData = 0x2945b2c033b93c32LL; 
    typedef TSharedPtr<idlezt::RequestPillElixirDataReq> FZRequestPillElixirDataReqPtr;
    typedef TSharedPtr<idlezt::RequestPillElixirDataAck> FZRequestPillElixirDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestPillElixirDataReqPtr&, const FZRequestPillElixirDataRspPtr&)> FZRequestPillElixirDataCallback;
    static void RequestPillElixirDataRegister(FMRpcManager* InManager, const FZRequestPillElixirDataCallback& InCallback);
    
    /**
     * 请求单种秘药数据
    */
    static constexpr uint64 GetOnePillElixirData = 0x88a2e5cd340feec1LL; 
    typedef TSharedPtr<idlezt::GetOnePillElixirDataReq> FZGetOnePillElixirDataReqPtr;
    typedef TSharedPtr<idlezt::GetOnePillElixirDataAck> FZGetOnePillElixirDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetOnePillElixirDataReqPtr&, const FZGetOnePillElixirDataRspPtr&)> FZGetOnePillElixirDataCallback;
    static void GetOnePillElixirDataRegister(FMRpcManager* InManager, const FZGetOnePillElixirDataCallback& InCallback);
    
    /**
     * 请求修改秘药过滤配置
    */
    static constexpr uint64 RequestModifyPillElixirFilter = 0x78ac6f91455e40d6LL; 
    typedef TSharedPtr<idlezt::RequestModifyPillElixirFilterReq> FZRequestModifyPillElixirFilterReqPtr;
    typedef TSharedPtr<idlezt::RequestModifyPillElixirFilterAck> FZRequestModifyPillElixirFilterRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestModifyPillElixirFilterReqPtr&, const FZRequestModifyPillElixirFilterRspPtr&)> FZRequestModifyPillElixirFilterCallback;
    static void RequestModifyPillElixirFilterRegister(FMRpcManager* InManager, const FZRequestModifyPillElixirFilterCallback& InCallback);
    
    /**
     * 使用单颗秘药
    */
    static constexpr uint64 UsePillElixir = 0x909091e4663e1950LL; 
    typedef TSharedPtr<idlezt::UsePillElixirReq> FZUsePillElixirReqPtr;
    typedef TSharedPtr<idlezt::UsePillElixirAck> FZUsePillElixirRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUsePillElixirReqPtr&, const FZUsePillElixirRspPtr&)> FZUsePillElixirCallback;
    static void UsePillElixirRegister(FMRpcManager* InManager, const FZUsePillElixirCallback& InCallback);
    
    /**
     * 一键使用秘药
    */
    static constexpr uint64 OneClickUsePillElixir = 0x2c7fc11389370444LL; 
    typedef TSharedPtr<idlezt::OneClickUsePillElixirReq> FZOneClickUsePillElixirReqPtr;
    typedef TSharedPtr<idlezt::OneClickUsePillElixirAck> FZOneClickUsePillElixirRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickUsePillElixirReqPtr&, const FZOneClickUsePillElixirRspPtr&)> FZOneClickUsePillElixirCallback;
    static void OneClickUsePillElixirRegister(FMRpcManager* InManager, const FZOneClickUsePillElixirCallback& InCallback);
    
    /**
     * 请求秘药兑换天机石
    */
    static constexpr uint64 TradePillElixir = 0x72ea45b09ee60275LL; 
    typedef TSharedPtr<idlezt::TradePillElixirReq> FZTradePillElixirReqPtr;
    typedef TSharedPtr<idlezt::TradePillElixirAck> FZTradePillElixirRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZTradePillElixirReqPtr&, const FZTradePillElixirRspPtr&)> FZTradePillElixirCallback;
    static void TradePillElixirRegister(FMRpcManager* InManager, const FZTradePillElixirCallback& InCallback);
    
    /**
     * 请求强化装备
    */
    static constexpr uint64 ReinforceEquipment = 0x25837281d7f4b96aLL; 
    typedef TSharedPtr<idlezt::ReinforceEquipmentReq> FZReinforceEquipmentReqPtr;
    typedef TSharedPtr<idlezt::ReinforceEquipmentAck> FZReinforceEquipmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReinforceEquipmentReqPtr&, const FZReinforceEquipmentRspPtr&)> FZReinforceEquipmentCallback;
    static void ReinforceEquipmentRegister(FMRpcManager* InManager, const FZReinforceEquipmentCallback& InCallback);
    
    /**
     * 请求精炼装备
    */
    static constexpr uint64 RefineEquipment = 0xd08425d38ff20830LL; 
    typedef TSharedPtr<idlezt::RefineEquipmentReq> FZRefineEquipmentReqPtr;
    typedef TSharedPtr<idlezt::RefineEquipmentAck> FZRefineEquipmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRefineEquipmentReqPtr&, const FZRefineEquipmentRspPtr&)> FZRefineEquipmentCallback;
    static void RefineEquipmentRegister(FMRpcManager* InManager, const FZRefineEquipmentCallback& InCallback);
    
    /**
     * 请求器纹装备
    */
    static constexpr uint64 QiWenEquipment = 0x26d70bd6bcac18a5LL; 
    typedef TSharedPtr<idlezt::QiWenEquipmentReq> FZQiWenEquipmentReqPtr;
    typedef TSharedPtr<idlezt::QiWenEquipmentAck> FZQiWenEquipmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZQiWenEquipmentReqPtr&, const FZQiWenEquipmentRspPtr&)> FZQiWenEquipmentCallback;
    static void QiWenEquipmentRegister(FMRpcManager* InManager, const FZQiWenEquipmentCallback& InCallback);
    
    /**
     * 请求还原装备
    */
    static constexpr uint64 ResetEquipment = 0x1ccb55f9c9126738LL; 
    typedef TSharedPtr<idlezt::ResetEquipmentReq> FZResetEquipmentReqPtr;
    typedef TSharedPtr<idlezt::ResetEquipmentAck> FZResetEquipmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZResetEquipmentReqPtr&, const FZResetEquipmentRspPtr&)> FZResetEquipmentCallback;
    static void ResetEquipmentRegister(FMRpcManager* InManager, const FZResetEquipmentCallback& InCallback);
    
    /**
     * 请求继承装备
    */
    static constexpr uint64 InheritEquipment = 0x86dabc3bdeffe718LL; 
    typedef TSharedPtr<idlezt::InheritEquipmentReq> FZInheritEquipmentReqPtr;
    typedef TSharedPtr<idlezt::InheritEquipmentAck> FZInheritEquipmentRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZInheritEquipmentReqPtr&, const FZInheritEquipmentRspPtr&)> FZInheritEquipmentCallback;
    static void InheritEquipmentRegister(FMRpcManager* InManager, const FZInheritEquipmentCallback& InCallback);
    
    /**
     * 请求锁定/解锁道具
    */
    static constexpr uint64 LockItem = 0xe24529e9f4d50c99LL; 
    typedef TSharedPtr<idlezt::LockItemReq> FZLockItemReqPtr;
    typedef TSharedPtr<idlezt::LockItemAck> FZLockItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZLockItemReqPtr&, const FZLockItemRspPtr&)> FZLockItemCallback;
    static void LockItemRegister(FMRpcManager* InManager, const FZLockItemCallback& InCallback);
    
    /**
     * 发起切磋
    */
    static constexpr uint64 SoloArenaChallenge = 0x39962a0fb2e02046LL; 
    typedef TSharedPtr<idlezt::SoloArenaChallengeReq> FZSoloArenaChallengeReqPtr;
    typedef TSharedPtr<idlezt::SoloArenaChallengeAck> FZSoloArenaChallengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSoloArenaChallengeReqPtr&, const FZSoloArenaChallengeRspPtr&)> FZSoloArenaChallengeCallback;
    static void SoloArenaChallengeRegister(FMRpcManager* InManager, const FZSoloArenaChallengeCallback& InCallback);
    
    /**
     * 快速结束切磋
    */
    static constexpr uint64 SoloArenaQuickEnd = 0xce1c8fa64b159e59LL; 
    typedef TSharedPtr<idlezt::SoloArenaQuickEndReq> FZSoloArenaQuickEndReqPtr;
    typedef TSharedPtr<idlezt::SoloArenaQuickEndAck> FZSoloArenaQuickEndRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSoloArenaQuickEndReqPtr&, const FZSoloArenaQuickEndRspPtr&)> FZSoloArenaQuickEndCallback;
    static void SoloArenaQuickEndRegister(FMRpcManager* InManager, const FZSoloArenaQuickEndCallback& InCallback);
    
    /**
     * 获取切磋历史列表
    */
    static constexpr uint64 GetSoloArenaHistoryList = 0x4c4eda72778397edLL; 
    typedef TSharedPtr<idlezt::GetSoloArenaHistoryListReq> FZGetSoloArenaHistoryListReqPtr;
    typedef TSharedPtr<idlezt::GetSoloArenaHistoryListAck> FZGetSoloArenaHistoryListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSoloArenaHistoryListReqPtr&, const FZGetSoloArenaHistoryListRspPtr&)> FZGetSoloArenaHistoryListCallback;
    static void GetSoloArenaHistoryListRegister(FMRpcManager* InManager, const FZGetSoloArenaHistoryListCallback& InCallback);
    
    /**
     * 挑战镇妖塔
    */
    static constexpr uint64 MonsterTowerChallenge = 0xe846fbe19bb38dfLL; 
    typedef TSharedPtr<idlezt::MonsterTowerChallengeReq> FZMonsterTowerChallengeReqPtr;
    typedef TSharedPtr<idlezt::MonsterTowerChallengeAck> FZMonsterTowerChallengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZMonsterTowerChallengeReqPtr&, const FZMonsterTowerChallengeRspPtr&)> FZMonsterTowerChallengeCallback;
    static void MonsterTowerChallengeRegister(FMRpcManager* InManager, const FZMonsterTowerChallengeCallback& InCallback);
    
    /**
     * 领取镇妖塔挂机奖励
    */
    static constexpr uint64 MonsterTowerDrawIdleAward = 0x4f576246f6fe2d57LL; 
    typedef TSharedPtr<idlezt::MonsterTowerDrawIdleAwardReq> FZMonsterTowerDrawIdleAwardReqPtr;
    typedef TSharedPtr<idlezt::MonsterTowerDrawIdleAwardAck> FZMonsterTowerDrawIdleAwardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZMonsterTowerDrawIdleAwardReqPtr&, const FZMonsterTowerDrawIdleAwardRspPtr&)> FZMonsterTowerDrawIdleAwardCallback;
    static void MonsterTowerDrawIdleAwardRegister(FMRpcManager* InManager, const FZMonsterTowerDrawIdleAwardCallback& InCallback);
    
    /**
     * 镇妖塔闭关
    */
    static constexpr uint64 MonsterTowerClosedDoorTraining = 0x42f3603049d201f4LL; 
    typedef TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingReq> FZMonsterTowerClosedDoorTrainingReqPtr;
    typedef TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingAck> FZMonsterTowerClosedDoorTrainingRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZMonsterTowerClosedDoorTrainingReqPtr&, const FZMonsterTowerClosedDoorTrainingRspPtr&)> FZMonsterTowerClosedDoorTrainingCallback;
    static void MonsterTowerClosedDoorTrainingRegister(FMRpcManager* InManager, const FZMonsterTowerClosedDoorTrainingCallback& InCallback);
    
    /**
     * 镇妖塔快速结束
    */
    static constexpr uint64 MonsterTowerQuickEnd = 0xc86f9309c963a49eLL; 
    typedef TSharedPtr<idlezt::MonsterTowerQuickEndReq> FZMonsterTowerQuickEndReqPtr;
    typedef TSharedPtr<idlezt::MonsterTowerQuickEndAck> FZMonsterTowerQuickEndRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZMonsterTowerQuickEndReqPtr&, const FZMonsterTowerQuickEndRspPtr&)> FZMonsterTowerQuickEndCallback;
    static void MonsterTowerQuickEndRegister(FMRpcManager* InManager, const FZMonsterTowerQuickEndCallback& InCallback);
    
    /**
     * 镇妖塔挑战榜数据
    */
    static constexpr uint64 GetMonsterTowerChallengeList = 0xc095bb1780b8a059LL; 
    typedef TSharedPtr<idlezt::GetMonsterTowerChallengeListReq> FZGetMonsterTowerChallengeListReqPtr;
    typedef TSharedPtr<idlezt::GetMonsterTowerChallengeListAck> FZGetMonsterTowerChallengeListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetMonsterTowerChallengeListReqPtr&, const FZGetMonsterTowerChallengeListRspPtr&)> FZGetMonsterTowerChallengeListCallback;
    static void GetMonsterTowerChallengeListRegister(FMRpcManager* InManager, const FZGetMonsterTowerChallengeListCallback& InCallback);
    
    /**
     * 镇妖塔挑战榜奖励
    */
    static constexpr uint64 GetMonsterTowerChallengeReward = 0xdb06e30206b69bf2LL; 
    typedef TSharedPtr<idlezt::GetMonsterTowerChallengeRewardReq> FZGetMonsterTowerChallengeRewardReqPtr;
    typedef TSharedPtr<idlezt::GetMonsterTowerChallengeRewardAck> FZGetMonsterTowerChallengeRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetMonsterTowerChallengeRewardReqPtr&, const FZGetMonsterTowerChallengeRewardRspPtr&)> FZGetMonsterTowerChallengeRewardCallback;
    static void GetMonsterTowerChallengeRewardRegister(FMRpcManager* InManager, const FZGetMonsterTowerChallengeRewardCallback& InCallback);
    
    /**
     * 设置地图TimeDilation
    */
    static constexpr uint64 SetWorldTimeDilation = 0xb6c0334513906ed2LL; 
    typedef TSharedPtr<idlezt::SetWorldTimeDilationReq> FZSetWorldTimeDilationReqPtr;
    typedef TSharedPtr<idlezt::SetWorldTimeDilationAck> FZSetWorldTimeDilationRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetWorldTimeDilationReqPtr&, const FZSetWorldTimeDilationRspPtr&)> FZSetWorldTimeDilationCallback;
    static void SetWorldTimeDilationRegister(FMRpcManager* InManager, const FZSetWorldTimeDilationCallback& InCallback);
    
    /**
     * 设置战斗模式
    */
    static constexpr uint64 SetFightMode = 0x335a9d676ed25abcLL; 
    typedef TSharedPtr<idlezt::SetFightModeReq> FZSetFightModeReqPtr;
    typedef TSharedPtr<idlezt::SetFightModeAck> FZSetFightModeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetFightModeReqPtr&, const FZSetFightModeRspPtr&)> FZSetFightModeCallback;
    static void SetFightModeRegister(FMRpcManager* InManager, const FZSetFightModeCallback& InCallback);
    
    /**
     * 升级聚灵阵
    */
    static constexpr uint64 UpgradeQiCollector = 0x2f43e464bb10a19cLL; 
    typedef TSharedPtr<idlezt::UpgradeQiCollectorReq> FZUpgradeQiCollectorReqPtr;
    typedef TSharedPtr<idlezt::UpgradeQiCollectorAck> FZUpgradeQiCollectorRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUpgradeQiCollectorReqPtr&, const FZUpgradeQiCollectorRspPtr&)> FZUpgradeQiCollectorCallback;
    static void UpgradeQiCollectorRegister(FMRpcManager* InManager, const FZUpgradeQiCollectorCallback& InCallback);
    
    /**
     * 请求玩家的游戏数值数据
    */
    static constexpr uint64 GetRoleAllStats = 0x216214463af5884dLL; 
    typedef TSharedPtr<idlezt::GetRoleAllStatsReq> FZGetRoleAllStatsReqPtr;
    typedef TSharedPtr<idlezt::GetRoleAllStatsAck> FZGetRoleAllStatsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleAllStatsReqPtr&, const FZGetRoleAllStatsRspPtr&)> FZGetRoleAllStatsCallback;
    static void GetRoleAllStatsRegister(FMRpcManager* InManager, const FZGetRoleAllStatsCallback& InCallback);
    
    /**
     * 请求玩家山河图数据
    */
    static constexpr uint64 GetShanhetuData = 0xeec577b92ccea947LL; 
    typedef TSharedPtr<idlezt::GetShanhetuDataReq> FZGetShanhetuDataReqPtr;
    typedef TSharedPtr<idlezt::GetShanhetuDataAck> FZGetShanhetuDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetShanhetuDataReqPtr&, const FZGetShanhetuDataRspPtr&)> FZGetShanhetuDataCallback;
    static void GetShanhetuDataRegister(FMRpcManager* InManager, const FZGetShanhetuDataCallback& InCallback);
    
    /**
     * 请求修改山河图使用配置
    */
    static constexpr uint64 SetShanhetuUseConfig = 0xaab1ee2cc23a2a14LL; 
    typedef TSharedPtr<idlezt::SetShanhetuUseConfigReq> FZSetShanhetuUseConfigReqPtr;
    typedef TSharedPtr<idlezt::SetShanhetuUseConfigAck> FZSetShanhetuUseConfigRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetShanhetuUseConfigReqPtr&, const FZSetShanhetuUseConfigRspPtr&)> FZSetShanhetuUseConfigCallback;
    static void SetShanhetuUseConfigRegister(FMRpcManager* InManager, const FZSetShanhetuUseConfigCallback& InCallback);
    
    /**
     * 请求使用山河图
    */
    static constexpr uint64 UseShanhetu = 0x790e255a6f91bf50LL; 
    typedef TSharedPtr<idlezt::UseShanhetuReq> FZUseShanhetuReqPtr;
    typedef TSharedPtr<idlezt::UseShanhetuAck> FZUseShanhetuRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUseShanhetuReqPtr&, const FZUseShanhetuRspPtr&)> FZUseShanhetuCallback;
    static void UseShanhetuRegister(FMRpcManager* InManager, const FZUseShanhetuCallback& InCallback);
    
    /**
     * 探索山河图
    */
    static constexpr uint64 StepShanhetu = 0x63ff615abba0c9c7LL; 
    typedef TSharedPtr<idlezt::StepShanhetuReq> FZStepShanhetuReqPtr;
    typedef TSharedPtr<idlezt::StepShanhetuAck> FZStepShanhetuRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZStepShanhetuReqPtr&, const FZStepShanhetuRspPtr&)> FZStepShanhetuCallback;
    static void StepShanhetuRegister(FMRpcManager* InManager, const FZStepShanhetuCallback& InCallback);
    
    /**
     * 请求山河图记录
    */
    static constexpr uint64 GetShanhetuUseRecord = 0xe7fe60395284dcf3LL; 
    typedef TSharedPtr<idlezt::GetShanhetuUseRecordReq> FZGetShanhetuUseRecordReqPtr;
    typedef TSharedPtr<idlezt::GetShanhetuUseRecordAck> FZGetShanhetuUseRecordRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetShanhetuUseRecordReqPtr&, const FZGetShanhetuUseRecordRspPtr&)> FZGetShanhetuUseRecordCallback;
    static void GetShanhetuUseRecordRegister(FMRpcManager* InManager, const FZGetShanhetuUseRecordCallback& InCallback);
    
    /**
     * 设置锁定方式
    */
    static constexpr uint64 SetAttackLockType = 0x2a2807392e4ae15cLL; 
    typedef TSharedPtr<idlezt::SetAttackLockTypeReq> FZSetAttackLockTypeReqPtr;
    typedef TSharedPtr<idlezt::SetAttackLockTypeAck> FZSetAttackLockTypeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetAttackLockTypeReqPtr&, const FZSetAttackLockTypeRspPtr&)> FZSetAttackLockTypeCallback;
    static void SetAttackLockTypeRegister(FMRpcManager* InManager, const FZSetAttackLockTypeCallback& InCallback);
    
    /**
     * 设置取消锁定方式
    */
    static constexpr uint64 SetAttackUnlockType = 0x20c5b2be9034b25bLL; 
    typedef TSharedPtr<idlezt::SetAttackUnlockTypeReq> FZSetAttackUnlockTypeReqPtr;
    typedef TSharedPtr<idlezt::SetAttackUnlockTypeAck> FZSetAttackUnlockTypeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetAttackUnlockTypeReqPtr&, const FZSetAttackUnlockTypeRspPtr&)> FZSetAttackUnlockTypeCallback;
    static void SetAttackUnlockTypeRegister(FMRpcManager* InManager, const FZSetAttackUnlockTypeCallback& InCallback);
    
    /**
     * 设置是否显示解锁按钮
    */
    static constexpr uint64 SetShowUnlockButton = 0xa278990df5a2b8faLL; 
    typedef TSharedPtr<idlezt::SetShowUnlockButtonReq> FZSetShowUnlockButtonReqPtr;
    typedef TSharedPtr<idlezt::SetShowUnlockButtonAck> FZSetShowUnlockButtonRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetShowUnlockButtonReqPtr&, const FZSetShowUnlockButtonRspPtr&)> FZSetShowUnlockButtonCallback;
    static void SetShowUnlockButtonRegister(FMRpcManager* InManager, const FZSetShowUnlockButtonCallback& InCallback);
    
    /**
     * 获取用户变量内容
    */
    static constexpr uint64 GetUserVar = 0xaca68c3d3a96aa45LL; 
    typedef TSharedPtr<idlezt::GetUserVarReq> FZGetUserVarReqPtr;
    typedef TSharedPtr<idlezt::GetUserVarRsp> FZGetUserVarRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetUserVarReqPtr&, const FZGetUserVarRspPtr&)> FZGetUserVarCallback;
    static void GetUserVarRegister(FMRpcManager* InManager, const FZGetUserVarCallback& InCallback);
    
    /**
     * 获取多个用户变量内容
    */
    static constexpr uint64 GetUserVars = 0xf5aa820a8e0339c2LL; 
    typedef TSharedPtr<idlezt::GetUserVarsReq> FZGetUserVarsReqPtr;
    typedef TSharedPtr<idlezt::GetUserVarsRsp> FZGetUserVarsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetUserVarsReqPtr&, const FZGetUserVarsRspPtr&)> FZGetUserVarsCallback;
    static void GetUserVarsRegister(FMRpcManager* InManager, const FZGetUserVarsCallback& InCallback);
    
    /**
     * 获取指定秘境BOSS入侵情况
    */
    static constexpr uint64 GetBossInvasionArenaSummary = 0xcac4728755bbf1f0LL; 
    typedef TSharedPtr<idlezt::GetBossInvasionArenaSummaryReq> FZGetBossInvasionArenaSummaryReqPtr;
    typedef TSharedPtr<idlezt::GetBossInvasionArenaSummaryRsp> FZGetBossInvasionArenaSummaryRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetBossInvasionArenaSummaryReqPtr&, const FZGetBossInvasionArenaSummaryRspPtr&)> FZGetBossInvasionArenaSummaryCallback;
    static void GetBossInvasionArenaSummaryRegister(FMRpcManager* InManager, const FZGetBossInvasionArenaSummaryCallback& InCallback);
    
    /**
     * 获取指定秘境BOSS入侵伤害排行榜
    */
    static constexpr uint64 GetBossInvasionArenaTopList = 0xb36f362150e026bbLL; 
    typedef TSharedPtr<idlezt::GetBossInvasionArenaTopListReq> FZGetBossInvasionArenaTopListReqPtr;
    typedef TSharedPtr<idlezt::GetBossInvasionArenaTopListRsp> FZGetBossInvasionArenaTopListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetBossInvasionArenaTopListReqPtr&, const FZGetBossInvasionArenaTopListRspPtr&)> FZGetBossInvasionArenaTopListCallback;
    static void GetBossInvasionArenaTopListRegister(FMRpcManager* InManager, const FZGetBossInvasionArenaTopListCallback& InCallback);
    
    /**
     * 获取BOSS入侵情况
    */
    static constexpr uint64 GetBossInvasionInfo = 0x76ea178a72b41e15LL; 
    typedef TSharedPtr<idlezt::GetBossInvasionInfoReq> FZGetBossInvasionInfoReqPtr;
    typedef TSharedPtr<idlezt::GetBossInvasionInfoRsp> FZGetBossInvasionInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetBossInvasionInfoReqPtr&, const FZGetBossInvasionInfoRspPtr&)> FZGetBossInvasionInfoCallback;
    static void GetBossInvasionInfoRegister(FMRpcManager* InManager, const FZGetBossInvasionInfoCallback& InCallback);
    
    /**
     * 领取击杀奖励
    */
    static constexpr uint64 DrawBossInvasionKillReward = 0xe486e368d36ec5a2LL; 
    typedef TSharedPtr<idlezt::DrawBossInvasionKillRewardReq> FZDrawBossInvasionKillRewardReqPtr;
    typedef TSharedPtr<idlezt::DrawBossInvasionKillRewardRsp> FZDrawBossInvasionKillRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDrawBossInvasionKillRewardReqPtr&, const FZDrawBossInvasionKillRewardRspPtr&)> FZDrawBossInvasionKillRewardCallback;
    static void DrawBossInvasionKillRewardRegister(FMRpcManager* InManager, const FZDrawBossInvasionKillRewardCallback& InCallback);
    
    /**
     * 领取伤害排行奖励
    */
    static constexpr uint64 DrawBossInvasionDamageReward = 0xcab7fb04d2812523LL; 
    typedef TSharedPtr<idlezt::DrawBossInvasionDamageRewardReq> FZDrawBossInvasionDamageRewardReqPtr;
    typedef TSharedPtr<idlezt::DrawBossInvasionDamageRewardRsp> FZDrawBossInvasionDamageRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDrawBossInvasionDamageRewardReqPtr&, const FZDrawBossInvasionDamageRewardRspPtr&)> FZDrawBossInvasionDamageRewardCallback;
    static void DrawBossInvasionDamageRewardRegister(FMRpcManager* InManager, const FZDrawBossInvasionDamageRewardCallback& InCallback);
    
    /**
    */
    static constexpr uint64 BossInvasionTeleport = 0xd2f39b37c2533f68LL; 
    typedef TSharedPtr<idlezt::BossInvasionTeleportReq> FZBossInvasionTeleportReqPtr;
    typedef TSharedPtr<idlezt::BossInvasionTeleportRsp> FZBossInvasionTeleportRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZBossInvasionTeleportReqPtr&, const FZBossInvasionTeleportRspPtr&)> FZBossInvasionTeleportCallback;
    static void BossInvasionTeleportRegister(FMRpcManager* InManager, const FZBossInvasionTeleportCallback& InCallback);
    
    /**
     * 分享自己的道具
    */
    static constexpr uint64 ShareSelfItem = 0x1a7ace3b1d106ed9LL; 
    typedef TSharedPtr<idlezt::ShareSelfItemReq> FZShareSelfItemReqPtr;
    typedef TSharedPtr<idlezt::ShareSelfItemRsp> FZShareSelfItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZShareSelfItemReqPtr&, const FZShareSelfItemRspPtr&)> FZShareSelfItemCallback;
    static void ShareSelfItemRegister(FMRpcManager* InManager, const FZShareSelfItemCallback& InCallback);
    
    /**
     * 分享自己的多个道具
    */
    static constexpr uint64 ShareSelfItems = 0xf1b187262ec0adeLL; 
    typedef TSharedPtr<idlezt::ShareSelfItemsReq> FZShareSelfItemsReqPtr;
    typedef TSharedPtr<idlezt::ShareSelfItemsRsp> FZShareSelfItemsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZShareSelfItemsReqPtr&, const FZShareSelfItemsRspPtr&)> FZShareSelfItemsCallback;
    static void ShareSelfItemsRegister(FMRpcManager* InManager, const FZShareSelfItemsCallback& InCallback);
    
    /**
     * 获取分享道具数据
    */
    static constexpr uint64 GetShareItemData = 0x4272d0a1dfd47adLL; 
    typedef TSharedPtr<idlezt::GetShareItemDataReq> FZGetShareItemDataReqPtr;
    typedef TSharedPtr<idlezt::GetShareItemDataRsp> FZGetShareItemDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetShareItemDataReqPtr&, const FZGetShareItemDataRspPtr&)> FZGetShareItemDataCallback;
    static void GetShareItemDataRegister(FMRpcManager* InManager, const FZGetShareItemDataCallback& InCallback);
    
    /**
     * 获取玩家古宝数据
    */
    static constexpr uint64 GetRoleCollectionData = 0xa9f037d0a697d8e3LL; 
    typedef TSharedPtr<idlezt::GetRoleCollectionDataReq> FZGetRoleCollectionDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleCollectionDataRsp> FZGetRoleCollectionDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleCollectionDataReqPtr&, const FZGetRoleCollectionDataRspPtr&)> FZGetRoleCollectionDataCallback;
    static void GetRoleCollectionDataRegister(FMRpcManager* InManager, const FZGetRoleCollectionDataCallback& InCallback);
    
    /**
     * 古宝操作
    */
    static constexpr uint64 RoleCollectionOp = 0xa07dd16299463242LL; 
    typedef TSharedPtr<idlezt::RoleCollectionOpReq> FZRoleCollectionOpReqPtr;
    typedef TSharedPtr<idlezt::RoleCollectionOpAck> FZRoleCollectionOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRoleCollectionOpReqPtr&, const FZRoleCollectionOpRspPtr&)> FZRoleCollectionOpCallback;
    static void RoleCollectionOpRegister(FMRpcManager* InManager, const FZRoleCollectionOpCallback& InCallback);
    
    /**
     * 分享自己的古宝
    */
    static constexpr uint64 ShareSelfRoleCollection = 0x3f972ad0f7b271b2LL; 
    typedef TSharedPtr<idlezt::ShareSelfRoleCollectionReq> FZShareSelfRoleCollectionReqPtr;
    typedef TSharedPtr<idlezt::ShareSelfRoleCollectionRsp> FZShareSelfRoleCollectionRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZShareSelfRoleCollectionReqPtr&, const FZShareSelfRoleCollectionRspPtr&)> FZShareSelfRoleCollectionCallback;
    static void ShareSelfRoleCollectionRegister(FMRpcManager* InManager, const FZShareSelfRoleCollectionCallback& InCallback);
    
    /**
     * 获取分享古宝数据
    */
    static constexpr uint64 GetShareRoleCollectionData = 0x1696b8cffc2aa24eLL; 
    typedef TSharedPtr<idlezt::GetShareRoleCollectionDataReq> FZGetShareRoleCollectionDataReqPtr;
    typedef TSharedPtr<idlezt::GetShareRoleCollectionDataRsp> FZGetShareRoleCollectionDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetShareRoleCollectionDataReqPtr&, const FZGetShareRoleCollectionDataRspPtr&)> FZGetShareRoleCollectionDataCallback;
    static void GetShareRoleCollectionDataRegister(FMRpcManager* InManager, const FZGetShareRoleCollectionDataCallback& InCallback);
    
    /**
     * 获取玩家福缘数据
    */
    static constexpr uint64 GetChecklistData = 0x69641cbe9effe765LL; 
    typedef TSharedPtr<idlezt::GetChecklistDataReq> FZGetChecklistDataReqPtr;
    typedef TSharedPtr<idlezt::GetChecklistDataAck> FZGetChecklistDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetChecklistDataReqPtr&, const FZGetChecklistDataRspPtr&)> FZGetChecklistDataCallback;
    static void GetChecklistDataRegister(FMRpcManager* InManager, const FZGetChecklistDataCallback& InCallback);
    
    /**
     * 福缘功能操作
    */
    static constexpr uint64 ChecklistOp = 0x558d6e5b588450f0LL; 
    typedef TSharedPtr<idlezt::ChecklistOpReq> FZChecklistOpReqPtr;
    typedef TSharedPtr<idlezt::ChecklistOpAck> FZChecklistOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZChecklistOpReqPtr&, const FZChecklistOpRspPtr&)> FZChecklistOpCallback;
    static void ChecklistOpRegister(FMRpcManager* InManager, const FZChecklistOpCallback& InCallback);
    
    /**
     * 福缘任务进度更新
    */
    static constexpr uint64 UpdateChecklist = 0x36110b87da39e364LL; 
    typedef TSharedPtr<idlezt::UpdateChecklistReq> FZUpdateChecklistReqPtr;
    typedef TSharedPtr<idlezt::UpdateChecklistAck> FZUpdateChecklistRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUpdateChecklistReqPtr&, const FZUpdateChecklistRspPtr&)> FZUpdateChecklistCallback;
    static void UpdateChecklistRegister(FMRpcManager* InManager, const FZUpdateChecklistCallback& InCallback);
    
    /**
     * 请求论剑台状态
    */
    static constexpr uint64 GetSwordPkInfo = 0xb8eb5ae4385f3895LL; 
    typedef TSharedPtr<idlezt::GetSwordPkInfoReq> FZGetSwordPkInfoReqPtr;
    typedef TSharedPtr<idlezt::GetSwordPkInfoRsp> FZGetSwordPkInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSwordPkInfoReqPtr&, const FZGetSwordPkInfoRspPtr&)> FZGetSwordPkInfoCallback;
    static void GetSwordPkInfoRegister(FMRpcManager* InManager, const FZGetSwordPkInfoCallback& InCallback);
    
    /**
     * 注册论剑台
    */
    static constexpr uint64 SwordPkSignup = 0x2fdb58f4c56b545bLL; 
    typedef TSharedPtr<idlezt::SwordPkSignupReq> FZSwordPkSignupReqPtr;
    typedef TSharedPtr<idlezt::SwordPkSignupRsp> FZSwordPkSignupRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSwordPkSignupReqPtr&, const FZSwordPkSignupRspPtr&)> FZSwordPkSignupCallback;
    static void SwordPkSignupRegister(FMRpcManager* InManager, const FZSwordPkSignupCallback& InCallback);
    
    /**
     * 论剑台匹配
    */
    static constexpr uint64 SwordPkMatching = 0x3712ca7b0b83f3bcLL; 
    typedef TSharedPtr<idlezt::SwordPkMatchingReq> FZSwordPkMatchingReqPtr;
    typedef TSharedPtr<idlezt::SwordPkMatchingRsp> FZSwordPkMatchingRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSwordPkMatchingReqPtr&, const FZSwordPkMatchingRspPtr&)> FZSwordPkMatchingCallback;
    static void SwordPkMatchingRegister(FMRpcManager* InManager, const FZSwordPkMatchingCallback& InCallback);
    
    /**
     * 论剑台挑战
    */
    static constexpr uint64 SwordPkChallenge = 0xb5f26b7378c3f074LL; 
    typedef TSharedPtr<idlezt::SwordPkChallengeReq> FZSwordPkChallengeReqPtr;
    typedef TSharedPtr<idlezt::SwordPkChallengeRsp> FZSwordPkChallengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSwordPkChallengeReqPtr&, const FZSwordPkChallengeRspPtr&)> FZSwordPkChallengeCallback;
    static void SwordPkChallengeRegister(FMRpcManager* InManager, const FZSwordPkChallengeCallback& InCallback);
    
    /**
     * 论剑台复仇
    */
    static constexpr uint64 SwordPkRevenge = 0xeeb19a802b6249efLL; 
    typedef TSharedPtr<idlezt::SwordPkRevengeReq> FZSwordPkRevengeReqPtr;
    typedef TSharedPtr<idlezt::SwordPkRevengeRsp> FZSwordPkRevengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSwordPkRevengeReqPtr&, const FZSwordPkRevengeRspPtr&)> FZSwordPkRevengeCallback;
    static void SwordPkRevengeRegister(FMRpcManager* InManager, const FZSwordPkRevengeCallback& InCallback);
    
    /**
     * 获取论剑台排行榜
    */
    static constexpr uint64 GetSwordPkTopList = 0x3e046a742ccc214LL; 
    typedef TSharedPtr<idlezt::GetSwordPkTopListReq> FZGetSwordPkTopListReqPtr;
    typedef TSharedPtr<idlezt::GetSwordPkTopListRsp> FZGetSwordPkTopListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSwordPkTopListReqPtr&, const FZGetSwordPkTopListRspPtr&)> FZGetSwordPkTopListCallback;
    static void GetSwordPkTopListRegister(FMRpcManager* InManager, const FZGetSwordPkTopListCallback& InCallback);
    
    /**
     * 兑换英雄令
    */
    static constexpr uint64 SwordPkExchangeHeroCard = 0xb0dec6a520f32c38LL; 
    typedef TSharedPtr<idlezt::SwordPkExchangeHeroCardReq> FZSwordPkExchangeHeroCardReqPtr;
    typedef TSharedPtr<idlezt::SwordPkExchangeHeroCardRsp> FZSwordPkExchangeHeroCardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSwordPkExchangeHeroCardReqPtr&, const FZSwordPkExchangeHeroCardRspPtr&)> FZSwordPkExchangeHeroCardCallback;
    static void SwordPkExchangeHeroCardRegister(FMRpcManager* InManager, const FZSwordPkExchangeHeroCardCallback& InCallback);
    
    /**
     * 获取玩家通用道具兑换数据
    */
    static constexpr uint64 GetCommonItemExchangeData = 0x3383a3d7ca927280LL; 
    typedef TSharedPtr<idlezt::GetCommonItemExchangeDataReq> FZGetCommonItemExchangeDataReqPtr;
    typedef TSharedPtr<idlezt::GetCommonItemExchangeDataAck> FZGetCommonItemExchangeDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetCommonItemExchangeDataReqPtr&, const FZGetCommonItemExchangeDataRspPtr&)> FZGetCommonItemExchangeDataCallback;
    static void GetCommonItemExchangeDataRegister(FMRpcManager* InManager, const FZGetCommonItemExchangeDataCallback& InCallback);
    
    /**
     * 请求兑换通用道具
    */
    static constexpr uint64 ExchangeCommonItem = 0xc9b73248531ca352LL; 
    typedef TSharedPtr<idlezt::ExchangeCommonItemReq> FZExchangeCommonItemReqPtr;
    typedef TSharedPtr<idlezt::ExchangeCommonItemAck> FZExchangeCommonItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZExchangeCommonItemReqPtr&, const FZExchangeCommonItemRspPtr&)> FZExchangeCommonItemCallback;
    static void ExchangeCommonItemRegister(FMRpcManager* InManager, const FZExchangeCommonItemCallback& InCallback);
    
    /**
     * 请求合成通用道具
    */
    static constexpr uint64 SynthesisCommonItem = 0x4a055a8e185e6a3bLL; 
    typedef TSharedPtr<idlezt::SynthesisCommonItemReq> FZSynthesisCommonItemReqPtr;
    typedef TSharedPtr<idlezt::SynthesisCommonItemAck> FZSynthesisCommonItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSynthesisCommonItemReqPtr&, const FZSynthesisCommonItemRspPtr&)> FZSynthesisCommonItemCallback;
    static void SynthesisCommonItemRegister(FMRpcManager* InManager, const FZSynthesisCommonItemCallback& InCallback);
    
    /**
     * 请求可加入宗门列表
    */
    static constexpr uint64 GetCandidatesSeptList = 0x510b221d581c2e09LL; 
    typedef TSharedPtr<idlezt::GetCandidatesSeptListReq> FZGetCandidatesSeptListReqPtr;
    typedef TSharedPtr<idlezt::GetCandidatesSeptListAck> FZGetCandidatesSeptListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetCandidatesSeptListReqPtr&, const FZGetCandidatesSeptListRspPtr&)> FZGetCandidatesSeptListCallback;
    static void GetCandidatesSeptListRegister(FMRpcManager* InManager, const FZGetCandidatesSeptListCallback& InCallback);
    
    /**
     * 搜索宗门
    */
    static constexpr uint64 SearchSept = 0x9d9cecf416fec8a5LL; 
    typedef TSharedPtr<idlezt::SearchSeptReq> FZSearchSeptReqPtr;
    typedef TSharedPtr<idlezt::SearchSeptAck> FZSearchSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSearchSeptReqPtr&, const FZSearchSeptRspPtr&)> FZSearchSeptCallback;
    static void SearchSeptRegister(FMRpcManager* InManager, const FZSearchSeptCallback& InCallback);
    
    /**
     * 获取指定宗门基本信息
    */
    static constexpr uint64 GetSeptBaseInfo = 0x9a413b91b6d6e06aLL; 
    typedef TSharedPtr<idlezt::GetSeptBaseInfoReq> FZGetSeptBaseInfoReqPtr;
    typedef TSharedPtr<idlezt::GetSeptBaseInfoAck> FZGetSeptBaseInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptBaseInfoReqPtr&, const FZGetSeptBaseInfoRspPtr&)> FZGetSeptBaseInfoCallback;
    static void GetSeptBaseInfoRegister(FMRpcManager* InManager, const FZGetSeptBaseInfoCallback& InCallback);
    
    /**
     * 获取宗门成员列表
    */
    static constexpr uint64 GetSeptMemberList = 0xfbd07a4993fc8a09LL; 
    typedef TSharedPtr<idlezt::GetSeptMemberListReq> FZGetSeptMemberListReqPtr;
    typedef TSharedPtr<idlezt::GetSeptMemberListAck> FZGetSeptMemberListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptMemberListReqPtr&, const FZGetSeptMemberListRspPtr&)> FZGetSeptMemberListCallback;
    static void GetSeptMemberListRegister(FMRpcManager* InManager, const FZGetSeptMemberListCallback& InCallback);
    
    /**
     * 创建宗门
    */
    static constexpr uint64 CreateSept = 0x6ad34f3ed4c11399LL; 
    typedef TSharedPtr<idlezt::CreateSeptReq> FZCreateSeptReqPtr;
    typedef TSharedPtr<idlezt::CreateSeptAck> FZCreateSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZCreateSeptReqPtr&, const FZCreateSeptRspPtr&)> FZCreateSeptCallback;
    static void CreateSeptRegister(FMRpcManager* InManager, const FZCreateSeptCallback& InCallback);
    
    /**
     * 解散宗门
    */
    static constexpr uint64 DismissSept = 0xa452b3c17071d0abLL; 
    typedef TSharedPtr<idlezt::DismissSeptReq> FZDismissSeptReqPtr;
    typedef TSharedPtr<idlezt::DismissSeptAck> FZDismissSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDismissSeptReqPtr&, const FZDismissSeptRspPtr&)> FZDismissSeptCallback;
    static void DismissSeptRegister(FMRpcManager* InManager, const FZDismissSeptCallback& InCallback);
    
    /**
     * 离开宗门
    */
    static constexpr uint64 ExitSept = 0x484a5672fc238a1LL; 
    typedef TSharedPtr<idlezt::ExitSeptReq> FZExitSeptReqPtr;
    typedef TSharedPtr<idlezt::ExitSeptAck> FZExitSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZExitSeptReqPtr&, const FZExitSeptRspPtr&)> FZExitSeptCallback;
    static void ExitSeptRegister(FMRpcManager* InManager, const FZExitSeptCallback& InCallback);
    
    /**
     * 申请加入宗门
    */
    static constexpr uint64 ApplyJoinSept = 0x4ba4a170d63e13d3LL; 
    typedef TSharedPtr<idlezt::ApplyJoinSeptReq> FZApplyJoinSeptReqPtr;
    typedef TSharedPtr<idlezt::ApplyJoinSeptAck> FZApplyJoinSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZApplyJoinSeptReqPtr&, const FZApplyJoinSeptRspPtr&)> FZApplyJoinSeptCallback;
    static void ApplyJoinSeptRegister(FMRpcManager* InManager, const FZApplyJoinSeptCallback& InCallback);
    
    /**
     * 审批入宗请求
    */
    static constexpr uint64 ApproveApplySept = 0x1c4ec23c92ce69e6LL; 
    typedef TSharedPtr<idlezt::ApproveApplySeptReq> FZApproveApplySeptReqPtr;
    typedef TSharedPtr<idlezt::ApproveApplySeptAck> FZApproveApplySeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZApproveApplySeptReqPtr&, const FZApproveApplySeptRspPtr&)> FZApproveApplySeptCallback;
    static void ApproveApplySeptRegister(FMRpcManager* InManager, const FZApproveApplySeptCallback& InCallback);
    
    /**
     * 获取入宗申请列表
    */
    static constexpr uint64 GetApplyJoinSeptList = 0xf4f66337cdd8c185LL; 
    typedef TSharedPtr<idlezt::GetApplyJoinSeptListReq> FZGetApplyJoinSeptListReqPtr;
    typedef TSharedPtr<idlezt::GetApplyJoinSeptListAck> FZGetApplyJoinSeptListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetApplyJoinSeptListReqPtr&, const FZGetApplyJoinSeptListRspPtr&)> FZGetApplyJoinSeptListCallback;
    static void GetApplyJoinSeptListRegister(FMRpcManager* InManager, const FZGetApplyJoinSeptListCallback& InCallback);
    
    /**
     * 回复入宗邀请
    */
    static constexpr uint64 RespondInviteSept = 0xd1b23f3cbe4068cfLL; 
    typedef TSharedPtr<idlezt::RespondInviteSeptReq> FZRespondInviteSeptReqPtr;
    typedef TSharedPtr<idlezt::RespondInviteSeptAck> FZRespondInviteSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRespondInviteSeptReqPtr&, const FZRespondInviteSeptRspPtr&)> FZRespondInviteSeptCallback;
    static void RespondInviteSeptRegister(FMRpcManager* InManager, const FZRespondInviteSeptCallback& InCallback);
    
    /**
     * 获取邀请我入宗的宗门列表
    */
    static constexpr uint64 GetInviteMeJoinSeptList = 0xaf177c8530fdc118LL; 
    typedef TSharedPtr<idlezt::GetInviteMeJoinSeptListReq> FZGetInviteMeJoinSeptListReqPtr;
    typedef TSharedPtr<idlezt::GetInviteMeJoinSeptListAck> FZGetInviteMeJoinSeptListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetInviteMeJoinSeptListReqPtr&, const FZGetInviteMeJoinSeptListRspPtr&)> FZGetInviteMeJoinSeptListCallback;
    static void GetInviteMeJoinSeptListRegister(FMRpcManager* InManager, const FZGetInviteMeJoinSeptListCallback& InCallback);
    
    /**
     * 获取可邀请入宗玩家列表
    */
    static constexpr uint64 GetCandidatesInviteRoleList = 0xbed8536cd7992d6cLL; 
    typedef TSharedPtr<idlezt::GetCandidatesInviteRoleListReq> FZGetCandidatesInviteRoleListReqPtr;
    typedef TSharedPtr<idlezt::GetCandidatesInviteRoleListAck> FZGetCandidatesInviteRoleListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetCandidatesInviteRoleListReqPtr&, const FZGetCandidatesInviteRoleListRspPtr&)> FZGetCandidatesInviteRoleListCallback;
    static void GetCandidatesInviteRoleListRegister(FMRpcManager* InManager, const FZGetCandidatesInviteRoleListCallback& InCallback);
    
    /**
     * 邀请加入宗门
    */
    static constexpr uint64 InviteJoinSept = 0xfad2eee8c751eb38LL; 
    typedef TSharedPtr<idlezt::InviteJoinSeptReq> FZInviteJoinSeptReqPtr;
    typedef TSharedPtr<idlezt::InviteJoinSeptAck> FZInviteJoinSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZInviteJoinSeptReqPtr&, const FZInviteJoinSeptRspPtr&)> FZInviteJoinSeptCallback;
    static void InviteJoinSeptRegister(FMRpcManager* InManager, const FZInviteJoinSeptCallback& InCallback);
    
    /**
     * 设置宗门设置
    */
    static constexpr uint64 SetSeptSettings = 0xf95733aaeed10a06LL; 
    typedef TSharedPtr<idlezt::SetSeptSettingsReq> FZSetSeptSettingsReqPtr;
    typedef TSharedPtr<idlezt::SetSeptSettingsAck> FZSetSeptSettingsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetSeptSettingsReqPtr&, const FZSetSeptSettingsRspPtr&)> FZSetSeptSettingsCallback;
    static void SetSeptSettingsRegister(FMRpcManager* InManager, const FZSetSeptSettingsCallback& InCallback);
    
    /**
     * 设置宗门公告
    */
    static constexpr uint64 SetSeptAnnounce = 0x794207b8821b7a4LL; 
    typedef TSharedPtr<idlezt::SetSeptAnnounceReq> FZSetSeptAnnounceReqPtr;
    typedef TSharedPtr<idlezt::SetSeptAnnounceAck> FZSetSeptAnnounceRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZSetSeptAnnounceReqPtr&, const FZSetSeptAnnounceRspPtr&)> FZSetSeptAnnounceCallback;
    static void SetSeptAnnounceRegister(FMRpcManager* InManager, const FZSetSeptAnnounceCallback& InCallback);
    
    /**
     * 宗门改名
    */
    static constexpr uint64 ChangeSeptName = 0x90d8fbf8ded2ae12LL; 
    typedef TSharedPtr<idlezt::ChangeSeptNameReq> FZChangeSeptNameReqPtr;
    typedef TSharedPtr<idlezt::ChangeSeptNameAck> FZChangeSeptNameRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZChangeSeptNameReqPtr&, const FZChangeSeptNameRspPtr&)> FZChangeSeptNameCallback;
    static void ChangeSeptNameRegister(FMRpcManager* InManager, const FZChangeSeptNameCallback& InCallback);
    
    /**
     * 请求宗门日志
    */
    static constexpr uint64 GetSeptLog = 0x6c2aeb2ebe2101ffLL; 
    typedef TSharedPtr<idlezt::GetSeptLogReq> FZGetSeptLogReqPtr;
    typedef TSharedPtr<idlezt::GetSeptLogAck> FZGetSeptLogRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptLogReqPtr&, const FZGetSeptLogRspPtr&)> FZGetSeptLogCallback;
    static void GetSeptLogRegister(FMRpcManager* InManager, const FZGetSeptLogCallback& InCallback);
    
    /**
     * 宗门建设
    */
    static constexpr uint64 ConstructSept = 0xe9d4b4fdfec4bb46LL; 
    typedef TSharedPtr<idlezt::ConstructSeptReq> FZConstructSeptReqPtr;
    typedef TSharedPtr<idlezt::ConstructSeptAck> FZConstructSeptRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZConstructSeptReqPtr&, const FZConstructSeptRspPtr&)> FZConstructSeptCallback;
    static void ConstructSeptRegister(FMRpcManager* InManager, const FZConstructSeptCallback& InCallback);
    
    /**
     * 获取宗门建设记录
    */
    static constexpr uint64 GetConstructSeptLog = 0x31ecdd68342eb88aLL; 
    typedef TSharedPtr<idlezt::GetConstructSeptLogReq> FZGetConstructSeptLogReqPtr;
    typedef TSharedPtr<idlezt::GetConstructSeptLogAck> FZGetConstructSeptLogRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetConstructSeptLogReqPtr&, const FZGetConstructSeptLogRspPtr&)> FZGetConstructSeptLogCallback;
    static void GetConstructSeptLogRegister(FMRpcManager* InManager, const FZGetConstructSeptLogCallback& InCallback);
    
    /**
     * 获取角色每日已邀请入宗次数
    */
    static constexpr uint64 GetSeptInvitedRoleDailyNum = 0x50e4bf8d66f465adLL; 
    typedef TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumReq> FZGetSeptInvitedRoleDailyNumReqPtr;
    typedef TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumAck> FZGetSeptInvitedRoleDailyNumRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptInvitedRoleDailyNumReqPtr&, const FZGetSeptInvitedRoleDailyNumRspPtr&)> FZGetSeptInvitedRoleDailyNumCallback;
    static void GetSeptInvitedRoleDailyNumRegister(FMRpcManager* InManager, const FZGetSeptInvitedRoleDailyNumCallback& InCallback);
    
    /**
     * 任命职位
    */
    static constexpr uint64 AppointSeptPosition = 0xda8603ce114f9f79LL; 
    typedef TSharedPtr<idlezt::AppointSeptPositionReq> FZAppointSeptPositionReqPtr;
    typedef TSharedPtr<idlezt::AppointSeptPositionAck> FZAppointSeptPositionRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppointSeptPositionReqPtr&, const FZAppointSeptPositionRspPtr&)> FZAppointSeptPositionCallback;
    static void AppointSeptPositionRegister(FMRpcManager* InManager, const FZAppointSeptPositionCallback& InCallback);
    
    /**
     * 转让宗主
    */
    static constexpr uint64 ResignSeptChairman = 0x77751e7cf40b9c5eLL; 
    typedef TSharedPtr<idlezt::ResignSeptChairmanReq> FZResignSeptChairmanReqPtr;
    typedef TSharedPtr<idlezt::ResignSeptChairmanAck> FZResignSeptChairmanRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZResignSeptChairmanReqPtr&, const FZResignSeptChairmanRspPtr&)> FZResignSeptChairmanCallback;
    static void ResignSeptChairmanRegister(FMRpcManager* InManager, const FZResignSeptChairmanCallback& InCallback);
    
    /**
     * 开除宗门成员
    */
    static constexpr uint64 KickOutSeptMember = 0x33bcdc17ab5f55cbLL; 
    typedef TSharedPtr<idlezt::KickOutSeptMemberReq> FZKickOutSeptMemberReqPtr;
    typedef TSharedPtr<idlezt::KickOutSeptMemberAck> FZKickOutSeptMemberRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZKickOutSeptMemberReqPtr&, const FZKickOutSeptMemberRspPtr&)> FZKickOutSeptMemberCallback;
    static void KickOutSeptMemberRegister(FMRpcManager* InManager, const FZKickOutSeptMemberCallback& InCallback);
    
    /**
     * 请求玩家宗门商店数据
    */
    static constexpr uint64 GetRoleSeptShopData = 0xb0f50b7a14b2778fLL; 
    typedef TSharedPtr<idlezt::GetRoleSeptShopDataReq> FZGetRoleSeptShopDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleSeptShopDataAck> FZGetRoleSeptShopDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleSeptShopDataReqPtr&, const FZGetRoleSeptShopDataRspPtr&)> FZGetRoleSeptShopDataCallback;
    static void GetRoleSeptShopDataRegister(FMRpcManager* InManager, const FZGetRoleSeptShopDataCallback& InCallback);
    
    /**
     * 请求兑换宗门商店道具
    */
    static constexpr uint64 BuySeptShopItem = 0xeaefbf205a52a328LL; 
    typedef TSharedPtr<idlezt::BuySeptShopItemReq> FZBuySeptShopItemReqPtr;
    typedef TSharedPtr<idlezt::BuySeptShopItemAck> FZBuySeptShopItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZBuySeptShopItemReqPtr&, const FZBuySeptShopItemRspPtr&)> FZBuySeptShopItemCallback;
    static void BuySeptShopItemRegister(FMRpcManager* InManager, const FZBuySeptShopItemCallback& InCallback);
    
    /**
     * 请求玩家宗门事务数据
    */
    static constexpr uint64 GetRoleSeptQuestData = 0xec24c957ca1bd293LL; 
    typedef TSharedPtr<idlezt::GetRoleSeptQuestDataReq> FZGetRoleSeptQuestDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleSeptQuestDataAck> FZGetRoleSeptQuestDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleSeptQuestDataReqPtr&, const FZGetRoleSeptQuestDataRspPtr&)> FZGetRoleSeptQuestDataCallback;
    static void GetRoleSeptQuestDataRegister(FMRpcManager* InManager, const FZGetRoleSeptQuestDataCallback& InCallback);
    
    /**
     * 玩家宗门事务操作
    */
    static constexpr uint64 ReqRoleSeptQuestOp = 0x8c538b51f2613710LL; 
    typedef TSharedPtr<idlezt::ReqRoleSeptQuestOpReq> FZReqRoleSeptQuestOpReqPtr;
    typedef TSharedPtr<idlezt::ReqRoleSeptQuestOpAck> FZReqRoleSeptQuestOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReqRoleSeptQuestOpReqPtr&, const FZReqRoleSeptQuestOpRspPtr&)> FZReqRoleSeptQuestOpCallback;
    static void ReqRoleSeptQuestOpRegister(FMRpcManager* InManager, const FZReqRoleSeptQuestOpCallback& InCallback);
    
    /**
     * 玩家宗门事务手动刷新
    */
    static constexpr uint64 RefreshSeptQuest = 0xf9265a4d103271ceLL; 
    typedef TSharedPtr<idlezt::RefreshSeptQuestReq> FZRefreshSeptQuestReqPtr;
    typedef TSharedPtr<idlezt::RefreshSeptQuestAck> FZRefreshSeptQuestRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRefreshSeptQuestReqPtr&, const FZRefreshSeptQuestRspPtr&)> FZRefreshSeptQuestCallback;
    static void RefreshSeptQuestRegister(FMRpcManager* InManager, const FZRefreshSeptQuestCallback& InCallback);
    
    /**
     * 玩家宗门事务升级
    */
    static constexpr uint64 ReqSeptQuestRankUp = 0xdadee3c2caac71e4LL; 
    typedef TSharedPtr<idlezt::ReqSeptQuestRankUpReq> FZReqSeptQuestRankUpReqPtr;
    typedef TSharedPtr<idlezt::ReqSeptQuestRankUpAck> FZReqSeptQuestRankUpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReqSeptQuestRankUpReqPtr&, const FZReqSeptQuestRankUpRspPtr&)> FZReqSeptQuestRankUpCallback;
    static void ReqSeptQuestRankUpRegister(FMRpcManager* InManager, const FZReqSeptQuestRankUpCallback& InCallback);
    
    /**
     * 开始占据中立秘镜矿脉
    */
    static constexpr uint64 BeginOccupySeptStone = 0x5344a87edb176adcLL; 
    typedef TSharedPtr<idlezt::BeginOccupySeptStoneReq> FZBeginOccupySeptStoneReqPtr;
    typedef TSharedPtr<idlezt::BeginOccupySeptStoneAck> FZBeginOccupySeptStoneRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZBeginOccupySeptStoneReqPtr&, const FZBeginOccupySeptStoneRspPtr&)> FZBeginOccupySeptStoneCallback;
    static void BeginOccupySeptStoneRegister(FMRpcManager* InManager, const FZBeginOccupySeptStoneCallback& InCallback);
    
    /**
     * 结束占领中立秘镜矿脉
    */
    static constexpr uint64 EndOccupySeptStone = 0xf3cbd99650113df0LL; 
    typedef TSharedPtr<idlezt::EndOccupySeptStoneReq> FZEndOccupySeptStoneReqPtr;
    typedef TSharedPtr<idlezt::EndOccupySeptStoneAck> FZEndOccupySeptStoneRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZEndOccupySeptStoneReqPtr&, const FZEndOccupySeptStoneRspPtr&)> FZEndOccupySeptStoneCallback;
    static void EndOccupySeptStoneRegister(FMRpcManager* InManager, const FZEndOccupySeptStoneCallback& InCallback);
    
    /**
     * 占领宗门领地
    */
    static constexpr uint64 OccupySeptLand = 0x4a2720b08c0428f5LL; 
    typedef TSharedPtr<idlezt::OccupySeptLandReq> FZOccupySeptLandReqPtr;
    typedef TSharedPtr<idlezt::OccupySeptLandAck> FZOccupySeptLandRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOccupySeptLandReqPtr&, const FZOccupySeptLandRspPtr&)> FZOccupySeptLandCallback;
    static void OccupySeptLandRegister(FMRpcManager* InManager, const FZOccupySeptLandCallback& InCallback);
    
    /**
     * 获取功法数据
    */
    static constexpr uint64 GetGongFaData = 0x7dee5a5e4fc256b7LL; 
    typedef TSharedPtr<idlezt::GetGongFaDataReq> FZGetGongFaDataReqPtr;
    typedef TSharedPtr<idlezt::GetGongFaDataAck> FZGetGongFaDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetGongFaDataReqPtr&, const FZGetGongFaDataRspPtr&)> FZGetGongFaDataCallback;
    static void GetGongFaDataRegister(FMRpcManager* InManager, const FZGetGongFaDataCallback& InCallback);
    
    /**
     * 功法操作：领悟 | 激活 | 升级
    */
    static constexpr uint64 GongFaOp = 0x10dab8900f5a84beLL; 
    typedef TSharedPtr<idlezt::GongFaOpReq> FZGongFaOpReqPtr;
    typedef TSharedPtr<idlezt::GongFaOpAck> FZGongFaOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGongFaOpReqPtr&, const FZGongFaOpRspPtr&)> FZGongFaOpCallback;
    static void GongFaOpRegister(FMRpcManager* InManager, const FZGongFaOpCallback& InCallback);
    
    /**
     * 激活功法圆满效果
    */
    static constexpr uint64 ActivateGongFaMaxEffect = 0x7707df59df00fadbLL; 
    typedef TSharedPtr<idlezt::ActivateGongFaMaxEffectReq> FZActivateGongFaMaxEffectReqPtr;
    typedef TSharedPtr<idlezt::ActivateGongFaMaxEffectAck> FZActivateGongFaMaxEffectRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZActivateGongFaMaxEffectReqPtr&, const FZActivateGongFaMaxEffectRspPtr&)> FZActivateGongFaMaxEffectCallback;
    static void ActivateGongFaMaxEffectRegister(FMRpcManager* InManager, const FZActivateGongFaMaxEffectCallback& InCallback);
    
    /**
     * 获取宗门领地伤害排行榜
    */
    static constexpr uint64 GetSeptLandDamageTopList = 0xca13063d68adaa1aLL; 
    typedef TSharedPtr<idlezt::GetSeptLandDamageTopListReq> FZGetSeptLandDamageTopListReqPtr;
    typedef TSharedPtr<idlezt::GetSeptLandDamageTopListAck> FZGetSeptLandDamageTopListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptLandDamageTopListReqPtr&, const FZGetSeptLandDamageTopListRspPtr&)> FZGetSeptLandDamageTopListCallback;
    static void GetSeptLandDamageTopListRegister(FMRpcManager* InManager, const FZGetSeptLandDamageTopListCallback& InCallback);
    
    /**
     * 领取福赠奖励
    */
    static constexpr uint64 ReceiveFuZengRewards = 0xc62d2a5f7e9cf8fLL; 
    typedef TSharedPtr<idlezt::ReceiveFuZengRewardsReq> FZReceiveFuZengRewardsReqPtr;
    typedef TSharedPtr<idlezt::ReceiveFuZengRewardsAck> FZReceiveFuZengRewardsRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReceiveFuZengRewardsReqPtr&, const FZReceiveFuZengRewardsRspPtr&)> FZReceiveFuZengRewardsCallback;
    static void ReceiveFuZengRewardsRegister(FMRpcManager* InManager, const FZReceiveFuZengRewardsCallback& InCallback);
    
    /**
     * 获取福赠数据
    */
    static constexpr uint64 GetRoleFuZengData = 0x960958081233e0d0LL; 
    typedef TSharedPtr<idlezt::GetRoleFuZengDataReq> FZGetRoleFuZengDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleFuZengDataAck> FZGetRoleFuZengDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleFuZengDataReqPtr&, const FZGetRoleFuZengDataRspPtr&)> FZGetRoleFuZengDataCallback;
    static void GetRoleFuZengDataRegister(FMRpcManager* InManager, const FZGetRoleFuZengDataCallback& InCallback);
    
    /**
     * 获取宝藏阁数据
    */
    static constexpr uint64 GetRoleTreasuryData = 0x71e3ee5dc8e8e82aLL; 
    typedef TSharedPtr<idlezt::GetRoleTreasuryDataReq> FZGetRoleTreasuryDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleTreasuryDataAck> FZGetRoleTreasuryDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleTreasuryDataReqPtr&, const FZGetRoleTreasuryDataRspPtr&)> FZGetRoleTreasuryDataCallback;
    static void GetRoleTreasuryDataRegister(FMRpcManager* InManager, const FZGetRoleTreasuryDataCallback& InCallback);
    
    /**
     * 请求开箱
    */
    static constexpr uint64 OpenTreasuryChest = 0xe949d561f5854519LL; 
    typedef TSharedPtr<idlezt::OpenTreasuryChestReq> FZOpenTreasuryChestReqPtr;
    typedef TSharedPtr<idlezt::OpenTreasuryChestAck> FZOpenTreasuryChestRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOpenTreasuryChestReqPtr&, const FZOpenTreasuryChestRspPtr&)> FZOpenTreasuryChestCallback;
    static void OpenTreasuryChestRegister(FMRpcManager* InManager, const FZOpenTreasuryChestCallback& InCallback);
    
    /**
     * 请求一键全开箱
    */
    static constexpr uint64 OneClickOpenTreasuryChest = 0x7fc331a2392a8eddLL; 
    typedef TSharedPtr<idlezt::OneClickOpenTreasuryChestReq> FZOneClickOpenTreasuryChestReqPtr;
    typedef TSharedPtr<idlezt::OneClickOpenTreasuryChestAck> FZOneClickOpenTreasuryChestRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOneClickOpenTreasuryChestReqPtr&, const FZOneClickOpenTreasuryChestRspPtr&)> FZOneClickOpenTreasuryChestCallback;
    static void OneClickOpenTreasuryChestRegister(FMRpcManager* InManager, const FZOneClickOpenTreasuryChestCallback& InCallback);
    
    /**
     * 请求探索卡池
    */
    static constexpr uint64 OpenTreasuryGacha = 0xb261f9c9db82f1d6LL; 
    typedef TSharedPtr<idlezt::OpenTreasuryGachaReq> FZOpenTreasuryGachaReqPtr;
    typedef TSharedPtr<idlezt::OpenTreasuryGachaAck> FZOpenTreasuryGachaRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZOpenTreasuryGachaReqPtr&, const FZOpenTreasuryGachaRspPtr&)> FZOpenTreasuryGachaCallback;
    static void OpenTreasuryGachaRegister(FMRpcManager* InManager, const FZOpenTreasuryGachaCallback& InCallback);
    
    /**
     * 请求刷新古修商店
    */
    static constexpr uint64 RefreshTreasuryShop = 0xccc805a6a1964f45LL; 
    typedef TSharedPtr<idlezt::RefreshTreasuryShopReq> FZRefreshTreasuryShopReqPtr;
    typedef TSharedPtr<idlezt::RefreshTreasuryShopAck> FZRefreshTreasuryShopRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRefreshTreasuryShopReqPtr&, const FZRefreshTreasuryShopRspPtr&)> FZRefreshTreasuryShopCallback;
    static void RefreshTreasuryShopRegister(FMRpcManager* InManager, const FZRefreshTreasuryShopCallback& InCallback);
    
    /**
     * 请求古修商店中购买
    */
    static constexpr uint64 TreasuryShopBuy = 0xd3d0c8b54d5a8c4cLL; 
    typedef TSharedPtr<idlezt::TreasuryShopBuyReq> FZTreasuryShopBuyReqPtr;
    typedef TSharedPtr<idlezt::TreasuryShopBuyAck> FZTreasuryShopBuyRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZTreasuryShopBuyReqPtr&, const FZTreasuryShopBuyRspPtr&)> FZTreasuryShopBuyCallback;
    static void TreasuryShopBuyRegister(FMRpcManager* InManager, const FZTreasuryShopBuyCallback& InCallback);
    
    /**
     * 获取生涯计数器数据
    */
    static constexpr uint64 GetLifeCounterData = 0xfab957c01bab33e3LL; 
    typedef TSharedPtr<idlezt::GetLifeCounterDataReq> FZGetLifeCounterDataReqPtr;
    typedef TSharedPtr<idlezt::GetLifeCounterDataAck> FZGetLifeCounterDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetLifeCounterDataReqPtr&, const FZGetLifeCounterDataRspPtr&)> FZGetLifeCounterDataCallback;
    static void GetLifeCounterDataRegister(FMRpcManager* InManager, const FZGetLifeCounterDataCallback& InCallback);
    
    /**
     * 进行任务对战
    */
    static constexpr uint64 DoQuestFight = 0xac7be8cfdcc47890LL; 
    typedef TSharedPtr<idlezt::DoQuestFightReq> FZDoQuestFightReqPtr;
    typedef TSharedPtr<idlezt::DoQuestFightAck> FZDoQuestFightRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDoQuestFightReqPtr&, const FZDoQuestFightRspPtr&)> FZDoQuestFightCallback;
    static void DoQuestFightRegister(FMRpcManager* InManager, const FZDoQuestFightCallback& InCallback);
    
    /**
     * 结束任务对战
    */
    static constexpr uint64 QuestFightQuickEnd = 0x46cdfcd6a2fafb9dLL; 
    typedef TSharedPtr<idlezt::QuestFightQuickEndReq> FZQuestFightQuickEndReqPtr;
    typedef TSharedPtr<idlezt::QuestFightQuickEndAck> FZQuestFightQuickEndRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZQuestFightQuickEndReqPtr&, const FZQuestFightQuickEndRspPtr&)> FZQuestFightQuickEndCallback;
    static void QuestFightQuickEndRegister(FMRpcManager* InManager, const FZQuestFightQuickEndCallback& InCallback);
    
    /**
     * 请求外观数据
    */
    static constexpr uint64 GetAppearanceData = 0xc70ca416f2bc599dLL; 
    typedef TSharedPtr<idlezt::GetAppearanceDataReq> FZGetAppearanceDataReqPtr;
    typedef TSharedPtr<idlezt::GetAppearanceDataAck> FZGetAppearanceDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetAppearanceDataReqPtr&, const FZGetAppearanceDataRspPtr&)> FZGetAppearanceDataCallback;
    static void GetAppearanceDataRegister(FMRpcManager* InManager, const FZGetAppearanceDataCallback& InCallback);
    
    /**
     * 请求添加外观（使用包含外观的礼包道具）
    */
    static constexpr uint64 AppearanceAdd = 0x92d686486aced0daLL; 
    typedef TSharedPtr<idlezt::AppearanceAddReq> FZAppearanceAddReqPtr;
    typedef TSharedPtr<idlezt::AppearanceAddAck> FZAppearanceAddRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppearanceAddReqPtr&, const FZAppearanceAddRspPtr&)> FZAppearanceAddCallback;
    static void AppearanceAddRegister(FMRpcManager* InManager, const FZAppearanceAddCallback& InCallback);
    
    /**
     * 请求激活外观
    */
    static constexpr uint64 AppearanceActive = 0xa6e9f3dfc8728dc1LL; 
    typedef TSharedPtr<idlezt::AppearanceActiveReq> FZAppearanceActiveReqPtr;
    typedef TSharedPtr<idlezt::AppearanceActiveAck> FZAppearanceActiveRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppearanceActiveReqPtr&, const FZAppearanceActiveRspPtr&)> FZAppearanceActiveCallback;
    static void AppearanceActiveRegister(FMRpcManager* InManager, const FZAppearanceActiveCallback& InCallback);
    
    /**
     * 请求穿戴外观
    */
    static constexpr uint64 AppearanceWear = 0x70064954dfa916a0LL; 
    typedef TSharedPtr<idlezt::AppearanceWearReq> FZAppearanceWearReqPtr;
    typedef TSharedPtr<idlezt::AppearanceWearAck> FZAppearanceWearRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppearanceWearReqPtr&, const FZAppearanceWearRspPtr&)> FZAppearanceWearCallback;
    static void AppearanceWearRegister(FMRpcManager* InManager, const FZAppearanceWearCallback& InCallback);
    
    /**
     * 请求外观商店购买
    */
    static constexpr uint64 AppearanceBuy = 0x8b1e4a4866b3bfddLL; 
    typedef TSharedPtr<idlezt::AppearanceBuyReq> FZAppearanceBuyReqPtr;
    typedef TSharedPtr<idlezt::AppearanceBuyAck> FZAppearanceBuyRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppearanceBuyReqPtr&, const FZAppearanceBuyRspPtr&)> FZAppearanceBuyCallback;
    static void AppearanceBuyRegister(FMRpcManager* InManager, const FZAppearanceBuyCallback& InCallback);
    
    /**
     * 请求修改外形
    */
    static constexpr uint64 AppearanceChangeSkType = 0x6823db11bbcdf4e1LL; 
    typedef TSharedPtr<idlezt::AppearanceChangeSkTypeReq> FZAppearanceChangeSkTypeReqPtr;
    typedef TSharedPtr<idlezt::AppearanceChangeSkTypeAck> FZAppearanceChangeSkTypeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAppearanceChangeSkTypeReqPtr&, const FZAppearanceChangeSkTypeRspPtr&)> FZAppearanceChangeSkTypeCallback;
    static void AppearanceChangeSkTypeRegister(FMRpcManager* InManager, const FZAppearanceChangeSkTypeCallback& InCallback);
    
    /**
     * 请求指定战斗信息
    */
    static constexpr uint64 GetBattleHistoryInfo = 0x71c46aee8f5948dbLL; 
    typedef TSharedPtr<idlezt::GetBattleHistoryInfoReq> FZGetBattleHistoryInfoReqPtr;
    typedef TSharedPtr<idlezt::GetBattleHistoryInfoAck> FZGetBattleHistoryInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetBattleHistoryInfoReqPtr&, const FZGetBattleHistoryInfoRspPtr&)> FZGetBattleHistoryInfoCallback;
    static void GetBattleHistoryInfoRegister(FMRpcManager* InManager, const FZGetBattleHistoryInfoCallback& InCallback);
    
    /**
     * 请求秘境探索数据
    */
    static constexpr uint64 GetArenaCheckListData = 0x360fe43e0b7f9c92LL; 
    typedef TSharedPtr<idlezt::GetArenaCheckListDataReq> FZGetArenaCheckListDataReqPtr;
    typedef TSharedPtr<idlezt::GetArenaCheckListDataAck> FZGetArenaCheckListDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetArenaCheckListDataReqPtr&, const FZGetArenaCheckListDataRspPtr&)> FZGetArenaCheckListDataCallback;
    static void GetArenaCheckListDataRegister(FMRpcManager* InManager, const FZGetArenaCheckListDataCallback& InCallback);
    
    /**
     * 请求提交秘境探索事件
    */
    static constexpr uint64 ArenaCheckListSubmit = 0xeb4b4917e8b21730LL; 
    typedef TSharedPtr<idlezt::ArenaCheckListSubmitReq> FZArenaCheckListSubmitReqPtr;
    typedef TSharedPtr<idlezt::ArenaCheckListSubmitAck> FZArenaCheckListSubmitRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZArenaCheckListSubmitReqPtr&, const FZArenaCheckListSubmitRspPtr&)> FZArenaCheckListSubmitCallback;
    static void ArenaCheckListSubmitRegister(FMRpcManager* InManager, const FZArenaCheckListSubmitCallback& InCallback);
    
    /**
     * 请求提交秘境探索奖励
    */
    static constexpr uint64 ArenaCheckListRewardSubmit = 0xaf040e1b455f3637LL; 
    typedef TSharedPtr<idlezt::ArenaCheckListRewardSubmitReq> FZArenaCheckListRewardSubmitReqPtr;
    typedef TSharedPtr<idlezt::ArenaCheckListRewardSubmitAck> FZArenaCheckListRewardSubmitRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZArenaCheckListRewardSubmitReqPtr&, const FZArenaCheckListRewardSubmitRspPtr&)> FZArenaCheckListRewardSubmitCallback;
    static void ArenaCheckListRewardSubmitRegister(FMRpcManager* InManager, const FZArenaCheckListRewardSubmitCallback& InCallback);
    
    /**
     * 请求开启剿灭副本
    */
    static constexpr uint64 DungeonKillAllChallenge = 0x4d6459cb13ce3401LL; 
    typedef TSharedPtr<idlezt::DungeonKillAllChallengeReq> FZDungeonKillAllChallengeReqPtr;
    typedef TSharedPtr<idlezt::DungeonKillAllChallengeAck> FZDungeonKillAllChallengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonKillAllChallengeReqPtr&, const FZDungeonKillAllChallengeRspPtr&)> FZDungeonKillAllChallengeCallback;
    static void DungeonKillAllChallengeRegister(FMRpcManager* InManager, const FZDungeonKillAllChallengeCallback& InCallback);
    
    /**
     * 请求剿灭副本快速结束
    */
    static constexpr uint64 DungeonKillAllQuickEnd = 0xccd20f58200c04bcLL; 
    typedef TSharedPtr<idlezt::DungeonKillAllQuickEndReq> FZDungeonKillAllQuickEndReqPtr;
    typedef TSharedPtr<idlezt::DungeonKillAllQuickEndAck> FZDungeonKillAllQuickEndRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonKillAllQuickEndReqPtr&, const FZDungeonKillAllQuickEndRspPtr&)> FZDungeonKillAllQuickEndCallback;
    static void DungeonKillAllQuickEndRegister(FMRpcManager* InManager, const FZDungeonKillAllQuickEndCallback& InCallback);
    
    /**
     * 询问剿灭副本是否完成
    */
    static constexpr uint64 DungeonKillAllData = 0xd5a108b49cd8f640LL; 
    typedef TSharedPtr<idlezt::DungeonKillAllDataReq> FZDungeonKillAllDataReqPtr;
    typedef TSharedPtr<idlezt::DungeonKillAllDataAck> FZDungeonKillAllDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonKillAllDataReqPtr&, const FZDungeonKillAllDataRspPtr&)> FZDungeonKillAllDataCallback;
    static void DungeonKillAllDataRegister(FMRpcManager* InManager, const FZDungeonKillAllDataCallback& InCallback);
    
    /**
     * 药园数据请求
    */
    static constexpr uint64 GetFarmlandData = 0xca9224589386601eLL; 
    typedef TSharedPtr<idlezt::GetFarmlandDataReq> FZGetFarmlandDataReqPtr;
    typedef TSharedPtr<idlezt::GetFarmlandDataAck> FZGetFarmlandDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetFarmlandDataReqPtr&, const FZGetFarmlandDataRspPtr&)> FZGetFarmlandDataCallback;
    static void GetFarmlandDataRegister(FMRpcManager* InManager, const FZGetFarmlandDataCallback& InCallback);
    
    /**
     * 药园地块解锁
    */
    static constexpr uint64 FarmlandUnlockBlock = 0xb1007768d3f2de0dLL; 
    typedef TSharedPtr<idlezt::FarmlandUnlockBlockReq> FZFarmlandUnlockBlockReqPtr;
    typedef TSharedPtr<idlezt::FarmlandUnlockBlockAck> FZFarmlandUnlockBlockRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandUnlockBlockReqPtr&, const FZFarmlandUnlockBlockRspPtr&)> FZFarmlandUnlockBlockCallback;
    static void FarmlandUnlockBlockRegister(FMRpcManager* InManager, const FZFarmlandUnlockBlockCallback& InCallback);
    
    /**
     * 药园种植或铲除
    */
    static constexpr uint64 FarmlandPlantSeed = 0xc674741d15bab974LL; 
    typedef TSharedPtr<idlezt::FarmlandPlantSeedReq> FZFarmlandPlantSeedReqPtr;
    typedef TSharedPtr<idlezt::FarmlandPlantSeedAck> FZFarmlandPlantSeedRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandPlantSeedReqPtr&, const FZFarmlandPlantSeedRspPtr&)> FZFarmlandPlantSeedCallback;
    static void FarmlandPlantSeedRegister(FMRpcManager* InManager, const FZFarmlandPlantSeedCallback& InCallback);
    
    /**
     * 药园浇灌
    */
    static constexpr uint64 FarmlandWatering = 0xe3ed0b4b10317d81LL; 
    typedef TSharedPtr<idlezt::FarmlandWateringReq> FZFarmlandWateringReqPtr;
    typedef TSharedPtr<idlezt::FarmlandWateringAck> FZFarmlandWateringRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandWateringReqPtr&, const FZFarmlandWateringRspPtr&)> FZFarmlandWateringCallback;
    static void FarmlandWateringRegister(FMRpcManager* InManager, const FZFarmlandWateringCallback& InCallback);
    
    /**
     * 药园催熟
    */
    static constexpr uint64 FarmlandRipening = 0x2b35a502e60a4ae8LL; 
    typedef TSharedPtr<idlezt::FarmlandRipeningReq> FZFarmlandRipeningReqPtr;
    typedef TSharedPtr<idlezt::FarmlandRipeningAck> FZFarmlandRipeningRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandRipeningReqPtr&, const FZFarmlandRipeningRspPtr&)> FZFarmlandRipeningCallback;
    static void FarmlandRipeningRegister(FMRpcManager* InManager, const FZFarmlandRipeningCallback& InCallback);
    
    /**
     * 药园收获
    */
    static constexpr uint64 FarmlandHarvest = 0x8f00e19bf7d1fcf9LL; 
    typedef TSharedPtr<idlezt::FarmlandHarvestReq> FZFarmlandHarvestReqPtr;
    typedef TSharedPtr<idlezt::FarmlandHarvestAck> FZFarmlandHarvestRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandHarvestReqPtr&, const FZFarmlandHarvestRspPtr&)> FZFarmlandHarvestCallback;
    static void FarmlandHarvestRegister(FMRpcManager* InManager, const FZFarmlandHarvestCallback& InCallback);
    
    /**
     * 药园药童升级
    */
    static constexpr uint64 FarmerRankUp = 0x734426ac71222769LL; 
    typedef TSharedPtr<idlezt::FarmerRankUpReq> FZFarmerRankUpReqPtr;
    typedef TSharedPtr<idlezt::FarmerRankUpAck> FZFarmerRankUpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmerRankUpReqPtr&, const FZFarmerRankUpRspPtr&)> FZFarmerRankUpCallback;
    static void FarmerRankUpRegister(FMRpcManager* InManager, const FZFarmerRankUpCallback& InCallback);
    
    /**
     * 药园打理
    */
    static constexpr uint64 FarmlandSetManagement = 0xbf0b6efea8385ed3LL; 
    typedef TSharedPtr<idlezt::FarmlandSetManagementReq> FZFarmlandSetManagementReqPtr;
    typedef TSharedPtr<idlezt::FarmlandSetManagementAck> FZFarmlandSetManagementRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFarmlandSetManagementReqPtr&, const FZFarmlandSetManagementRspPtr&)> FZFarmlandSetManagementCallback;
    static void FarmlandSetManagementRegister(FMRpcManager* InManager, const FZFarmlandSetManagementCallback& InCallback);
    
    /**
     * 获取药园状态，自动收获
    */
    static constexpr uint64 UpdateFarmlandState = 0xf6a5ab987b36843eLL; 
    typedef TSharedPtr<idlezt::UpdateFarmlandStateReq> FZUpdateFarmlandStateReqPtr;
    typedef TSharedPtr<idlezt::UpdateFarmlandStateAck> FZUpdateFarmlandStateRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZUpdateFarmlandStateReqPtr&, const FZUpdateFarmlandStateRspPtr&)> FZUpdateFarmlandStateCallback;
    static void UpdateFarmlandStateRegister(FMRpcManager* InManager, const FZUpdateFarmlandStateCallback& InCallback);
    
    /**
     * 请求开启生存副本
    */
    static constexpr uint64 DungeonSurviveChallenge = 0x94c0c2bccdfb336LL; 
    typedef TSharedPtr<idlezt::DungeonSurviveChallengeReq> FZDungeonSurviveChallengeReqPtr;
    typedef TSharedPtr<idlezt::DungeonSurviveChallengeAck> FZDungeonSurviveChallengeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonSurviveChallengeReqPtr&, const FZDungeonSurviveChallengeRspPtr&)> FZDungeonSurviveChallengeCallback;
    static void DungeonSurviveChallengeRegister(FMRpcManager* InManager, const FZDungeonSurviveChallengeCallback& InCallback);
    
    /**
     * 请求生存副本快速结束
    */
    static constexpr uint64 DungeonSurviveQuickEnd = 0x982f0eec89174ca9LL; 
    typedef TSharedPtr<idlezt::DungeonSurviveQuickEndReq> FZDungeonSurviveQuickEndReqPtr;
    typedef TSharedPtr<idlezt::DungeonSurviveQuickEndAck> FZDungeonSurviveQuickEndRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonSurviveQuickEndReqPtr&, const FZDungeonSurviveQuickEndRspPtr&)> FZDungeonSurviveQuickEndCallback;
    static void DungeonSurviveQuickEndRegister(FMRpcManager* InManager, const FZDungeonSurviveQuickEndCallback& InCallback);
    
    /**
     * 询问生存副本是否完成
    */
    static constexpr uint64 DungeonSurviveData = 0xe17aea5e8b0f54dLL; 
    typedef TSharedPtr<idlezt::DungeonSurviveDataReq> FZDungeonSurviveDataReqPtr;
    typedef TSharedPtr<idlezt::DungeonSurviveDataAck> FZDungeonSurviveDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDungeonSurviveDataReqPtr&, const FZDungeonSurviveDataRspPtr&)> FZDungeonSurviveDataCallback;
    static void DungeonSurviveDataRegister(FMRpcManager* InManager, const FZDungeonSurviveDataCallback& InCallback);
    
    /**
     * 神通一键重置CD请求
    */
    static constexpr uint64 GetRevertAllSkillCoolDown = 0x5770fae9f1913078LL; 
    typedef TSharedPtr<idlezt::GetRevertAllSkillCoolDownReq> FZGetRevertAllSkillCoolDownReqPtr;
    typedef TSharedPtr<idlezt::GetRevertAllSkillCoolDownAck> FZGetRevertAllSkillCoolDownRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRevertAllSkillCoolDownReqPtr&, const FZGetRevertAllSkillCoolDownRspPtr&)> FZGetRevertAllSkillCoolDownCallback;
    static void GetRevertAllSkillCoolDownRegister(FMRpcManager* InManager, const FZGetRevertAllSkillCoolDownCallback& InCallback);
    
    /**
     * 获取道友功能数据
    */
    static constexpr uint64 GetRoleFriendData = 0xb89c794b806ab14fLL; 
    typedef TSharedPtr<idlezt::GetRoleFriendDataReq> FZGetRoleFriendDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleFriendDataAck> FZGetRoleFriendDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleFriendDataReqPtr&, const FZGetRoleFriendDataRspPtr&)> FZGetRoleFriendDataCallback;
    static void GetRoleFriendDataRegister(FMRpcManager* InManager, const FZGetRoleFriendDataCallback& InCallback);
    
    /**
     * 发起 好友申请/或移除好友 拉黑/或移除拉黑 成为道侣或解除道侣
    */
    static constexpr uint64 FriendOp = 0x28573fdb3403e202LL; 
    typedef TSharedPtr<idlezt::FriendOpReq> FZFriendOpReqPtr;
    typedef TSharedPtr<idlezt::FriendOpAck> FZFriendOpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFriendOpReqPtr&, const FZFriendOpRspPtr&)> FZFriendOpCallback;
    static void FriendOpRegister(FMRpcManager* InManager, const FZFriendOpCallback& InCallback);
    
    /**
     * 处理好友申请
    */
    static constexpr uint64 ReplyFriendRequest = 0xd2cc0e763eb8cad4LL; 
    typedef TSharedPtr<idlezt::ReplyFriendRequestReq> FZReplyFriendRequestReqPtr;
    typedef TSharedPtr<idlezt::ReplyFriendRequestAck> FZReplyFriendRequestRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReplyFriendRequestReqPtr&, const FZReplyFriendRequestRspPtr&)> FZReplyFriendRequestCallback;
    static void ReplyFriendRequestRegister(FMRpcManager* InManager, const FZReplyFriendRequestCallback& InCallback);
    
    /**
     * 查找玩家（道友功能）
    */
    static constexpr uint64 FriendSearchRoleInfo = 0xb5c5120d51c25dbdLL; 
    typedef TSharedPtr<idlezt::FriendSearchRoleInfoReq> FZFriendSearchRoleInfoReqPtr;
    typedef TSharedPtr<idlezt::FriendSearchRoleInfoAck> FZFriendSearchRoleInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZFriendSearchRoleInfoReqPtr&, const FZFriendSearchRoleInfoRspPtr&)> FZFriendSearchRoleInfoCallback;
    static void FriendSearchRoleInfoRegister(FMRpcManager* InManager, const FZFriendSearchRoleInfoCallback& InCallback);
    
    /**
     * 请求玩家信息缓存(Todo 用于聊天查找，可能需要整合)
    */
    static constexpr uint64 GetRoleInfoCache = 0xabf13ca9f44e096bLL; 
    typedef TSharedPtr<idlezt::GetRoleInfoCacheReq> FZGetRoleInfoCacheReqPtr;
    typedef TSharedPtr<idlezt::GetRoleInfoCacheAck> FZGetRoleInfoCacheRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleInfoCacheReqPtr&, const FZGetRoleInfoCacheRspPtr&)> FZGetRoleInfoCacheCallback;
    static void GetRoleInfoCacheRegister(FMRpcManager* InManager, const FZGetRoleInfoCacheCallback& InCallback);
    
    /**
     * 请求玩家个人信息(Todo 老接口，可能需要整合)
    */
    static constexpr uint64 GetRoleInfo = 0x38b4cdc7b5de6957LL; 
    typedef TSharedPtr<idlezt::GetRoleInfoReq> FZGetRoleInfoReqPtr;
    typedef TSharedPtr<idlezt::GetRoleInfoAck> FZGetRoleInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleInfoReqPtr&, const FZGetRoleInfoRspPtr&)> FZGetRoleInfoCallback;
    static void GetRoleInfoRegister(FMRpcManager* InManager, const FZGetRoleInfoCallback& InCallback);
    
    /**
     * 获取化身数据
    */
    static constexpr uint64 GetRoleAvatarData = 0xea50ca619baa92baLL; 
    typedef TSharedPtr<idlezt::GetRoleAvatarDataReq> FZGetRoleAvatarDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleAvatarDataAck> FZGetRoleAvatarDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleAvatarDataReqPtr&, const FZGetRoleAvatarDataRspPtr&)> FZGetRoleAvatarDataCallback;
    static void GetRoleAvatarDataRegister(FMRpcManager* InManager, const FZGetRoleAvatarDataCallback& InCallback);
    
    /**
     * 派遣化身
    */
    static constexpr uint64 DispatchAvatar = 0xd94339b499fdcb32LL; 
    typedef TSharedPtr<idlezt::DispatchAvatarReq> FZDispatchAvatarReqPtr;
    typedef TSharedPtr<idlezt::DispatchAvatarAck> FZDispatchAvatarRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZDispatchAvatarReqPtr&, const FZDispatchAvatarRspPtr&)> FZDispatchAvatarCallback;
    static void DispatchAvatarRegister(FMRpcManager* InManager, const FZDispatchAvatarCallback& InCallback);
    
    /**
     * 化身升级
    */
    static constexpr uint64 AvatarRankUp = 0xe1ef1ed9f810a4f7LL; 
    typedef TSharedPtr<idlezt::AvatarRankUpReq> FZAvatarRankUpReqPtr;
    typedef TSharedPtr<idlezt::AvatarRankUpAck> FZAvatarRankUpRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAvatarRankUpReqPtr&, const FZAvatarRankUpRspPtr&)> FZAvatarRankUpCallback;
    static void AvatarRankUpRegister(FMRpcManager* InManager, const FZAvatarRankUpCallback& InCallback);
    
    /**
     * 收获化身包裹道具
    */
    static constexpr uint64 ReceiveAvatarTempPackage = 0xa491d788f953d31bLL; 
    typedef TSharedPtr<idlezt::ReceiveAvatarTempPackageReq> FZReceiveAvatarTempPackageReqPtr;
    typedef TSharedPtr<idlezt::ReceiveAvatarTempPackageAck> FZReceiveAvatarTempPackageRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReceiveAvatarTempPackageReqPtr&, const FZReceiveAvatarTempPackageRspPtr&)> FZReceiveAvatarTempPackageCallback;
    static void ReceiveAvatarTempPackageRegister(FMRpcManager* InManager, const FZReceiveAvatarTempPackageCallback& InCallback);
    
    /**
     * 获取秘境探索统计数据
    */
    static constexpr uint64 GetArenaExplorationStatisticalData = 0xddb17352c8ac24c2LL; 
    typedef TSharedPtr<idlezt::GetArenaExplorationStatisticalDataReq> FZGetArenaExplorationStatisticalDataReqPtr;
    typedef TSharedPtr<idlezt::GetArenaExplorationStatisticalDataAck> FZGetArenaExplorationStatisticalDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetArenaExplorationStatisticalDataReqPtr&, const FZGetArenaExplorationStatisticalDataRspPtr&)> FZGetArenaExplorationStatisticalDataCallback;
    static void GetArenaExplorationStatisticalDataRegister(FMRpcManager* InManager, const FZGetArenaExplorationStatisticalDataCallback& InCallback);
    
    /**
     * 获取角色传记数据
    */
    static constexpr uint64 GetRoleBiographyData = 0x819c33db3dac76d0LL; 
    typedef TSharedPtr<idlezt::GetRoleBiographyDataReq> FZGetRoleBiographyDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleBiographyDataAck> FZGetRoleBiographyDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleBiographyDataReqPtr&, const FZGetRoleBiographyDataRspPtr&)> FZGetRoleBiographyDataCallback;
    static void GetRoleBiographyDataRegister(FMRpcManager* InManager, const FZGetRoleBiographyDataCallback& InCallback);
    
    /**
     * 请求领取传记奖励
    */
    static constexpr uint64 ReceiveBiographyItem = 0x212b3a04800bffdcLL; 
    typedef TSharedPtr<idlezt::ReceiveBiographyItemReq> FZReceiveBiographyItemReqPtr;
    typedef TSharedPtr<idlezt::ReceiveBiographyItemAck> FZReceiveBiographyItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReceiveBiographyItemReqPtr&, const FZReceiveBiographyItemRspPtr&)> FZReceiveBiographyItemCallback;
    static void ReceiveBiographyItemRegister(FMRpcManager* InManager, const FZReceiveBiographyItemCallback& InCallback);
    
    /**
     * 请求领取史记数据
    */
    static constexpr uint64 GetBiographyEventData = 0x3f8b2d3e7cef7aa4LL; 
    typedef TSharedPtr<idlezt::GetBiographyEventDataReq> FZGetBiographyEventDataReqPtr;
    typedef TSharedPtr<idlezt::GetBiographyEventDataAck> FZGetBiographyEventDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetBiographyEventDataReqPtr&, const FZGetBiographyEventDataRspPtr&)> FZGetBiographyEventDataCallback;
    static void GetBiographyEventDataRegister(FMRpcManager* InManager, const FZGetBiographyEventDataCallback& InCallback);
    
    /**
     * 请求领取史记奖励
    */
    static constexpr uint64 ReceiveBiographyEventItem = 0x78850f010c36bac2LL; 
    typedef TSharedPtr<idlezt::ReceiveBiographyEventItemReq> FZReceiveBiographyEventItemReqPtr;
    typedef TSharedPtr<idlezt::ReceiveBiographyEventItemAck> FZReceiveBiographyEventItemRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZReceiveBiographyEventItemReqPtr&, const FZReceiveBiographyEventItemRspPtr&)> FZReceiveBiographyEventItemCallback;
    static void ReceiveBiographyEventItemRegister(FMRpcManager* InManager, const FZReceiveBiographyEventItemCallback& InCallback);
    
    /**
     * 请求上传纪念日志
    */
    static constexpr uint64 AddBiographyRoleLog = 0x6baefce01c304b9LL; 
    typedef TSharedPtr<idlezt::AddBiographyRoleLogReq> FZAddBiographyRoleLogReqPtr;
    typedef TSharedPtr<idlezt::AddBiographyRoleLogAck> FZAddBiographyRoleLogRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZAddBiographyRoleLogReqPtr&, const FZAddBiographyRoleLogRspPtr&)> FZAddBiographyRoleLogCallback;
    static void AddBiographyRoleLogRegister(FMRpcManager* InManager, const FZAddBiographyRoleLogCallback& InCallback);
    
    /**
     * 请求进入镇魔深渊
    */
    static constexpr uint64 RequestEnterSeptDemonWorld = 0x33254bf6749d82f5LL; 
    typedef TSharedPtr<idlezt::RequestEnterSeptDemonWorldReq> FZRequestEnterSeptDemonWorldReqPtr;
    typedef TSharedPtr<idlezt::RequestEnterSeptDemonWorldAck> FZRequestEnterSeptDemonWorldRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestEnterSeptDemonWorldReqPtr&, const FZRequestEnterSeptDemonWorldRspPtr&)> FZRequestEnterSeptDemonWorldCallback;
    static void RequestEnterSeptDemonWorldRegister(FMRpcManager* InManager, const FZRequestEnterSeptDemonWorldCallback& InCallback);
    
    /**
     * 请求退出镇魔深渊
    */
    static constexpr uint64 RequestLeaveSeptDemonWorld = 0x2dfd2897f4845cdcLL; 
    typedef TSharedPtr<idlezt::RequestLeaveSeptDemonWorldReq> FZRequestLeaveSeptDemonWorldReqPtr;
    typedef TSharedPtr<idlezt::RequestLeaveSeptDemonWorldAck> FZRequestLeaveSeptDemonWorldRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestLeaveSeptDemonWorldReqPtr&, const FZRequestLeaveSeptDemonWorldRspPtr&)> FZRequestLeaveSeptDemonWorldCallback;
    static void RequestLeaveSeptDemonWorldRegister(FMRpcManager* InManager, const FZRequestLeaveSeptDemonWorldCallback& InCallback);
    
    /**
     * 请求镇魔深渊相关数据
    */
    static constexpr uint64 RequestSeptDemonWorldData = 0x9c8a174d0686863bLL; 
    typedef TSharedPtr<idlezt::RequestSeptDemonWorldDataReq> FZRequestSeptDemonWorldDataReqPtr;
    typedef TSharedPtr<idlezt::RequestSeptDemonWorldDataAck> FZRequestSeptDemonWorldDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestSeptDemonWorldDataReqPtr&, const FZRequestSeptDemonWorldDataRspPtr&)> FZRequestSeptDemonWorldDataCallback;
    static void RequestSeptDemonWorldDataRegister(FMRpcManager* InManager, const FZRequestSeptDemonWorldDataCallback& InCallback);
    
    /**
     * 请求在镇魔深渊待的最后时间点
    */
    static constexpr uint64 RequestInSeptDemonWorldEndTime = 0xcd673321ff1419ecLL; 
    typedef TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeReq> FZRequestInSeptDemonWorldEndTimeReqPtr;
    typedef TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeAck> FZRequestInSeptDemonWorldEndTimeRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZRequestInSeptDemonWorldEndTimeReqPtr&, const FZRequestInSeptDemonWorldEndTimeRspPtr&)> FZRequestInSeptDemonWorldEndTimeCallback;
    static void RequestInSeptDemonWorldEndTimeRegister(FMRpcManager* InManager, const FZRequestInSeptDemonWorldEndTimeCallback& InCallback);
    
    /**
     * 请求镇魔深渊待伤害排行榜
    */
    static constexpr uint64 GetSeptDemonDamageTopList = 0xba30f3ac3722953eLL; 
    typedef TSharedPtr<idlezt::GetSeptDemonDamageTopListReq> FZGetSeptDemonDamageTopListReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonDamageTopListAck> FZGetSeptDemonDamageTopListRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonDamageTopListReqPtr&, const FZGetSeptDemonDamageTopListRspPtr&)> FZGetSeptDemonDamageTopListCallback;
    static void GetSeptDemonDamageTopListRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageTopListCallback& InCallback);
    
    /**
     * 请求镇魔深渊待玩家伤害预览信息
    */
    static constexpr uint64 GetSeptDemonDamageSelfSummary = 0xf8c4677995b8853LL; 
    typedef TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryReq> FZGetSeptDemonDamageSelfSummaryReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryAck> FZGetSeptDemonDamageSelfSummaryRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonDamageSelfSummaryReqPtr&, const FZGetSeptDemonDamageSelfSummaryRspPtr&)> FZGetSeptDemonDamageSelfSummaryCallback;
    static void GetSeptDemonDamageSelfSummaryRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageSelfSummaryCallback& InCallback);
    
    /**
     * 请求镇魔深渊待宝库奖励剩余抽奖次数
    */
    static constexpr uint64 GetSeptDemonStageRewardNum = 0x9658e3c4808c309dLL; 
    typedef TSharedPtr<idlezt::GetSeptDemonStageRewardNumReq> FZGetSeptDemonStageRewardNumReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonStageRewardNumAck> FZGetSeptDemonStageRewardNumRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonStageRewardNumReqPtr&, const FZGetSeptDemonStageRewardNumRspPtr&)> FZGetSeptDemonStageRewardNumCallback;
    static void GetSeptDemonStageRewardNumRegister(FMRpcManager* InManager, const FZGetSeptDemonStageRewardNumCallback& InCallback);
    
    /**
     * 请求镇魔深渊待宝库奖励
    */
    static constexpr uint64 GetSeptDemonStageReward = 0x3f9f5e5332a607bfLL; 
    typedef TSharedPtr<idlezt::GetSeptDemonStageRewardReq> FZGetSeptDemonStageRewardReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonStageRewardAck> FZGetSeptDemonStageRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonStageRewardReqPtr&, const FZGetSeptDemonStageRewardRspPtr&)> FZGetSeptDemonStageRewardCallback;
    static void GetSeptDemonStageRewardRegister(FMRpcManager* InManager, const FZGetSeptDemonStageRewardCallback& InCallback);
    
    /**
     * 请求镇魔深渊挑战奖励列表信息
    */
    static constexpr uint64 GetSeptDemonDamageRewardsInfo = 0xa82b6fbb2c009d2dLL; 
    typedef TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoReq> FZGetSeptDemonDamageRewardsInfoReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoAck> FZGetSeptDemonDamageRewardsInfoRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonDamageRewardsInfoReqPtr&, const FZGetSeptDemonDamageRewardsInfoRspPtr&)> FZGetSeptDemonDamageRewardsInfoCallback;
    static void GetSeptDemonDamageRewardsInfoRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageRewardsInfoCallback& InCallback);
    
    /**
     * 请求镇魔深渊待挑战奖励
    */
    static constexpr uint64 GetSeptDemonDamageReward = 0xeb4dd5ead75d6abcLL; 
    typedef TSharedPtr<idlezt::GetSeptDemonDamageRewardReq> FZGetSeptDemonDamageRewardReqPtr;
    typedef TSharedPtr<idlezt::GetSeptDemonDamageRewardAck> FZGetSeptDemonDamageRewardRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetSeptDemonDamageRewardReqPtr&, const FZGetSeptDemonDamageRewardRspPtr&)> FZGetSeptDemonDamageRewardCallback;
    static void GetSeptDemonDamageRewardRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageRewardCallback& InCallback);
    
    /**
     * 请求仙阁商店数据
    */
    static constexpr uint64 GetRoleVipShopData = 0xa53a5f39c26657aLL; 
    typedef TSharedPtr<idlezt::GetRoleVipShopDataReq> FZGetRoleVipShopDataReqPtr;
    typedef TSharedPtr<idlezt::GetRoleVipShopDataAck> FZGetRoleVipShopDataRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZGetRoleVipShopDataReqPtr&, const FZGetRoleVipShopDataRspPtr&)> FZGetRoleVipShopDataCallback;
    static void GetRoleVipShopDataRegister(FMRpcManager* InManager, const FZGetRoleVipShopDataCallback& InCallback);
    
    /**
     * 请求仙阁商店购买
    */
    static constexpr uint64 VipShopBuy = 0xea7ffb6c880a128cLL; 
    typedef TSharedPtr<idlezt::VipShopBuyReq> FZVipShopBuyReqPtr;
    typedef TSharedPtr<idlezt::VipShopBuyAck> FZVipShopBuyRspPtr;
    typedef TFunction<void(FZPbMessageSupportBase*, const FZVipShopBuyReqPtr&, const FZVipShopBuyRspPtr&)> FZVipShopBuyCallback;
    static void VipShopBuyRegister(FMRpcManager* InManager, const FZVipShopBuyCallback& InCallback);
    

};
