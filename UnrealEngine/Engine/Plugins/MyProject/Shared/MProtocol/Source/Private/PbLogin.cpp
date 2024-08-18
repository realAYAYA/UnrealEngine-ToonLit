#include "PbLogin.h"
#include "login.pb.h"



FPbLoginAccountReq::FPbLoginAccountReq()
{
    Reset();        
}

FPbLoginAccountReq::FPbLoginAccountReq(const idlezt::LoginAccountReq& Right)
{
    this->FromPb(Right);
}

void FPbLoginAccountReq::FromPb(const idlezt::LoginAccountReq& Right)
{
    account = UTF8_TO_TCHAR(Right.account().c_str());
    client_version = UTF8_TO_TCHAR(Right.client_version().c_str());
}

void FPbLoginAccountReq::ToPb(idlezt::LoginAccountReq* Out) const
{
    Out->set_account(TCHAR_TO_UTF8(*account));
    Out->set_client_version(TCHAR_TO_UTF8(*client_version));    
}

void FPbLoginAccountReq::Reset()
{
    account = FString();
    client_version = FString();    
}

void FPbLoginAccountReq::operator=(const idlezt::LoginAccountReq& Right)
{
    this->FromPb(Right);
}

bool FPbLoginAccountReq::operator==(const FPbLoginAccountReq& Right) const
{
    if (this->account != Right.account)
        return false;
    if (this->client_version != Right.client_version)
        return false;
    return true;
}

bool FPbLoginAccountReq::operator!=(const FPbLoginAccountReq& Right) const
{
    return !operator==(Right);
}

FPbLoginAccountAck::FPbLoginAccountAck()
{
    Reset();        
}

FPbLoginAccountAck::FPbLoginAccountAck(const idlezt::LoginAccountAck& Right)
{
    this->FromPb(Right);
}

void FPbLoginAccountAck::FromPb(const idlezt::LoginAccountAck& Right)
{
}

void FPbLoginAccountAck::ToPb(idlezt::LoginAccountAck* Out) const
{    
}

void FPbLoginAccountAck::Reset()
{    
}

void FPbLoginAccountAck::operator=(const idlezt::LoginAccountAck& Right)
{
    this->FromPb(Right);
}

bool FPbLoginAccountAck::operator==(const FPbLoginAccountAck& Right) const
{
    return true;
}

bool FPbLoginAccountAck::operator!=(const FPbLoginAccountAck& Right) const
{
    return !operator==(Right);
}