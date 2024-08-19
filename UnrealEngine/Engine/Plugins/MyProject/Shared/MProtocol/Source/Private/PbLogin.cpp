#include "PbLogin.h"
#include "login.pb.h"



FPbLoginAccountReq::FPbLoginAccountReq()
{
    Reset();        
}

FPbLoginAccountReq::FPbLoginAccountReq(const idlepb::LoginAccountReq& Right)
{
    this->FromPb(Right);
}

void FPbLoginAccountReq::FromPb(const idlepb::LoginAccountReq& Right)
{
    account = UTF8_TO_TCHAR(Right.account().c_str());
    client_version = UTF8_TO_TCHAR(Right.client_version().c_str());
}

void FPbLoginAccountReq::ToPb(idlepb::LoginAccountReq* Out) const
{
    Out->set_account(TCHAR_TO_UTF8(*account));
    Out->set_client_version(TCHAR_TO_UTF8(*client_version));    
}

void FPbLoginAccountReq::Reset()
{
    account = FString();
    client_version = FString();    
}

void FPbLoginAccountReq::operator=(const idlepb::LoginAccountReq& Right)
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

FPbLoginAccountAck::FPbLoginAccountAck(const idlepb::LoginAccountAck& Right)
{
    this->FromPb(Right);
}

void FPbLoginAccountAck::FromPb(const idlepb::LoginAccountAck& Right)
{
}

void FPbLoginAccountAck::ToPb(idlepb::LoginAccountAck* Out) const
{    
}

void FPbLoginAccountAck::Reset()
{    
}

void FPbLoginAccountAck::operator=(const idlepb::LoginAccountAck& Right)
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