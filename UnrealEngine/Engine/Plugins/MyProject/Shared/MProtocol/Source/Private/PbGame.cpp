#include "PbGame.h"
#include "game.pb.h"



FPbPing::FPbPing()
{
    Reset();        
}

FPbPing::FPbPing(const idlepb::Ping& Right)
{
    this->FromPb(Right);
}

void FPbPing::FromPb(const idlepb::Ping& Right)
{
    req_ticks = Right.req_ticks();
}

void FPbPing::ToPb(idlepb::Ping* Out) const
{
    Out->set_req_ticks(req_ticks);    
}

void FPbPing::Reset()
{
    req_ticks = int64();    
}

void FPbPing::operator=(const idlepb::Ping& Right)
{
    this->FromPb(Right);
}

bool FPbPing::operator==(const FPbPing& Right) const
{
    if (this->req_ticks != Right.req_ticks)
        return false;
    return true;
}

bool FPbPing::operator!=(const FPbPing& Right) const
{
    return !operator==(Right);
}

FPbPong::FPbPong()
{
    Reset();        
}

FPbPong::FPbPong(const idlepb::Pong& Right)
{
    this->FromPb(Right);
}

void FPbPong::FromPb(const idlepb::Pong& Right)
{
    req_ticks = Right.req_ticks();
    rsp_ticks = Right.rsp_ticks();
}

void FPbPong::ToPb(idlepb::Pong* Out) const
{
    Out->set_req_ticks(req_ticks);
    Out->set_rsp_ticks(rsp_ticks);    
}

void FPbPong::Reset()
{
    req_ticks = int64();
    rsp_ticks = int64();    
}

void FPbPong::operator=(const idlepb::Pong& Right)
{
    this->FromPb(Right);
}

bool FPbPong::operator==(const FPbPong& Right) const
{
    if (this->req_ticks != Right.req_ticks)
        return false;
    if (this->rsp_ticks != Right.rsp_ticks)
        return false;
    return true;
}

bool FPbPong::operator!=(const FPbPong& Right) const
{
    return !operator==(Right);
}

FPbDoGmCommand::FPbDoGmCommand()
{
    Reset();        
}

FPbDoGmCommand::FPbDoGmCommand(const idlepb::DoGmCommand& Right)
{
    this->FromPb(Right);
}

void FPbDoGmCommand::FromPb(const idlepb::DoGmCommand& Right)
{
    command = UTF8_TO_TCHAR(Right.command().c_str());
}

void FPbDoGmCommand::ToPb(idlepb::DoGmCommand* Out) const
{
    Out->set_command(TCHAR_TO_UTF8(*command));    
}

void FPbDoGmCommand::Reset()
{
    command = FString();    
}

void FPbDoGmCommand::operator=(const idlepb::DoGmCommand& Right)
{
    this->FromPb(Right);
}

bool FPbDoGmCommand::operator==(const FPbDoGmCommand& Right) const
{
    if (this->command != Right.command)
        return false;
    return true;
}

bool FPbDoGmCommand::operator!=(const FPbDoGmCommand& Right) const
{
    return !operator==(Right);
}

FPbReportError::FPbReportError()
{
    Reset();        
}

FPbReportError::FPbReportError(const idlepb::ReportError& Right)
{
    this->FromPb(Right);
}

void FPbReportError::FromPb(const idlepb::ReportError& Right)
{
    text = UTF8_TO_TCHAR(Right.text().c_str());
}

void FPbReportError::ToPb(idlepb::ReportError* Out) const
{
    Out->set_text(TCHAR_TO_UTF8(*text));    
}

void FPbReportError::Reset()
{
    text = FString();    
}

void FPbReportError::operator=(const idlepb::ReportError& Right)
{
    this->FromPb(Right);
}

bool FPbReportError::operator==(const FPbReportError& Right) const
{
    if (this->text != Right.text)
        return false;
    return true;
}

bool FPbReportError::operator!=(const FPbReportError& Right) const
{
    return !operator==(Right);
}

bool CheckEPbLoginGameRetCodeValid(int32 Val)
{
    return idlepb::LoginGameRetCode_IsValid(Val);
}

const TCHAR* GetEPbLoginGameRetCodeDescription(EPbLoginGameRetCode Val)
{
    switch (Val)
    {
        case EPbLoginGameRetCode::LoginGameRetCode_Ok: return TEXT("正常登陆");
        case EPbLoginGameRetCode::LoginGameRetCode_Unknown: return TEXT("未知错误");
        case EPbLoginGameRetCode::LoginGameRetCode_NoRole: return TEXT("没有角色");
        case EPbLoginGameRetCode::LoginGameRetCode_DuplicateLogin: return TEXT("已经在线");
        case EPbLoginGameRetCode::LoginGameRetCode_AccountInvalid: return TEXT("帐号非法");
        case EPbLoginGameRetCode::LoginGameRetCode_VersionError: return TEXT("版本错误");
    }
    return TEXT("UNKNOWN");
}

FPbLoginGameReq::FPbLoginGameReq()
{
    Reset();        
}

FPbLoginGameReq::FPbLoginGameReq(const idlepb::LoginGameReq& Right)
{
    this->FromPb(Right);
}

void FPbLoginGameReq::FromPb(const idlepb::LoginGameReq& Right)
{
    account = UTF8_TO_TCHAR(Right.account().c_str());
    client_version = UTF8_TO_TCHAR(Right.client_version().c_str());
}

void FPbLoginGameReq::ToPb(idlepb::LoginGameReq* Out) const
{
    Out->set_account(TCHAR_TO_UTF8(*account));
    Out->set_client_version(TCHAR_TO_UTF8(*client_version));    
}

void FPbLoginGameReq::Reset()
{
    account = FString();
    client_version = FString();    
}

void FPbLoginGameReq::operator=(const idlepb::LoginGameReq& Right)
{
    this->FromPb(Right);
}

bool FPbLoginGameReq::operator==(const FPbLoginGameReq& Right) const
{
    if (this->account != Right.account)
        return false;
    if (this->client_version != Right.client_version)
        return false;
    return true;
}

bool FPbLoginGameReq::operator!=(const FPbLoginGameReq& Right) const
{
    return !operator==(Right);
}

FPbLoginGameAck::FPbLoginGameAck()
{
    Reset();        
}

FPbLoginGameAck::FPbLoginGameAck(const idlepb::LoginGameAck& Right)
{
    this->FromPb(Right);
}

void FPbLoginGameAck::FromPb(const idlepb::LoginGameAck& Right)
{
    ret = static_cast<EPbLoginGameRetCode>(Right.ret());
    player_data = Right.player_data();
    is_relogin = Right.is_relogin();
}

void FPbLoginGameAck::ToPb(idlepb::LoginGameAck* Out) const
{
    Out->set_ret(static_cast<idlepb::LoginGameRetCode>(ret));
    player_data.ToPb(Out->mutable_player_data());
    Out->set_is_relogin(is_relogin);    
}

void FPbLoginGameAck::Reset()
{
    ret = EPbLoginGameRetCode();
    player_data = FPbPlayerData();
    is_relogin = bool();    
}

void FPbLoginGameAck::operator=(const idlepb::LoginGameAck& Right)
{
    this->FromPb(Right);
}

bool FPbLoginGameAck::operator==(const FPbLoginGameAck& Right) const
{
    if (this->ret != Right.ret)
        return false;
    if (this->player_data != Right.player_data)
        return false;
    if (this->is_relogin != Right.is_relogin)
        return false;
    return true;
}

bool FPbLoginGameAck::operator!=(const FPbLoginGameAck& Right) const
{
    return !operator==(Right);
}

FPbEnterLevelReq::FPbEnterLevelReq()
{
    Reset();        
}

FPbEnterLevelReq::FPbEnterLevelReq(const idlepb::EnterLevelReq& Right)
{
    this->FromPb(Right);
}

void FPbEnterLevelReq::FromPb(const idlepb::EnterLevelReq& Right)
{
}

void FPbEnterLevelReq::ToPb(idlepb::EnterLevelReq* Out) const
{    
}

void FPbEnterLevelReq::Reset()
{    
}

void FPbEnterLevelReq::operator=(const idlepb::EnterLevelReq& Right)
{
    this->FromPb(Right);
}

bool FPbEnterLevelReq::operator==(const FPbEnterLevelReq& Right) const
{
    return true;
}

bool FPbEnterLevelReq::operator!=(const FPbEnterLevelReq& Right) const
{
    return !operator==(Right);
}

FPbEnterLevelAck::FPbEnterLevelAck()
{
    Reset();        
}

FPbEnterLevelAck::FPbEnterLevelAck(const idlepb::EnterLevelAck& Right)
{
    this->FromPb(Right);
}

void FPbEnterLevelAck::FromPb(const idlepb::EnterLevelAck& Right)
{
    ok = Right.ok();
}

void FPbEnterLevelAck::ToPb(idlepb::EnterLevelAck* Out) const
{
    Out->set_ok(ok);    
}

void FPbEnterLevelAck::Reset()
{
    ok = bool();    
}

void FPbEnterLevelAck::operator=(const idlepb::EnterLevelAck& Right)
{
    this->FromPb(Right);
}

bool FPbEnterLevelAck::operator==(const FPbEnterLevelAck& Right) const
{
    if (this->ok != Right.ok)
        return false;
    return true;
}

bool FPbEnterLevelAck::operator!=(const FPbEnterLevelAck& Right) const
{
    return !operator==(Right);
}