#pragma once

#include "MyNetFwd.h"
#include "MRpcManager.h"

#include "PbCommon.h"
#include "PbGame.h"
#include "common.pb.h"
#include "game.pb.h"

#include "GameRpcStub.generated.h"


DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLoginGameResult, EPbRpcErrorCode, InErrorCode, FZLoginGameAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetCurrentCultivationDirectionResult, EPbRpcErrorCode, InErrorCode, FZSetCurrentCultivationDirectionAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDoBreakthroughResult, EPbRpcErrorCode, InErrorCode, FZDoBreakthroughAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestCommonCultivationDataResult, EPbRpcErrorCode, InErrorCode, FZRequestCommonCultivationDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickMergeBreathingResult, EPbRpcErrorCode, InErrorCode, FZOneClickMergeBreathingAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBreathingExerciseRewardResult, EPbRpcErrorCode, InErrorCode, FZReceiveBreathingExerciseRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetInventoryDataResult, EPbRpcErrorCode, InErrorCode, FZGetInventoryDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetQuestDataResult, EPbRpcErrorCode, InErrorCode, FZGetQuestDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnCreateCharacterResult, EPbRpcErrorCode, InErrorCode, FZCreateCharacterAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseItemResult, EPbRpcErrorCode, InErrorCode, FZUseItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseSelectGiftResult, EPbRpcErrorCode, InErrorCode, FZUseSelectGiftAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSellItemResult, EPbRpcErrorCode, InErrorCode, FZSellItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockEquipmentSlotResult, EPbRpcErrorCode, InErrorCode, FZUnlockEquipmentSlotAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineStartResult, EPbRpcErrorCode, InErrorCode, FZAlchemyRefineStartAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineCancelResult, EPbRpcErrorCode, InErrorCode, FZAlchemyRefineCancelAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAlchemyRefineExtractResult, EPbRpcErrorCode, InErrorCode, FZAlchemyRefineExtractAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleShopDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshShopResult, EPbRpcErrorCode, InErrorCode, FZRefreshShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuyShopItemResult, EPbRpcErrorCode, InErrorCode, FZBuyShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleDeluxeShopDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleDeluxeShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshDeluxeShopResult, EPbRpcErrorCode, InErrorCode, FZRefreshDeluxeShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuyDeluxeShopItemResult, EPbRpcErrorCode, InErrorCode, FZBuyDeluxeShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetTemporaryPackageDataResult, EPbRpcErrorCode, InErrorCode, FZGetTemporaryPackageDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExtractTemporaryPackageItemsResult, EPbRpcErrorCode, InErrorCode, FZExtractTemporaryPackageItemsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSpeedupReliveResult, EPbRpcErrorCode, InErrorCode, FZSpeedupReliveAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMapInfoResult, EPbRpcErrorCode, InErrorCode, FZGetMapInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockArenaResult, EPbRpcErrorCode, InErrorCode, FZUnlockArenaAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQuestOpResult, EPbRpcErrorCode, InErrorCode, FZQuestOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEquipmentPutOnResult, EPbRpcErrorCode, InErrorCode, FZEquipmentPutOnAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEquipmentTakeOffResult, EPbRpcErrorCode, InErrorCode, FZEquipmentTakeOffAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLeaderboardPreviewResult, EPbRpcErrorCode, InErrorCode, FZGetLeaderboardPreviewAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLeaderboardDataResult, EPbRpcErrorCode, InErrorCode, FZGetLeaderboardDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleLeaderboardDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleLeaderboardDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLeaderboardClickLikeResult, EPbRpcErrorCode, InErrorCode, FZLeaderboardClickLikeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLeaderboardUpdateMessageResult, EPbRpcErrorCode, InErrorCode, FZLeaderboardUpdateMessageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetFuZeRewardResult, EPbRpcErrorCode, InErrorCode, FZGetFuZeRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleMailDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleMailDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReadMailResult, EPbRpcErrorCode, InErrorCode, FZReadMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMailAttachmentResult, EPbRpcErrorCode, InErrorCode, FZGetMailAttachmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDeleteMailResult, EPbRpcErrorCode, InErrorCode, FZDeleteMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickGetMailAttachmentResult, EPbRpcErrorCode, InErrorCode, FZOneClickGetMailAttachmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickReadMailResult, EPbRpcErrorCode, InErrorCode, FZOneClickReadMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickDeleteMailResult, EPbRpcErrorCode, InErrorCode, FZOneClickDeleteMailAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUnlockFunctionModuleResult, EPbRpcErrorCode, InErrorCode, FZUnlockFunctionModuleAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetChatRecordResult, EPbRpcErrorCode, InErrorCode, FZGetChatRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDeletePrivateChatRecordResult, EPbRpcErrorCode, InErrorCode, FZDeletePrivateChatRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSendChatMessageResult, EPbRpcErrorCode, InErrorCode, FZSendChatMessageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnClearChatUnreadNumResult, EPbRpcErrorCode, InErrorCode, FZClearChatUnreadNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineStartResult, EPbRpcErrorCode, InErrorCode, FZForgeRefineStartAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineCancelResult, EPbRpcErrorCode, InErrorCode, FZForgeRefineCancelAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeRefineExtractResult, EPbRpcErrorCode, InErrorCode, FZForgeRefineExtractAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetForgeLostEquipmentDataResult, EPbRpcErrorCode, InErrorCode, FZGetForgeLostEquipmentDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeDestroyResult, EPbRpcErrorCode, InErrorCode, FZForgeDestroyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnForgeFindBackResult, EPbRpcErrorCode, InErrorCode, FZForgeFindBackAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestPillElixirDataResult, EPbRpcErrorCode, InErrorCode, FZRequestPillElixirDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetOnePillElixirDataResult, EPbRpcErrorCode, InErrorCode, FZGetOnePillElixirDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestModifyPillElixirFilterResult, EPbRpcErrorCode, InErrorCode, FZRequestModifyPillElixirFilterAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUsePillElixirResult, EPbRpcErrorCode, InErrorCode, FZUsePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickUsePillElixirResult, EPbRpcErrorCode, InErrorCode, FZOneClickUsePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnTradePillElixirResult, EPbRpcErrorCode, InErrorCode, FZTradePillElixirAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReinforceEquipmentResult, EPbRpcErrorCode, InErrorCode, FZReinforceEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefineEquipmentResult, EPbRpcErrorCode, InErrorCode, FZRefineEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQiWenEquipmentResult, EPbRpcErrorCode, InErrorCode, FZQiWenEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnResetEquipmentResult, EPbRpcErrorCode, InErrorCode, FZResetEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnInheritEquipmentResult, EPbRpcErrorCode, InErrorCode, FZInheritEquipmentAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnLockItemResult, EPbRpcErrorCode, InErrorCode, FZLockItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSoloArenaChallengeResult, EPbRpcErrorCode, InErrorCode, FZSoloArenaChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSoloArenaQuickEndResult, EPbRpcErrorCode, InErrorCode, FZSoloArenaQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSoloArenaHistoryListResult, EPbRpcErrorCode, InErrorCode, FZGetSoloArenaHistoryListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerChallengeResult, EPbRpcErrorCode, InErrorCode, FZMonsterTowerChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerDrawIdleAwardResult, EPbRpcErrorCode, InErrorCode, FZMonsterTowerDrawIdleAwardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerClosedDoorTrainingResult, EPbRpcErrorCode, InErrorCode, FZMonsterTowerClosedDoorTrainingAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnMonsterTowerQuickEndResult, EPbRpcErrorCode, InErrorCode, FZMonsterTowerQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMonsterTowerChallengeListResult, EPbRpcErrorCode, InErrorCode, FZGetMonsterTowerChallengeListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetMonsterTowerChallengeRewardResult, EPbRpcErrorCode, InErrorCode, FZGetMonsterTowerChallengeRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetWorldTimeDilationResult, EPbRpcErrorCode, InErrorCode, FZSetWorldTimeDilationAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetFightModeResult, EPbRpcErrorCode, InErrorCode, FZSetFightModeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpgradeQiCollectorResult, EPbRpcErrorCode, InErrorCode, FZUpgradeQiCollectorAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleAllStatsResult, EPbRpcErrorCode, InErrorCode, FZGetRoleAllStatsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShanhetuDataResult, EPbRpcErrorCode, InErrorCode, FZGetShanhetuDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetShanhetuUseConfigResult, EPbRpcErrorCode, InErrorCode, FZSetShanhetuUseConfigAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUseShanhetuResult, EPbRpcErrorCode, InErrorCode, FZUseShanhetuAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnStepShanhetuResult, EPbRpcErrorCode, InErrorCode, FZStepShanhetuAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShanhetuUseRecordResult, EPbRpcErrorCode, InErrorCode, FZGetShanhetuUseRecordAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetAttackLockTypeResult, EPbRpcErrorCode, InErrorCode, FZSetAttackLockTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetAttackUnlockTypeResult, EPbRpcErrorCode, InErrorCode, FZSetAttackUnlockTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetShowUnlockButtonResult, EPbRpcErrorCode, InErrorCode, FZSetShowUnlockButtonAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetUserVarResult, EPbRpcErrorCode, InErrorCode, FZGetUserVarRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetUserVarsResult, EPbRpcErrorCode, InErrorCode, FZGetUserVarsRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionArenaSummaryResult, EPbRpcErrorCode, InErrorCode, FZGetBossInvasionArenaSummaryRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionArenaTopListResult, EPbRpcErrorCode, InErrorCode, FZGetBossInvasionArenaTopListRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBossInvasionInfoResult, EPbRpcErrorCode, InErrorCode, FZGetBossInvasionInfoRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDrawBossInvasionKillRewardResult, EPbRpcErrorCode, InErrorCode, FZDrawBossInvasionKillRewardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDrawBossInvasionDamageRewardResult, EPbRpcErrorCode, InErrorCode, FZDrawBossInvasionDamageRewardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBossInvasionTeleportResult, EPbRpcErrorCode, InErrorCode, FZBossInvasionTeleportRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfItemResult, EPbRpcErrorCode, InErrorCode, FZShareSelfItemRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfItemsResult, EPbRpcErrorCode, InErrorCode, FZShareSelfItemsRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShareItemDataResult, EPbRpcErrorCode, InErrorCode, FZGetShareItemDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleCollectionDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleCollectionDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRoleCollectionOpResult, EPbRpcErrorCode, InErrorCode, FZRoleCollectionOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnShareSelfRoleCollectionResult, EPbRpcErrorCode, InErrorCode, FZShareSelfRoleCollectionRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetShareRoleCollectionDataResult, EPbRpcErrorCode, InErrorCode, FZGetShareRoleCollectionDataRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetChecklistDataResult, EPbRpcErrorCode, InErrorCode, FZGetChecklistDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnChecklistOpResult, EPbRpcErrorCode, InErrorCode, FZChecklistOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpdateChecklistResult, EPbRpcErrorCode, InErrorCode, FZUpdateChecklistAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSwordPkInfoResult, EPbRpcErrorCode, InErrorCode, FZGetSwordPkInfoRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkSignupResult, EPbRpcErrorCode, InErrorCode, FZSwordPkSignupRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkMatchingResult, EPbRpcErrorCode, InErrorCode, FZSwordPkMatchingRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkChallengeResult, EPbRpcErrorCode, InErrorCode, FZSwordPkChallengeRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkRevengeResult, EPbRpcErrorCode, InErrorCode, FZSwordPkRevengeRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSwordPkTopListResult, EPbRpcErrorCode, InErrorCode, FZGetSwordPkTopListRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSwordPkExchangeHeroCardResult, EPbRpcErrorCode, InErrorCode, FZSwordPkExchangeHeroCardRsp, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCommonItemExchangeDataResult, EPbRpcErrorCode, InErrorCode, FZGetCommonItemExchangeDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExchangeCommonItemResult, EPbRpcErrorCode, InErrorCode, FZExchangeCommonItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSynthesisCommonItemResult, EPbRpcErrorCode, InErrorCode, FZSynthesisCommonItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCandidatesSeptListResult, EPbRpcErrorCode, InErrorCode, FZGetCandidatesSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSearchSeptResult, EPbRpcErrorCode, InErrorCode, FZSearchSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptBaseInfoResult, EPbRpcErrorCode, InErrorCode, FZGetSeptBaseInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptMemberListResult, EPbRpcErrorCode, InErrorCode, FZGetSeptMemberListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnCreateSeptResult, EPbRpcErrorCode, InErrorCode, FZCreateSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDismissSeptResult, EPbRpcErrorCode, InErrorCode, FZDismissSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnExitSeptResult, EPbRpcErrorCode, InErrorCode, FZExitSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnApplyJoinSeptResult, EPbRpcErrorCode, InErrorCode, FZApplyJoinSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnApproveApplySeptResult, EPbRpcErrorCode, InErrorCode, FZApproveApplySeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetApplyJoinSeptListResult, EPbRpcErrorCode, InErrorCode, FZGetApplyJoinSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRespondInviteSeptResult, EPbRpcErrorCode, InErrorCode, FZRespondInviteSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetInviteMeJoinSeptListResult, EPbRpcErrorCode, InErrorCode, FZGetInviteMeJoinSeptListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetCandidatesInviteRoleListResult, EPbRpcErrorCode, InErrorCode, FZGetCandidatesInviteRoleListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnInviteJoinSeptResult, EPbRpcErrorCode, InErrorCode, FZInviteJoinSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetSeptSettingsResult, EPbRpcErrorCode, InErrorCode, FZSetSeptSettingsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnSetSeptAnnounceResult, EPbRpcErrorCode, InErrorCode, FZSetSeptAnnounceAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnChangeSeptNameResult, EPbRpcErrorCode, InErrorCode, FZChangeSeptNameAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptLogResult, EPbRpcErrorCode, InErrorCode, FZGetSeptLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnConstructSeptResult, EPbRpcErrorCode, InErrorCode, FZConstructSeptAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetConstructSeptLogResult, EPbRpcErrorCode, InErrorCode, FZGetConstructSeptLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptInvitedRoleDailyNumResult, EPbRpcErrorCode, InErrorCode, FZGetSeptInvitedRoleDailyNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppointSeptPositionResult, EPbRpcErrorCode, InErrorCode, FZAppointSeptPositionAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnResignSeptChairmanResult, EPbRpcErrorCode, InErrorCode, FZResignSeptChairmanAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnKickOutSeptMemberResult, EPbRpcErrorCode, InErrorCode, FZKickOutSeptMemberAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleSeptShopDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleSeptShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBuySeptShopItemResult, EPbRpcErrorCode, InErrorCode, FZBuySeptShopItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleSeptQuestDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleSeptQuestDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReqRoleSeptQuestOpResult, EPbRpcErrorCode, InErrorCode, FZReqRoleSeptQuestOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshSeptQuestResult, EPbRpcErrorCode, InErrorCode, FZRefreshSeptQuestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReqSeptQuestRankUpResult, EPbRpcErrorCode, InErrorCode, FZReqSeptQuestRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnBeginOccupySeptStoneResult, EPbRpcErrorCode, InErrorCode, FZBeginOccupySeptStoneAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnEndOccupySeptStoneResult, EPbRpcErrorCode, InErrorCode, FZEndOccupySeptStoneAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOccupySeptLandResult, EPbRpcErrorCode, InErrorCode, FZOccupySeptLandAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetGongFaDataResult, EPbRpcErrorCode, InErrorCode, FZGetGongFaDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGongFaOpResult, EPbRpcErrorCode, InErrorCode, FZGongFaOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnActivateGongFaMaxEffectResult, EPbRpcErrorCode, InErrorCode, FZActivateGongFaMaxEffectAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptLandDamageTopListResult, EPbRpcErrorCode, InErrorCode, FZGetSeptLandDamageTopListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveFuZengRewardsResult, EPbRpcErrorCode, InErrorCode, FZReceiveFuZengRewardsAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleFuZengDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleFuZengDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleTreasuryDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleTreasuryDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOpenTreasuryChestResult, EPbRpcErrorCode, InErrorCode, FZOpenTreasuryChestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOneClickOpenTreasuryChestResult, EPbRpcErrorCode, InErrorCode, FZOneClickOpenTreasuryChestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnOpenTreasuryGachaResult, EPbRpcErrorCode, InErrorCode, FZOpenTreasuryGachaAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRefreshTreasuryShopResult, EPbRpcErrorCode, InErrorCode, FZRefreshTreasuryShopAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnTreasuryShopBuyResult, EPbRpcErrorCode, InErrorCode, FZTreasuryShopBuyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetLifeCounterDataResult, EPbRpcErrorCode, InErrorCode, FZGetLifeCounterDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDoQuestFightResult, EPbRpcErrorCode, InErrorCode, FZDoQuestFightAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnQuestFightQuickEndResult, EPbRpcErrorCode, InErrorCode, FZQuestFightQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetAppearanceDataResult, EPbRpcErrorCode, InErrorCode, FZGetAppearanceDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceAddResult, EPbRpcErrorCode, InErrorCode, FZAppearanceAddAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceActiveResult, EPbRpcErrorCode, InErrorCode, FZAppearanceActiveAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceWearResult, EPbRpcErrorCode, InErrorCode, FZAppearanceWearAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceBuyResult, EPbRpcErrorCode, InErrorCode, FZAppearanceBuyAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAppearanceChangeSkTypeResult, EPbRpcErrorCode, InErrorCode, FZAppearanceChangeSkTypeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBattleHistoryInfoResult, EPbRpcErrorCode, InErrorCode, FZGetBattleHistoryInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetArenaCheckListDataResult, EPbRpcErrorCode, InErrorCode, FZGetArenaCheckListDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnArenaCheckListSubmitResult, EPbRpcErrorCode, InErrorCode, FZArenaCheckListSubmitAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnArenaCheckListRewardSubmitResult, EPbRpcErrorCode, InErrorCode, FZArenaCheckListRewardSubmitAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllChallengeResult, EPbRpcErrorCode, InErrorCode, FZDungeonKillAllChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllQuickEndResult, EPbRpcErrorCode, InErrorCode, FZDungeonKillAllQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonKillAllDataResult, EPbRpcErrorCode, InErrorCode, FZDungeonKillAllDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetFarmlandDataResult, EPbRpcErrorCode, InErrorCode, FZGetFarmlandDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandUnlockBlockResult, EPbRpcErrorCode, InErrorCode, FZFarmlandUnlockBlockAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandPlantSeedResult, EPbRpcErrorCode, InErrorCode, FZFarmlandPlantSeedAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandWateringResult, EPbRpcErrorCode, InErrorCode, FZFarmlandWateringAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandRipeningResult, EPbRpcErrorCode, InErrorCode, FZFarmlandRipeningAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandHarvestResult, EPbRpcErrorCode, InErrorCode, FZFarmlandHarvestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmerRankUpResult, EPbRpcErrorCode, InErrorCode, FZFarmerRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFarmlandSetManagementResult, EPbRpcErrorCode, InErrorCode, FZFarmlandSetManagementAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnUpdateFarmlandStateResult, EPbRpcErrorCode, InErrorCode, FZUpdateFarmlandStateAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveChallengeResult, EPbRpcErrorCode, InErrorCode, FZDungeonSurviveChallengeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveQuickEndResult, EPbRpcErrorCode, InErrorCode, FZDungeonSurviveQuickEndAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDungeonSurviveDataResult, EPbRpcErrorCode, InErrorCode, FZDungeonSurviveDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRevertAllSkillCoolDownResult, EPbRpcErrorCode, InErrorCode, FZGetRevertAllSkillCoolDownAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleFriendDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleFriendDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFriendOpResult, EPbRpcErrorCode, InErrorCode, FZFriendOpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReplyFriendRequestResult, EPbRpcErrorCode, InErrorCode, FZReplyFriendRequestAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnFriendSearchRoleInfoResult, EPbRpcErrorCode, InErrorCode, FZFriendSearchRoleInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleInfoCacheResult, EPbRpcErrorCode, InErrorCode, FZGetRoleInfoCacheAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleInfoResult, EPbRpcErrorCode, InErrorCode, FZGetRoleInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleAvatarDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleAvatarDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnDispatchAvatarResult, EPbRpcErrorCode, InErrorCode, FZDispatchAvatarAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAvatarRankUpResult, EPbRpcErrorCode, InErrorCode, FZAvatarRankUpAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveAvatarTempPackageResult, EPbRpcErrorCode, InErrorCode, FZReceiveAvatarTempPackageAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetArenaExplorationStatisticalDataResult, EPbRpcErrorCode, InErrorCode, FZGetArenaExplorationStatisticalDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleBiographyDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleBiographyDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBiographyItemResult, EPbRpcErrorCode, InErrorCode, FZReceiveBiographyItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetBiographyEventDataResult, EPbRpcErrorCode, InErrorCode, FZGetBiographyEventDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnReceiveBiographyEventItemResult, EPbRpcErrorCode, InErrorCode, FZReceiveBiographyEventItemAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnAddBiographyRoleLogResult, EPbRpcErrorCode, InErrorCode, FZAddBiographyRoleLogAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestEnterSeptDemonWorldResult, EPbRpcErrorCode, InErrorCode, FZRequestEnterSeptDemonWorldAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestLeaveSeptDemonWorldResult, EPbRpcErrorCode, InErrorCode, FZRequestLeaveSeptDemonWorldAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestSeptDemonWorldDataResult, EPbRpcErrorCode, InErrorCode, FZRequestSeptDemonWorldDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnRequestInSeptDemonWorldEndTimeResult, EPbRpcErrorCode, InErrorCode, FZRequestInSeptDemonWorldEndTimeAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageTopListResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonDamageTopListAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageSelfSummaryResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonDamageSelfSummaryAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonStageRewardNumResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonStageRewardNumAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonStageRewardResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonStageRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageRewardsInfoResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonDamageRewardsInfoAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetSeptDemonDamageRewardResult, EPbRpcErrorCode, InErrorCode, FZGetSeptDemonDamageRewardAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnGetRoleVipShopDataResult, EPbRpcErrorCode, InErrorCode, FZGetRoleVipShopDataAck, InData);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FZOnVipShopBuyResult, EPbRpcErrorCode, InErrorCode, FZVipShopBuyAck, InData);



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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyFarmlandMessageResult, FZNotifyFarmlandMessage, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonSurviveChallengeCurWaveNumResult, FZNotifyDungeonSurviveChallengeCurWaveNum, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyDungeonSurviveChallengeOverResult, FZNotifyDungeonSurviveChallengeOver, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyFriendMessageResult, FZNotifyFriendMessage, InData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FZOnNotifyBiographyMessageResult, FZNotifyBiographyMessage, InData);


UCLASS(BlueprintType, Blueprintable)
class MRPC_API UZGameRpcStub : public UObject
{
    GENERATED_BODY()

public:

    void Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn);
    void Cleanup();    

    /**
     * 登录游戏
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LoginGame")
    void K2_LoginGame(const FZLoginGameReq& InParams, const FZOnLoginGameResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LoginGameAck>&)> OnLoginGameResult;
    void LoginGame(const TSharedPtr<idlepb::LoginGameReq>& InReqMessage, const OnLoginGameResult& InCallback);    

    /**
     * 设置修炼方向
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetCurrentCultivationDirection")
    void K2_SetCurrentCultivationDirection(const FZSetCurrentCultivationDirectionReq& InParams, const FZOnSetCurrentCultivationDirectionResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetCurrentCultivationDirectionAck>&)> OnSetCurrentCultivationDirectionResult;
    void SetCurrentCultivationDirection(const TSharedPtr<idlepb::SetCurrentCultivationDirectionReq>& InReqMessage, const OnSetCurrentCultivationDirectionResult& InCallback);    

    /**
     * 突破
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DoBreakthrough")
    void K2_DoBreakthrough(const FZDoBreakthroughReq& InParams, const FZOnDoBreakthroughResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DoBreakthroughAck>&)> OnDoBreakthroughResult;
    void DoBreakthrough(const TSharedPtr<idlepb::DoBreakthroughReq>& InReqMessage, const OnDoBreakthroughResult& InCallback);    

    /**
     * 请求公共修炼数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestCommonCultivationData")
    void K2_RequestCommonCultivationData(const FZRequestCommonCultivationDataReq& InParams, const FZOnRequestCommonCultivationDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestCommonCultivationDataAck>&)> OnRequestCommonCultivationDataResult;
    void RequestCommonCultivationData(const TSharedPtr<idlepb::RequestCommonCultivationDataReq>& InReqMessage, const OnRequestCommonCultivationDataResult& InCallback);    

    /**
     * 请求合并吐纳
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickMergeBreathing")
    void K2_OneClickMergeBreathing(const FZOneClickMergeBreathingReq& InParams, const FZOnOneClickMergeBreathingResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickMergeBreathingAck>&)> OnOneClickMergeBreathingResult;
    void OneClickMergeBreathing(const TSharedPtr<idlepb::OneClickMergeBreathingReq>& InReqMessage, const OnOneClickMergeBreathingResult& InCallback);    

    /**
     * 请求领取吐纳奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReceiveBreathingExerciseReward")
    void K2_ReceiveBreathingExerciseReward(const FZReceiveBreathingExerciseRewardReq& InParams, const FZOnReceiveBreathingExerciseRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReceiveBreathingExerciseRewardAck>&)> OnReceiveBreathingExerciseRewardResult;
    void ReceiveBreathingExerciseReward(const TSharedPtr<idlepb::ReceiveBreathingExerciseRewardReq>& InReqMessage, const OnReceiveBreathingExerciseRewardResult& InCallback);    

    /**
     * 请求包裹数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetInventoryData")
    void K2_GetInventoryData(const FZGetInventoryDataReq& InParams, const FZOnGetInventoryDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetInventoryDataAck>&)> OnGetInventoryDataResult;
    void GetInventoryData(const TSharedPtr<idlepb::GetInventoryDataReq>& InReqMessage, const OnGetInventoryDataResult& InCallback);    

    /**
     * 请求任务数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetQuestData")
    void K2_GetQuestData(const FZGetQuestDataReq& InParams, const FZOnGetQuestDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetQuestDataAck>&)> OnGetQuestDataResult;
    void GetQuestData(const TSharedPtr<idlepb::GetQuestDataReq>& InReqMessage, const OnGetQuestDataResult& InCallback);    

    /**
     * 创建角色
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="CreateCharacter")
    void K2_CreateCharacter(const FZCreateCharacterReq& InParams, const FZOnCreateCharacterResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::CreateCharacterAck>&)> OnCreateCharacterResult;
    void CreateCharacter(const TSharedPtr<idlepb::CreateCharacterReq>& InReqMessage, const OnCreateCharacterResult& InCallback);    

    /**
     * 使用道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UseItem")
    void K2_UseItem(const FZUseItemReq& InParams, const FZOnUseItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UseItemAck>&)> OnUseItemResult;
    void UseItem(const TSharedPtr<idlepb::UseItemReq>& InReqMessage, const OnUseItemResult& InCallback);    

    /**
     * 使用自选宝箱
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UseSelectGift")
    void K2_UseSelectGift(const FZUseSelectGiftReq& InParams, const FZOnUseSelectGiftResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UseSelectGiftAck>&)> OnUseSelectGiftResult;
    void UseSelectGift(const TSharedPtr<idlepb::UseSelectGiftReq>& InReqMessage, const OnUseSelectGiftResult& InCallback);    

    /**
     * 出售道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SellItem")
    void K2_SellItem(const FZSellItemReq& InParams, const FZOnSellItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SellItemAck>&)> OnSellItemResult;
    void SellItem(const TSharedPtr<idlepb::SellItemReq>& InReqMessage, const OnSellItemResult& InCallback);    

    /**
     * 解锁装备槽位
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UnlockEquipmentSlot")
    void K2_UnlockEquipmentSlot(const FZUnlockEquipmentSlotReq& InParams, const FZOnUnlockEquipmentSlotResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UnlockEquipmentSlotAck>&)> OnUnlockEquipmentSlotResult;
    void UnlockEquipmentSlot(const TSharedPtr<idlepb::UnlockEquipmentSlotReq>& InReqMessage, const OnUnlockEquipmentSlotResult& InCallback);    

    /**
     * 开始炼丹
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AlchemyRefineStart")
    void K2_AlchemyRefineStart(const FZAlchemyRefineStartReq& InParams, const FZOnAlchemyRefineStartResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AlchemyRefineStartAck>&)> OnAlchemyRefineStartResult;
    void AlchemyRefineStart(const TSharedPtr<idlepb::AlchemyRefineStartReq>& InReqMessage, const OnAlchemyRefineStartResult& InCallback);    

    /**
     * 终止炼丹
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AlchemyRefineCancel")
    void K2_AlchemyRefineCancel(const FZAlchemyRefineCancelReq& InParams, const FZOnAlchemyRefineCancelResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AlchemyRefineCancelAck>&)> OnAlchemyRefineCancelResult;
    void AlchemyRefineCancel(const TSharedPtr<idlepb::AlchemyRefineCancelReq>& InReqMessage, const OnAlchemyRefineCancelResult& InCallback);    

    /**
     * 领取丹药
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AlchemyRefineExtract")
    void K2_AlchemyRefineExtract(const FZAlchemyRefineExtractReq& InParams, const FZOnAlchemyRefineExtractResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AlchemyRefineExtractAck>&)> OnAlchemyRefineExtractResult;
    void AlchemyRefineExtract(const TSharedPtr<idlepb::AlchemyRefineExtractReq>& InReqMessage, const OnAlchemyRefineExtractResult& InCallback);    

    /**
     * 获取坊市数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleShopData")
    void K2_GetRoleShopData(const FZGetRoleShopDataReq& InParams, const FZOnGetRoleShopDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleShopDataAck>&)> OnGetRoleShopDataResult;
    void GetRoleShopData(const TSharedPtr<idlepb::GetRoleShopDataReq>& InReqMessage, const OnGetRoleShopDataResult& InCallback);    

    /**
     * 手动刷新坊市
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RefreshShop")
    void K2_RefreshShop(const FZRefreshShopReq& InParams, const FZOnRefreshShopResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RefreshShopAck>&)> OnRefreshShopResult;
    void RefreshShop(const TSharedPtr<idlepb::RefreshShopReq>& InReqMessage, const OnRefreshShopResult& InCallback);    

    /**
     * 购买坊市道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="BuyShopItem")
    void K2_BuyShopItem(const FZBuyShopItemReq& InParams, const FZOnBuyShopItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::BuyShopItemAck>&)> OnBuyShopItemResult;
    void BuyShopItem(const TSharedPtr<idlepb::BuyShopItemReq>& InReqMessage, const OnBuyShopItemResult& InCallback);    

    /**
     * 获取天机阁数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleDeluxeShopData")
    void K2_GetRoleDeluxeShopData(const FZGetRoleDeluxeShopDataReq& InParams, const FZOnGetRoleDeluxeShopDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleDeluxeShopDataAck>&)> OnGetRoleDeluxeShopDataResult;
    void GetRoleDeluxeShopData(const TSharedPtr<idlepb::GetRoleDeluxeShopDataReq>& InReqMessage, const OnGetRoleDeluxeShopDataResult& InCallback);    

    /**
     * 手动刷新天机阁
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RefreshDeluxeShop")
    void K2_RefreshDeluxeShop(const FZRefreshDeluxeShopReq& InParams, const FZOnRefreshDeluxeShopResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RefreshDeluxeShopAck>&)> OnRefreshDeluxeShopResult;
    void RefreshDeluxeShop(const TSharedPtr<idlepb::RefreshDeluxeShopReq>& InReqMessage, const OnRefreshDeluxeShopResult& InCallback);    

    /**
     * 购买天机阁道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="BuyDeluxeShopItem")
    void K2_BuyDeluxeShopItem(const FZBuyDeluxeShopItemReq& InParams, const FZOnBuyDeluxeShopItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::BuyDeluxeShopItemAck>&)> OnBuyDeluxeShopItemResult;
    void BuyDeluxeShopItem(const TSharedPtr<idlepb::BuyDeluxeShopItemReq>& InReqMessage, const OnBuyDeluxeShopItemResult& InCallback);    

    /**
     * 获取临时包裹数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetTemporaryPackageData")
    void K2_GetTemporaryPackageData(const FZGetTemporaryPackageDataReq& InParams, const FZOnGetTemporaryPackageDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetTemporaryPackageDataAck>&)> OnGetTemporaryPackageDataResult;
    void GetTemporaryPackageData(const TSharedPtr<idlepb::GetTemporaryPackageDataReq>& InReqMessage, const OnGetTemporaryPackageDataResult& InCallback);    

    /**
     * 提取临时包裹中的道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ExtractTemporaryPackageItems")
    void K2_ExtractTemporaryPackageItems(const FZExtractTemporaryPackageItemsReq& InParams, const FZOnExtractTemporaryPackageItemsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ExtractTemporaryPackageItemsAck>&)> OnExtractTemporaryPackageItemsResult;
    void ExtractTemporaryPackageItems(const TSharedPtr<idlepb::ExtractTemporaryPackageItemsReq>& InReqMessage, const OnExtractTemporaryPackageItemsResult& InCallback);    

    /**
     * 加速重生
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SpeedupRelive")
    void K2_SpeedupRelive(const FZSpeedupReliveReq& InParams, const FZOnSpeedupReliveResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SpeedupReliveAck>&)> OnSpeedupReliveResult;
    void SpeedupRelive(const TSharedPtr<idlepb::SpeedupReliveReq>& InReqMessage, const OnSpeedupReliveResult& InCallback);    

    /**
     * 获取地图信息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetMapInfo")
    void K2_GetMapInfo(const FZGetMapInfoReq& InParams, const FZOnGetMapInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetMapInfoAck>&)> OnGetMapInfoResult;
    void GetMapInfo(const TSharedPtr<idlepb::GetMapInfoReq>& InReqMessage, const OnGetMapInfoResult& InCallback);    

    /**
     * 解锁指定秘境
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UnlockArena")
    void K2_UnlockArena(const FZUnlockArenaReq& InParams, const FZOnUnlockArenaResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UnlockArenaAck>&)> OnUnlockArenaResult;
    void UnlockArena(const TSharedPtr<idlepb::UnlockArenaReq>& InReqMessage, const OnUnlockArenaResult& InCallback);    

    /**
     * 请求任务操作
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="QuestOp")
    void K2_QuestOp(const FZQuestOpReq& InParams, const FZOnQuestOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::QuestOpAck>&)> OnQuestOpResult;
    void QuestOp(const TSharedPtr<idlepb::QuestOpReq>& InReqMessage, const OnQuestOpResult& InCallback);    

    /**
     * 穿装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="EquipmentPutOn")
    void K2_EquipmentPutOn(const FZEquipmentPutOnReq& InParams, const FZOnEquipmentPutOnResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::EquipmentPutOnAck>&)> OnEquipmentPutOnResult;
    void EquipmentPutOn(const TSharedPtr<idlepb::EquipmentPutOnReq>& InReqMessage, const OnEquipmentPutOnResult& InCallback);    

    /**
     * 脱装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="EquipmentTakeOff")
    void K2_EquipmentTakeOff(const FZEquipmentTakeOffReq& InParams, const FZOnEquipmentTakeOffResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::EquipmentTakeOffAck>&)> OnEquipmentTakeOffResult;
    void EquipmentTakeOff(const TSharedPtr<idlepb::EquipmentTakeOffReq>& InReqMessage, const OnEquipmentTakeOffResult& InCallback);    

    /**
     * 请求排行榜预览，每个榜的榜一数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetLeaderboardPreview")
    void K2_GetLeaderboardPreview(const FZGetLeaderboardPreviewReq& InParams, const FZOnGetLeaderboardPreviewResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetLeaderboardPreviewAck>&)> OnGetLeaderboardPreviewResult;
    void GetLeaderboardPreview(const TSharedPtr<idlepb::GetLeaderboardPreviewReq>& InReqMessage, const OnGetLeaderboardPreviewResult& InCallback);    

    /**
     * 请求排行榜数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetLeaderboardData")
    void K2_GetLeaderboardData(const FZGetLeaderboardDataReq& InParams, const FZOnGetLeaderboardDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetLeaderboardDataAck>&)> OnGetLeaderboardDataResult;
    void GetLeaderboardData(const TSharedPtr<idlepb::GetLeaderboardDataReq>& InReqMessage, const OnGetLeaderboardDataResult& InCallback);    

    /**
     * 请求单个玩家排行榜数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleLeaderboardData")
    void K2_GetRoleLeaderboardData(const FZGetRoleLeaderboardDataReq& InParams, const FZOnGetRoleLeaderboardDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleLeaderboardDataAck>&)> OnGetRoleLeaderboardDataResult;
    void GetRoleLeaderboardData(const TSharedPtr<idlepb::GetRoleLeaderboardDataReq>& InReqMessage, const OnGetRoleLeaderboardDataResult& InCallback);    

    /**
     * 请求排行榜点赞
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LeaderboardClickLike")
    void K2_LeaderboardClickLike(const FZLeaderboardClickLikeReq& InParams, const FZOnLeaderboardClickLikeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LeaderboardClickLikeAck>&)> OnLeaderboardClickLikeResult;
    void LeaderboardClickLike(const TSharedPtr<idlepb::LeaderboardClickLikeReq>& InReqMessage, const OnLeaderboardClickLikeResult& InCallback);    

    /**
     * 请求排行榜更新留言
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LeaderboardUpdateMessage")
    void K2_LeaderboardUpdateMessage(const FZLeaderboardUpdateMessageReq& InParams, const FZOnLeaderboardUpdateMessageResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LeaderboardUpdateMessageAck>&)> OnLeaderboardUpdateMessageResult;
    void LeaderboardUpdateMessage(const TSharedPtr<idlepb::LeaderboardUpdateMessageReq>& InReqMessage, const OnLeaderboardUpdateMessageResult& InCallback);    

    /**
     * 请求领取福泽奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetFuZeReward")
    void K2_GetFuZeReward(const FZGetFuZeRewardReq& InParams, const FZOnGetFuZeRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetFuZeRewardAck>&)> OnGetFuZeRewardResult;
    void GetFuZeReward(const TSharedPtr<idlepb::GetFuZeRewardReq>& InReqMessage, const OnGetFuZeRewardResult& InCallback);    

    /**
     * 请求邮箱数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleMailData")
    void K2_GetRoleMailData(const FZGetRoleMailDataReq& InParams, const FZOnGetRoleMailDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleMailDataAck>&)> OnGetRoleMailDataResult;
    void GetRoleMailData(const TSharedPtr<idlepb::GetRoleMailDataReq>& InReqMessage, const OnGetRoleMailDataResult& InCallback);    

    /**
     * 请求邮箱已读
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReadMail")
    void K2_ReadMail(const FZReadMailReq& InParams, const FZOnReadMailResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReadMailAck>&)> OnReadMailResult;
    void ReadMail(const TSharedPtr<idlepb::ReadMailReq>& InReqMessage, const OnReadMailResult& InCallback);    

    /**
     * 请求邮箱领取
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetMailAttachment")
    void K2_GetMailAttachment(const FZGetMailAttachmentReq& InParams, const FZOnGetMailAttachmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetMailAttachmentAck>&)> OnGetMailAttachmentResult;
    void GetMailAttachment(const TSharedPtr<idlepb::GetMailAttachmentReq>& InReqMessage, const OnGetMailAttachmentResult& InCallback);    

    /**
     * 请求删除邮件
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DeleteMail")
    void K2_DeleteMail(const FZDeleteMailReq& InParams, const FZOnDeleteMailResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DeleteMailAck>&)> OnDeleteMailResult;
    void DeleteMail(const TSharedPtr<idlepb::DeleteMailReq>& InReqMessage, const OnDeleteMailResult& InCallback);    

    /**
     * 请求邮件一键领取
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickGetMailAttachment")
    void K2_OneClickGetMailAttachment(const FZOneClickGetMailAttachmentReq& InParams, const FZOnOneClickGetMailAttachmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickGetMailAttachmentAck>&)> OnOneClickGetMailAttachmentResult;
    void OneClickGetMailAttachment(const TSharedPtr<idlepb::OneClickGetMailAttachmentReq>& InReqMessage, const OnOneClickGetMailAttachmentResult& InCallback);    

    /**
     * 请求邮件一键已读
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickReadMail")
    void K2_OneClickReadMail(const FZOneClickReadMailReq& InParams, const FZOnOneClickReadMailResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickReadMailAck>&)> OnOneClickReadMailResult;
    void OneClickReadMail(const TSharedPtr<idlepb::OneClickReadMailReq>& InReqMessage, const OnOneClickReadMailResult& InCallback);    

    /**
     * 请求邮件一键删除
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickDeleteMail")
    void K2_OneClickDeleteMail(const FZOneClickDeleteMailReq& InParams, const FZOnOneClickDeleteMailResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickDeleteMailAck>&)> OnOneClickDeleteMailResult;
    void OneClickDeleteMail(const TSharedPtr<idlepb::OneClickDeleteMailReq>& InReqMessage, const OnOneClickDeleteMailResult& InCallback);    

    /**
     * 解锁指定模块
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UnlockFunctionModule")
    void K2_UnlockFunctionModule(const FZUnlockFunctionModuleReq& InParams, const FZOnUnlockFunctionModuleResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UnlockFunctionModuleAck>&)> OnUnlockFunctionModuleResult;
    void UnlockFunctionModule(const TSharedPtr<idlepb::UnlockFunctionModuleReq>& InReqMessage, const OnUnlockFunctionModuleResult& InCallback);    

    /**
     * 请求聊天消息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetChatRecord")
    void K2_GetChatRecord(const FZGetChatRecordReq& InParams, const FZOnGetChatRecordResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetChatRecordAck>&)> OnGetChatRecordResult;
    void GetChatRecord(const TSharedPtr<idlepb::GetChatRecordReq>& InReqMessage, const OnGetChatRecordResult& InCallback);    

    /**
     * 请求删除私聊消息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DeletePrivateChatRecord")
    void K2_DeletePrivateChatRecord(const FZDeletePrivateChatRecordReq& InParams, const FZOnDeletePrivateChatRecordResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DeletePrivateChatRecordAck>&)> OnDeletePrivateChatRecordResult;
    void DeletePrivateChatRecord(const TSharedPtr<idlepb::DeletePrivateChatRecordReq>& InReqMessage, const OnDeletePrivateChatRecordResult& InCallback);    

    /**
     * 发送聊天消息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SendChatMessage")
    void K2_SendChatMessage(const FZSendChatMessageReq& InParams, const FZOnSendChatMessageResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SendChatMessageAck>&)> OnSendChatMessageResult;
    void SendChatMessage(const TSharedPtr<idlepb::SendChatMessageReq>& InReqMessage, const OnSendChatMessageResult& InCallback);    

    /**
     * 请求聊天记录已读
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ClearChatUnreadNum")
    void K2_ClearChatUnreadNum(const FZClearChatUnreadNumReq& InParams, const FZOnClearChatUnreadNumResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ClearChatUnreadNumAck>&)> OnClearChatUnreadNumResult;
    void ClearChatUnreadNum(const TSharedPtr<idlepb::ClearChatUnreadNumReq>& InReqMessage, const OnClearChatUnreadNumResult& InCallback);    

    /**
     * 开始炼器
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ForgeRefineStart")
    void K2_ForgeRefineStart(const FZForgeRefineStartReq& InParams, const FZOnForgeRefineStartResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ForgeRefineStartAck>&)> OnForgeRefineStartResult;
    void ForgeRefineStart(const TSharedPtr<idlepb::ForgeRefineStartReq>& InReqMessage, const OnForgeRefineStartResult& InCallback);    

    /**
     * 终止炼器
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ForgeRefineCancel")
    void K2_ForgeRefineCancel(const FZForgeRefineCancelReq& InParams, const FZOnForgeRefineCancelResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ForgeRefineCancelAck>&)> OnForgeRefineCancelResult;
    void ForgeRefineCancel(const TSharedPtr<idlepb::ForgeRefineCancelReq>& InReqMessage, const OnForgeRefineCancelResult& InCallback);    

    /**
     * 领取炼器生成的道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ForgeRefineExtract")
    void K2_ForgeRefineExtract(const FZForgeRefineExtractReq& InParams, const FZOnForgeRefineExtractResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ForgeRefineExtractAck>&)> OnForgeRefineExtractResult;
    void ForgeRefineExtract(const TSharedPtr<idlepb::ForgeRefineExtractReq>& InReqMessage, const OnForgeRefineExtractResult& InCallback);    

    /**
     * 请求找回装备数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetForgeLostEquipmentData")
    void K2_GetForgeLostEquipmentData(const FZGetForgeLostEquipmentDataReq& InParams, const FZOnGetForgeLostEquipmentDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetForgeLostEquipmentDataAck>&)> OnGetForgeLostEquipmentDataResult;
    void GetForgeLostEquipmentData(const TSharedPtr<idlepb::GetForgeLostEquipmentDataReq>& InReqMessage, const OnGetForgeLostEquipmentDataResult& InCallback);    

    /**
     * 请求销毁装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ForgeDestroy")
    void K2_ForgeDestroy(const FZForgeDestroyReq& InParams, const FZOnForgeDestroyResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ForgeDestroyAck>&)> OnForgeDestroyResult;
    void ForgeDestroy(const TSharedPtr<idlepb::ForgeDestroyReq>& InReqMessage, const OnForgeDestroyResult& InCallback);    

    /**
     * 请求找回装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ForgeFindBack")
    void K2_ForgeFindBack(const FZForgeFindBackReq& InParams, const FZOnForgeFindBackResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ForgeFindBackAck>&)> OnForgeFindBackResult;
    void ForgeFindBack(const TSharedPtr<idlepb::ForgeFindBackReq>& InReqMessage, const OnForgeFindBackResult& InCallback);    

    /**
     * 请求秘药数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestPillElixirData")
    void K2_RequestPillElixirData(const FZRequestPillElixirDataReq& InParams, const FZOnRequestPillElixirDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestPillElixirDataAck>&)> OnRequestPillElixirDataResult;
    void RequestPillElixirData(const TSharedPtr<idlepb::RequestPillElixirDataReq>& InReqMessage, const OnRequestPillElixirDataResult& InCallback);    

    /**
     * 请求单种秘药数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetOnePillElixirData")
    void K2_GetOnePillElixirData(const FZGetOnePillElixirDataReq& InParams, const FZOnGetOnePillElixirDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetOnePillElixirDataAck>&)> OnGetOnePillElixirDataResult;
    void GetOnePillElixirData(const TSharedPtr<idlepb::GetOnePillElixirDataReq>& InReqMessage, const OnGetOnePillElixirDataResult& InCallback);    

    /**
     * 请求修改秘药过滤配置
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestModifyPillElixirFilter")
    void K2_RequestModifyPillElixirFilter(const FZRequestModifyPillElixirFilterReq& InParams, const FZOnRequestModifyPillElixirFilterResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestModifyPillElixirFilterAck>&)> OnRequestModifyPillElixirFilterResult;
    void RequestModifyPillElixirFilter(const TSharedPtr<idlepb::RequestModifyPillElixirFilterReq>& InReqMessage, const OnRequestModifyPillElixirFilterResult& InCallback);    

    /**
     * 使用单颗秘药
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UsePillElixir")
    void K2_UsePillElixir(const FZUsePillElixirReq& InParams, const FZOnUsePillElixirResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UsePillElixirAck>&)> OnUsePillElixirResult;
    void UsePillElixir(const TSharedPtr<idlepb::UsePillElixirReq>& InReqMessage, const OnUsePillElixirResult& InCallback);    

    /**
     * 一键使用秘药
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickUsePillElixir")
    void K2_OneClickUsePillElixir(const FZOneClickUsePillElixirReq& InParams, const FZOnOneClickUsePillElixirResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickUsePillElixirAck>&)> OnOneClickUsePillElixirResult;
    void OneClickUsePillElixir(const TSharedPtr<idlepb::OneClickUsePillElixirReq>& InReqMessage, const OnOneClickUsePillElixirResult& InCallback);    

    /**
     * 请求秘药兑换天机石
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="TradePillElixir")
    void K2_TradePillElixir(const FZTradePillElixirReq& InParams, const FZOnTradePillElixirResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::TradePillElixirAck>&)> OnTradePillElixirResult;
    void TradePillElixir(const TSharedPtr<idlepb::TradePillElixirReq>& InReqMessage, const OnTradePillElixirResult& InCallback);    

    /**
     * 请求强化装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReinforceEquipment")
    void K2_ReinforceEquipment(const FZReinforceEquipmentReq& InParams, const FZOnReinforceEquipmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReinforceEquipmentAck>&)> OnReinforceEquipmentResult;
    void ReinforceEquipment(const TSharedPtr<idlepb::ReinforceEquipmentReq>& InReqMessage, const OnReinforceEquipmentResult& InCallback);    

    /**
     * 请求精炼装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RefineEquipment")
    void K2_RefineEquipment(const FZRefineEquipmentReq& InParams, const FZOnRefineEquipmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RefineEquipmentAck>&)> OnRefineEquipmentResult;
    void RefineEquipment(const TSharedPtr<idlepb::RefineEquipmentReq>& InReqMessage, const OnRefineEquipmentResult& InCallback);    

    /**
     * 请求器纹装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="QiWenEquipment")
    void K2_QiWenEquipment(const FZQiWenEquipmentReq& InParams, const FZOnQiWenEquipmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::QiWenEquipmentAck>&)> OnQiWenEquipmentResult;
    void QiWenEquipment(const TSharedPtr<idlepb::QiWenEquipmentReq>& InReqMessage, const OnQiWenEquipmentResult& InCallback);    

    /**
     * 请求还原装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ResetEquipment")
    void K2_ResetEquipment(const FZResetEquipmentReq& InParams, const FZOnResetEquipmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ResetEquipmentAck>&)> OnResetEquipmentResult;
    void ResetEquipment(const TSharedPtr<idlepb::ResetEquipmentReq>& InReqMessage, const OnResetEquipmentResult& InCallback);    

    /**
     * 请求继承装备
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="InheritEquipment")
    void K2_InheritEquipment(const FZInheritEquipmentReq& InParams, const FZOnInheritEquipmentResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::InheritEquipmentAck>&)> OnInheritEquipmentResult;
    void InheritEquipment(const TSharedPtr<idlepb::InheritEquipmentReq>& InReqMessage, const OnInheritEquipmentResult& InCallback);    

    /**
     * 请求锁定/解锁道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="LockItem")
    void K2_LockItem(const FZLockItemReq& InParams, const FZOnLockItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::LockItemAck>&)> OnLockItemResult;
    void LockItem(const TSharedPtr<idlepb::LockItemReq>& InReqMessage, const OnLockItemResult& InCallback);    

    /**
     * 发起切磋
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SoloArenaChallenge")
    void K2_SoloArenaChallenge(const FZSoloArenaChallengeReq& InParams, const FZOnSoloArenaChallengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SoloArenaChallengeAck>&)> OnSoloArenaChallengeResult;
    void SoloArenaChallenge(const TSharedPtr<idlepb::SoloArenaChallengeReq>& InReqMessage, const OnSoloArenaChallengeResult& InCallback);    

    /**
     * 快速结束切磋
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SoloArenaQuickEnd")
    void K2_SoloArenaQuickEnd(const FZSoloArenaQuickEndReq& InParams, const FZOnSoloArenaQuickEndResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SoloArenaQuickEndAck>&)> OnSoloArenaQuickEndResult;
    void SoloArenaQuickEnd(const TSharedPtr<idlepb::SoloArenaQuickEndReq>& InReqMessage, const OnSoloArenaQuickEndResult& InCallback);    

    /**
     * 获取切磋历史列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSoloArenaHistoryList")
    void K2_GetSoloArenaHistoryList(const FZGetSoloArenaHistoryListReq& InParams, const FZOnGetSoloArenaHistoryListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSoloArenaHistoryListAck>&)> OnGetSoloArenaHistoryListResult;
    void GetSoloArenaHistoryList(const TSharedPtr<idlepb::GetSoloArenaHistoryListReq>& InReqMessage, const OnGetSoloArenaHistoryListResult& InCallback);    

    /**
     * 挑战镇妖塔
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="MonsterTowerChallenge")
    void K2_MonsterTowerChallenge(const FZMonsterTowerChallengeReq& InParams, const FZOnMonsterTowerChallengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::MonsterTowerChallengeAck>&)> OnMonsterTowerChallengeResult;
    void MonsterTowerChallenge(const TSharedPtr<idlepb::MonsterTowerChallengeReq>& InReqMessage, const OnMonsterTowerChallengeResult& InCallback);    

    /**
     * 领取镇妖塔挂机奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="MonsterTowerDrawIdleAward")
    void K2_MonsterTowerDrawIdleAward(const FZMonsterTowerDrawIdleAwardReq& InParams, const FZOnMonsterTowerDrawIdleAwardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::MonsterTowerDrawIdleAwardAck>&)> OnMonsterTowerDrawIdleAwardResult;
    void MonsterTowerDrawIdleAward(const TSharedPtr<idlepb::MonsterTowerDrawIdleAwardReq>& InReqMessage, const OnMonsterTowerDrawIdleAwardResult& InCallback);    

    /**
     * 镇妖塔闭关
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="MonsterTowerClosedDoorTraining")
    void K2_MonsterTowerClosedDoorTraining(const FZMonsterTowerClosedDoorTrainingReq& InParams, const FZOnMonsterTowerClosedDoorTrainingResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::MonsterTowerClosedDoorTrainingAck>&)> OnMonsterTowerClosedDoorTrainingResult;
    void MonsterTowerClosedDoorTraining(const TSharedPtr<idlepb::MonsterTowerClosedDoorTrainingReq>& InReqMessage, const OnMonsterTowerClosedDoorTrainingResult& InCallback);    

    /**
     * 镇妖塔快速结束
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="MonsterTowerQuickEnd")
    void K2_MonsterTowerQuickEnd(const FZMonsterTowerQuickEndReq& InParams, const FZOnMonsterTowerQuickEndResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::MonsterTowerQuickEndAck>&)> OnMonsterTowerQuickEndResult;
    void MonsterTowerQuickEnd(const TSharedPtr<idlepb::MonsterTowerQuickEndReq>& InReqMessage, const OnMonsterTowerQuickEndResult& InCallback);    

    /**
     * 镇妖塔挑战榜数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetMonsterTowerChallengeList")
    void K2_GetMonsterTowerChallengeList(const FZGetMonsterTowerChallengeListReq& InParams, const FZOnGetMonsterTowerChallengeListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetMonsterTowerChallengeListAck>&)> OnGetMonsterTowerChallengeListResult;
    void GetMonsterTowerChallengeList(const TSharedPtr<idlepb::GetMonsterTowerChallengeListReq>& InReqMessage, const OnGetMonsterTowerChallengeListResult& InCallback);    

    /**
     * 镇妖塔挑战榜奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetMonsterTowerChallengeReward")
    void K2_GetMonsterTowerChallengeReward(const FZGetMonsterTowerChallengeRewardReq& InParams, const FZOnGetMonsterTowerChallengeRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetMonsterTowerChallengeRewardAck>&)> OnGetMonsterTowerChallengeRewardResult;
    void GetMonsterTowerChallengeReward(const TSharedPtr<idlepb::GetMonsterTowerChallengeRewardReq>& InReqMessage, const OnGetMonsterTowerChallengeRewardResult& InCallback);    

    /**
     * 设置地图TimeDilation
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetWorldTimeDilation")
    void K2_SetWorldTimeDilation(const FZSetWorldTimeDilationReq& InParams, const FZOnSetWorldTimeDilationResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetWorldTimeDilationAck>&)> OnSetWorldTimeDilationResult;
    void SetWorldTimeDilation(const TSharedPtr<idlepb::SetWorldTimeDilationReq>& InReqMessage, const OnSetWorldTimeDilationResult& InCallback);    

    /**
     * 设置战斗模式
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetFightMode")
    void K2_SetFightMode(const FZSetFightModeReq& InParams, const FZOnSetFightModeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetFightModeAck>&)> OnSetFightModeResult;
    void SetFightMode(const TSharedPtr<idlepb::SetFightModeReq>& InReqMessage, const OnSetFightModeResult& InCallback);    

    /**
     * 升级聚灵阵
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UpgradeQiCollector")
    void K2_UpgradeQiCollector(const FZUpgradeQiCollectorReq& InParams, const FZOnUpgradeQiCollectorResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UpgradeQiCollectorAck>&)> OnUpgradeQiCollectorResult;
    void UpgradeQiCollector(const TSharedPtr<idlepb::UpgradeQiCollectorReq>& InReqMessage, const OnUpgradeQiCollectorResult& InCallback);    

    /**
     * 请求玩家的游戏数值数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleAllStats")
    void K2_GetRoleAllStats(const FZGetRoleAllStatsReq& InParams, const FZOnGetRoleAllStatsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleAllStatsAck>&)> OnGetRoleAllStatsResult;
    void GetRoleAllStats(const TSharedPtr<idlepb::GetRoleAllStatsReq>& InReqMessage, const OnGetRoleAllStatsResult& InCallback);    

    /**
     * 请求玩家山河图数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetShanhetuData")
    void K2_GetShanhetuData(const FZGetShanhetuDataReq& InParams, const FZOnGetShanhetuDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetShanhetuDataAck>&)> OnGetShanhetuDataResult;
    void GetShanhetuData(const TSharedPtr<idlepb::GetShanhetuDataReq>& InReqMessage, const OnGetShanhetuDataResult& InCallback);    

    /**
     * 请求修改山河图使用配置
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetShanhetuUseConfig")
    void K2_SetShanhetuUseConfig(const FZSetShanhetuUseConfigReq& InParams, const FZOnSetShanhetuUseConfigResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetShanhetuUseConfigAck>&)> OnSetShanhetuUseConfigResult;
    void SetShanhetuUseConfig(const TSharedPtr<idlepb::SetShanhetuUseConfigReq>& InReqMessage, const OnSetShanhetuUseConfigResult& InCallback);    

    /**
     * 请求使用山河图
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UseShanhetu")
    void K2_UseShanhetu(const FZUseShanhetuReq& InParams, const FZOnUseShanhetuResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UseShanhetuAck>&)> OnUseShanhetuResult;
    void UseShanhetu(const TSharedPtr<idlepb::UseShanhetuReq>& InReqMessage, const OnUseShanhetuResult& InCallback);    

    /**
     * 探索山河图
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="StepShanhetu")
    void K2_StepShanhetu(const FZStepShanhetuReq& InParams, const FZOnStepShanhetuResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::StepShanhetuAck>&)> OnStepShanhetuResult;
    void StepShanhetu(const TSharedPtr<idlepb::StepShanhetuReq>& InReqMessage, const OnStepShanhetuResult& InCallback);    

    /**
     * 请求山河图记录
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetShanhetuUseRecord")
    void K2_GetShanhetuUseRecord(const FZGetShanhetuUseRecordReq& InParams, const FZOnGetShanhetuUseRecordResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetShanhetuUseRecordAck>&)> OnGetShanhetuUseRecordResult;
    void GetShanhetuUseRecord(const TSharedPtr<idlepb::GetShanhetuUseRecordReq>& InReqMessage, const OnGetShanhetuUseRecordResult& InCallback);    

    /**
     * 设置锁定方式
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetAttackLockType")
    void K2_SetAttackLockType(const FZSetAttackLockTypeReq& InParams, const FZOnSetAttackLockTypeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetAttackLockTypeAck>&)> OnSetAttackLockTypeResult;
    void SetAttackLockType(const TSharedPtr<idlepb::SetAttackLockTypeReq>& InReqMessage, const OnSetAttackLockTypeResult& InCallback);    

    /**
     * 设置取消锁定方式
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetAttackUnlockType")
    void K2_SetAttackUnlockType(const FZSetAttackUnlockTypeReq& InParams, const FZOnSetAttackUnlockTypeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetAttackUnlockTypeAck>&)> OnSetAttackUnlockTypeResult;
    void SetAttackUnlockType(const TSharedPtr<idlepb::SetAttackUnlockTypeReq>& InReqMessage, const OnSetAttackUnlockTypeResult& InCallback);    

    /**
     * 设置是否显示解锁按钮
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetShowUnlockButton")
    void K2_SetShowUnlockButton(const FZSetShowUnlockButtonReq& InParams, const FZOnSetShowUnlockButtonResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetShowUnlockButtonAck>&)> OnSetShowUnlockButtonResult;
    void SetShowUnlockButton(const TSharedPtr<idlepb::SetShowUnlockButtonReq>& InReqMessage, const OnSetShowUnlockButtonResult& InCallback);    

    /**
     * 获取用户变量内容
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetUserVar")
    void K2_GetUserVar(const FZGetUserVarReq& InParams, const FZOnGetUserVarResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetUserVarRsp>&)> OnGetUserVarResult;
    void GetUserVar(const TSharedPtr<idlepb::GetUserVarReq>& InReqMessage, const OnGetUserVarResult& InCallback);    

    /**
     * 获取多个用户变量内容
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetUserVars")
    void K2_GetUserVars(const FZGetUserVarsReq& InParams, const FZOnGetUserVarsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetUserVarsRsp>&)> OnGetUserVarsResult;
    void GetUserVars(const TSharedPtr<idlepb::GetUserVarsReq>& InReqMessage, const OnGetUserVarsResult& InCallback);    

    /**
     * 获取指定秘境BOSS入侵情况
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetBossInvasionArenaSummary")
    void K2_GetBossInvasionArenaSummary(const FZGetBossInvasionArenaSummaryReq& InParams, const FZOnGetBossInvasionArenaSummaryResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetBossInvasionArenaSummaryRsp>&)> OnGetBossInvasionArenaSummaryResult;
    void GetBossInvasionArenaSummary(const TSharedPtr<idlepb::GetBossInvasionArenaSummaryReq>& InReqMessage, const OnGetBossInvasionArenaSummaryResult& InCallback);    

    /**
     * 获取指定秘境BOSS入侵伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetBossInvasionArenaTopList")
    void K2_GetBossInvasionArenaTopList(const FZGetBossInvasionArenaTopListReq& InParams, const FZOnGetBossInvasionArenaTopListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetBossInvasionArenaTopListRsp>&)> OnGetBossInvasionArenaTopListResult;
    void GetBossInvasionArenaTopList(const TSharedPtr<idlepb::GetBossInvasionArenaTopListReq>& InReqMessage, const OnGetBossInvasionArenaTopListResult& InCallback);    

    /**
     * 获取BOSS入侵情况
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetBossInvasionInfo")
    void K2_GetBossInvasionInfo(const FZGetBossInvasionInfoReq& InParams, const FZOnGetBossInvasionInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetBossInvasionInfoRsp>&)> OnGetBossInvasionInfoResult;
    void GetBossInvasionInfo(const TSharedPtr<idlepb::GetBossInvasionInfoReq>& InReqMessage, const OnGetBossInvasionInfoResult& InCallback);    

    /**
     * 领取击杀奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DrawBossInvasionKillReward")
    void K2_DrawBossInvasionKillReward(const FZDrawBossInvasionKillRewardReq& InParams, const FZOnDrawBossInvasionKillRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DrawBossInvasionKillRewardRsp>&)> OnDrawBossInvasionKillRewardResult;
    void DrawBossInvasionKillReward(const TSharedPtr<idlepb::DrawBossInvasionKillRewardReq>& InReqMessage, const OnDrawBossInvasionKillRewardResult& InCallback);    

    /**
     * 领取伤害排行奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DrawBossInvasionDamageReward")
    void K2_DrawBossInvasionDamageReward(const FZDrawBossInvasionDamageRewardReq& InParams, const FZOnDrawBossInvasionDamageRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DrawBossInvasionDamageRewardRsp>&)> OnDrawBossInvasionDamageRewardResult;
    void DrawBossInvasionDamageReward(const TSharedPtr<idlepb::DrawBossInvasionDamageRewardReq>& InReqMessage, const OnDrawBossInvasionDamageRewardResult& InCallback);    

    /**
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="BossInvasionTeleport")
    void K2_BossInvasionTeleport(const FZBossInvasionTeleportReq& InParams, const FZOnBossInvasionTeleportResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::BossInvasionTeleportRsp>&)> OnBossInvasionTeleportResult;
    void BossInvasionTeleport(const TSharedPtr<idlepb::BossInvasionTeleportReq>& InReqMessage, const OnBossInvasionTeleportResult& InCallback);    

    /**
     * 分享自己的道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ShareSelfItem")
    void K2_ShareSelfItem(const FZShareSelfItemReq& InParams, const FZOnShareSelfItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ShareSelfItemRsp>&)> OnShareSelfItemResult;
    void ShareSelfItem(const TSharedPtr<idlepb::ShareSelfItemReq>& InReqMessage, const OnShareSelfItemResult& InCallback);    

    /**
     * 分享自己的多个道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ShareSelfItems")
    void K2_ShareSelfItems(const FZShareSelfItemsReq& InParams, const FZOnShareSelfItemsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ShareSelfItemsRsp>&)> OnShareSelfItemsResult;
    void ShareSelfItems(const TSharedPtr<idlepb::ShareSelfItemsReq>& InReqMessage, const OnShareSelfItemsResult& InCallback);    

    /**
     * 获取分享道具数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetShareItemData")
    void K2_GetShareItemData(const FZGetShareItemDataReq& InParams, const FZOnGetShareItemDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetShareItemDataRsp>&)> OnGetShareItemDataResult;
    void GetShareItemData(const TSharedPtr<idlepb::GetShareItemDataReq>& InReqMessage, const OnGetShareItemDataResult& InCallback);    

    /**
     * 获取玩家古宝数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleCollectionData")
    void K2_GetRoleCollectionData(const FZGetRoleCollectionDataReq& InParams, const FZOnGetRoleCollectionDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleCollectionDataRsp>&)> OnGetRoleCollectionDataResult;
    void GetRoleCollectionData(const TSharedPtr<idlepb::GetRoleCollectionDataReq>& InReqMessage, const OnGetRoleCollectionDataResult& InCallback);    

    /**
     * 古宝操作
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RoleCollectionOp")
    void K2_RoleCollectionOp(const FZRoleCollectionOpReq& InParams, const FZOnRoleCollectionOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RoleCollectionOpAck>&)> OnRoleCollectionOpResult;
    void RoleCollectionOp(const TSharedPtr<idlepb::RoleCollectionOpReq>& InReqMessage, const OnRoleCollectionOpResult& InCallback);    

    /**
     * 分享自己的古宝
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ShareSelfRoleCollection")
    void K2_ShareSelfRoleCollection(const FZShareSelfRoleCollectionReq& InParams, const FZOnShareSelfRoleCollectionResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ShareSelfRoleCollectionRsp>&)> OnShareSelfRoleCollectionResult;
    void ShareSelfRoleCollection(const TSharedPtr<idlepb::ShareSelfRoleCollectionReq>& InReqMessage, const OnShareSelfRoleCollectionResult& InCallback);    

    /**
     * 获取分享古宝数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetShareRoleCollectionData")
    void K2_GetShareRoleCollectionData(const FZGetShareRoleCollectionDataReq& InParams, const FZOnGetShareRoleCollectionDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetShareRoleCollectionDataRsp>&)> OnGetShareRoleCollectionDataResult;
    void GetShareRoleCollectionData(const TSharedPtr<idlepb::GetShareRoleCollectionDataReq>& InReqMessage, const OnGetShareRoleCollectionDataResult& InCallback);    

    /**
     * 获取玩家福缘数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetChecklistData")
    void K2_GetChecklistData(const FZGetChecklistDataReq& InParams, const FZOnGetChecklistDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetChecklistDataAck>&)> OnGetChecklistDataResult;
    void GetChecklistData(const TSharedPtr<idlepb::GetChecklistDataReq>& InReqMessage, const OnGetChecklistDataResult& InCallback);    

    /**
     * 福缘功能操作
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ChecklistOp")
    void K2_ChecklistOp(const FZChecklistOpReq& InParams, const FZOnChecklistOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ChecklistOpAck>&)> OnChecklistOpResult;
    void ChecklistOp(const TSharedPtr<idlepb::ChecklistOpReq>& InReqMessage, const OnChecklistOpResult& InCallback);    

    /**
     * 福缘任务进度更新
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UpdateChecklist")
    void K2_UpdateChecklist(const FZUpdateChecklistReq& InParams, const FZOnUpdateChecklistResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UpdateChecklistAck>&)> OnUpdateChecklistResult;
    void UpdateChecklist(const TSharedPtr<idlepb::UpdateChecklistReq>& InReqMessage, const OnUpdateChecklistResult& InCallback);    

    /**
     * 请求论剑台状态
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSwordPkInfo")
    void K2_GetSwordPkInfo(const FZGetSwordPkInfoReq& InParams, const FZOnGetSwordPkInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSwordPkInfoRsp>&)> OnGetSwordPkInfoResult;
    void GetSwordPkInfo(const TSharedPtr<idlepb::GetSwordPkInfoReq>& InReqMessage, const OnGetSwordPkInfoResult& InCallback);    

    /**
     * 注册论剑台
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SwordPkSignup")
    void K2_SwordPkSignup(const FZSwordPkSignupReq& InParams, const FZOnSwordPkSignupResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SwordPkSignupRsp>&)> OnSwordPkSignupResult;
    void SwordPkSignup(const TSharedPtr<idlepb::SwordPkSignupReq>& InReqMessage, const OnSwordPkSignupResult& InCallback);    

    /**
     * 论剑台匹配
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SwordPkMatching")
    void K2_SwordPkMatching(const FZSwordPkMatchingReq& InParams, const FZOnSwordPkMatchingResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SwordPkMatchingRsp>&)> OnSwordPkMatchingResult;
    void SwordPkMatching(const TSharedPtr<idlepb::SwordPkMatchingReq>& InReqMessage, const OnSwordPkMatchingResult& InCallback);    

    /**
     * 论剑台挑战
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SwordPkChallenge")
    void K2_SwordPkChallenge(const FZSwordPkChallengeReq& InParams, const FZOnSwordPkChallengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SwordPkChallengeRsp>&)> OnSwordPkChallengeResult;
    void SwordPkChallenge(const TSharedPtr<idlepb::SwordPkChallengeReq>& InReqMessage, const OnSwordPkChallengeResult& InCallback);    

    /**
     * 论剑台复仇
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SwordPkRevenge")
    void K2_SwordPkRevenge(const FZSwordPkRevengeReq& InParams, const FZOnSwordPkRevengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SwordPkRevengeRsp>&)> OnSwordPkRevengeResult;
    void SwordPkRevenge(const TSharedPtr<idlepb::SwordPkRevengeReq>& InReqMessage, const OnSwordPkRevengeResult& InCallback);    

    /**
     * 获取论剑台排行榜
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSwordPkTopList")
    void K2_GetSwordPkTopList(const FZGetSwordPkTopListReq& InParams, const FZOnGetSwordPkTopListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSwordPkTopListRsp>&)> OnGetSwordPkTopListResult;
    void GetSwordPkTopList(const TSharedPtr<idlepb::GetSwordPkTopListReq>& InReqMessage, const OnGetSwordPkTopListResult& InCallback);    

    /**
     * 兑换英雄令
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SwordPkExchangeHeroCard")
    void K2_SwordPkExchangeHeroCard(const FZSwordPkExchangeHeroCardReq& InParams, const FZOnSwordPkExchangeHeroCardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SwordPkExchangeHeroCardRsp>&)> OnSwordPkExchangeHeroCardResult;
    void SwordPkExchangeHeroCard(const TSharedPtr<idlepb::SwordPkExchangeHeroCardReq>& InReqMessage, const OnSwordPkExchangeHeroCardResult& InCallback);    

    /**
     * 获取玩家通用道具兑换数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetCommonItemExchangeData")
    void K2_GetCommonItemExchangeData(const FZGetCommonItemExchangeDataReq& InParams, const FZOnGetCommonItemExchangeDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetCommonItemExchangeDataAck>&)> OnGetCommonItemExchangeDataResult;
    void GetCommonItemExchangeData(const TSharedPtr<idlepb::GetCommonItemExchangeDataReq>& InReqMessage, const OnGetCommonItemExchangeDataResult& InCallback);    

    /**
     * 请求兑换通用道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ExchangeCommonItem")
    void K2_ExchangeCommonItem(const FZExchangeCommonItemReq& InParams, const FZOnExchangeCommonItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ExchangeCommonItemAck>&)> OnExchangeCommonItemResult;
    void ExchangeCommonItem(const TSharedPtr<idlepb::ExchangeCommonItemReq>& InReqMessage, const OnExchangeCommonItemResult& InCallback);    

    /**
     * 请求合成通用道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SynthesisCommonItem")
    void K2_SynthesisCommonItem(const FZSynthesisCommonItemReq& InParams, const FZOnSynthesisCommonItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SynthesisCommonItemAck>&)> OnSynthesisCommonItemResult;
    void SynthesisCommonItem(const TSharedPtr<idlepb::SynthesisCommonItemReq>& InReqMessage, const OnSynthesisCommonItemResult& InCallback);    

    /**
     * 请求可加入宗门列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetCandidatesSeptList")
    void K2_GetCandidatesSeptList(const FZGetCandidatesSeptListReq& InParams, const FZOnGetCandidatesSeptListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetCandidatesSeptListAck>&)> OnGetCandidatesSeptListResult;
    void GetCandidatesSeptList(const TSharedPtr<idlepb::GetCandidatesSeptListReq>& InReqMessage, const OnGetCandidatesSeptListResult& InCallback);    

    /**
     * 搜索宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SearchSept")
    void K2_SearchSept(const FZSearchSeptReq& InParams, const FZOnSearchSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SearchSeptAck>&)> OnSearchSeptResult;
    void SearchSept(const TSharedPtr<idlepb::SearchSeptReq>& InReqMessage, const OnSearchSeptResult& InCallback);    

    /**
     * 获取指定宗门基本信息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptBaseInfo")
    void K2_GetSeptBaseInfo(const FZGetSeptBaseInfoReq& InParams, const FZOnGetSeptBaseInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptBaseInfoAck>&)> OnGetSeptBaseInfoResult;
    void GetSeptBaseInfo(const TSharedPtr<idlepb::GetSeptBaseInfoReq>& InReqMessage, const OnGetSeptBaseInfoResult& InCallback);    

    /**
     * 获取宗门成员列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptMemberList")
    void K2_GetSeptMemberList(const FZGetSeptMemberListReq& InParams, const FZOnGetSeptMemberListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptMemberListAck>&)> OnGetSeptMemberListResult;
    void GetSeptMemberList(const TSharedPtr<idlepb::GetSeptMemberListReq>& InReqMessage, const OnGetSeptMemberListResult& InCallback);    

    /**
     * 创建宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="CreateSept")
    void K2_CreateSept(const FZCreateSeptReq& InParams, const FZOnCreateSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::CreateSeptAck>&)> OnCreateSeptResult;
    void CreateSept(const TSharedPtr<idlepb::CreateSeptReq>& InReqMessage, const OnCreateSeptResult& InCallback);    

    /**
     * 解散宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DismissSept")
    void K2_DismissSept(const FZDismissSeptReq& InParams, const FZOnDismissSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DismissSeptAck>&)> OnDismissSeptResult;
    void DismissSept(const TSharedPtr<idlepb::DismissSeptReq>& InReqMessage, const OnDismissSeptResult& InCallback);    

    /**
     * 离开宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ExitSept")
    void K2_ExitSept(const FZExitSeptReq& InParams, const FZOnExitSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ExitSeptAck>&)> OnExitSeptResult;
    void ExitSept(const TSharedPtr<idlepb::ExitSeptReq>& InReqMessage, const OnExitSeptResult& InCallback);    

    /**
     * 申请加入宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ApplyJoinSept")
    void K2_ApplyJoinSept(const FZApplyJoinSeptReq& InParams, const FZOnApplyJoinSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ApplyJoinSeptAck>&)> OnApplyJoinSeptResult;
    void ApplyJoinSept(const TSharedPtr<idlepb::ApplyJoinSeptReq>& InReqMessage, const OnApplyJoinSeptResult& InCallback);    

    /**
     * 审批入宗请求
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ApproveApplySept")
    void K2_ApproveApplySept(const FZApproveApplySeptReq& InParams, const FZOnApproveApplySeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ApproveApplySeptAck>&)> OnApproveApplySeptResult;
    void ApproveApplySept(const TSharedPtr<idlepb::ApproveApplySeptReq>& InReqMessage, const OnApproveApplySeptResult& InCallback);    

    /**
     * 获取入宗申请列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetApplyJoinSeptList")
    void K2_GetApplyJoinSeptList(const FZGetApplyJoinSeptListReq& InParams, const FZOnGetApplyJoinSeptListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetApplyJoinSeptListAck>&)> OnGetApplyJoinSeptListResult;
    void GetApplyJoinSeptList(const TSharedPtr<idlepb::GetApplyJoinSeptListReq>& InReqMessage, const OnGetApplyJoinSeptListResult& InCallback);    

    /**
     * 回复入宗邀请
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RespondInviteSept")
    void K2_RespondInviteSept(const FZRespondInviteSeptReq& InParams, const FZOnRespondInviteSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RespondInviteSeptAck>&)> OnRespondInviteSeptResult;
    void RespondInviteSept(const TSharedPtr<idlepb::RespondInviteSeptReq>& InReqMessage, const OnRespondInviteSeptResult& InCallback);    

    /**
     * 获取邀请我入宗的宗门列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetInviteMeJoinSeptList")
    void K2_GetInviteMeJoinSeptList(const FZGetInviteMeJoinSeptListReq& InParams, const FZOnGetInviteMeJoinSeptListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetInviteMeJoinSeptListAck>&)> OnGetInviteMeJoinSeptListResult;
    void GetInviteMeJoinSeptList(const TSharedPtr<idlepb::GetInviteMeJoinSeptListReq>& InReqMessage, const OnGetInviteMeJoinSeptListResult& InCallback);    

    /**
     * 获取可邀请入宗玩家列表
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetCandidatesInviteRoleList")
    void K2_GetCandidatesInviteRoleList(const FZGetCandidatesInviteRoleListReq& InParams, const FZOnGetCandidatesInviteRoleListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetCandidatesInviteRoleListAck>&)> OnGetCandidatesInviteRoleListResult;
    void GetCandidatesInviteRoleList(const TSharedPtr<idlepb::GetCandidatesInviteRoleListReq>& InReqMessage, const OnGetCandidatesInviteRoleListResult& InCallback);    

    /**
     * 邀请加入宗门
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="InviteJoinSept")
    void K2_InviteJoinSept(const FZInviteJoinSeptReq& InParams, const FZOnInviteJoinSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::InviteJoinSeptAck>&)> OnInviteJoinSeptResult;
    void InviteJoinSept(const TSharedPtr<idlepb::InviteJoinSeptReq>& InReqMessage, const OnInviteJoinSeptResult& InCallback);    

    /**
     * 设置宗门设置
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetSeptSettings")
    void K2_SetSeptSettings(const FZSetSeptSettingsReq& InParams, const FZOnSetSeptSettingsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetSeptSettingsAck>&)> OnSetSeptSettingsResult;
    void SetSeptSettings(const TSharedPtr<idlepb::SetSeptSettingsReq>& InReqMessage, const OnSetSeptSettingsResult& InCallback);    

    /**
     * 设置宗门公告
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="SetSeptAnnounce")
    void K2_SetSeptAnnounce(const FZSetSeptAnnounceReq& InParams, const FZOnSetSeptAnnounceResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::SetSeptAnnounceAck>&)> OnSetSeptAnnounceResult;
    void SetSeptAnnounce(const TSharedPtr<idlepb::SetSeptAnnounceReq>& InReqMessage, const OnSetSeptAnnounceResult& InCallback);    

    /**
     * 宗门改名
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ChangeSeptName")
    void K2_ChangeSeptName(const FZChangeSeptNameReq& InParams, const FZOnChangeSeptNameResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ChangeSeptNameAck>&)> OnChangeSeptNameResult;
    void ChangeSeptName(const TSharedPtr<idlepb::ChangeSeptNameReq>& InReqMessage, const OnChangeSeptNameResult& InCallback);    

    /**
     * 请求宗门日志
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptLog")
    void K2_GetSeptLog(const FZGetSeptLogReq& InParams, const FZOnGetSeptLogResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptLogAck>&)> OnGetSeptLogResult;
    void GetSeptLog(const TSharedPtr<idlepb::GetSeptLogReq>& InReqMessage, const OnGetSeptLogResult& InCallback);    

    /**
     * 宗门建设
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ConstructSept")
    void K2_ConstructSept(const FZConstructSeptReq& InParams, const FZOnConstructSeptResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ConstructSeptAck>&)> OnConstructSeptResult;
    void ConstructSept(const TSharedPtr<idlepb::ConstructSeptReq>& InReqMessage, const OnConstructSeptResult& InCallback);    

    /**
     * 获取宗门建设记录
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetConstructSeptLog")
    void K2_GetConstructSeptLog(const FZGetConstructSeptLogReq& InParams, const FZOnGetConstructSeptLogResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetConstructSeptLogAck>&)> OnGetConstructSeptLogResult;
    void GetConstructSeptLog(const TSharedPtr<idlepb::GetConstructSeptLogReq>& InReqMessage, const OnGetConstructSeptLogResult& InCallback);    

    /**
     * 获取角色每日已邀请入宗次数
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptInvitedRoleDailyNum")
    void K2_GetSeptInvitedRoleDailyNum(const FZGetSeptInvitedRoleDailyNumReq& InParams, const FZOnGetSeptInvitedRoleDailyNumResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptInvitedRoleDailyNumAck>&)> OnGetSeptInvitedRoleDailyNumResult;
    void GetSeptInvitedRoleDailyNum(const TSharedPtr<idlepb::GetSeptInvitedRoleDailyNumReq>& InReqMessage, const OnGetSeptInvitedRoleDailyNumResult& InCallback);    

    /**
     * 任命职位
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppointSeptPosition")
    void K2_AppointSeptPosition(const FZAppointSeptPositionReq& InParams, const FZOnAppointSeptPositionResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppointSeptPositionAck>&)> OnAppointSeptPositionResult;
    void AppointSeptPosition(const TSharedPtr<idlepb::AppointSeptPositionReq>& InReqMessage, const OnAppointSeptPositionResult& InCallback);    

    /**
     * 转让宗主
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ResignSeptChairman")
    void K2_ResignSeptChairman(const FZResignSeptChairmanReq& InParams, const FZOnResignSeptChairmanResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ResignSeptChairmanAck>&)> OnResignSeptChairmanResult;
    void ResignSeptChairman(const TSharedPtr<idlepb::ResignSeptChairmanReq>& InReqMessage, const OnResignSeptChairmanResult& InCallback);    

    /**
     * 开除宗门成员
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="KickOutSeptMember")
    void K2_KickOutSeptMember(const FZKickOutSeptMemberReq& InParams, const FZOnKickOutSeptMemberResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::KickOutSeptMemberAck>&)> OnKickOutSeptMemberResult;
    void KickOutSeptMember(const TSharedPtr<idlepb::KickOutSeptMemberReq>& InReqMessage, const OnKickOutSeptMemberResult& InCallback);    

    /**
     * 请求玩家宗门商店数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleSeptShopData")
    void K2_GetRoleSeptShopData(const FZGetRoleSeptShopDataReq& InParams, const FZOnGetRoleSeptShopDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleSeptShopDataAck>&)> OnGetRoleSeptShopDataResult;
    void GetRoleSeptShopData(const TSharedPtr<idlepb::GetRoleSeptShopDataReq>& InReqMessage, const OnGetRoleSeptShopDataResult& InCallback);    

    /**
     * 请求兑换宗门商店道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="BuySeptShopItem")
    void K2_BuySeptShopItem(const FZBuySeptShopItemReq& InParams, const FZOnBuySeptShopItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::BuySeptShopItemAck>&)> OnBuySeptShopItemResult;
    void BuySeptShopItem(const TSharedPtr<idlepb::BuySeptShopItemReq>& InReqMessage, const OnBuySeptShopItemResult& InCallback);    

    /**
     * 请求玩家宗门事务数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleSeptQuestData")
    void K2_GetRoleSeptQuestData(const FZGetRoleSeptQuestDataReq& InParams, const FZOnGetRoleSeptQuestDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleSeptQuestDataAck>&)> OnGetRoleSeptQuestDataResult;
    void GetRoleSeptQuestData(const TSharedPtr<idlepb::GetRoleSeptQuestDataReq>& InReqMessage, const OnGetRoleSeptQuestDataResult& InCallback);    

    /**
     * 玩家宗门事务操作
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReqRoleSeptQuestOp")
    void K2_ReqRoleSeptQuestOp(const FZReqRoleSeptQuestOpReq& InParams, const FZOnReqRoleSeptQuestOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReqRoleSeptQuestOpAck>&)> OnReqRoleSeptQuestOpResult;
    void ReqRoleSeptQuestOp(const TSharedPtr<idlepb::ReqRoleSeptQuestOpReq>& InReqMessage, const OnReqRoleSeptQuestOpResult& InCallback);    

    /**
     * 玩家宗门事务手动刷新
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RefreshSeptQuest")
    void K2_RefreshSeptQuest(const FZRefreshSeptQuestReq& InParams, const FZOnRefreshSeptQuestResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RefreshSeptQuestAck>&)> OnRefreshSeptQuestResult;
    void RefreshSeptQuest(const TSharedPtr<idlepb::RefreshSeptQuestReq>& InReqMessage, const OnRefreshSeptQuestResult& InCallback);    

    /**
     * 玩家宗门事务升级
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReqSeptQuestRankUp")
    void K2_ReqSeptQuestRankUp(const FZReqSeptQuestRankUpReq& InParams, const FZOnReqSeptQuestRankUpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReqSeptQuestRankUpAck>&)> OnReqSeptQuestRankUpResult;
    void ReqSeptQuestRankUp(const TSharedPtr<idlepb::ReqSeptQuestRankUpReq>& InReqMessage, const OnReqSeptQuestRankUpResult& InCallback);    

    /**
     * 开始占据中立秘镜矿脉
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="BeginOccupySeptStone")
    void K2_BeginOccupySeptStone(const FZBeginOccupySeptStoneReq& InParams, const FZOnBeginOccupySeptStoneResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::BeginOccupySeptStoneAck>&)> OnBeginOccupySeptStoneResult;
    void BeginOccupySeptStone(const TSharedPtr<idlepb::BeginOccupySeptStoneReq>& InReqMessage, const OnBeginOccupySeptStoneResult& InCallback);    

    /**
     * 结束占领中立秘镜矿脉
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="EndOccupySeptStone")
    void K2_EndOccupySeptStone(const FZEndOccupySeptStoneReq& InParams, const FZOnEndOccupySeptStoneResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::EndOccupySeptStoneAck>&)> OnEndOccupySeptStoneResult;
    void EndOccupySeptStone(const TSharedPtr<idlepb::EndOccupySeptStoneReq>& InReqMessage, const OnEndOccupySeptStoneResult& InCallback);    

    /**
     * 占领宗门领地
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OccupySeptLand")
    void K2_OccupySeptLand(const FZOccupySeptLandReq& InParams, const FZOnOccupySeptLandResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OccupySeptLandAck>&)> OnOccupySeptLandResult;
    void OccupySeptLand(const TSharedPtr<idlepb::OccupySeptLandReq>& InReqMessage, const OnOccupySeptLandResult& InCallback);    

    /**
     * 获取功法数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetGongFaData")
    void K2_GetGongFaData(const FZGetGongFaDataReq& InParams, const FZOnGetGongFaDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetGongFaDataAck>&)> OnGetGongFaDataResult;
    void GetGongFaData(const TSharedPtr<idlepb::GetGongFaDataReq>& InReqMessage, const OnGetGongFaDataResult& InCallback);    

    /**
     * 功法操作：领悟 | 激活 | 升级
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GongFaOp")
    void K2_GongFaOp(const FZGongFaOpReq& InParams, const FZOnGongFaOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GongFaOpAck>&)> OnGongFaOpResult;
    void GongFaOp(const TSharedPtr<idlepb::GongFaOpReq>& InReqMessage, const OnGongFaOpResult& InCallback);    

    /**
     * 激活功法圆满效果
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ActivateGongFaMaxEffect")
    void K2_ActivateGongFaMaxEffect(const FZActivateGongFaMaxEffectReq& InParams, const FZOnActivateGongFaMaxEffectResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ActivateGongFaMaxEffectAck>&)> OnActivateGongFaMaxEffectResult;
    void ActivateGongFaMaxEffect(const TSharedPtr<idlepb::ActivateGongFaMaxEffectReq>& InReqMessage, const OnActivateGongFaMaxEffectResult& InCallback);    

    /**
     * 获取宗门领地伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptLandDamageTopList")
    void K2_GetSeptLandDamageTopList(const FZGetSeptLandDamageTopListReq& InParams, const FZOnGetSeptLandDamageTopListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptLandDamageTopListAck>&)> OnGetSeptLandDamageTopListResult;
    void GetSeptLandDamageTopList(const TSharedPtr<idlepb::GetSeptLandDamageTopListReq>& InReqMessage, const OnGetSeptLandDamageTopListResult& InCallback);    

    /**
     * 领取福赠奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReceiveFuZengRewards")
    void K2_ReceiveFuZengRewards(const FZReceiveFuZengRewardsReq& InParams, const FZOnReceiveFuZengRewardsResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReceiveFuZengRewardsAck>&)> OnReceiveFuZengRewardsResult;
    void ReceiveFuZengRewards(const TSharedPtr<idlepb::ReceiveFuZengRewardsReq>& InReqMessage, const OnReceiveFuZengRewardsResult& InCallback);    

    /**
     * 获取福赠数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleFuZengData")
    void K2_GetRoleFuZengData(const FZGetRoleFuZengDataReq& InParams, const FZOnGetRoleFuZengDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleFuZengDataAck>&)> OnGetRoleFuZengDataResult;
    void GetRoleFuZengData(const TSharedPtr<idlepb::GetRoleFuZengDataReq>& InReqMessage, const OnGetRoleFuZengDataResult& InCallback);    

    /**
     * 获取宝藏阁数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleTreasuryData")
    void K2_GetRoleTreasuryData(const FZGetRoleTreasuryDataReq& InParams, const FZOnGetRoleTreasuryDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleTreasuryDataAck>&)> OnGetRoleTreasuryDataResult;
    void GetRoleTreasuryData(const TSharedPtr<idlepb::GetRoleTreasuryDataReq>& InReqMessage, const OnGetRoleTreasuryDataResult& InCallback);    

    /**
     * 请求开箱
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OpenTreasuryChest")
    void K2_OpenTreasuryChest(const FZOpenTreasuryChestReq& InParams, const FZOnOpenTreasuryChestResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OpenTreasuryChestAck>&)> OnOpenTreasuryChestResult;
    void OpenTreasuryChest(const TSharedPtr<idlepb::OpenTreasuryChestReq>& InReqMessage, const OnOpenTreasuryChestResult& InCallback);    

    /**
     * 请求一键全开箱
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OneClickOpenTreasuryChest")
    void K2_OneClickOpenTreasuryChest(const FZOneClickOpenTreasuryChestReq& InParams, const FZOnOneClickOpenTreasuryChestResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OneClickOpenTreasuryChestAck>&)> OnOneClickOpenTreasuryChestResult;
    void OneClickOpenTreasuryChest(const TSharedPtr<idlepb::OneClickOpenTreasuryChestReq>& InReqMessage, const OnOneClickOpenTreasuryChestResult& InCallback);    

    /**
     * 请求探索卡池
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="OpenTreasuryGacha")
    void K2_OpenTreasuryGacha(const FZOpenTreasuryGachaReq& InParams, const FZOnOpenTreasuryGachaResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::OpenTreasuryGachaAck>&)> OnOpenTreasuryGachaResult;
    void OpenTreasuryGacha(const TSharedPtr<idlepb::OpenTreasuryGachaReq>& InReqMessage, const OnOpenTreasuryGachaResult& InCallback);    

    /**
     * 请求刷新古修商店
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RefreshTreasuryShop")
    void K2_RefreshTreasuryShop(const FZRefreshTreasuryShopReq& InParams, const FZOnRefreshTreasuryShopResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RefreshTreasuryShopAck>&)> OnRefreshTreasuryShopResult;
    void RefreshTreasuryShop(const TSharedPtr<idlepb::RefreshTreasuryShopReq>& InReqMessage, const OnRefreshTreasuryShopResult& InCallback);    

    /**
     * 请求古修商店中购买
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="TreasuryShopBuy")
    void K2_TreasuryShopBuy(const FZTreasuryShopBuyReq& InParams, const FZOnTreasuryShopBuyResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::TreasuryShopBuyAck>&)> OnTreasuryShopBuyResult;
    void TreasuryShopBuy(const TSharedPtr<idlepb::TreasuryShopBuyReq>& InReqMessage, const OnTreasuryShopBuyResult& InCallback);    

    /**
     * 获取生涯计数器数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetLifeCounterData")
    void K2_GetLifeCounterData(const FZGetLifeCounterDataReq& InParams, const FZOnGetLifeCounterDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetLifeCounterDataAck>&)> OnGetLifeCounterDataResult;
    void GetLifeCounterData(const TSharedPtr<idlepb::GetLifeCounterDataReq>& InReqMessage, const OnGetLifeCounterDataResult& InCallback);    

    /**
     * 进行任务对战
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DoQuestFight")
    void K2_DoQuestFight(const FZDoQuestFightReq& InParams, const FZOnDoQuestFightResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DoQuestFightAck>&)> OnDoQuestFightResult;
    void DoQuestFight(const TSharedPtr<idlepb::DoQuestFightReq>& InReqMessage, const OnDoQuestFightResult& InCallback);    

    /**
     * 结束任务对战
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="QuestFightQuickEnd")
    void K2_QuestFightQuickEnd(const FZQuestFightQuickEndReq& InParams, const FZOnQuestFightQuickEndResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::QuestFightQuickEndAck>&)> OnQuestFightQuickEndResult;
    void QuestFightQuickEnd(const TSharedPtr<idlepb::QuestFightQuickEndReq>& InReqMessage, const OnQuestFightQuickEndResult& InCallback);    

    /**
     * 请求外观数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetAppearanceData")
    void K2_GetAppearanceData(const FZGetAppearanceDataReq& InParams, const FZOnGetAppearanceDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetAppearanceDataAck>&)> OnGetAppearanceDataResult;
    void GetAppearanceData(const TSharedPtr<idlepb::GetAppearanceDataReq>& InReqMessage, const OnGetAppearanceDataResult& InCallback);    

    /**
     * 请求添加外观（使用包含外观的礼包道具）
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppearanceAdd")
    void K2_AppearanceAdd(const FZAppearanceAddReq& InParams, const FZOnAppearanceAddResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppearanceAddAck>&)> OnAppearanceAddResult;
    void AppearanceAdd(const TSharedPtr<idlepb::AppearanceAddReq>& InReqMessage, const OnAppearanceAddResult& InCallback);    

    /**
     * 请求激活外观
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppearanceActive")
    void K2_AppearanceActive(const FZAppearanceActiveReq& InParams, const FZOnAppearanceActiveResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppearanceActiveAck>&)> OnAppearanceActiveResult;
    void AppearanceActive(const TSharedPtr<idlepb::AppearanceActiveReq>& InReqMessage, const OnAppearanceActiveResult& InCallback);    

    /**
     * 请求穿戴外观
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppearanceWear")
    void K2_AppearanceWear(const FZAppearanceWearReq& InParams, const FZOnAppearanceWearResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppearanceWearAck>&)> OnAppearanceWearResult;
    void AppearanceWear(const TSharedPtr<idlepb::AppearanceWearReq>& InReqMessage, const OnAppearanceWearResult& InCallback);    

    /**
     * 请求外观商店购买
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppearanceBuy")
    void K2_AppearanceBuy(const FZAppearanceBuyReq& InParams, const FZOnAppearanceBuyResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppearanceBuyAck>&)> OnAppearanceBuyResult;
    void AppearanceBuy(const TSharedPtr<idlepb::AppearanceBuyReq>& InReqMessage, const OnAppearanceBuyResult& InCallback);    

    /**
     * 请求修改外形
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AppearanceChangeSkType")
    void K2_AppearanceChangeSkType(const FZAppearanceChangeSkTypeReq& InParams, const FZOnAppearanceChangeSkTypeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AppearanceChangeSkTypeAck>&)> OnAppearanceChangeSkTypeResult;
    void AppearanceChangeSkType(const TSharedPtr<idlepb::AppearanceChangeSkTypeReq>& InReqMessage, const OnAppearanceChangeSkTypeResult& InCallback);    

    /**
     * 请求指定战斗信息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetBattleHistoryInfo")
    void K2_GetBattleHistoryInfo(const FZGetBattleHistoryInfoReq& InParams, const FZOnGetBattleHistoryInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetBattleHistoryInfoAck>&)> OnGetBattleHistoryInfoResult;
    void GetBattleHistoryInfo(const TSharedPtr<idlepb::GetBattleHistoryInfoReq>& InReqMessage, const OnGetBattleHistoryInfoResult& InCallback);    

    /**
     * 请求秘境探索数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetArenaCheckListData")
    void K2_GetArenaCheckListData(const FZGetArenaCheckListDataReq& InParams, const FZOnGetArenaCheckListDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetArenaCheckListDataAck>&)> OnGetArenaCheckListDataResult;
    void GetArenaCheckListData(const TSharedPtr<idlepb::GetArenaCheckListDataReq>& InReqMessage, const OnGetArenaCheckListDataResult& InCallback);    

    /**
     * 请求提交秘境探索事件
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ArenaCheckListSubmit")
    void K2_ArenaCheckListSubmit(const FZArenaCheckListSubmitReq& InParams, const FZOnArenaCheckListSubmitResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ArenaCheckListSubmitAck>&)> OnArenaCheckListSubmitResult;
    void ArenaCheckListSubmit(const TSharedPtr<idlepb::ArenaCheckListSubmitReq>& InReqMessage, const OnArenaCheckListSubmitResult& InCallback);    

    /**
     * 请求提交秘境探索奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ArenaCheckListRewardSubmit")
    void K2_ArenaCheckListRewardSubmit(const FZArenaCheckListRewardSubmitReq& InParams, const FZOnArenaCheckListRewardSubmitResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ArenaCheckListRewardSubmitAck>&)> OnArenaCheckListRewardSubmitResult;
    void ArenaCheckListRewardSubmit(const TSharedPtr<idlepb::ArenaCheckListRewardSubmitReq>& InReqMessage, const OnArenaCheckListRewardSubmitResult& InCallback);    

    /**
     * 请求开启剿灭副本
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonKillAllChallenge")
    void K2_DungeonKillAllChallenge(const FZDungeonKillAllChallengeReq& InParams, const FZOnDungeonKillAllChallengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonKillAllChallengeAck>&)> OnDungeonKillAllChallengeResult;
    void DungeonKillAllChallenge(const TSharedPtr<idlepb::DungeonKillAllChallengeReq>& InReqMessage, const OnDungeonKillAllChallengeResult& InCallback);    

    /**
     * 请求剿灭副本快速结束
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonKillAllQuickEnd")
    void K2_DungeonKillAllQuickEnd(const FZDungeonKillAllQuickEndReq& InParams, const FZOnDungeonKillAllQuickEndResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonKillAllQuickEndAck>&)> OnDungeonKillAllQuickEndResult;
    void DungeonKillAllQuickEnd(const TSharedPtr<idlepb::DungeonKillAllQuickEndReq>& InReqMessage, const OnDungeonKillAllQuickEndResult& InCallback);    

    /**
     * 询问剿灭副本是否完成
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonKillAllData")
    void K2_DungeonKillAllData(const FZDungeonKillAllDataReq& InParams, const FZOnDungeonKillAllDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonKillAllDataAck>&)> OnDungeonKillAllDataResult;
    void DungeonKillAllData(const TSharedPtr<idlepb::DungeonKillAllDataReq>& InReqMessage, const OnDungeonKillAllDataResult& InCallback);    

    /**
     * 药园数据请求
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetFarmlandData")
    void K2_GetFarmlandData(const FZGetFarmlandDataReq& InParams, const FZOnGetFarmlandDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetFarmlandDataAck>&)> OnGetFarmlandDataResult;
    void GetFarmlandData(const TSharedPtr<idlepb::GetFarmlandDataReq>& InReqMessage, const OnGetFarmlandDataResult& InCallback);    

    /**
     * 药园地块解锁
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandUnlockBlock")
    void K2_FarmlandUnlockBlock(const FZFarmlandUnlockBlockReq& InParams, const FZOnFarmlandUnlockBlockResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandUnlockBlockAck>&)> OnFarmlandUnlockBlockResult;
    void FarmlandUnlockBlock(const TSharedPtr<idlepb::FarmlandUnlockBlockReq>& InReqMessage, const OnFarmlandUnlockBlockResult& InCallback);    

    /**
     * 药园种植或铲除
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandPlantSeed")
    void K2_FarmlandPlantSeed(const FZFarmlandPlantSeedReq& InParams, const FZOnFarmlandPlantSeedResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandPlantSeedAck>&)> OnFarmlandPlantSeedResult;
    void FarmlandPlantSeed(const TSharedPtr<idlepb::FarmlandPlantSeedReq>& InReqMessage, const OnFarmlandPlantSeedResult& InCallback);    

    /**
     * 药园浇灌
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandWatering")
    void K2_FarmlandWatering(const FZFarmlandWateringReq& InParams, const FZOnFarmlandWateringResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandWateringAck>&)> OnFarmlandWateringResult;
    void FarmlandWatering(const TSharedPtr<idlepb::FarmlandWateringReq>& InReqMessage, const OnFarmlandWateringResult& InCallback);    

    /**
     * 药园催熟
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandRipening")
    void K2_FarmlandRipening(const FZFarmlandRipeningReq& InParams, const FZOnFarmlandRipeningResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandRipeningAck>&)> OnFarmlandRipeningResult;
    void FarmlandRipening(const TSharedPtr<idlepb::FarmlandRipeningReq>& InReqMessage, const OnFarmlandRipeningResult& InCallback);    

    /**
     * 药园收获
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandHarvest")
    void K2_FarmlandHarvest(const FZFarmlandHarvestReq& InParams, const FZOnFarmlandHarvestResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandHarvestAck>&)> OnFarmlandHarvestResult;
    void FarmlandHarvest(const TSharedPtr<idlepb::FarmlandHarvestReq>& InReqMessage, const OnFarmlandHarvestResult& InCallback);    

    /**
     * 药园药童升级
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmerRankUp")
    void K2_FarmerRankUp(const FZFarmerRankUpReq& InParams, const FZOnFarmerRankUpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmerRankUpAck>&)> OnFarmerRankUpResult;
    void FarmerRankUp(const TSharedPtr<idlepb::FarmerRankUpReq>& InReqMessage, const OnFarmerRankUpResult& InCallback);    

    /**
     * 药园打理
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FarmlandSetManagement")
    void K2_FarmlandSetManagement(const FZFarmlandSetManagementReq& InParams, const FZOnFarmlandSetManagementResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FarmlandSetManagementAck>&)> OnFarmlandSetManagementResult;
    void FarmlandSetManagement(const TSharedPtr<idlepb::FarmlandSetManagementReq>& InReqMessage, const OnFarmlandSetManagementResult& InCallback);    

    /**
     * 获取药园状态，自动收获
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="UpdateFarmlandState")
    void K2_UpdateFarmlandState(const FZUpdateFarmlandStateReq& InParams, const FZOnUpdateFarmlandStateResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::UpdateFarmlandStateAck>&)> OnUpdateFarmlandStateResult;
    void UpdateFarmlandState(const TSharedPtr<idlepb::UpdateFarmlandStateReq>& InReqMessage, const OnUpdateFarmlandStateResult& InCallback);    

    /**
     * 请求开启生存副本
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonSurviveChallenge")
    void K2_DungeonSurviveChallenge(const FZDungeonSurviveChallengeReq& InParams, const FZOnDungeonSurviveChallengeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonSurviveChallengeAck>&)> OnDungeonSurviveChallengeResult;
    void DungeonSurviveChallenge(const TSharedPtr<idlepb::DungeonSurviveChallengeReq>& InReqMessage, const OnDungeonSurviveChallengeResult& InCallback);    

    /**
     * 请求生存副本快速结束
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonSurviveQuickEnd")
    void K2_DungeonSurviveQuickEnd(const FZDungeonSurviveQuickEndReq& InParams, const FZOnDungeonSurviveQuickEndResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonSurviveQuickEndAck>&)> OnDungeonSurviveQuickEndResult;
    void DungeonSurviveQuickEnd(const TSharedPtr<idlepb::DungeonSurviveQuickEndReq>& InReqMessage, const OnDungeonSurviveQuickEndResult& InCallback);    

    /**
     * 询问生存副本是否完成
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DungeonSurviveData")
    void K2_DungeonSurviveData(const FZDungeonSurviveDataReq& InParams, const FZOnDungeonSurviveDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DungeonSurviveDataAck>&)> OnDungeonSurviveDataResult;
    void DungeonSurviveData(const TSharedPtr<idlepb::DungeonSurviveDataReq>& InReqMessage, const OnDungeonSurviveDataResult& InCallback);    

    /**
     * 神通一键重置CD请求
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRevertAllSkillCoolDown")
    void K2_GetRevertAllSkillCoolDown(const FZGetRevertAllSkillCoolDownReq& InParams, const FZOnGetRevertAllSkillCoolDownResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRevertAllSkillCoolDownAck>&)> OnGetRevertAllSkillCoolDownResult;
    void GetRevertAllSkillCoolDown(const TSharedPtr<idlepb::GetRevertAllSkillCoolDownReq>& InReqMessage, const OnGetRevertAllSkillCoolDownResult& InCallback);    

    /**
     * 获取道友功能数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleFriendData")
    void K2_GetRoleFriendData(const FZGetRoleFriendDataReq& InParams, const FZOnGetRoleFriendDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleFriendDataAck>&)> OnGetRoleFriendDataResult;
    void GetRoleFriendData(const TSharedPtr<idlepb::GetRoleFriendDataReq>& InReqMessage, const OnGetRoleFriendDataResult& InCallback);    

    /**
     * 发起 好友申请/或移除好友 拉黑/或移除拉黑 成为道侣或解除道侣
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FriendOp")
    void K2_FriendOp(const FZFriendOpReq& InParams, const FZOnFriendOpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FriendOpAck>&)> OnFriendOpResult;
    void FriendOp(const TSharedPtr<idlepb::FriendOpReq>& InReqMessage, const OnFriendOpResult& InCallback);    

    /**
     * 处理好友申请
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReplyFriendRequest")
    void K2_ReplyFriendRequest(const FZReplyFriendRequestReq& InParams, const FZOnReplyFriendRequestResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReplyFriendRequestAck>&)> OnReplyFriendRequestResult;
    void ReplyFriendRequest(const TSharedPtr<idlepb::ReplyFriendRequestReq>& InReqMessage, const OnReplyFriendRequestResult& InCallback);    

    /**
     * 查找玩家（道友功能）
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="FriendSearchRoleInfo")
    void K2_FriendSearchRoleInfo(const FZFriendSearchRoleInfoReq& InParams, const FZOnFriendSearchRoleInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::FriendSearchRoleInfoAck>&)> OnFriendSearchRoleInfoResult;
    void FriendSearchRoleInfo(const TSharedPtr<idlepb::FriendSearchRoleInfoReq>& InReqMessage, const OnFriendSearchRoleInfoResult& InCallback);    

    /**
     * 请求玩家信息缓存(Todo 用于聊天查找，可能需要整合)
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleInfoCache")
    void K2_GetRoleInfoCache(const FZGetRoleInfoCacheReq& InParams, const FZOnGetRoleInfoCacheResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleInfoCacheAck>&)> OnGetRoleInfoCacheResult;
    void GetRoleInfoCache(const TSharedPtr<idlepb::GetRoleInfoCacheReq>& InReqMessage, const OnGetRoleInfoCacheResult& InCallback);    

    /**
     * 请求玩家个人信息(Todo 老接口，可能需要整合)
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleInfo")
    void K2_GetRoleInfo(const FZGetRoleInfoReq& InParams, const FZOnGetRoleInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleInfoAck>&)> OnGetRoleInfoResult;
    void GetRoleInfo(const TSharedPtr<idlepb::GetRoleInfoReq>& InReqMessage, const OnGetRoleInfoResult& InCallback);    

    /**
     * 获取化身数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleAvatarData")
    void K2_GetRoleAvatarData(const FZGetRoleAvatarDataReq& InParams, const FZOnGetRoleAvatarDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleAvatarDataAck>&)> OnGetRoleAvatarDataResult;
    void GetRoleAvatarData(const TSharedPtr<idlepb::GetRoleAvatarDataReq>& InReqMessage, const OnGetRoleAvatarDataResult& InCallback);    

    /**
     * 派遣化身
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="DispatchAvatar")
    void K2_DispatchAvatar(const FZDispatchAvatarReq& InParams, const FZOnDispatchAvatarResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::DispatchAvatarAck>&)> OnDispatchAvatarResult;
    void DispatchAvatar(const TSharedPtr<idlepb::DispatchAvatarReq>& InReqMessage, const OnDispatchAvatarResult& InCallback);    

    /**
     * 化身升级
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AvatarRankUp")
    void K2_AvatarRankUp(const FZAvatarRankUpReq& InParams, const FZOnAvatarRankUpResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AvatarRankUpAck>&)> OnAvatarRankUpResult;
    void AvatarRankUp(const TSharedPtr<idlepb::AvatarRankUpReq>& InReqMessage, const OnAvatarRankUpResult& InCallback);    

    /**
     * 收获化身包裹道具
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReceiveAvatarTempPackage")
    void K2_ReceiveAvatarTempPackage(const FZReceiveAvatarTempPackageReq& InParams, const FZOnReceiveAvatarTempPackageResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReceiveAvatarTempPackageAck>&)> OnReceiveAvatarTempPackageResult;
    void ReceiveAvatarTempPackage(const TSharedPtr<idlepb::ReceiveAvatarTempPackageReq>& InReqMessage, const OnReceiveAvatarTempPackageResult& InCallback);    

    /**
     * 获取秘境探索统计数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetArenaExplorationStatisticalData")
    void K2_GetArenaExplorationStatisticalData(const FZGetArenaExplorationStatisticalDataReq& InParams, const FZOnGetArenaExplorationStatisticalDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetArenaExplorationStatisticalDataAck>&)> OnGetArenaExplorationStatisticalDataResult;
    void GetArenaExplorationStatisticalData(const TSharedPtr<idlepb::GetArenaExplorationStatisticalDataReq>& InReqMessage, const OnGetArenaExplorationStatisticalDataResult& InCallback);    

    /**
     * 获取角色传记数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleBiographyData")
    void K2_GetRoleBiographyData(const FZGetRoleBiographyDataReq& InParams, const FZOnGetRoleBiographyDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleBiographyDataAck>&)> OnGetRoleBiographyDataResult;
    void GetRoleBiographyData(const TSharedPtr<idlepb::GetRoleBiographyDataReq>& InReqMessage, const OnGetRoleBiographyDataResult& InCallback);    

    /**
     * 请求领取传记奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReceiveBiographyItem")
    void K2_ReceiveBiographyItem(const FZReceiveBiographyItemReq& InParams, const FZOnReceiveBiographyItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReceiveBiographyItemAck>&)> OnReceiveBiographyItemResult;
    void ReceiveBiographyItem(const TSharedPtr<idlepb::ReceiveBiographyItemReq>& InReqMessage, const OnReceiveBiographyItemResult& InCallback);    

    /**
     * 请求领取史记数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetBiographyEventData")
    void K2_GetBiographyEventData(const FZGetBiographyEventDataReq& InParams, const FZOnGetBiographyEventDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetBiographyEventDataAck>&)> OnGetBiographyEventDataResult;
    void GetBiographyEventData(const TSharedPtr<idlepb::GetBiographyEventDataReq>& InReqMessage, const OnGetBiographyEventDataResult& InCallback);    

    /**
     * 请求领取史记奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="ReceiveBiographyEventItem")
    void K2_ReceiveBiographyEventItem(const FZReceiveBiographyEventItemReq& InParams, const FZOnReceiveBiographyEventItemResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::ReceiveBiographyEventItemAck>&)> OnReceiveBiographyEventItemResult;
    void ReceiveBiographyEventItem(const TSharedPtr<idlepb::ReceiveBiographyEventItemReq>& InReqMessage, const OnReceiveBiographyEventItemResult& InCallback);    

    /**
     * 请求上传纪念日志
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="AddBiographyRoleLog")
    void K2_AddBiographyRoleLog(const FZAddBiographyRoleLogReq& InParams, const FZOnAddBiographyRoleLogResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::AddBiographyRoleLogAck>&)> OnAddBiographyRoleLogResult;
    void AddBiographyRoleLog(const TSharedPtr<idlepb::AddBiographyRoleLogReq>& InReqMessage, const OnAddBiographyRoleLogResult& InCallback);    

    /**
     * 请求进入镇魔深渊
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestEnterSeptDemonWorld")
    void K2_RequestEnterSeptDemonWorld(const FZRequestEnterSeptDemonWorldReq& InParams, const FZOnRequestEnterSeptDemonWorldResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestEnterSeptDemonWorldAck>&)> OnRequestEnterSeptDemonWorldResult;
    void RequestEnterSeptDemonWorld(const TSharedPtr<idlepb::RequestEnterSeptDemonWorldReq>& InReqMessage, const OnRequestEnterSeptDemonWorldResult& InCallback);    

    /**
     * 请求退出镇魔深渊
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestLeaveSeptDemonWorld")
    void K2_RequestLeaveSeptDemonWorld(const FZRequestLeaveSeptDemonWorldReq& InParams, const FZOnRequestLeaveSeptDemonWorldResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestLeaveSeptDemonWorldAck>&)> OnRequestLeaveSeptDemonWorldResult;
    void RequestLeaveSeptDemonWorld(const TSharedPtr<idlepb::RequestLeaveSeptDemonWorldReq>& InReqMessage, const OnRequestLeaveSeptDemonWorldResult& InCallback);    

    /**
     * 请求镇魔深渊相关数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestSeptDemonWorldData")
    void K2_RequestSeptDemonWorldData(const FZRequestSeptDemonWorldDataReq& InParams, const FZOnRequestSeptDemonWorldDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestSeptDemonWorldDataAck>&)> OnRequestSeptDemonWorldDataResult;
    void RequestSeptDemonWorldData(const TSharedPtr<idlepb::RequestSeptDemonWorldDataReq>& InReqMessage, const OnRequestSeptDemonWorldDataResult& InCallback);    

    /**
     * 请求在镇魔深渊待的最后时间点
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="RequestInSeptDemonWorldEndTime")
    void K2_RequestInSeptDemonWorldEndTime(const FZRequestInSeptDemonWorldEndTimeReq& InParams, const FZOnRequestInSeptDemonWorldEndTimeResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::RequestInSeptDemonWorldEndTimeAck>&)> OnRequestInSeptDemonWorldEndTimeResult;
    void RequestInSeptDemonWorldEndTime(const TSharedPtr<idlepb::RequestInSeptDemonWorldEndTimeReq>& InReqMessage, const OnRequestInSeptDemonWorldEndTimeResult& InCallback);    

    /**
     * 请求镇魔深渊待伤害排行榜
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonDamageTopList")
    void K2_GetSeptDemonDamageTopList(const FZGetSeptDemonDamageTopListReq& InParams, const FZOnGetSeptDemonDamageTopListResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageTopListAck>&)> OnGetSeptDemonDamageTopListResult;
    void GetSeptDemonDamageTopList(const TSharedPtr<idlepb::GetSeptDemonDamageTopListReq>& InReqMessage, const OnGetSeptDemonDamageTopListResult& InCallback);    

    /**
     * 请求镇魔深渊待玩家伤害预览信息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonDamageSelfSummary")
    void K2_GetSeptDemonDamageSelfSummary(const FZGetSeptDemonDamageSelfSummaryReq& InParams, const FZOnGetSeptDemonDamageSelfSummaryResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageSelfSummaryAck>&)> OnGetSeptDemonDamageSelfSummaryResult;
    void GetSeptDemonDamageSelfSummary(const TSharedPtr<idlepb::GetSeptDemonDamageSelfSummaryReq>& InReqMessage, const OnGetSeptDemonDamageSelfSummaryResult& InCallback);    

    /**
     * 请求镇魔深渊待宝库奖励剩余抽奖次数
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonStageRewardNum")
    void K2_GetSeptDemonStageRewardNum(const FZGetSeptDemonStageRewardNumReq& InParams, const FZOnGetSeptDemonStageRewardNumResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonStageRewardNumAck>&)> OnGetSeptDemonStageRewardNumResult;
    void GetSeptDemonStageRewardNum(const TSharedPtr<idlepb::GetSeptDemonStageRewardNumReq>& InReqMessage, const OnGetSeptDemonStageRewardNumResult& InCallback);    

    /**
     * 请求镇魔深渊待宝库奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonStageReward")
    void K2_GetSeptDemonStageReward(const FZGetSeptDemonStageRewardReq& InParams, const FZOnGetSeptDemonStageRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonStageRewardAck>&)> OnGetSeptDemonStageRewardResult;
    void GetSeptDemonStageReward(const TSharedPtr<idlepb::GetSeptDemonStageRewardReq>& InReqMessage, const OnGetSeptDemonStageRewardResult& InCallback);    

    /**
     * 请求镇魔深渊挑战奖励列表信息
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonDamageRewardsInfo")
    void K2_GetSeptDemonDamageRewardsInfo(const FZGetSeptDemonDamageRewardsInfoReq& InParams, const FZOnGetSeptDemonDamageRewardsInfoResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageRewardsInfoAck>&)> OnGetSeptDemonDamageRewardsInfoResult;
    void GetSeptDemonDamageRewardsInfo(const TSharedPtr<idlepb::GetSeptDemonDamageRewardsInfoReq>& InReqMessage, const OnGetSeptDemonDamageRewardsInfoResult& InCallback);    

    /**
     * 请求镇魔深渊待挑战奖励
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetSeptDemonDamageReward")
    void K2_GetSeptDemonDamageReward(const FZGetSeptDemonDamageRewardReq& InParams, const FZOnGetSeptDemonDamageRewardResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageRewardAck>&)> OnGetSeptDemonDamageRewardResult;
    void GetSeptDemonDamageReward(const TSharedPtr<idlepb::GetSeptDemonDamageRewardReq>& InReqMessage, const OnGetSeptDemonDamageRewardResult& InCallback);    

    /**
     * 请求仙阁商店数据
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="GetRoleVipShopData")
    void K2_GetRoleVipShopData(const FZGetRoleVipShopDataReq& InParams, const FZOnGetRoleVipShopDataResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::GetRoleVipShopDataAck>&)> OnGetRoleVipShopDataResult;
    void GetRoleVipShopData(const TSharedPtr<idlepb::GetRoleVipShopDataReq>& InReqMessage, const OnGetRoleVipShopDataResult& InCallback);    

    /**
     * 请求仙阁商店购买
    */
    UFUNCTION(BlueprintCallable, Category="MRpc", DisplayName="VipShopBuy")
    void K2_VipShopBuy(const FZVipShopBuyReq& InParams, const FZOnVipShopBuyResult& InCallback);
    
    typedef TFunction<void(EPbRpcErrorCode, const TSharedPtr<idlepb::VipShopBuyAck>&)> OnVipShopBuyResult;
    void VipShopBuy(const TSharedPtr<idlepb::VipShopBuyReq>& InReqMessage, const OnVipShopBuyResult& InCallback);

    
    /**
     * 通知炼丹单次结果
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyAlchemyRefineResultResult OnNotifyAlchemyRefineResult;
    
    /**
     * 刷新道具数据
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnRefreshItemsResult OnRefreshItems;
    
    /**
     * 更新背包空间
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyInventorySpaceNumResult OnNotifyInventorySpaceNum;
    
    /**
     * 更新已解锁装备槽位列表
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnRefreshUnlockedEquipmentSlotsResult OnRefreshUnlockedEquipmentSlots;
    
    /**
     * 通知解锁挑战结果
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyUnlockArenaChallengeResultResult OnNotifyUnlockArenaChallengeResult;
    
    /**
     * 更新邮箱
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnUpdateRoleMailResult OnUpdateRoleMail;
    
    /**
     * 通知炼器单次结果
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyForgeRefineResultResult OnNotifyForgeRefineResult;
    
    /**
     * 礼包结果通知
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyGiftPackageResultResult OnNotifyGiftPackageResult;
    
    /**
     * 使用属性丹药通知
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyUsePillPropertyResult OnNotifyUsePillProperty;
    
    /**
     * 通知背包已经满，道具经邮件发送
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyInventoryFullMailItemResult OnNotifyInventoryFullMailItem;
    
    /**
     * 通知古宝数据刷新
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyRoleCollectionDataResult OnNotifyRoleCollectionData;
    
    /**
     * 通知古宝通用碎片刷新
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyCommonCollectionPieceDataResult OnNotifyCommonCollectionPieceData;
    
    /**
     * 通知古宝更新已经激活套装
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyCollectionActivatedSuitResult OnNotifyCollectionActivatedSuit;
    
    /**
     * 通知古宝渊源更新
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyRoleCollectionHistoriesResult OnNotifyRoleCollectionHistories;
    
    /**
     * 通知更新古宝累计收集奖励领取情况
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyCollectionZoneActiveAwardsResult OnNotifyCollectionZoneActiveAwards;
    
    /**
     * 通知下次可重置强化的时间
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyRoleCollectionNextResetEnhanceTicksResult OnNotifyRoleCollectionNextResetEnhanceTicks;
    
    /**
     * 入侵BOSS被击杀
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyBossInvasionNpcKilledResult OnNotifyBossInvasionNpcKilled;
    
    /**
     * 福缘任务通知
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyChecklistResult OnNotifyChecklist;
    
    /**
     * 通知玩家中立秘境矿脉采集结束
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifySeptStoneOccupyEndResult OnNotifySeptStoneOccupyEnd;
    
    /**
     * 通知传送失败
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyTeleportFailedResult OnNotifyTeleportFailed;
    
    /**
     * 福赠完成通知
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyFuZengResult OnNotifyFuZeng;
    
    /**
     * 通知计数器更新
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnUpdateLifeCounterResult OnUpdateLifeCounter;
    
    /**
     * 通知任务对战挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyQuestFightChallengeOverResult OnNotifyQuestFightChallengeOver;
    
    /**
     * 副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnDungeonChallengeOverResult OnDungeonChallengeOver;
    
    /**
     * 切磋结果数据
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifySoloArenaChallengeOverResult OnNotifySoloArenaChallengeOver;
    
    /**
     * 通知聊天消息
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnUpdateChatResult OnUpdateChat;
    
    /**
     * 询问剿灭副本是否完成
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyDungeonKillAllChallengeCurWaveNumResult OnNotifyDungeonKillAllChallengeCurWaveNum;
    
    /**
     * 剿灭副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyDungeonKillAllChallengeOverResult OnNotifyDungeonKillAllChallengeOver;
    
    /**
     * 通知药园功能
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyFarmlandMessageResult OnNotifyFarmlandMessage;
    
    /**
     * 询问生存副本是否完成
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyDungeonSurviveChallengeCurWaveNumResult OnNotifyDungeonSurviveChallengeCurWaveNum;
    
    /**
     * 生存副本挑战结束
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyDungeonSurviveChallengeOverResult OnNotifyDungeonSurviveChallengeOver;
    
    /**
     * 通知道友功能消息
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyFriendMessageResult OnNotifyFriendMessage;
    
    /**
     * 传记功能通知（包括史记或纪念）
    */
    UPROPERTY(BlueprintAssignable, Category="MRpc") 
    FZOnNotifyBiographyMessageResult OnNotifyBiographyMessage;
    
    
private:
    FMRpcManager* Manager = nullptr;
    FPbConnectionPtr Connection;
};
