#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbCommon.h"
#include "PbGddGlobal.generated.h"


namespace idlepb {
class CommonGlobalConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbCommonGlobalConfig
{
    GENERATED_BODY();

    /** TsRpc 超时时间 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    float ts_rpc_max_seconds;


    FPbCommonGlobalConfig();
    FPbCommonGlobalConfig(const idlepb::CommonGlobalConfig& Right);
    void FromPb(const idlepb::CommonGlobalConfig& Right);
    void ToPb(idlepb::CommonGlobalConfig* Out) const;
    void Reset();
    void operator=(const idlepb::CommonGlobalConfig& Right);
    bool operator==(const FPbCommonGlobalConfig& Right) const;
    bool operator!=(const FPbCommonGlobalConfig& Right) const;
     
};

namespace idlepb {
class GameServicesConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameServicesConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString listen_ip;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 port;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString redis_ip;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 redis_port;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString redis_password;


    FPbGameServicesConfig();
    FPbGameServicesConfig(const idlepb::GameServicesConfig& Right);
    void FromPb(const idlepb::GameServicesConfig& Right);
    void ToPb(idlepb::GameServicesConfig* Out) const;
    void Reset();
    void operator=(const idlepb::GameServicesConfig& Right);
    bool operator==(const FPbGameServicesConfig& Right) const;
    bool operator!=(const FPbGameServicesConfig& Right) const;
     
};

namespace idlepb {
class GameClientConfig;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbGameClientConfig
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString server_ip;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 server_port;


    FPbGameClientConfig();
    FPbGameClientConfig(const idlepb::GameClientConfig& Right);
    void FromPb(const idlepb::GameClientConfig& Right);
    void ToPb(idlepb::GameClientConfig* Out) const;
    void Reset();
    void operator=(const idlepb::GameClientConfig& Right);
    bool operator==(const FPbGameClientConfig& Right) const;
    bool operator!=(const FPbGameClientConfig& Right) const;
     
};
