#include "GameRpcStub.h"
#include "GameRpcInterface.h"
#include "MRpcManager.h"

void UZGameRpcStub::Setup(FMRpcManager* InManager, const FZPbConnectionPtr& InConn)
{
    if (Manager)
    {
        Cleanup();
    }

    Manager = InManager;
    Connection = InConn;

    if (Manager)
    {
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyAlchemyRefineResult>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyAlchemyRefineResult>& InMessage)
        {
            if (OnNotifyAlchemyRefineResult.IsBound())
            {
                FZNotifyAlchemyRefineResult Result = *InMessage;
                OnNotifyAlchemyRefineResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::RefreshItems>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::RefreshItems>& InMessage)
        {
            if (OnRefreshItems.IsBound())
            {
                FZRefreshItems Result = *InMessage;
                OnRefreshItems.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyInventorySpaceNum>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyInventorySpaceNum>& InMessage)
        {
            if (OnNotifyInventorySpaceNum.IsBound())
            {
                FZNotifyInventorySpaceNum Result = *InMessage;
                OnNotifyInventorySpaceNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::RefreshUnlockedEquipmentSlots>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::RefreshUnlockedEquipmentSlots>& InMessage)
        {
            if (OnRefreshUnlockedEquipmentSlots.IsBound())
            {
                FZRefreshUnlockedEquipmentSlots Result = *InMessage;
                OnRefreshUnlockedEquipmentSlots.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyUnlockArenaChallengeResult>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyUnlockArenaChallengeResult>& InMessage)
        {
            if (OnNotifyUnlockArenaChallengeResult.IsBound())
            {
                FZNotifyUnlockArenaChallengeResult Result = *InMessage;
                OnNotifyUnlockArenaChallengeResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::UpdateRoleMail>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::UpdateRoleMail>& InMessage)
        {
            if (OnUpdateRoleMail.IsBound())
            {
                FZUpdateRoleMail Result = *InMessage;
                OnUpdateRoleMail.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyForgeRefineResult>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyForgeRefineResult>& InMessage)
        {
            if (OnNotifyForgeRefineResult.IsBound())
            {
                FZNotifyForgeRefineResult Result = *InMessage;
                OnNotifyForgeRefineResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyGiftPackageResult>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyGiftPackageResult>& InMessage)
        {
            if (OnNotifyGiftPackageResult.IsBound())
            {
                FZNotifyGiftPackageResult Result = *InMessage;
                OnNotifyGiftPackageResult.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyUsePillProperty>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyUsePillProperty>& InMessage)
        {
            if (OnNotifyUsePillProperty.IsBound())
            {
                FZNotifyUsePillProperty Result = *InMessage;
                OnNotifyUsePillProperty.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyInventoryFullMailItem>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyInventoryFullMailItem>& InMessage)
        {
            if (OnNotifyInventoryFullMailItem.IsBound())
            {
                FZNotifyInventoryFullMailItem Result = *InMessage;
                OnNotifyInventoryFullMailItem.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyRoleCollectionData>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyRoleCollectionData>& InMessage)
        {
            if (OnNotifyRoleCollectionData.IsBound())
            {
                FZNotifyRoleCollectionData Result = *InMessage;
                OnNotifyRoleCollectionData.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyCommonCollectionPieceData>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyCommonCollectionPieceData>& InMessage)
        {
            if (OnNotifyCommonCollectionPieceData.IsBound())
            {
                FZNotifyCommonCollectionPieceData Result = *InMessage;
                OnNotifyCommonCollectionPieceData.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyCollectionActivatedSuit>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyCollectionActivatedSuit>& InMessage)
        {
            if (OnNotifyCollectionActivatedSuit.IsBound())
            {
                FZNotifyCollectionActivatedSuit Result = *InMessage;
                OnNotifyCollectionActivatedSuit.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyRoleCollectionHistories>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyRoleCollectionHistories>& InMessage)
        {
            if (OnNotifyRoleCollectionHistories.IsBound())
            {
                FZNotifyRoleCollectionHistories Result = *InMessage;
                OnNotifyRoleCollectionHistories.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyCollectionZoneActiveAwards>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyCollectionZoneActiveAwards>& InMessage)
        {
            if (OnNotifyCollectionZoneActiveAwards.IsBound())
            {
                FZNotifyCollectionZoneActiveAwards Result = *InMessage;
                OnNotifyCollectionZoneActiveAwards.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyRoleCollectionNextResetEnhanceTicks>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyRoleCollectionNextResetEnhanceTicks>& InMessage)
        {
            if (OnNotifyRoleCollectionNextResetEnhanceTicks.IsBound())
            {
                FZNotifyRoleCollectionNextResetEnhanceTicks Result = *InMessage;
                OnNotifyRoleCollectionNextResetEnhanceTicks.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyBossInvasionNpcKilled>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyBossInvasionNpcKilled>& InMessage)
        {
            if (OnNotifyBossInvasionNpcKilled.IsBound())
            {
                FZNotifyBossInvasionNpcKilled Result = *InMessage;
                OnNotifyBossInvasionNpcKilled.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyChecklist>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyChecklist>& InMessage)
        {
            if (OnNotifyChecklist.IsBound())
            {
                FZNotifyChecklist Result = *InMessage;
                OnNotifyChecklist.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifySeptStoneOccupyEnd>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifySeptStoneOccupyEnd>& InMessage)
        {
            if (OnNotifySeptStoneOccupyEnd.IsBound())
            {
                FZNotifySeptStoneOccupyEnd Result = *InMessage;
                OnNotifySeptStoneOccupyEnd.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyTeleportFailed>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyTeleportFailed>& InMessage)
        {
            if (OnNotifyTeleportFailed.IsBound())
            {
                FZNotifyTeleportFailed Result = *InMessage;
                OnNotifyTeleportFailed.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyFuZeng>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyFuZeng>& InMessage)
        {
            if (OnNotifyFuZeng.IsBound())
            {
                FZNotifyFuZeng Result = *InMessage;
                OnNotifyFuZeng.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::UpdateLifeCounter>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::UpdateLifeCounter>& InMessage)
        {
            if (OnUpdateLifeCounter.IsBound())
            {
                FZUpdateLifeCounter Result = *InMessage;
                OnUpdateLifeCounter.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyQuestFightChallengeOver>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyQuestFightChallengeOver>& InMessage)
        {
            if (OnNotifyQuestFightChallengeOver.IsBound())
            {
                FZNotifyQuestFightChallengeOver Result = *InMessage;
                OnNotifyQuestFightChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::DungeonChallengeOver>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::DungeonChallengeOver>& InMessage)
        {
            if (OnDungeonChallengeOver.IsBound())
            {
                FZDungeonChallengeOver Result = *InMessage;
                OnDungeonChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifySoloArenaChallengeOver>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifySoloArenaChallengeOver>& InMessage)
        {
            if (OnNotifySoloArenaChallengeOver.IsBound())
            {
                FZNotifySoloArenaChallengeOver Result = *InMessage;
                OnNotifySoloArenaChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::UpdateChat>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::UpdateChat>& InMessage)
        {
            if (OnUpdateChat.IsBound())
            {
                FZUpdateChat Result = *InMessage;
                OnUpdateChat.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyDungeonKillAllChallengeCurWaveNum>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyDungeonKillAllChallengeCurWaveNum>& InMessage)
        {
            if (OnNotifyDungeonKillAllChallengeCurWaveNum.IsBound())
            {
                FZNotifyDungeonKillAllChallengeCurWaveNum Result = *InMessage;
                OnNotifyDungeonKillAllChallengeCurWaveNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyDungeonKillAllChallengeOver>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyDungeonKillAllChallengeOver>& InMessage)
        {
            if (OnNotifyDungeonKillAllChallengeOver.IsBound())
            {
                FZNotifyDungeonKillAllChallengeOver Result = *InMessage;
                OnNotifyDungeonKillAllChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyDungeonSurviveChallengeCurWaveNum>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyDungeonSurviveChallengeCurWaveNum>& InMessage)
        {
            if (OnNotifyDungeonSurviveChallengeCurWaveNum.IsBound())
            {
                FZNotifyDungeonSurviveChallengeCurWaveNum Result = *InMessage;
                OnNotifyDungeonSurviveChallengeCurWaveNum.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyDungeonSurviveChallengeOver>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyDungeonSurviveChallengeOver>& InMessage)
        {
            if (OnNotifyDungeonSurviveChallengeOver.IsBound())
            {
                FZNotifyDungeonSurviveChallengeOver Result = *InMessage;
                OnNotifyDungeonSurviveChallengeOver.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyFriendMessage>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyFriendMessage>& InMessage)
        {
            if (OnNotifyFriendMessage.IsBound())
            {
                FZNotifyFriendMessage Result = *InMessage;
                OnNotifyFriendMessage.Broadcast(Result);
            }
        });
        Manager->GetMessageDispatcher().Reg<idlezt::NotifyBiographyMessage>([this](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::NotifyBiographyMessage>& InMessage)
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
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyAlchemyRefineResult>();
        Manager->GetMessageDispatcher().UnReg<idlezt::RefreshItems>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyInventorySpaceNum>();
        Manager->GetMessageDispatcher().UnReg<idlezt::RefreshUnlockedEquipmentSlots>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyUnlockArenaChallengeResult>();
        Manager->GetMessageDispatcher().UnReg<idlezt::UpdateRoleMail>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyForgeRefineResult>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyGiftPackageResult>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyUsePillProperty>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyInventoryFullMailItem>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyRoleCollectionData>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyCommonCollectionPieceData>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyCollectionActivatedSuit>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyRoleCollectionHistories>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyCollectionZoneActiveAwards>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyRoleCollectionNextResetEnhanceTicks>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyBossInvasionNpcKilled>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyChecklist>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifySeptStoneOccupyEnd>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyTeleportFailed>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyFuZeng>();
        Manager->GetMessageDispatcher().UnReg<idlezt::UpdateLifeCounter>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyQuestFightChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlezt::DungeonChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifySoloArenaChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlezt::UpdateChat>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyDungeonKillAllChallengeCurWaveNum>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyDungeonKillAllChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyDungeonSurviveChallengeCurWaveNum>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyDungeonSurviveChallengeOver>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyFriendMessage>();
        Manager->GetMessageDispatcher().UnReg<idlezt::NotifyBiographyMessage>();        
    }
    Manager = nullptr;
    Connection = nullptr;    
}


void UZGameRpcStub::K2_LoginGame(const FZLoginGameReq& InParams, const FZOnLoginGameResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::LoginGameReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LoginGame(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::LoginGameAck>& InRspMessage)
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

void UZGameRpcStub::LoginGame(const TSharedPtr<idlezt::LoginGameReq>& InReqMessage, const OnLoginGameResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LoginGame;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::LoginGameAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetCurrentCultivationDirection(const FZSetCurrentCultivationDirectionReq& InParams, const FZOnSetCurrentCultivationDirectionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetCurrentCultivationDirectionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetCurrentCultivationDirection(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetCurrentCultivationDirectionAck>& InRspMessage)
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

void UZGameRpcStub::SetCurrentCultivationDirection(const TSharedPtr<idlezt::SetCurrentCultivationDirectionReq>& InReqMessage, const OnSetCurrentCultivationDirectionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetCurrentCultivationDirection;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetCurrentCultivationDirectionAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DoBreakthrough(const FZDoBreakthroughReq& InParams, const FZOnDoBreakthroughResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DoBreakthroughReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DoBreakthrough(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DoBreakthroughAck>& InRspMessage)
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

void UZGameRpcStub::DoBreakthrough(const TSharedPtr<idlezt::DoBreakthroughReq>& InReqMessage, const OnDoBreakthroughResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DoBreakthrough;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DoBreakthroughAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestCommonCultivationData(const FZRequestCommonCultivationDataReq& InParams, const FZOnRequestCommonCultivationDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestCommonCultivationDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestCommonCultivationData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestCommonCultivationDataAck>& InRspMessage)
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

void UZGameRpcStub::RequestCommonCultivationData(const TSharedPtr<idlezt::RequestCommonCultivationDataReq>& InReqMessage, const OnRequestCommonCultivationDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestCommonCultivationData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestCommonCultivationDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickMergeBreathing(const FZOneClickMergeBreathingReq& InParams, const FZOnOneClickMergeBreathingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickMergeBreathingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickMergeBreathing(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickMergeBreathingAck>& InRspMessage)
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

void UZGameRpcStub::OneClickMergeBreathing(const TSharedPtr<idlezt::OneClickMergeBreathingReq>& InReqMessage, const OnOneClickMergeBreathingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickMergeBreathing;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickMergeBreathingAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBreathingExerciseReward(const FZReceiveBreathingExerciseRewardReq& InParams, const FZOnReceiveBreathingExerciseRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReceiveBreathingExerciseRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBreathingExerciseReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReceiveBreathingExerciseRewardAck>& InRspMessage)
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

void UZGameRpcStub::ReceiveBreathingExerciseReward(const TSharedPtr<idlezt::ReceiveBreathingExerciseRewardReq>& InReqMessage, const OnReceiveBreathingExerciseRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBreathingExerciseReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReceiveBreathingExerciseRewardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetInventoryData(const FZGetInventoryDataReq& InParams, const FZOnGetInventoryDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetInventoryDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetInventoryData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetInventoryDataAck>& InRspMessage)
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

void UZGameRpcStub::GetInventoryData(const TSharedPtr<idlezt::GetInventoryDataReq>& InReqMessage, const OnGetInventoryDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetInventoryData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetInventoryDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetQuestData(const FZGetQuestDataReq& InParams, const FZOnGetQuestDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetQuestDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetQuestData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetQuestDataAck>& InRspMessage)
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

void UZGameRpcStub::GetQuestData(const TSharedPtr<idlezt::GetQuestDataReq>& InReqMessage, const OnGetQuestDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetQuestData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetQuestDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_CreateCharacter(const FZCreateCharacterReq& InParams, const FZOnCreateCharacterResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::CreateCharacterReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    CreateCharacter(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::CreateCharacterAck>& InRspMessage)
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

void UZGameRpcStub::CreateCharacter(const TSharedPtr<idlezt::CreateCharacterReq>& InReqMessage, const OnCreateCharacterResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::CreateCharacter;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::CreateCharacterAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseItem(const FZUseItemReq& InParams, const FZOnUseItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UseItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UseItemAck>& InRspMessage)
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

void UZGameRpcStub::UseItem(const TSharedPtr<idlezt::UseItemReq>& InReqMessage, const OnUseItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UseItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseSelectGift(const FZUseSelectGiftReq& InParams, const FZOnUseSelectGiftResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UseSelectGiftReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseSelectGift(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UseSelectGiftAck>& InRspMessage)
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

void UZGameRpcStub::UseSelectGift(const TSharedPtr<idlezt::UseSelectGiftReq>& InReqMessage, const OnUseSelectGiftResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseSelectGift;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UseSelectGiftAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SellItem(const FZSellItemReq& InParams, const FZOnSellItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SellItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SellItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SellItemAck>& InRspMessage)
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

void UZGameRpcStub::SellItem(const TSharedPtr<idlezt::SellItemReq>& InReqMessage, const OnSellItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SellItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SellItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockEquipmentSlot(const FZUnlockEquipmentSlotReq& InParams, const FZOnUnlockEquipmentSlotResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UnlockEquipmentSlotReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockEquipmentSlot(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UnlockEquipmentSlotAck>& InRspMessage)
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

void UZGameRpcStub::UnlockEquipmentSlot(const TSharedPtr<idlezt::UnlockEquipmentSlotReq>& InReqMessage, const OnUnlockEquipmentSlotResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockEquipmentSlot;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UnlockEquipmentSlotAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineStart(const FZAlchemyRefineStartReq& InParams, const FZOnAlchemyRefineStartResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AlchemyRefineStartReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineStart(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AlchemyRefineStartAck>& InRspMessage)
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

void UZGameRpcStub::AlchemyRefineStart(const TSharedPtr<idlezt::AlchemyRefineStartReq>& InReqMessage, const OnAlchemyRefineStartResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineStart;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AlchemyRefineStartAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineCancel(const FZAlchemyRefineCancelReq& InParams, const FZOnAlchemyRefineCancelResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AlchemyRefineCancelReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineCancel(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AlchemyRefineCancelAck>& InRspMessage)
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

void UZGameRpcStub::AlchemyRefineCancel(const TSharedPtr<idlezt::AlchemyRefineCancelReq>& InReqMessage, const OnAlchemyRefineCancelResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineCancel;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AlchemyRefineCancelAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AlchemyRefineExtract(const FZAlchemyRefineExtractReq& InParams, const FZOnAlchemyRefineExtractResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AlchemyRefineExtractReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AlchemyRefineExtract(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AlchemyRefineExtractAck>& InRspMessage)
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

void UZGameRpcStub::AlchemyRefineExtract(const TSharedPtr<idlezt::AlchemyRefineExtractReq>& InReqMessage, const OnAlchemyRefineExtractResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineExtract;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AlchemyRefineExtractAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleShopData(const FZGetRoleShopDataReq& InParams, const FZOnGetRoleShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleShopData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleShopDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleShopData(const TSharedPtr<idlezt::GetRoleShopDataReq>& InReqMessage, const OnGetRoleShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleShopDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshShop(const FZRefreshShopReq& InParams, const FZOnRefreshShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RefreshShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshShop(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RefreshShopAck>& InRspMessage)
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

void UZGameRpcStub::RefreshShop(const TSharedPtr<idlezt::RefreshShopReq>& InReqMessage, const OnRefreshShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RefreshShopAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuyShopItem(const FZBuyShopItemReq& InParams, const FZOnBuyShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::BuyShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuyShopItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::BuyShopItemAck>& InRspMessage)
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

void UZGameRpcStub::BuyShopItem(const TSharedPtr<idlezt::BuyShopItemReq>& InReqMessage, const OnBuyShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuyShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::BuyShopItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleDeluxeShopData(const FZGetRoleDeluxeShopDataReq& InParams, const FZOnGetRoleDeluxeShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleDeluxeShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleDeluxeShopData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleDeluxeShopDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleDeluxeShopData(const TSharedPtr<idlezt::GetRoleDeluxeShopDataReq>& InReqMessage, const OnGetRoleDeluxeShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleDeluxeShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleDeluxeShopDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshDeluxeShop(const FZRefreshDeluxeShopReq& InParams, const FZOnRefreshDeluxeShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RefreshDeluxeShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshDeluxeShop(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RefreshDeluxeShopAck>& InRspMessage)
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

void UZGameRpcStub::RefreshDeluxeShop(const TSharedPtr<idlezt::RefreshDeluxeShopReq>& InReqMessage, const OnRefreshDeluxeShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshDeluxeShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RefreshDeluxeShopAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuyDeluxeShopItem(const FZBuyDeluxeShopItemReq& InParams, const FZOnBuyDeluxeShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::BuyDeluxeShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuyDeluxeShopItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::BuyDeluxeShopItemAck>& InRspMessage)
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

void UZGameRpcStub::BuyDeluxeShopItem(const TSharedPtr<idlezt::BuyDeluxeShopItemReq>& InReqMessage, const OnBuyDeluxeShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuyDeluxeShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::BuyDeluxeShopItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetTemporaryPackageData(const FZGetTemporaryPackageDataReq& InParams, const FZOnGetTemporaryPackageDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetTemporaryPackageDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetTemporaryPackageData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetTemporaryPackageDataAck>& InRspMessage)
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

void UZGameRpcStub::GetTemporaryPackageData(const TSharedPtr<idlezt::GetTemporaryPackageDataReq>& InReqMessage, const OnGetTemporaryPackageDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetTemporaryPackageData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetTemporaryPackageDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExtractTemporaryPackageItems(const FZExtractTemporaryPackageItemsReq& InParams, const FZOnExtractTemporaryPackageItemsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ExtractTemporaryPackageItemsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExtractTemporaryPackageItems(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ExtractTemporaryPackageItemsAck>& InRspMessage)
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

void UZGameRpcStub::ExtractTemporaryPackageItems(const TSharedPtr<idlezt::ExtractTemporaryPackageItemsReq>& InReqMessage, const OnExtractTemporaryPackageItemsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExtractTemporaryPackageItems;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ExtractTemporaryPackageItemsAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SpeedupRelive(const FZSpeedupReliveReq& InParams, const FZOnSpeedupReliveResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SpeedupReliveReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SpeedupRelive(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SpeedupReliveAck>& InRspMessage)
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

void UZGameRpcStub::SpeedupRelive(const TSharedPtr<idlezt::SpeedupReliveReq>& InReqMessage, const OnSpeedupReliveResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SpeedupRelive;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SpeedupReliveAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMapInfo(const FZGetMapInfoReq& InParams, const FZOnGetMapInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetMapInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMapInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetMapInfoAck>& InRspMessage)
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

void UZGameRpcStub::GetMapInfo(const TSharedPtr<idlezt::GetMapInfoReq>& InReqMessage, const OnGetMapInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMapInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetMapInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockArena(const FZUnlockArenaReq& InParams, const FZOnUnlockArenaResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UnlockArenaReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockArena(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UnlockArenaAck>& InRspMessage)
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

void UZGameRpcStub::UnlockArena(const TSharedPtr<idlezt::UnlockArenaReq>& InReqMessage, const OnUnlockArenaResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockArena;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UnlockArenaAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QuestOp(const FZQuestOpReq& InParams, const FZOnQuestOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::QuestOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QuestOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::QuestOpAck>& InRspMessage)
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

void UZGameRpcStub::QuestOp(const TSharedPtr<idlezt::QuestOpReq>& InReqMessage, const OnQuestOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QuestOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::QuestOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EquipmentPutOn(const FZEquipmentPutOnReq& InParams, const FZOnEquipmentPutOnResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::EquipmentPutOnReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EquipmentPutOn(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::EquipmentPutOnAck>& InRspMessage)
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

void UZGameRpcStub::EquipmentPutOn(const TSharedPtr<idlezt::EquipmentPutOnReq>& InReqMessage, const OnEquipmentPutOnResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentPutOn;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::EquipmentPutOnAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EquipmentTakeOff(const FZEquipmentTakeOffReq& InParams, const FZOnEquipmentTakeOffResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::EquipmentTakeOffReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EquipmentTakeOff(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::EquipmentTakeOffAck>& InRspMessage)
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

void UZGameRpcStub::EquipmentTakeOff(const TSharedPtr<idlezt::EquipmentTakeOffReq>& InReqMessage, const OnEquipmentTakeOffResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentTakeOff;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::EquipmentTakeOffAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLeaderboardPreview(const FZGetLeaderboardPreviewReq& InParams, const FZOnGetLeaderboardPreviewResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetLeaderboardPreviewReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLeaderboardPreview(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetLeaderboardPreviewAck>& InRspMessage)
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

void UZGameRpcStub::GetLeaderboardPreview(const TSharedPtr<idlezt::GetLeaderboardPreviewReq>& InReqMessage, const OnGetLeaderboardPreviewResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardPreview;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetLeaderboardPreviewAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLeaderboardData(const FZGetLeaderboardDataReq& InParams, const FZOnGetLeaderboardDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetLeaderboardDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLeaderboardData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetLeaderboardDataAck>& InRspMessage)
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

void UZGameRpcStub::GetLeaderboardData(const TSharedPtr<idlezt::GetLeaderboardDataReq>& InReqMessage, const OnGetLeaderboardDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetLeaderboardDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleLeaderboardData(const FZGetRoleLeaderboardDataReq& InParams, const FZOnGetRoleLeaderboardDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleLeaderboardDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleLeaderboardData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleLeaderboardDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleLeaderboardData(const TSharedPtr<idlezt::GetRoleLeaderboardDataReq>& InReqMessage, const OnGetRoleLeaderboardDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleLeaderboardData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleLeaderboardDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LeaderboardClickLike(const FZLeaderboardClickLikeReq& InParams, const FZOnLeaderboardClickLikeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::LeaderboardClickLikeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LeaderboardClickLike(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::LeaderboardClickLikeAck>& InRspMessage)
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

void UZGameRpcStub::LeaderboardClickLike(const TSharedPtr<idlezt::LeaderboardClickLikeReq>& InReqMessage, const OnLeaderboardClickLikeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardClickLike;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::LeaderboardClickLikeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LeaderboardUpdateMessage(const FZLeaderboardUpdateMessageReq& InParams, const FZOnLeaderboardUpdateMessageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::LeaderboardUpdateMessageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LeaderboardUpdateMessage(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::LeaderboardUpdateMessageAck>& InRspMessage)
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

void UZGameRpcStub::LeaderboardUpdateMessage(const TSharedPtr<idlezt::LeaderboardUpdateMessageReq>& InReqMessage, const OnLeaderboardUpdateMessageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardUpdateMessage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::LeaderboardUpdateMessageAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetFuZeReward(const FZGetFuZeRewardReq& InParams, const FZOnGetFuZeRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetFuZeRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetFuZeReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetFuZeRewardAck>& InRspMessage)
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

void UZGameRpcStub::GetFuZeReward(const TSharedPtr<idlezt::GetFuZeRewardReq>& InReqMessage, const OnGetFuZeRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetFuZeReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetFuZeRewardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleMailData(const FZGetRoleMailDataReq& InParams, const FZOnGetRoleMailDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleMailDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleMailData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleMailDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleMailData(const TSharedPtr<idlezt::GetRoleMailDataReq>& InReqMessage, const OnGetRoleMailDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleMailData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleMailDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReadMail(const FZReadMailReq& InParams, const FZOnReadMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReadMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReadMail(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReadMailAck>& InRspMessage)
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

void UZGameRpcStub::ReadMail(const TSharedPtr<idlezt::ReadMailReq>& InReqMessage, const OnReadMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReadMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReadMailAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMailAttachment(const FZGetMailAttachmentReq& InParams, const FZOnGetMailAttachmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetMailAttachmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMailAttachment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetMailAttachmentAck>& InRspMessage)
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

void UZGameRpcStub::GetMailAttachment(const TSharedPtr<idlezt::GetMailAttachmentReq>& InReqMessage, const OnGetMailAttachmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMailAttachment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetMailAttachmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DeleteMail(const FZDeleteMailReq& InParams, const FZOnDeleteMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DeleteMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DeleteMail(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DeleteMailAck>& InRspMessage)
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

void UZGameRpcStub::DeleteMail(const TSharedPtr<idlezt::DeleteMailReq>& InReqMessage, const OnDeleteMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DeleteMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DeleteMailAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickGetMailAttachment(const FZOneClickGetMailAttachmentReq& InParams, const FZOnOneClickGetMailAttachmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickGetMailAttachmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickGetMailAttachment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickGetMailAttachmentAck>& InRspMessage)
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

void UZGameRpcStub::OneClickGetMailAttachment(const TSharedPtr<idlezt::OneClickGetMailAttachmentReq>& InReqMessage, const OnOneClickGetMailAttachmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickGetMailAttachment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickGetMailAttachmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickReadMail(const FZOneClickReadMailReq& InParams, const FZOnOneClickReadMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickReadMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickReadMail(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickReadMailAck>& InRspMessage)
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

void UZGameRpcStub::OneClickReadMail(const TSharedPtr<idlezt::OneClickReadMailReq>& InReqMessage, const OnOneClickReadMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickReadMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickReadMailAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickDeleteMail(const FZOneClickDeleteMailReq& InParams, const FZOnOneClickDeleteMailResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickDeleteMailReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickDeleteMail(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickDeleteMailAck>& InRspMessage)
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

void UZGameRpcStub::OneClickDeleteMail(const TSharedPtr<idlezt::OneClickDeleteMailReq>& InReqMessage, const OnOneClickDeleteMailResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickDeleteMail;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickDeleteMailAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UnlockFunctionModule(const FZUnlockFunctionModuleReq& InParams, const FZOnUnlockFunctionModuleResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UnlockFunctionModuleReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UnlockFunctionModule(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UnlockFunctionModuleAck>& InRspMessage)
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

void UZGameRpcStub::UnlockFunctionModule(const TSharedPtr<idlezt::UnlockFunctionModuleReq>& InReqMessage, const OnUnlockFunctionModuleResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockFunctionModule;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UnlockFunctionModuleAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetChatRecord(const FZGetChatRecordReq& InParams, const FZOnGetChatRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetChatRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetChatRecord(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetChatRecordAck>& InRspMessage)
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

void UZGameRpcStub::GetChatRecord(const TSharedPtr<idlezt::GetChatRecordReq>& InReqMessage, const OnGetChatRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetChatRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetChatRecordAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DeletePrivateChatRecord(const FZDeletePrivateChatRecordReq& InParams, const FZOnDeletePrivateChatRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DeletePrivateChatRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DeletePrivateChatRecord(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DeletePrivateChatRecordAck>& InRspMessage)
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

void UZGameRpcStub::DeletePrivateChatRecord(const TSharedPtr<idlezt::DeletePrivateChatRecordReq>& InReqMessage, const OnDeletePrivateChatRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DeletePrivateChatRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DeletePrivateChatRecordAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SendChatMessage(const FZSendChatMessageReq& InParams, const FZOnSendChatMessageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SendChatMessageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SendChatMessage(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SendChatMessageAck>& InRspMessage)
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

void UZGameRpcStub::SendChatMessage(const TSharedPtr<idlezt::SendChatMessageReq>& InReqMessage, const OnSendChatMessageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SendChatMessage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SendChatMessageAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ClearChatUnreadNum(const FZClearChatUnreadNumReq& InParams, const FZOnClearChatUnreadNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ClearChatUnreadNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ClearChatUnreadNum(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ClearChatUnreadNumAck>& InRspMessage)
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

void UZGameRpcStub::ClearChatUnreadNum(const TSharedPtr<idlezt::ClearChatUnreadNumReq>& InReqMessage, const OnClearChatUnreadNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ClearChatUnreadNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ClearChatUnreadNumAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineStart(const FZForgeRefineStartReq& InParams, const FZOnForgeRefineStartResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ForgeRefineStartReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineStart(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ForgeRefineStartAck>& InRspMessage)
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

void UZGameRpcStub::ForgeRefineStart(const TSharedPtr<idlezt::ForgeRefineStartReq>& InReqMessage, const OnForgeRefineStartResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineStart;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ForgeRefineStartAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineCancel(const FZForgeRefineCancelReq& InParams, const FZOnForgeRefineCancelResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ForgeRefineCancelReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineCancel(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ForgeRefineCancelAck>& InRspMessage)
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

void UZGameRpcStub::ForgeRefineCancel(const TSharedPtr<idlezt::ForgeRefineCancelReq>& InReqMessage, const OnForgeRefineCancelResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineCancel;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ForgeRefineCancelAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeRefineExtract(const FZForgeRefineExtractReq& InParams, const FZOnForgeRefineExtractResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ForgeRefineExtractReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeRefineExtract(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ForgeRefineExtractAck>& InRspMessage)
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

void UZGameRpcStub::ForgeRefineExtract(const TSharedPtr<idlezt::ForgeRefineExtractReq>& InReqMessage, const OnForgeRefineExtractResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineExtract;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ForgeRefineExtractAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetForgeLostEquipmentData(const FZGetForgeLostEquipmentDataReq& InParams, const FZOnGetForgeLostEquipmentDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetForgeLostEquipmentDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetForgeLostEquipmentData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetForgeLostEquipmentDataAck>& InRspMessage)
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

void UZGameRpcStub::GetForgeLostEquipmentData(const TSharedPtr<idlezt::GetForgeLostEquipmentDataReq>& InReqMessage, const OnGetForgeLostEquipmentDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetForgeLostEquipmentData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetForgeLostEquipmentDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeDestroy(const FZForgeDestroyReq& InParams, const FZOnForgeDestroyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ForgeDestroyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeDestroy(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ForgeDestroyAck>& InRspMessage)
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

void UZGameRpcStub::ForgeDestroy(const TSharedPtr<idlezt::ForgeDestroyReq>& InReqMessage, const OnForgeDestroyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeDestroy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ForgeDestroyAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ForgeFindBack(const FZForgeFindBackReq& InParams, const FZOnForgeFindBackResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ForgeFindBackReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ForgeFindBack(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ForgeFindBackAck>& InRspMessage)
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

void UZGameRpcStub::ForgeFindBack(const TSharedPtr<idlezt::ForgeFindBackReq>& InReqMessage, const OnForgeFindBackResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeFindBack;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ForgeFindBackAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestPillElixirData(const FZRequestPillElixirDataReq& InParams, const FZOnRequestPillElixirDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestPillElixirDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestPillElixirData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestPillElixirDataAck>& InRspMessage)
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

void UZGameRpcStub::RequestPillElixirData(const TSharedPtr<idlezt::RequestPillElixirDataReq>& InReqMessage, const OnRequestPillElixirDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestPillElixirData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestPillElixirDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetOnePillElixirData(const FZGetOnePillElixirDataReq& InParams, const FZOnGetOnePillElixirDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetOnePillElixirDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetOnePillElixirData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetOnePillElixirDataAck>& InRspMessage)
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

void UZGameRpcStub::GetOnePillElixirData(const TSharedPtr<idlezt::GetOnePillElixirDataReq>& InReqMessage, const OnGetOnePillElixirDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetOnePillElixirData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetOnePillElixirDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestModifyPillElixirFilter(const FZRequestModifyPillElixirFilterReq& InParams, const FZOnRequestModifyPillElixirFilterResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestModifyPillElixirFilterReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestModifyPillElixirFilter(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestModifyPillElixirFilterAck>& InRspMessage)
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

void UZGameRpcStub::RequestModifyPillElixirFilter(const TSharedPtr<idlezt::RequestModifyPillElixirFilterReq>& InReqMessage, const OnRequestModifyPillElixirFilterResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestModifyPillElixirFilter;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestModifyPillElixirFilterAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UsePillElixir(const FZUsePillElixirReq& InParams, const FZOnUsePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UsePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UsePillElixir(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UsePillElixirAck>& InRspMessage)
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

void UZGameRpcStub::UsePillElixir(const TSharedPtr<idlezt::UsePillElixirReq>& InReqMessage, const OnUsePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UsePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UsePillElixirAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickUsePillElixir(const FZOneClickUsePillElixirReq& InParams, const FZOnOneClickUsePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickUsePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickUsePillElixir(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickUsePillElixirAck>& InRspMessage)
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

void UZGameRpcStub::OneClickUsePillElixir(const TSharedPtr<idlezt::OneClickUsePillElixirReq>& InReqMessage, const OnOneClickUsePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickUsePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickUsePillElixirAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_TradePillElixir(const FZTradePillElixirReq& InParams, const FZOnTradePillElixirResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::TradePillElixirReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    TradePillElixir(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::TradePillElixirAck>& InRspMessage)
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

void UZGameRpcStub::TradePillElixir(const TSharedPtr<idlezt::TradePillElixirReq>& InReqMessage, const OnTradePillElixirResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::TradePillElixir;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::TradePillElixirAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReinforceEquipment(const FZReinforceEquipmentReq& InParams, const FZOnReinforceEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReinforceEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReinforceEquipment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReinforceEquipmentAck>& InRspMessage)
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

void UZGameRpcStub::ReinforceEquipment(const TSharedPtr<idlezt::ReinforceEquipmentReq>& InReqMessage, const OnReinforceEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReinforceEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReinforceEquipmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefineEquipment(const FZRefineEquipmentReq& InParams, const FZOnRefineEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RefineEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefineEquipment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RefineEquipmentAck>& InRspMessage)
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

void UZGameRpcStub::RefineEquipment(const TSharedPtr<idlezt::RefineEquipmentReq>& InReqMessage, const OnRefineEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefineEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RefineEquipmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QiWenEquipment(const FZQiWenEquipmentReq& InParams, const FZOnQiWenEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::QiWenEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QiWenEquipment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::QiWenEquipmentAck>& InRspMessage)
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

void UZGameRpcStub::QiWenEquipment(const TSharedPtr<idlezt::QiWenEquipmentReq>& InReqMessage, const OnQiWenEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QiWenEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::QiWenEquipmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ResetEquipment(const FZResetEquipmentReq& InParams, const FZOnResetEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ResetEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ResetEquipment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ResetEquipmentAck>& InRspMessage)
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

void UZGameRpcStub::ResetEquipment(const TSharedPtr<idlezt::ResetEquipmentReq>& InReqMessage, const OnResetEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ResetEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ResetEquipmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_InheritEquipment(const FZInheritEquipmentReq& InParams, const FZOnInheritEquipmentResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::InheritEquipmentReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    InheritEquipment(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::InheritEquipmentAck>& InRspMessage)
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

void UZGameRpcStub::InheritEquipment(const TSharedPtr<idlezt::InheritEquipmentReq>& InReqMessage, const OnInheritEquipmentResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::InheritEquipment;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::InheritEquipmentAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_LockItem(const FZLockItemReq& InParams, const FZOnLockItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::LockItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    LockItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::LockItemAck>& InRspMessage)
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

void UZGameRpcStub::LockItem(const TSharedPtr<idlezt::LockItemReq>& InReqMessage, const OnLockItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::LockItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::LockItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SoloArenaChallenge(const FZSoloArenaChallengeReq& InParams, const FZOnSoloArenaChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SoloArenaChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SoloArenaChallenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SoloArenaChallengeAck>& InRspMessage)
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

void UZGameRpcStub::SoloArenaChallenge(const TSharedPtr<idlezt::SoloArenaChallengeReq>& InReqMessage, const OnSoloArenaChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SoloArenaChallengeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SoloArenaQuickEnd(const FZSoloArenaQuickEndReq& InParams, const FZOnSoloArenaQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SoloArenaQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SoloArenaQuickEnd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SoloArenaQuickEndAck>& InRspMessage)
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

void UZGameRpcStub::SoloArenaQuickEnd(const TSharedPtr<idlezt::SoloArenaQuickEndReq>& InReqMessage, const OnSoloArenaQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SoloArenaQuickEndAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSoloArenaHistoryList(const FZGetSoloArenaHistoryListReq& InParams, const FZOnGetSoloArenaHistoryListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSoloArenaHistoryListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSoloArenaHistoryList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSoloArenaHistoryListAck>& InRspMessage)
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

void UZGameRpcStub::GetSoloArenaHistoryList(const TSharedPtr<idlezt::GetSoloArenaHistoryListReq>& InReqMessage, const OnGetSoloArenaHistoryListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSoloArenaHistoryList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSoloArenaHistoryListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerChallenge(const FZMonsterTowerChallengeReq& InParams, const FZOnMonsterTowerChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::MonsterTowerChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerChallenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::MonsterTowerChallengeAck>& InRspMessage)
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

void UZGameRpcStub::MonsterTowerChallenge(const TSharedPtr<idlezt::MonsterTowerChallengeReq>& InReqMessage, const OnMonsterTowerChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::MonsterTowerChallengeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerDrawIdleAward(const FZMonsterTowerDrawIdleAwardReq& InParams, const FZOnMonsterTowerDrawIdleAwardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::MonsterTowerDrawIdleAwardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerDrawIdleAward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::MonsterTowerDrawIdleAwardAck>& InRspMessage)
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

void UZGameRpcStub::MonsterTowerDrawIdleAward(const TSharedPtr<idlezt::MonsterTowerDrawIdleAwardReq>& InReqMessage, const OnMonsterTowerDrawIdleAwardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerDrawIdleAward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::MonsterTowerDrawIdleAwardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerClosedDoorTraining(const FZMonsterTowerClosedDoorTrainingReq& InParams, const FZOnMonsterTowerClosedDoorTrainingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::MonsterTowerClosedDoorTrainingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerClosedDoorTraining(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingAck>& InRspMessage)
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

void UZGameRpcStub::MonsterTowerClosedDoorTraining(const TSharedPtr<idlezt::MonsterTowerClosedDoorTrainingReq>& InReqMessage, const OnMonsterTowerClosedDoorTrainingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerClosedDoorTraining;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::MonsterTowerClosedDoorTrainingAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_MonsterTowerQuickEnd(const FZMonsterTowerQuickEndReq& InParams, const FZOnMonsterTowerQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::MonsterTowerQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    MonsterTowerQuickEnd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::MonsterTowerQuickEndAck>& InRspMessage)
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

void UZGameRpcStub::MonsterTowerQuickEnd(const TSharedPtr<idlezt::MonsterTowerQuickEndReq>& InReqMessage, const OnMonsterTowerQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::MonsterTowerQuickEndAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMonsterTowerChallengeList(const FZGetMonsterTowerChallengeListReq& InParams, const FZOnGetMonsterTowerChallengeListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetMonsterTowerChallengeListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMonsterTowerChallengeList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetMonsterTowerChallengeListAck>& InRspMessage)
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

void UZGameRpcStub::GetMonsterTowerChallengeList(const TSharedPtr<idlezt::GetMonsterTowerChallengeListReq>& InReqMessage, const OnGetMonsterTowerChallengeListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetMonsterTowerChallengeListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetMonsterTowerChallengeReward(const FZGetMonsterTowerChallengeRewardReq& InParams, const FZOnGetMonsterTowerChallengeRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetMonsterTowerChallengeRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetMonsterTowerChallengeReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetMonsterTowerChallengeRewardAck>& InRspMessage)
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

void UZGameRpcStub::GetMonsterTowerChallengeReward(const TSharedPtr<idlezt::GetMonsterTowerChallengeRewardReq>& InReqMessage, const OnGetMonsterTowerChallengeRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetMonsterTowerChallengeRewardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetWorldTimeDilation(const FZSetWorldTimeDilationReq& InParams, const FZOnSetWorldTimeDilationResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetWorldTimeDilationReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetWorldTimeDilation(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetWorldTimeDilationAck>& InRspMessage)
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

void UZGameRpcStub::SetWorldTimeDilation(const TSharedPtr<idlezt::SetWorldTimeDilationReq>& InReqMessage, const OnSetWorldTimeDilationResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetWorldTimeDilation;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetWorldTimeDilationAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetFightMode(const FZSetFightModeReq& InParams, const FZOnSetFightModeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetFightModeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetFightMode(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetFightModeAck>& InRspMessage)
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

void UZGameRpcStub::SetFightMode(const TSharedPtr<idlezt::SetFightModeReq>& InReqMessage, const OnSetFightModeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetFightMode;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetFightModeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpgradeQiCollector(const FZUpgradeQiCollectorReq& InParams, const FZOnUpgradeQiCollectorResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UpgradeQiCollectorReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpgradeQiCollector(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UpgradeQiCollectorAck>& InRspMessage)
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

void UZGameRpcStub::UpgradeQiCollector(const TSharedPtr<idlezt::UpgradeQiCollectorReq>& InReqMessage, const OnUpgradeQiCollectorResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpgradeQiCollector;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UpgradeQiCollectorAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleAllStats(const FZGetRoleAllStatsReq& InParams, const FZOnGetRoleAllStatsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleAllStatsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleAllStats(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleAllStatsAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleAllStats(const TSharedPtr<idlezt::GetRoleAllStatsReq>& InReqMessage, const OnGetRoleAllStatsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAllStats;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleAllStatsAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShanhetuData(const FZGetShanhetuDataReq& InParams, const FZOnGetShanhetuDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetShanhetuDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShanhetuData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetShanhetuDataAck>& InRspMessage)
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

void UZGameRpcStub::GetShanhetuData(const TSharedPtr<idlezt::GetShanhetuDataReq>& InReqMessage, const OnGetShanhetuDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetShanhetuDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetShanhetuUseConfig(const FZSetShanhetuUseConfigReq& InParams, const FZOnSetShanhetuUseConfigResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetShanhetuUseConfigReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetShanhetuUseConfig(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetShanhetuUseConfigAck>& InRspMessage)
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

void UZGameRpcStub::SetShanhetuUseConfig(const TSharedPtr<idlezt::SetShanhetuUseConfigReq>& InReqMessage, const OnSetShanhetuUseConfigResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetShanhetuUseConfig;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetShanhetuUseConfigAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UseShanhetu(const FZUseShanhetuReq& InParams, const FZOnUseShanhetuResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UseShanhetuReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UseShanhetu(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UseShanhetuAck>& InRspMessage)
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

void UZGameRpcStub::UseShanhetu(const TSharedPtr<idlezt::UseShanhetuReq>& InReqMessage, const OnUseShanhetuResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UseShanhetu;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UseShanhetuAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_StepShanhetu(const FZStepShanhetuReq& InParams, const FZOnStepShanhetuResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::StepShanhetuReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    StepShanhetu(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::StepShanhetuAck>& InRspMessage)
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

void UZGameRpcStub::StepShanhetu(const TSharedPtr<idlezt::StepShanhetuReq>& InReqMessage, const OnStepShanhetuResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::StepShanhetu;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::StepShanhetuAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShanhetuUseRecord(const FZGetShanhetuUseRecordReq& InParams, const FZOnGetShanhetuUseRecordResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetShanhetuUseRecordReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShanhetuUseRecord(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetShanhetuUseRecordAck>& InRspMessage)
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

void UZGameRpcStub::GetShanhetuUseRecord(const TSharedPtr<idlezt::GetShanhetuUseRecordReq>& InReqMessage, const OnGetShanhetuUseRecordResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuUseRecord;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetShanhetuUseRecordAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetAttackLockType(const FZSetAttackLockTypeReq& InParams, const FZOnSetAttackLockTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetAttackLockTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetAttackLockType(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetAttackLockTypeAck>& InRspMessage)
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

void UZGameRpcStub::SetAttackLockType(const TSharedPtr<idlezt::SetAttackLockTypeReq>& InReqMessage, const OnSetAttackLockTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackLockType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetAttackLockTypeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetAttackUnlockType(const FZSetAttackUnlockTypeReq& InParams, const FZOnSetAttackUnlockTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetAttackUnlockTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetAttackUnlockType(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetAttackUnlockTypeAck>& InRspMessage)
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

void UZGameRpcStub::SetAttackUnlockType(const TSharedPtr<idlezt::SetAttackUnlockTypeReq>& InReqMessage, const OnSetAttackUnlockTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackUnlockType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetAttackUnlockTypeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetShowUnlockButton(const FZSetShowUnlockButtonReq& InParams, const FZOnSetShowUnlockButtonResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetShowUnlockButtonReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetShowUnlockButton(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetShowUnlockButtonAck>& InRspMessage)
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

void UZGameRpcStub::SetShowUnlockButton(const TSharedPtr<idlezt::SetShowUnlockButtonReq>& InReqMessage, const OnSetShowUnlockButtonResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetShowUnlockButton;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetShowUnlockButtonAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetUserVar(const FZGetUserVarReq& InParams, const FZOnGetUserVarResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetUserVarReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetUserVar(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetUserVarRsp>& InRspMessage)
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

void UZGameRpcStub::GetUserVar(const TSharedPtr<idlezt::GetUserVarReq>& InReqMessage, const OnGetUserVarResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVar;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetUserVarRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetUserVars(const FZGetUserVarsReq& InParams, const FZOnGetUserVarsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetUserVarsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetUserVars(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetUserVarsRsp>& InRspMessage)
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

void UZGameRpcStub::GetUserVars(const TSharedPtr<idlezt::GetUserVarsReq>& InReqMessage, const OnGetUserVarsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVars;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetUserVarsRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionArenaSummary(const FZGetBossInvasionArenaSummaryReq& InParams, const FZOnGetBossInvasionArenaSummaryResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetBossInvasionArenaSummaryReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionArenaSummary(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetBossInvasionArenaSummaryRsp>& InRspMessage)
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

void UZGameRpcStub::GetBossInvasionArenaSummary(const TSharedPtr<idlezt::GetBossInvasionArenaSummaryReq>& InReqMessage, const OnGetBossInvasionArenaSummaryResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaSummary;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetBossInvasionArenaSummaryRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionArenaTopList(const FZGetBossInvasionArenaTopListReq& InParams, const FZOnGetBossInvasionArenaTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetBossInvasionArenaTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionArenaTopList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetBossInvasionArenaTopListRsp>& InRspMessage)
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

void UZGameRpcStub::GetBossInvasionArenaTopList(const TSharedPtr<idlezt::GetBossInvasionArenaTopListReq>& InReqMessage, const OnGetBossInvasionArenaTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetBossInvasionArenaTopListRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBossInvasionInfo(const FZGetBossInvasionInfoReq& InParams, const FZOnGetBossInvasionInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetBossInvasionInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBossInvasionInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetBossInvasionInfoRsp>& InRspMessage)
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

void UZGameRpcStub::GetBossInvasionInfo(const TSharedPtr<idlezt::GetBossInvasionInfoReq>& InReqMessage, const OnGetBossInvasionInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetBossInvasionInfoRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DrawBossInvasionKillReward(const FZDrawBossInvasionKillRewardReq& InParams, const FZOnDrawBossInvasionKillRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DrawBossInvasionKillRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DrawBossInvasionKillReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DrawBossInvasionKillRewardRsp>& InRspMessage)
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

void UZGameRpcStub::DrawBossInvasionKillReward(const TSharedPtr<idlezt::DrawBossInvasionKillRewardReq>& InReqMessage, const OnDrawBossInvasionKillRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionKillReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DrawBossInvasionKillRewardRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DrawBossInvasionDamageReward(const FZDrawBossInvasionDamageRewardReq& InParams, const FZOnDrawBossInvasionDamageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DrawBossInvasionDamageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DrawBossInvasionDamageReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DrawBossInvasionDamageRewardRsp>& InRspMessage)
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

void UZGameRpcStub::DrawBossInvasionDamageReward(const TSharedPtr<idlezt::DrawBossInvasionDamageRewardReq>& InReqMessage, const OnDrawBossInvasionDamageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionDamageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DrawBossInvasionDamageRewardRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BossInvasionTeleport(const FZBossInvasionTeleportReq& InParams, const FZOnBossInvasionTeleportResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::BossInvasionTeleportReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BossInvasionTeleport(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::BossInvasionTeleportRsp>& InRspMessage)
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

void UZGameRpcStub::BossInvasionTeleport(const TSharedPtr<idlezt::BossInvasionTeleportReq>& InReqMessage, const OnBossInvasionTeleportResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BossInvasionTeleport;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::BossInvasionTeleportRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfItem(const FZShareSelfItemReq& InParams, const FZOnShareSelfItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ShareSelfItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ShareSelfItemRsp>& InRspMessage)
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

void UZGameRpcStub::ShareSelfItem(const TSharedPtr<idlezt::ShareSelfItemReq>& InReqMessage, const OnShareSelfItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ShareSelfItemRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfItems(const FZShareSelfItemsReq& InParams, const FZOnShareSelfItemsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ShareSelfItemsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfItems(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ShareSelfItemsRsp>& InRspMessage)
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

void UZGameRpcStub::ShareSelfItems(const TSharedPtr<idlezt::ShareSelfItemsReq>& InReqMessage, const OnShareSelfItemsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItems;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ShareSelfItemsRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShareItemData(const FZGetShareItemDataReq& InParams, const FZOnGetShareItemDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetShareItemDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShareItemData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetShareItemDataRsp>& InRspMessage)
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

void UZGameRpcStub::GetShareItemData(const TSharedPtr<idlezt::GetShareItemDataReq>& InReqMessage, const OnGetShareItemDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareItemData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetShareItemDataRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleCollectionData(const FZGetRoleCollectionDataReq& InParams, const FZOnGetRoleCollectionDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleCollectionDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleCollectionData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleCollectionDataRsp>& InRspMessage)
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

void UZGameRpcStub::GetRoleCollectionData(const TSharedPtr<idlezt::GetRoleCollectionDataReq>& InReqMessage, const OnGetRoleCollectionDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleCollectionData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleCollectionDataRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RoleCollectionOp(const FZRoleCollectionOpReq& InParams, const FZOnRoleCollectionOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RoleCollectionOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RoleCollectionOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RoleCollectionOpAck>& InRspMessage)
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

void UZGameRpcStub::RoleCollectionOp(const TSharedPtr<idlezt::RoleCollectionOpReq>& InReqMessage, const OnRoleCollectionOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RoleCollectionOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RoleCollectionOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ShareSelfRoleCollection(const FZShareSelfRoleCollectionReq& InParams, const FZOnShareSelfRoleCollectionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ShareSelfRoleCollectionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ShareSelfRoleCollection(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ShareSelfRoleCollectionRsp>& InRspMessage)
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

void UZGameRpcStub::ShareSelfRoleCollection(const TSharedPtr<idlezt::ShareSelfRoleCollectionReq>& InReqMessage, const OnShareSelfRoleCollectionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfRoleCollection;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ShareSelfRoleCollectionRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetShareRoleCollectionData(const FZGetShareRoleCollectionDataReq& InParams, const FZOnGetShareRoleCollectionDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetShareRoleCollectionDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetShareRoleCollectionData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetShareRoleCollectionDataRsp>& InRspMessage)
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

void UZGameRpcStub::GetShareRoleCollectionData(const TSharedPtr<idlezt::GetShareRoleCollectionDataReq>& InReqMessage, const OnGetShareRoleCollectionDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareRoleCollectionData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetShareRoleCollectionDataRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetChecklistData(const FZGetChecklistDataReq& InParams, const FZOnGetChecklistDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetChecklistDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetChecklistData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetChecklistDataAck>& InRspMessage)
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

void UZGameRpcStub::GetChecklistData(const TSharedPtr<idlezt::GetChecklistDataReq>& InReqMessage, const OnGetChecklistDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetChecklistData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetChecklistDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ChecklistOp(const FZChecklistOpReq& InParams, const FZOnChecklistOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ChecklistOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ChecklistOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ChecklistOpAck>& InRspMessage)
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

void UZGameRpcStub::ChecklistOp(const TSharedPtr<idlezt::ChecklistOpReq>& InReqMessage, const OnChecklistOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ChecklistOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ChecklistOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpdateChecklist(const FZUpdateChecklistReq& InParams, const FZOnUpdateChecklistResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UpdateChecklistReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpdateChecklist(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UpdateChecklistAck>& InRspMessage)
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

void UZGameRpcStub::UpdateChecklist(const TSharedPtr<idlezt::UpdateChecklistReq>& InReqMessage, const OnUpdateChecklistResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateChecklist;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UpdateChecklistAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSwordPkInfo(const FZGetSwordPkInfoReq& InParams, const FZOnGetSwordPkInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSwordPkInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSwordPkInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSwordPkInfoRsp>& InRspMessage)
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

void UZGameRpcStub::GetSwordPkInfo(const TSharedPtr<idlezt::GetSwordPkInfoReq>& InReqMessage, const OnGetSwordPkInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSwordPkInfoRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkSignup(const FZSwordPkSignupReq& InParams, const FZOnSwordPkSignupResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SwordPkSignupReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkSignup(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SwordPkSignupRsp>& InRspMessage)
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

void UZGameRpcStub::SwordPkSignup(const TSharedPtr<idlezt::SwordPkSignupReq>& InReqMessage, const OnSwordPkSignupResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkSignup;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SwordPkSignupRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkMatching(const FZSwordPkMatchingReq& InParams, const FZOnSwordPkMatchingResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SwordPkMatchingReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkMatching(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SwordPkMatchingRsp>& InRspMessage)
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

void UZGameRpcStub::SwordPkMatching(const TSharedPtr<idlezt::SwordPkMatchingReq>& InReqMessage, const OnSwordPkMatchingResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkMatching;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SwordPkMatchingRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkChallenge(const FZSwordPkChallengeReq& InParams, const FZOnSwordPkChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SwordPkChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkChallenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SwordPkChallengeRsp>& InRspMessage)
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

void UZGameRpcStub::SwordPkChallenge(const TSharedPtr<idlezt::SwordPkChallengeReq>& InReqMessage, const OnSwordPkChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SwordPkChallengeRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkRevenge(const FZSwordPkRevengeReq& InParams, const FZOnSwordPkRevengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SwordPkRevengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkRevenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SwordPkRevengeRsp>& InRspMessage)
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

void UZGameRpcStub::SwordPkRevenge(const TSharedPtr<idlezt::SwordPkRevengeReq>& InReqMessage, const OnSwordPkRevengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkRevenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SwordPkRevengeRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSwordPkTopList(const FZGetSwordPkTopListReq& InParams, const FZOnGetSwordPkTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSwordPkTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSwordPkTopList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSwordPkTopListRsp>& InRspMessage)
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

void UZGameRpcStub::GetSwordPkTopList(const TSharedPtr<idlezt::GetSwordPkTopListReq>& InReqMessage, const OnGetSwordPkTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSwordPkTopListRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SwordPkExchangeHeroCard(const FZSwordPkExchangeHeroCardReq& InParams, const FZOnSwordPkExchangeHeroCardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SwordPkExchangeHeroCardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SwordPkExchangeHeroCard(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SwordPkExchangeHeroCardRsp>& InRspMessage)
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

void UZGameRpcStub::SwordPkExchangeHeroCard(const TSharedPtr<idlezt::SwordPkExchangeHeroCardReq>& InReqMessage, const OnSwordPkExchangeHeroCardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkExchangeHeroCard;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SwordPkExchangeHeroCardRsp>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCommonItemExchangeData(const FZGetCommonItemExchangeDataReq& InParams, const FZOnGetCommonItemExchangeDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetCommonItemExchangeDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCommonItemExchangeData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetCommonItemExchangeDataAck>& InRspMessage)
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

void UZGameRpcStub::GetCommonItemExchangeData(const TSharedPtr<idlezt::GetCommonItemExchangeDataReq>& InReqMessage, const OnGetCommonItemExchangeDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCommonItemExchangeData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetCommonItemExchangeDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExchangeCommonItem(const FZExchangeCommonItemReq& InParams, const FZOnExchangeCommonItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ExchangeCommonItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExchangeCommonItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ExchangeCommonItemAck>& InRspMessage)
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

void UZGameRpcStub::ExchangeCommonItem(const TSharedPtr<idlezt::ExchangeCommonItemReq>& InReqMessage, const OnExchangeCommonItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExchangeCommonItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ExchangeCommonItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SynthesisCommonItem(const FZSynthesisCommonItemReq& InParams, const FZOnSynthesisCommonItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SynthesisCommonItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SynthesisCommonItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SynthesisCommonItemAck>& InRspMessage)
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

void UZGameRpcStub::SynthesisCommonItem(const TSharedPtr<idlezt::SynthesisCommonItemReq>& InReqMessage, const OnSynthesisCommonItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SynthesisCommonItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SynthesisCommonItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCandidatesSeptList(const FZGetCandidatesSeptListReq& InParams, const FZOnGetCandidatesSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetCandidatesSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCandidatesSeptList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetCandidatesSeptListAck>& InRspMessage)
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

void UZGameRpcStub::GetCandidatesSeptList(const TSharedPtr<idlezt::GetCandidatesSeptListReq>& InReqMessage, const OnGetCandidatesSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetCandidatesSeptListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SearchSept(const FZSearchSeptReq& InParams, const FZOnSearchSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SearchSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SearchSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SearchSeptAck>& InRspMessage)
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

void UZGameRpcStub::SearchSept(const TSharedPtr<idlezt::SearchSeptReq>& InReqMessage, const OnSearchSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SearchSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SearchSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptBaseInfo(const FZGetSeptBaseInfoReq& InParams, const FZOnGetSeptBaseInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptBaseInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptBaseInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptBaseInfoAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptBaseInfo(const TSharedPtr<idlezt::GetSeptBaseInfoReq>& InReqMessage, const OnGetSeptBaseInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptBaseInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptBaseInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptMemberList(const FZGetSeptMemberListReq& InParams, const FZOnGetSeptMemberListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptMemberListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptMemberList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptMemberListAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptMemberList(const TSharedPtr<idlezt::GetSeptMemberListReq>& InReqMessage, const OnGetSeptMemberListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptMemberList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptMemberListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_CreateSept(const FZCreateSeptReq& InParams, const FZOnCreateSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::CreateSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    CreateSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::CreateSeptAck>& InRspMessage)
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

void UZGameRpcStub::CreateSept(const TSharedPtr<idlezt::CreateSeptReq>& InReqMessage, const OnCreateSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::CreateSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::CreateSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DismissSept(const FZDismissSeptReq& InParams, const FZOnDismissSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DismissSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DismissSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DismissSeptAck>& InRspMessage)
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

void UZGameRpcStub::DismissSept(const TSharedPtr<idlezt::DismissSeptReq>& InReqMessage, const OnDismissSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DismissSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DismissSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ExitSept(const FZExitSeptReq& InParams, const FZOnExitSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ExitSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ExitSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ExitSeptAck>& InRspMessage)
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

void UZGameRpcStub::ExitSept(const TSharedPtr<idlezt::ExitSeptReq>& InReqMessage, const OnExitSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ExitSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ExitSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ApplyJoinSept(const FZApplyJoinSeptReq& InParams, const FZOnApplyJoinSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ApplyJoinSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ApplyJoinSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ApplyJoinSeptAck>& InRspMessage)
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

void UZGameRpcStub::ApplyJoinSept(const TSharedPtr<idlezt::ApplyJoinSeptReq>& InReqMessage, const OnApplyJoinSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ApplyJoinSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ApplyJoinSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ApproveApplySept(const FZApproveApplySeptReq& InParams, const FZOnApproveApplySeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ApproveApplySeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ApproveApplySept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ApproveApplySeptAck>& InRspMessage)
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

void UZGameRpcStub::ApproveApplySept(const TSharedPtr<idlezt::ApproveApplySeptReq>& InReqMessage, const OnApproveApplySeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ApproveApplySept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ApproveApplySeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetApplyJoinSeptList(const FZGetApplyJoinSeptListReq& InParams, const FZOnGetApplyJoinSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetApplyJoinSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetApplyJoinSeptList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetApplyJoinSeptListAck>& InRspMessage)
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

void UZGameRpcStub::GetApplyJoinSeptList(const TSharedPtr<idlezt::GetApplyJoinSeptListReq>& InReqMessage, const OnGetApplyJoinSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetApplyJoinSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetApplyJoinSeptListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RespondInviteSept(const FZRespondInviteSeptReq& InParams, const FZOnRespondInviteSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RespondInviteSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RespondInviteSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RespondInviteSeptAck>& InRspMessage)
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

void UZGameRpcStub::RespondInviteSept(const TSharedPtr<idlezt::RespondInviteSeptReq>& InReqMessage, const OnRespondInviteSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RespondInviteSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RespondInviteSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetInviteMeJoinSeptList(const FZGetInviteMeJoinSeptListReq& InParams, const FZOnGetInviteMeJoinSeptListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetInviteMeJoinSeptListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetInviteMeJoinSeptList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetInviteMeJoinSeptListAck>& InRspMessage)
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

void UZGameRpcStub::GetInviteMeJoinSeptList(const TSharedPtr<idlezt::GetInviteMeJoinSeptListReq>& InReqMessage, const OnGetInviteMeJoinSeptListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetInviteMeJoinSeptList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetInviteMeJoinSeptListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetCandidatesInviteRoleList(const FZGetCandidatesInviteRoleListReq& InParams, const FZOnGetCandidatesInviteRoleListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetCandidatesInviteRoleListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetCandidatesInviteRoleList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetCandidatesInviteRoleListAck>& InRspMessage)
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

void UZGameRpcStub::GetCandidatesInviteRoleList(const TSharedPtr<idlezt::GetCandidatesInviteRoleListReq>& InReqMessage, const OnGetCandidatesInviteRoleListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesInviteRoleList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetCandidatesInviteRoleListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_InviteJoinSept(const FZInviteJoinSeptReq& InParams, const FZOnInviteJoinSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::InviteJoinSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    InviteJoinSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::InviteJoinSeptAck>& InRspMessage)
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

void UZGameRpcStub::InviteJoinSept(const TSharedPtr<idlezt::InviteJoinSeptReq>& InReqMessage, const OnInviteJoinSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::InviteJoinSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::InviteJoinSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetSeptSettings(const FZSetSeptSettingsReq& InParams, const FZOnSetSeptSettingsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetSeptSettingsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetSeptSettings(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetSeptSettingsAck>& InRspMessage)
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

void UZGameRpcStub::SetSeptSettings(const TSharedPtr<idlezt::SetSeptSettingsReq>& InReqMessage, const OnSetSeptSettingsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptSettings;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetSeptSettingsAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_SetSeptAnnounce(const FZSetSeptAnnounceReq& InParams, const FZOnSetSeptAnnounceResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::SetSeptAnnounceReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    SetSeptAnnounce(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::SetSeptAnnounceAck>& InRspMessage)
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

void UZGameRpcStub::SetSeptAnnounce(const TSharedPtr<idlezt::SetSeptAnnounceReq>& InReqMessage, const OnSetSeptAnnounceResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptAnnounce;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::SetSeptAnnounceAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ChangeSeptName(const FZChangeSeptNameReq& InParams, const FZOnChangeSeptNameResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ChangeSeptNameReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ChangeSeptName(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ChangeSeptNameAck>& InRspMessage)
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

void UZGameRpcStub::ChangeSeptName(const TSharedPtr<idlezt::ChangeSeptNameReq>& InReqMessage, const OnChangeSeptNameResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ChangeSeptName;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ChangeSeptNameAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptLog(const FZGetSeptLogReq& InParams, const FZOnGetSeptLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptLog(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptLogAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptLog(const TSharedPtr<idlezt::GetSeptLogReq>& InReqMessage, const OnGetSeptLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptLogAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ConstructSept(const FZConstructSeptReq& InParams, const FZOnConstructSeptResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ConstructSeptReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ConstructSept(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ConstructSeptAck>& InRspMessage)
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

void UZGameRpcStub::ConstructSept(const TSharedPtr<idlezt::ConstructSeptReq>& InReqMessage, const OnConstructSeptResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ConstructSept;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ConstructSeptAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetConstructSeptLog(const FZGetConstructSeptLogReq& InParams, const FZOnGetConstructSeptLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetConstructSeptLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetConstructSeptLog(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetConstructSeptLogAck>& InRspMessage)
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

void UZGameRpcStub::GetConstructSeptLog(const TSharedPtr<idlezt::GetConstructSeptLogReq>& InReqMessage, const OnGetConstructSeptLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetConstructSeptLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetConstructSeptLogAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptInvitedRoleDailyNum(const FZGetSeptInvitedRoleDailyNumReq& InParams, const FZOnGetSeptInvitedRoleDailyNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptInvitedRoleDailyNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptInvitedRoleDailyNum(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptInvitedRoleDailyNum(const TSharedPtr<idlezt::GetSeptInvitedRoleDailyNumReq>& InReqMessage, const OnGetSeptInvitedRoleDailyNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptInvitedRoleDailyNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptInvitedRoleDailyNumAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppointSeptPosition(const FZAppointSeptPositionReq& InParams, const FZOnAppointSeptPositionResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppointSeptPositionReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppointSeptPosition(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppointSeptPositionAck>& InRspMessage)
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

void UZGameRpcStub::AppointSeptPosition(const TSharedPtr<idlezt::AppointSeptPositionReq>& InReqMessage, const OnAppointSeptPositionResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppointSeptPosition;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppointSeptPositionAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ResignSeptChairman(const FZResignSeptChairmanReq& InParams, const FZOnResignSeptChairmanResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ResignSeptChairmanReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ResignSeptChairman(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ResignSeptChairmanAck>& InRspMessage)
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

void UZGameRpcStub::ResignSeptChairman(const TSharedPtr<idlezt::ResignSeptChairmanReq>& InReqMessage, const OnResignSeptChairmanResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ResignSeptChairman;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ResignSeptChairmanAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_KickOutSeptMember(const FZKickOutSeptMemberReq& InParams, const FZOnKickOutSeptMemberResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::KickOutSeptMemberReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    KickOutSeptMember(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::KickOutSeptMemberAck>& InRspMessage)
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

void UZGameRpcStub::KickOutSeptMember(const TSharedPtr<idlezt::KickOutSeptMemberReq>& InReqMessage, const OnKickOutSeptMemberResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::KickOutSeptMember;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::KickOutSeptMemberAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleSeptShopData(const FZGetRoleSeptShopDataReq& InParams, const FZOnGetRoleSeptShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleSeptShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleSeptShopData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleSeptShopDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleSeptShopData(const TSharedPtr<idlezt::GetRoleSeptShopDataReq>& InReqMessage, const OnGetRoleSeptShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleSeptShopDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BuySeptShopItem(const FZBuySeptShopItemReq& InParams, const FZOnBuySeptShopItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::BuySeptShopItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BuySeptShopItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::BuySeptShopItemAck>& InRspMessage)
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

void UZGameRpcStub::BuySeptShopItem(const TSharedPtr<idlezt::BuySeptShopItemReq>& InReqMessage, const OnBuySeptShopItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BuySeptShopItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::BuySeptShopItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleSeptQuestData(const FZGetRoleSeptQuestDataReq& InParams, const FZOnGetRoleSeptQuestDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleSeptQuestDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleSeptQuestData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleSeptQuestDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleSeptQuestData(const TSharedPtr<idlezt::GetRoleSeptQuestDataReq>& InReqMessage, const OnGetRoleSeptQuestDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptQuestData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleSeptQuestDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReqRoleSeptQuestOp(const FZReqRoleSeptQuestOpReq& InParams, const FZOnReqRoleSeptQuestOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReqRoleSeptQuestOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReqRoleSeptQuestOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReqRoleSeptQuestOpAck>& InRspMessage)
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

void UZGameRpcStub::ReqRoleSeptQuestOp(const TSharedPtr<idlezt::ReqRoleSeptQuestOpReq>& InReqMessage, const OnReqRoleSeptQuestOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReqRoleSeptQuestOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReqRoleSeptQuestOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshSeptQuest(const FZRefreshSeptQuestReq& InParams, const FZOnRefreshSeptQuestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RefreshSeptQuestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshSeptQuest(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RefreshSeptQuestAck>& InRspMessage)
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

void UZGameRpcStub::RefreshSeptQuest(const TSharedPtr<idlezt::RefreshSeptQuestReq>& InReqMessage, const OnRefreshSeptQuestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshSeptQuest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RefreshSeptQuestAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReqSeptQuestRankUp(const FZReqSeptQuestRankUpReq& InParams, const FZOnReqSeptQuestRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReqSeptQuestRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReqSeptQuestRankUp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReqSeptQuestRankUpAck>& InRspMessage)
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

void UZGameRpcStub::ReqSeptQuestRankUp(const TSharedPtr<idlezt::ReqSeptQuestRankUpReq>& InReqMessage, const OnReqSeptQuestRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReqSeptQuestRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReqSeptQuestRankUpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_BeginOccupySeptStone(const FZBeginOccupySeptStoneReq& InParams, const FZOnBeginOccupySeptStoneResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::BeginOccupySeptStoneReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    BeginOccupySeptStone(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::BeginOccupySeptStoneAck>& InRspMessage)
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

void UZGameRpcStub::BeginOccupySeptStone(const TSharedPtr<idlezt::BeginOccupySeptStoneReq>& InReqMessage, const OnBeginOccupySeptStoneResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::BeginOccupySeptStone;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::BeginOccupySeptStoneAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_EndOccupySeptStone(const FZEndOccupySeptStoneReq& InParams, const FZOnEndOccupySeptStoneResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::EndOccupySeptStoneReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    EndOccupySeptStone(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::EndOccupySeptStoneAck>& InRspMessage)
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

void UZGameRpcStub::EndOccupySeptStone(const TSharedPtr<idlezt::EndOccupySeptStoneReq>& InReqMessage, const OnEndOccupySeptStoneResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::EndOccupySeptStone;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::EndOccupySeptStoneAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OccupySeptLand(const FZOccupySeptLandReq& InParams, const FZOnOccupySeptLandResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OccupySeptLandReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OccupySeptLand(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OccupySeptLandAck>& InRspMessage)
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

void UZGameRpcStub::OccupySeptLand(const TSharedPtr<idlezt::OccupySeptLandReq>& InReqMessage, const OnOccupySeptLandResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OccupySeptLand;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OccupySeptLandAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetGongFaData(const FZGetGongFaDataReq& InParams, const FZOnGetGongFaDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetGongFaDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetGongFaData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetGongFaDataAck>& InRspMessage)
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

void UZGameRpcStub::GetGongFaData(const TSharedPtr<idlezt::GetGongFaDataReq>& InReqMessage, const OnGetGongFaDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetGongFaData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetGongFaDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GongFaOp(const FZGongFaOpReq& InParams, const FZOnGongFaOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GongFaOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GongFaOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GongFaOpAck>& InRspMessage)
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

void UZGameRpcStub::GongFaOp(const TSharedPtr<idlezt::GongFaOpReq>& InReqMessage, const OnGongFaOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GongFaOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GongFaOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ActivateGongFaMaxEffect(const FZActivateGongFaMaxEffectReq& InParams, const FZOnActivateGongFaMaxEffectResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ActivateGongFaMaxEffectReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ActivateGongFaMaxEffect(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ActivateGongFaMaxEffectAck>& InRspMessage)
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

void UZGameRpcStub::ActivateGongFaMaxEffect(const TSharedPtr<idlezt::ActivateGongFaMaxEffectReq>& InReqMessage, const OnActivateGongFaMaxEffectResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ActivateGongFaMaxEffect;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ActivateGongFaMaxEffectAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptLandDamageTopList(const FZGetSeptLandDamageTopListReq& InParams, const FZOnGetSeptLandDamageTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptLandDamageTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptLandDamageTopList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptLandDamageTopListAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptLandDamageTopList(const TSharedPtr<idlezt::GetSeptLandDamageTopListReq>& InReqMessage, const OnGetSeptLandDamageTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLandDamageTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptLandDamageTopListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveFuZengRewards(const FZReceiveFuZengRewardsReq& InParams, const FZOnReceiveFuZengRewardsResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReceiveFuZengRewardsReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveFuZengRewards(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReceiveFuZengRewardsAck>& InRspMessage)
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

void UZGameRpcStub::ReceiveFuZengRewards(const TSharedPtr<idlezt::ReceiveFuZengRewardsReq>& InReqMessage, const OnReceiveFuZengRewardsResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveFuZengRewards;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReceiveFuZengRewardsAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleFuZengData(const FZGetRoleFuZengDataReq& InParams, const FZOnGetRoleFuZengDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleFuZengDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleFuZengData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleFuZengDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleFuZengData(const TSharedPtr<idlezt::GetRoleFuZengDataReq>& InReqMessage, const OnGetRoleFuZengDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFuZengData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleFuZengDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleTreasuryData(const FZGetRoleTreasuryDataReq& InParams, const FZOnGetRoleTreasuryDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleTreasuryDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleTreasuryData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleTreasuryDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleTreasuryData(const TSharedPtr<idlezt::GetRoleTreasuryDataReq>& InReqMessage, const OnGetRoleTreasuryDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleTreasuryData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleTreasuryDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OpenTreasuryChest(const FZOpenTreasuryChestReq& InParams, const FZOnOpenTreasuryChestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OpenTreasuryChestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OpenTreasuryChest(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OpenTreasuryChestAck>& InRspMessage)
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

void UZGameRpcStub::OpenTreasuryChest(const TSharedPtr<idlezt::OpenTreasuryChestReq>& InReqMessage, const OnOpenTreasuryChestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryChest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OpenTreasuryChestAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OneClickOpenTreasuryChest(const FZOneClickOpenTreasuryChestReq& InParams, const FZOnOneClickOpenTreasuryChestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OneClickOpenTreasuryChestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OneClickOpenTreasuryChest(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OneClickOpenTreasuryChestAck>& InRspMessage)
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

void UZGameRpcStub::OneClickOpenTreasuryChest(const TSharedPtr<idlezt::OneClickOpenTreasuryChestReq>& InReqMessage, const OnOneClickOpenTreasuryChestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickOpenTreasuryChest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OneClickOpenTreasuryChestAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_OpenTreasuryGacha(const FZOpenTreasuryGachaReq& InParams, const FZOnOpenTreasuryGachaResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::OpenTreasuryGachaReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    OpenTreasuryGacha(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::OpenTreasuryGachaAck>& InRspMessage)
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

void UZGameRpcStub::OpenTreasuryGacha(const TSharedPtr<idlezt::OpenTreasuryGachaReq>& InReqMessage, const OnOpenTreasuryGachaResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryGacha;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::OpenTreasuryGachaAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RefreshTreasuryShop(const FZRefreshTreasuryShopReq& InParams, const FZOnRefreshTreasuryShopResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RefreshTreasuryShopReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RefreshTreasuryShop(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RefreshTreasuryShopAck>& InRspMessage)
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

void UZGameRpcStub::RefreshTreasuryShop(const TSharedPtr<idlezt::RefreshTreasuryShopReq>& InReqMessage, const OnRefreshTreasuryShopResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshTreasuryShop;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RefreshTreasuryShopAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_TreasuryShopBuy(const FZTreasuryShopBuyReq& InParams, const FZOnTreasuryShopBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::TreasuryShopBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    TreasuryShopBuy(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::TreasuryShopBuyAck>& InRspMessage)
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

void UZGameRpcStub::TreasuryShopBuy(const TSharedPtr<idlezt::TreasuryShopBuyReq>& InReqMessage, const OnTreasuryShopBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::TreasuryShopBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::TreasuryShopBuyAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetLifeCounterData(const FZGetLifeCounterDataReq& InParams, const FZOnGetLifeCounterDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetLifeCounterDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetLifeCounterData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetLifeCounterDataAck>& InRspMessage)
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

void UZGameRpcStub::GetLifeCounterData(const TSharedPtr<idlezt::GetLifeCounterDataReq>& InReqMessage, const OnGetLifeCounterDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetLifeCounterData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetLifeCounterDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DoQuestFight(const FZDoQuestFightReq& InParams, const FZOnDoQuestFightResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DoQuestFightReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DoQuestFight(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DoQuestFightAck>& InRspMessage)
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

void UZGameRpcStub::DoQuestFight(const TSharedPtr<idlezt::DoQuestFightReq>& InReqMessage, const OnDoQuestFightResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DoQuestFight;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DoQuestFightAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_QuestFightQuickEnd(const FZQuestFightQuickEndReq& InParams, const FZOnQuestFightQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::QuestFightQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    QuestFightQuickEnd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::QuestFightQuickEndAck>& InRspMessage)
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

void UZGameRpcStub::QuestFightQuickEnd(const TSharedPtr<idlezt::QuestFightQuickEndReq>& InReqMessage, const OnQuestFightQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::QuestFightQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::QuestFightQuickEndAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetAppearanceData(const FZGetAppearanceDataReq& InParams, const FZOnGetAppearanceDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetAppearanceDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetAppearanceData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetAppearanceDataAck>& InRspMessage)
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

void UZGameRpcStub::GetAppearanceData(const TSharedPtr<idlezt::GetAppearanceDataReq>& InReqMessage, const OnGetAppearanceDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetAppearanceData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetAppearanceDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceAdd(const FZAppearanceAddReq& InParams, const FZOnAppearanceAddResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppearanceAddReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceAdd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppearanceAddAck>& InRspMessage)
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

void UZGameRpcStub::AppearanceAdd(const TSharedPtr<idlezt::AppearanceAddReq>& InReqMessage, const OnAppearanceAddResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceAdd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppearanceAddAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceActive(const FZAppearanceActiveReq& InParams, const FZOnAppearanceActiveResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppearanceActiveReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceActive(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppearanceActiveAck>& InRspMessage)
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

void UZGameRpcStub::AppearanceActive(const TSharedPtr<idlezt::AppearanceActiveReq>& InReqMessage, const OnAppearanceActiveResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceActive;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppearanceActiveAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceWear(const FZAppearanceWearReq& InParams, const FZOnAppearanceWearResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppearanceWearReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceWear(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppearanceWearAck>& InRspMessage)
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

void UZGameRpcStub::AppearanceWear(const TSharedPtr<idlezt::AppearanceWearReq>& InReqMessage, const OnAppearanceWearResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceWear;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppearanceWearAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceBuy(const FZAppearanceBuyReq& InParams, const FZOnAppearanceBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppearanceBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceBuy(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppearanceBuyAck>& InRspMessage)
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

void UZGameRpcStub::AppearanceBuy(const TSharedPtr<idlezt::AppearanceBuyReq>& InReqMessage, const OnAppearanceBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppearanceBuyAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AppearanceChangeSkType(const FZAppearanceChangeSkTypeReq& InParams, const FZOnAppearanceChangeSkTypeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AppearanceChangeSkTypeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AppearanceChangeSkType(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AppearanceChangeSkTypeAck>& InRspMessage)
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

void UZGameRpcStub::AppearanceChangeSkType(const TSharedPtr<idlezt::AppearanceChangeSkTypeReq>& InReqMessage, const OnAppearanceChangeSkTypeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceChangeSkType;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AppearanceChangeSkTypeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBattleHistoryInfo(const FZGetBattleHistoryInfoReq& InParams, const FZOnGetBattleHistoryInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetBattleHistoryInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBattleHistoryInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetBattleHistoryInfoAck>& InRspMessage)
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

void UZGameRpcStub::GetBattleHistoryInfo(const TSharedPtr<idlezt::GetBattleHistoryInfoReq>& InReqMessage, const OnGetBattleHistoryInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBattleHistoryInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetBattleHistoryInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetArenaCheckListData(const FZGetArenaCheckListDataReq& InParams, const FZOnGetArenaCheckListDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetArenaCheckListDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetArenaCheckListData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetArenaCheckListDataAck>& InRspMessage)
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

void UZGameRpcStub::GetArenaCheckListData(const TSharedPtr<idlezt::GetArenaCheckListDataReq>& InReqMessage, const OnGetArenaCheckListDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaCheckListData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetArenaCheckListDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ArenaCheckListSubmit(const FZArenaCheckListSubmitReq& InParams, const FZOnArenaCheckListSubmitResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ArenaCheckListSubmitReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ArenaCheckListSubmit(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ArenaCheckListSubmitAck>& InRspMessage)
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

void UZGameRpcStub::ArenaCheckListSubmit(const TSharedPtr<idlezt::ArenaCheckListSubmitReq>& InReqMessage, const OnArenaCheckListSubmitResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListSubmit;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ArenaCheckListSubmitAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ArenaCheckListRewardSubmit(const FZArenaCheckListRewardSubmitReq& InParams, const FZOnArenaCheckListRewardSubmitResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ArenaCheckListRewardSubmitReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ArenaCheckListRewardSubmit(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ArenaCheckListRewardSubmitAck>& InRspMessage)
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

void UZGameRpcStub::ArenaCheckListRewardSubmit(const TSharedPtr<idlezt::ArenaCheckListRewardSubmitReq>& InReqMessage, const OnArenaCheckListRewardSubmitResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListRewardSubmit;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ArenaCheckListRewardSubmitAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllChallenge(const FZDungeonKillAllChallengeReq& InParams, const FZOnDungeonKillAllChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonKillAllChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllChallenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonKillAllChallengeAck>& InRspMessage)
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

void UZGameRpcStub::DungeonKillAllChallenge(const TSharedPtr<idlezt::DungeonKillAllChallengeReq>& InReqMessage, const OnDungeonKillAllChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonKillAllChallengeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllQuickEnd(const FZDungeonKillAllQuickEndReq& InParams, const FZOnDungeonKillAllQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonKillAllQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllQuickEnd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonKillAllQuickEndAck>& InRspMessage)
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

void UZGameRpcStub::DungeonKillAllQuickEnd(const TSharedPtr<idlezt::DungeonKillAllQuickEndReq>& InReqMessage, const OnDungeonKillAllQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonKillAllQuickEndAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonKillAllData(const FZDungeonKillAllDataReq& InParams, const FZOnDungeonKillAllDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonKillAllDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonKillAllData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonKillAllDataAck>& InRspMessage)
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

void UZGameRpcStub::DungeonKillAllData(const TSharedPtr<idlezt::DungeonKillAllDataReq>& InReqMessage, const OnDungeonKillAllDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonKillAllDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetFarmlandData(const FZGetFarmlandDataReq& InParams, const FZOnGetFarmlandDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetFarmlandDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetFarmlandData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetFarmlandDataAck>& InRspMessage)
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

void UZGameRpcStub::GetFarmlandData(const TSharedPtr<idlezt::GetFarmlandDataReq>& InReqMessage, const OnGetFarmlandDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetFarmlandData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetFarmlandDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandUnlockBlock(const FZFarmlandUnlockBlockReq& InParams, const FZOnFarmlandUnlockBlockResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandUnlockBlockReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandUnlockBlock(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandUnlockBlockAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandUnlockBlock(const TSharedPtr<idlezt::FarmlandUnlockBlockReq>& InReqMessage, const OnFarmlandUnlockBlockResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandUnlockBlock;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandUnlockBlockAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandPlantSeed(const FZFarmlandPlantSeedReq& InParams, const FZOnFarmlandPlantSeedResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandPlantSeedReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandPlantSeed(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandPlantSeedAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandPlantSeed(const TSharedPtr<idlezt::FarmlandPlantSeedReq>& InReqMessage, const OnFarmlandPlantSeedResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandPlantSeed;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandPlantSeedAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandWatering(const FZFarmlandWateringReq& InParams, const FZOnFarmlandWateringResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandWateringReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandWatering(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandWateringAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandWatering(const TSharedPtr<idlezt::FarmlandWateringReq>& InReqMessage, const OnFarmlandWateringResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandWatering;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandWateringAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandRipening(const FZFarmlandRipeningReq& InParams, const FZOnFarmlandRipeningResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandRipeningReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandRipening(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandRipeningAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandRipening(const TSharedPtr<idlezt::FarmlandRipeningReq>& InReqMessage, const OnFarmlandRipeningResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandRipening;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandRipeningAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandHarvest(const FZFarmlandHarvestReq& InParams, const FZOnFarmlandHarvestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandHarvestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandHarvest(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandHarvestAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandHarvest(const TSharedPtr<idlezt::FarmlandHarvestReq>& InReqMessage, const OnFarmlandHarvestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandHarvest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandHarvestAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmerRankUp(const FZFarmerRankUpReq& InParams, const FZOnFarmerRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmerRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmerRankUp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmerRankUpAck>& InRspMessage)
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

void UZGameRpcStub::FarmerRankUp(const TSharedPtr<idlezt::FarmerRankUpReq>& InReqMessage, const OnFarmerRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmerRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmerRankUpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FarmlandSetManagement(const FZFarmlandSetManagementReq& InParams, const FZOnFarmlandSetManagementResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FarmlandSetManagementReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FarmlandSetManagement(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FarmlandSetManagementAck>& InRspMessage)
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

void UZGameRpcStub::FarmlandSetManagement(const TSharedPtr<idlezt::FarmlandSetManagementReq>& InReqMessage, const OnFarmlandSetManagementResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandSetManagement;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FarmlandSetManagementAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_UpdateFarmlandState(const FZUpdateFarmlandStateReq& InParams, const FZOnUpdateFarmlandStateResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::UpdateFarmlandStateReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    UpdateFarmlandState(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::UpdateFarmlandStateAck>& InRspMessage)
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

void UZGameRpcStub::UpdateFarmlandState(const TSharedPtr<idlezt::UpdateFarmlandStateReq>& InReqMessage, const OnUpdateFarmlandStateResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateFarmlandState;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::UpdateFarmlandStateAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveChallenge(const FZDungeonSurviveChallengeReq& InParams, const FZOnDungeonSurviveChallengeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonSurviveChallengeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveChallenge(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonSurviveChallengeAck>& InRspMessage)
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

void UZGameRpcStub::DungeonSurviveChallenge(const TSharedPtr<idlezt::DungeonSurviveChallengeReq>& InReqMessage, const OnDungeonSurviveChallengeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveChallenge;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonSurviveChallengeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveQuickEnd(const FZDungeonSurviveQuickEndReq& InParams, const FZOnDungeonSurviveQuickEndResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonSurviveQuickEndReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveQuickEnd(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonSurviveQuickEndAck>& InRspMessage)
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

void UZGameRpcStub::DungeonSurviveQuickEnd(const TSharedPtr<idlezt::DungeonSurviveQuickEndReq>& InReqMessage, const OnDungeonSurviveQuickEndResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveQuickEnd;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonSurviveQuickEndAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DungeonSurviveData(const FZDungeonSurviveDataReq& InParams, const FZOnDungeonSurviveDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DungeonSurviveDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DungeonSurviveData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DungeonSurviveDataAck>& InRspMessage)
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

void UZGameRpcStub::DungeonSurviveData(const TSharedPtr<idlezt::DungeonSurviveDataReq>& InReqMessage, const OnDungeonSurviveDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DungeonSurviveDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRevertAllSkillCoolDown(const FZGetRevertAllSkillCoolDownReq& InParams, const FZOnGetRevertAllSkillCoolDownResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRevertAllSkillCoolDownReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRevertAllSkillCoolDown(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRevertAllSkillCoolDownAck>& InRspMessage)
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

void UZGameRpcStub::GetRevertAllSkillCoolDown(const TSharedPtr<idlezt::GetRevertAllSkillCoolDownReq>& InReqMessage, const OnGetRevertAllSkillCoolDownResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRevertAllSkillCoolDown;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRevertAllSkillCoolDownAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleFriendData(const FZGetRoleFriendDataReq& InParams, const FZOnGetRoleFriendDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleFriendDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleFriendData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleFriendDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleFriendData(const TSharedPtr<idlezt::GetRoleFriendDataReq>& InReqMessage, const OnGetRoleFriendDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFriendData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleFriendDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FriendOp(const FZFriendOpReq& InParams, const FZOnFriendOpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FriendOpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FriendOp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FriendOpAck>& InRspMessage)
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

void UZGameRpcStub::FriendOp(const TSharedPtr<idlezt::FriendOpReq>& InReqMessage, const OnFriendOpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FriendOp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FriendOpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReplyFriendRequest(const FZReplyFriendRequestReq& InParams, const FZOnReplyFriendRequestResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReplyFriendRequestReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReplyFriendRequest(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReplyFriendRequestAck>& InRspMessage)
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

void UZGameRpcStub::ReplyFriendRequest(const TSharedPtr<idlezt::ReplyFriendRequestReq>& InReqMessage, const OnReplyFriendRequestResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReplyFriendRequest;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReplyFriendRequestAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_FriendSearchRoleInfo(const FZFriendSearchRoleInfoReq& InParams, const FZOnFriendSearchRoleInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::FriendSearchRoleInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    FriendSearchRoleInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::FriendSearchRoleInfoAck>& InRspMessage)
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

void UZGameRpcStub::FriendSearchRoleInfo(const TSharedPtr<idlezt::FriendSearchRoleInfoReq>& InReqMessage, const OnFriendSearchRoleInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::FriendSearchRoleInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::FriendSearchRoleInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleInfoCache(const FZGetRoleInfoCacheReq& InParams, const FZOnGetRoleInfoCacheResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleInfoCacheReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleInfoCache(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleInfoCacheAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleInfoCache(const TSharedPtr<idlezt::GetRoleInfoCacheReq>& InReqMessage, const OnGetRoleInfoCacheResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfoCache;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleInfoCacheAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleInfo(const FZGetRoleInfoReq& InParams, const FZOnGetRoleInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleInfoAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleInfo(const TSharedPtr<idlezt::GetRoleInfoReq>& InReqMessage, const OnGetRoleInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleAvatarData(const FZGetRoleAvatarDataReq& InParams, const FZOnGetRoleAvatarDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleAvatarDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleAvatarData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleAvatarDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleAvatarData(const TSharedPtr<idlezt::GetRoleAvatarDataReq>& InReqMessage, const OnGetRoleAvatarDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAvatarData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleAvatarDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_DispatchAvatar(const FZDispatchAvatarReq& InParams, const FZOnDispatchAvatarResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::DispatchAvatarReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    DispatchAvatar(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::DispatchAvatarAck>& InRspMessage)
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

void UZGameRpcStub::DispatchAvatar(const TSharedPtr<idlezt::DispatchAvatarReq>& InReqMessage, const OnDispatchAvatarResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::DispatchAvatar;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::DispatchAvatarAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AvatarRankUp(const FZAvatarRankUpReq& InParams, const FZOnAvatarRankUpResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AvatarRankUpReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AvatarRankUp(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AvatarRankUpAck>& InRspMessage)
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

void UZGameRpcStub::AvatarRankUp(const TSharedPtr<idlezt::AvatarRankUpReq>& InReqMessage, const OnAvatarRankUpResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AvatarRankUp;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AvatarRankUpAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveAvatarTempPackage(const FZReceiveAvatarTempPackageReq& InParams, const FZOnReceiveAvatarTempPackageResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReceiveAvatarTempPackageReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveAvatarTempPackage(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReceiveAvatarTempPackageAck>& InRspMessage)
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

void UZGameRpcStub::ReceiveAvatarTempPackage(const TSharedPtr<idlezt::ReceiveAvatarTempPackageReq>& InReqMessage, const OnReceiveAvatarTempPackageResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveAvatarTempPackage;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReceiveAvatarTempPackageAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetArenaExplorationStatisticalData(const FZGetArenaExplorationStatisticalDataReq& InParams, const FZOnGetArenaExplorationStatisticalDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetArenaExplorationStatisticalDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetArenaExplorationStatisticalData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetArenaExplorationStatisticalDataAck>& InRspMessage)
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

void UZGameRpcStub::GetArenaExplorationStatisticalData(const TSharedPtr<idlezt::GetArenaExplorationStatisticalDataReq>& InReqMessage, const OnGetArenaExplorationStatisticalDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaExplorationStatisticalData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetArenaExplorationStatisticalDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleBiographyData(const FZGetRoleBiographyDataReq& InParams, const FZOnGetRoleBiographyDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleBiographyDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleBiographyData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleBiographyDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleBiographyData(const TSharedPtr<idlezt::GetRoleBiographyDataReq>& InReqMessage, const OnGetRoleBiographyDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleBiographyData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleBiographyDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBiographyItem(const FZReceiveBiographyItemReq& InParams, const FZOnReceiveBiographyItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReceiveBiographyItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBiographyItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReceiveBiographyItemAck>& InRspMessage)
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

void UZGameRpcStub::ReceiveBiographyItem(const TSharedPtr<idlezt::ReceiveBiographyItemReq>& InReqMessage, const OnReceiveBiographyItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReceiveBiographyItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetBiographyEventData(const FZGetBiographyEventDataReq& InParams, const FZOnGetBiographyEventDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetBiographyEventDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetBiographyEventData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetBiographyEventDataAck>& InRspMessage)
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

void UZGameRpcStub::GetBiographyEventData(const TSharedPtr<idlezt::GetBiographyEventDataReq>& InReqMessage, const OnGetBiographyEventDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetBiographyEventData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetBiographyEventDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_ReceiveBiographyEventItem(const FZReceiveBiographyEventItemReq& InParams, const FZOnReceiveBiographyEventItemResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::ReceiveBiographyEventItemReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    ReceiveBiographyEventItem(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ReceiveBiographyEventItemAck>& InRspMessage)
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

void UZGameRpcStub::ReceiveBiographyEventItem(const TSharedPtr<idlezt::ReceiveBiographyEventItemReq>& InReqMessage, const OnReceiveBiographyEventItemResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyEventItem;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::ReceiveBiographyEventItemAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_AddBiographyRoleLog(const FZAddBiographyRoleLogReq& InParams, const FZOnAddBiographyRoleLogResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::AddBiographyRoleLogReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    AddBiographyRoleLog(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::AddBiographyRoleLogAck>& InRspMessage)
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

void UZGameRpcStub::AddBiographyRoleLog(const TSharedPtr<idlezt::AddBiographyRoleLogReq>& InReqMessage, const OnAddBiographyRoleLogResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::AddBiographyRoleLog;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::AddBiographyRoleLogAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestEnterSeptDemonWorld(const FZRequestEnterSeptDemonWorldReq& InParams, const FZOnRequestEnterSeptDemonWorldResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestEnterSeptDemonWorldReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestEnterSeptDemonWorld(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestEnterSeptDemonWorldAck>& InRspMessage)
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

void UZGameRpcStub::RequestEnterSeptDemonWorld(const TSharedPtr<idlezt::RequestEnterSeptDemonWorldReq>& InReqMessage, const OnRequestEnterSeptDemonWorldResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestEnterSeptDemonWorld;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestEnterSeptDemonWorldAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestLeaveSeptDemonWorld(const FZRequestLeaveSeptDemonWorldReq& InParams, const FZOnRequestLeaveSeptDemonWorldResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestLeaveSeptDemonWorldReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestLeaveSeptDemonWorld(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestLeaveSeptDemonWorldAck>& InRspMessage)
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

void UZGameRpcStub::RequestLeaveSeptDemonWorld(const TSharedPtr<idlezt::RequestLeaveSeptDemonWorldReq>& InReqMessage, const OnRequestLeaveSeptDemonWorldResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestLeaveSeptDemonWorld;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestLeaveSeptDemonWorldAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestSeptDemonWorldData(const FZRequestSeptDemonWorldDataReq& InParams, const FZOnRequestSeptDemonWorldDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestSeptDemonWorldDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestSeptDemonWorldData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestSeptDemonWorldDataAck>& InRspMessage)
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

void UZGameRpcStub::RequestSeptDemonWorldData(const TSharedPtr<idlezt::RequestSeptDemonWorldDataReq>& InReqMessage, const OnRequestSeptDemonWorldDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestSeptDemonWorldData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestSeptDemonWorldDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_RequestInSeptDemonWorldEndTime(const FZRequestInSeptDemonWorldEndTimeReq& InParams, const FZOnRequestInSeptDemonWorldEndTimeResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::RequestInSeptDemonWorldEndTimeReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    RequestInSeptDemonWorldEndTime(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeAck>& InRspMessage)
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

void UZGameRpcStub::RequestInSeptDemonWorldEndTime(const TSharedPtr<idlezt::RequestInSeptDemonWorldEndTimeReq>& InReqMessage, const OnRequestInSeptDemonWorldEndTimeResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::RequestInSeptDemonWorldEndTime;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::RequestInSeptDemonWorldEndTimeAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageTopList(const FZGetSeptDemonDamageTopListReq& InParams, const FZOnGetSeptDemonDamageTopListResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageTopListReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageTopList(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageTopListAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonDamageTopList(const TSharedPtr<idlezt::GetSeptDemonDamageTopListReq>& InReqMessage, const OnGetSeptDemonDamageTopListResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageTopList;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageTopListAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageSelfSummary(const FZGetSeptDemonDamageSelfSummaryReq& InParams, const FZOnGetSeptDemonDamageSelfSummaryResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageSelfSummaryReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageSelfSummary(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonDamageSelfSummary(const TSharedPtr<idlezt::GetSeptDemonDamageSelfSummaryReq>& InReqMessage, const OnGetSeptDemonDamageSelfSummaryResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageSelfSummary;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageSelfSummaryAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonStageRewardNum(const FZGetSeptDemonStageRewardNumReq& InParams, const FZOnGetSeptDemonStageRewardNumResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonStageRewardNumReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonStageRewardNum(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonStageRewardNumAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonStageRewardNum(const TSharedPtr<idlezt::GetSeptDemonStageRewardNumReq>& InReqMessage, const OnGetSeptDemonStageRewardNumResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageRewardNum;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonStageRewardNumAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonStageReward(const FZGetSeptDemonStageRewardReq& InParams, const FZOnGetSeptDemonStageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonStageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonStageReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonStageRewardAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonStageReward(const TSharedPtr<idlezt::GetSeptDemonStageRewardReq>& InReqMessage, const OnGetSeptDemonStageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonStageRewardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageRewardsInfo(const FZGetSeptDemonDamageRewardsInfoReq& InParams, const FZOnGetSeptDemonDamageRewardsInfoResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageRewardsInfoReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageRewardsInfo(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonDamageRewardsInfo(const TSharedPtr<idlezt::GetSeptDemonDamageRewardsInfoReq>& InReqMessage, const OnGetSeptDemonDamageRewardsInfoResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageRewardsInfo;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageRewardsInfoAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetSeptDemonDamageReward(const FZGetSeptDemonDamageRewardReq& InParams, const FZOnGetSeptDemonDamageRewardResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageRewardReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetSeptDemonDamageReward(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetSeptDemonDamageRewardAck>& InRspMessage)
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

void UZGameRpcStub::GetSeptDemonDamageReward(const TSharedPtr<idlezt::GetSeptDemonDamageRewardReq>& InReqMessage, const OnGetSeptDemonDamageRewardResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageReward;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageRewardAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_GetRoleVipShopData(const FZGetRoleVipShopDataReq& InParams, const FZOnGetRoleVipShopDataResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::GetRoleVipShopDataReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    GetRoleVipShopData(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::GetRoleVipShopDataAck>& InRspMessage)
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

void UZGameRpcStub::GetRoleVipShopData(const TSharedPtr<idlezt::GetRoleVipShopDataReq>& InReqMessage, const OnGetRoleVipShopDataResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleVipShopData;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::GetRoleVipShopDataAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}


void UZGameRpcStub::K2_VipShopBuy(const FZVipShopBuyReq& InParams, const FZOnVipShopBuyResult& InCallback)
{
    if (!Manager || !Connection)
        return;
        
    auto ReqMessage = MakeShared<idlezt::VipShopBuyReq>();
    InParams.ToPb(&ReqMessage.Get());  // USTRUCT -> PB
    
    VipShopBuy(ReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::VipShopBuyAck>& InRspMessage)
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

void UZGameRpcStub::VipShopBuy(const TSharedPtr<idlezt::VipShopBuyReq>& InReqMessage, const OnVipShopBuyResult& InCallback)
{   
    if (!Manager || !Connection)
        return;

    static constexpr uint64 RpcId = FZGameRpcInterface::VipShopBuy;
    Manager->CallRpc(Connection.Get(), RpcId, InReqMessage, [InCallback](EZRpcErrorCode ErrorCode, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        auto RspMessage = MakeShared<idlezt::VipShopBuyAck>();               
        if (ErrorCode == EZRpcErrorCode::RpcErrorCode_Ok)
        {
            if (!RspMessage->ParseFromString(InMessage->body_data()))
            {
                ErrorCode = EZRpcErrorCode::RpcErrorCode_RspDataError;
            }
        }
        InCallback(ErrorCode, RspMessage);
    });
}



































       


