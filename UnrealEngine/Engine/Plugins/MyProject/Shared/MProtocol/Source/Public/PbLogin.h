#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbCommon.h"
#include "PbLogin.generated.h"


namespace idlepb {
class LoginAccountReq;
}  // namespace idlepb

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
    FPbLoginAccountReq(const idlepb::LoginAccountReq& Right);
    void FromPb(const idlepb::LoginAccountReq& Right);
    void ToPb(idlepb::LoginAccountReq* Out) const;
    void Reset();
    void operator=(const idlepb::LoginAccountReq& Right);
    bool operator==(const FPbLoginAccountReq& Right) const;
    bool operator!=(const FPbLoginAccountReq& Right) const;
     
};

namespace idlepb {
class LoginAccountAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginAccountAck
{
    GENERATED_BODY();


    FPbLoginAccountAck();
    FPbLoginAccountAck(const idlepb::LoginAccountAck& Right);
    void FromPb(const idlepb::LoginAccountAck& Right);
    void ToPb(idlepb::LoginAccountAck* Out) const;
    void Reset();
    void operator=(const idlepb::LoginAccountAck& Right);
    bool operator==(const FPbLoginAccountAck& Right) const;
    bool operator!=(const FPbLoginAccountAck& Right) const;
     
};
