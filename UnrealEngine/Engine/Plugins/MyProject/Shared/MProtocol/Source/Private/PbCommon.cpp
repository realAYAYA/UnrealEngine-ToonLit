#include "PbCommon.h"
#include "common.pb.h"



FPbInt64Pair::FPbInt64Pair()
{
    Reset();        
}

FPbInt64Pair::FPbInt64Pair(const idlepb::Int64Pair& Right)
{
    this->FromPb(Right);
}

void FPbInt64Pair::FromPb(const idlepb::Int64Pair& Right)
{
    v1 = Right.v1();
    v2 = Right.v2();
}

void FPbInt64Pair::ToPb(idlepb::Int64Pair* Out) const
{
    Out->set_v1(v1);
    Out->set_v2(v2);    
}

void FPbInt64Pair::Reset()
{
    v1 = int64();
    v2 = int64();    
}

void FPbInt64Pair::operator=(const idlepb::Int64Pair& Right)
{
    this->FromPb(Right);
}

bool FPbInt64Pair::operator==(const FPbInt64Pair& Right) const
{
    if (this->v1 != Right.v1)
        return false;
    if (this->v2 != Right.v2)
        return false;
    return true;
}

bool FPbInt64Pair::operator!=(const FPbInt64Pair& Right) const
{
    return !operator==(Right);
}

FPbInt32Pair::FPbInt32Pair()
{
    Reset();        
}

FPbInt32Pair::FPbInt32Pair(const idlepb::Int32Pair& Right)
{
    this->FromPb(Right);
}

void FPbInt32Pair::FromPb(const idlepb::Int32Pair& Right)
{
    v1 = Right.v1();
    v2 = Right.v2();
}

void FPbInt32Pair::ToPb(idlepb::Int32Pair* Out) const
{
    Out->set_v1(v1);
    Out->set_v2(v2);    
}

void FPbInt32Pair::Reset()
{
    v1 = int64();
    v2 = int64();    
}

void FPbInt32Pair::operator=(const idlepb::Int32Pair& Right)
{
    this->FromPb(Right);
}

bool FPbInt32Pair::operator==(const FPbInt32Pair& Right) const
{
    if (this->v1 != Right.v1)
        return false;
    if (this->v2 != Right.v2)
        return false;
    return true;
}

bool FPbInt32Pair::operator!=(const FPbInt32Pair& Right) const
{
    return !operator==(Right);
}

FPbStringInt32Pair::FPbStringInt32Pair()
{
    Reset();        
}

FPbStringInt32Pair::FPbStringInt32Pair(const idlepb::StringInt32Pair& Right)
{
    this->FromPb(Right);
}

void FPbStringInt32Pair::FromPb(const idlepb::StringInt32Pair& Right)
{
    str = UTF8_TO_TCHAR(Right.str().c_str());
    value = Right.value();
}

void FPbStringInt32Pair::ToPb(idlepb::StringInt32Pair* Out) const
{
    Out->set_str(TCHAR_TO_UTF8(*str));
    Out->set_value(value);    
}

void FPbStringInt32Pair::Reset()
{
    str = FString();
    value = int32();    
}

void FPbStringInt32Pair::operator=(const idlepb::StringInt32Pair& Right)
{
    this->FromPb(Right);
}

bool FPbStringInt32Pair::operator==(const FPbStringInt32Pair& Right) const
{
    if (this->str != Right.str)
        return false;
    if (this->value != Right.value)
        return false;
    return true;
}

bool FPbStringInt32Pair::operator!=(const FPbStringInt32Pair& Right) const
{
    return !operator==(Right);
}

FPbInt32Int64Pair::FPbInt32Int64Pair()
{
    Reset();        
}

FPbInt32Int64Pair::FPbInt32Int64Pair(const idlepb::Int32Int64Pair& Right)
{
    this->FromPb(Right);
}

void FPbInt32Int64Pair::FromPb(const idlepb::Int32Int64Pair& Right)
{
    v32 = Right.v32();
    v64 = Right.v64();
}

void FPbInt32Int64Pair::ToPb(idlepb::Int32Int64Pair* Out) const
{
    Out->set_v32(v32);
    Out->set_v64(v64);    
}

void FPbInt32Int64Pair::Reset()
{
    v32 = int32();
    v64 = int64();    
}

void FPbInt32Int64Pair::operator=(const idlepb::Int32Int64Pair& Right)
{
    this->FromPb(Right);
}

bool FPbInt32Int64Pair::operator==(const FPbInt32Int64Pair& Right) const
{
    if (this->v32 != Right.v32)
        return false;
    if (this->v64 != Right.v64)
        return false;
    return true;
}

bool FPbInt32Int64Pair::operator!=(const FPbInt32Int64Pair& Right) const
{
    return !operator==(Right);
}

bool CheckEPbReplicatedLevelTypeValid(int32 Val)
{
    return idlepb::ReplicatedLevelType_IsValid(Val);
}

const TCHAR* GetEPbReplicatedLevelTypeDescription(EPbReplicatedLevelType Val)
{
    switch (Val)
    {
        case EPbReplicatedLevelType::RLT_Local: return TEXT("自己所在客户端");
        case EPbReplicatedLevelType::RLT_Offical: return TEXT("官服");
        case EPbReplicatedLevelType::RLT_Private: return TEXT("私服");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSystemNoticeStyleValid(int32 Val)
{
    return idlepb::SystemNoticeStyle_IsValid(Val);
}

const TCHAR* GetEPbSystemNoticeStyleDescription(EPbSystemNoticeStyle Val)
{
    switch (Val)
    {
        case EPbSystemNoticeStyle::SystemNoticeStyle_None: return TEXT("未知");
        case EPbSystemNoticeStyle::SystemNoticeStyle_Dialog: return TEXT("弹框提示");
        case EPbSystemNoticeStyle::SystemNoticeStyle_ScreenCenter: return TEXT("屏幕中央提示");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbSystemNoticeTypeValid(int32 Val)
{
    return idlepb::SystemNoticeType_IsValid(Val);
}

const TCHAR* GetEPbSystemNoticeTypeDescription(EPbSystemNoticeType Val)
{
    switch (Val)
    {
        case EPbSystemNoticeType::SNT: return TEXT("未知");
        case EPbSystemNoticeType::SNT_AddItem: return TEXT("添加道具");
    }
    return TEXT("UNKNOWN");
}

FPbPlayerData::FPbPlayerData()
{
    Reset();        
}

FPbPlayerData::FPbPlayerData(const idlepb::PlayerData& Right)
{
    this->FromPb(Right);
}

void FPbPlayerData::FromPb(const idlepb::PlayerData& Right)
{
    player_id = Right.player_id();
    player_name = UTF8_TO_TCHAR(Right.player_name().c_str());
    last_online_date = Right.last_online_date();
    create_date = Right.create_date();
}

void FPbPlayerData::ToPb(idlepb::PlayerData* Out) const
{
    Out->set_player_id(player_id);
    Out->set_player_name(TCHAR_TO_UTF8(*player_name));
    Out->set_last_online_date(last_online_date);
    Out->set_create_date(create_date);    
}

void FPbPlayerData::Reset()
{
    player_id = int64();
    player_name = FString();
    last_online_date = int64();
    create_date = int64();    
}

void FPbPlayerData::operator=(const idlepb::PlayerData& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerData::operator==(const FPbPlayerData& Right) const
{
    if (this->player_id != Right.player_id)
        return false;
    if (this->player_name != Right.player_name)
        return false;
    if (this->last_online_date != Right.last_online_date)
        return false;
    if (this->create_date != Right.create_date)
        return false;
    return true;
}

bool FPbPlayerData::operator!=(const FPbPlayerData& Right) const
{
    return !operator==(Right);
}

FPbPlayerSaveData::FPbPlayerSaveData()
{
    Reset();        
}

FPbPlayerSaveData::FPbPlayerSaveData(const idlepb::PlayerSaveData& Right)
{
    this->FromPb(Right);
}

void FPbPlayerSaveData::FromPb(const idlepb::PlayerSaveData& Right)
{
    player_data = Right.player_data();
}

void FPbPlayerSaveData::ToPb(idlepb::PlayerSaveData* Out) const
{
    player_data.ToPb(Out->mutable_player_data());    
}

void FPbPlayerSaveData::Reset()
{
    player_data = FPbPlayerData();    
}

void FPbPlayerSaveData::operator=(const idlepb::PlayerSaveData& Right)
{
    this->FromPb(Right);
}

bool FPbPlayerSaveData::operator==(const FPbPlayerSaveData& Right) const
{
    if (this->player_data != Right.player_data)
        return false;
    return true;
}

bool FPbPlayerSaveData::operator!=(const FPbPlayerSaveData& Right) const
{
    return !operator==(Right);
}