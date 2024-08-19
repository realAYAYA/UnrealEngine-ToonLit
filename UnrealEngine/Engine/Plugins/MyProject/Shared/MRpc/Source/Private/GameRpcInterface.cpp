#include "GameRpcInterface.h"

FZGameRpcInterface::FZGameRpcInterface(FMRpcManager* InManager)
{
}

FZGameRpcInterface::~FZGameRpcInterface()
{
}

void FZGameRpcInterface::LoginGameRegister(FMRpcManager* InManager, const FZLoginGameCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LoginGame;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LoginGameReq>();
        auto RspMessage = MakeShared<idlepb::LoginGameAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetCurrentCultivationDirectionRegister(FMRpcManager* InManager, const FZSetCurrentCultivationDirectionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetCurrentCultivationDirection;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetCurrentCultivationDirectionReq>();
        auto RspMessage = MakeShared<idlepb::SetCurrentCultivationDirectionAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DoBreakthroughRegister(FMRpcManager* InManager, const FZDoBreakthroughCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DoBreakthrough;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DoBreakthroughReq>();
        auto RspMessage = MakeShared<idlepb::DoBreakthroughAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestCommonCultivationDataRegister(FMRpcManager* InManager, const FZRequestCommonCultivationDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestCommonCultivationData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestCommonCultivationDataReq>();
        auto RspMessage = MakeShared<idlepb::RequestCommonCultivationDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickMergeBreathingRegister(FMRpcManager* InManager, const FZOneClickMergeBreathingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickMergeBreathing;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickMergeBreathingReq>();
        auto RspMessage = MakeShared<idlepb::OneClickMergeBreathingAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBreathingExerciseRewardRegister(FMRpcManager* InManager, const FZReceiveBreathingExerciseRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBreathingExerciseReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReceiveBreathingExerciseRewardReq>();
        auto RspMessage = MakeShared<idlepb::ReceiveBreathingExerciseRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetInventoryDataRegister(FMRpcManager* InManager, const FZGetInventoryDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetInventoryData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetInventoryDataReq>();
        auto RspMessage = MakeShared<idlepb::GetInventoryDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetQuestDataRegister(FMRpcManager* InManager, const FZGetQuestDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetQuestData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetQuestDataReq>();
        auto RspMessage = MakeShared<idlepb::GetQuestDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::CreateCharacterRegister(FMRpcManager* InManager, const FZCreateCharacterCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::CreateCharacter;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::CreateCharacterReq>();
        auto RspMessage = MakeShared<idlepb::CreateCharacterAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseItemRegister(FMRpcManager* InManager, const FZUseItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UseItemReq>();
        auto RspMessage = MakeShared<idlepb::UseItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseSelectGiftRegister(FMRpcManager* InManager, const FZUseSelectGiftCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseSelectGift;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UseSelectGiftReq>();
        auto RspMessage = MakeShared<idlepb::UseSelectGiftAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SellItemRegister(FMRpcManager* InManager, const FZSellItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SellItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SellItemReq>();
        auto RspMessage = MakeShared<idlepb::SellItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockEquipmentSlotRegister(FMRpcManager* InManager, const FZUnlockEquipmentSlotCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockEquipmentSlot;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UnlockEquipmentSlotReq>();
        auto RspMessage = MakeShared<idlepb::UnlockEquipmentSlotAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineStartRegister(FMRpcManager* InManager, const FZAlchemyRefineStartCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineStart;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AlchemyRefineStartReq>();
        auto RspMessage = MakeShared<idlepb::AlchemyRefineStartAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineCancelRegister(FMRpcManager* InManager, const FZAlchemyRefineCancelCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineCancel;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AlchemyRefineCancelReq>();
        auto RspMessage = MakeShared<idlepb::AlchemyRefineCancelAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineExtractRegister(FMRpcManager* InManager, const FZAlchemyRefineExtractCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineExtract;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AlchemyRefineExtractReq>();
        auto RspMessage = MakeShared<idlepb::AlchemyRefineExtractAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleShopDataRegister(FMRpcManager* InManager, const FZGetRoleShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleShopData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleShopDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshShopRegister(FMRpcManager* InManager, const FZRefreshShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshShop;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RefreshShopReq>();
        auto RspMessage = MakeShared<idlepb::RefreshShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuyShopItemRegister(FMRpcManager* InManager, const FZBuyShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuyShopItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::BuyShopItemReq>();
        auto RspMessage = MakeShared<idlepb::BuyShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleDeluxeShopDataRegister(FMRpcManager* InManager, const FZGetRoleDeluxeShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleDeluxeShopData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleDeluxeShopDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleDeluxeShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshDeluxeShopRegister(FMRpcManager* InManager, const FZRefreshDeluxeShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshDeluxeShop;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RefreshDeluxeShopReq>();
        auto RspMessage = MakeShared<idlepb::RefreshDeluxeShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuyDeluxeShopItemRegister(FMRpcManager* InManager, const FZBuyDeluxeShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuyDeluxeShopItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::BuyDeluxeShopItemReq>();
        auto RspMessage = MakeShared<idlepb::BuyDeluxeShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetTemporaryPackageDataRegister(FMRpcManager* InManager, const FZGetTemporaryPackageDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetTemporaryPackageData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetTemporaryPackageDataReq>();
        auto RspMessage = MakeShared<idlepb::GetTemporaryPackageDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExtractTemporaryPackageItemsRegister(FMRpcManager* InManager, const FZExtractTemporaryPackageItemsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExtractTemporaryPackageItems;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ExtractTemporaryPackageItemsReq>();
        auto RspMessage = MakeShared<idlepb::ExtractTemporaryPackageItemsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SpeedupReliveRegister(FMRpcManager* InManager, const FZSpeedupReliveCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SpeedupRelive;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SpeedupReliveReq>();
        auto RspMessage = MakeShared<idlepb::SpeedupReliveAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMapInfoRegister(FMRpcManager* InManager, const FZGetMapInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMapInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetMapInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetMapInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockArenaRegister(FMRpcManager* InManager, const FZUnlockArenaCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockArena;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UnlockArenaReq>();
        auto RspMessage = MakeShared<idlepb::UnlockArenaAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QuestOpRegister(FMRpcManager* InManager, const FZQuestOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QuestOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::QuestOpReq>();
        auto RspMessage = MakeShared<idlepb::QuestOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EquipmentPutOnRegister(FMRpcManager* InManager, const FZEquipmentPutOnCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentPutOn;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::EquipmentPutOnReq>();
        auto RspMessage = MakeShared<idlepb::EquipmentPutOnAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EquipmentTakeOffRegister(FMRpcManager* InManager, const FZEquipmentTakeOffCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentTakeOff;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::EquipmentTakeOffReq>();
        auto RspMessage = MakeShared<idlepb::EquipmentTakeOffAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLeaderboardPreviewRegister(FMRpcManager* InManager, const FZGetLeaderboardPreviewCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardPreview;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetLeaderboardPreviewReq>();
        auto RspMessage = MakeShared<idlepb::GetLeaderboardPreviewAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLeaderboardDataRegister(FMRpcManager* InManager, const FZGetLeaderboardDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetLeaderboardDataReq>();
        auto RspMessage = MakeShared<idlepb::GetLeaderboardDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleLeaderboardDataRegister(FMRpcManager* InManager, const FZGetRoleLeaderboardDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleLeaderboardData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleLeaderboardDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleLeaderboardDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LeaderboardClickLikeRegister(FMRpcManager* InManager, const FZLeaderboardClickLikeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardClickLike;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LeaderboardClickLikeReq>();
        auto RspMessage = MakeShared<idlepb::LeaderboardClickLikeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LeaderboardUpdateMessageRegister(FMRpcManager* InManager, const FZLeaderboardUpdateMessageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardUpdateMessage;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LeaderboardUpdateMessageReq>();
        auto RspMessage = MakeShared<idlepb::LeaderboardUpdateMessageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetFuZeRewardRegister(FMRpcManager* InManager, const FZGetFuZeRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetFuZeReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetFuZeRewardReq>();
        auto RspMessage = MakeShared<idlepb::GetFuZeRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleMailDataRegister(FMRpcManager* InManager, const FZGetRoleMailDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleMailData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleMailDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleMailDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReadMailRegister(FMRpcManager* InManager, const FZReadMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReadMail;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReadMailReq>();
        auto RspMessage = MakeShared<idlepb::ReadMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMailAttachmentRegister(FMRpcManager* InManager, const FZGetMailAttachmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMailAttachment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetMailAttachmentReq>();
        auto RspMessage = MakeShared<idlepb::GetMailAttachmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DeleteMailRegister(FMRpcManager* InManager, const FZDeleteMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DeleteMail;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DeleteMailReq>();
        auto RspMessage = MakeShared<idlepb::DeleteMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickGetMailAttachmentRegister(FMRpcManager* InManager, const FZOneClickGetMailAttachmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickGetMailAttachment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickGetMailAttachmentReq>();
        auto RspMessage = MakeShared<idlepb::OneClickGetMailAttachmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickReadMailRegister(FMRpcManager* InManager, const FZOneClickReadMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickReadMail;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickReadMailReq>();
        auto RspMessage = MakeShared<idlepb::OneClickReadMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickDeleteMailRegister(FMRpcManager* InManager, const FZOneClickDeleteMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickDeleteMail;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickDeleteMailReq>();
        auto RspMessage = MakeShared<idlepb::OneClickDeleteMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockFunctionModuleRegister(FMRpcManager* InManager, const FZUnlockFunctionModuleCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockFunctionModule;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UnlockFunctionModuleReq>();
        auto RspMessage = MakeShared<idlepb::UnlockFunctionModuleAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetChatRecordRegister(FMRpcManager* InManager, const FZGetChatRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetChatRecord;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetChatRecordReq>();
        auto RspMessage = MakeShared<idlepb::GetChatRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DeletePrivateChatRecordRegister(FMRpcManager* InManager, const FZDeletePrivateChatRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DeletePrivateChatRecord;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DeletePrivateChatRecordReq>();
        auto RspMessage = MakeShared<idlepb::DeletePrivateChatRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SendChatMessageRegister(FMRpcManager* InManager, const FZSendChatMessageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SendChatMessage;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SendChatMessageReq>();
        auto RspMessage = MakeShared<idlepb::SendChatMessageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ClearChatUnreadNumRegister(FMRpcManager* InManager, const FZClearChatUnreadNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ClearChatUnreadNum;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ClearChatUnreadNumReq>();
        auto RspMessage = MakeShared<idlepb::ClearChatUnreadNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineStartRegister(FMRpcManager* InManager, const FZForgeRefineStartCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineStart;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ForgeRefineStartReq>();
        auto RspMessage = MakeShared<idlepb::ForgeRefineStartAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineCancelRegister(FMRpcManager* InManager, const FZForgeRefineCancelCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineCancel;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ForgeRefineCancelReq>();
        auto RspMessage = MakeShared<idlepb::ForgeRefineCancelAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineExtractRegister(FMRpcManager* InManager, const FZForgeRefineExtractCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineExtract;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ForgeRefineExtractReq>();
        auto RspMessage = MakeShared<idlepb::ForgeRefineExtractAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetForgeLostEquipmentDataRegister(FMRpcManager* InManager, const FZGetForgeLostEquipmentDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetForgeLostEquipmentData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetForgeLostEquipmentDataReq>();
        auto RspMessage = MakeShared<idlepb::GetForgeLostEquipmentDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeDestroyRegister(FMRpcManager* InManager, const FZForgeDestroyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeDestroy;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ForgeDestroyReq>();
        auto RspMessage = MakeShared<idlepb::ForgeDestroyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeFindBackRegister(FMRpcManager* InManager, const FZForgeFindBackCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeFindBack;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ForgeFindBackReq>();
        auto RspMessage = MakeShared<idlepb::ForgeFindBackAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestPillElixirDataRegister(FMRpcManager* InManager, const FZRequestPillElixirDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestPillElixirData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestPillElixirDataReq>();
        auto RspMessage = MakeShared<idlepb::RequestPillElixirDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetOnePillElixirDataRegister(FMRpcManager* InManager, const FZGetOnePillElixirDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetOnePillElixirData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetOnePillElixirDataReq>();
        auto RspMessage = MakeShared<idlepb::GetOnePillElixirDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestModifyPillElixirFilterRegister(FMRpcManager* InManager, const FZRequestModifyPillElixirFilterCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestModifyPillElixirFilter;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestModifyPillElixirFilterReq>();
        auto RspMessage = MakeShared<idlepb::RequestModifyPillElixirFilterAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UsePillElixirRegister(FMRpcManager* InManager, const FZUsePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UsePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UsePillElixirReq>();
        auto RspMessage = MakeShared<idlepb::UsePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickUsePillElixirRegister(FMRpcManager* InManager, const FZOneClickUsePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickUsePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickUsePillElixirReq>();
        auto RspMessage = MakeShared<idlepb::OneClickUsePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::TradePillElixirRegister(FMRpcManager* InManager, const FZTradePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::TradePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::TradePillElixirReq>();
        auto RspMessage = MakeShared<idlepb::TradePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReinforceEquipmentRegister(FMRpcManager* InManager, const FZReinforceEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReinforceEquipment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReinforceEquipmentReq>();
        auto RspMessage = MakeShared<idlepb::ReinforceEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefineEquipmentRegister(FMRpcManager* InManager, const FZRefineEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefineEquipment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RefineEquipmentReq>();
        auto RspMessage = MakeShared<idlepb::RefineEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QiWenEquipmentRegister(FMRpcManager* InManager, const FZQiWenEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QiWenEquipment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::QiWenEquipmentReq>();
        auto RspMessage = MakeShared<idlepb::QiWenEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ResetEquipmentRegister(FMRpcManager* InManager, const FZResetEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ResetEquipment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ResetEquipmentReq>();
        auto RspMessage = MakeShared<idlepb::ResetEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::InheritEquipmentRegister(FMRpcManager* InManager, const FZInheritEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::InheritEquipment;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::InheritEquipmentReq>();
        auto RspMessage = MakeShared<idlepb::InheritEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LockItemRegister(FMRpcManager* InManager, const FZLockItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LockItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::LockItemReq>();
        auto RspMessage = MakeShared<idlepb::LockItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SoloArenaChallengeRegister(FMRpcManager* InManager, const FZSoloArenaChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaChallenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SoloArenaChallengeReq>();
        auto RspMessage = MakeShared<idlepb::SoloArenaChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SoloArenaQuickEndRegister(FMRpcManager* InManager, const FZSoloArenaQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SoloArenaQuickEndReq>();
        auto RspMessage = MakeShared<idlepb::SoloArenaQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSoloArenaHistoryListRegister(FMRpcManager* InManager, const FZGetSoloArenaHistoryListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSoloArenaHistoryList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSoloArenaHistoryListReq>();
        auto RspMessage = MakeShared<idlepb::GetSoloArenaHistoryListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerChallengeRegister(FMRpcManager* InManager, const FZMonsterTowerChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerChallenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::MonsterTowerChallengeReq>();
        auto RspMessage = MakeShared<idlepb::MonsterTowerChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerDrawIdleAwardRegister(FMRpcManager* InManager, const FZMonsterTowerDrawIdleAwardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerDrawIdleAward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::MonsterTowerDrawIdleAwardReq>();
        auto RspMessage = MakeShared<idlepb::MonsterTowerDrawIdleAwardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerClosedDoorTrainingRegister(FMRpcManager* InManager, const FZMonsterTowerClosedDoorTrainingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerClosedDoorTraining;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::MonsterTowerClosedDoorTrainingReq>();
        auto RspMessage = MakeShared<idlepb::MonsterTowerClosedDoorTrainingAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerQuickEndRegister(FMRpcManager* InManager, const FZMonsterTowerQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::MonsterTowerQuickEndReq>();
        auto RspMessage = MakeShared<idlepb::MonsterTowerQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMonsterTowerChallengeListRegister(FMRpcManager* InManager, const FZGetMonsterTowerChallengeListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetMonsterTowerChallengeListReq>();
        auto RspMessage = MakeShared<idlepb::GetMonsterTowerChallengeListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMonsterTowerChallengeRewardRegister(FMRpcManager* InManager, const FZGetMonsterTowerChallengeRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetMonsterTowerChallengeRewardReq>();
        auto RspMessage = MakeShared<idlepb::GetMonsterTowerChallengeRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetWorldTimeDilationRegister(FMRpcManager* InManager, const FZSetWorldTimeDilationCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetWorldTimeDilation;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetWorldTimeDilationReq>();
        auto RspMessage = MakeShared<idlepb::SetWorldTimeDilationAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetFightModeRegister(FMRpcManager* InManager, const FZSetFightModeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetFightMode;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetFightModeReq>();
        auto RspMessage = MakeShared<idlepb::SetFightModeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpgradeQiCollectorRegister(FMRpcManager* InManager, const FZUpgradeQiCollectorCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpgradeQiCollector;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UpgradeQiCollectorReq>();
        auto RspMessage = MakeShared<idlepb::UpgradeQiCollectorAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleAllStatsRegister(FMRpcManager* InManager, const FZGetRoleAllStatsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAllStats;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleAllStatsReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleAllStatsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShanhetuDataRegister(FMRpcManager* InManager, const FZGetShanhetuDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetShanhetuDataReq>();
        auto RspMessage = MakeShared<idlepb::GetShanhetuDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetShanhetuUseConfigRegister(FMRpcManager* InManager, const FZSetShanhetuUseConfigCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetShanhetuUseConfig;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetShanhetuUseConfigReq>();
        auto RspMessage = MakeShared<idlepb::SetShanhetuUseConfigAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseShanhetuRegister(FMRpcManager* InManager, const FZUseShanhetuCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseShanhetu;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UseShanhetuReq>();
        auto RspMessage = MakeShared<idlepb::UseShanhetuAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::StepShanhetuRegister(FMRpcManager* InManager, const FZStepShanhetuCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::StepShanhetu;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::StepShanhetuReq>();
        auto RspMessage = MakeShared<idlepb::StepShanhetuAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShanhetuUseRecordRegister(FMRpcManager* InManager, const FZGetShanhetuUseRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuUseRecord;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetShanhetuUseRecordReq>();
        auto RspMessage = MakeShared<idlepb::GetShanhetuUseRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetAttackLockTypeRegister(FMRpcManager* InManager, const FZSetAttackLockTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackLockType;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetAttackLockTypeReq>();
        auto RspMessage = MakeShared<idlepb::SetAttackLockTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetAttackUnlockTypeRegister(FMRpcManager* InManager, const FZSetAttackUnlockTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackUnlockType;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetAttackUnlockTypeReq>();
        auto RspMessage = MakeShared<idlepb::SetAttackUnlockTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetShowUnlockButtonRegister(FMRpcManager* InManager, const FZSetShowUnlockButtonCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetShowUnlockButton;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetShowUnlockButtonReq>();
        auto RspMessage = MakeShared<idlepb::SetShowUnlockButtonAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetUserVarRegister(FMRpcManager* InManager, const FZGetUserVarCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVar;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetUserVarReq>();
        auto RspMessage = MakeShared<idlepb::GetUserVarRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetUserVarsRegister(FMRpcManager* InManager, const FZGetUserVarsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVars;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetUserVarsReq>();
        auto RspMessage = MakeShared<idlepb::GetUserVarsRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionArenaSummaryRegister(FMRpcManager* InManager, const FZGetBossInvasionArenaSummaryCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaSummary;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetBossInvasionArenaSummaryReq>();
        auto RspMessage = MakeShared<idlepb::GetBossInvasionArenaSummaryRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionArenaTopListRegister(FMRpcManager* InManager, const FZGetBossInvasionArenaTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaTopList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetBossInvasionArenaTopListReq>();
        auto RspMessage = MakeShared<idlepb::GetBossInvasionArenaTopListRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionInfoRegister(FMRpcManager* InManager, const FZGetBossInvasionInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetBossInvasionInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetBossInvasionInfoRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DrawBossInvasionKillRewardRegister(FMRpcManager* InManager, const FZDrawBossInvasionKillRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionKillReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DrawBossInvasionKillRewardReq>();
        auto RspMessage = MakeShared<idlepb::DrawBossInvasionKillRewardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DrawBossInvasionDamageRewardRegister(FMRpcManager* InManager, const FZDrawBossInvasionDamageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionDamageReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DrawBossInvasionDamageRewardReq>();
        auto RspMessage = MakeShared<idlepb::DrawBossInvasionDamageRewardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BossInvasionTeleportRegister(FMRpcManager* InManager, const FZBossInvasionTeleportCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BossInvasionTeleport;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::BossInvasionTeleportReq>();
        auto RspMessage = MakeShared<idlepb::BossInvasionTeleportRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfItemRegister(FMRpcManager* InManager, const FZShareSelfItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ShareSelfItemReq>();
        auto RspMessage = MakeShared<idlepb::ShareSelfItemRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfItemsRegister(FMRpcManager* InManager, const FZShareSelfItemsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItems;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ShareSelfItemsReq>();
        auto RspMessage = MakeShared<idlepb::ShareSelfItemsRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShareItemDataRegister(FMRpcManager* InManager, const FZGetShareItemDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareItemData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetShareItemDataReq>();
        auto RspMessage = MakeShared<idlepb::GetShareItemDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleCollectionDataRegister(FMRpcManager* InManager, const FZGetRoleCollectionDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleCollectionData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleCollectionDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleCollectionDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RoleCollectionOpRegister(FMRpcManager* InManager, const FZRoleCollectionOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RoleCollectionOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RoleCollectionOpReq>();
        auto RspMessage = MakeShared<idlepb::RoleCollectionOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfRoleCollectionRegister(FMRpcManager* InManager, const FZShareSelfRoleCollectionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfRoleCollection;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ShareSelfRoleCollectionReq>();
        auto RspMessage = MakeShared<idlepb::ShareSelfRoleCollectionRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShareRoleCollectionDataRegister(FMRpcManager* InManager, const FZGetShareRoleCollectionDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareRoleCollectionData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetShareRoleCollectionDataReq>();
        auto RspMessage = MakeShared<idlepb::GetShareRoleCollectionDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetChecklistDataRegister(FMRpcManager* InManager, const FZGetChecklistDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetChecklistData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetChecklistDataReq>();
        auto RspMessage = MakeShared<idlepb::GetChecklistDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ChecklistOpRegister(FMRpcManager* InManager, const FZChecklistOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ChecklistOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ChecklistOpReq>();
        auto RspMessage = MakeShared<idlepb::ChecklistOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpdateChecklistRegister(FMRpcManager* InManager, const FZUpdateChecklistCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateChecklist;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UpdateChecklistReq>();
        auto RspMessage = MakeShared<idlepb::UpdateChecklistAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSwordPkInfoRegister(FMRpcManager* InManager, const FZGetSwordPkInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSwordPkInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetSwordPkInfoRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkSignupRegister(FMRpcManager* InManager, const FZSwordPkSignupCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkSignup;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SwordPkSignupReq>();
        auto RspMessage = MakeShared<idlepb::SwordPkSignupRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkMatchingRegister(FMRpcManager* InManager, const FZSwordPkMatchingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkMatching;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SwordPkMatchingReq>();
        auto RspMessage = MakeShared<idlepb::SwordPkMatchingRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkChallengeRegister(FMRpcManager* InManager, const FZSwordPkChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkChallenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SwordPkChallengeReq>();
        auto RspMessage = MakeShared<idlepb::SwordPkChallengeRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkRevengeRegister(FMRpcManager* InManager, const FZSwordPkRevengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkRevenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SwordPkRevengeReq>();
        auto RspMessage = MakeShared<idlepb::SwordPkRevengeRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSwordPkTopListRegister(FMRpcManager* InManager, const FZGetSwordPkTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkTopList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSwordPkTopListReq>();
        auto RspMessage = MakeShared<idlepb::GetSwordPkTopListRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkExchangeHeroCardRegister(FMRpcManager* InManager, const FZSwordPkExchangeHeroCardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkExchangeHeroCard;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SwordPkExchangeHeroCardReq>();
        auto RspMessage = MakeShared<idlepb::SwordPkExchangeHeroCardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCommonItemExchangeDataRegister(FMRpcManager* InManager, const FZGetCommonItemExchangeDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCommonItemExchangeData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetCommonItemExchangeDataReq>();
        auto RspMessage = MakeShared<idlepb::GetCommonItemExchangeDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExchangeCommonItemRegister(FMRpcManager* InManager, const FZExchangeCommonItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExchangeCommonItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ExchangeCommonItemReq>();
        auto RspMessage = MakeShared<idlepb::ExchangeCommonItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SynthesisCommonItemRegister(FMRpcManager* InManager, const FZSynthesisCommonItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SynthesisCommonItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SynthesisCommonItemReq>();
        auto RspMessage = MakeShared<idlepb::SynthesisCommonItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCandidatesSeptListRegister(FMRpcManager* InManager, const FZGetCandidatesSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesSeptList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetCandidatesSeptListReq>();
        auto RspMessage = MakeShared<idlepb::GetCandidatesSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SearchSeptRegister(FMRpcManager* InManager, const FZSearchSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SearchSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SearchSeptReq>();
        auto RspMessage = MakeShared<idlepb::SearchSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptBaseInfoRegister(FMRpcManager* InManager, const FZGetSeptBaseInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptBaseInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptBaseInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptBaseInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptMemberListRegister(FMRpcManager* InManager, const FZGetSeptMemberListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptMemberList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptMemberListReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptMemberListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::CreateSeptRegister(FMRpcManager* InManager, const FZCreateSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::CreateSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::CreateSeptReq>();
        auto RspMessage = MakeShared<idlepb::CreateSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DismissSeptRegister(FMRpcManager* InManager, const FZDismissSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DismissSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DismissSeptReq>();
        auto RspMessage = MakeShared<idlepb::DismissSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExitSeptRegister(FMRpcManager* InManager, const FZExitSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExitSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ExitSeptReq>();
        auto RspMessage = MakeShared<idlepb::ExitSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ApplyJoinSeptRegister(FMRpcManager* InManager, const FZApplyJoinSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ApplyJoinSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ApplyJoinSeptReq>();
        auto RspMessage = MakeShared<idlepb::ApplyJoinSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ApproveApplySeptRegister(FMRpcManager* InManager, const FZApproveApplySeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ApproveApplySept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ApproveApplySeptReq>();
        auto RspMessage = MakeShared<idlepb::ApproveApplySeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetApplyJoinSeptListRegister(FMRpcManager* InManager, const FZGetApplyJoinSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetApplyJoinSeptList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetApplyJoinSeptListReq>();
        auto RspMessage = MakeShared<idlepb::GetApplyJoinSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RespondInviteSeptRegister(FMRpcManager* InManager, const FZRespondInviteSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RespondInviteSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RespondInviteSeptReq>();
        auto RspMessage = MakeShared<idlepb::RespondInviteSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetInviteMeJoinSeptListRegister(FMRpcManager* InManager, const FZGetInviteMeJoinSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetInviteMeJoinSeptList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetInviteMeJoinSeptListReq>();
        auto RspMessage = MakeShared<idlepb::GetInviteMeJoinSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCandidatesInviteRoleListRegister(FMRpcManager* InManager, const FZGetCandidatesInviteRoleListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesInviteRoleList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetCandidatesInviteRoleListReq>();
        auto RspMessage = MakeShared<idlepb::GetCandidatesInviteRoleListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::InviteJoinSeptRegister(FMRpcManager* InManager, const FZInviteJoinSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::InviteJoinSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::InviteJoinSeptReq>();
        auto RspMessage = MakeShared<idlepb::InviteJoinSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetSeptSettingsRegister(FMRpcManager* InManager, const FZSetSeptSettingsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptSettings;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetSeptSettingsReq>();
        auto RspMessage = MakeShared<idlepb::SetSeptSettingsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetSeptAnnounceRegister(FMRpcManager* InManager, const FZSetSeptAnnounceCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptAnnounce;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::SetSeptAnnounceReq>();
        auto RspMessage = MakeShared<idlepb::SetSeptAnnounceAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ChangeSeptNameRegister(FMRpcManager* InManager, const FZChangeSeptNameCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ChangeSeptName;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ChangeSeptNameReq>();
        auto RspMessage = MakeShared<idlepb::ChangeSeptNameAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptLogRegister(FMRpcManager* InManager, const FZGetSeptLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLog;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptLogReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ConstructSeptRegister(FMRpcManager* InManager, const FZConstructSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ConstructSept;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ConstructSeptReq>();
        auto RspMessage = MakeShared<idlepb::ConstructSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetConstructSeptLogRegister(FMRpcManager* InManager, const FZGetConstructSeptLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetConstructSeptLog;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetConstructSeptLogReq>();
        auto RspMessage = MakeShared<idlepb::GetConstructSeptLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptInvitedRoleDailyNumRegister(FMRpcManager* InManager, const FZGetSeptInvitedRoleDailyNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptInvitedRoleDailyNum;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptInvitedRoleDailyNumReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptInvitedRoleDailyNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppointSeptPositionRegister(FMRpcManager* InManager, const FZAppointSeptPositionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppointSeptPosition;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppointSeptPositionReq>();
        auto RspMessage = MakeShared<idlepb::AppointSeptPositionAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ResignSeptChairmanRegister(FMRpcManager* InManager, const FZResignSeptChairmanCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ResignSeptChairman;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ResignSeptChairmanReq>();
        auto RspMessage = MakeShared<idlepb::ResignSeptChairmanAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::KickOutSeptMemberRegister(FMRpcManager* InManager, const FZKickOutSeptMemberCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::KickOutSeptMember;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::KickOutSeptMemberReq>();
        auto RspMessage = MakeShared<idlepb::KickOutSeptMemberAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleSeptShopDataRegister(FMRpcManager* InManager, const FZGetRoleSeptShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptShopData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleSeptShopDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleSeptShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuySeptShopItemRegister(FMRpcManager* InManager, const FZBuySeptShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuySeptShopItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::BuySeptShopItemReq>();
        auto RspMessage = MakeShared<idlepb::BuySeptShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleSeptQuestDataRegister(FMRpcManager* InManager, const FZGetRoleSeptQuestDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptQuestData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleSeptQuestDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleSeptQuestDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReqRoleSeptQuestOpRegister(FMRpcManager* InManager, const FZReqRoleSeptQuestOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReqRoleSeptQuestOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReqRoleSeptQuestOpReq>();
        auto RspMessage = MakeShared<idlepb::ReqRoleSeptQuestOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshSeptQuestRegister(FMRpcManager* InManager, const FZRefreshSeptQuestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshSeptQuest;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RefreshSeptQuestReq>();
        auto RspMessage = MakeShared<idlepb::RefreshSeptQuestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReqSeptQuestRankUpRegister(FMRpcManager* InManager, const FZReqSeptQuestRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReqSeptQuestRankUp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReqSeptQuestRankUpReq>();
        auto RspMessage = MakeShared<idlepb::ReqSeptQuestRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BeginOccupySeptStoneRegister(FMRpcManager* InManager, const FZBeginOccupySeptStoneCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BeginOccupySeptStone;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::BeginOccupySeptStoneReq>();
        auto RspMessage = MakeShared<idlepb::BeginOccupySeptStoneAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EndOccupySeptStoneRegister(FMRpcManager* InManager, const FZEndOccupySeptStoneCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EndOccupySeptStone;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::EndOccupySeptStoneReq>();
        auto RspMessage = MakeShared<idlepb::EndOccupySeptStoneAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OccupySeptLandRegister(FMRpcManager* InManager, const FZOccupySeptLandCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OccupySeptLand;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OccupySeptLandReq>();
        auto RspMessage = MakeShared<idlepb::OccupySeptLandAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetGongFaDataRegister(FMRpcManager* InManager, const FZGetGongFaDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetGongFaData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetGongFaDataReq>();
        auto RspMessage = MakeShared<idlepb::GetGongFaDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GongFaOpRegister(FMRpcManager* InManager, const FZGongFaOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GongFaOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GongFaOpReq>();
        auto RspMessage = MakeShared<idlepb::GongFaOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ActivateGongFaMaxEffectRegister(FMRpcManager* InManager, const FZActivateGongFaMaxEffectCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ActivateGongFaMaxEffect;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ActivateGongFaMaxEffectReq>();
        auto RspMessage = MakeShared<idlepb::ActivateGongFaMaxEffectAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptLandDamageTopListRegister(FMRpcManager* InManager, const FZGetSeptLandDamageTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLandDamageTopList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptLandDamageTopListReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptLandDamageTopListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveFuZengRewardsRegister(FMRpcManager* InManager, const FZReceiveFuZengRewardsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveFuZengRewards;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReceiveFuZengRewardsReq>();
        auto RspMessage = MakeShared<idlepb::ReceiveFuZengRewardsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleFuZengDataRegister(FMRpcManager* InManager, const FZGetRoleFuZengDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFuZengData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleFuZengDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleFuZengDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleTreasuryDataRegister(FMRpcManager* InManager, const FZGetRoleTreasuryDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleTreasuryData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleTreasuryDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleTreasuryDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OpenTreasuryChestRegister(FMRpcManager* InManager, const FZOpenTreasuryChestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryChest;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OpenTreasuryChestReq>();
        auto RspMessage = MakeShared<idlepb::OpenTreasuryChestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickOpenTreasuryChestRegister(FMRpcManager* InManager, const FZOneClickOpenTreasuryChestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickOpenTreasuryChest;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OneClickOpenTreasuryChestReq>();
        auto RspMessage = MakeShared<idlepb::OneClickOpenTreasuryChestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OpenTreasuryGachaRegister(FMRpcManager* InManager, const FZOpenTreasuryGachaCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryGacha;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::OpenTreasuryGachaReq>();
        auto RspMessage = MakeShared<idlepb::OpenTreasuryGachaAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshTreasuryShopRegister(FMRpcManager* InManager, const FZRefreshTreasuryShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshTreasuryShop;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RefreshTreasuryShopReq>();
        auto RspMessage = MakeShared<idlepb::RefreshTreasuryShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::TreasuryShopBuyRegister(FMRpcManager* InManager, const FZTreasuryShopBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::TreasuryShopBuy;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::TreasuryShopBuyReq>();
        auto RspMessage = MakeShared<idlepb::TreasuryShopBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLifeCounterDataRegister(FMRpcManager* InManager, const FZGetLifeCounterDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLifeCounterData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetLifeCounterDataReq>();
        auto RspMessage = MakeShared<idlepb::GetLifeCounterDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DoQuestFightRegister(FMRpcManager* InManager, const FZDoQuestFightCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DoQuestFight;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DoQuestFightReq>();
        auto RspMessage = MakeShared<idlepb::DoQuestFightAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QuestFightQuickEndRegister(FMRpcManager* InManager, const FZQuestFightQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QuestFightQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::QuestFightQuickEndReq>();
        auto RspMessage = MakeShared<idlepb::QuestFightQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetAppearanceDataRegister(FMRpcManager* InManager, const FZGetAppearanceDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetAppearanceData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetAppearanceDataReq>();
        auto RspMessage = MakeShared<idlepb::GetAppearanceDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceAddRegister(FMRpcManager* InManager, const FZAppearanceAddCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceAdd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppearanceAddReq>();
        auto RspMessage = MakeShared<idlepb::AppearanceAddAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceActiveRegister(FMRpcManager* InManager, const FZAppearanceActiveCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceActive;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppearanceActiveReq>();
        auto RspMessage = MakeShared<idlepb::AppearanceActiveAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceWearRegister(FMRpcManager* InManager, const FZAppearanceWearCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceWear;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppearanceWearReq>();
        auto RspMessage = MakeShared<idlepb::AppearanceWearAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceBuyRegister(FMRpcManager* InManager, const FZAppearanceBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceBuy;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppearanceBuyReq>();
        auto RspMessage = MakeShared<idlepb::AppearanceBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceChangeSkTypeRegister(FMRpcManager* InManager, const FZAppearanceChangeSkTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceChangeSkType;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AppearanceChangeSkTypeReq>();
        auto RspMessage = MakeShared<idlepb::AppearanceChangeSkTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBattleHistoryInfoRegister(FMRpcManager* InManager, const FZGetBattleHistoryInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBattleHistoryInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetBattleHistoryInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetBattleHistoryInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetArenaCheckListDataRegister(FMRpcManager* InManager, const FZGetArenaCheckListDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaCheckListData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetArenaCheckListDataReq>();
        auto RspMessage = MakeShared<idlepb::GetArenaCheckListDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ArenaCheckListSubmitRegister(FMRpcManager* InManager, const FZArenaCheckListSubmitCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListSubmit;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ArenaCheckListSubmitReq>();
        auto RspMessage = MakeShared<idlepb::ArenaCheckListSubmitAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ArenaCheckListRewardSubmitRegister(FMRpcManager* InManager, const FZArenaCheckListRewardSubmitCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListRewardSubmit;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ArenaCheckListRewardSubmitReq>();
        auto RspMessage = MakeShared<idlepb::ArenaCheckListRewardSubmitAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllChallengeRegister(FMRpcManager* InManager, const FZDungeonKillAllChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllChallenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonKillAllChallengeReq>();
        auto RspMessage = MakeShared<idlepb::DungeonKillAllChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllQuickEndRegister(FMRpcManager* InManager, const FZDungeonKillAllQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonKillAllQuickEndReq>();
        auto RspMessage = MakeShared<idlepb::DungeonKillAllQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllDataRegister(FMRpcManager* InManager, const FZDungeonKillAllDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonKillAllDataReq>();
        auto RspMessage = MakeShared<idlepb::DungeonKillAllDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetFarmlandDataRegister(FMRpcManager* InManager, const FZGetFarmlandDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetFarmlandData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetFarmlandDataReq>();
        auto RspMessage = MakeShared<idlepb::GetFarmlandDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandUnlockBlockRegister(FMRpcManager* InManager, const FZFarmlandUnlockBlockCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandUnlockBlock;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandUnlockBlockReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandUnlockBlockAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandPlantSeedRegister(FMRpcManager* InManager, const FZFarmlandPlantSeedCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandPlantSeed;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandPlantSeedReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandPlantSeedAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandWateringRegister(FMRpcManager* InManager, const FZFarmlandWateringCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandWatering;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandWateringReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandWateringAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandRipeningRegister(FMRpcManager* InManager, const FZFarmlandRipeningCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandRipening;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandRipeningReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandRipeningAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandHarvestRegister(FMRpcManager* InManager, const FZFarmlandHarvestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandHarvest;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandHarvestReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandHarvestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmerRankUpRegister(FMRpcManager* InManager, const FZFarmerRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmerRankUp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmerRankUpReq>();
        auto RspMessage = MakeShared<idlepb::FarmerRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandSetManagementRegister(FMRpcManager* InManager, const FZFarmlandSetManagementCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandSetManagement;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FarmlandSetManagementReq>();
        auto RspMessage = MakeShared<idlepb::FarmlandSetManagementAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpdateFarmlandStateRegister(FMRpcManager* InManager, const FZUpdateFarmlandStateCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateFarmlandState;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::UpdateFarmlandStateReq>();
        auto RspMessage = MakeShared<idlepb::UpdateFarmlandStateAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveChallengeRegister(FMRpcManager* InManager, const FZDungeonSurviveChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveChallenge;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonSurviveChallengeReq>();
        auto RspMessage = MakeShared<idlepb::DungeonSurviveChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveQuickEndRegister(FMRpcManager* InManager, const FZDungeonSurviveQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonSurviveQuickEndReq>();
        auto RspMessage = MakeShared<idlepb::DungeonSurviveQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveDataRegister(FMRpcManager* InManager, const FZDungeonSurviveDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DungeonSurviveDataReq>();
        auto RspMessage = MakeShared<idlepb::DungeonSurviveDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRevertAllSkillCoolDownRegister(FMRpcManager* InManager, const FZGetRevertAllSkillCoolDownCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRevertAllSkillCoolDown;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRevertAllSkillCoolDownReq>();
        auto RspMessage = MakeShared<idlepb::GetRevertAllSkillCoolDownAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleFriendDataRegister(FMRpcManager* InManager, const FZGetRoleFriendDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFriendData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleFriendDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleFriendDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FriendOpRegister(FMRpcManager* InManager, const FZFriendOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FriendOp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FriendOpReq>();
        auto RspMessage = MakeShared<idlepb::FriendOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReplyFriendRequestRegister(FMRpcManager* InManager, const FZReplyFriendRequestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReplyFriendRequest;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReplyFriendRequestReq>();
        auto RspMessage = MakeShared<idlepb::ReplyFriendRequestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FriendSearchRoleInfoRegister(FMRpcManager* InManager, const FZFriendSearchRoleInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FriendSearchRoleInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::FriendSearchRoleInfoReq>();
        auto RspMessage = MakeShared<idlepb::FriendSearchRoleInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleInfoCacheRegister(FMRpcManager* InManager, const FZGetRoleInfoCacheCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfoCache;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleInfoCacheReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleInfoCacheAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleInfoRegister(FMRpcManager* InManager, const FZGetRoleInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleAvatarDataRegister(FMRpcManager* InManager, const FZGetRoleAvatarDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAvatarData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleAvatarDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleAvatarDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DispatchAvatarRegister(FMRpcManager* InManager, const FZDispatchAvatarCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DispatchAvatar;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::DispatchAvatarReq>();
        auto RspMessage = MakeShared<idlepb::DispatchAvatarAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AvatarRankUpRegister(FMRpcManager* InManager, const FZAvatarRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AvatarRankUp;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AvatarRankUpReq>();
        auto RspMessage = MakeShared<idlepb::AvatarRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveAvatarTempPackageRegister(FMRpcManager* InManager, const FZReceiveAvatarTempPackageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveAvatarTempPackage;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReceiveAvatarTempPackageReq>();
        auto RspMessage = MakeShared<idlepb::ReceiveAvatarTempPackageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetArenaExplorationStatisticalDataRegister(FMRpcManager* InManager, const FZGetArenaExplorationStatisticalDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaExplorationStatisticalData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetArenaExplorationStatisticalDataReq>();
        auto RspMessage = MakeShared<idlepb::GetArenaExplorationStatisticalDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleBiographyDataRegister(FMRpcManager* InManager, const FZGetRoleBiographyDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleBiographyData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleBiographyDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleBiographyDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBiographyItemRegister(FMRpcManager* InManager, const FZReceiveBiographyItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReceiveBiographyItemReq>();
        auto RspMessage = MakeShared<idlepb::ReceiveBiographyItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBiographyEventDataRegister(FMRpcManager* InManager, const FZGetBiographyEventDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBiographyEventData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetBiographyEventDataReq>();
        auto RspMessage = MakeShared<idlepb::GetBiographyEventDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBiographyEventItemRegister(FMRpcManager* InManager, const FZReceiveBiographyEventItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyEventItem;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::ReceiveBiographyEventItemReq>();
        auto RspMessage = MakeShared<idlepb::ReceiveBiographyEventItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AddBiographyRoleLogRegister(FMRpcManager* InManager, const FZAddBiographyRoleLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AddBiographyRoleLog;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::AddBiographyRoleLogReq>();
        auto RspMessage = MakeShared<idlepb::AddBiographyRoleLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestEnterSeptDemonWorldRegister(FMRpcManager* InManager, const FZRequestEnterSeptDemonWorldCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestEnterSeptDemonWorld;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestEnterSeptDemonWorldReq>();
        auto RspMessage = MakeShared<idlepb::RequestEnterSeptDemonWorldAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestLeaveSeptDemonWorldRegister(FMRpcManager* InManager, const FZRequestLeaveSeptDemonWorldCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestLeaveSeptDemonWorld;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestLeaveSeptDemonWorldReq>();
        auto RspMessage = MakeShared<idlepb::RequestLeaveSeptDemonWorldAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestSeptDemonWorldDataRegister(FMRpcManager* InManager, const FZRequestSeptDemonWorldDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestSeptDemonWorldData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestSeptDemonWorldDataReq>();
        auto RspMessage = MakeShared<idlepb::RequestSeptDemonWorldDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestInSeptDemonWorldEndTimeRegister(FMRpcManager* InManager, const FZRequestInSeptDemonWorldEndTimeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestInSeptDemonWorldEndTime;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::RequestInSeptDemonWorldEndTimeReq>();
        auto RspMessage = MakeShared<idlepb::RequestInSeptDemonWorldEndTimeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageTopListRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageTopList;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageTopListReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageTopListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageSelfSummaryRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageSelfSummaryCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageSelfSummary;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageSelfSummaryReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageSelfSummaryAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonStageRewardNumRegister(FMRpcManager* InManager, const FZGetSeptDemonStageRewardNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageRewardNum;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonStageRewardNumReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonStageRewardNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonStageRewardRegister(FMRpcManager* InManager, const FZGetSeptDemonStageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonStageRewardReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonStageRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageRewardsInfoRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageRewardsInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageRewardsInfo;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageRewardsInfoReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageRewardsInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageRewardRegister(FMRpcManager* InManager, const FZGetSeptDemonDamageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageReward;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetSeptDemonDamageRewardReq>();
        auto RspMessage = MakeShared<idlepb::GetSeptDemonDamageRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleVipShopDataRegister(FMRpcManager* InManager, const FZGetRoleVipShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleVipShopData;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::GetRoleVipShopDataReq>();
        auto RspMessage = MakeShared<idlepb::GetRoleVipShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::VipShopBuyRegister(FMRpcManager* InManager, const FZVipShopBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::VipShopBuy;
    InManager->AddMethod(RpcId, [InCallback](FPbMessageSupportBase* InConn, const TSharedPtr<idlepb::PbRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlepb::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlepb::VipShopBuyReq>();
        auto RspMessage = MakeShared<idlepb::VipShopBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlepb::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlepb::RpcErrorCode_ReqDataError;
        }
        FMRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}
