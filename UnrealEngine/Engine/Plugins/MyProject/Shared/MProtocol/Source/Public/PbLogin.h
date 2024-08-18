#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbCommon.h"
#include "PbLogin.generated.h"


namespace idlezt {
class LoginAccountReq;
}  // namespace idlezt

/**
 * 登录帐号
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginAccountReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString account;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString client_version;


    FPbLoginAccountReq();
    FPbLoginAccountReq(const idlezt::LoginAccountReq& Right);
    void FromPb(const idlezt::LoginAccountReq& Right);
    void ToPb(idlezt::LoginAccountReq* Out) const;
    void Reset();
    void operator=(const idlezt::LoginAccountReq& Right);
    bool operator==(const FPbLoginAccountReq& Right) const;
    bool operator!=(const FPbLoginAccountReq& Right) const;
     
};

namespace idlezt {
class LoginAccountAck;
}  // namespace idlezt

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginAccountAck
{
    GENERATED_BODY();


    FPbLoginAccountAck();
    FPbLoginAccountAck(const idlezt::LoginAccountAck& Right);
    void FromPb(const idlezt::LoginAccountAck& Right);
    void ToPb(idlezt::LoginAccountAck* Out) const;
    void Reset();
    void operator=(const idlezt::LoginAccountAck& Right);
    bool operator==(const FPbLoginAccountAck& Right) const;
    bool operator!=(const FPbLoginAccountAck& Right) const;
     
};
