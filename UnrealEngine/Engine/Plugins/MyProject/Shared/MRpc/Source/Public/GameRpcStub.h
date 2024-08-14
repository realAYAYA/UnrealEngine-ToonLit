#pragma once

#include "ZNetFwd.h"
#include "ZRpcManager.h"

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

#include "GameRpcStub.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLoginGameResult, EZRpcErrorCode, InErrorCode, FZLoginGameAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetCurrentCultivationDirectionResult, EZRpcErrorCode, InErrorCode, FZSetCurrentCultivationDirectionAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDoBreakthroughResult, EZRpcErrorCode, InErrorCode, FZDoBreakthroughAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestCommonCultivationDataResult, EZRpcErrorCode, InErrorCode, FZRequestCommonCultivationDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickMergeBreathingResult, EZRpcErrorCode, InErrorCode, FZOneClickMergeBreathingAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBreathingExerciseRewardResult, EZRpcErrorCode, InErrorCode, FZReceiveBreathingExerciseRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetInventoryDataResult, EZRpcErrorCode, InErrorCode, FZGetInventoryDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetQuestDataResult, EZRpcErrorCode, InErrorCode, FZGetQuestDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnCreateCharacterResult, EZRpcErrorCode, InErrorCode, FZCreateCharacterAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseItemResult, EZRpcErrorCode, InErrorCode, FZUseItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseSelectGiftResult, EZRpcErrorCode, InErrorCode, FZUseSelectGiftAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSellItemResult, EZRpcErrorCode, InErrorCode, FZSellItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockEquipmentSlotResult, EZRpcErrorCode, InErrorCode, FZUnlockEquipmentSlotAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineStartResult, EZRpcErrorCode, InErrorCode, FZAlchemyRefineStartAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineCancelResult, EZRpcErrorCode, InErrorCode, FZAlchemyRefineCancelAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineExtractResult, EZRpcErrorCode, InErrorCode, FZAlchemyRefineExtractAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleShopDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshShopResult, EZRpcErrorCode, InErrorCode, FZRefreshShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuyShopItemResult, EZRpcErrorCode, InErrorCode, FZBuyShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleDeluxeShopDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleDeluxeShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshDeluxeShopResult, EZRpcErrorCode, InErrorCode, FZRefreshDeluxeShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuyDeluxeShopItemResult, EZRpcErrorCode, InErrorCode, FZBuyDeluxeShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetTemporaryPackageDataResult, EZRpcErrorCode, InErrorCode, FZGetTemporaryPackageDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExtractTemporaryPackageItemsResult, EZRpcErrorCode, InErrorCode, FZExtractTemporaryPackageItemsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSpeedupReliveResult, EZRpcErrorCode, InErrorCode, FZSpeedupReliveAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMapInfoResult, EZRpcErrorCode, InErrorCode, FZGetMapInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockArenaResult, EZRpcErrorCode, InErrorCode, FZUnlockArenaAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQuestOpResult, EZRpcErrorCode, InErrorCode, FZQuestOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEquipmentPutOnResult, EZRpcErrorCode, InErrorCode, FZEquipmentPutOnAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEquipmentTakeOffResult, EZRpcErrorCode, InErrorCode, FZEquipmentTakeOffAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLeaderboardPreviewResult, EZRpcErrorCode, InErrorCode, FZGetLeaderboardPreviewAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLeaderboardDataResult, EZRpcErrorCode, InErrorCode, FZGetLeaderboardDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleLeaderboardDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleLeaderboardDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLeaderboardClickLikeResult, EZRpcErrorCode, InErrorCode, FZLeaderboardClickLikeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLeaderboardUpdateMessageResult, EZRpcErrorCode, InErrorCode, FZLeaderboardUpdateMessageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetFuZeRewardResult, EZRpcErrorCode, InErrorCode, FZGetFuZeRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleMailDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleMailDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReadMailResult, EZRpcErrorCode, InErrorCode, FZReadMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMailAttachmentResult, EZRpcErrorCode, InErrorCode, FZGetMailAttachmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDeleteMailResult, EZRpcErrorCode, InErrorCode, FZDeleteMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickGetMailAttachmentResult, EZRpcErrorCode, InErrorCode, FZOneClickGetMailAttachmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickReadMailResult, EZRpcErrorCode, InErrorCode, FZOneClickReadMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickDeleteMailResult, EZRpcErrorCode, InErrorCode, FZOneClickDeleteMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockFunctionModuleResult, EZRpcErrorCode, InErrorCode, FZUnlockFunctionModuleAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetChatRecordResult, EZRpcErrorCode, InErrorCode, FZGetChatRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDeletePrivateChatRecordResult, EZRpcErrorCode, InErrorCode, FZDeletePrivateChatRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSendChatMessageResult, EZRpcErrorCode, InErrorCode, FZSendChatMessageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnClearChatUnreadNumResult, EZRpcErrorCode, InErrorCode, FZClearChatUnreadNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineStartResult, EZRpcErrorCode, InErrorCode, FZForgeRefineStartAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineCancelResult, EZRpcErrorCode, InErrorCode, FZForgeRefineCancelAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineExtractResult, EZRpcErrorCode, InErrorCode, FZForgeRefineExtractAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetForgeLostEquipmentDataResult, EZRpcErrorCode, InErrorCode, FZGetForgeLostEquipmentDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeDestroyResult, EZRpcErrorCode, InErrorCode, FZForgeDestroyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeFindBackResult, EZRpcErrorCode, InErrorCode, FZForgeFindBackAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestPillElixirDataResult, EZRpcErrorCode, InErrorCode, FZRequestPillElixirDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetOnePillElixirDataResult, EZRpcErrorCode, InErrorCode, FZGetOnePillElixirDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestModifyPillElixirFilterResult, EZRpcErrorCode, InErrorCode, FZRequestModifyPillElixirFilterAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUsePillElixirResult, EZRpcErrorCode, InErrorCode, FZUsePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickUsePillElixirResult, EZRpcErrorCode, InErrorCode, FZOneClickUsePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnTradePillElixirResult, EZRpcErrorCode, InErrorCode, FZTradePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReinforceEquipmentResult, EZRpcErrorCode, InErrorCode, FZReinforceEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefineEquipmentResult, EZRpcErrorCode, InErrorCode, FZRefineEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQiWenEquipmentResult, EZRpcErrorCode, InErrorCode, FZQiWenEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnResetEquipmentResult, EZRpcErrorCode, InErrorCode, FZResetEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnInheritEquipmentResult, EZRpcErrorCode, InErrorCode, FZInheritEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLockItemResult, EZRpcErrorCode, InErrorCode, FZLockItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSoloArenaChallengeResult, EZRpcErrorCode, InErrorCode, FZSoloArenaChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSoloArenaQuickEndResult, EZRpcErrorCode, InErrorCode, FZSoloArenaQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSoloArenaHistoryListResult, EZRpcErrorCode, InErrorCode, FZGetSoloArenaHistoryListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerChallengeResult, EZRpcErrorCode, InErrorCode, FZMonsterTowerChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerDrawIdleAwardResult, EZRpcErrorCode, InErrorCode, FZMonsterTowerDrawIdleAwardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerClosedDoorTrainingResult, EZRpcErrorCode, InErrorCode, FZMonsterTowerClosedDoorTrainingAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerQuickEndResult, EZRpcErrorCode, InErrorCode, FZMonsterTowerQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMonsterTowerChallengeListResult, EZRpcErrorCode, InErrorCode, FZGetMonsterTowerChallengeListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMonsterTowerChallengeRewardResult, EZRpcErrorCode, InErrorCode, FZGetMonsterTowerChallengeRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetWorldTimeDilationResult, EZRpcErrorCode, InErrorCode, FZSetWorldTimeDilationAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetFightModeResult, EZRpcErrorCode, InErrorCode, FZSetFightModeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpgradeQiCollectorResult, EZRpcErrorCode, InErrorCode, FZUpgradeQiCollectorAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleAllStatsResult, EZRpcErrorCode, InErrorCode, FZGetRoleAllStatsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShanhetuDataResult, EZRpcErrorCode, InErrorCode, FZGetShanhetuDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetShanhetuUseConfigResult, EZRpcErrorCode, InErrorCode, FZSetShanhetuUseConfigAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseShanhetuResult, EZRpcErrorCode, InErrorCode, FZUseShanhetuAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnStepShanhetuResult, EZRpcErrorCode, InErrorCode, FZStepShanhetuAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShanhetuUseRecordResult, EZRpcErrorCode, InErrorCode, FZGetShanhetuUseRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetAttackLockTypeResult, EZRpcErrorCode, InErrorCode, FZSetAttackLockTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetAttackUnlockTypeResult, EZRpcErrorCode, InErrorCode, FZSetAttackUnlockTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetShowUnlockButtonResult, EZRpcErrorCode, InErrorCode, FZSetShowUnlockButtonAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetUserVarResult, EZRpcErrorCode, InErrorCode, FZGetUserVarRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetUserVarsResult, EZRpcErrorCode, InErrorCode, FZGetUserVarsRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionArenaSummaryResult, EZRpcErrorCode, InErrorCode, FZGetBossInvasionArenaSummaryRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionArenaTopListResult, EZRpcErrorCode, InErrorCode, FZGetBossInvasionArenaTopListRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionInfoResult, EZRpcErrorCode, InErrorCode, FZGetBossInvasionInfoRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDrawBossInvasionKillRewardResult, EZRpcErrorCode, InErrorCode, FZDrawBossInvasionKillRewardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDrawBossInvasionDamageRewardResult, EZRpcErrorCode, InErrorCode, FZDrawBossInvasionDamageRewardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBossInvasionTeleportResult, EZRpcErrorCode, InErrorCode, FZBossInvasionTeleportRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfItemResult, EZRpcErrorCode, InErrorCode, FZShareSelfItemRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfItemsResult, EZRpcErrorCode, InErrorCode, FZShareSelfItemsRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShareItemDataResult, EZRpcErrorCode, InErrorCode, FZGetShareItemDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleCollectionDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleCollectionDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRoleCollectionOpResult, EZRpcErrorCode, InErrorCode, FZRoleCollectionOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfRoleCollectionResult, EZRpcErrorCode, InErrorCode, FZShareSelfRoleCollectionRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShareRoleCollectionDataResult, EZRpcErrorCode, InErrorCode, FZGetShareRoleCollectionDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetChecklistDataResult, EZRpcErrorCode, InErrorCode, FZGetChecklistDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnChecklistOpResult, EZRpcErrorCode, InErrorCode, FZChecklistOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpdateChecklistResult, EZRpcErrorCode, InErrorCode, FZUpdateChecklistAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSwordPkInfoResult, EZRpcErrorCode, InErrorCode, FZGetSwordPkInfoRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkSignupResult, EZRpcErrorCode, InErrorCode, FZSwordPkSignupRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkMatchingResult, EZRpcErrorCode, InErrorCode, FZSwordPkMatchingRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkChallengeResult, EZRpcErrorCode, InErrorCode, FZSwordPkChallengeRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkRevengeResult, EZRpcErrorCode, InErrorCode, FZSwordPkRevengeRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSwordPkTopListResult, EZRpcErrorCode, InErrorCode, FZGetSwordPkTopListRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkExchangeHeroCardResult, EZRpcErrorCode, InErrorCode, FZSwordPkExchangeHeroCardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCommonItemExchangeDataResult, EZRpcErrorCode, InErrorCode, FZGetCommonItemExchangeDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExchangeCommonItemResult, EZRpcErrorCode, InErrorCode, FZExchangeCommonItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSynthesisCommonItemResult, EZRpcErrorCode, InErrorCode, FZSynthesisCommonItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCandidatesSeptListResult, EZRpcErrorCode, InErrorCode, FZGetCandidatesSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSearchSeptResult, EZRpcErrorCode, InErrorCode, FZSearchSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptBaseInfoResult, EZRpcErrorCode, InErrorCode, FZGetSeptBaseInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptMemberListResult, EZRpcErrorCode, InErrorCode, FZGetSeptMemberListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnCreateSeptResult, EZRpcErrorCode, InErrorCode, FZCreateSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDismissSeptResult, EZRpcErrorCode, InErrorCode, FZDismissSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExitSeptResult, EZRpcErrorCode, InErrorCode, FZExitSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnApplyJoinSeptResult, EZRpcErrorCode, InErrorCode, FZApplyJoinSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnApproveApplySeptResult, EZRpcErrorCode, InErrorCode, FZApproveApplySeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetApplyJoinSeptListResult, EZRpcErrorCode, InErrorCode, FZGetApplyJoinSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRespondInviteSeptResult, EZRpcErrorCode, InErrorCode, FZRespondInviteSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetInviteMeJoinSeptListResult, EZRpcErrorCode, InErrorCode, FZGetInviteMeJoinSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCandidatesInviteRoleListResult, EZRpcErrorCode, InErrorCode, FZGetCandidatesInviteRoleListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnInviteJoinSeptResult, EZRpcErrorCode, InErrorCode, FZInviteJoinSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetSeptSettingsResult, EZRpcErrorCode, InErrorCode, FZSetSeptSettingsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetSeptAnnounceResult, EZRpcErrorCode, InErrorCode, FZSetSeptAnnounceAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnChangeSeptNameResult, EZRpcErrorCode, InErrorCode, FZChangeSeptNameAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptLogResult, EZRpcErrorCode, InErrorCode, FZGetSeptLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnConstructSeptResult, EZRpcErrorCode, InErrorCode, FZConstructSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetConstructSeptLogResult, EZRpcErrorCode, InErrorCode, FZGetConstructSeptLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptInvitedRoleDailyNumResult, EZRpcErrorCode, InErrorCode, FZGetSeptInvitedRoleDailyNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppointSeptPositionResult, EZRpcErrorCode, InErrorCode, FZAppointSeptPositionAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnResignSeptChairmanResult, EZRpcErrorCode, InErrorCode, FZResignSeptChairmanAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnKickOutSeptMemberResult, EZRpcErrorCode, InErrorCode, FZKickOutSeptMemberAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleSeptShopDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleSeptShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuySeptShopItemResult, EZRpcErrorCode, InErrorCode, FZBuySeptShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleSeptQuestDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleSeptQuestDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReqRoleSeptQuestOpResult, EZRpcErrorCode, InErrorCode, FZReqRoleSeptQuestOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshSeptQuestResult, EZRpcErrorCode, InErrorCode, FZRefreshSeptQuestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReqSeptQuestRankUpResult, EZRpcErrorCode, InErrorCode, FZReqSeptQuestRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBeginOccupySeptStoneResult, EZRpcErrorCode, InErrorCode, FZBeginOccupySeptStoneAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEndOccupySeptStoneResult, EZRpcErrorCode, InErrorCode, FZEndOccupySeptStoneAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOccupySeptLandResult, EZRpcErrorCode, InErrorCode, FZOccupySeptLandAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetGongFaDataResult, EZRpcErrorCode, InErrorCode, FZGetGongFaDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGongFaOpResult, EZRpcErrorCode, InErrorCode, FZGongFaOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnActivateGongFaMaxEffectResult, EZRpcErrorCode, InErrorCode, FZActivateGongFaMaxEffectAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptLandDamageTopListResult, EZRpcErrorCode, InErrorCode, FZGetSeptLandDamageTopListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveFuZengRewardsResult, EZRpcErrorCode, InErrorCode, FZReceiveFuZengRewardsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleFuZengDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleFuZengDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleTreasuryDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleTreasuryDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOpenTreasuryChestResult, EZRpcErrorCode, InErrorCode, FZOpenTreasuryChestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickOpenTreasuryChestResult, EZRpcErrorCode, InErrorCode, FZOneClickOpenTreasuryChestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOpenTreasuryGachaResult, EZRpcErrorCode, InErrorCode, FZOpenTreasuryGachaAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshTreasuryShopResult, EZRpcErrorCode, InErrorCode, FZRefreshTreasuryShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnTreasuryShopBuyResult, EZRpcErrorCode, InErrorCode, FZTreasuryShopBuyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLifeCounterDataResult, EZRpcErrorCode, InErrorCode, FZGetLifeCounterDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDoQuestFightResult, EZRpcErrorCode, InErrorCode, FZDoQuestFightAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQuestFightQuickEndResult, EZRpcErrorCode, InErrorCode, FZQuestFightQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetAppearanceDataResult, EZRpcErrorCode, InErrorCode, FZGetAppearanceDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceAddResult, EZRpcErrorCode, InErrorCode, FZAppearanceAddAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceActiveResult, EZRpcErrorCode, InErrorCode, FZAppearanceActiveAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceWearResult, EZRpcErrorCode, InErrorCode, FZAppearanceWearAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceBuyResult, EZRpcErrorCode, InErrorCode, FZAppearanceBuyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceChangeSkTypeResult, EZRpcErrorCode, InErrorCode, FZAppearanceChangeSkTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBattleHistoryInfoResult, EZRpcErrorCode, InErrorCode, FZGetBattleHistoryInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetArenaCheckListDataResult, EZRpcErrorCode, InErrorCode, FZGetArenaCheckListDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnArenaCheckListSubmitResult, EZRpcErrorCode, InErrorCode, FZArenaCheckListSubmitAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnArenaCheckListRewardSubmitResult, EZRpcErrorCode, InErrorCode, FZArenaCheckListRewardSubmitAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllChallengeResult, EZRpcErrorCode, InErrorCode, FZDungeonKillAllChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllQuickEndResult, EZRpcErrorCode, InErrorCode, FZDungeonKillAllQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllDataResult, EZRpcErrorCode, InErrorCode, FZDungeonKillAllDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetFarmlandDataResult, EZRpcErrorCode, InErrorCode, FZGetFarmlandDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandUnlockBlockResult, EZRpcErrorCode, InErrorCode, FZFarmlandUnlockBlockAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandPlantSeedResult, EZRpcErrorCode, InErrorCode, FZFarmlandPlantSeedAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandWateringResult, EZRpcErrorCode, InErrorCode, FZFarmlandWateringAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandRipeningResult, EZRpcErrorCode, InErrorCode, FZFarmlandRipeningAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandHarvestResult, EZRpcErrorCode, InErrorCode, FZFarmlandHarvestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmerRankUpResult, EZRpcErrorCode, InErrorCode, FZFarmerRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandSetManagementResult, EZRpcErrorCode, InErrorCode, FZFarmlandSetManagementAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpdateFarmlandStateResult, EZRpcErrorCode, InErrorCode, FZUpdateFarmlandStateAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveChallengeResult, EZRpcErrorCode, InErrorCode, FZDungeonSurviveChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveQuickEndResult, EZRpcErrorCode, InErrorCode, FZDungeonSurviveQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveDataResult, EZRpcErrorCode, InErrorCode, FZDungeonSurviveDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRevertAllSkillCoolDownResult, EZRpcErrorCode, InErrorCode, FZGetRevertAllSkillCoolDownAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleFriendDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleFriendDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFriendOpResult, EZRpcErrorCode, InErrorCode, FZFriendOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReplyFriendRequestResult, EZRpcErrorCode, InErrorCode, FZReplyFriendRequestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFriendSearchRoleInfoResult, EZRpcErrorCode, InErrorCode, FZFriendSearchRoleInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleInfoCacheResult, EZRpcErrorCode, InErrorCode, FZGetRoleInfoCacheAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleInfoResult, EZRpcErrorCode, InErrorCode, FZGetRoleInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleAvatarDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleAvatarDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDispatchAvatarResult, EZRpcErrorCode, InErrorCode, FZDispatchAvatarAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAvatarRankUpResult, EZRpcErrorCode, InErrorCode, FZAvatarRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveAvatarTempPackageResult, EZRpcErrorCode, InErrorCode, FZReceiveAvatarTempPackageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetArenaExplorationStatisticalDataResult, EZRpcErrorCode, InErrorCode, FZGetArenaExplorationStatisticalDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleBiographyDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleBiographyDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBiographyItemResult, EZRpcErrorCode, InErrorCode, FZReceiveBiographyItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBiographyEventDataResult, EZRpcErrorCode, InErrorCode, FZGetBiographyEventDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBiographyEventItemResult, EZRpcErrorCode, InErrorCode, FZReceiveBiographyEventItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAddBiographyRoleLogResult, EZRpcErrorCode, InErrorCode, FZAddBiographyRoleLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestEnterSeptDemonWorldResult, EZRpcErrorCode, InErrorCode, FZRequestEnterSeptDemonWorldAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestLeaveSeptDemonWorldResult, EZRpcErrorCode, InErrorCode, FZRequestLeaveSeptDemonWorldAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestSeptDemonWorldDataResult, EZRpcErrorCode, InErrorCode, FZRequestSeptDemonWorldDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestInSeptDemonWorldEndTimeResult, EZRpcErrorCode, InErrorCode, FZRequestInSeptDemonWorldEndTimeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageTopListResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonDamageTopListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageSelfSummaryResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonDamageSelfSummaryAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonStageRewardNumResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonStageRewardNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonStageRewardResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonStageRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageRewardsInfoResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonDamageRewardsInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageRewardResult, EZRpcErrorCode, InErrorCode, FZGetSeptDemonDamageRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleVipShopDataResult, EZRpcErrorCode, InErrorCode, FZGetRoleVipShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnVipShopBuyResult, EZRpcErrorCode, InErrorCode, FZVipShopBuyAck, InData);



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyAlchemyRefineResultResult, FZNotifyAlchemyRefineResult, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnRefreshItemsResult, FZRefreshItems, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyInventorySpaceNumResult, FZNotifyInventorySpaceNum, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnRefreshUnlockedEquipmentSlotsResult, FZRefreshUnlockedEquipmentSlots, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyUnlockArenaChallengeResultResult, FZNotifyUnlockArenaChallengeResult, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnUpdateRoleMailResult, FZUpdateRoleMail, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyForgeRefineResultResult, FZNotifyForgeRefineResult, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyGiftPackageResultResult, FZNotifyGiftPackageResult, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyUsePillPropertyResult, FZNotifyUsePillProperty, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyInventoryFullMailItemResult, FZNotifyInventoryFullMailItem, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyRoleCollectionDataResult, FZNotifyRoleCollectionData, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyCommonCollectionPieceDataResult, FZNotifyCommonCollectionPieceData, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyCollectionActivatedSuitResult, FZNotifyCollectionActivatedSuit, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyRoleCollectionHistoriesResult, FZNotifyRoleCollectionHistories, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyCollectionZoneActiveAwardsResult, FZNotifyCollectionZoneActiveAwards, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyRoleCollectionNextResetEnhanceTicksResult, FZNotifyRoleCollectionNextResetEnhanceTicks, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyBossInvasionNpcKilledResult, FZNotifyBossInvasionNpcKilled, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyChecklistResult, FZNotifyChecklist, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifySeptStoneOccupyEndResult, FZNotifySeptStoneOccupyEnd, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyTeleportFailedResult, FZNotifyTeleportFailed, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyFuZengResult, FZNotifyFuZeng, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnUpdateLifeCounterResult, FZUpdateLifeCounter, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyQuestFightChallengeOverResult, FZNotifyQuestFightChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnDungeonChallengeOverResult, FZDungeonChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifySoloArenaChallengeOverResult, FZNotifySoloArenaChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnUpdateChatResult, FZUpdateChat, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonKillAllChallengeCurWaveNumResult, FZNotifyDungeonKillAllChallengeCurWaveNum, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonKillAllChallengeOverResult, FZNotifyDungeonKillAllChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonSurviveChallengeCurWaveNumResult, FZNotifyDungeonSurviveChallengeCurWaveNum, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonSurviveChallengeOverResult, FZNotifyDungeonSurviveChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyFriendMessageResult, FZNotifyFriendMessage, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyBiographyMessageResult, FZNotifyBiographyMessage, InData);


UCLASS(BlueprintType, Blueprintable)
class ZRPC_API UZGameRpcStub : public UObject
{
    GENERATED_BODY()

public:

    void Setup(FZRpcManager* InManager, const FZPbConnectionPtr& InConn);
    void Cleanup();    

    /**
     * 登录游戏
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="LoginGame")
    void K2_LoginGame(const FZLoginGameReq& InParams, const FZOnLoginGameResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::LoginGameAck>&)> OnLoginGameResult;
    void LoginGame(const TSharedPtr<idlezt::LoginGameReq>& InReqMessage, const OnLoginGameResult& InCallback);    

    /**
     * 设置修炼方向
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetCurrentCultivationDirection")
    void K2_SetCurrentCultivationDirection(const FZSetCurrentCultivationDirectionReq& InParams, const FZOnSetCurrentCultivationDirectionResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetCurrentCultivationDirectionAck>&)> OnSetCurrentCultivationDirectionResult;
    void SetCurrentCultivationDirection(const TSharedPtr<idlezt::SetCurrentCultivationDirectionReq>& InReqMessage, const OnSetCurrentCultivationDirectionResult& InCallback);    

    /**
     * 突破
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DoBreakthrough")
    void K2_DoBreakthrough(const FZDoBreakthroughReq& InParams, const FZOnDoBreakthroughResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DoBreakthroughAck>&)> OnDoBreakthroughResult;
    void DoBreakthrough(const TSharedPtr<idlezt::DoBreakthroughReq>& InReqMessage, const OnDoBreakthroughResult& InCallback);    

    /**
     * 请求公共修炼数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestCommonCultivationData")
    void K2_RequestCommonCultivationData(const FZRequestCommonCultivationDataReq& InParams, const FZOnRequestCommonCultivationDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestCommonCultivationDataAck>&)> OnRequestCommonCultivationDataResult;
    void RequestCommonCultivationData(const TSharedPtr<idlezt::RequestCommonCultivationDataReq>& InReqMessage, const OnRequestCommonCultivationDataResult& InCallback);    

    /**
     * 请求合并吐纳
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickMergeBreathing")
    void K2_OneClickMergeBreathing(const FZOneClickMergeBreathingReq& InParams, const FZOnOneClickMergeBreathingResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickMergeBreathingAck>&)> OnOneClickMergeBreathingResult;
    void OneClickMergeBreathing(const TSharedPtr<idlezt::OneClickMergeBreathingReq>& InReqMessage, const OnOneClickMergeBreathingResult& InCallback);    

    /**
     * 请求领取吐纳奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReceiveBreathingExerciseReward")
    void K2_ReceiveBreathingExerciseReward(const FZReceiveBreathingExerciseRewardReq& InParams, const FZOnReceiveBreathingExerciseRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReceiveBreathingExerciseRewardAck>&)> OnReceiveBreathingExerciseRewardResult;
    void ReceiveBreathingExerciseReward(const TSharedPtr<idlezt::ReceiveBreathingExerciseRewardReq>& InReqMessage, const OnReceiveBreathingExerciseRewardResult& InCallback);    

    /**
     * 请求包裹数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetInventoryData")
    void K2_GetInventoryData(const FZGetInventoryDataReq& InParams, const FZOnGetInventoryDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetInventoryDataAck>&)> OnGetInventoryDataResult;
    void GetInventoryData(const TSharedPtr<idlezt::GetInventoryDataReq>& InReqMessage, const OnGetInventoryDataResult& InCallback);    

    /**
     * 请求任务数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetQuestData")
    void K2_GetQuestData(const FZGetQuestDataReq& InParams, const FZOnGetQuestDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetQuestDataAck>&)> OnGetQuestDataResult;
    void GetQuestData(const TSharedPtr<idlezt::GetQuestDataReq>& InReqMessage, const OnGetQuestDataResult& InCallback);    

    /**
     * 创建角色
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="CreateCharacter")
    void K2_CreateCharacter(const FZCreateCharacterReq& InParams, const FZOnCreateCharacterResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::CreateCharacterAck>&)> OnCreateCharacterResult;
    void CreateCharacter(const TSharedPtr<idlezt::CreateCharacterReq>& InReqMessage, const OnCreateCharacterResult& InCallback);    

    /**
     * 使用道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UseItem")
    void K2_UseItem(const FZUseItemReq& InParams, const FZOnUseItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UseItemAck>&)> OnUseItemResult;
    void UseItem(const TSharedPtr<idlezt::UseItemReq>& InReqMessage, const OnUseItemResult& InCallback);    

    /**
     * 使用自选宝箱
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UseSelectGift")
    void K2_UseSelectGift(const FZUseSelectGiftReq& InParams, const FZOnUseSelectGiftResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UseSelectGiftAck>&)> OnUseSelectGiftResult;
    void UseSelectGift(const TSharedPtr<idlezt::UseSelectGiftReq>& InReqMessage, const OnUseSelectGiftResult& InCallback);    

    /**
     * 出售道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SellItem")
    void K2_SellItem(const FZSellItemReq& InParams, const FZOnSellItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SellItemAck>&)> OnSellItemResult;
    void SellItem(const TSharedPtr<idlezt::SellItemReq>& InReqMessage, const OnSellItemResult& InCallback);    

    /**
     * 解锁装备槽位
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UnlockEquipmentSlot")
    void K2_UnlockEquipmentSlot(const FZUnlockEquipmentSlotReq& InParams, const FZOnUnlockEquipmentSlotResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UnlockEquipmentSlotAck>&)> OnUnlockEquipmentSlotResult;
    void UnlockEquipmentSlot(const TSharedPtr<idlezt::UnlockEquipmentSlotReq>& InReqMessage, const OnUnlockEquipmentSlotResult& InCallback);    

    /**
     * 开始炼丹
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AlchemyRefineStart")
    void K2_AlchemyRefineStart(const FZAlchemyRefineStartReq& InParams, const FZOnAlchemyRefineStartResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AlchemyRefineStartAck>&)> OnAlchemyRefineStartResult;
    void AlchemyRefineStart(const TSharedPtr<idlezt::AlchemyRefineStartReq>& InReqMessage, const OnAlchemyRefineStartResult& InCallback);    

    /**
     * 终止炼丹
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AlchemyRefineCancel")
    void K2_AlchemyRefineCancel(const FZAlchemyRefineCancelReq& InParams, const FZOnAlchemyRefineCancelResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AlchemyRefineCancelAck>&)> OnAlchemyRefineCancelResult;
    void AlchemyRefineCancel(const TSharedPtr<idlezt::AlchemyRefineCancelReq>& InReqMessage, const OnAlchemyRefineCancelResult& InCallback);    

    /**
     * 领取丹药
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AlchemyRefineExtract")
    void K2_AlchemyRefineExtract(const FZAlchemyRefineExtractReq& InParams, const FZOnAlchemyRefineExtractResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AlchemyRefineExtractAck>&)> OnAlchemyRefineExtractResult;
    void AlchemyRefineExtract(const TSharedPtr<idlezt::AlchemyRefineExtractReq>& InReqMessage, const OnAlchemyRefineExtractResult& InCallback);    

    /**
     * 获取坊市数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleShopData")
    void K2_GetRoleShopData(const FZGetRoleShopDataReq& InParams, const FZOnGetRoleShopDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleShopDataAck>&)> OnGetRoleShopDataResult;
    void GetRoleShopData(const TSharedPtr<idlezt::GetRoleShopDataReq>& InReqMessage, const OnGetRoleShopDataResult& InCallback);    

    /**
     * 手动刷新坊市
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RefreshShop")
    void K2_RefreshShop(const FZRefreshShopReq& InParams, const FZOnRefreshShopResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RefreshShopAck>&)> OnRefreshShopResult;
    void RefreshShop(const TSharedPtr<idlezt::RefreshShopReq>& InReqMessage, const OnRefreshShopResult& InCallback);    

    /**
     * 购买坊市道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="BuyShopItem")
    void K2_BuyShopItem(const FZBuyShopItemReq& InParams, const FZOnBuyShopItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::BuyShopItemAck>&)> OnBuyShopItemResult;
    void BuyShopItem(const TSharedPtr<idlezt::BuyShopItemReq>& InReqMessage, const OnBuyShopItemResult& InCallback);    

    /**
     * 获取天机阁数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleDeluxeShopData")
    void K2_GetRoleDeluxeShopData(const FZGetRoleDeluxeShopDataReq& InParams, const FZOnGetRoleDeluxeShopDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleDeluxeShopDataAck>&)> OnGetRoleDeluxeShopDataResult;
    void GetRoleDeluxeShopData(const TSharedPtr<idlezt::GetRoleDeluxeShopDataReq>& InReqMessage, const OnGetRoleDeluxeShopDataResult& InCallback);    

    /**
     * 手动刷新天机阁
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RefreshDeluxeShop")
    void K2_RefreshDeluxeShop(const FZRefreshDeluxeShopReq& InParams, const FZOnRefreshDeluxeShopResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RefreshDeluxeShopAck>&)> OnRefreshDeluxeShopResult;
    void RefreshDeluxeShop(const TSharedPtr<idlezt::RefreshDeluxeShopReq>& InReqMessage, const OnRefreshDeluxeShopResult& InCallback);    

    /**
     * 购买天机阁道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="BuyDeluxeShopItem")
    void K2_BuyDeluxeShopItem(const FZBuyDeluxeShopItemReq& InParams, const FZOnBuyDeluxeShopItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::BuyDeluxeShopItemAck>&)> OnBuyDeluxeShopItemResult;
    void BuyDeluxeShopItem(const TSharedPtr<idlezt::BuyDeluxeShopItemReq>& InReqMessage, const OnBuyDeluxeShopItemResult& InCallback);    

    /**
     * 获取临时包裹数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetTemporaryPackageData")
    void K2_GetTemporaryPackageData(const FZGetTemporaryPackageDataReq& InParams, const FZOnGetTemporaryPackageDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetTemporaryPackageDataAck>&)> OnGetTemporaryPackageDataResult;
    void GetTemporaryPackageData(const TSharedPtr<idlezt::GetTemporaryPackageDataReq>& InReqMessage, const OnGetTemporaryPackageDataResult& InCallback);    

    /**
     * 提取临时包裹中的道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ExtractTemporaryPackageItems")
    void K2_ExtractTemporaryPackageItems(const FZExtractTemporaryPackageItemsReq& InParams, const FZOnExtractTemporaryPackageItemsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ExtractTemporaryPackageItemsAck>&)> OnExtractTemporaryPackageItemsResult;
    void ExtractTemporaryPackageItems(const TSharedPtr<idlezt::ExtractTemporaryPackageItemsReq>& InReqMessage, const OnExtractTemporaryPackageItemsResult& InCallback);    

    /**
     * 加速重生
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SpeedupRelive")
    void K2_SpeedupRelive(const FZSpeedupReliveReq& InParams, const FZOnSpeedupReliveResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SpeedupReliveAck>&)> OnSpeedupReliveResult;
    void SpeedupRelive(const TSharedPtr<idlezt::SpeedupReliveReq>& InReqMessage, const OnSpeedupReliveResult& InCallback);    

    /**
     * 获取地图信息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetMapInfo")
    void K2_GetMapInfo(const FZGetMapInfoReq& InParams, const FZOnGetMapInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetMapInfoAck>&)> OnGetMapInfoResult;
    void GetMapInfo(const TSharedPtr<idlezt::GetMapInfoReq>& InReqMessage, const OnGetMapInfoResult& InCallback);    

    /**
     * 解锁指定秘境
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UnlockArena")
    void K2_UnlockArena(const FZUnlockArenaReq& InParams, const FZOnUnlockArenaResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UnlockArenaAck>&)> OnUnlockArenaResult;
    void UnlockArena(const TSharedPtr<idlezt::UnlockArenaReq>& InReqMessage, const OnUnlockArenaResult& InCallback);    

    /**
     * 请求任务操作
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="QuestOp")
    void K2_QuestOp(const FZQuestOpReq& InParams, const FZOnQuestOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::QuestOpAck>&)> OnQuestOpResult;
    void QuestOp(const TSharedPtr<idlezt::QuestOpReq>& InReqMessage, const OnQuestOpResult& InCallback);    

    /**
     * 穿装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="EquipmentPutOn")
    void K2_EquipmentPutOn(const FZEquipmentPutOnReq& InParams, const FZOnEquipmentPutOnResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::EquipmentPutOnAck>&)> OnEquipmentPutOnResult;
    void EquipmentPutOn(const TSharedPtr<idlezt::EquipmentPutOnReq>& InReqMessage, const OnEquipmentPutOnResult& InCallback);    

    /**
     * 脱装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="EquipmentTakeOff")
    void K2_EquipmentTakeOff(const FZEquipmentTakeOffReq& InParams, const FZOnEquipmentTakeOffResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::EquipmentTakeOffAck>&)> OnEquipmentTakeOffResult;
    void EquipmentTakeOff(const TSharedPtr<idlezt::EquipmentTakeOffReq>& InReqMessage, const OnEquipmentTakeOffResult& InCallback);    

    /**
     * 请求排行榜预览，每个榜的榜一数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetLeaderboardPreview")
    void K2_GetLeaderboardPreview(const FZGetLeaderboardPreviewReq& InParams, const FZOnGetLeaderboardPreviewResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetLeaderboardPreviewAck>&)> OnGetLeaderboardPreviewResult;
    void GetLeaderboardPreview(const TSharedPtr<idlezt::GetLeaderboardPreviewReq>& InReqMessage, const OnGetLeaderboardPreviewResult& InCallback);    

    /**
     * 请求排行榜数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetLeaderboardData")
    void K2_GetLeaderboardData(const FZGetLeaderboardDataReq& InParams, const FZOnGetLeaderboardDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetLeaderboardDataAck>&)> OnGetLeaderboardDataResult;
    void GetLeaderboardData(const TSharedPtr<idlezt::GetLeaderboardDataReq>& InReqMessage, const OnGetLeaderboardDataResult& InCallback);    

    /**
     * 请求单个玩家排行榜数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleLeaderboardData")
    void K2_GetRoleLeaderboardData(const FZGetRoleLeaderboardDataReq& InParams, const FZOnGetRoleLeaderboardDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleLeaderboardDataAck>&)> OnGetRoleLeaderboardDataResult;
    void GetRoleLeaderboardData(const TSharedPtr<idlezt::GetRoleLeaderboardDataReq>& InReqMessage, const OnGetRoleLeaderboardDataResult& InCallback);    

    /**
     * 请求排行榜点赞
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="LeaderboardClickLike")
    void K2_LeaderboardClickLike(const FZLeaderboardClickLikeReq& InParams, const FZOnLeaderboardClickLikeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::LeaderboardClickLikeAck>&)> OnLeaderboardClickLikeResult;
    void LeaderboardClickLike(const TSharedPtr<idlezt::LeaderboardClickLikeReq>& InReqMessage, const OnLeaderboardClickLikeResult& InCallback);    

    /**
     * 请求排行榜更新留言
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="LeaderboardUpdateMessage")
    void K2_LeaderboardUpdateMessage(const FZLeaderboardUpdateMessageReq& InParams, const FZOnLeaderboardUpdateMessageResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::LeaderboardUpdateMessageAck>&)> OnLeaderboardUpdateMessageResult;
    void LeaderboardUpdateMessage(const TSharedPtr<idlezt::LeaderboardUpdateMessageReq>& InReqMessage, const OnLeaderboardUpdateMessageResult& InCallback);    

    /**
     * 请求领取福泽奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetFuZeReward")
    void K2_GetFuZeReward(const FZGetFuZeRewardReq& InParams, const FZOnGetFuZeRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetFuZeRewardAck>&)> OnGetFuZeRewardResult;
    void GetFuZeReward(const TSharedPtr<idlezt::GetFuZeRewardReq>& InReqMessage, const OnGetFuZeRewardResult& InCallback);    

    /**
     * 请求邮箱数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleMailData")
    void K2_GetRoleMailData(const FZGetRoleMailDataReq& InParams, const FZOnGetRoleMailDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleMailDataAck>&)> OnGetRoleMailDataResult;
    void GetRoleMailData(const TSharedPtr<idlezt::GetRoleMailDataReq>& InReqMessage, const OnGetRoleMailDataResult& InCallback);    

    /**
     * 请求邮箱已读
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReadMail")
    void K2_ReadMail(const FZReadMailReq& InParams, const FZOnReadMailResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReadMailAck>&)> OnReadMailResult;
    void ReadMail(const TSharedPtr<idlezt::ReadMailReq>& InReqMessage, const OnReadMailResult& InCallback);    

    /**
     * 请求邮箱领取
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetMailAttachment")
    void K2_GetMailAttachment(const FZGetMailAttachmentReq& InParams, const FZOnGetMailAttachmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetMailAttachmentAck>&)> OnGetMailAttachmentResult;
    void GetMailAttachment(const TSharedPtr<idlezt::GetMailAttachmentReq>& InReqMessage, const OnGetMailAttachmentResult& InCallback);    

    /**
     * 请求删除邮件
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DeleteMail")
    void K2_DeleteMail(const FZDeleteMailReq& InParams, const FZOnDeleteMailResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DeleteMailAck>&)> OnDeleteMailResult;
    void DeleteMail(const TSharedPtr<idlezt::DeleteMailReq>& InReqMessage, const OnDeleteMailResult& InCallback);    

    /**
     * 请求邮件一键领取
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickGetMailAttachment")
    void K2_OneClickGetMailAttachment(const FZOneClickGetMailAttachmentReq& InParams, const FZOnOneClickGetMailAttachmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickGetMailAttachmentAck>&)> OnOneClickGetMailAttachmentResult;
    void OneClickGetMailAttachment(const TSharedPtr<idlezt::OneClickGetMailAttachmentReq>& InReqMessage, const OnOneClickGetMailAttachmentResult& InCallback);    

    /**
     * 请求邮件一键已读
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickReadMail")
    void K2_OneClickReadMail(const FZOneClickReadMailReq& InParams, const FZOnOneClickReadMailResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickReadMailAck>&)> OnOneClickReadMailResult;
    void OneClickReadMail(const TSharedPtr<idlezt::OneClickReadMailReq>& InReqMessage, const OnOneClickReadMailResult& InCallback);    

    /**
     * 请求邮件一键删除
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickDeleteMail")
    void K2_OneClickDeleteMail(const FZOneClickDeleteMailReq& InParams, const FZOnOneClickDeleteMailResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickDeleteMailAck>&)> OnOneClickDeleteMailResult;
    void OneClickDeleteMail(const TSharedPtr<idlezt::OneClickDeleteMailReq>& InReqMessage, const OnOneClickDeleteMailResult& InCallback);    

    /**
     * 解锁指定模块
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UnlockFunctionModule")
    void K2_UnlockFunctionModule(const FZUnlockFunctionModuleReq& InParams, const FZOnUnlockFunctionModuleResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UnlockFunctionModuleAck>&)> OnUnlockFunctionModuleResult;
    void UnlockFunctionModule(const TSharedPtr<idlezt::UnlockFunctionModuleReq>& InReqMessage, const OnUnlockFunctionModuleResult& InCallback);    

    /**
     * 请求聊天消息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetChatRecord")
    void K2_GetChatRecord(const FZGetChatRecordReq& InParams, const FZOnGetChatRecordResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetChatRecordAck>&)> OnGetChatRecordResult;
    void GetChatRecord(const TSharedPtr<idlezt::GetChatRecordReq>& InReqMessage, const OnGetChatRecordResult& InCallback);    

    /**
     * 请求删除私聊消息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DeletePrivateChatRecord")
    void K2_DeletePrivateChatRecord(const FZDeletePrivateChatRecordReq& InParams, const FZOnDeletePrivateChatRecordResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DeletePrivateChatRecordAck>&)> OnDeletePrivateChatRecordResult;
    void DeletePrivateChatRecord(const TSharedPtr<idlezt::DeletePrivateChatRecordReq>& InReqMessage, const OnDeletePrivateChatRecordResult& InCallback);    

    /**
     * 发送聊天消息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SendChatMessage")
    void K2_SendChatMessage(const FZSendChatMessageReq& InParams, const FZOnSendChatMessageResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SendChatMessageAck>&)> OnSendChatMessageResult;
    void SendChatMessage(const TSharedPtr<idlezt::SendChatMessageReq>& InReqMessage, const OnSendChatMessageResult& InCallback);    

    /**
     * 请求聊天记录已读
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ClearChatUnreadNum")
    void K2_ClearChatUnreadNum(const FZClearChatUnreadNumReq& InParams, const FZOnClearChatUnreadNumResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ClearChatUnreadNumAck>&)> OnClearChatUnreadNumResult;
    void ClearChatUnreadNum(const TSharedPtr<idlezt::ClearChatUnreadNumReq>& InReqMessage, const OnClearChatUnreadNumResult& InCallback);    

    /**
     * 开始炼器
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ForgeRefineStart")
    void K2_ForgeRefineStart(const FZForgeRefineStartReq& InParams, const FZOnForgeRefineStartResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ForgeRefineStartAck>&)> OnForgeRefineStartResult;
    void ForgeRefineStart(const TSharedPtr<idlezt::ForgeRefineStartReq>& InReqMessage, const OnForgeRefineStartResult& InCallback);    

    /**
     * 终止炼器
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ForgeRefineCancel")
    void K2_ForgeRefineCancel(const FZForgeRefineCancelReq& InParams, const FZOnForgeRefineCancelResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ForgeRefineCancelAck>&)> OnForgeRefineCancelResult;
    void ForgeRefineCancel(const TSharedPtr<idlezt::ForgeRefineCancelReq>& InReqMessage, const OnForgeRefineCancelResult& InCallback);    

    /**
     * 领取炼器生成的道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ForgeRefineExtract")
    void K2_ForgeRefineExtract(const FZForgeRefineExtractReq& InParams, const FZOnForgeRefineExtractResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ForgeRefineExtractAck>&)> OnForgeRefineExtractResult;
    void ForgeRefineExtract(const TSharedPtr<idlezt::ForgeRefineExtractReq>& InReqMessage, const OnForgeRefineExtractResult& InCallback);    

    /**
     * 请求找回装备数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetForgeLostEquipmentData")
    void K2_GetForgeLostEquipmentData(const FZGetForgeLostEquipmentDataReq& InParams, const FZOnGetForgeLostEquipmentDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetForgeLostEquipmentDataAck>&)> OnGetForgeLostEquipmentDataResult;
    void GetForgeLostEquipmentData(const TSharedPtr<idlezt::GetForgeLostEquipmentDataReq>& InReqMessage, const OnGetForgeLostEquipmentDataResult& InCallback);    

    /**
     * 请求销毁装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ForgeDestroy")
    void K2_ForgeDestroy(const FZForgeDestroyReq& InParams, const FZOnForgeDestroyResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ForgeDestroyAck>&)> OnForgeDestroyResult;
    void ForgeDestroy(const TSharedPtr<idlezt::ForgeDestroyReq>& InReqMessage, const OnForgeDestroyResult& InCallback);    

    /**
     * 请求找回装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ForgeFindBack")
    void K2_ForgeFindBack(const FZForgeFindBackReq& InParams, const FZOnForgeFindBackResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ForgeFindBackAck>&)> OnForgeFindBackResult;
    void ForgeFindBack(const TSharedPtr<idlezt::ForgeFindBackReq>& InReqMessage, const OnForgeFindBackResult& InCallback);    

    /**
     * 请求秘药数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestPillElixirData")
    void K2_RequestPillElixirData(const FZRequestPillElixirDataReq& InParams, const FZOnRequestPillElixirDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestPillElixirDataAck>&)> OnRequestPillElixirDataResult;
    void RequestPillElixirData(const TSharedPtr<idlezt::RequestPillElixirDataReq>& InReqMessage, const OnRequestPillElixirDataResult& InCallback);    

    /**
     * 请求单种秘药数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetOnePillElixirData")
    void K2_GetOnePillElixirData(const FZGetOnePillElixirDataReq& InParams, const FZOnGetOnePillElixirDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetOnePillElixirDataAck>&)> OnGetOnePillElixirDataResult;
    void GetOnePillElixirData(const TSharedPtr<idlezt::GetOnePillElixirDataReq>& InReqMessage, const OnGetOnePillElixirDataResult& InCallback);    

    /**
     * 请求修改秘药过滤配置
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestModifyPillElixirFilter")
    void K2_RequestModifyPillElixirFilter(const FZRequestModifyPillElixirFilterReq& InParams, const FZOnRequestModifyPillElixirFilterResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestModifyPillElixirFilterAck>&)> OnRequestModifyPillElixirFilterResult;
    void RequestModifyPillElixirFilter(const TSharedPtr<idlezt::RequestModifyPillElixirFilterReq>& InReqMessage, const OnRequestModifyPillElixirFilterResult& InCallback);    

    /**
     * 使用单颗秘药
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UsePillElixir")
    void K2_UsePillElixir(const FZUsePillElixirReq& InParams, const FZOnUsePillElixirResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UsePillElixirAck>&)> OnUsePillElixirResult;
    void UsePillElixir(const TSharedPtr<idlezt::UsePillElixirReq>& InReqMessage, const OnUsePillElixirResult& InCallback);    

    /**
     * 一键使用秘药
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickUsePillElixir")
    void K2_OneClickUsePillElixir(const FZOneClickUsePillElixirReq& InParams, const FZOnOneClickUsePillElixirResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickUsePillElixirAck>&)> OnOneClickUsePillElixirResult;
    void OneClickUsePillElixir(const TSharedPtr<idlezt::OneClickUsePillElixirReq>& InReqMessage, const OnOneClickUsePillElixirResult& InCallback);    

    /**
     * 请求秘药兑换天机石
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="TradePillElixir")
    void K2_TradePillElixir(const FZTradePillElixirReq& InParams, const FZOnTradePillElixirResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::TradePillElixirAck>&)> OnTradePillElixirResult;
    void TradePillElixir(const TSharedPtr<idlezt::TradePillElixirReq>& InReqMessage, const OnTradePillElixirResult& InCallback);    

    /**
     * 请求强化装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReinforceEquipment")
    void K2_ReinforceEquipment(const FZReinforceEquipmentReq& InParams, const FZOnReinforceEquipmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReinforceEquipmentAck>&)> OnReinforceEquipmentResult;
    void ReinforceEquipment(const TSharedPtr<idlezt::ReinforceEquipmentReq>& InReqMessage, const OnReinforceEquipmentResult& InCallback);    

    /**
     * 请求精炼装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RefineEquipment")
    void K2_RefineEquipment(const FZRefineEquipmentReq& InParams, const FZOnRefineEquipmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RefineEquipmentAck>&)> OnRefineEquipmentResult;
    void RefineEquipment(const TSharedPtr<idlezt::RefineEquipmentReq>& InReqMessage, const OnRefineEquipmentResult& InCallback);    

    /**
     * 请求器纹装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="QiWenEquipment")
    void K2_QiWenEquipment(const FZQiWenEquipmentReq& InParams, const FZOnQiWenEquipmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::QiWenEquipmentAck>&)> OnQiWenEquipmentResult;
    void QiWenEquipment(const TSharedPtr<idlezt::QiWenEquipmentReq>& InReqMessage, const OnQiWenEquipmentResult& InCallback);    

    /**
     * 请求还原装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ResetEquipment")
    void K2_ResetEquipment(const FZResetEquipmentReq& InParams, const FZOnResetEquipmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ResetEquipmentAck>&)> OnResetEquipmentResult;
    void ResetEquipment(const TSharedPtr<idlezt::ResetEquipmentReq>& InReqMessage, const OnResetEquipmentResult& InCallback);    

    /**
     * 请求继承装备
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="InheritEquipment")
    void K2_InheritEquipment(const FZInheritEquipmentReq& InParams, const FZOnInheritEquipmentResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::InheritEquipmentAck>&)> OnInheritEquipmentResult;
    void InheritEquipment(const TSharedPtr<idlezt::InheritEquipmentReq>& InReqMessage, const OnInheritEquipmentResult& InCallback);    

    /**
     * 请求锁定/解锁道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="LockItem")
    void K2_LockItem(const FZLockItemReq& InParams, const FZOnLockItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::LockItemAck>&)> OnLockItemResult;
    void LockItem(const TSharedPtr<idlezt::LockItemReq>& InReqMessage, const OnLockItemResult& InCallback);    

    /**
     * 发起切磋
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SoloArenaChallenge")
    void K2_SoloArenaChallenge(const FZSoloArenaChallengeReq& InParams, const FZOnSoloArenaChallengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SoloArenaChallengeAck>&)> OnSoloArenaChallengeResult;
    void SoloArenaChallenge(const TSharedPtr<idlezt::SoloArenaChallengeReq>& InReqMessage, const OnSoloArenaChallengeResult& InCallback);    

    /**
     * 快速结束切磋
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SoloArenaQuickEnd")
    void K2_SoloArenaQuickEnd(const FZSoloArenaQuickEndReq& InParams, const FZOnSoloArenaQuickEndResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SoloArenaQuickEndAck>&)> OnSoloArenaQuickEndResult;
    void SoloArenaQuickEnd(const TSharedPtr<idlezt::SoloArenaQuickEndReq>& InReqMessage, const OnSoloArenaQuickEndResult& InCallback);    

    /**
     * 获取切磋历史列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSoloArenaHistoryList")
    void K2_GetSoloArenaHistoryList(const FZGetSoloArenaHistoryListReq& InParams, const FZOnGetSoloArenaHistoryListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSoloArenaHistoryListAck>&)> OnGetSoloArenaHistoryListResult;
    void GetSoloArenaHistoryList(const TSharedPtr<idlezt::GetSoloArenaHistoryListReq>& InReqMessage, const OnGetSoloArenaHistoryListResult& InCallback);    

    /**
     * 挑战镇妖塔
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="MonsterTowerChallenge")
    void K2_MonsterTowerChallenge(const FZMonsterTowerChallengeReq& InParams, const FZOnMonsterTowerChallengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::MonsterTowerChallengeAck>&)> OnMonsterTowerChallengeResult;
    void MonsterTowerChallenge(const TSharedPtr<idlezt::MonsterTowerChallengeReq>& InReqMessage, const OnMonsterTowerChallengeResult& InCallback);    

    /**
     * 领取镇妖塔挂机奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="MonsterTowerDrawIdleAward")
    void K2_MonsterTowerDrawIdleAward(const FZMonsterTowerDrawIdleAwardReq& InParams, const FZOnMonsterTowerDrawIdleAwardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::MonsterTowerDrawIdleAwardAck>&)> OnMonsterTowerDrawIdleAwardResult;
    void MonsterTowerDrawIdleAward(const TSharedPtr<idlezt::MonsterTowerDrawIdleAwardReq>& InReqMessage, const OnMonsterTowerDrawIdleAwardResult& InCallback);    

    /**
     * 镇妖塔闭关
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="MonsterTowerClosedDoorTraining")
    void K2_MonsterTowerClosedDoorTraining(const FZMonsterTowerClosedDoorTrainingReq& InParams, const FZOnMonsterTowerClosedDoorTrainingResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingAck>&)> OnMonsterTowerClosedDoorTrainingResult;
    void MonsterTowerClosedDoorTraining(const TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingReq>& InReqMessage, const OnMonsterTowerClosedDoorTrainingResult& InCallback);    

    /**
     * 镇妖塔快速结束
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="MonsterTowerQuickEnd")
    void K2_MonsterTowerQuickEnd(const FZMonsterTowerQuickEndReq& InParams, const FZOnMonsterTowerQuickEndResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::MonsterTowerQuickEndAck>&)> OnMonsterTowerQuickEndResult;
    void MonsterTowerQuickEnd(const TSharedPtr<idlezt::MonsterTowerQuickEndReq>& InReqMessage, const OnMonsterTowerQuickEndResult& InCallback);    

    /**
     * 镇妖塔挑战榜数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetMonsterTowerChallengeList")
    void K2_GetMonsterTowerChallengeList(const FZGetMonsterTowerChallengeListReq& InParams, const FZOnGetMonsterTowerChallengeListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetMonsterTowerChallengeListAck>&)> OnGetMonsterTowerChallengeListResult;
    void GetMonsterTowerChallengeList(const TSharedPtr<idlezt::GetMonsterTowerChallengeListReq>& InReqMessage, const OnGetMonsterTowerChallengeListResult& InCallback);    

    /**
     * 镇妖塔挑战榜奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetMonsterTowerChallengeReward")
    void K2_GetMonsterTowerChallengeReward(const FZGetMonsterTowerChallengeRewardReq& InParams, const FZOnGetMonsterTowerChallengeRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetMonsterTowerChallengeRewardAck>&)> OnGetMonsterTowerChallengeRewardResult;
    void GetMonsterTowerChallengeReward(const TSharedPtr<idlezt::GetMonsterTowerChallengeRewardReq>& InReqMessage, const OnGetMonsterTowerChallengeRewardResult& InCallback);    

    /**
     * 设置地图TimeDilation
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetWorldTimeDilation")
    void K2_SetWorldTimeDilation(const FZSetWorldTimeDilationReq& InParams, const FZOnSetWorldTimeDilationResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetWorldTimeDilationAck>&)> OnSetWorldTimeDilationResult;
    void SetWorldTimeDilation(const TSharedPtr<idlezt::SetWorldTimeDilationReq>& InReqMessage, const OnSetWorldTimeDilationResult& InCallback);    

    /**
     * 设置战斗模式
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetFightMode")
    void K2_SetFightMode(const FZSetFightModeReq& InParams, const FZOnSetFightModeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetFightModeAck>&)> OnSetFightModeResult;
    void SetFightMode(const TSharedPtr<idlezt::SetFightModeReq>& InReqMessage, const OnSetFightModeResult& InCallback);    

    /**
     * 升级聚灵阵
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UpgradeQiCollector")
    void K2_UpgradeQiCollector(const FZUpgradeQiCollectorReq& InParams, const FZOnUpgradeQiCollectorResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UpgradeQiCollectorAck>&)> OnUpgradeQiCollectorResult;
    void UpgradeQiCollector(const TSharedPtr<idlezt::UpgradeQiCollectorReq>& InReqMessage, const OnUpgradeQiCollectorResult& InCallback);    

    /**
     * 请求玩家的游戏数值数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleAllStats")
    void K2_GetRoleAllStats(const FZGetRoleAllStatsReq& InParams, const FZOnGetRoleAllStatsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleAllStatsAck>&)> OnGetRoleAllStatsResult;
    void GetRoleAllStats(const TSharedPtr<idlezt::GetRoleAllStatsReq>& InReqMessage, const OnGetRoleAllStatsResult& InCallback);    

    /**
     * 请求玩家山河图数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetShanhetuData")
    void K2_GetShanhetuData(const FZGetShanhetuDataReq& InParams, const FZOnGetShanhetuDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetShanhetuDataAck>&)> OnGetShanhetuDataResult;
    void GetShanhetuData(const TSharedPtr<idlezt::GetShanhetuDataReq>& InReqMessage, const OnGetShanhetuDataResult& InCallback);    

    /**
     * 请求修改山河图使用配置
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetShanhetuUseConfig")
    void K2_SetShanhetuUseConfig(const FZSetShanhetuUseConfigReq& InParams, const FZOnSetShanhetuUseConfigResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetShanhetuUseConfigAck>&)> OnSetShanhetuUseConfigResult;
    void SetShanhetuUseConfig(const TSharedPtr<idlezt::SetShanhetuUseConfigReq>& InReqMessage, const OnSetShanhetuUseConfigResult& InCallback);    

    /**
     * 请求使用山河图
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UseShanhetu")
    void K2_UseShanhetu(const FZUseShanhetuReq& InParams, const FZOnUseShanhetuResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UseShanhetuAck>&)> OnUseShanhetuResult;
    void UseShanhetu(const TSharedPtr<idlezt::UseShanhetuReq>& InReqMessage, const OnUseShanhetuResult& InCallback);    

    /**
     * 探索山河图
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="StepShanhetu")
    void K2_StepShanhetu(const FZStepShanhetuReq& InParams, const FZOnStepShanhetuResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::StepShanhetuAck>&)> OnStepShanhetuResult;
    void StepShanhetu(const TSharedPtr<idlezt::StepShanhetuReq>& InReqMessage, const OnStepShanhetuResult& InCallback);    

    /**
     * 请求山河图记录
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetShanhetuUseRecord")
    void K2_GetShanhetuUseRecord(const FZGetShanhetuUseRecordReq& InParams, const FZOnGetShanhetuUseRecordResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetShanhetuUseRecordAck>&)> OnGetShanhetuUseRecordResult;
    void GetShanhetuUseRecord(const TSharedPtr<idlezt::GetShanhetuUseRecordReq>& InReqMessage, const OnGetShanhetuUseRecordResult& InCallback);    

    /**
     * 设置锁定方式
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetAttackLockType")
    void K2_SetAttackLockType(const FZSetAttackLockTypeReq& InParams, const FZOnSetAttackLockTypeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetAttackLockTypeAck>&)> OnSetAttackLockTypeResult;
    void SetAttackLockType(const TSharedPtr<idlezt::SetAttackLockTypeReq>& InReqMessage, const OnSetAttackLockTypeResult& InCallback);    

    /**
     * 设置取消锁定方式
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetAttackUnlockType")
    void K2_SetAttackUnlockType(const FZSetAttackUnlockTypeReq& InParams, const FZOnSetAttackUnlockTypeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetAttackUnlockTypeAck>&)> OnSetAttackUnlockTypeResult;
    void SetAttackUnlockType(const TSharedPtr<idlezt::SetAttackUnlockTypeReq>& InReqMessage, const OnSetAttackUnlockTypeResult& InCallback);    

    /**
     * 设置是否显示解锁按钮
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetShowUnlockButton")
    void K2_SetShowUnlockButton(const FZSetShowUnlockButtonReq& InParams, const FZOnSetShowUnlockButtonResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetShowUnlockButtonAck>&)> OnSetShowUnlockButtonResult;
    void SetShowUnlockButton(const TSharedPtr<idlezt::SetShowUnlockButtonReq>& InReqMessage, const OnSetShowUnlockButtonResult& InCallback);    

    /**
     * 获取用户变量内容
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetUserVar")
    void K2_GetUserVar(const FZGetUserVarReq& InParams, const FZOnGetUserVarResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetUserVarRsp>&)> OnGetUserVarResult;
    void GetUserVar(const TSharedPtr<idlezt::GetUserVarReq>& InReqMessage, const OnGetUserVarResult& InCallback);    

    /**
     * 获取多个用户变量内容
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetUserVars")
    void K2_GetUserVars(const FZGetUserVarsReq& InParams, const FZOnGetUserVarsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetUserVarsRsp>&)> OnGetUserVarsResult;
    void GetUserVars(const TSharedPtr<idlezt::GetUserVarsReq>& InReqMessage, const OnGetUserVarsResult& InCallback);    

    /**
     * 获取指定秘境BOSS入侵情况
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetBossInvasionArenaSummary")
    void K2_GetBossInvasionArenaSummary(const FZGetBossInvasionArenaSummaryReq& InParams, const FZOnGetBossInvasionArenaSummaryResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetBossInvasionArenaSummaryRsp>&)> OnGetBossInvasionArenaSummaryResult;
    void GetBossInvasionArenaSummary(const TSharedPtr<idlezt::GetBossInvasionArenaSummaryReq>& InReqMessage, const OnGetBossInvasionArenaSummaryResult& InCallback);    

    /**
     * 获取指定秘境BOSS入侵伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetBossInvasionArenaTopList")
    void K2_GetBossInvasionArenaTopList(const FZGetBossInvasionArenaTopListReq& InParams, const FZOnGetBossInvasionArenaTopListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetBossInvasionArenaTopListRsp>&)> OnGetBossInvasionArenaTopListResult;
    void GetBossInvasionArenaTopList(const TSharedPtr<idlezt::GetBossInvasionArenaTopListReq>& InReqMessage, const OnGetBossInvasionArenaTopListResult& InCallback);    

    /**
     * 获取BOSS入侵情况
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetBossInvasionInfo")
    void K2_GetBossInvasionInfo(const FZGetBossInvasionInfoReq& InParams, const FZOnGetBossInvasionInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetBossInvasionInfoRsp>&)> OnGetBossInvasionInfoResult;
    void GetBossInvasionInfo(const TSharedPtr<idlezt::GetBossInvasionInfoReq>& InReqMessage, const OnGetBossInvasionInfoResult& InCallback);    

    /**
     * 领取击杀奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DrawBossInvasionKillReward")
    void K2_DrawBossInvasionKillReward(const FZDrawBossInvasionKillRewardReq& InParams, const FZOnDrawBossInvasionKillRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DrawBossInvasionKillRewardRsp>&)> OnDrawBossInvasionKillRewardResult;
    void DrawBossInvasionKillReward(const TSharedPtr<idlezt::DrawBossInvasionKillRewardReq>& InReqMessage, const OnDrawBossInvasionKillRewardResult& InCallback);    

    /**
     * 领取伤害排行奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DrawBossInvasionDamageReward")
    void K2_DrawBossInvasionDamageReward(const FZDrawBossInvasionDamageRewardReq& InParams, const FZOnDrawBossInvasionDamageRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DrawBossInvasionDamageRewardRsp>&)> OnDrawBossInvasionDamageRewardResult;
    void DrawBossInvasionDamageReward(const TSharedPtr<idlezt::DrawBossInvasionDamageRewardReq>& InReqMessage, const OnDrawBossInvasionDamageRewardResult& InCallback);    

    /**
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="BossInvasionTeleport")
    void K2_BossInvasionTeleport(const FZBossInvasionTeleportReq& InParams, const FZOnBossInvasionTeleportResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::BossInvasionTeleportRsp>&)> OnBossInvasionTeleportResult;
    void BossInvasionTeleport(const TSharedPtr<idlezt::BossInvasionTeleportReq>& InReqMessage, const OnBossInvasionTeleportResult& InCallback);    

    /**
     * 分享自己的道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ShareSelfItem")
    void K2_ShareSelfItem(const FZShareSelfItemReq& InParams, const FZOnShareSelfItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ShareSelfItemRsp>&)> OnShareSelfItemResult;
    void ShareSelfItem(const TSharedPtr<idlezt::ShareSelfItemReq>& InReqMessage, const OnShareSelfItemResult& InCallback);    

    /**
     * 分享自己的多个道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ShareSelfItems")
    void K2_ShareSelfItems(const FZShareSelfItemsReq& InParams, const FZOnShareSelfItemsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ShareSelfItemsRsp>&)> OnShareSelfItemsResult;
    void ShareSelfItems(const TSharedPtr<idlezt::ShareSelfItemsReq>& InReqMessage, const OnShareSelfItemsResult& InCallback);    

    /**
     * 获取分享道具数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetShareItemData")
    void K2_GetShareItemData(const FZGetShareItemDataReq& InParams, const FZOnGetShareItemDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetShareItemDataRsp>&)> OnGetShareItemDataResult;
    void GetShareItemData(const TSharedPtr<idlezt::GetShareItemDataReq>& InReqMessage, const OnGetShareItemDataResult& InCallback);    

    /**
     * 获取玩家古宝数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleCollectionData")
    void K2_GetRoleCollectionData(const FZGetRoleCollectionDataReq& InParams, const FZOnGetRoleCollectionDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleCollectionDataRsp>&)> OnGetRoleCollectionDataResult;
    void GetRoleCollectionData(const TSharedPtr<idlezt::GetRoleCollectionDataReq>& InReqMessage, const OnGetRoleCollectionDataResult& InCallback);    

    /**
     * 古宝操作
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RoleCollectionOp")
    void K2_RoleCollectionOp(const FZRoleCollectionOpReq& InParams, const FZOnRoleCollectionOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RoleCollectionOpAck>&)> OnRoleCollectionOpResult;
    void RoleCollectionOp(const TSharedPtr<idlezt::RoleCollectionOpReq>& InReqMessage, const OnRoleCollectionOpResult& InCallback);    

    /**
     * 分享自己的古宝
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ShareSelfRoleCollection")
    void K2_ShareSelfRoleCollection(const FZShareSelfRoleCollectionReq& InParams, const FZOnShareSelfRoleCollectionResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ShareSelfRoleCollectionRsp>&)> OnShareSelfRoleCollectionResult;
    void ShareSelfRoleCollection(const TSharedPtr<idlezt::ShareSelfRoleCollectionReq>& InReqMessage, const OnShareSelfRoleCollectionResult& InCallback);    

    /**
     * 获取分享古宝数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetShareRoleCollectionData")
    void K2_GetShareRoleCollectionData(const FZGetShareRoleCollectionDataReq& InParams, const FZOnGetShareRoleCollectionDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetShareRoleCollectionDataRsp>&)> OnGetShareRoleCollectionDataResult;
    void GetShareRoleCollectionData(const TSharedPtr<idlezt::GetShareRoleCollectionDataReq>& InReqMessage, const OnGetShareRoleCollectionDataResult& InCallback);    

    /**
     * 获取玩家福缘数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetChecklistData")
    void K2_GetChecklistData(const FZGetChecklistDataReq& InParams, const FZOnGetChecklistDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetChecklistDataAck>&)> OnGetChecklistDataResult;
    void GetChecklistData(const TSharedPtr<idlezt::GetChecklistDataReq>& InReqMessage, const OnGetChecklistDataResult& InCallback);    

    /**
     * 福缘功能操作
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ChecklistOp")
    void K2_ChecklistOp(const FZChecklistOpReq& InParams, const FZOnChecklistOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ChecklistOpAck>&)> OnChecklistOpResult;
    void ChecklistOp(const TSharedPtr<idlezt::ChecklistOpReq>& InReqMessage, const OnChecklistOpResult& InCallback);    

    /**
     * 福缘任务进度更新
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UpdateChecklist")
    void K2_UpdateChecklist(const FZUpdateChecklistReq& InParams, const FZOnUpdateChecklistResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UpdateChecklistAck>&)> OnUpdateChecklistResult;
    void UpdateChecklist(const TSharedPtr<idlezt::UpdateChecklistReq>& InReqMessage, const OnUpdateChecklistResult& InCallback);    

    /**
     * 请求论剑台状态
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSwordPkInfo")
    void K2_GetSwordPkInfo(const FZGetSwordPkInfoReq& InParams, const FZOnGetSwordPkInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSwordPkInfoRsp>&)> OnGetSwordPkInfoResult;
    void GetSwordPkInfo(const TSharedPtr<idlezt::GetSwordPkInfoReq>& InReqMessage, const OnGetSwordPkInfoResult& InCallback);    

    /**
     * 注册论剑台
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SwordPkSignup")
    void K2_SwordPkSignup(const FZSwordPkSignupReq& InParams, const FZOnSwordPkSignupResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SwordPkSignupRsp>&)> OnSwordPkSignupResult;
    void SwordPkSignup(const TSharedPtr<idlezt::SwordPkSignupReq>& InReqMessage, const OnSwordPkSignupResult& InCallback);    

    /**
     * 论剑台匹配
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SwordPkMatching")
    void K2_SwordPkMatching(const FZSwordPkMatchingReq& InParams, const FZOnSwordPkMatchingResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SwordPkMatchingRsp>&)> OnSwordPkMatchingResult;
    void SwordPkMatching(const TSharedPtr<idlezt::SwordPkMatchingReq>& InReqMessage, const OnSwordPkMatchingResult& InCallback);    

    /**
     * 论剑台挑战
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SwordPkChallenge")
    void K2_SwordPkChallenge(const FZSwordPkChallengeReq& InParams, const FZOnSwordPkChallengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SwordPkChallengeRsp>&)> OnSwordPkChallengeResult;
    void SwordPkChallenge(const TSharedPtr<idlezt::SwordPkChallengeReq>& InReqMessage, const OnSwordPkChallengeResult& InCallback);    

    /**
     * 论剑台复仇
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SwordPkRevenge")
    void K2_SwordPkRevenge(const FZSwordPkRevengeReq& InParams, const FZOnSwordPkRevengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SwordPkRevengeRsp>&)> OnSwordPkRevengeResult;
    void SwordPkRevenge(const TSharedPtr<idlezt::SwordPkRevengeReq>& InReqMessage, const OnSwordPkRevengeResult& InCallback);    

    /**
     * 获取论剑台排行榜
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSwordPkTopList")
    void K2_GetSwordPkTopList(const FZGetSwordPkTopListReq& InParams, const FZOnGetSwordPkTopListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSwordPkTopListRsp>&)> OnGetSwordPkTopListResult;
    void GetSwordPkTopList(const TSharedPtr<idlezt::GetSwordPkTopListReq>& InReqMessage, const OnGetSwordPkTopListResult& InCallback);    

    /**
     * 兑换英雄令
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SwordPkExchangeHeroCard")
    void K2_SwordPkExchangeHeroCard(const FZSwordPkExchangeHeroCardReq& InParams, const FZOnSwordPkExchangeHeroCardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SwordPkExchangeHeroCardRsp>&)> OnSwordPkExchangeHeroCardResult;
    void SwordPkExchangeHeroCard(const TSharedPtr<idlezt::SwordPkExchangeHeroCardReq>& InReqMessage, const OnSwordPkExchangeHeroCardResult& InCallback);    

    /**
     * 获取玩家通用道具兑换数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetCommonItemExchangeData")
    void K2_GetCommonItemExchangeData(const FZGetCommonItemExchangeDataReq& InParams, const FZOnGetCommonItemExchangeDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetCommonItemExchangeDataAck>&)> OnGetCommonItemExchangeDataResult;
    void GetCommonItemExchangeData(const TSharedPtr<idlezt::GetCommonItemExchangeDataReq>& InReqMessage, const OnGetCommonItemExchangeDataResult& InCallback);    

    /**
     * 请求兑换通用道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ExchangeCommonItem")
    void K2_ExchangeCommonItem(const FZExchangeCommonItemReq& InParams, const FZOnExchangeCommonItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ExchangeCommonItemAck>&)> OnExchangeCommonItemResult;
    void ExchangeCommonItem(const TSharedPtr<idlezt::ExchangeCommonItemReq>& InReqMessage, const OnExchangeCommonItemResult& InCallback);    

    /**
     * 请求合成通用道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SynthesisCommonItem")
    void K2_SynthesisCommonItem(const FZSynthesisCommonItemReq& InParams, const FZOnSynthesisCommonItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SynthesisCommonItemAck>&)> OnSynthesisCommonItemResult;
    void SynthesisCommonItem(const TSharedPtr<idlezt::SynthesisCommonItemReq>& InReqMessage, const OnSynthesisCommonItemResult& InCallback);    

    /**
     * 请求可加入宗门列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetCandidatesSeptList")
    void K2_GetCandidatesSeptList(const FZGetCandidatesSeptListReq& InParams, const FZOnGetCandidatesSeptListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetCandidatesSeptListAck>&)> OnGetCandidatesSeptListResult;
    void GetCandidatesSeptList(const TSharedPtr<idlezt::GetCandidatesSeptListReq>& InReqMessage, const OnGetCandidatesSeptListResult& InCallback);    

    /**
     * 搜索宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SearchSept")
    void K2_SearchSept(const FZSearchSeptReq& InParams, const FZOnSearchSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SearchSeptAck>&)> OnSearchSeptResult;
    void SearchSept(const TSharedPtr<idlezt::SearchSeptReq>& InReqMessage, const OnSearchSeptResult& InCallback);    

    /**
     * 获取指定宗门基本信息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptBaseInfo")
    void K2_GetSeptBaseInfo(const FZGetSeptBaseInfoReq& InParams, const FZOnGetSeptBaseInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptBaseInfoAck>&)> OnGetSeptBaseInfoResult;
    void GetSeptBaseInfo(const TSharedPtr<idlezt::GetSeptBaseInfoReq>& InReqMessage, const OnGetSeptBaseInfoResult& InCallback);    

    /**
     * 获取宗门成员列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptMemberList")
    void K2_GetSeptMemberList(const FZGetSeptMemberListReq& InParams, const FZOnGetSeptMemberListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptMemberListAck>&)> OnGetSeptMemberListResult;
    void GetSeptMemberList(const TSharedPtr<idlezt::GetSeptMemberListReq>& InReqMessage, const OnGetSeptMemberListResult& InCallback);    

    /**
     * 创建宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="CreateSept")
    void K2_CreateSept(const FZCreateSeptReq& InParams, const FZOnCreateSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::CreateSeptAck>&)> OnCreateSeptResult;
    void CreateSept(const TSharedPtr<idlezt::CreateSeptReq>& InReqMessage, const OnCreateSeptResult& InCallback);    

    /**
     * 解散宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DismissSept")
    void K2_DismissSept(const FZDismissSeptReq& InParams, const FZOnDismissSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DismissSeptAck>&)> OnDismissSeptResult;
    void DismissSept(const TSharedPtr<idlezt::DismissSeptReq>& InReqMessage, const OnDismissSeptResult& InCallback);    

    /**
     * 离开宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ExitSept")
    void K2_ExitSept(const FZExitSeptReq& InParams, const FZOnExitSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ExitSeptAck>&)> OnExitSeptResult;
    void ExitSept(const TSharedPtr<idlezt::ExitSeptReq>& InReqMessage, const OnExitSeptResult& InCallback);    

    /**
     * 申请加入宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ApplyJoinSept")
    void K2_ApplyJoinSept(const FZApplyJoinSeptReq& InParams, const FZOnApplyJoinSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ApplyJoinSeptAck>&)> OnApplyJoinSeptResult;
    void ApplyJoinSept(const TSharedPtr<idlezt::ApplyJoinSeptReq>& InReqMessage, const OnApplyJoinSeptResult& InCallback);    

    /**
     * 审批入宗请求
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ApproveApplySept")
    void K2_ApproveApplySept(const FZApproveApplySeptReq& InParams, const FZOnApproveApplySeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ApproveApplySeptAck>&)> OnApproveApplySeptResult;
    void ApproveApplySept(const TSharedPtr<idlezt::ApproveApplySeptReq>& InReqMessage, const OnApproveApplySeptResult& InCallback);    

    /**
     * 获取入宗申请列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetApplyJoinSeptList")
    void K2_GetApplyJoinSeptList(const FZGetApplyJoinSeptListReq& InParams, const FZOnGetApplyJoinSeptListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetApplyJoinSeptListAck>&)> OnGetApplyJoinSeptListResult;
    void GetApplyJoinSeptList(const TSharedPtr<idlezt::GetApplyJoinSeptListReq>& InReqMessage, const OnGetApplyJoinSeptListResult& InCallback);    

    /**
     * 回复入宗邀请
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RespondInviteSept")
    void K2_RespondInviteSept(const FZRespondInviteSeptReq& InParams, const FZOnRespondInviteSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RespondInviteSeptAck>&)> OnRespondInviteSeptResult;
    void RespondInviteSept(const TSharedPtr<idlezt::RespondInviteSeptReq>& InReqMessage, const OnRespondInviteSeptResult& InCallback);    

    /**
     * 获取邀请我入宗的宗门列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetInviteMeJoinSeptList")
    void K2_GetInviteMeJoinSeptList(const FZGetInviteMeJoinSeptListReq& InParams, const FZOnGetInviteMeJoinSeptListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetInviteMeJoinSeptListAck>&)> OnGetInviteMeJoinSeptListResult;
    void GetInviteMeJoinSeptList(const TSharedPtr<idlezt::GetInviteMeJoinSeptListReq>& InReqMessage, const OnGetInviteMeJoinSeptListResult& InCallback);    

    /**
     * 获取可邀请入宗玩家列表
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetCandidatesInviteRoleList")
    void K2_GetCandidatesInviteRoleList(const FZGetCandidatesInviteRoleListReq& InParams, const FZOnGetCandidatesInviteRoleListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetCandidatesInviteRoleListAck>&)> OnGetCandidatesInviteRoleListResult;
    void GetCandidatesInviteRoleList(const TSharedPtr<idlezt::GetCandidatesInviteRoleListReq>& InReqMessage, const OnGetCandidatesInviteRoleListResult& InCallback);    

    /**
     * 邀请加入宗门
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="InviteJoinSept")
    void K2_InviteJoinSept(const FZInviteJoinSeptReq& InParams, const FZOnInviteJoinSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::InviteJoinSeptAck>&)> OnInviteJoinSeptResult;
    void InviteJoinSept(const TSharedPtr<idlezt::InviteJoinSeptReq>& InReqMessage, const OnInviteJoinSeptResult& InCallback);    

    /**
     * 设置宗门设置
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetSeptSettings")
    void K2_SetSeptSettings(const FZSetSeptSettingsReq& InParams, const FZOnSetSeptSettingsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetSeptSettingsAck>&)> OnSetSeptSettingsResult;
    void SetSeptSettings(const TSharedPtr<idlezt::SetSeptSettingsReq>& InReqMessage, const OnSetSeptSettingsResult& InCallback);    

    /**
     * 设置宗门公告
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="SetSeptAnnounce")
    void K2_SetSeptAnnounce(const FZSetSeptAnnounceReq& InParams, const FZOnSetSeptAnnounceResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::SetSeptAnnounceAck>&)> OnSetSeptAnnounceResult;
    void SetSeptAnnounce(const TSharedPtr<idlezt::SetSeptAnnounceReq>& InReqMessage, const OnSetSeptAnnounceResult& InCallback);    

    /**
     * 宗门改名
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ChangeSeptName")
    void K2_ChangeSeptName(const FZChangeSeptNameReq& InParams, const FZOnChangeSeptNameResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ChangeSeptNameAck>&)> OnChangeSeptNameResult;
    void ChangeSeptName(const TSharedPtr<idlezt::ChangeSeptNameReq>& InReqMessage, const OnChangeSeptNameResult& InCallback);    

    /**
     * 请求宗门日志
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptLog")
    void K2_GetSeptLog(const FZGetSeptLogReq& InParams, const FZOnGetSeptLogResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptLogAck>&)> OnGetSeptLogResult;
    void GetSeptLog(const TSharedPtr<idlezt::GetSeptLogReq>& InReqMessage, const OnGetSeptLogResult& InCallback);    

    /**
     * 宗门建设
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ConstructSept")
    void K2_ConstructSept(const FZConstructSeptReq& InParams, const FZOnConstructSeptResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ConstructSeptAck>&)> OnConstructSeptResult;
    void ConstructSept(const TSharedPtr<idlezt::ConstructSeptReq>& InReqMessage, const OnConstructSeptResult& InCallback);    

    /**
     * 获取宗门建设记录
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetConstructSeptLog")
    void K2_GetConstructSeptLog(const FZGetConstructSeptLogReq& InParams, const FZOnGetConstructSeptLogResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetConstructSeptLogAck>&)> OnGetConstructSeptLogResult;
    void GetConstructSeptLog(const TSharedPtr<idlezt::GetConstructSeptLogReq>& InReqMessage, const OnGetConstructSeptLogResult& InCallback);    

    /**
     * 获取角色每日已邀请入宗次数
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptInvitedRoleDailyNum")
    void K2_GetSeptInvitedRoleDailyNum(const FZGetSeptInvitedRoleDailyNumReq& InParams, const FZOnGetSeptInvitedRoleDailyNumResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumAck>&)> OnGetSeptInvitedRoleDailyNumResult;
    void GetSeptInvitedRoleDailyNum(const TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumReq>& InReqMessage, const OnGetSeptInvitedRoleDailyNumResult& InCallback);    

    /**
     * 任命职位
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppointSeptPosition")
    void K2_AppointSeptPosition(const FZAppointSeptPositionReq& InParams, const FZOnAppointSeptPositionResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppointSeptPositionAck>&)> OnAppointSeptPositionResult;
    void AppointSeptPosition(const TSharedPtr<idlezt::AppointSeptPositionReq>& InReqMessage, const OnAppointSeptPositionResult& InCallback);    

    /**
     * 转让宗主
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ResignSeptChairman")
    void K2_ResignSeptChairman(const FZResignSeptChairmanReq& InParams, const FZOnResignSeptChairmanResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ResignSeptChairmanAck>&)> OnResignSeptChairmanResult;
    void ResignSeptChairman(const TSharedPtr<idlezt::ResignSeptChairmanReq>& InReqMessage, const OnResignSeptChairmanResult& InCallback);    

    /**
     * 开除宗门成员
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="KickOutSeptMember")
    void K2_KickOutSeptMember(const FZKickOutSeptMemberReq& InParams, const FZOnKickOutSeptMemberResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::KickOutSeptMemberAck>&)> OnKickOutSeptMemberResult;
    void KickOutSeptMember(const TSharedPtr<idlezt::KickOutSeptMemberReq>& InReqMessage, const OnKickOutSeptMemberResult& InCallback);    

    /**
     * 请求玩家宗门商店数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleSeptShopData")
    void K2_GetRoleSeptShopData(const FZGetRoleSeptShopDataReq& InParams, const FZOnGetRoleSeptShopDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleSeptShopDataAck>&)> OnGetRoleSeptShopDataResult;
    void GetRoleSeptShopData(const TSharedPtr<idlezt::GetRoleSeptShopDataReq>& InReqMessage, const OnGetRoleSeptShopDataResult& InCallback);    

    /**
     * 请求兑换宗门商店道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="BuySeptShopItem")
    void K2_BuySeptShopItem(const FZBuySeptShopItemReq& InParams, const FZOnBuySeptShopItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::BuySeptShopItemAck>&)> OnBuySeptShopItemResult;
    void BuySeptShopItem(const TSharedPtr<idlezt::BuySeptShopItemReq>& InReqMessage, const OnBuySeptShopItemResult& InCallback);    

    /**
     * 请求玩家宗门事务数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleSeptQuestData")
    void K2_GetRoleSeptQuestData(const FZGetRoleSeptQuestDataReq& InParams, const FZOnGetRoleSeptQuestDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleSeptQuestDataAck>&)> OnGetRoleSeptQuestDataResult;
    void GetRoleSeptQuestData(const TSharedPtr<idlezt::GetRoleSeptQuestDataReq>& InReqMessage, const OnGetRoleSeptQuestDataResult& InCallback);    

    /**
     * 玩家宗门事务操作
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReqRoleSeptQuestOp")
    void K2_ReqRoleSeptQuestOp(const FZReqRoleSeptQuestOpReq& InParams, const FZOnReqRoleSeptQuestOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReqRoleSeptQuestOpAck>&)> OnReqRoleSeptQuestOpResult;
    void ReqRoleSeptQuestOp(const TSharedPtr<idlezt::ReqRoleSeptQuestOpReq>& InReqMessage, const OnReqRoleSeptQuestOpResult& InCallback);    

    /**
     * 玩家宗门事务手动刷新
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RefreshSeptQuest")
    void K2_RefreshSeptQuest(const FZRefreshSeptQuestReq& InParams, const FZOnRefreshSeptQuestResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RefreshSeptQuestAck>&)> OnRefreshSeptQuestResult;
    void RefreshSeptQuest(const TSharedPtr<idlezt::RefreshSeptQuestReq>& InReqMessage, const OnRefreshSeptQuestResult& InCallback);    

    /**
     * 玩家宗门事务升级
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReqSeptQuestRankUp")
    void K2_ReqSeptQuestRankUp(const FZReqSeptQuestRankUpReq& InParams, const FZOnReqSeptQuestRankUpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReqSeptQuestRankUpAck>&)> OnReqSeptQuestRankUpResult;
    void ReqSeptQuestRankUp(const TSharedPtr<idlezt::ReqSeptQuestRankUpReq>& InReqMessage, const OnReqSeptQuestRankUpResult& InCallback);    

    /**
     * 开始占据中立秘镜矿脉
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="BeginOccupySeptStone")
    void K2_BeginOccupySeptStone(const FZBeginOccupySeptStoneReq& InParams, const FZOnBeginOccupySeptStoneResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::BeginOccupySeptStoneAck>&)> OnBeginOccupySeptStoneResult;
    void BeginOccupySeptStone(const TSharedPtr<idlezt::BeginOccupySeptStoneReq>& InReqMessage, const OnBeginOccupySeptStoneResult& InCallback);    

    /**
     * 结束占领中立秘镜矿脉
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="EndOccupySeptStone")
    void K2_EndOccupySeptStone(const FZEndOccupySeptStoneReq& InParams, const FZOnEndOccupySeptStoneResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::EndOccupySeptStoneAck>&)> OnEndOccupySeptStoneResult;
    void EndOccupySeptStone(const TSharedPtr<idlezt::EndOccupySeptStoneReq>& InReqMessage, const OnEndOccupySeptStoneResult& InCallback);    

    /**
     * 占领宗门领地
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OccupySeptLand")
    void K2_OccupySeptLand(const FZOccupySeptLandReq& InParams, const FZOnOccupySeptLandResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OccupySeptLandAck>&)> OnOccupySeptLandResult;
    void OccupySeptLand(const TSharedPtr<idlezt::OccupySeptLandReq>& InReqMessage, const OnOccupySeptLandResult& InCallback);    

    /**
     * 获取功法数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetGongFaData")
    void K2_GetGongFaData(const FZGetGongFaDataReq& InParams, const FZOnGetGongFaDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetGongFaDataAck>&)> OnGetGongFaDataResult;
    void GetGongFaData(const TSharedPtr<idlezt::GetGongFaDataReq>& InReqMessage, const OnGetGongFaDataResult& InCallback);    

    /**
     * 功法操作：领悟 | 激活 | 升级
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GongFaOp")
    void K2_GongFaOp(const FZGongFaOpReq& InParams, const FZOnGongFaOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GongFaOpAck>&)> OnGongFaOpResult;
    void GongFaOp(const TSharedPtr<idlezt::GongFaOpReq>& InReqMessage, const OnGongFaOpResult& InCallback);    

    /**
     * 激活功法圆满效果
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ActivateGongFaMaxEffect")
    void K2_ActivateGongFaMaxEffect(const FZActivateGongFaMaxEffectReq& InParams, const FZOnActivateGongFaMaxEffectResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ActivateGongFaMaxEffectAck>&)> OnActivateGongFaMaxEffectResult;
    void ActivateGongFaMaxEffect(const TSharedPtr<idlezt::ActivateGongFaMaxEffectReq>& InReqMessage, const OnActivateGongFaMaxEffectResult& InCallback);    

    /**
     * 获取宗门领地伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptLandDamageTopList")
    void K2_GetSeptLandDamageTopList(const FZGetSeptLandDamageTopListReq& InParams, const FZOnGetSeptLandDamageTopListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptLandDamageTopListAck>&)> OnGetSeptLandDamageTopListResult;
    void GetSeptLandDamageTopList(const TSharedPtr<idlezt::GetSeptLandDamageTopListReq>& InReqMessage, const OnGetSeptLandDamageTopListResult& InCallback);    

    /**
     * 领取福赠奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReceiveFuZengRewards")
    void K2_ReceiveFuZengRewards(const FZReceiveFuZengRewardsReq& InParams, const FZOnReceiveFuZengRewardsResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReceiveFuZengRewardsAck>&)> OnReceiveFuZengRewardsResult;
    void ReceiveFuZengRewards(const TSharedPtr<idlezt::ReceiveFuZengRewardsReq>& InReqMessage, const OnReceiveFuZengRewardsResult& InCallback);    

    /**
     * 获取福赠数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleFuZengData")
    void K2_GetRoleFuZengData(const FZGetRoleFuZengDataReq& InParams, const FZOnGetRoleFuZengDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleFuZengDataAck>&)> OnGetRoleFuZengDataResult;
    void GetRoleFuZengData(const TSharedPtr<idlezt::GetRoleFuZengDataReq>& InReqMessage, const OnGetRoleFuZengDataResult& InCallback);    

    /**
     * 获取宝藏阁数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleTreasuryData")
    void K2_GetRoleTreasuryData(const FZGetRoleTreasuryDataReq& InParams, const FZOnGetRoleTreasuryDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleTreasuryDataAck>&)> OnGetRoleTreasuryDataResult;
    void GetRoleTreasuryData(const TSharedPtr<idlezt::GetRoleTreasuryDataReq>& InReqMessage, const OnGetRoleTreasuryDataResult& InCallback);    

    /**
     * 请求开箱
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OpenTreasuryChest")
    void K2_OpenTreasuryChest(const FZOpenTreasuryChestReq& InParams, const FZOnOpenTreasuryChestResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OpenTreasuryChestAck>&)> OnOpenTreasuryChestResult;
    void OpenTreasuryChest(const TSharedPtr<idlezt::OpenTreasuryChestReq>& InReqMessage, const OnOpenTreasuryChestResult& InCallback);    

    /**
     * 请求一键全开箱
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OneClickOpenTreasuryChest")
    void K2_OneClickOpenTreasuryChest(const FZOneClickOpenTreasuryChestReq& InParams, const FZOnOneClickOpenTreasuryChestResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OneClickOpenTreasuryChestAck>&)> OnOneClickOpenTreasuryChestResult;
    void OneClickOpenTreasuryChest(const TSharedPtr<idlezt::OneClickOpenTreasuryChestReq>& InReqMessage, const OnOneClickOpenTreasuryChestResult& InCallback);    

    /**
     * 请求探索卡池
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="OpenTreasuryGacha")
    void K2_OpenTreasuryGacha(const FZOpenTreasuryGachaReq& InParams, const FZOnOpenTreasuryGachaResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::OpenTreasuryGachaAck>&)> OnOpenTreasuryGachaResult;
    void OpenTreasuryGacha(const TSharedPtr<idlezt::OpenTreasuryGachaReq>& InReqMessage, const OnOpenTreasuryGachaResult& InCallback);    

    /**
     * 请求刷新古修商店
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RefreshTreasuryShop")
    void K2_RefreshTreasuryShop(const FZRefreshTreasuryShopReq& InParams, const FZOnRefreshTreasuryShopResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RefreshTreasuryShopAck>&)> OnRefreshTreasuryShopResult;
    void RefreshTreasuryShop(const TSharedPtr<idlezt::RefreshTreasuryShopReq>& InReqMessage, const OnRefreshTreasuryShopResult& InCallback);    

    /**
     * 请求古修商店中购买
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="TreasuryShopBuy")
    void K2_TreasuryShopBuy(const FZTreasuryShopBuyReq& InParams, const FZOnTreasuryShopBuyResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::TreasuryShopBuyAck>&)> OnTreasuryShopBuyResult;
    void TreasuryShopBuy(const TSharedPtr<idlezt::TreasuryShopBuyReq>& InReqMessage, const OnTreasuryShopBuyResult& InCallback);    

    /**
     * 获取生涯计数器数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetLifeCounterData")
    void K2_GetLifeCounterData(const FZGetLifeCounterDataReq& InParams, const FZOnGetLifeCounterDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetLifeCounterDataAck>&)> OnGetLifeCounterDataResult;
    void GetLifeCounterData(const TSharedPtr<idlezt::GetLifeCounterDataReq>& InReqMessage, const OnGetLifeCounterDataResult& InCallback);    

    /**
     * 进行任务对战
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DoQuestFight")
    void K2_DoQuestFight(const FZDoQuestFightReq& InParams, const FZOnDoQuestFightResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DoQuestFightAck>&)> OnDoQuestFightResult;
    void DoQuestFight(const TSharedPtr<idlezt::DoQuestFightReq>& InReqMessage, const OnDoQuestFightResult& InCallback);    

    /**
     * 结束任务对战
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="QuestFightQuickEnd")
    void K2_QuestFightQuickEnd(const FZQuestFightQuickEndReq& InParams, const FZOnQuestFightQuickEndResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::QuestFightQuickEndAck>&)> OnQuestFightQuickEndResult;
    void QuestFightQuickEnd(const TSharedPtr<idlezt::QuestFightQuickEndReq>& InReqMessage, const OnQuestFightQuickEndResult& InCallback);    

    /**
     * 请求外观数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetAppearanceData")
    void K2_GetAppearanceData(const FZGetAppearanceDataReq& InParams, const FZOnGetAppearanceDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetAppearanceDataAck>&)> OnGetAppearanceDataResult;
    void GetAppearanceData(const TSharedPtr<idlezt::GetAppearanceDataReq>& InReqMessage, const OnGetAppearanceDataResult& InCallback);    

    /**
     * 请求添加外观（使用包含外观的礼包道具）
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppearanceAdd")
    void K2_AppearanceAdd(const FZAppearanceAddReq& InParams, const FZOnAppearanceAddResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppearanceAddAck>&)> OnAppearanceAddResult;
    void AppearanceAdd(const TSharedPtr<idlezt::AppearanceAddReq>& InReqMessage, const OnAppearanceAddResult& InCallback);    

    /**
     * 请求激活外观
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppearanceActive")
    void K2_AppearanceActive(const FZAppearanceActiveReq& InParams, const FZOnAppearanceActiveResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppearanceActiveAck>&)> OnAppearanceActiveResult;
    void AppearanceActive(const TSharedPtr<idlezt::AppearanceActiveReq>& InReqMessage, const OnAppearanceActiveResult& InCallback);    

    /**
     * 请求穿戴外观
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppearanceWear")
    void K2_AppearanceWear(const FZAppearanceWearReq& InParams, const FZOnAppearanceWearResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppearanceWearAck>&)> OnAppearanceWearResult;
    void AppearanceWear(const TSharedPtr<idlezt::AppearanceWearReq>& InReqMessage, const OnAppearanceWearResult& InCallback);    

    /**
     * 请求外观商店购买
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppearanceBuy")
    void K2_AppearanceBuy(const FZAppearanceBuyReq& InParams, const FZOnAppearanceBuyResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppearanceBuyAck>&)> OnAppearanceBuyResult;
    void AppearanceBuy(const TSharedPtr<idlezt::AppearanceBuyReq>& InReqMessage, const OnAppearanceBuyResult& InCallback);    

    /**
     * 请求修改外形
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AppearanceChangeSkType")
    void K2_AppearanceChangeSkType(const FZAppearanceChangeSkTypeReq& InParams, const FZOnAppearanceChangeSkTypeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AppearanceChangeSkTypeAck>&)> OnAppearanceChangeSkTypeResult;
    void AppearanceChangeSkType(const TSharedPtr<idlezt::AppearanceChangeSkTypeReq>& InReqMessage, const OnAppearanceChangeSkTypeResult& InCallback);    

    /**
     * 请求指定战斗信息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetBattleHistoryInfo")
    void K2_GetBattleHistoryInfo(const FZGetBattleHistoryInfoReq& InParams, const FZOnGetBattleHistoryInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetBattleHistoryInfoAck>&)> OnGetBattleHistoryInfoResult;
    void GetBattleHistoryInfo(const TSharedPtr<idlezt::GetBattleHistoryInfoReq>& InReqMessage, const OnGetBattleHistoryInfoResult& InCallback);    

    /**
     * 请求秘境探索数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetArenaCheckListData")
    void K2_GetArenaCheckListData(const FZGetArenaCheckListDataReq& InParams, const FZOnGetArenaCheckListDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetArenaCheckListDataAck>&)> OnGetArenaCheckListDataResult;
    void GetArenaCheckListData(const TSharedPtr<idlezt::GetArenaCheckListDataReq>& InReqMessage, const OnGetArenaCheckListDataResult& InCallback);    

    /**
     * 请求提交秘境探索事件
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ArenaCheckListSubmit")
    void K2_ArenaCheckListSubmit(const FZArenaCheckListSubmitReq& InParams, const FZOnArenaCheckListSubmitResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ArenaCheckListSubmitAck>&)> OnArenaCheckListSubmitResult;
    void ArenaCheckListSubmit(const TSharedPtr<idlezt::ArenaCheckListSubmitReq>& InReqMessage, const OnArenaCheckListSubmitResult& InCallback);    

    /**
     * 请求提交秘境探索奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ArenaCheckListRewardSubmit")
    void K2_ArenaCheckListRewardSubmit(const FZArenaCheckListRewardSubmitReq& InParams, const FZOnArenaCheckListRewardSubmitResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ArenaCheckListRewardSubmitAck>&)> OnArenaCheckListRewardSubmitResult;
    void ArenaCheckListRewardSubmit(const TSharedPtr<idlezt::ArenaCheckListRewardSubmitReq>& InReqMessage, const OnArenaCheckListRewardSubmitResult& InCallback);    

    /**
     * 请求开启剿灭副本
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonKillAllChallenge")
    void K2_DungeonKillAllChallenge(const FZDungeonKillAllChallengeReq& InParams, const FZOnDungeonKillAllChallengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonKillAllChallengeAck>&)> OnDungeonKillAllChallengeResult;
    void DungeonKillAllChallenge(const TSharedPtr<idlezt::DungeonKillAllChallengeReq>& InReqMessage, const OnDungeonKillAllChallengeResult& InCallback);    

    /**
     * 请求剿灭副本快速结束
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonKillAllQuickEnd")
    void K2_DungeonKillAllQuickEnd(const FZDungeonKillAllQuickEndReq& InParams, const FZOnDungeonKillAllQuickEndResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonKillAllQuickEndAck>&)> OnDungeonKillAllQuickEndResult;
    void DungeonKillAllQuickEnd(const TSharedPtr<idlezt::DungeonKillAllQuickEndReq>& InReqMessage, const OnDungeonKillAllQuickEndResult& InCallback);    

    /**
     * 询问剿灭副本是否完成
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonKillAllData")
    void K2_DungeonKillAllData(const FZDungeonKillAllDataReq& InParams, const FZOnDungeonKillAllDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonKillAllDataAck>&)> OnDungeonKillAllDataResult;
    void DungeonKillAllData(const TSharedPtr<idlezt::DungeonKillAllDataReq>& InReqMessage, const OnDungeonKillAllDataResult& InCallback);    

    /**
     * 药园数据请求
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetFarmlandData")
    void K2_GetFarmlandData(const FZGetFarmlandDataReq& InParams, const FZOnGetFarmlandDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetFarmlandDataAck>&)> OnGetFarmlandDataResult;
    void GetFarmlandData(const TSharedPtr<idlezt::GetFarmlandDataReq>& InReqMessage, const OnGetFarmlandDataResult& InCallback);    

    /**
     * 药园地块解锁
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandUnlockBlock")
    void K2_FarmlandUnlockBlock(const FZFarmlandUnlockBlockReq& InParams, const FZOnFarmlandUnlockBlockResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandUnlockBlockAck>&)> OnFarmlandUnlockBlockResult;
    void FarmlandUnlockBlock(const TSharedPtr<idlezt::FarmlandUnlockBlockReq>& InReqMessage, const OnFarmlandUnlockBlockResult& InCallback);    

    /**
     * 药园种植或铲除
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandPlantSeed")
    void K2_FarmlandPlantSeed(const FZFarmlandPlantSeedReq& InParams, const FZOnFarmlandPlantSeedResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandPlantSeedAck>&)> OnFarmlandPlantSeedResult;
    void FarmlandPlantSeed(const TSharedPtr<idlezt::FarmlandPlantSeedReq>& InReqMessage, const OnFarmlandPlantSeedResult& InCallback);    

    /**
     * 药园浇灌
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandWatering")
    void K2_FarmlandWatering(const FZFarmlandWateringReq& InParams, const FZOnFarmlandWateringResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandWateringAck>&)> OnFarmlandWateringResult;
    void FarmlandWatering(const TSharedPtr<idlezt::FarmlandWateringReq>& InReqMessage, const OnFarmlandWateringResult& InCallback);    

    /**
     * 药园催熟
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandRipening")
    void K2_FarmlandRipening(const FZFarmlandRipeningReq& InParams, const FZOnFarmlandRipeningResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandRipeningAck>&)> OnFarmlandRipeningResult;
    void FarmlandRipening(const TSharedPtr<idlezt::FarmlandRipeningReq>& InReqMessage, const OnFarmlandRipeningResult& InCallback);    

    /**
     * 药园收获
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandHarvest")
    void K2_FarmlandHarvest(const FZFarmlandHarvestReq& InParams, const FZOnFarmlandHarvestResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandHarvestAck>&)> OnFarmlandHarvestResult;
    void FarmlandHarvest(const TSharedPtr<idlezt::FarmlandHarvestReq>& InReqMessage, const OnFarmlandHarvestResult& InCallback);    

    /**
     * 药园药童升级
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmerRankUp")
    void K2_FarmerRankUp(const FZFarmerRankUpReq& InParams, const FZOnFarmerRankUpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmerRankUpAck>&)> OnFarmerRankUpResult;
    void FarmerRankUp(const TSharedPtr<idlezt::FarmerRankUpReq>& InReqMessage, const OnFarmerRankUpResult& InCallback);    

    /**
     * 药园打理
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FarmlandSetManagement")
    void K2_FarmlandSetManagement(const FZFarmlandSetManagementReq& InParams, const FZOnFarmlandSetManagementResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FarmlandSetManagementAck>&)> OnFarmlandSetManagementResult;
    void FarmlandSetManagement(const TSharedPtr<idlezt::FarmlandSetManagementReq>& InReqMessage, const OnFarmlandSetManagementResult& InCallback);    

    /**
     * 获取药园状态，自动收获
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="UpdateFarmlandState")
    void K2_UpdateFarmlandState(const FZUpdateFarmlandStateReq& InParams, const FZOnUpdateFarmlandStateResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::UpdateFarmlandStateAck>&)> OnUpdateFarmlandStateResult;
    void UpdateFarmlandState(const TSharedPtr<idlezt::UpdateFarmlandStateReq>& InReqMessage, const OnUpdateFarmlandStateResult& InCallback);    

    /**
     * 请求开启生存副本
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonSurviveChallenge")
    void K2_DungeonSurviveChallenge(const FZDungeonSurviveChallengeReq& InParams, const FZOnDungeonSurviveChallengeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonSurviveChallengeAck>&)> OnDungeonSurviveChallengeResult;
    void DungeonSurviveChallenge(const TSharedPtr<idlezt::DungeonSurviveChallengeReq>& InReqMessage, const OnDungeonSurviveChallengeResult& InCallback);    

    /**
     * 请求生存副本快速结束
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonSurviveQuickEnd")
    void K2_DungeonSurviveQuickEnd(const FZDungeonSurviveQuickEndReq& InParams, const FZOnDungeonSurviveQuickEndResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonSurviveQuickEndAck>&)> OnDungeonSurviveQuickEndResult;
    void DungeonSurviveQuickEnd(const TSharedPtr<idlezt::DungeonSurviveQuickEndReq>& InReqMessage, const OnDungeonSurviveQuickEndResult& InCallback);    

    /**
     * 询问生存副本是否完成
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DungeonSurviveData")
    void K2_DungeonSurviveData(const FZDungeonSurviveDataReq& InParams, const FZOnDungeonSurviveDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DungeonSurviveDataAck>&)> OnDungeonSurviveDataResult;
    void DungeonSurviveData(const TSharedPtr<idlezt::DungeonSurviveDataReq>& InReqMessage, const OnDungeonSurviveDataResult& InCallback);    

    /**
     * 神通一键重置CD请求
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRevertAllSkillCoolDown")
    void K2_GetRevertAllSkillCoolDown(const FZGetRevertAllSkillCoolDownReq& InParams, const FZOnGetRevertAllSkillCoolDownResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRevertAllSkillCoolDownAck>&)> OnGetRevertAllSkillCoolDownResult;
    void GetRevertAllSkillCoolDown(const TSharedPtr<idlezt::GetRevertAllSkillCoolDownReq>& InReqMessage, const OnGetRevertAllSkillCoolDownResult& InCallback);    

    /**
     * 获取道友功能数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleFriendData")
    void K2_GetRoleFriendData(const FZGetRoleFriendDataReq& InParams, const FZOnGetRoleFriendDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleFriendDataAck>&)> OnGetRoleFriendDataResult;
    void GetRoleFriendData(const TSharedPtr<idlezt::GetRoleFriendDataReq>& InReqMessage, const OnGetRoleFriendDataResult& InCallback);    

    /**
     * 发起 好友申请/或移除好友 拉黑/或移除拉黑 成为道侣或解除道侣
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FriendOp")
    void K2_FriendOp(const FZFriendOpReq& InParams, const FZOnFriendOpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FriendOpAck>&)> OnFriendOpResult;
    void FriendOp(const TSharedPtr<idlezt::FriendOpReq>& InReqMessage, const OnFriendOpResult& InCallback);    

    /**
     * 处理好友申请
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReplyFriendRequest")
    void K2_ReplyFriendRequest(const FZReplyFriendRequestReq& InParams, const FZOnReplyFriendRequestResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReplyFriendRequestAck>&)> OnReplyFriendRequestResult;
    void ReplyFriendRequest(const TSharedPtr<idlezt::ReplyFriendRequestReq>& InReqMessage, const OnReplyFriendRequestResult& InCallback);    

    /**
     * 查找玩家（道友功能）
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="FriendSearchRoleInfo")
    void K2_FriendSearchRoleInfo(const FZFriendSearchRoleInfoReq& InParams, const FZOnFriendSearchRoleInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::FriendSearchRoleInfoAck>&)> OnFriendSearchRoleInfoResult;
    void FriendSearchRoleInfo(const TSharedPtr<idlezt::FriendSearchRoleInfoReq>& InReqMessage, const OnFriendSearchRoleInfoResult& InCallback);    

    /**
     * 请求玩家信息缓存(Todo 用于聊天查找，可能需要整合)
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleInfoCache")
    void K2_GetRoleInfoCache(const FZGetRoleInfoCacheReq& InParams, const FZOnGetRoleInfoCacheResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleInfoCacheAck>&)> OnGetRoleInfoCacheResult;
    void GetRoleInfoCache(const TSharedPtr<idlezt::GetRoleInfoCacheReq>& InReqMessage, const OnGetRoleInfoCacheResult& InCallback);    

    /**
     * 请求玩家个人信息(Todo 老接口，可能需要整合)
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleInfo")
    void K2_GetRoleInfo(const FZGetRoleInfoReq& InParams, const FZOnGetRoleInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleInfoAck>&)> OnGetRoleInfoResult;
    void GetRoleInfo(const TSharedPtr<idlezt::GetRoleInfoReq>& InReqMessage, const OnGetRoleInfoResult& InCallback);    

    /**
     * 获取化身数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleAvatarData")
    void K2_GetRoleAvatarData(const FZGetRoleAvatarDataReq& InParams, const FZOnGetRoleAvatarDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleAvatarDataAck>&)> OnGetRoleAvatarDataResult;
    void GetRoleAvatarData(const TSharedPtr<idlezt::GetRoleAvatarDataReq>& InReqMessage, const OnGetRoleAvatarDataResult& InCallback);    

    /**
     * 派遣化身
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="DispatchAvatar")
    void K2_DispatchAvatar(const FZDispatchAvatarReq& InParams, const FZOnDispatchAvatarResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::DispatchAvatarAck>&)> OnDispatchAvatarResult;
    void DispatchAvatar(const TSharedPtr<idlezt::DispatchAvatarReq>& InReqMessage, const OnDispatchAvatarResult& InCallback);    

    /**
     * 化身升级
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AvatarRankUp")
    void K2_AvatarRankUp(const FZAvatarRankUpReq& InParams, const FZOnAvatarRankUpResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AvatarRankUpAck>&)> OnAvatarRankUpResult;
    void AvatarRankUp(const TSharedPtr<idlezt::AvatarRankUpReq>& InReqMessage, const OnAvatarRankUpResult& InCallback);    

    /**
     * 收获化身包裹道具
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReceiveAvatarTempPackage")
    void K2_ReceiveAvatarTempPackage(const FZReceiveAvatarTempPackageReq& InParams, const FZOnReceiveAvatarTempPackageResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReceiveAvatarTempPackageAck>&)> OnReceiveAvatarTempPackageResult;
    void ReceiveAvatarTempPackage(const TSharedPtr<idlezt::ReceiveAvatarTempPackageReq>& InReqMessage, const OnReceiveAvatarTempPackageResult& InCallback);    

    /**
     * 获取秘境探索统计数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetArenaExplorationStatisticalData")
    void K2_GetArenaExplorationStatisticalData(const FZGetArenaExplorationStatisticalDataReq& InParams, const FZOnGetArenaExplorationStatisticalDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetArenaExplorationStatisticalDataAck>&)> OnGetArenaExplorationStatisticalDataResult;
    void GetArenaExplorationStatisticalData(const TSharedPtr<idlezt::GetArenaExplorationStatisticalDataReq>& InReqMessage, const OnGetArenaExplorationStatisticalDataResult& InCallback);    

    /**
     * 获取角色传记数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleBiographyData")
    void K2_GetRoleBiographyData(const FZGetRoleBiographyDataReq& InParams, const FZOnGetRoleBiographyDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleBiographyDataAck>&)> OnGetRoleBiographyDataResult;
    void GetRoleBiographyData(const TSharedPtr<idlezt::GetRoleBiographyDataReq>& InReqMessage, const OnGetRoleBiographyDataResult& InCallback);    

    /**
     * 请求领取传记奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReceiveBiographyItem")
    void K2_ReceiveBiographyItem(const FZReceiveBiographyItemReq& InParams, const FZOnReceiveBiographyItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReceiveBiographyItemAck>&)> OnReceiveBiographyItemResult;
    void ReceiveBiographyItem(const TSharedPtr<idlezt::ReceiveBiographyItemReq>& InReqMessage, const OnReceiveBiographyItemResult& InCallback);    

    /**
     * 请求领取史记数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetBiographyEventData")
    void K2_GetBiographyEventData(const FZGetBiographyEventDataReq& InParams, const FZOnGetBiographyEventDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetBiographyEventDataAck>&)> OnGetBiographyEventDataResult;
    void GetBiographyEventData(const TSharedPtr<idlezt::GetBiographyEventDataReq>& InReqMessage, const OnGetBiographyEventDataResult& InCallback);    

    /**
     * 请求领取史记奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="ReceiveBiographyEventItem")
    void K2_ReceiveBiographyEventItem(const FZReceiveBiographyEventItemReq& InParams, const FZOnReceiveBiographyEventItemResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::ReceiveBiographyEventItemAck>&)> OnReceiveBiographyEventItemResult;
    void ReceiveBiographyEventItem(const TSharedPtr<idlezt::ReceiveBiographyEventItemReq>& InReqMessage, const OnReceiveBiographyEventItemResult& InCallback);    

    /**
     * 请求上传纪念日志
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="AddBiographyRoleLog")
    void K2_AddBiographyRoleLog(const FZAddBiographyRoleLogReq& InParams, const FZOnAddBiographyRoleLogResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::AddBiographyRoleLogAck>&)> OnAddBiographyRoleLogResult;
    void AddBiographyRoleLog(const TSharedPtr<idlezt::AddBiographyRoleLogReq>& InReqMessage, const OnAddBiographyRoleLogResult& InCallback);    

    /**
     * 请求进入镇魔深渊
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestEnterSeptDemonWorld")
    void K2_RequestEnterSeptDemonWorld(const FZRequestEnterSeptDemonWorldReq& InParams, const FZOnRequestEnterSeptDemonWorldResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestEnterSeptDemonWorldAck>&)> OnRequestEnterSeptDemonWorldResult;
    void RequestEnterSeptDemonWorld(const TSharedPtr<idlezt::RequestEnterSeptDemonWorldReq>& InReqMessage, const OnRequestEnterSeptDemonWorldResult& InCallback);    

    /**
     * 请求退出镇魔深渊
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestLeaveSeptDemonWorld")
    void K2_RequestLeaveSeptDemonWorld(const FZRequestLeaveSeptDemonWorldReq& InParams, const FZOnRequestLeaveSeptDemonWorldResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestLeaveSeptDemonWorldAck>&)> OnRequestLeaveSeptDemonWorldResult;
    void RequestLeaveSeptDemonWorld(const TSharedPtr<idlezt::RequestLeaveSeptDemonWorldReq>& InReqMessage, const OnRequestLeaveSeptDemonWorldResult& InCallback);    

    /**
     * 请求镇魔深渊相关数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestSeptDemonWorldData")
    void K2_RequestSeptDemonWorldData(const FZRequestSeptDemonWorldDataReq& InParams, const FZOnRequestSeptDemonWorldDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestSeptDemonWorldDataAck>&)> OnRequestSeptDemonWorldDataResult;
    void RequestSeptDemonWorldData(const TSharedPtr<idlezt::RequestSeptDemonWorldDataReq>& InReqMessage, const OnRequestSeptDemonWorldDataResult& InCallback);    

    /**
     * 请求在镇魔深渊待的最后时间点
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="RequestInSeptDemonWorldEndTime")
    void K2_RequestInSeptDemonWorldEndTime(const FZRequestInSeptDemonWorldEndTimeReq& InParams, const FZOnRequestInSeptDemonWorldEndTimeResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeAck>&)> OnRequestInSeptDemonWorldEndTimeResult;
    void RequestInSeptDemonWorldEndTime(const TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeReq>& InReqMessage, const OnRequestInSeptDemonWorldEndTimeResult& InCallback);    

    /**
     * 请求镇魔深渊待伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonDamageTopList")
    void K2_GetSeptDemonDamageTopList(const FZGetSeptDemonDamageTopListReq& InParams, const FZOnGetSeptDemonDamageTopListResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageTopListAck>&)> OnGetSeptDemonDamageTopListResult;
    void GetSeptDemonDamageTopList(const TSharedPtr<idlezt::GetSeptDemonDamageTopListReq>& InReqMessage, const OnGetSeptDemonDamageTopListResult& InCallback);    

    /**
     * 请求镇魔深渊待玩家伤害预览信息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonDamageSelfSummary")
    void K2_GetSeptDemonDamageSelfSummary(const FZGetSeptDemonDamageSelfSummaryReq& InParams, const FZOnGetSeptDemonDamageSelfSummaryResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryAck>&)> OnGetSeptDemonDamageSelfSummaryResult;
    void GetSeptDemonDamageSelfSummary(const TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryReq>& InReqMessage, const OnGetSeptDemonDamageSelfSummaryResult& InCallback);    

    /**
     * 请求镇魔深渊待宝库奖励剩余抽奖次数
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonStageRewardNum")
    void K2_GetSeptDemonStageRewardNum(const FZGetSeptDemonStageRewardNumReq& InParams, const FZOnGetSeptDemonStageRewardNumResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonStageRewardNumAck>&)> OnGetSeptDemonStageRewardNumResult;
    void GetSeptDemonStageRewardNum(const TSharedPtr<idlezt::GetSeptDemonStageRewardNumReq>& InReqMessage, const OnGetSeptDemonStageRewardNumResult& InCallback);    

    /**
     * 请求镇魔深渊待宝库奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonStageReward")
    void K2_GetSeptDemonStageReward(const FZGetSeptDemonStageRewardReq& InParams, const FZOnGetSeptDemonStageRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonStageRewardAck>&)> OnGetSeptDemonStageRewardResult;
    void GetSeptDemonStageReward(const TSharedPtr<idlezt::GetSeptDemonStageRewardReq>& InReqMessage, const OnGetSeptDemonStageRewardResult& InCallback);    

    /**
     * 请求镇魔深渊挑战奖励列表信息
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonDamageRewardsInfo")
    void K2_GetSeptDemonDamageRewardsInfo(const FZGetSeptDemonDamageRewardsInfoReq& InParams, const FZOnGetSeptDemonDamageRewardsInfoResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoAck>&)> OnGetSeptDemonDamageRewardsInfoResult;
    void GetSeptDemonDamageRewardsInfo(const TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoReq>& InReqMessage, const OnGetSeptDemonDamageRewardsInfoResult& InCallback);    

    /**
     * 请求镇魔深渊待挑战奖励
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetSeptDemonDamageReward")
    void K2_GetSeptDemonDamageReward(const FZGetSeptDemonDamageRewardReq& InParams, const FZOnGetSeptDemonDamageRewardResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageRewardAck>&)> OnGetSeptDemonDamageRewardResult;
    void GetSeptDemonDamageReward(const TSharedPtr<idlezt::GetSeptDemonDamageRewardReq>& InReqMessage, const OnGetSeptDemonDamageRewardResult& InCallback);    

    /**
     * 请求仙阁商店数据
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="GetRoleVipShopData")
    void K2_GetRoleVipShopData(const FZGetRoleVipShopDataReq& InParams, const FZOnGetRoleVipShopDataResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::GetRoleVipShopDataAck>&)> OnGetRoleVipShopDataResult;
    void GetRoleVipShopData(const TSharedPtr<idlezt::GetRoleVipShopDataReq>& InReqMessage, const OnGetRoleVipShopDataResult& InCallback);    

    /**
     * 请求仙阁商店购买
    */
    UFUNCTION(BlueprintCallable, Category="IdleZ", DisplayName="VipShopBuy")
    void K2_VipShopBuy(const FZVipShopBuyReq& InParams, const FZOnVipShopBuyResult& InCallback);
    
    typedef TFunction<void(EZRpcErrorCode, const TSharedPtr<idlezt::VipShopBuyAck>&)> OnVipShopBuyResult;
    void VipShopBuy(const TSharedPtr<idlezt::VipShopBuyReq>& InReqMessage, const OnVipShopBuyResult& InCallback);

    
    /**
     * 通知炼丹单次结果
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyAlchemyRefineResultResult OnNotifyAlchemyRefineResult;
    
    /**
     * 刷新道具数据
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnRefreshItemsResult OnRefreshItems;
    
    /**
     * 更新背包空间
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyInventorySpaceNumResult OnNotifyInventorySpaceNum;
    
    /**
     * 更新已解锁装备槽位列表
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnRefreshUnlockedEquipmentSlotsResult OnRefreshUnlockedEquipmentSlots;
    
    /**
     * 通知解锁挑战结果
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyUnlockArenaChallengeResultResult OnNotifyUnlockArenaChallengeResult;
    
    /**
     * 更新邮箱
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnUpdateRoleMailResult OnUpdateRoleMail;
    
    /**
     * 通知炼器单次结果
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyForgeRefineResultResult OnNotifyForgeRefineResult;
    
    /**
     * 礼包结果通知
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyGiftPackageResultResult OnNotifyGiftPackageResult;
    
    /**
     * 使用属性丹药通知
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyUsePillPropertyResult OnNotifyUsePillProperty;
    
    /**
     * 通知背包已经满，道具经邮件发送
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyInventoryFullMailItemResult OnNotifyInventoryFullMailItem;
    
    /**
     * 通知古宝数据刷新
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyRoleCollectionDataResult OnNotifyRoleCollectionData;
    
    /**
     * 通知古宝通用碎片刷新
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyCommonCollectionPieceDataResult OnNotifyCommonCollectionPieceData;
    
    /**
     * 通知古宝更新已经激活套装
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyCollectionActivatedSuitResult OnNotifyCollectionActivatedSuit;
    
    /**
     * 通知古宝渊源更新
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyRoleCollectionHistoriesResult OnNotifyRoleCollectionHistories;
    
    /**
     * 通知更新古宝累计收集奖励领取情况
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyCollectionZoneActiveAwardsResult OnNotifyCollectionZoneActiveAwards;
    
    /**
     * 通知下次可重置强化的时间
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyRoleCollectionNextResetEnhanceTicksResult OnNotifyRoleCollectionNextResetEnhanceTicks;
    
    /**
     * 入侵BOSS被击杀
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyBossInvasionNpcKilledResult OnNotifyBossInvasionNpcKilled;
    
    /**
     * 福缘任务通知
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyChecklistResult OnNotifyChecklist;
    
    /**
     * 通知玩家中立秘境矿脉采集结束
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifySeptStoneOccupyEndResult OnNotifySeptStoneOccupyEnd;
    
    /**
     * 通知传送失败
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyTeleportFailedResult OnNotifyTeleportFailed;
    
    /**
     * 福赠完成通知
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyFuZengResult OnNotifyFuZeng;
    
    /**
     * 通知计数器更新
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnUpdateLifeCounterResult OnUpdateLifeCounter;
    
    /**
     * 通知任务对战挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyQuestFightChallengeOverResult OnNotifyQuestFightChallengeOver;
    
    /**
     * 副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnDungeonChallengeOverResult OnDungeonChallengeOver;
    
    /**
     * 切磋结果数据
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifySoloArenaChallengeOverResult OnNotifySoloArenaChallengeOver;
    
    /**
     * 通知聊天消息
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnUpdateChatResult OnUpdateChat;
    
    /**
     * 询问剿灭副本是否完成
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyDungeonKillAllChallengeCurWaveNumResult OnNotifyDungeonKillAllChallengeCurWaveNum;
    
    /**
     * 剿灭副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyDungeonKillAllChallengeOverResult OnNotifyDungeonKillAllChallengeOver;
    
    /**
     * 询问生存副本是否完成
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyDungeonSurviveChallengeCurWaveNumResult OnNotifyDungeonSurviveChallengeCurWaveNum;
    
    /**
     * 生存副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyDungeonSurviveChallengeOverResult OnNotifyDungeonSurviveChallengeOver;
    
    /**
     * 通知道友功能消息
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyFriendMessageResult OnNotifyFriendMessage;
    
    /**
     * 传记功能通知（包括史记或纪念）
    */
    UPROPERTY(BlueprintAssignable, Category="IdleZ") 
    FZOnNotifyBiographyMessageResult OnNotifyBiographyMessage;
    
    
private:
    FZRpcManager* Manager = nullptr;
    FZPbConnectionPtr Connection;
};
