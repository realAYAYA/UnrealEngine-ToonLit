#include "GameRpcStub.h"
#include "GameRpcInterface.h"
#include "MRpcManager.h"

void UZGameRpcStub::Setup(FMRpcManager* InManager, const FPbConnectionPtr& InConn)
{
    if (Manager)
    {
        Cleanup();
    }

    Manager = InManager;
    Connection = InConn;

    if (Manager)
    {
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyAlchemyRefineResult>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyAlchemyRefineResult>& InMessage)
        {
            if (OnNotifyAlchemyRefineResult.IsBound())
            {
                FZNotifyAlchemyRefineResult Result = *InMessage;
                OnNotifyAlchemyRefineResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::RefreshItems>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::RefreshItems>& InMessage)
        {
            if (OnRefreshItems.IsBound())
            {
                FZRefreshItems Result = *InMessage;
                OnRefreshItems.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyInventorySpaceNum>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyInventorySpaceNum>& InMessage)
        {
            if (OnNotifyInventorySpaceNum.IsBound())
            {
                FZNotifyInventorySpaceNum Result = *InMessage;
                OnNotifyInventorySpaceNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::RefreshUnlockedEquipmentSlots>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::RefreshUnlockedEquipmentSlots>& InMessage)
        {
            if (OnRefreshUnlockedEquipmentSlots.IsBound())
            {
                FZRefreshUnlockedEquipmentSlots Result = *InMessage;
                OnRefreshUnlockedEquipmentSlots.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyUnlockArenaChallengeResult>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyUnlockArenaChallengeResult>& InMessage)
        {
            if (OnNotifyUnlockArenaChallengeResult.IsBound())
            {
                FZNotifyUnlockArenaChallengeResult Result = *InMessage;
                OnNotifyUnlockArenaChallengeResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::UpdateRoleMail>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::UpdateRoleMail>& InMessage)
        {
            if (OnUpdateRoleMail.IsBound())
            {
                FZUpdateRoleMail Result = *InMessage;
                OnUpdateRoleMail.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyForgeRefineResult>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyForgeRefineResult>& InMessage)
        {
            if (OnNotifyForgeRefineResult.IsBound())
            {
                FZNotifyForgeRefineResult Result = *InMessage;
                OnNotifyForgeRefineResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyGiftPackageResult>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyGiftPackageResult>& InMessage)
        {
            if (OnNotifyGiftPackageResult.IsBound())
            {
                FZNotifyGiftPackageResult Result = *InMessage;
                OnNotifyGiftPackageResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyUsePillProperty>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyUsePillProperty>& InMessage)
        {
            if (OnNotifyUsePillProperty.IsBound())
            {
                FZNotifyUsePillProperty Result = *InMessage;
                OnNotifyUsePillProperty.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyInventoryFullMailItem>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyInventoryFullMailItem>& InMessage)
        {
            if (OnNotifyInventoryFullMailItem.IsBound())
            {
                FZNotifyInventoryFullMailItem Result = *InMessage;
                OnNotifyInventoryFullMailItem.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyRoleCollectionData>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyRoleCollectionData>& InMessage)
        {
            if (OnNotifyRoleCollectionData.IsBound())
            {
                FZNotifyRoleCollectionData Result = *InMessage;
                OnNotifyRoleCollectionData.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyCommonCollectionPieceData>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyCommonCollectionPieceData>& InMessage)
        {
            if (OnNotifyCommonCollectionPieceData.IsBound())
            {
                FZNotifyCommonCollectionPieceData Result = *InMessage;
                OnNotifyCommonCollectionPieceData.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyCollectionActivatedSuit>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyCollectionActivatedSuit>& InMessage)
        {
            if (OnNotifyCollectionActivatedSuit.IsBound())
            {
                FZNotifyCollectionActivatedSuit Result = *InMessage;
                OnNotifyCollectionActivatedSuit.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyRoleCollectionHistories>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyRoleCollectionHistories>& InMessage)
        {
            if (OnNotifyRoleCollectionHistories.IsBound())
            {
                FZNotifyRoleCollectionHistories Result = *InMessage;
                OnNotifyRoleCollectionHistories.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyCollectionZoneActiveAwards>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyCollectionZoneActiveAwards>& InMessage)
        {
            if (OnNotifyCollectionZoneActiveAwards.IsBound())
            {
                FZNotifyCollectionZoneActiveAwards Result = *InMessage;
                OnNotifyCollectionZoneActiveAwards.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyRoleCollectionNextResetEnhanceTicks>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyRoleCollectionNextResetEnhanceTicks>& InMessage)
        {
            if (OnNotifyRoleCollectionNextResetEnhanceTicks.IsBound())
            {
                FZNotifyRoleCollectionNextResetEnhanceTicks Result = *InMessage;
                OnNotifyRoleCollectionNextResetEnhanceTicks.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyBossInvasionNpcKilled>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyBossInvasionNpcKilled>& InMessage)
        {
            if (OnNotifyBossInvasionNpcKilled.IsBound())
            {
                FZNotifyBossInvasionNpcKilled Result = *InMessage;
                OnNotifyBossInvasionNpcKilled.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyChecklist>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyChecklist>& InMessage)
        {
            if (OnNotifyChecklist.IsBound())
            {
                FZNotifyChecklist Result = *InMessage;
                OnNotifyChecklist.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifySeptStoneOccupyEnd>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifySeptStoneOccupyEnd>& InMessage)
        {
            if (OnNotifySeptStoneOccupyEnd.IsBound())
            {
                FZNotifySeptStoneOccupyEnd Result = *InMessage;
                OnNotifySeptStoneOccupyEnd.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyTeleportFailed>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyTeleportFailed>& InMessage)
        {
            if (OnNotifyTeleportFailed.IsBound())
            {
                FZNotifyTeleportFailed Result = *InMessage;
                OnNotifyTeleportFailed.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyFuZeng>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyFuZeng>& InMessage)
        {
            if (OnNotifyFuZeng.IsBound())
            {
                FZNotifyFuZeng Result = *InMessage;
                OnNotifyFuZeng.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::UpdateLifeCounter>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::UpdateLifeCounter>& InMessage)
        {
            if (OnUpdateLifeCounter.IsBound())
            {
                FZUpdateLifeCounter Result = *InMessage;
                OnUpdateLifeCounter.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyQuestFightChallengeOver>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyQuestFightChallengeOver>& InMessage)
        {
            if (OnNotifyQuestFightChallengeOver.IsBound())
            {
                FZNotifyQuestFightChallengeOver Result = *InMessage;
                OnNotifyQuestFightChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::DungeonChallengeOver>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::DungeonChallengeOver>& InMessage)
        {
            if (OnDungeonChallengeOver.IsBound())
            {
                FZDungeonChallengeOver Result = *InMessage;
                OnDungeonChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifySoloArenaChallengeOver>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifySoloArenaChallengeOver>& InMessage)
        {
            if (OnNotifySoloArenaChallengeOver.IsBound())
            {
                FZNotifySoloArenaChallengeOver Result = *InMessage;
                OnNotifySoloArenaChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::UpdateChat>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::UpdateChat>& InMessage)
        {
            if (OnUpdateChat.IsBound())
            {
                FZUpdateChat Result = *InMessage;
                OnUpdateChat.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyDungeonKillAllChallengeCurWaveNum>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyDungeonKillAllChallengeCurWaveNum>& InMessage)
        {
            if (OnNotifyDungeonKillAllChallengeCurWaveNum.IsBound())
            {
                FZNotifyDungeonKillAllChallengeCurWaveNum Result = *InMessage;
                OnNotifyDungeonKillAllChallengeCurWaveNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyDungeonKillAllChallengeOver>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyDungeonKillAllChallengeOver>& InMessage)
        {
            if (OnNotifyDungeonKillAllChallengeOver.IsBound())
            {
                FZNotifyDungeonKillAllChallengeOver Result = *InMessage;
                OnNotifyDungeonKillAllChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyFarmlandMessage>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyFarmlandMessage>& InMessage)
        {
            if (OnNotifyFarmlandMessage.IsBound())
            {
                FZNotifyFarmlandMessage Result = *InMessage;
                OnNotifyFarmlandMessage.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyDungeonSurviveChallengeCurWaveNum>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyDungeonSurviveChallengeCurWaveNum>& InMessage)
        {
            if (OnNotifyDungeonSurviveChallengeCurWaveNum.IsBound())
            {
                FZNotifyDungeonSurviveChallengeCurWaveNum Result = *InMessage;
                OnNotifyDungeonSurviveChallengeCurWaveNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyDungeonSurviveChallengeOver>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyDungeonSurviveChallengeOver>& InMessage)
        {
            if (OnNotifyDungeonSurviveChallengeOver.IsBound())
            {
                FZNotifyDungeonSurviveChallengeOver Result = *InMessage;
                OnNotifyDungeonSurviveChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyFriendMessage>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyFriendMessage>& InMessage)
        {
            if (OnNotifyFriendMessage.IsBound())
            {
                FZNotifyFriendMessage Result = *InMessage;
                OnNotifyFriendMessage.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlepb::NotifyBiographyMessage>([this](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::NotifyBiographyMessage>& InMessage)
        {
            if (OnNotifyBiographyMessage.IsBound())
            {
                FZNotifyBiographyMessage Result = *InMessage;
                OnNotifyBiographyMessage.Broadcast(Result);
            }
        });
    }
}

void UZGameRpcStub::Cleanup()
{
    if (Manager)
    {
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyAlchemyRefineResult>();
        Manager->GetMessageDispatcher().UnReg<idlepb::RefreshItems>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyInventorySpaceNum>();
        Manager->GetMessageDispatcher().UnReg<idlepb::RefreshUnlockedEquipmentSlots>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyUnlockArenaChallengeResult>();
        Manager->GetMessageDispatcher().UnReg<idlepb::UpdateRoleMail>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyForgeRefineResult>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyGiftPackageResult>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyUsePillProperty>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyInventoryFullMailItem>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyRoleCollectionData>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyCommonCollectionPieceData>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyCollectionActivatedSuit>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyRoleCollectionHistories>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyCollectionZoneActiveAwards>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyRoleCollectionNextResetEnhanceTicks>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyBossInvasionNpcKilled>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyChecklist>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifySeptStoneOccupyEnd>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyTeleportFailed>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyFuZeng>();
        Manager->GetMessageDispatcher().UnReg<idlepb::UpdateLifeCounter>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyQuestFightChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlepb::DungeonChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifySoloArenaChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlepb::UpdateChat>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyDungeonKillAllChallengeCurWaveNum>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyDungeonKillAllChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyFarmlandMessage>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyDungeonSurviveChallengeCurWaveNum>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyDungeonSurviveChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyFriendMessage>();
        Manager->GetMessageDispatcher().UnReg<idlepb::NotifyBiographyMessage>();        
    }
    Manager = nullptr;
    Connection = nullptr;    
}


void UZGameRpcStub::K2_LoginGame(const FZLoginGameReq& InParams, const FZOnLoginGameResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LoginGameReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LoginGame(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LoginGameAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZLoginGameAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::LoginGame(const TSharedPtr<idlepb::LoginGameReq>& InReqMessage, const OnLoginGameResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LoginGame;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LoginGameAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetCurrentCultivationDirection(const FZSetCurrentCultivationDirectionReq& InParams, const FZOnSetCurrentCultivationDirectionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetCurrentCultivationDirectionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetCurrentCultivationDirection(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetCurrentCultivationDirectionAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetCurrentCultivationDirectionAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetCurrentCultivationDirection(const TSharedPtr<idlepb::SetCurrentCultivationDirectionReq>& InReqMessage, const OnSetCurrentCultivationDirectionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetCurrentCultivationDirection;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetCurrentCultivationDirectionAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DoBreakthrough(const FZDoBreakthroughReq& InParams, const FZOnDoBreakthroughResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DoBreakthroughReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DoBreakthrough(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DoBreakthroughAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDoBreakthroughAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DoBreakthrough(const TSharedPtr<idlepb::DoBreakthroughReq>& InReqMessage, const OnDoBreakthroughResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DoBreakthrough;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DoBreakthroughAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestCommonCultivationData(const FZRequestCommonCultivationDataReq& InParams, const FZOnRequestCommonCultivationDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestCommonCultivationDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestCommonCultivationData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestCommonCultivationDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestCommonCultivationDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestCommonCultivationData(const TSharedPtr<idlepb::RequestCommonCultivationDataReq>& InReqMessage, const OnRequestCommonCultivationDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestCommonCultivationData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestCommonCultivationDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickMergeBreathing(const FZOneClickMergeBreathingReq& InParams, const FZOnOneClickMergeBreathingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickMergeBreathingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickMergeBreathing(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickMergeBreathingAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickMergeBreathingAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickMergeBreathing(const TSharedPtr<idlepb::OneClickMergeBreathingReq>& InReqMessage, const OnOneClickMergeBreathingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickMergeBreathing;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickMergeBreathingAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBreathingExerciseReward(const FZReceiveBreathingExerciseRewardReq& InParams, const FZOnReceiveBreathingExerciseRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReceiveBreathingExerciseRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBreathingExerciseReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReceiveBreathingExerciseRewardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReceiveBreathingExerciseRewardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReceiveBreathingExerciseReward(const TSharedPtr<idlepb::ReceiveBreathingExerciseRewardReq>& InReqMessage, const OnReceiveBreathingExerciseRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBreathingExerciseReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReceiveBreathingExerciseRewardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetInventoryData(const FZGetInventoryDataReq& InParams, const FZOnGetInventoryDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetInventoryDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetInventoryData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetInventoryDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetInventoryDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetInventoryData(const TSharedPtr<idlepb::GetInventoryDataReq>& InReqMessage, const OnGetInventoryDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetInventoryData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetInventoryDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetQuestData(const FZGetQuestDataReq& InParams, const FZOnGetQuestDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetQuestDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetQuestData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetQuestDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetQuestDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetQuestData(const TSharedPtr<idlepb::GetQuestDataReq>& InReqMessage, const OnGetQuestDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetQuestData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetQuestDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_CreateCharacter(const FZCreateCharacterReq& InParams, const FZOnCreateCharacterResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::CreateCharacterReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    CreateCharacter(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::CreateCharacterAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZCreateCharacterAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::CreateCharacter(const TSharedPtr<idlepb::CreateCharacterReq>& InReqMessage, const OnCreateCharacterResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::CreateCharacter;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::CreateCharacterAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseItem(const FZUseItemReq& InParams, const FZOnUseItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UseItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UseItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUseItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UseItem(const TSharedPtr<idlepb::UseItemReq>& InReqMessage, const OnUseItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UseItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseSelectGift(const FZUseSelectGiftReq& InParams, const FZOnUseSelectGiftResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UseSelectGiftReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseSelectGift(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UseSelectGiftAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUseSelectGiftAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UseSelectGift(const TSharedPtr<idlepb::UseSelectGiftReq>& InReqMessage, const OnUseSelectGiftResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseSelectGift;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UseSelectGiftAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SellItem(const FZSellItemReq& InParams, const FZOnSellItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SellItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SellItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SellItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSellItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SellItem(const TSharedPtr<idlepb::SellItemReq>& InReqMessage, const OnSellItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SellItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SellItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockEquipmentSlot(const FZUnlockEquipmentSlotReq& InParams, const FZOnUnlockEquipmentSlotResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UnlockEquipmentSlotReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockEquipmentSlot(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UnlockEquipmentSlotAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUnlockEquipmentSlotAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UnlockEquipmentSlot(const TSharedPtr<idlepb::UnlockEquipmentSlotReq>& InReqMessage, const OnUnlockEquipmentSlotResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockEquipmentSlot;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UnlockEquipmentSlotAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineStart(const FZAlchemyRefineStartReq& InParams, const FZOnAlchemyRefineStartResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AlchemyRefineStartReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineStart(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AlchemyRefineStartAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAlchemyRefineStartAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AlchemyRefineStart(const TSharedPtr<idlepb::AlchemyRefineStartReq>& InReqMessage, const OnAlchemyRefineStartResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineStart;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AlchemyRefineStartAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineCancel(const FZAlchemyRefineCancelReq& InParams, const FZOnAlchemyRefineCancelResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AlchemyRefineCancelReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineCancel(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AlchemyRefineCancelAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAlchemyRefineCancelAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AlchemyRefineCancel(const TSharedPtr<idlepb::AlchemyRefineCancelReq>& InReqMessage, const OnAlchemyRefineCancelResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineCancel;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AlchemyRefineCancelAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineExtract(const FZAlchemyRefineExtractReq& InParams, const FZOnAlchemyRefineExtractResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AlchemyRefineExtractReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineExtract(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AlchemyRefineExtractAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAlchemyRefineExtractAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AlchemyRefineExtract(const TSharedPtr<idlepb::AlchemyRefineExtractReq>& InReqMessage, const OnAlchemyRefineExtractResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineExtract;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AlchemyRefineExtractAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleShopData(const FZGetRoleShopDataReq& InParams, const FZOnGetRoleShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleShopData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleShopDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleShopDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleShopData(const TSharedPtr<idlepb::GetRoleShopDataReq>& InReqMessage, const OnGetRoleShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleShopDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshShop(const FZRefreshShopReq& InParams, const FZOnRefreshShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RefreshShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshShop(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RefreshShopAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRefreshShopAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RefreshShop(const TSharedPtr<idlepb::RefreshShopReq>& InReqMessage, const OnRefreshShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RefreshShopAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuyShopItem(const FZBuyShopItemReq& InParams, const FZOnBuyShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::BuyShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuyShopItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::BuyShopItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZBuyShopItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::BuyShopItem(const TSharedPtr<idlepb::BuyShopItemReq>& InReqMessage, const OnBuyShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuyShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::BuyShopItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleDeluxeShopData(const FZGetRoleDeluxeShopDataReq& InParams, const FZOnGetRoleDeluxeShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleDeluxeShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleDeluxeShopData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleDeluxeShopDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleDeluxeShopDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleDeluxeShopData(const TSharedPtr<idlepb::GetRoleDeluxeShopDataReq>& InReqMessage, const OnGetRoleDeluxeShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleDeluxeShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleDeluxeShopDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshDeluxeShop(const FZRefreshDeluxeShopReq& InParams, const FZOnRefreshDeluxeShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RefreshDeluxeShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshDeluxeShop(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RefreshDeluxeShopAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRefreshDeluxeShopAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RefreshDeluxeShop(const TSharedPtr<idlepb::RefreshDeluxeShopReq>& InReqMessage, const OnRefreshDeluxeShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshDeluxeShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RefreshDeluxeShopAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuyDeluxeShopItem(const FZBuyDeluxeShopItemReq& InParams, const FZOnBuyDeluxeShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::BuyDeluxeShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuyDeluxeShopItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::BuyDeluxeShopItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZBuyDeluxeShopItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::BuyDeluxeShopItem(const TSharedPtr<idlepb::BuyDeluxeShopItemReq>& InReqMessage, const OnBuyDeluxeShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuyDeluxeShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::BuyDeluxeShopItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetTemporaryPackageData(const FZGetTemporaryPackageDataReq& InParams, const FZOnGetTemporaryPackageDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetTemporaryPackageDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetTemporaryPackageData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetTemporaryPackageDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetTemporaryPackageDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetTemporaryPackageData(const TSharedPtr<idlepb::GetTemporaryPackageDataReq>& InReqMessage, const OnGetTemporaryPackageDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetTemporaryPackageData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetTemporaryPackageDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExtractTemporaryPackageItems(const FZExtractTemporaryPackageItemsReq& InParams, const FZOnExtractTemporaryPackageItemsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ExtractTemporaryPackageItemsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExtractTemporaryPackageItems(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ExtractTemporaryPackageItemsAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZExtractTemporaryPackageItemsAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ExtractTemporaryPackageItems(const TSharedPtr<idlepb::ExtractTemporaryPackageItemsReq>& InReqMessage, const OnExtractTemporaryPackageItemsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExtractTemporaryPackageItems;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ExtractTemporaryPackageItemsAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SpeedupRelive(const FZSpeedupReliveReq& InParams, const FZOnSpeedupReliveResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SpeedupReliveReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SpeedupRelive(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SpeedupReliveAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSpeedupReliveAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SpeedupRelive(const TSharedPtr<idlepb::SpeedupReliveReq>& InReqMessage, const OnSpeedupReliveResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SpeedupRelive;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SpeedupReliveAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMapInfo(const FZGetMapInfoReq& InParams, const FZOnGetMapInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetMapInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMapInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetMapInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetMapInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetMapInfo(const TSharedPtr<idlepb::GetMapInfoReq>& InReqMessage, const OnGetMapInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMapInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetMapInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockArena(const FZUnlockArenaReq& InParams, const FZOnUnlockArenaResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UnlockArenaReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockArena(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UnlockArenaAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUnlockArenaAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UnlockArena(const TSharedPtr<idlepb::UnlockArenaReq>& InReqMessage, const OnUnlockArenaResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockArena;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UnlockArenaAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QuestOp(const FZQuestOpReq& InParams, const FZOnQuestOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::QuestOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QuestOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::QuestOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZQuestOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::QuestOp(const TSharedPtr<idlepb::QuestOpReq>& InReqMessage, const OnQuestOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QuestOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::QuestOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EquipmentPutOn(const FZEquipmentPutOnReq& InParams, const FZOnEquipmentPutOnResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::EquipmentPutOnReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EquipmentPutOn(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::EquipmentPutOnAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZEquipmentPutOnAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::EquipmentPutOn(const TSharedPtr<idlepb::EquipmentPutOnReq>& InReqMessage, const OnEquipmentPutOnResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentPutOn;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::EquipmentPutOnAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EquipmentTakeOff(const FZEquipmentTakeOffReq& InParams, const FZOnEquipmentTakeOffResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::EquipmentTakeOffReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EquipmentTakeOff(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::EquipmentTakeOffAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZEquipmentTakeOffAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::EquipmentTakeOff(const TSharedPtr<idlepb::EquipmentTakeOffReq>& InReqMessage, const OnEquipmentTakeOffResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentTakeOff;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::EquipmentTakeOffAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLeaderboardPreview(const FZGetLeaderboardPreviewReq& InParams, const FZOnGetLeaderboardPreviewResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetLeaderboardPreviewReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLeaderboardPreview(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetLeaderboardPreviewAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetLeaderboardPreviewAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetLeaderboardPreview(const TSharedPtr<idlepb::GetLeaderboardPreviewReq>& InReqMessage, const OnGetLeaderboardPreviewResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardPreview;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetLeaderboardPreviewAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLeaderboardData(const FZGetLeaderboardDataReq& InParams, const FZOnGetLeaderboardDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetLeaderboardDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLeaderboardData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetLeaderboardDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetLeaderboardDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetLeaderboardData(const TSharedPtr<idlepb::GetLeaderboardDataReq>& InReqMessage, const OnGetLeaderboardDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetLeaderboardDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleLeaderboardData(const FZGetRoleLeaderboardDataReq& InParams, const FZOnGetRoleLeaderboardDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleLeaderboardDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleLeaderboardData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleLeaderboardDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleLeaderboardDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleLeaderboardData(const TSharedPtr<idlepb::GetRoleLeaderboardDataReq>& InReqMessage, const OnGetRoleLeaderboardDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleLeaderboardData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleLeaderboardDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LeaderboardClickLike(const FZLeaderboardClickLikeReq& InParams, const FZOnLeaderboardClickLikeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LeaderboardClickLikeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LeaderboardClickLike(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LeaderboardClickLikeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZLeaderboardClickLikeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::LeaderboardClickLike(const TSharedPtr<idlepb::LeaderboardClickLikeReq>& InReqMessage, const OnLeaderboardClickLikeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardClickLike;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LeaderboardClickLikeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LeaderboardUpdateMessage(const FZLeaderboardUpdateMessageReq& InParams, const FZOnLeaderboardUpdateMessageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LeaderboardUpdateMessageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LeaderboardUpdateMessage(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LeaderboardUpdateMessageAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZLeaderboardUpdateMessageAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::LeaderboardUpdateMessage(const TSharedPtr<idlepb::LeaderboardUpdateMessageReq>& InReqMessage, const OnLeaderboardUpdateMessageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardUpdateMessage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LeaderboardUpdateMessageAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetFuZeReward(const FZGetFuZeRewardReq& InParams, const FZOnGetFuZeRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetFuZeRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetFuZeReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetFuZeRewardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetFuZeRewardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetFuZeReward(const TSharedPtr<idlepb::GetFuZeRewardReq>& InReqMessage, const OnGetFuZeRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetFuZeReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetFuZeRewardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleMailData(const FZGetRoleMailDataReq& InParams, const FZOnGetRoleMailDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleMailDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleMailData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleMailDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleMailDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleMailData(const TSharedPtr<idlepb::GetRoleMailDataReq>& InReqMessage, const OnGetRoleMailDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleMailData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleMailDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReadMail(const FZReadMailReq& InParams, const FZOnReadMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReadMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReadMail(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReadMailAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReadMailAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReadMail(const TSharedPtr<idlepb::ReadMailReq>& InReqMessage, const OnReadMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReadMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReadMailAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMailAttachment(const FZGetMailAttachmentReq& InParams, const FZOnGetMailAttachmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetMailAttachmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMailAttachment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetMailAttachmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetMailAttachmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetMailAttachment(const TSharedPtr<idlepb::GetMailAttachmentReq>& InReqMessage, const OnGetMailAttachmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMailAttachment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetMailAttachmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DeleteMail(const FZDeleteMailReq& InParams, const FZOnDeleteMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DeleteMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DeleteMail(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DeleteMailAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDeleteMailAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DeleteMail(const TSharedPtr<idlepb::DeleteMailReq>& InReqMessage, const OnDeleteMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DeleteMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DeleteMailAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickGetMailAttachment(const FZOneClickGetMailAttachmentReq& InParams, const FZOnOneClickGetMailAttachmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickGetMailAttachmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickGetMailAttachment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickGetMailAttachmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickGetMailAttachmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickGetMailAttachment(const TSharedPtr<idlepb::OneClickGetMailAttachmentReq>& InReqMessage, const OnOneClickGetMailAttachmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickGetMailAttachment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickGetMailAttachmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickReadMail(const FZOneClickReadMailReq& InParams, const FZOnOneClickReadMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickReadMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickReadMail(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickReadMailAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickReadMailAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickReadMail(const TSharedPtr<idlepb::OneClickReadMailReq>& InReqMessage, const OnOneClickReadMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickReadMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickReadMailAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickDeleteMail(const FZOneClickDeleteMailReq& InParams, const FZOnOneClickDeleteMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickDeleteMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickDeleteMail(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickDeleteMailAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickDeleteMailAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickDeleteMail(const TSharedPtr<idlepb::OneClickDeleteMailReq>& InReqMessage, const OnOneClickDeleteMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickDeleteMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickDeleteMailAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockFunctionModule(const FZUnlockFunctionModuleReq& InParams, const FZOnUnlockFunctionModuleResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UnlockFunctionModuleReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockFunctionModule(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UnlockFunctionModuleAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUnlockFunctionModuleAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UnlockFunctionModule(const TSharedPtr<idlepb::UnlockFunctionModuleReq>& InReqMessage, const OnUnlockFunctionModuleResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockFunctionModule;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UnlockFunctionModuleAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetChatRecord(const FZGetChatRecordReq& InParams, const FZOnGetChatRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetChatRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetChatRecord(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetChatRecordAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetChatRecordAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetChatRecord(const TSharedPtr<idlepb::GetChatRecordReq>& InReqMessage, const OnGetChatRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetChatRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetChatRecordAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DeletePrivateChatRecord(const FZDeletePrivateChatRecordReq& InParams, const FZOnDeletePrivateChatRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DeletePrivateChatRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DeletePrivateChatRecord(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DeletePrivateChatRecordAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDeletePrivateChatRecordAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DeletePrivateChatRecord(const TSharedPtr<idlepb::DeletePrivateChatRecordReq>& InReqMessage, const OnDeletePrivateChatRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DeletePrivateChatRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DeletePrivateChatRecordAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SendChatMessage(const FZSendChatMessageReq& InParams, const FZOnSendChatMessageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SendChatMessageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SendChatMessage(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SendChatMessageAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSendChatMessageAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SendChatMessage(const TSharedPtr<idlepb::SendChatMessageReq>& InReqMessage, const OnSendChatMessageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SendChatMessage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SendChatMessageAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ClearChatUnreadNum(const FZClearChatUnreadNumReq& InParams, const FZOnClearChatUnreadNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ClearChatUnreadNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ClearChatUnreadNum(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ClearChatUnreadNumAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZClearChatUnreadNumAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ClearChatUnreadNum(const TSharedPtr<idlepb::ClearChatUnreadNumReq>& InReqMessage, const OnClearChatUnreadNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ClearChatUnreadNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ClearChatUnreadNumAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineStart(const FZForgeRefineStartReq& InParams, const FZOnForgeRefineStartResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ForgeRefineStartReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineStart(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ForgeRefineStartAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZForgeRefineStartAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ForgeRefineStart(const TSharedPtr<idlepb::ForgeRefineStartReq>& InReqMessage, const OnForgeRefineStartResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineStart;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ForgeRefineStartAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineCancel(const FZForgeRefineCancelReq& InParams, const FZOnForgeRefineCancelResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ForgeRefineCancelReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineCancel(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ForgeRefineCancelAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZForgeRefineCancelAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ForgeRefineCancel(const TSharedPtr<idlepb::ForgeRefineCancelReq>& InReqMessage, const OnForgeRefineCancelResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineCancel;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ForgeRefineCancelAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineExtract(const FZForgeRefineExtractReq& InParams, const FZOnForgeRefineExtractResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ForgeRefineExtractReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineExtract(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ForgeRefineExtractAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZForgeRefineExtractAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ForgeRefineExtract(const TSharedPtr<idlepb::ForgeRefineExtractReq>& InReqMessage, const OnForgeRefineExtractResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineExtract;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ForgeRefineExtractAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetForgeLostEquipmentData(const FZGetForgeLostEquipmentDataReq& InParams, const FZOnGetForgeLostEquipmentDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetForgeLostEquipmentDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetForgeLostEquipmentData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetForgeLostEquipmentDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetForgeLostEquipmentDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetForgeLostEquipmentData(const TSharedPtr<idlepb::GetForgeLostEquipmentDataReq>& InReqMessage, const OnGetForgeLostEquipmentDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetForgeLostEquipmentData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetForgeLostEquipmentDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeDestroy(const FZForgeDestroyReq& InParams, const FZOnForgeDestroyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ForgeDestroyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeDestroy(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ForgeDestroyAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZForgeDestroyAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ForgeDestroy(const TSharedPtr<idlepb::ForgeDestroyReq>& InReqMessage, const OnForgeDestroyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeDestroy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ForgeDestroyAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeFindBack(const FZForgeFindBackReq& InParams, const FZOnForgeFindBackResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ForgeFindBackReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeFindBack(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ForgeFindBackAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZForgeFindBackAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ForgeFindBack(const TSharedPtr<idlepb::ForgeFindBackReq>& InReqMessage, const OnForgeFindBackResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeFindBack;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ForgeFindBackAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestPillElixirData(const FZRequestPillElixirDataReq& InParams, const FZOnRequestPillElixirDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestPillElixirDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestPillElixirData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestPillElixirDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestPillElixirDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestPillElixirData(const TSharedPtr<idlepb::RequestPillElixirDataReq>& InReqMessage, const OnRequestPillElixirDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestPillElixirData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestPillElixirDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetOnePillElixirData(const FZGetOnePillElixirDataReq& InParams, const FZOnGetOnePillElixirDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetOnePillElixirDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetOnePillElixirData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetOnePillElixirDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetOnePillElixirDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetOnePillElixirData(const TSharedPtr<idlepb::GetOnePillElixirDataReq>& InReqMessage, const OnGetOnePillElixirDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetOnePillElixirData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetOnePillElixirDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestModifyPillElixirFilter(const FZRequestModifyPillElixirFilterReq& InParams, const FZOnRequestModifyPillElixirFilterResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestModifyPillElixirFilterReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestModifyPillElixirFilter(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestModifyPillElixirFilterAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestModifyPillElixirFilterAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestModifyPillElixirFilter(const TSharedPtr<idlepb::RequestModifyPillElixirFilterReq>& InReqMessage, const OnRequestModifyPillElixirFilterResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestModifyPillElixirFilter;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestModifyPillElixirFilterAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UsePillElixir(const FZUsePillElixirReq& InParams, const FZOnUsePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UsePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UsePillElixir(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UsePillElixirAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUsePillElixirAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UsePillElixir(const TSharedPtr<idlepb::UsePillElixirReq>& InReqMessage, const OnUsePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UsePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UsePillElixirAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickUsePillElixir(const FZOneClickUsePillElixirReq& InParams, const FZOnOneClickUsePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickUsePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickUsePillElixir(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickUsePillElixirAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickUsePillElixirAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickUsePillElixir(const TSharedPtr<idlepb::OneClickUsePillElixirReq>& InReqMessage, const OnOneClickUsePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickUsePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickUsePillElixirAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_TradePillElixir(const FZTradePillElixirReq& InParams, const FZOnTradePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::TradePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    TradePillElixir(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::TradePillElixirAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZTradePillElixirAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::TradePillElixir(const TSharedPtr<idlepb::TradePillElixirReq>& InReqMessage, const OnTradePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::TradePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::TradePillElixirAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReinforceEquipment(const FZReinforceEquipmentReq& InParams, const FZOnReinforceEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReinforceEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReinforceEquipment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReinforceEquipmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReinforceEquipmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReinforceEquipment(const TSharedPtr<idlepb::ReinforceEquipmentReq>& InReqMessage, const OnReinforceEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReinforceEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReinforceEquipmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefineEquipment(const FZRefineEquipmentReq& InParams, const FZOnRefineEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RefineEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefineEquipment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RefineEquipmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRefineEquipmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RefineEquipment(const TSharedPtr<idlepb::RefineEquipmentReq>& InReqMessage, const OnRefineEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefineEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RefineEquipmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QiWenEquipment(const FZQiWenEquipmentReq& InParams, const FZOnQiWenEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::QiWenEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QiWenEquipment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::QiWenEquipmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZQiWenEquipmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::QiWenEquipment(const TSharedPtr<idlepb::QiWenEquipmentReq>& InReqMessage, const OnQiWenEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QiWenEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::QiWenEquipmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ResetEquipment(const FZResetEquipmentReq& InParams, const FZOnResetEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ResetEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ResetEquipment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ResetEquipmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZResetEquipmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ResetEquipment(const TSharedPtr<idlepb::ResetEquipmentReq>& InReqMessage, const OnResetEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ResetEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ResetEquipmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_InheritEquipment(const FZInheritEquipmentReq& InParams, const FZOnInheritEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::InheritEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    InheritEquipment(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::InheritEquipmentAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZInheritEquipmentAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::InheritEquipment(const TSharedPtr<idlepb::InheritEquipmentReq>& InReqMessage, const OnInheritEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::InheritEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::InheritEquipmentAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LockItem(const FZLockItemReq& InParams, const FZOnLockItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::LockItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LockItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::LockItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZLockItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::LockItem(const TSharedPtr<idlepb::LockItemReq>& InReqMessage, const OnLockItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LockItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::LockItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SoloArenaChallenge(const FZSoloArenaChallengeReq& InParams, const FZOnSoloArenaChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SoloArenaChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SoloArenaChallenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SoloArenaChallengeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSoloArenaChallengeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SoloArenaChallenge(const TSharedPtr<idlepb::SoloArenaChallengeReq>& InReqMessage, const OnSoloArenaChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SoloArenaChallengeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SoloArenaQuickEnd(const FZSoloArenaQuickEndReq& InParams, const FZOnSoloArenaQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SoloArenaQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SoloArenaQuickEnd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SoloArenaQuickEndAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSoloArenaQuickEndAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SoloArenaQuickEnd(const TSharedPtr<idlepb::SoloArenaQuickEndReq>& InReqMessage, const OnSoloArenaQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SoloArenaQuickEndAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSoloArenaHistoryList(const FZGetSoloArenaHistoryListReq& InParams, const FZOnGetSoloArenaHistoryListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSoloArenaHistoryListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSoloArenaHistoryList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSoloArenaHistoryListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSoloArenaHistoryListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSoloArenaHistoryList(const TSharedPtr<idlepb::GetSoloArenaHistoryListReq>& InReqMessage, const OnGetSoloArenaHistoryListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSoloArenaHistoryList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSoloArenaHistoryListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerChallenge(const FZMonsterTowerChallengeReq& InParams, const FZOnMonsterTowerChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::MonsterTowerChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerChallenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::MonsterTowerChallengeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZMonsterTowerChallengeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::MonsterTowerChallenge(const TSharedPtr<idlepb::MonsterTowerChallengeReq>& InReqMessage, const OnMonsterTowerChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::MonsterTowerChallengeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerDrawIdleAward(const FZMonsterTowerDrawIdleAwardReq& InParams, const FZOnMonsterTowerDrawIdleAwardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::MonsterTowerDrawIdleAwardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerDrawIdleAward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::MonsterTowerDrawIdleAwardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZMonsterTowerDrawIdleAwardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::MonsterTowerDrawIdleAward(const TSharedPtr<idlepb::MonsterTowerDrawIdleAwardReq>& InReqMessage, const OnMonsterTowerDrawIdleAwardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerDrawIdleAward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::MonsterTowerDrawIdleAwardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerClosedDoorTraining(const FZMonsterTowerClosedDoorTrainingReq& InParams, const FZOnMonsterTowerClosedDoorTrainingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::MonsterTowerClosedDoorTrainingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerClosedDoorTraining(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::MonsterTowerClosedDoorTrainingAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZMonsterTowerClosedDoorTrainingAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::MonsterTowerClosedDoorTraining(const TSharedPtr<idlepb::MonsterTowerClosedDoorTrainingReq>& InReqMessage, const OnMonsterTowerClosedDoorTrainingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerClosedDoorTraining;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::MonsterTowerClosedDoorTrainingAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerQuickEnd(const FZMonsterTowerQuickEndReq& InParams, const FZOnMonsterTowerQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::MonsterTowerQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerQuickEnd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::MonsterTowerQuickEndAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZMonsterTowerQuickEndAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::MonsterTowerQuickEnd(const TSharedPtr<idlepb::MonsterTowerQuickEndReq>& InReqMessage, const OnMonsterTowerQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::MonsterTowerQuickEndAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMonsterTowerChallengeList(const FZGetMonsterTowerChallengeListReq& InParams, const FZOnGetMonsterTowerChallengeListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetMonsterTowerChallengeListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMonsterTowerChallengeList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetMonsterTowerChallengeListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetMonsterTowerChallengeListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetMonsterTowerChallengeList(const TSharedPtr<idlepb::GetMonsterTowerChallengeListReq>& InReqMessage, const OnGetMonsterTowerChallengeListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetMonsterTowerChallengeListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMonsterTowerChallengeReward(const FZGetMonsterTowerChallengeRewardReq& InParams, const FZOnGetMonsterTowerChallengeRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetMonsterTowerChallengeRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMonsterTowerChallengeReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetMonsterTowerChallengeRewardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetMonsterTowerChallengeRewardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetMonsterTowerChallengeReward(const TSharedPtr<idlepb::GetMonsterTowerChallengeRewardReq>& InReqMessage, const OnGetMonsterTowerChallengeRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetMonsterTowerChallengeRewardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetWorldTimeDilation(const FZSetWorldTimeDilationReq& InParams, const FZOnSetWorldTimeDilationResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetWorldTimeDilationReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetWorldTimeDilation(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetWorldTimeDilationAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetWorldTimeDilationAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetWorldTimeDilation(const TSharedPtr<idlepb::SetWorldTimeDilationReq>& InReqMessage, const OnSetWorldTimeDilationResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetWorldTimeDilation;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetWorldTimeDilationAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetFightMode(const FZSetFightModeReq& InParams, const FZOnSetFightModeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetFightModeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetFightMode(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetFightModeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetFightModeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetFightMode(const TSharedPtr<idlepb::SetFightModeReq>& InReqMessage, const OnSetFightModeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetFightMode;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetFightModeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpgradeQiCollector(const FZUpgradeQiCollectorReq& InParams, const FZOnUpgradeQiCollectorResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UpgradeQiCollectorReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpgradeQiCollector(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UpgradeQiCollectorAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUpgradeQiCollectorAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UpgradeQiCollector(const TSharedPtr<idlepb::UpgradeQiCollectorReq>& InReqMessage, const OnUpgradeQiCollectorResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpgradeQiCollector;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UpgradeQiCollectorAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleAllStats(const FZGetRoleAllStatsReq& InParams, const FZOnGetRoleAllStatsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleAllStatsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleAllStats(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleAllStatsAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleAllStatsAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleAllStats(const TSharedPtr<idlepb::GetRoleAllStatsReq>& InReqMessage, const OnGetRoleAllStatsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAllStats;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleAllStatsAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShanhetuData(const FZGetShanhetuDataReq& InParams, const FZOnGetShanhetuDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetShanhetuDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShanhetuData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetShanhetuDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetShanhetuDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetShanhetuData(const TSharedPtr<idlepb::GetShanhetuDataReq>& InReqMessage, const OnGetShanhetuDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetShanhetuDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetShanhetuUseConfig(const FZSetShanhetuUseConfigReq& InParams, const FZOnSetShanhetuUseConfigResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetShanhetuUseConfigReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetShanhetuUseConfig(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetShanhetuUseConfigAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetShanhetuUseConfigAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetShanhetuUseConfig(const TSharedPtr<idlepb::SetShanhetuUseConfigReq>& InReqMessage, const OnSetShanhetuUseConfigResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetShanhetuUseConfig;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetShanhetuUseConfigAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseShanhetu(const FZUseShanhetuReq& InParams, const FZOnUseShanhetuResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UseShanhetuReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseShanhetu(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UseShanhetuAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUseShanhetuAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UseShanhetu(const TSharedPtr<idlepb::UseShanhetuReq>& InReqMessage, const OnUseShanhetuResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseShanhetu;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UseShanhetuAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_StepShanhetu(const FZStepShanhetuReq& InParams, const FZOnStepShanhetuResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::StepShanhetuReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    StepShanhetu(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::StepShanhetuAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZStepShanhetuAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::StepShanhetu(const TSharedPtr<idlepb::StepShanhetuReq>& InReqMessage, const OnStepShanhetuResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::StepShanhetu;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::StepShanhetuAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShanhetuUseRecord(const FZGetShanhetuUseRecordReq& InParams, const FZOnGetShanhetuUseRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetShanhetuUseRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShanhetuUseRecord(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetShanhetuUseRecordAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetShanhetuUseRecordAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetShanhetuUseRecord(const TSharedPtr<idlepb::GetShanhetuUseRecordReq>& InReqMessage, const OnGetShanhetuUseRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuUseRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetShanhetuUseRecordAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetAttackLockType(const FZSetAttackLockTypeReq& InParams, const FZOnSetAttackLockTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetAttackLockTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetAttackLockType(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetAttackLockTypeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetAttackLockTypeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetAttackLockType(const TSharedPtr<idlepb::SetAttackLockTypeReq>& InReqMessage, const OnSetAttackLockTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackLockType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetAttackLockTypeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetAttackUnlockType(const FZSetAttackUnlockTypeReq& InParams, const FZOnSetAttackUnlockTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetAttackUnlockTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetAttackUnlockType(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetAttackUnlockTypeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetAttackUnlockTypeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetAttackUnlockType(const TSharedPtr<idlepb::SetAttackUnlockTypeReq>& InReqMessage, const OnSetAttackUnlockTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackUnlockType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetAttackUnlockTypeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetShowUnlockButton(const FZSetShowUnlockButtonReq& InParams, const FZOnSetShowUnlockButtonResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetShowUnlockButtonReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetShowUnlockButton(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetShowUnlockButtonAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetShowUnlockButtonAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetShowUnlockButton(const TSharedPtr<idlepb::SetShowUnlockButtonReq>& InReqMessage, const OnSetShowUnlockButtonResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetShowUnlockButton;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetShowUnlockButtonAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetUserVar(const FZGetUserVarReq& InParams, const FZOnGetUserVarResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetUserVarReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetUserVar(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetUserVarRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetUserVarRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetUserVar(const TSharedPtr<idlepb::GetUserVarReq>& InReqMessage, const OnGetUserVarResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVar;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetUserVarRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetUserVars(const FZGetUserVarsReq& InParams, const FZOnGetUserVarsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetUserVarsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetUserVars(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetUserVarsRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetUserVarsRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetUserVars(const TSharedPtr<idlepb::GetUserVarsReq>& InReqMessage, const OnGetUserVarsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVars;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetUserVarsRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionArenaSummary(const FZGetBossInvasionArenaSummaryReq& InParams, const FZOnGetBossInvasionArenaSummaryResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetBossInvasionArenaSummaryReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionArenaSummary(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetBossInvasionArenaSummaryRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetBossInvasionArenaSummaryRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetBossInvasionArenaSummary(const TSharedPtr<idlepb::GetBossInvasionArenaSummaryReq>& InReqMessage, const OnGetBossInvasionArenaSummaryResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaSummary;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetBossInvasionArenaSummaryRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionArenaTopList(const FZGetBossInvasionArenaTopListReq& InParams, const FZOnGetBossInvasionArenaTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetBossInvasionArenaTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionArenaTopList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetBossInvasionArenaTopListRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetBossInvasionArenaTopListRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetBossInvasionArenaTopList(const TSharedPtr<idlepb::GetBossInvasionArenaTopListReq>& InReqMessage, const OnGetBossInvasionArenaTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetBossInvasionArenaTopListRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionInfo(const FZGetBossInvasionInfoReq& InParams, const FZOnGetBossInvasionInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetBossInvasionInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetBossInvasionInfoRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetBossInvasionInfoRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetBossInvasionInfo(const TSharedPtr<idlepb::GetBossInvasionInfoReq>& InReqMessage, const OnGetBossInvasionInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetBossInvasionInfoRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DrawBossInvasionKillReward(const FZDrawBossInvasionKillRewardReq& InParams, const FZOnDrawBossInvasionKillRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DrawBossInvasionKillRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DrawBossInvasionKillReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DrawBossInvasionKillRewardRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDrawBossInvasionKillRewardRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DrawBossInvasionKillReward(const TSharedPtr<idlepb::DrawBossInvasionKillRewardReq>& InReqMessage, const OnDrawBossInvasionKillRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionKillReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DrawBossInvasionKillRewardRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DrawBossInvasionDamageReward(const FZDrawBossInvasionDamageRewardReq& InParams, const FZOnDrawBossInvasionDamageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DrawBossInvasionDamageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DrawBossInvasionDamageReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DrawBossInvasionDamageRewardRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDrawBossInvasionDamageRewardRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DrawBossInvasionDamageReward(const TSharedPtr<idlepb::DrawBossInvasionDamageRewardReq>& InReqMessage, const OnDrawBossInvasionDamageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionDamageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DrawBossInvasionDamageRewardRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BossInvasionTeleport(const FZBossInvasionTeleportReq& InParams, const FZOnBossInvasionTeleportResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::BossInvasionTeleportReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BossInvasionTeleport(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::BossInvasionTeleportRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZBossInvasionTeleportRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::BossInvasionTeleport(const TSharedPtr<idlepb::BossInvasionTeleportReq>& InReqMessage, const OnBossInvasionTeleportResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BossInvasionTeleport;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::BossInvasionTeleportRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfItem(const FZShareSelfItemReq& InParams, const FZOnShareSelfItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ShareSelfItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ShareSelfItemRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZShareSelfItemRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ShareSelfItem(const TSharedPtr<idlepb::ShareSelfItemReq>& InReqMessage, const OnShareSelfItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ShareSelfItemRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfItems(const FZShareSelfItemsReq& InParams, const FZOnShareSelfItemsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ShareSelfItemsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfItems(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ShareSelfItemsRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZShareSelfItemsRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ShareSelfItems(const TSharedPtr<idlepb::ShareSelfItemsReq>& InReqMessage, const OnShareSelfItemsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItems;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ShareSelfItemsRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShareItemData(const FZGetShareItemDataReq& InParams, const FZOnGetShareItemDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetShareItemDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShareItemData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetShareItemDataRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetShareItemDataRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetShareItemData(const TSharedPtr<idlepb::GetShareItemDataReq>& InReqMessage, const OnGetShareItemDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareItemData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetShareItemDataRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleCollectionData(const FZGetRoleCollectionDataReq& InParams, const FZOnGetRoleCollectionDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleCollectionDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleCollectionData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleCollectionDataRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleCollectionDataRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleCollectionData(const TSharedPtr<idlepb::GetRoleCollectionDataReq>& InReqMessage, const OnGetRoleCollectionDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleCollectionData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleCollectionDataRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RoleCollectionOp(const FZRoleCollectionOpReq& InParams, const FZOnRoleCollectionOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RoleCollectionOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RoleCollectionOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RoleCollectionOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRoleCollectionOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RoleCollectionOp(const TSharedPtr<idlepb::RoleCollectionOpReq>& InReqMessage, const OnRoleCollectionOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RoleCollectionOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RoleCollectionOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfRoleCollection(const FZShareSelfRoleCollectionReq& InParams, const FZOnShareSelfRoleCollectionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ShareSelfRoleCollectionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfRoleCollection(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ShareSelfRoleCollectionRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZShareSelfRoleCollectionRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ShareSelfRoleCollection(const TSharedPtr<idlepb::ShareSelfRoleCollectionReq>& InReqMessage, const OnShareSelfRoleCollectionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfRoleCollection;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ShareSelfRoleCollectionRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShareRoleCollectionData(const FZGetShareRoleCollectionDataReq& InParams, const FZOnGetShareRoleCollectionDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetShareRoleCollectionDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShareRoleCollectionData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetShareRoleCollectionDataRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetShareRoleCollectionDataRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetShareRoleCollectionData(const TSharedPtr<idlepb::GetShareRoleCollectionDataReq>& InReqMessage, const OnGetShareRoleCollectionDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareRoleCollectionData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetShareRoleCollectionDataRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetChecklistData(const FZGetChecklistDataReq& InParams, const FZOnGetChecklistDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetChecklistDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetChecklistData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetChecklistDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetChecklistDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetChecklistData(const TSharedPtr<idlepb::GetChecklistDataReq>& InReqMessage, const OnGetChecklistDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetChecklistData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetChecklistDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ChecklistOp(const FZChecklistOpReq& InParams, const FZOnChecklistOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ChecklistOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ChecklistOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ChecklistOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZChecklistOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ChecklistOp(const TSharedPtr<idlepb::ChecklistOpReq>& InReqMessage, const OnChecklistOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ChecklistOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ChecklistOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpdateChecklist(const FZUpdateChecklistReq& InParams, const FZOnUpdateChecklistResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UpdateChecklistReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpdateChecklist(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UpdateChecklistAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUpdateChecklistAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UpdateChecklist(const TSharedPtr<idlepb::UpdateChecklistReq>& InReqMessage, const OnUpdateChecklistResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateChecklist;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UpdateChecklistAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSwordPkInfo(const FZGetSwordPkInfoReq& InParams, const FZOnGetSwordPkInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSwordPkInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSwordPkInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSwordPkInfoRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSwordPkInfoRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSwordPkInfo(const TSharedPtr<idlepb::GetSwordPkInfoReq>& InReqMessage, const OnGetSwordPkInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSwordPkInfoRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkSignup(const FZSwordPkSignupReq& InParams, const FZOnSwordPkSignupResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SwordPkSignupReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkSignup(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SwordPkSignupRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSwordPkSignupRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SwordPkSignup(const TSharedPtr<idlepb::SwordPkSignupReq>& InReqMessage, const OnSwordPkSignupResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkSignup;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SwordPkSignupRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkMatching(const FZSwordPkMatchingReq& InParams, const FZOnSwordPkMatchingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SwordPkMatchingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkMatching(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SwordPkMatchingRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSwordPkMatchingRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SwordPkMatching(const TSharedPtr<idlepb::SwordPkMatchingReq>& InReqMessage, const OnSwordPkMatchingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkMatching;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SwordPkMatchingRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkChallenge(const FZSwordPkChallengeReq& InParams, const FZOnSwordPkChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SwordPkChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkChallenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SwordPkChallengeRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSwordPkChallengeRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SwordPkChallenge(const TSharedPtr<idlepb::SwordPkChallengeReq>& InReqMessage, const OnSwordPkChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SwordPkChallengeRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkRevenge(const FZSwordPkRevengeReq& InParams, const FZOnSwordPkRevengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SwordPkRevengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkRevenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SwordPkRevengeRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSwordPkRevengeRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SwordPkRevenge(const TSharedPtr<idlepb::SwordPkRevengeReq>& InReqMessage, const OnSwordPkRevengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkRevenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SwordPkRevengeRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSwordPkTopList(const FZGetSwordPkTopListReq& InParams, const FZOnGetSwordPkTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSwordPkTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSwordPkTopList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSwordPkTopListRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSwordPkTopListRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSwordPkTopList(const TSharedPtr<idlepb::GetSwordPkTopListReq>& InReqMessage, const OnGetSwordPkTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSwordPkTopListRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkExchangeHeroCard(const FZSwordPkExchangeHeroCardReq& InParams, const FZOnSwordPkExchangeHeroCardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SwordPkExchangeHeroCardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkExchangeHeroCard(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SwordPkExchangeHeroCardRsp>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSwordPkExchangeHeroCardRsp Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SwordPkExchangeHeroCard(const TSharedPtr<idlepb::SwordPkExchangeHeroCardReq>& InReqMessage, const OnSwordPkExchangeHeroCardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkExchangeHeroCard;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SwordPkExchangeHeroCardRsp>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCommonItemExchangeData(const FZGetCommonItemExchangeDataReq& InParams, const FZOnGetCommonItemExchangeDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetCommonItemExchangeDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCommonItemExchangeData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetCommonItemExchangeDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetCommonItemExchangeDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetCommonItemExchangeData(const TSharedPtr<idlepb::GetCommonItemExchangeDataReq>& InReqMessage, const OnGetCommonItemExchangeDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCommonItemExchangeData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetCommonItemExchangeDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExchangeCommonItem(const FZExchangeCommonItemReq& InParams, const FZOnExchangeCommonItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ExchangeCommonItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExchangeCommonItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ExchangeCommonItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZExchangeCommonItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ExchangeCommonItem(const TSharedPtr<idlepb::ExchangeCommonItemReq>& InReqMessage, const OnExchangeCommonItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExchangeCommonItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ExchangeCommonItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SynthesisCommonItem(const FZSynthesisCommonItemReq& InParams, const FZOnSynthesisCommonItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SynthesisCommonItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SynthesisCommonItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SynthesisCommonItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSynthesisCommonItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SynthesisCommonItem(const TSharedPtr<idlepb::SynthesisCommonItemReq>& InReqMessage, const OnSynthesisCommonItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SynthesisCommonItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SynthesisCommonItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCandidatesSeptList(const FZGetCandidatesSeptListReq& InParams, const FZOnGetCandidatesSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetCandidatesSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCandidatesSeptList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetCandidatesSeptListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetCandidatesSeptListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetCandidatesSeptList(const TSharedPtr<idlepb::GetCandidatesSeptListReq>& InReqMessage, const OnGetCandidatesSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetCandidatesSeptListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SearchSept(const FZSearchSeptReq& InParams, const FZOnSearchSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SearchSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SearchSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SearchSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSearchSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SearchSept(const TSharedPtr<idlepb::SearchSeptReq>& InReqMessage, const OnSearchSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SearchSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SearchSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptBaseInfo(const FZGetSeptBaseInfoReq& InParams, const FZOnGetSeptBaseInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptBaseInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptBaseInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptBaseInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptBaseInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptBaseInfo(const TSharedPtr<idlepb::GetSeptBaseInfoReq>& InReqMessage, const OnGetSeptBaseInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptBaseInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptBaseInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptMemberList(const FZGetSeptMemberListReq& InParams, const FZOnGetSeptMemberListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptMemberListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptMemberList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptMemberListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptMemberListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptMemberList(const TSharedPtr<idlepb::GetSeptMemberListReq>& InReqMessage, const OnGetSeptMemberListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptMemberList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptMemberListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_CreateSept(const FZCreateSeptReq& InParams, const FZOnCreateSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::CreateSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    CreateSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::CreateSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZCreateSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::CreateSept(const TSharedPtr<idlepb::CreateSeptReq>& InReqMessage, const OnCreateSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::CreateSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::CreateSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DismissSept(const FZDismissSeptReq& InParams, const FZOnDismissSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DismissSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DismissSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DismissSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDismissSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DismissSept(const TSharedPtr<idlepb::DismissSeptReq>& InReqMessage, const OnDismissSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DismissSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DismissSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExitSept(const FZExitSeptReq& InParams, const FZOnExitSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ExitSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExitSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ExitSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZExitSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ExitSept(const TSharedPtr<idlepb::ExitSeptReq>& InReqMessage, const OnExitSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExitSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ExitSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ApplyJoinSept(const FZApplyJoinSeptReq& InParams, const FZOnApplyJoinSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ApplyJoinSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ApplyJoinSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ApplyJoinSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZApplyJoinSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ApplyJoinSept(const TSharedPtr<idlepb::ApplyJoinSeptReq>& InReqMessage, const OnApplyJoinSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ApplyJoinSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ApplyJoinSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ApproveApplySept(const FZApproveApplySeptReq& InParams, const FZOnApproveApplySeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ApproveApplySeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ApproveApplySept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ApproveApplySeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZApproveApplySeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ApproveApplySept(const TSharedPtr<idlepb::ApproveApplySeptReq>& InReqMessage, const OnApproveApplySeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ApproveApplySept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ApproveApplySeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetApplyJoinSeptList(const FZGetApplyJoinSeptListReq& InParams, const FZOnGetApplyJoinSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetApplyJoinSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetApplyJoinSeptList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetApplyJoinSeptListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetApplyJoinSeptListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetApplyJoinSeptList(const TSharedPtr<idlepb::GetApplyJoinSeptListReq>& InReqMessage, const OnGetApplyJoinSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetApplyJoinSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetApplyJoinSeptListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RespondInviteSept(const FZRespondInviteSeptReq& InParams, const FZOnRespondInviteSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RespondInviteSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RespondInviteSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RespondInviteSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRespondInviteSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RespondInviteSept(const TSharedPtr<idlepb::RespondInviteSeptReq>& InReqMessage, const OnRespondInviteSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RespondInviteSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RespondInviteSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetInviteMeJoinSeptList(const FZGetInviteMeJoinSeptListReq& InParams, const FZOnGetInviteMeJoinSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetInviteMeJoinSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetInviteMeJoinSeptList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetInviteMeJoinSeptListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetInviteMeJoinSeptListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetInviteMeJoinSeptList(const TSharedPtr<idlepb::GetInviteMeJoinSeptListReq>& InReqMessage, const OnGetInviteMeJoinSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetInviteMeJoinSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetInviteMeJoinSeptListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCandidatesInviteRoleList(const FZGetCandidatesInviteRoleListReq& InParams, const FZOnGetCandidatesInviteRoleListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetCandidatesInviteRoleListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCandidatesInviteRoleList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetCandidatesInviteRoleListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetCandidatesInviteRoleListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetCandidatesInviteRoleList(const TSharedPtr<idlepb::GetCandidatesInviteRoleListReq>& InReqMessage, const OnGetCandidatesInviteRoleListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesInviteRoleList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetCandidatesInviteRoleListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_InviteJoinSept(const FZInviteJoinSeptReq& InParams, const FZOnInviteJoinSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::InviteJoinSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    InviteJoinSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::InviteJoinSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZInviteJoinSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::InviteJoinSept(const TSharedPtr<idlepb::InviteJoinSeptReq>& InReqMessage, const OnInviteJoinSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::InviteJoinSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::InviteJoinSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetSeptSettings(const FZSetSeptSettingsReq& InParams, const FZOnSetSeptSettingsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetSeptSettingsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetSeptSettings(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetSeptSettingsAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetSeptSettingsAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetSeptSettings(const TSharedPtr<idlepb::SetSeptSettingsReq>& InReqMessage, const OnSetSeptSettingsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptSettings;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetSeptSettingsAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetSeptAnnounce(const FZSetSeptAnnounceReq& InParams, const FZOnSetSeptAnnounceResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::SetSeptAnnounceReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetSeptAnnounce(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::SetSeptAnnounceAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZSetSeptAnnounceAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::SetSeptAnnounce(const TSharedPtr<idlepb::SetSeptAnnounceReq>& InReqMessage, const OnSetSeptAnnounceResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptAnnounce;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::SetSeptAnnounceAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ChangeSeptName(const FZChangeSeptNameReq& InParams, const FZOnChangeSeptNameResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ChangeSeptNameReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ChangeSeptName(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ChangeSeptNameAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZChangeSeptNameAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ChangeSeptName(const TSharedPtr<idlepb::ChangeSeptNameReq>& InReqMessage, const OnChangeSeptNameResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ChangeSeptName;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ChangeSeptNameAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptLog(const FZGetSeptLogReq& InParams, const FZOnGetSeptLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptLog(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptLogAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptLogAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptLog(const TSharedPtr<idlepb::GetSeptLogReq>& InReqMessage, const OnGetSeptLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptLogAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ConstructSept(const FZConstructSeptReq& InParams, const FZOnConstructSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ConstructSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ConstructSept(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ConstructSeptAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZConstructSeptAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ConstructSept(const TSharedPtr<idlepb::ConstructSeptReq>& InReqMessage, const OnConstructSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ConstructSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ConstructSeptAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetConstructSeptLog(const FZGetConstructSeptLogReq& InParams, const FZOnGetConstructSeptLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetConstructSeptLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetConstructSeptLog(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetConstructSeptLogAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetConstructSeptLogAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetConstructSeptLog(const TSharedPtr<idlepb::GetConstructSeptLogReq>& InReqMessage, const OnGetConstructSeptLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetConstructSeptLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetConstructSeptLogAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptInvitedRoleDailyNum(const FZGetSeptInvitedRoleDailyNumReq& InParams, const FZOnGetSeptInvitedRoleDailyNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptInvitedRoleDailyNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptInvitedRoleDailyNum(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptInvitedRoleDailyNumAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptInvitedRoleDailyNumAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptInvitedRoleDailyNum(const TSharedPtr<idlepb::GetSeptInvitedRoleDailyNumReq>& InReqMessage, const OnGetSeptInvitedRoleDailyNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptInvitedRoleDailyNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptInvitedRoleDailyNumAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppointSeptPosition(const FZAppointSeptPositionReq& InParams, const FZOnAppointSeptPositionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppointSeptPositionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppointSeptPosition(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppointSeptPositionAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppointSeptPositionAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppointSeptPosition(const TSharedPtr<idlepb::AppointSeptPositionReq>& InReqMessage, const OnAppointSeptPositionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppointSeptPosition;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppointSeptPositionAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ResignSeptChairman(const FZResignSeptChairmanReq& InParams, const FZOnResignSeptChairmanResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ResignSeptChairmanReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ResignSeptChairman(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ResignSeptChairmanAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZResignSeptChairmanAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ResignSeptChairman(const TSharedPtr<idlepb::ResignSeptChairmanReq>& InReqMessage, const OnResignSeptChairmanResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ResignSeptChairman;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ResignSeptChairmanAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_KickOutSeptMember(const FZKickOutSeptMemberReq& InParams, const FZOnKickOutSeptMemberResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::KickOutSeptMemberReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    KickOutSeptMember(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::KickOutSeptMemberAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZKickOutSeptMemberAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::KickOutSeptMember(const TSharedPtr<idlepb::KickOutSeptMemberReq>& InReqMessage, const OnKickOutSeptMemberResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::KickOutSeptMember;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::KickOutSeptMemberAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleSeptShopData(const FZGetRoleSeptShopDataReq& InParams, const FZOnGetRoleSeptShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleSeptShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleSeptShopData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleSeptShopDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleSeptShopDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleSeptShopData(const TSharedPtr<idlepb::GetRoleSeptShopDataReq>& InReqMessage, const OnGetRoleSeptShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleSeptShopDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuySeptShopItem(const FZBuySeptShopItemReq& InParams, const FZOnBuySeptShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::BuySeptShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuySeptShopItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::BuySeptShopItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZBuySeptShopItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::BuySeptShopItem(const TSharedPtr<idlepb::BuySeptShopItemReq>& InReqMessage, const OnBuySeptShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuySeptShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::BuySeptShopItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleSeptQuestData(const FZGetRoleSeptQuestDataReq& InParams, const FZOnGetRoleSeptQuestDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleSeptQuestDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleSeptQuestData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleSeptQuestDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleSeptQuestDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleSeptQuestData(const TSharedPtr<idlepb::GetRoleSeptQuestDataReq>& InReqMessage, const OnGetRoleSeptQuestDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptQuestData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleSeptQuestDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReqRoleSeptQuestOp(const FZReqRoleSeptQuestOpReq& InParams, const FZOnReqRoleSeptQuestOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReqRoleSeptQuestOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReqRoleSeptQuestOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReqRoleSeptQuestOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReqRoleSeptQuestOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReqRoleSeptQuestOp(const TSharedPtr<idlepb::ReqRoleSeptQuestOpReq>& InReqMessage, const OnReqRoleSeptQuestOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReqRoleSeptQuestOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReqRoleSeptQuestOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshSeptQuest(const FZRefreshSeptQuestReq& InParams, const FZOnRefreshSeptQuestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RefreshSeptQuestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshSeptQuest(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RefreshSeptQuestAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRefreshSeptQuestAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RefreshSeptQuest(const TSharedPtr<idlepb::RefreshSeptQuestReq>& InReqMessage, const OnRefreshSeptQuestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshSeptQuest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RefreshSeptQuestAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReqSeptQuestRankUp(const FZReqSeptQuestRankUpReq& InParams, const FZOnReqSeptQuestRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReqSeptQuestRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReqSeptQuestRankUp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReqSeptQuestRankUpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReqSeptQuestRankUpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReqSeptQuestRankUp(const TSharedPtr<idlepb::ReqSeptQuestRankUpReq>& InReqMessage, const OnReqSeptQuestRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReqSeptQuestRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReqSeptQuestRankUpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BeginOccupySeptStone(const FZBeginOccupySeptStoneReq& InParams, const FZOnBeginOccupySeptStoneResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::BeginOccupySeptStoneReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BeginOccupySeptStone(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::BeginOccupySeptStoneAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZBeginOccupySeptStoneAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::BeginOccupySeptStone(const TSharedPtr<idlepb::BeginOccupySeptStoneReq>& InReqMessage, const OnBeginOccupySeptStoneResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BeginOccupySeptStone;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::BeginOccupySeptStoneAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EndOccupySeptStone(const FZEndOccupySeptStoneReq& InParams, const FZOnEndOccupySeptStoneResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::EndOccupySeptStoneReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EndOccupySeptStone(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::EndOccupySeptStoneAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZEndOccupySeptStoneAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::EndOccupySeptStone(const TSharedPtr<idlepb::EndOccupySeptStoneReq>& InReqMessage, const OnEndOccupySeptStoneResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EndOccupySeptStone;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::EndOccupySeptStoneAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OccupySeptLand(const FZOccupySeptLandReq& InParams, const FZOnOccupySeptLandResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OccupySeptLandReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OccupySeptLand(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OccupySeptLandAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOccupySeptLandAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OccupySeptLand(const TSharedPtr<idlepb::OccupySeptLandReq>& InReqMessage, const OnOccupySeptLandResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OccupySeptLand;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OccupySeptLandAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetGongFaData(const FZGetGongFaDataReq& InParams, const FZOnGetGongFaDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetGongFaDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetGongFaData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetGongFaDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetGongFaDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetGongFaData(const TSharedPtr<idlepb::GetGongFaDataReq>& InReqMessage, const OnGetGongFaDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetGongFaData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetGongFaDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GongFaOp(const FZGongFaOpReq& InParams, const FZOnGongFaOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GongFaOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GongFaOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GongFaOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGongFaOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GongFaOp(const TSharedPtr<idlepb::GongFaOpReq>& InReqMessage, const OnGongFaOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GongFaOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GongFaOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ActivateGongFaMaxEffect(const FZActivateGongFaMaxEffectReq& InParams, const FZOnActivateGongFaMaxEffectResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ActivateGongFaMaxEffectReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ActivateGongFaMaxEffect(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ActivateGongFaMaxEffectAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZActivateGongFaMaxEffectAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ActivateGongFaMaxEffect(const TSharedPtr<idlepb::ActivateGongFaMaxEffectReq>& InReqMessage, const OnActivateGongFaMaxEffectResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ActivateGongFaMaxEffect;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ActivateGongFaMaxEffectAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptLandDamageTopList(const FZGetSeptLandDamageTopListReq& InParams, const FZOnGetSeptLandDamageTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptLandDamageTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptLandDamageTopList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptLandDamageTopListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptLandDamageTopListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptLandDamageTopList(const TSharedPtr<idlepb::GetSeptLandDamageTopListReq>& InReqMessage, const OnGetSeptLandDamageTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLandDamageTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptLandDamageTopListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveFuZengRewards(const FZReceiveFuZengRewardsReq& InParams, const FZOnReceiveFuZengRewardsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReceiveFuZengRewardsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveFuZengRewards(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReceiveFuZengRewardsAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReceiveFuZengRewardsAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReceiveFuZengRewards(const TSharedPtr<idlepb::ReceiveFuZengRewardsReq>& InReqMessage, const OnReceiveFuZengRewardsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveFuZengRewards;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReceiveFuZengRewardsAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleFuZengData(const FZGetRoleFuZengDataReq& InParams, const FZOnGetRoleFuZengDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleFuZengDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleFuZengData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleFuZengDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleFuZengDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleFuZengData(const TSharedPtr<idlepb::GetRoleFuZengDataReq>& InReqMessage, const OnGetRoleFuZengDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFuZengData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleFuZengDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleTreasuryData(const FZGetRoleTreasuryDataReq& InParams, const FZOnGetRoleTreasuryDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleTreasuryDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleTreasuryData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleTreasuryDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleTreasuryDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleTreasuryData(const TSharedPtr<idlepb::GetRoleTreasuryDataReq>& InReqMessage, const OnGetRoleTreasuryDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleTreasuryData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleTreasuryDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OpenTreasuryChest(const FZOpenTreasuryChestReq& InParams, const FZOnOpenTreasuryChestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OpenTreasuryChestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OpenTreasuryChest(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OpenTreasuryChestAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOpenTreasuryChestAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OpenTreasuryChest(const TSharedPtr<idlepb::OpenTreasuryChestReq>& InReqMessage, const OnOpenTreasuryChestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryChest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OpenTreasuryChestAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickOpenTreasuryChest(const FZOneClickOpenTreasuryChestReq& InParams, const FZOnOneClickOpenTreasuryChestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OneClickOpenTreasuryChestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickOpenTreasuryChest(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OneClickOpenTreasuryChestAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOneClickOpenTreasuryChestAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OneClickOpenTreasuryChest(const TSharedPtr<idlepb::OneClickOpenTreasuryChestReq>& InReqMessage, const OnOneClickOpenTreasuryChestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickOpenTreasuryChest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OneClickOpenTreasuryChestAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OpenTreasuryGacha(const FZOpenTreasuryGachaReq& InParams, const FZOnOpenTreasuryGachaResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::OpenTreasuryGachaReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OpenTreasuryGacha(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::OpenTreasuryGachaAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZOpenTreasuryGachaAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::OpenTreasuryGacha(const TSharedPtr<idlepb::OpenTreasuryGachaReq>& InReqMessage, const OnOpenTreasuryGachaResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryGacha;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::OpenTreasuryGachaAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshTreasuryShop(const FZRefreshTreasuryShopReq& InParams, const FZOnRefreshTreasuryShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RefreshTreasuryShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshTreasuryShop(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RefreshTreasuryShopAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRefreshTreasuryShopAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RefreshTreasuryShop(const TSharedPtr<idlepb::RefreshTreasuryShopReq>& InReqMessage, const OnRefreshTreasuryShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshTreasuryShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RefreshTreasuryShopAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_TreasuryShopBuy(const FZTreasuryShopBuyReq& InParams, const FZOnTreasuryShopBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::TreasuryShopBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    TreasuryShopBuy(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::TreasuryShopBuyAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZTreasuryShopBuyAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::TreasuryShopBuy(const TSharedPtr<idlepb::TreasuryShopBuyReq>& InReqMessage, const OnTreasuryShopBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::TreasuryShopBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::TreasuryShopBuyAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLifeCounterData(const FZGetLifeCounterDataReq& InParams, const FZOnGetLifeCounterDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetLifeCounterDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLifeCounterData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetLifeCounterDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetLifeCounterDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetLifeCounterData(const TSharedPtr<idlepb::GetLifeCounterDataReq>& InReqMessage, const OnGetLifeCounterDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLifeCounterData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetLifeCounterDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DoQuestFight(const FZDoQuestFightReq& InParams, const FZOnDoQuestFightResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DoQuestFightReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DoQuestFight(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DoQuestFightAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDoQuestFightAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DoQuestFight(const TSharedPtr<idlepb::DoQuestFightReq>& InReqMessage, const OnDoQuestFightResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DoQuestFight;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DoQuestFightAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QuestFightQuickEnd(const FZQuestFightQuickEndReq& InParams, const FZOnQuestFightQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::QuestFightQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QuestFightQuickEnd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::QuestFightQuickEndAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZQuestFightQuickEndAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::QuestFightQuickEnd(const TSharedPtr<idlepb::QuestFightQuickEndReq>& InReqMessage, const OnQuestFightQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QuestFightQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::QuestFightQuickEndAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetAppearanceData(const FZGetAppearanceDataReq& InParams, const FZOnGetAppearanceDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetAppearanceDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetAppearanceData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetAppearanceDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetAppearanceDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetAppearanceData(const TSharedPtr<idlepb::GetAppearanceDataReq>& InReqMessage, const OnGetAppearanceDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetAppearanceData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetAppearanceDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceAdd(const FZAppearanceAddReq& InParams, const FZOnAppearanceAddResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppearanceAddReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceAdd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppearanceAddAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppearanceAddAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppearanceAdd(const TSharedPtr<idlepb::AppearanceAddReq>& InReqMessage, const OnAppearanceAddResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceAdd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppearanceAddAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceActive(const FZAppearanceActiveReq& InParams, const FZOnAppearanceActiveResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppearanceActiveReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceActive(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppearanceActiveAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppearanceActiveAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppearanceActive(const TSharedPtr<idlepb::AppearanceActiveReq>& InReqMessage, const OnAppearanceActiveResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceActive;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppearanceActiveAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceWear(const FZAppearanceWearReq& InParams, const FZOnAppearanceWearResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppearanceWearReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceWear(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppearanceWearAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppearanceWearAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppearanceWear(const TSharedPtr<idlepb::AppearanceWearReq>& InReqMessage, const OnAppearanceWearResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceWear;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppearanceWearAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceBuy(const FZAppearanceBuyReq& InParams, const FZOnAppearanceBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppearanceBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceBuy(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppearanceBuyAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppearanceBuyAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppearanceBuy(const TSharedPtr<idlepb::AppearanceBuyReq>& InReqMessage, const OnAppearanceBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppearanceBuyAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceChangeSkType(const FZAppearanceChangeSkTypeReq& InParams, const FZOnAppearanceChangeSkTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AppearanceChangeSkTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceChangeSkType(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AppearanceChangeSkTypeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAppearanceChangeSkTypeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AppearanceChangeSkType(const TSharedPtr<idlepb::AppearanceChangeSkTypeReq>& InReqMessage, const OnAppearanceChangeSkTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceChangeSkType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AppearanceChangeSkTypeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBattleHistoryInfo(const FZGetBattleHistoryInfoReq& InParams, const FZOnGetBattleHistoryInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetBattleHistoryInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBattleHistoryInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetBattleHistoryInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetBattleHistoryInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetBattleHistoryInfo(const TSharedPtr<idlepb::GetBattleHistoryInfoReq>& InReqMessage, const OnGetBattleHistoryInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBattleHistoryInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetBattleHistoryInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetArenaCheckListData(const FZGetArenaCheckListDataReq& InParams, const FZOnGetArenaCheckListDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetArenaCheckListDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetArenaCheckListData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetArenaCheckListDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetArenaCheckListDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetArenaCheckListData(const TSharedPtr<idlepb::GetArenaCheckListDataReq>& InReqMessage, const OnGetArenaCheckListDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaCheckListData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetArenaCheckListDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ArenaCheckListSubmit(const FZArenaCheckListSubmitReq& InParams, const FZOnArenaCheckListSubmitResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ArenaCheckListSubmitReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ArenaCheckListSubmit(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ArenaCheckListSubmitAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZArenaCheckListSubmitAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ArenaCheckListSubmit(const TSharedPtr<idlepb::ArenaCheckListSubmitReq>& InReqMessage, const OnArenaCheckListSubmitResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListSubmit;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ArenaCheckListSubmitAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ArenaCheckListRewardSubmit(const FZArenaCheckListRewardSubmitReq& InParams, const FZOnArenaCheckListRewardSubmitResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ArenaCheckListRewardSubmitReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ArenaCheckListRewardSubmit(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ArenaCheckListRewardSubmitAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZArenaCheckListRewardSubmitAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ArenaCheckListRewardSubmit(const TSharedPtr<idlepb::ArenaCheckListRewardSubmitReq>& InReqMessage, const OnArenaCheckListRewardSubmitResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListRewardSubmit;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ArenaCheckListRewardSubmitAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllChallenge(const FZDungeonKillAllChallengeReq& InParams, const FZOnDungeonKillAllChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonKillAllChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllChallenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonKillAllChallengeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonKillAllChallengeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonKillAllChallenge(const TSharedPtr<idlepb::DungeonKillAllChallengeReq>& InReqMessage, const OnDungeonKillAllChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonKillAllChallengeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllQuickEnd(const FZDungeonKillAllQuickEndReq& InParams, const FZOnDungeonKillAllQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonKillAllQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllQuickEnd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonKillAllQuickEndAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonKillAllQuickEndAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonKillAllQuickEnd(const TSharedPtr<idlepb::DungeonKillAllQuickEndReq>& InReqMessage, const OnDungeonKillAllQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonKillAllQuickEndAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllData(const FZDungeonKillAllDataReq& InParams, const FZOnDungeonKillAllDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonKillAllDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonKillAllDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonKillAllDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonKillAllData(const TSharedPtr<idlepb::DungeonKillAllDataReq>& InReqMessage, const OnDungeonKillAllDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonKillAllDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetFarmlandData(const FZGetFarmlandDataReq& InParams, const FZOnGetFarmlandDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetFarmlandDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetFarmlandData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetFarmlandDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetFarmlandDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetFarmlandData(const TSharedPtr<idlepb::GetFarmlandDataReq>& InReqMessage, const OnGetFarmlandDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetFarmlandData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetFarmlandDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandUnlockBlock(const FZFarmlandUnlockBlockReq& InParams, const FZOnFarmlandUnlockBlockResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandUnlockBlockReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandUnlockBlock(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandUnlockBlockAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandUnlockBlockAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandUnlockBlock(const TSharedPtr<idlepb::FarmlandUnlockBlockReq>& InReqMessage, const OnFarmlandUnlockBlockResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandUnlockBlock;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandUnlockBlockAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandPlantSeed(const FZFarmlandPlantSeedReq& InParams, const FZOnFarmlandPlantSeedResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandPlantSeedReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandPlantSeed(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandPlantSeedAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandPlantSeedAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandPlantSeed(const TSharedPtr<idlepb::FarmlandPlantSeedReq>& InReqMessage, const OnFarmlandPlantSeedResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandPlantSeed;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandPlantSeedAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandWatering(const FZFarmlandWateringReq& InParams, const FZOnFarmlandWateringResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandWateringReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandWatering(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandWateringAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandWateringAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandWatering(const TSharedPtr<idlepb::FarmlandWateringReq>& InReqMessage, const OnFarmlandWateringResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandWatering;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandWateringAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandRipening(const FZFarmlandRipeningReq& InParams, const FZOnFarmlandRipeningResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandRipeningReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandRipening(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandRipeningAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandRipeningAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandRipening(const TSharedPtr<idlepb::FarmlandRipeningReq>& InReqMessage, const OnFarmlandRipeningResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandRipening;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandRipeningAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandHarvest(const FZFarmlandHarvestReq& InParams, const FZOnFarmlandHarvestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandHarvestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandHarvest(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandHarvestAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandHarvestAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandHarvest(const TSharedPtr<idlepb::FarmlandHarvestReq>& InReqMessage, const OnFarmlandHarvestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandHarvest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandHarvestAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmerRankUp(const FZFarmerRankUpReq& InParams, const FZOnFarmerRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmerRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmerRankUp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmerRankUpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmerRankUpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmerRankUp(const TSharedPtr<idlepb::FarmerRankUpReq>& InReqMessage, const OnFarmerRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmerRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmerRankUpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandSetManagement(const FZFarmlandSetManagementReq& InParams, const FZOnFarmlandSetManagementResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FarmlandSetManagementReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandSetManagement(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FarmlandSetManagementAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFarmlandSetManagementAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FarmlandSetManagement(const TSharedPtr<idlepb::FarmlandSetManagementReq>& InReqMessage, const OnFarmlandSetManagementResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandSetManagement;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FarmlandSetManagementAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpdateFarmlandState(const FZUpdateFarmlandStateReq& InParams, const FZOnUpdateFarmlandStateResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::UpdateFarmlandStateReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpdateFarmlandState(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::UpdateFarmlandStateAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZUpdateFarmlandStateAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::UpdateFarmlandState(const TSharedPtr<idlepb::UpdateFarmlandStateReq>& InReqMessage, const OnUpdateFarmlandStateResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateFarmlandState;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::UpdateFarmlandStateAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveChallenge(const FZDungeonSurviveChallengeReq& InParams, const FZOnDungeonSurviveChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonSurviveChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveChallenge(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonSurviveChallengeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonSurviveChallengeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonSurviveChallenge(const TSharedPtr<idlepb::DungeonSurviveChallengeReq>& InReqMessage, const OnDungeonSurviveChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonSurviveChallengeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveQuickEnd(const FZDungeonSurviveQuickEndReq& InParams, const FZOnDungeonSurviveQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonSurviveQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveQuickEnd(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonSurviveQuickEndAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonSurviveQuickEndAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonSurviveQuickEnd(const TSharedPtr<idlepb::DungeonSurviveQuickEndReq>& InReqMessage, const OnDungeonSurviveQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonSurviveQuickEndAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveData(const FZDungeonSurviveDataReq& InParams, const FZOnDungeonSurviveDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DungeonSurviveDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DungeonSurviveDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDungeonSurviveDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DungeonSurviveData(const TSharedPtr<idlepb::DungeonSurviveDataReq>& InReqMessage, const OnDungeonSurviveDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DungeonSurviveDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRevertAllSkillCoolDown(const FZGetRevertAllSkillCoolDownReq& InParams, const FZOnGetRevertAllSkillCoolDownResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRevertAllSkillCoolDownReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRevertAllSkillCoolDown(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRevertAllSkillCoolDownAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRevertAllSkillCoolDownAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRevertAllSkillCoolDown(const TSharedPtr<idlepb::GetRevertAllSkillCoolDownReq>& InReqMessage, const OnGetRevertAllSkillCoolDownResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRevertAllSkillCoolDown;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRevertAllSkillCoolDownAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleFriendData(const FZGetRoleFriendDataReq& InParams, const FZOnGetRoleFriendDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleFriendDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleFriendData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleFriendDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleFriendDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleFriendData(const TSharedPtr<idlepb::GetRoleFriendDataReq>& InReqMessage, const OnGetRoleFriendDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFriendData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleFriendDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FriendOp(const FZFriendOpReq& InParams, const FZOnFriendOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FriendOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FriendOp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FriendOpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFriendOpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FriendOp(const TSharedPtr<idlepb::FriendOpReq>& InReqMessage, const OnFriendOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FriendOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FriendOpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReplyFriendRequest(const FZReplyFriendRequestReq& InParams, const FZOnReplyFriendRequestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReplyFriendRequestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReplyFriendRequest(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReplyFriendRequestAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReplyFriendRequestAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReplyFriendRequest(const TSharedPtr<idlepb::ReplyFriendRequestReq>& InReqMessage, const OnReplyFriendRequestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReplyFriendRequest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReplyFriendRequestAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FriendSearchRoleInfo(const FZFriendSearchRoleInfoReq& InParams, const FZOnFriendSearchRoleInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::FriendSearchRoleInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FriendSearchRoleInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::FriendSearchRoleInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZFriendSearchRoleInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::FriendSearchRoleInfo(const TSharedPtr<idlepb::FriendSearchRoleInfoReq>& InReqMessage, const OnFriendSearchRoleInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FriendSearchRoleInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::FriendSearchRoleInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleInfoCache(const FZGetRoleInfoCacheReq& InParams, const FZOnGetRoleInfoCacheResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleInfoCacheReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleInfoCache(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleInfoCacheAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleInfoCacheAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleInfoCache(const TSharedPtr<idlepb::GetRoleInfoCacheReq>& InReqMessage, const OnGetRoleInfoCacheResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfoCache;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleInfoCacheAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleInfo(const FZGetRoleInfoReq& InParams, const FZOnGetRoleInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleInfo(const TSharedPtr<idlepb::GetRoleInfoReq>& InReqMessage, const OnGetRoleInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleAvatarData(const FZGetRoleAvatarDataReq& InParams, const FZOnGetRoleAvatarDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleAvatarDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleAvatarData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleAvatarDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleAvatarDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleAvatarData(const TSharedPtr<idlepb::GetRoleAvatarDataReq>& InReqMessage, const OnGetRoleAvatarDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAvatarData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleAvatarDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DispatchAvatar(const FZDispatchAvatarReq& InParams, const FZOnDispatchAvatarResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::DispatchAvatarReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DispatchAvatar(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::DispatchAvatarAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZDispatchAvatarAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::DispatchAvatar(const TSharedPtr<idlepb::DispatchAvatarReq>& InReqMessage, const OnDispatchAvatarResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DispatchAvatar;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::DispatchAvatarAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AvatarRankUp(const FZAvatarRankUpReq& InParams, const FZOnAvatarRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AvatarRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AvatarRankUp(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AvatarRankUpAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAvatarRankUpAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AvatarRankUp(const TSharedPtr<idlepb::AvatarRankUpReq>& InReqMessage, const OnAvatarRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AvatarRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AvatarRankUpAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveAvatarTempPackage(const FZReceiveAvatarTempPackageReq& InParams, const FZOnReceiveAvatarTempPackageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReceiveAvatarTempPackageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveAvatarTempPackage(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReceiveAvatarTempPackageAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReceiveAvatarTempPackageAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReceiveAvatarTempPackage(const TSharedPtr<idlepb::ReceiveAvatarTempPackageReq>& InReqMessage, const OnReceiveAvatarTempPackageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveAvatarTempPackage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReceiveAvatarTempPackageAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetArenaExplorationStatisticalData(const FZGetArenaExplorationStatisticalDataReq& InParams, const FZOnGetArenaExplorationStatisticalDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetArenaExplorationStatisticalDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetArenaExplorationStatisticalData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetArenaExplorationStatisticalDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetArenaExplorationStatisticalDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetArenaExplorationStatisticalData(const TSharedPtr<idlepb::GetArenaExplorationStatisticalDataReq>& InReqMessage, const OnGetArenaExplorationStatisticalDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaExplorationStatisticalData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetArenaExplorationStatisticalDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleBiographyData(const FZGetRoleBiographyDataReq& InParams, const FZOnGetRoleBiographyDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleBiographyDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleBiographyData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleBiographyDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleBiographyDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleBiographyData(const TSharedPtr<idlepb::GetRoleBiographyDataReq>& InReqMessage, const OnGetRoleBiographyDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleBiographyData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleBiographyDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBiographyItem(const FZReceiveBiographyItemReq& InParams, const FZOnReceiveBiographyItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReceiveBiographyItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBiographyItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReceiveBiographyItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReceiveBiographyItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReceiveBiographyItem(const TSharedPtr<idlepb::ReceiveBiographyItemReq>& InReqMessage, const OnReceiveBiographyItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReceiveBiographyItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBiographyEventData(const FZGetBiographyEventDataReq& InParams, const FZOnGetBiographyEventDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetBiographyEventDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBiographyEventData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetBiographyEventDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetBiographyEventDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetBiographyEventData(const TSharedPtr<idlepb::GetBiographyEventDataReq>& InReqMessage, const OnGetBiographyEventDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBiographyEventData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetBiographyEventDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBiographyEventItem(const FZReceiveBiographyEventItemReq& InParams, const FZOnReceiveBiographyEventItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::ReceiveBiographyEventItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBiographyEventItem(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::ReceiveBiographyEventItemAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZReceiveBiographyEventItemAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::ReceiveBiographyEventItem(const TSharedPtr<idlepb::ReceiveBiographyEventItemReq>& InReqMessage, const OnReceiveBiographyEventItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyEventItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::ReceiveBiographyEventItemAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AddBiographyRoleLog(const FZAddBiographyRoleLogReq& InParams, const FZOnAddBiographyRoleLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::AddBiographyRoleLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AddBiographyRoleLog(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::AddBiographyRoleLogAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZAddBiographyRoleLogAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::AddBiographyRoleLog(const TSharedPtr<idlepb::AddBiographyRoleLogReq>& InReqMessage, const OnAddBiographyRoleLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AddBiographyRoleLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::AddBiographyRoleLogAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestEnterSeptDemonWorld(const FZRequestEnterSeptDemonWorldReq& InParams, const FZOnRequestEnterSeptDemonWorldResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestEnterSeptDemonWorldReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestEnterSeptDemonWorld(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestEnterSeptDemonWorldAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestEnterSeptDemonWorldAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestEnterSeptDemonWorld(const TSharedPtr<idlepb::RequestEnterSeptDemonWorldReq>& InReqMessage, const OnRequestEnterSeptDemonWorldResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestEnterSeptDemonWorld;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestEnterSeptDemonWorldAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestLeaveSeptDemonWorld(const FZRequestLeaveSeptDemonWorldReq& InParams, const FZOnRequestLeaveSeptDemonWorldResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestLeaveSeptDemonWorldReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestLeaveSeptDemonWorld(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestLeaveSeptDemonWorldAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestLeaveSeptDemonWorldAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestLeaveSeptDemonWorld(const TSharedPtr<idlepb::RequestLeaveSeptDemonWorldReq>& InReqMessage, const OnRequestLeaveSeptDemonWorldResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestLeaveSeptDemonWorld;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestLeaveSeptDemonWorldAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestSeptDemonWorldData(const FZRequestSeptDemonWorldDataReq& InParams, const FZOnRequestSeptDemonWorldDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestSeptDemonWorldDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestSeptDemonWorldData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestSeptDemonWorldDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestSeptDemonWorldDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestSeptDemonWorldData(const TSharedPtr<idlepb::RequestSeptDemonWorldDataReq>& InReqMessage, const OnRequestSeptDemonWorldDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestSeptDemonWorldData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestSeptDemonWorldDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestInSeptDemonWorldEndTime(const FZRequestInSeptDemonWorldEndTimeReq& InParams, const FZOnRequestInSeptDemonWorldEndTimeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::RequestInSeptDemonWorldEndTimeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestInSeptDemonWorldEndTime(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::RequestInSeptDemonWorldEndTimeAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZRequestInSeptDemonWorldEndTimeAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::RequestInSeptDemonWorldEndTime(const TSharedPtr<idlepb::RequestInSeptDemonWorldEndTimeReq>& InReqMessage, const OnRequestInSeptDemonWorldEndTimeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestInSeptDemonWorldEndTime;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::RequestInSeptDemonWorldEndTimeAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageTopList(const FZGetSeptDemonDamageTopListReq& InParams, const FZOnGetSeptDemonDamageTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageTopList(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageTopListAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonDamageTopListAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonDamageTopList(const TSharedPtr<idlepb::GetSeptDemonDamageTopListReq>& InReqMessage, const OnGetSeptDemonDamageTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageTopListAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageSelfSummary(const FZGetSeptDemonDamageSelfSummaryReq& InParams, const FZOnGetSeptDemonDamageSelfSummaryResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageSelfSummaryReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageSelfSummary(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageSelfSummaryAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonDamageSelfSummaryAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonDamageSelfSummary(const TSharedPtr<idlepb::GetSeptDemonDamageSelfSummaryReq>& InReqMessage, const OnGetSeptDemonDamageSelfSummaryResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageSelfSummary;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageSelfSummaryAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonStageRewardNum(const FZGetSeptDemonStageRewardNumReq& InParams, const FZOnGetSeptDemonStageRewardNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonStageRewardNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonStageRewardNum(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonStageRewardNumAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonStageRewardNumAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonStageRewardNum(const TSharedPtr<idlepb::GetSeptDemonStageRewardNumReq>& InReqMessage, const OnGetSeptDemonStageRewardNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageRewardNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonStageRewardNumAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonStageReward(const FZGetSeptDemonStageRewardReq& InParams, const FZOnGetSeptDemonStageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonStageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonStageReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonStageRewardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonStageRewardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonStageReward(const TSharedPtr<idlepb::GetSeptDemonStageRewardReq>& InReqMessage, const OnGetSeptDemonStageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonStageRewardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageRewardsInfo(const FZGetSeptDemonDamageRewardsInfoReq& InParams, const FZOnGetSeptDemonDamageRewardsInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageRewardsInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageRewardsInfo(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageRewardsInfoAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonDamageRewardsInfoAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonDamageRewardsInfo(const TSharedPtr<idlepb::GetSeptDemonDamageRewardsInfoReq>& InReqMessage, const OnGetSeptDemonDamageRewardsInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageRewardsInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageRewardsInfoAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageReward(const FZGetSeptDemonDamageRewardReq& InParams, const FZOnGetSeptDemonDamageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageReward(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetSeptDemonDamageRewardAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetSeptDemonDamageRewardAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetSeptDemonDamageReward(const TSharedPtr<idlepb::GetSeptDemonDamageRewardReq>& InReqMessage, const OnGetSeptDemonDamageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageRewardAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleVipShopData(const FZGetRoleVipShopDataReq& InParams, const FZOnGetRoleVipShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::GetRoleVipShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleVipShopData(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::GetRoleVipShopDataAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZGetRoleVipShopDataAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::GetRoleVipShopData(const TSharedPtr<idlepb::GetRoleVipShopDataReq>& InReqMessage, const OnGetRoleVipShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleVipShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::GetRoleVipShopDataAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_VipShopBuy(const FZVipShopBuyReq& InParams, const FZOnVipShopBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlepb::VipShopBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    VipShopBuy(ReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::VipShopBuyAck>& InRspMessage)
    {        
        const UObject* Owner = InCallback.GetUObject();
        if (IsValid(Owner))
        {
            FZVipShopBuyAck Rsp;
            if (InRspMessage)
            {
                Rsp = *InRspMessage;  // PB -> USTRUCT
            }        
            
            if (InCallback.ExecuteIfBound(ErrorCode, Rsp)) {}
        }
    });
}

void UZGameRpcStub::VipShopBuy(const TSharedPtr<idlepb::VipShopBuyReq>& InReqMessage, const OnVipShopBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::VipShopBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EPbRpcErrorCode ErrorCode, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlepb::VipShopBuyAck>();               
        if (ErrorCode == EPbRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EPbRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}




































       


