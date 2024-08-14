#include "GameRpcInterface.h"

FZGameRpcInterface::FZGameRpcInterface(FZRpcManager* InManager)
{
}

FZGameRpcInterface::~FZGameRpcInterface()
{
}

void FZGameRpcInterface::LoginGameRegister(FZRpcManager* InManager, const FZLoginGameCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LoginGame;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::LoginGameReq>();
        auto RspMessage = MakeShared<idlezt::LoginGameAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetCurrentCultivationDirectionRegister(FZRpcManager* InManager, const FZSetCurrentCultivationDirectionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetCurrentCultivationDirection;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetCurrentCultivationDirectionReq>();
        auto RspMessage = MakeShared<idlezt::SetCurrentCultivationDirectionAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DoBreakthroughRegister(FZRpcManager* InManager, const FZDoBreakthroughCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DoBreakthrough;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DoBreakthroughReq>();
        auto RspMessage = MakeShared<idlezt::DoBreakthroughAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestCommonCultivationDataRegister(FZRpcManager* InManager, const FZRequestCommonCultivationDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestCommonCultivationData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestCommonCultivationDataReq>();
        auto RspMessage = MakeShared<idlezt::RequestCommonCultivationDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickMergeBreathingRegister(FZRpcManager* InManager, const FZOneClickMergeBreathingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickMergeBreathing;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickMergeBreathingReq>();
        auto RspMessage = MakeShared<idlezt::OneClickMergeBreathingAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBreathingExerciseRewardRegister(FZRpcManager* InManager, const FZReceiveBreathingExerciseRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBreathingExerciseReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReceiveBreathingExerciseRewardReq>();
        auto RspMessage = MakeShared<idlezt::ReceiveBreathingExerciseRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetInventoryDataRegister(FZRpcManager* InManager, const FZGetInventoryDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetInventoryData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetInventoryDataReq>();
        auto RspMessage = MakeShared<idlezt::GetInventoryDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetQuestDataRegister(FZRpcManager* InManager, const FZGetQuestDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetQuestData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetQuestDataReq>();
        auto RspMessage = MakeShared<idlezt::GetQuestDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::CreateCharacterRegister(FZRpcManager* InManager, const FZCreateCharacterCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::CreateCharacter;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::CreateCharacterReq>();
        auto RspMessage = MakeShared<idlezt::CreateCharacterAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseItemRegister(FZRpcManager* InManager, const FZUseItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UseItemReq>();
        auto RspMessage = MakeShared<idlezt::UseItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseSelectGiftRegister(FZRpcManager* InManager, const FZUseSelectGiftCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseSelectGift;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UseSelectGiftReq>();
        auto RspMessage = MakeShared<idlezt::UseSelectGiftAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SellItemRegister(FZRpcManager* InManager, const FZSellItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SellItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SellItemReq>();
        auto RspMessage = MakeShared<idlezt::SellItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockEquipmentSlotRegister(FZRpcManager* InManager, const FZUnlockEquipmentSlotCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockEquipmentSlot;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UnlockEquipmentSlotReq>();
        auto RspMessage = MakeShared<idlezt::UnlockEquipmentSlotAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineStartRegister(FZRpcManager* InManager, const FZAlchemyRefineStartCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineStart;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AlchemyRefineStartReq>();
        auto RspMessage = MakeShared<idlezt::AlchemyRefineStartAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineCancelRegister(FZRpcManager* InManager, const FZAlchemyRefineCancelCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineCancel;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AlchemyRefineCancelReq>();
        auto RspMessage = MakeShared<idlezt::AlchemyRefineCancelAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AlchemyRefineExtractRegister(FZRpcManager* InManager, const FZAlchemyRefineExtractCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AlchemyRefineExtract;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AlchemyRefineExtractReq>();
        auto RspMessage = MakeShared<idlezt::AlchemyRefineExtractAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleShopDataRegister(FZRpcManager* InManager, const FZGetRoleShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleShopData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleShopDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshShopRegister(FZRpcManager* InManager, const FZRefreshShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshShop;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RefreshShopReq>();
        auto RspMessage = MakeShared<idlezt::RefreshShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuyShopItemRegister(FZRpcManager* InManager, const FZBuyShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuyShopItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::BuyShopItemReq>();
        auto RspMessage = MakeShared<idlezt::BuyShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleDeluxeShopDataRegister(FZRpcManager* InManager, const FZGetRoleDeluxeShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleDeluxeShopData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleDeluxeShopDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleDeluxeShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshDeluxeShopRegister(FZRpcManager* InManager, const FZRefreshDeluxeShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshDeluxeShop;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RefreshDeluxeShopReq>();
        auto RspMessage = MakeShared<idlezt::RefreshDeluxeShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuyDeluxeShopItemRegister(FZRpcManager* InManager, const FZBuyDeluxeShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuyDeluxeShopItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::BuyDeluxeShopItemReq>();
        auto RspMessage = MakeShared<idlezt::BuyDeluxeShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetTemporaryPackageDataRegister(FZRpcManager* InManager, const FZGetTemporaryPackageDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetTemporaryPackageData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetTemporaryPackageDataReq>();
        auto RspMessage = MakeShared<idlezt::GetTemporaryPackageDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExtractTemporaryPackageItemsRegister(FZRpcManager* InManager, const FZExtractTemporaryPackageItemsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExtractTemporaryPackageItems;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ExtractTemporaryPackageItemsReq>();
        auto RspMessage = MakeShared<idlezt::ExtractTemporaryPackageItemsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SpeedupReliveRegister(FZRpcManager* InManager, const FZSpeedupReliveCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SpeedupRelive;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SpeedupReliveReq>();
        auto RspMessage = MakeShared<idlezt::SpeedupReliveAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMapInfoRegister(FZRpcManager* InManager, const FZGetMapInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMapInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetMapInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetMapInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockArenaRegister(FZRpcManager* InManager, const FZUnlockArenaCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockArena;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UnlockArenaReq>();
        auto RspMessage = MakeShared<idlezt::UnlockArenaAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QuestOpRegister(FZRpcManager* InManager, const FZQuestOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QuestOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::QuestOpReq>();
        auto RspMessage = MakeShared<idlezt::QuestOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EquipmentPutOnRegister(FZRpcManager* InManager, const FZEquipmentPutOnCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentPutOn;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::EquipmentPutOnReq>();
        auto RspMessage = MakeShared<idlezt::EquipmentPutOnAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EquipmentTakeOffRegister(FZRpcManager* InManager, const FZEquipmentTakeOffCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EquipmentTakeOff;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::EquipmentTakeOffReq>();
        auto RspMessage = MakeShared<idlezt::EquipmentTakeOffAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLeaderboardPreviewRegister(FZRpcManager* InManager, const FZGetLeaderboardPreviewCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardPreview;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetLeaderboardPreviewReq>();
        auto RspMessage = MakeShared<idlezt::GetLeaderboardPreviewAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLeaderboardDataRegister(FZRpcManager* InManager, const FZGetLeaderboardDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLeaderboardData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetLeaderboardDataReq>();
        auto RspMessage = MakeShared<idlezt::GetLeaderboardDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleLeaderboardDataRegister(FZRpcManager* InManager, const FZGetRoleLeaderboardDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleLeaderboardData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleLeaderboardDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleLeaderboardDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LeaderboardClickLikeRegister(FZRpcManager* InManager, const FZLeaderboardClickLikeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardClickLike;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::LeaderboardClickLikeReq>();
        auto RspMessage = MakeShared<idlezt::LeaderboardClickLikeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LeaderboardUpdateMessageRegister(FZRpcManager* InManager, const FZLeaderboardUpdateMessageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LeaderboardUpdateMessage;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::LeaderboardUpdateMessageReq>();
        auto RspMessage = MakeShared<idlezt::LeaderboardUpdateMessageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetFuZeRewardRegister(FZRpcManager* InManager, const FZGetFuZeRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetFuZeReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetFuZeRewardReq>();
        auto RspMessage = MakeShared<idlezt::GetFuZeRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleMailDataRegister(FZRpcManager* InManager, const FZGetRoleMailDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleMailData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleMailDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleMailDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReadMailRegister(FZRpcManager* InManager, const FZReadMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReadMail;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReadMailReq>();
        auto RspMessage = MakeShared<idlezt::ReadMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMailAttachmentRegister(FZRpcManager* InManager, const FZGetMailAttachmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMailAttachment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetMailAttachmentReq>();
        auto RspMessage = MakeShared<idlezt::GetMailAttachmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DeleteMailRegister(FZRpcManager* InManager, const FZDeleteMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DeleteMail;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DeleteMailReq>();
        auto RspMessage = MakeShared<idlezt::DeleteMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickGetMailAttachmentRegister(FZRpcManager* InManager, const FZOneClickGetMailAttachmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickGetMailAttachment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickGetMailAttachmentReq>();
        auto RspMessage = MakeShared<idlezt::OneClickGetMailAttachmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickReadMailRegister(FZRpcManager* InManager, const FZOneClickReadMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickReadMail;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickReadMailReq>();
        auto RspMessage = MakeShared<idlezt::OneClickReadMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickDeleteMailRegister(FZRpcManager* InManager, const FZOneClickDeleteMailCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickDeleteMail;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickDeleteMailReq>();
        auto RspMessage = MakeShared<idlezt::OneClickDeleteMailAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UnlockFunctionModuleRegister(FZRpcManager* InManager, const FZUnlockFunctionModuleCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UnlockFunctionModule;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UnlockFunctionModuleReq>();
        auto RspMessage = MakeShared<idlezt::UnlockFunctionModuleAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetChatRecordRegister(FZRpcManager* InManager, const FZGetChatRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetChatRecord;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetChatRecordReq>();
        auto RspMessage = MakeShared<idlezt::GetChatRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DeletePrivateChatRecordRegister(FZRpcManager* InManager, const FZDeletePrivateChatRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DeletePrivateChatRecord;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DeletePrivateChatRecordReq>();
        auto RspMessage = MakeShared<idlezt::DeletePrivateChatRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SendChatMessageRegister(FZRpcManager* InManager, const FZSendChatMessageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SendChatMessage;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SendChatMessageReq>();
        auto RspMessage = MakeShared<idlezt::SendChatMessageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ClearChatUnreadNumRegister(FZRpcManager* InManager, const FZClearChatUnreadNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ClearChatUnreadNum;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ClearChatUnreadNumReq>();
        auto RspMessage = MakeShared<idlezt::ClearChatUnreadNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineStartRegister(FZRpcManager* InManager, const FZForgeRefineStartCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineStart;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ForgeRefineStartReq>();
        auto RspMessage = MakeShared<idlezt::ForgeRefineStartAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineCancelRegister(FZRpcManager* InManager, const FZForgeRefineCancelCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineCancel;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ForgeRefineCancelReq>();
        auto RspMessage = MakeShared<idlezt::ForgeRefineCancelAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeRefineExtractRegister(FZRpcManager* InManager, const FZForgeRefineExtractCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeRefineExtract;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ForgeRefineExtractReq>();
        auto RspMessage = MakeShared<idlezt::ForgeRefineExtractAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetForgeLostEquipmentDataRegister(FZRpcManager* InManager, const FZGetForgeLostEquipmentDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetForgeLostEquipmentData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetForgeLostEquipmentDataReq>();
        auto RspMessage = MakeShared<idlezt::GetForgeLostEquipmentDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeDestroyRegister(FZRpcManager* InManager, const FZForgeDestroyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeDestroy;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ForgeDestroyReq>();
        auto RspMessage = MakeShared<idlezt::ForgeDestroyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ForgeFindBackRegister(FZRpcManager* InManager, const FZForgeFindBackCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ForgeFindBack;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ForgeFindBackReq>();
        auto RspMessage = MakeShared<idlezt::ForgeFindBackAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestPillElixirDataRegister(FZRpcManager* InManager, const FZRequestPillElixirDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestPillElixirData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestPillElixirDataReq>();
        auto RspMessage = MakeShared<idlezt::RequestPillElixirDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetOnePillElixirDataRegister(FZRpcManager* InManager, const FZGetOnePillElixirDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetOnePillElixirData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetOnePillElixirDataReq>();
        auto RspMessage = MakeShared<idlezt::GetOnePillElixirDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestModifyPillElixirFilterRegister(FZRpcManager* InManager, const FZRequestModifyPillElixirFilterCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestModifyPillElixirFilter;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestModifyPillElixirFilterReq>();
        auto RspMessage = MakeShared<idlezt::RequestModifyPillElixirFilterAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UsePillElixirRegister(FZRpcManager* InManager, const FZUsePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UsePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UsePillElixirReq>();
        auto RspMessage = MakeShared<idlezt::UsePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickUsePillElixirRegister(FZRpcManager* InManager, const FZOneClickUsePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickUsePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickUsePillElixirReq>();
        auto RspMessage = MakeShared<idlezt::OneClickUsePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::TradePillElixirRegister(FZRpcManager* InManager, const FZTradePillElixirCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::TradePillElixir;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::TradePillElixirReq>();
        auto RspMessage = MakeShared<idlezt::TradePillElixirAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReinforceEquipmentRegister(FZRpcManager* InManager, const FZReinforceEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReinforceEquipment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReinforceEquipmentReq>();
        auto RspMessage = MakeShared<idlezt::ReinforceEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefineEquipmentRegister(FZRpcManager* InManager, const FZRefineEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefineEquipment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RefineEquipmentReq>();
        auto RspMessage = MakeShared<idlezt::RefineEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QiWenEquipmentRegister(FZRpcManager* InManager, const FZQiWenEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QiWenEquipment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::QiWenEquipmentReq>();
        auto RspMessage = MakeShared<idlezt::QiWenEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ResetEquipmentRegister(FZRpcManager* InManager, const FZResetEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ResetEquipment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ResetEquipmentReq>();
        auto RspMessage = MakeShared<idlezt::ResetEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::InheritEquipmentRegister(FZRpcManager* InManager, const FZInheritEquipmentCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::InheritEquipment;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::InheritEquipmentReq>();
        auto RspMessage = MakeShared<idlezt::InheritEquipmentAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::LockItemRegister(FZRpcManager* InManager, const FZLockItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::LockItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::LockItemReq>();
        auto RspMessage = MakeShared<idlezt::LockItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SoloArenaChallengeRegister(FZRpcManager* InManager, const FZSoloArenaChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaChallenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SoloArenaChallengeReq>();
        auto RspMessage = MakeShared<idlezt::SoloArenaChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SoloArenaQuickEndRegister(FZRpcManager* InManager, const FZSoloArenaQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SoloArenaQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SoloArenaQuickEndReq>();
        auto RspMessage = MakeShared<idlezt::SoloArenaQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSoloArenaHistoryListRegister(FZRpcManager* InManager, const FZGetSoloArenaHistoryListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSoloArenaHistoryList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSoloArenaHistoryListReq>();
        auto RspMessage = MakeShared<idlezt::GetSoloArenaHistoryListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerChallengeRegister(FZRpcManager* InManager, const FZMonsterTowerChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerChallenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::MonsterTowerChallengeReq>();
        auto RspMessage = MakeShared<idlezt::MonsterTowerChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerDrawIdleAwardRegister(FZRpcManager* InManager, const FZMonsterTowerDrawIdleAwardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerDrawIdleAward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::MonsterTowerDrawIdleAwardReq>();
        auto RspMessage = MakeShared<idlezt::MonsterTowerDrawIdleAwardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerClosedDoorTrainingRegister(FZRpcManager* InManager, const FZMonsterTowerClosedDoorTrainingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerClosedDoorTraining;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::MonsterTowerClosedDoorTrainingReq>();
        auto RspMessage = MakeShared<idlezt::MonsterTowerClosedDoorTrainingAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::MonsterTowerQuickEndRegister(FZRpcManager* InManager, const FZMonsterTowerQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::MonsterTowerQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::MonsterTowerQuickEndReq>();
        auto RspMessage = MakeShared<idlezt::MonsterTowerQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMonsterTowerChallengeListRegister(FZRpcManager* InManager, const FZGetMonsterTowerChallengeListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetMonsterTowerChallengeListReq>();
        auto RspMessage = MakeShared<idlezt::GetMonsterTowerChallengeListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetMonsterTowerChallengeRewardRegister(FZRpcManager* InManager, const FZGetMonsterTowerChallengeRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetMonsterTowerChallengeReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetMonsterTowerChallengeRewardReq>();
        auto RspMessage = MakeShared<idlezt::GetMonsterTowerChallengeRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetWorldTimeDilationRegister(FZRpcManager* InManager, const FZSetWorldTimeDilationCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetWorldTimeDilation;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetWorldTimeDilationReq>();
        auto RspMessage = MakeShared<idlezt::SetWorldTimeDilationAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetFightModeRegister(FZRpcManager* InManager, const FZSetFightModeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetFightMode;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetFightModeReq>();
        auto RspMessage = MakeShared<idlezt::SetFightModeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpgradeQiCollectorRegister(FZRpcManager* InManager, const FZUpgradeQiCollectorCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpgradeQiCollector;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UpgradeQiCollectorReq>();
        auto RspMessage = MakeShared<idlezt::UpgradeQiCollectorAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleAllStatsRegister(FZRpcManager* InManager, const FZGetRoleAllStatsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAllStats;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleAllStatsReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleAllStatsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShanhetuDataRegister(FZRpcManager* InManager, const FZGetShanhetuDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetShanhetuDataReq>();
        auto RspMessage = MakeShared<idlezt::GetShanhetuDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetShanhetuUseConfigRegister(FZRpcManager* InManager, const FZSetShanhetuUseConfigCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetShanhetuUseConfig;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetShanhetuUseConfigReq>();
        auto RspMessage = MakeShared<idlezt::SetShanhetuUseConfigAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UseShanhetuRegister(FZRpcManager* InManager, const FZUseShanhetuCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UseShanhetu;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UseShanhetuReq>();
        auto RspMessage = MakeShared<idlezt::UseShanhetuAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::StepShanhetuRegister(FZRpcManager* InManager, const FZStepShanhetuCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::StepShanhetu;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::StepShanhetuReq>();
        auto RspMessage = MakeShared<idlezt::StepShanhetuAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShanhetuUseRecordRegister(FZRpcManager* InManager, const FZGetShanhetuUseRecordCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShanhetuUseRecord;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetShanhetuUseRecordReq>();
        auto RspMessage = MakeShared<idlezt::GetShanhetuUseRecordAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetAttackLockTypeRegister(FZRpcManager* InManager, const FZSetAttackLockTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackLockType;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetAttackLockTypeReq>();
        auto RspMessage = MakeShared<idlezt::SetAttackLockTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetAttackUnlockTypeRegister(FZRpcManager* InManager, const FZSetAttackUnlockTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetAttackUnlockType;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetAttackUnlockTypeReq>();
        auto RspMessage = MakeShared<idlezt::SetAttackUnlockTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetShowUnlockButtonRegister(FZRpcManager* InManager, const FZSetShowUnlockButtonCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetShowUnlockButton;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetShowUnlockButtonReq>();
        auto RspMessage = MakeShared<idlezt::SetShowUnlockButtonAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetUserVarRegister(FZRpcManager* InManager, const FZGetUserVarCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVar;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetUserVarReq>();
        auto RspMessage = MakeShared<idlezt::GetUserVarRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetUserVarsRegister(FZRpcManager* InManager, const FZGetUserVarsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetUserVars;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetUserVarsReq>();
        auto RspMessage = MakeShared<idlezt::GetUserVarsRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionArenaSummaryRegister(FZRpcManager* InManager, const FZGetBossInvasionArenaSummaryCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaSummary;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetBossInvasionArenaSummaryReq>();
        auto RspMessage = MakeShared<idlezt::GetBossInvasionArenaSummaryRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionArenaTopListRegister(FZRpcManager* InManager, const FZGetBossInvasionArenaTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionArenaTopList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetBossInvasionArenaTopListReq>();
        auto RspMessage = MakeShared<idlezt::GetBossInvasionArenaTopListRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBossInvasionInfoRegister(FZRpcManager* InManager, const FZGetBossInvasionInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBossInvasionInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetBossInvasionInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetBossInvasionInfoRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DrawBossInvasionKillRewardRegister(FZRpcManager* InManager, const FZDrawBossInvasionKillRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionKillReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DrawBossInvasionKillRewardReq>();
        auto RspMessage = MakeShared<idlezt::DrawBossInvasionKillRewardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DrawBossInvasionDamageRewardRegister(FZRpcManager* InManager, const FZDrawBossInvasionDamageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DrawBossInvasionDamageReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DrawBossInvasionDamageRewardReq>();
        auto RspMessage = MakeShared<idlezt::DrawBossInvasionDamageRewardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BossInvasionTeleportRegister(FZRpcManager* InManager, const FZBossInvasionTeleportCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BossInvasionTeleport;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::BossInvasionTeleportReq>();
        auto RspMessage = MakeShared<idlezt::BossInvasionTeleportRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfItemRegister(FZRpcManager* InManager, const FZShareSelfItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ShareSelfItemReq>();
        auto RspMessage = MakeShared<idlezt::ShareSelfItemRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfItemsRegister(FZRpcManager* InManager, const FZShareSelfItemsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfItems;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ShareSelfItemsReq>();
        auto RspMessage = MakeShared<idlezt::ShareSelfItemsRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShareItemDataRegister(FZRpcManager* InManager, const FZGetShareItemDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareItemData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetShareItemDataReq>();
        auto RspMessage = MakeShared<idlezt::GetShareItemDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleCollectionDataRegister(FZRpcManager* InManager, const FZGetRoleCollectionDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleCollectionData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleCollectionDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleCollectionDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RoleCollectionOpRegister(FZRpcManager* InManager, const FZRoleCollectionOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RoleCollectionOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RoleCollectionOpReq>();
        auto RspMessage = MakeShared<idlezt::RoleCollectionOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ShareSelfRoleCollectionRegister(FZRpcManager* InManager, const FZShareSelfRoleCollectionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ShareSelfRoleCollection;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ShareSelfRoleCollectionReq>();
        auto RspMessage = MakeShared<idlezt::ShareSelfRoleCollectionRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetShareRoleCollectionDataRegister(FZRpcManager* InManager, const FZGetShareRoleCollectionDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetShareRoleCollectionData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetShareRoleCollectionDataReq>();
        auto RspMessage = MakeShared<idlezt::GetShareRoleCollectionDataRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetChecklistDataRegister(FZRpcManager* InManager, const FZGetChecklistDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetChecklistData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetChecklistDataReq>();
        auto RspMessage = MakeShared<idlezt::GetChecklistDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ChecklistOpRegister(FZRpcManager* InManager, const FZChecklistOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ChecklistOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ChecklistOpReq>();
        auto RspMessage = MakeShared<idlezt::ChecklistOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpdateChecklistRegister(FZRpcManager* InManager, const FZUpdateChecklistCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateChecklist;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UpdateChecklistReq>();
        auto RspMessage = MakeShared<idlezt::UpdateChecklistAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSwordPkInfoRegister(FZRpcManager* InManager, const FZGetSwordPkInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSwordPkInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetSwordPkInfoRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkSignupRegister(FZRpcManager* InManager, const FZSwordPkSignupCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkSignup;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SwordPkSignupReq>();
        auto RspMessage = MakeShared<idlezt::SwordPkSignupRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkMatchingRegister(FZRpcManager* InManager, const FZSwordPkMatchingCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkMatching;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SwordPkMatchingReq>();
        auto RspMessage = MakeShared<idlezt::SwordPkMatchingRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkChallengeRegister(FZRpcManager* InManager, const FZSwordPkChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkChallenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SwordPkChallengeReq>();
        auto RspMessage = MakeShared<idlezt::SwordPkChallengeRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkRevengeRegister(FZRpcManager* InManager, const FZSwordPkRevengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkRevenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SwordPkRevengeReq>();
        auto RspMessage = MakeShared<idlezt::SwordPkRevengeRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSwordPkTopListRegister(FZRpcManager* InManager, const FZGetSwordPkTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSwordPkTopList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSwordPkTopListReq>();
        auto RspMessage = MakeShared<idlezt::GetSwordPkTopListRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SwordPkExchangeHeroCardRegister(FZRpcManager* InManager, const FZSwordPkExchangeHeroCardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SwordPkExchangeHeroCard;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SwordPkExchangeHeroCardReq>();
        auto RspMessage = MakeShared<idlezt::SwordPkExchangeHeroCardRsp>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCommonItemExchangeDataRegister(FZRpcManager* InManager, const FZGetCommonItemExchangeDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCommonItemExchangeData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetCommonItemExchangeDataReq>();
        auto RspMessage = MakeShared<idlezt::GetCommonItemExchangeDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExchangeCommonItemRegister(FZRpcManager* InManager, const FZExchangeCommonItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExchangeCommonItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ExchangeCommonItemReq>();
        auto RspMessage = MakeShared<idlezt::ExchangeCommonItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SynthesisCommonItemRegister(FZRpcManager* InManager, const FZSynthesisCommonItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SynthesisCommonItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SynthesisCommonItemReq>();
        auto RspMessage = MakeShared<idlezt::SynthesisCommonItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCandidatesSeptListRegister(FZRpcManager* InManager, const FZGetCandidatesSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesSeptList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetCandidatesSeptListReq>();
        auto RspMessage = MakeShared<idlezt::GetCandidatesSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SearchSeptRegister(FZRpcManager* InManager, const FZSearchSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SearchSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SearchSeptReq>();
        auto RspMessage = MakeShared<idlezt::SearchSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptBaseInfoRegister(FZRpcManager* InManager, const FZGetSeptBaseInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptBaseInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptBaseInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptBaseInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptMemberListRegister(FZRpcManager* InManager, const FZGetSeptMemberListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptMemberList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptMemberListReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptMemberListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::CreateSeptRegister(FZRpcManager* InManager, const FZCreateSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::CreateSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::CreateSeptReq>();
        auto RspMessage = MakeShared<idlezt::CreateSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DismissSeptRegister(FZRpcManager* InManager, const FZDismissSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DismissSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DismissSeptReq>();
        auto RspMessage = MakeShared<idlezt::DismissSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ExitSeptRegister(FZRpcManager* InManager, const FZExitSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ExitSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ExitSeptReq>();
        auto RspMessage = MakeShared<idlezt::ExitSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ApplyJoinSeptRegister(FZRpcManager* InManager, const FZApplyJoinSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ApplyJoinSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ApplyJoinSeptReq>();
        auto RspMessage = MakeShared<idlezt::ApplyJoinSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ApproveApplySeptRegister(FZRpcManager* InManager, const FZApproveApplySeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ApproveApplySept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ApproveApplySeptReq>();
        auto RspMessage = MakeShared<idlezt::ApproveApplySeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetApplyJoinSeptListRegister(FZRpcManager* InManager, const FZGetApplyJoinSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetApplyJoinSeptList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetApplyJoinSeptListReq>();
        auto RspMessage = MakeShared<idlezt::GetApplyJoinSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RespondInviteSeptRegister(FZRpcManager* InManager, const FZRespondInviteSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RespondInviteSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RespondInviteSeptReq>();
        auto RspMessage = MakeShared<idlezt::RespondInviteSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetInviteMeJoinSeptListRegister(FZRpcManager* InManager, const FZGetInviteMeJoinSeptListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetInviteMeJoinSeptList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetInviteMeJoinSeptListReq>();
        auto RspMessage = MakeShared<idlezt::GetInviteMeJoinSeptListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetCandidatesInviteRoleListRegister(FZRpcManager* InManager, const FZGetCandidatesInviteRoleListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetCandidatesInviteRoleList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetCandidatesInviteRoleListReq>();
        auto RspMessage = MakeShared<idlezt::GetCandidatesInviteRoleListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::InviteJoinSeptRegister(FZRpcManager* InManager, const FZInviteJoinSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::InviteJoinSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::InviteJoinSeptReq>();
        auto RspMessage = MakeShared<idlezt::InviteJoinSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetSeptSettingsRegister(FZRpcManager* InManager, const FZSetSeptSettingsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptSettings;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetSeptSettingsReq>();
        auto RspMessage = MakeShared<idlezt::SetSeptSettingsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::SetSeptAnnounceRegister(FZRpcManager* InManager, const FZSetSeptAnnounceCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::SetSeptAnnounce;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::SetSeptAnnounceReq>();
        auto RspMessage = MakeShared<idlezt::SetSeptAnnounceAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ChangeSeptNameRegister(FZRpcManager* InManager, const FZChangeSeptNameCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ChangeSeptName;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ChangeSeptNameReq>();
        auto RspMessage = MakeShared<idlezt::ChangeSeptNameAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptLogRegister(FZRpcManager* InManager, const FZGetSeptLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLog;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptLogReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ConstructSeptRegister(FZRpcManager* InManager, const FZConstructSeptCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ConstructSept;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ConstructSeptReq>();
        auto RspMessage = MakeShared<idlezt::ConstructSeptAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetConstructSeptLogRegister(FZRpcManager* InManager, const FZGetConstructSeptLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetConstructSeptLog;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetConstructSeptLogReq>();
        auto RspMessage = MakeShared<idlezt::GetConstructSeptLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptInvitedRoleDailyNumRegister(FZRpcManager* InManager, const FZGetSeptInvitedRoleDailyNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptInvitedRoleDailyNum;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptInvitedRoleDailyNumReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptInvitedRoleDailyNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppointSeptPositionRegister(FZRpcManager* InManager, const FZAppointSeptPositionCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppointSeptPosition;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppointSeptPositionReq>();
        auto RspMessage = MakeShared<idlezt::AppointSeptPositionAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ResignSeptChairmanRegister(FZRpcManager* InManager, const FZResignSeptChairmanCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ResignSeptChairman;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ResignSeptChairmanReq>();
        auto RspMessage = MakeShared<idlezt::ResignSeptChairmanAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::KickOutSeptMemberRegister(FZRpcManager* InManager, const FZKickOutSeptMemberCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::KickOutSeptMember;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::KickOutSeptMemberReq>();
        auto RspMessage = MakeShared<idlezt::KickOutSeptMemberAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleSeptShopDataRegister(FZRpcManager* InManager, const FZGetRoleSeptShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptShopData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleSeptShopDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleSeptShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BuySeptShopItemRegister(FZRpcManager* InManager, const FZBuySeptShopItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BuySeptShopItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::BuySeptShopItemReq>();
        auto RspMessage = MakeShared<idlezt::BuySeptShopItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleSeptQuestDataRegister(FZRpcManager* InManager, const FZGetRoleSeptQuestDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleSeptQuestData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleSeptQuestDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleSeptQuestDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReqRoleSeptQuestOpRegister(FZRpcManager* InManager, const FZReqRoleSeptQuestOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReqRoleSeptQuestOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReqRoleSeptQuestOpReq>();
        auto RspMessage = MakeShared<idlezt::ReqRoleSeptQuestOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshSeptQuestRegister(FZRpcManager* InManager, const FZRefreshSeptQuestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshSeptQuest;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RefreshSeptQuestReq>();
        auto RspMessage = MakeShared<idlezt::RefreshSeptQuestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReqSeptQuestRankUpRegister(FZRpcManager* InManager, const FZReqSeptQuestRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReqSeptQuestRankUp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReqSeptQuestRankUpReq>();
        auto RspMessage = MakeShared<idlezt::ReqSeptQuestRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::BeginOccupySeptStoneRegister(FZRpcManager* InManager, const FZBeginOccupySeptStoneCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::BeginOccupySeptStone;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::BeginOccupySeptStoneReq>();
        auto RspMessage = MakeShared<idlezt::BeginOccupySeptStoneAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::EndOccupySeptStoneRegister(FZRpcManager* InManager, const FZEndOccupySeptStoneCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::EndOccupySeptStone;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::EndOccupySeptStoneReq>();
        auto RspMessage = MakeShared<idlezt::EndOccupySeptStoneAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OccupySeptLandRegister(FZRpcManager* InManager, const FZOccupySeptLandCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OccupySeptLand;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OccupySeptLandReq>();
        auto RspMessage = MakeShared<idlezt::OccupySeptLandAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetGongFaDataRegister(FZRpcManager* InManager, const FZGetGongFaDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetGongFaData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetGongFaDataReq>();
        auto RspMessage = MakeShared<idlezt::GetGongFaDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GongFaOpRegister(FZRpcManager* InManager, const FZGongFaOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GongFaOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GongFaOpReq>();
        auto RspMessage = MakeShared<idlezt::GongFaOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ActivateGongFaMaxEffectRegister(FZRpcManager* InManager, const FZActivateGongFaMaxEffectCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ActivateGongFaMaxEffect;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ActivateGongFaMaxEffectReq>();
        auto RspMessage = MakeShared<idlezt::ActivateGongFaMaxEffectAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptLandDamageTopListRegister(FZRpcManager* InManager, const FZGetSeptLandDamageTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptLandDamageTopList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptLandDamageTopListReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptLandDamageTopListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveFuZengRewardsRegister(FZRpcManager* InManager, const FZReceiveFuZengRewardsCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveFuZengRewards;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReceiveFuZengRewardsReq>();
        auto RspMessage = MakeShared<idlezt::ReceiveFuZengRewardsAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleFuZengDataRegister(FZRpcManager* InManager, const FZGetRoleFuZengDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFuZengData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleFuZengDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleFuZengDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleTreasuryDataRegister(FZRpcManager* InManager, const FZGetRoleTreasuryDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleTreasuryData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleTreasuryDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleTreasuryDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OpenTreasuryChestRegister(FZRpcManager* InManager, const FZOpenTreasuryChestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryChest;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OpenTreasuryChestReq>();
        auto RspMessage = MakeShared<idlezt::OpenTreasuryChestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OneClickOpenTreasuryChestRegister(FZRpcManager* InManager, const FZOneClickOpenTreasuryChestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OneClickOpenTreasuryChest;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OneClickOpenTreasuryChestReq>();
        auto RspMessage = MakeShared<idlezt::OneClickOpenTreasuryChestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::OpenTreasuryGachaRegister(FZRpcManager* InManager, const FZOpenTreasuryGachaCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::OpenTreasuryGacha;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::OpenTreasuryGachaReq>();
        auto RspMessage = MakeShared<idlezt::OpenTreasuryGachaAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RefreshTreasuryShopRegister(FZRpcManager* InManager, const FZRefreshTreasuryShopCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RefreshTreasuryShop;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RefreshTreasuryShopReq>();
        auto RspMessage = MakeShared<idlezt::RefreshTreasuryShopAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::TreasuryShopBuyRegister(FZRpcManager* InManager, const FZTreasuryShopBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::TreasuryShopBuy;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::TreasuryShopBuyReq>();
        auto RspMessage = MakeShared<idlezt::TreasuryShopBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetLifeCounterDataRegister(FZRpcManager* InManager, const FZGetLifeCounterDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetLifeCounterData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetLifeCounterDataReq>();
        auto RspMessage = MakeShared<idlezt::GetLifeCounterDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DoQuestFightRegister(FZRpcManager* InManager, const FZDoQuestFightCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DoQuestFight;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DoQuestFightReq>();
        auto RspMessage = MakeShared<idlezt::DoQuestFightAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::QuestFightQuickEndRegister(FZRpcManager* InManager, const FZQuestFightQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::QuestFightQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::QuestFightQuickEndReq>();
        auto RspMessage = MakeShared<idlezt::QuestFightQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetAppearanceDataRegister(FZRpcManager* InManager, const FZGetAppearanceDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetAppearanceData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetAppearanceDataReq>();
        auto RspMessage = MakeShared<idlezt::GetAppearanceDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceAddRegister(FZRpcManager* InManager, const FZAppearanceAddCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceAdd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppearanceAddReq>();
        auto RspMessage = MakeShared<idlezt::AppearanceAddAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceActiveRegister(FZRpcManager* InManager, const FZAppearanceActiveCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceActive;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppearanceActiveReq>();
        auto RspMessage = MakeShared<idlezt::AppearanceActiveAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceWearRegister(FZRpcManager* InManager, const FZAppearanceWearCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceWear;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppearanceWearReq>();
        auto RspMessage = MakeShared<idlezt::AppearanceWearAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceBuyRegister(FZRpcManager* InManager, const FZAppearanceBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceBuy;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppearanceBuyReq>();
        auto RspMessage = MakeShared<idlezt::AppearanceBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AppearanceChangeSkTypeRegister(FZRpcManager* InManager, const FZAppearanceChangeSkTypeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AppearanceChangeSkType;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AppearanceChangeSkTypeReq>();
        auto RspMessage = MakeShared<idlezt::AppearanceChangeSkTypeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBattleHistoryInfoRegister(FZRpcManager* InManager, const FZGetBattleHistoryInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBattleHistoryInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetBattleHistoryInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetBattleHistoryInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetArenaCheckListDataRegister(FZRpcManager* InManager, const FZGetArenaCheckListDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaCheckListData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetArenaCheckListDataReq>();
        auto RspMessage = MakeShared<idlezt::GetArenaCheckListDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ArenaCheckListSubmitRegister(FZRpcManager* InManager, const FZArenaCheckListSubmitCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListSubmit;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ArenaCheckListSubmitReq>();
        auto RspMessage = MakeShared<idlezt::ArenaCheckListSubmitAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ArenaCheckListRewardSubmitRegister(FZRpcManager* InManager, const FZArenaCheckListRewardSubmitCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ArenaCheckListRewardSubmit;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ArenaCheckListRewardSubmitReq>();
        auto RspMessage = MakeShared<idlezt::ArenaCheckListRewardSubmitAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllChallengeRegister(FZRpcManager* InManager, const FZDungeonKillAllChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllChallenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonKillAllChallengeReq>();
        auto RspMessage = MakeShared<idlezt::DungeonKillAllChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllQuickEndRegister(FZRpcManager* InManager, const FZDungeonKillAllQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonKillAllQuickEndReq>();
        auto RspMessage = MakeShared<idlezt::DungeonKillAllQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonKillAllDataRegister(FZRpcManager* InManager, const FZDungeonKillAllDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonKillAllData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonKillAllDataReq>();
        auto RspMessage = MakeShared<idlezt::DungeonKillAllDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetFarmlandDataRegister(FZRpcManager* InManager, const FZGetFarmlandDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetFarmlandData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetFarmlandDataReq>();
        auto RspMessage = MakeShared<idlezt::GetFarmlandDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandUnlockBlockRegister(FZRpcManager* InManager, const FZFarmlandUnlockBlockCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandUnlockBlock;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandUnlockBlockReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandUnlockBlockAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandPlantSeedRegister(FZRpcManager* InManager, const FZFarmlandPlantSeedCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandPlantSeed;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandPlantSeedReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandPlantSeedAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandWateringRegister(FZRpcManager* InManager, const FZFarmlandWateringCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandWatering;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandWateringReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandWateringAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandRipeningRegister(FZRpcManager* InManager, const FZFarmlandRipeningCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandRipening;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandRipeningReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandRipeningAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandHarvestRegister(FZRpcManager* InManager, const FZFarmlandHarvestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandHarvest;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandHarvestReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandHarvestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmerRankUpRegister(FZRpcManager* InManager, const FZFarmerRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmerRankUp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmerRankUpReq>();
        auto RspMessage = MakeShared<idlezt::FarmerRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FarmlandSetManagementRegister(FZRpcManager* InManager, const FZFarmlandSetManagementCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FarmlandSetManagement;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FarmlandSetManagementReq>();
        auto RspMessage = MakeShared<idlezt::FarmlandSetManagementAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::UpdateFarmlandStateRegister(FZRpcManager* InManager, const FZUpdateFarmlandStateCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::UpdateFarmlandState;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::UpdateFarmlandStateReq>();
        auto RspMessage = MakeShared<idlezt::UpdateFarmlandStateAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveChallengeRegister(FZRpcManager* InManager, const FZDungeonSurviveChallengeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveChallenge;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonSurviveChallengeReq>();
        auto RspMessage = MakeShared<idlezt::DungeonSurviveChallengeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveQuickEndRegister(FZRpcManager* InManager, const FZDungeonSurviveQuickEndCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveQuickEnd;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonSurviveQuickEndReq>();
        auto RspMessage = MakeShared<idlezt::DungeonSurviveQuickEndAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DungeonSurviveDataRegister(FZRpcManager* InManager, const FZDungeonSurviveDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DungeonSurviveData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DungeonSurviveDataReq>();
        auto RspMessage = MakeShared<idlezt::DungeonSurviveDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRevertAllSkillCoolDownRegister(FZRpcManager* InManager, const FZGetRevertAllSkillCoolDownCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRevertAllSkillCoolDown;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRevertAllSkillCoolDownReq>();
        auto RspMessage = MakeShared<idlezt::GetRevertAllSkillCoolDownAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleFriendDataRegister(FZRpcManager* InManager, const FZGetRoleFriendDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleFriendData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleFriendDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleFriendDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FriendOpRegister(FZRpcManager* InManager, const FZFriendOpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FriendOp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FriendOpReq>();
        auto RspMessage = MakeShared<idlezt::FriendOpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReplyFriendRequestRegister(FZRpcManager* InManager, const FZReplyFriendRequestCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReplyFriendRequest;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReplyFriendRequestReq>();
        auto RspMessage = MakeShared<idlezt::ReplyFriendRequestAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::FriendSearchRoleInfoRegister(FZRpcManager* InManager, const FZFriendSearchRoleInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::FriendSearchRoleInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::FriendSearchRoleInfoReq>();
        auto RspMessage = MakeShared<idlezt::FriendSearchRoleInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleInfoCacheRegister(FZRpcManager* InManager, const FZGetRoleInfoCacheCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfoCache;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleInfoCacheReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleInfoCacheAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleInfoRegister(FZRpcManager* InManager, const FZGetRoleInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleAvatarDataRegister(FZRpcManager* InManager, const FZGetRoleAvatarDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleAvatarData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleAvatarDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleAvatarDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::DispatchAvatarRegister(FZRpcManager* InManager, const FZDispatchAvatarCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::DispatchAvatar;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::DispatchAvatarReq>();
        auto RspMessage = MakeShared<idlezt::DispatchAvatarAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AvatarRankUpRegister(FZRpcManager* InManager, const FZAvatarRankUpCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AvatarRankUp;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AvatarRankUpReq>();
        auto RspMessage = MakeShared<idlezt::AvatarRankUpAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveAvatarTempPackageRegister(FZRpcManager* InManager, const FZReceiveAvatarTempPackageCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveAvatarTempPackage;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReceiveAvatarTempPackageReq>();
        auto RspMessage = MakeShared<idlezt::ReceiveAvatarTempPackageAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetArenaExplorationStatisticalDataRegister(FZRpcManager* InManager, const FZGetArenaExplorationStatisticalDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetArenaExplorationStatisticalData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetArenaExplorationStatisticalDataReq>();
        auto RspMessage = MakeShared<idlezt::GetArenaExplorationStatisticalDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleBiographyDataRegister(FZRpcManager* InManager, const FZGetRoleBiographyDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleBiographyData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleBiographyDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleBiographyDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBiographyItemRegister(FZRpcManager* InManager, const FZReceiveBiographyItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReceiveBiographyItemReq>();
        auto RspMessage = MakeShared<idlezt::ReceiveBiographyItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetBiographyEventDataRegister(FZRpcManager* InManager, const FZGetBiographyEventDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetBiographyEventData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetBiographyEventDataReq>();
        auto RspMessage = MakeShared<idlezt::GetBiographyEventDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::ReceiveBiographyEventItemRegister(FZRpcManager* InManager, const FZReceiveBiographyEventItemCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::ReceiveBiographyEventItem;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::ReceiveBiographyEventItemReq>();
        auto RspMessage = MakeShared<idlezt::ReceiveBiographyEventItemAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::AddBiographyRoleLogRegister(FZRpcManager* InManager, const FZAddBiographyRoleLogCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::AddBiographyRoleLog;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::AddBiographyRoleLogReq>();
        auto RspMessage = MakeShared<idlezt::AddBiographyRoleLogAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestEnterSeptDemonWorldRegister(FZRpcManager* InManager, const FZRequestEnterSeptDemonWorldCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestEnterSeptDemonWorld;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestEnterSeptDemonWorldReq>();
        auto RspMessage = MakeShared<idlezt::RequestEnterSeptDemonWorldAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestLeaveSeptDemonWorldRegister(FZRpcManager* InManager, const FZRequestLeaveSeptDemonWorldCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestLeaveSeptDemonWorld;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestLeaveSeptDemonWorldReq>();
        auto RspMessage = MakeShared<idlezt::RequestLeaveSeptDemonWorldAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestSeptDemonWorldDataRegister(FZRpcManager* InManager, const FZRequestSeptDemonWorldDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestSeptDemonWorldData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestSeptDemonWorldDataReq>();
        auto RspMessage = MakeShared<idlezt::RequestSeptDemonWorldDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::RequestInSeptDemonWorldEndTimeRegister(FZRpcManager* InManager, const FZRequestInSeptDemonWorldEndTimeCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::RequestInSeptDemonWorldEndTime;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::RequestInSeptDemonWorldEndTimeReq>();
        auto RspMessage = MakeShared<idlezt::RequestInSeptDemonWorldEndTimeAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageTopListRegister(FZRpcManager* InManager, const FZGetSeptDemonDamageTopListCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageTopList;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageTopListReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageTopListAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageSelfSummaryRegister(FZRpcManager* InManager, const FZGetSeptDemonDamageSelfSummaryCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageSelfSummary;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageSelfSummaryReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageSelfSummaryAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonStageRewardNumRegister(FZRpcManager* InManager, const FZGetSeptDemonStageRewardNumCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageRewardNum;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonStageRewardNumReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonStageRewardNumAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonStageRewardRegister(FZRpcManager* InManager, const FZGetSeptDemonStageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonStageReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonStageRewardReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonStageRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageRewardsInfoRegister(FZRpcManager* InManager, const FZGetSeptDemonDamageRewardsInfoCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageRewardsInfo;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageRewardsInfoReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageRewardsInfoAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetSeptDemonDamageRewardRegister(FZRpcManager* InManager, const FZGetSeptDemonDamageRewardCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetSeptDemonDamageReward;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetSeptDemonDamageRewardReq>();
        auto RspMessage = MakeShared<idlezt::GetSeptDemonDamageRewardAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::GetRoleVipShopDataRegister(FZRpcManager* InManager, const FZGetRoleVipShopDataCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::GetRoleVipShopData;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::GetRoleVipShopDataReq>();
        auto RspMessage = MakeShared<idlezt::GetRoleVipShopDataAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}

void FZGameRpcInterface::VipShopBuyRegister(FZRpcManager* InManager, const FZVipShopBuyCallback& InCallback)
{
    static constexpr uint64 RpcId = FZGameRpcInterface::VipShopBuy;
    InManager->AddMethod(RpcId, [InCallback](FZPbMessageSupportBase* InConn, const TSharedPtr<idlezt::ZRpcMessage>& InMessage)
    {
        const uint64 ReqSerialNum = InMessage->sn();
        auto ErrorCode = idlezt::RpcErrorCode_Unimplemented;
        auto ReqMessage = MakeShared<idlezt::VipShopBuyReq>();
        auto RspMessage = MakeShared<idlezt::VipShopBuyAck>();
        if (ReqMessage->ParseFromString(InMessage->body_data()))
        {
            if (InCallback)
            {
                InCallback(InConn, ReqMessage, RspMessage);
                ErrorCode = idlezt::RpcErrorCode_Ok;
            }
        }
        else
        {
            ErrorCode = idlezt::RpcErrorCode_ReqDataError;
        }
        FZRpcManager::SendResponse(InConn, RpcId, ReqSerialNum, RspMessage, ErrorCode);
    });    
}
