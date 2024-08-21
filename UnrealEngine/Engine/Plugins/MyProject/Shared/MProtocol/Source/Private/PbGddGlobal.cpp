#include "PbGddGlobal.h"
#include "gdd_global.pb.h"



FPbCommonGlobalConfig::FPbCommonGlobalConfig()
{
    Reset();        
}

FPbCommonGlobalConfig::FPbCommonGlobalConfig(const idlepb::CommonGlobalConfig& Right)
{
    this->FromPb(Right);
}

void FPbCommonGlobalConfig::FromPb(const idlepb::CommonGlobalConfig& Right)
{
    ts_rpc_max_seconds = Right.ts_rpc_max_seconds();
}

void FPbCommonGlobalConfig::ToPb(idlepb::CommonGlobalConfig* Out) const
{
    Out->set_ts_rpc_max_seconds(ts_rpc_max_seconds);    
}

void FPbCommonGlobalConfig::Reset()
{
    ts_rpc_max_seconds = float();    
}

void FPbCommonGlobalConfig::operator=(const idlepb::CommonGlobalConfig& Right)
{
    this->FromPb(Right);
}

bool FPbCommonGlobalConfig::operator==(const FPbCommonGlobalConfig& Right) const
{
    if (this->ts_rpc_max_seconds != Right.ts_rpc_max_seconds)
        return false;
    return true;
}

bool FPbCommonGlobalConfig::operator!=(const FPbCommonGlobalConfig& Right) const
{
    return !operator==(Right);
}

FPbGameServicesConfig::FPbGameServicesConfig()
{
    Reset();        
}

FPbGameServicesConfig::FPbGameServicesConfig(const idlepb::GameServicesConfig& Right)
{
    this->FromPb(Right);
}

void FPbGameServicesConfig::FromPb(const idlepb::GameServicesConfig& Right)
{
    listen_ip = UTF8_TO_TCHAR(Right.listen_ip().c_str());
    port = Right.port();
    redis_ip = UTF8_TO_TCHAR(Right.redis_ip().c_str());
    redis_port = Right.redis_port();
    redis_password = UTF8_TO_TCHAR(Right.redis_password().c_str());
}

void FPbGameServicesConfig::ToPb(idlepb::GameServicesConfig* Out) const
{
    Out->set_listen_ip(TCHAR_TO_UTF8(*listen_ip));
    Out->set_port(port);
    Out->set_redis_ip(TCHAR_TO_UTF8(*redis_ip));
    Out->set_redis_port(redis_port);
    Out->set_redis_password(TCHAR_TO_UTF8(*redis_password));    
}

void FPbGameServicesConfig::Reset()
{
    listen_ip = FString();
    port = int32();
    redis_ip = FString();
    redis_port = int32();
    redis_password = FString();    
}

void FPbGameServicesConfig::operator=(const idlepb::GameServicesConfig& Right)
{
    this->FromPb(Right);
}

bool FPbGameServicesConfig::operator==(const FPbGameServicesConfig& Right) const
{
    if (this->listen_ip != Right.listen_ip)
        return false;
    if (this->port != Right.port)
        return false;
    if (this->redis_ip != Right.redis_ip)
        return false;
    if (this->redis_port != Right.redis_port)
        return false;
    if (this->redis_password != Right.redis_password)
        return false;
    return true;
}

bool FPbGameServicesConfig::operator!=(const FPbGameServicesConfig& Right) const
{
    return !operator==(Right);
}

FPbGameClientConfig::FPbGameClientConfig()
{
    Reset();        
}

FPbGameClientConfig::FPbGameClientConfig(const idlepb::GameClientConfig& Right)
{
    this->FromPb(Right);
}

void FPbGameClientConfig::FromPb(const idlepb::GameClientConfig& Right)
{
    server_ip = UTF8_TO_TCHAR(Right.server_ip().c_str());
    server_port = Right.server_port();
}

void FPbGameClientConfig::ToPb(idlepb::GameClientConfig* Out) const
{
    Out->set_server_ip(TCHAR_TO_UTF8(*server_ip));
    Out->set_server_port(server_port);    
}

void FPbGameClientConfig::Reset()
{
    server_ip = FString();
    server_port = int32();    
}

void FPbGameClientConfig::operator=(const idlepb::GameClientConfig& Right)
{
    this->FromPb(Right);
}

bool FPbGameClientConfig::operator==(const FPbGameClientConfig& Right) const
{
    if (this->server_ip != Right.server_ip)
        return false;
    if (this->server_port != Right.server_port)
        return false;
    return true;
}

bool FPbGameClientConfig::operator!=(const FPbGameClientConfig& Right) const
{
    return !operator==(Right);
}