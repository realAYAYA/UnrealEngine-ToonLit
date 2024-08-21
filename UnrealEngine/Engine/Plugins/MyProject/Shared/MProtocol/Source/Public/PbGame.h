#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbCommon.h"
#include "PbGame.generated.h"


namespace idlepb {
class Ping;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPing
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 req_ticks;


    FPbPing();
    FPbPing(const idlepb::Ping& Right);
    void FromPb(const idlepb::Ping& Right);
    void ToPb(idlepb::Ping* Out) const;
    void Reset();
    void operator=(const idlepb::Ping& Right);
    bool operator==(const FPbPing& Right) const;
    bool operator!=(const FPbPing& Right) const;
     
};

namespace idlepb {
class Pong;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPong
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 req_ticks;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 rsp_ticks;


    FPbPong();
    FPbPong(const idlepb::Pong& Right);
    void FromPb(const idlepb::Pong& Right);
    void ToPb(idlepb::Pong* Out) const;
    void Reset();
    void operator=(const idlepb::Pong& Right);
    bool operator==(const FPbPong& Right) const;
    bool operator!=(const FPbPong& Right) const;
     
};

namespace idlepb {
class DoGmCommand;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbDoGmCommand
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString command;


    FPbDoGmCommand();
    FPbDoGmCommand(const idlepb::DoGmCommand& Right);
    void FromPb(const idlepb::DoGmCommand& Right);
    void ToPb(idlepb::DoGmCommand* Out) const;
    void Reset();
    void operator=(const idlepb::DoGmCommand& Right);
    bool operator==(const FPbDoGmCommand& Right) const;
    bool operator!=(const FPbDoGmCommand& Right) const;
     
};

namespace idlepb {
class ReportError;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbReportError
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString text;


    FPbReportError();
    FPbReportError(const idlepb::ReportError& Right);
    void FromPb(const idlepb::ReportError& Right);
    void ToPb(idlepb::ReportError* Out) const;
    void Reset();
    void operator=(const idlepb::ReportError& Right);
    bool operator==(const FPbReportError& Right) const;
    bool operator!=(const FPbReportError& Right) const;
     
};


/**
*/
UENUM(BlueprintType)
enum class EPbLoginGameRetCode : uint8
{
    LoginGameRetCode_Ok = 0 UMETA(DisplayName="正常登陆"),
    LoginGameRetCode_Unknown = 1 UMETA(DisplayName="未知错误"),
    LoginGameRetCode_NoRole = 2 UMETA(DisplayName="没有角色"),
    LoginGameRetCode_DuplicateLogin = 3 UMETA(DisplayName="已经在线"),
    LoginGameRetCode_AccountInvalid = 4 UMETA(DisplayName="帐号非法"),
    LoginGameRetCode_VersionError = 5 UMETA(DisplayName="版本错误"),
};
constexpr EPbLoginGameRetCode EPbLoginGameRetCode_Min = EPbLoginGameRetCode::LoginGameRetCode_Ok;
constexpr EPbLoginGameRetCode EPbLoginGameRetCode_Max = EPbLoginGameRetCode::LoginGameRetCode_VersionError;
constexpr int32 EPbLoginGameRetCode_ArraySize = static_cast<int32>(EPbLoginGameRetCode_Max) + 1;
MPROTOCOL_API bool CheckEPbLoginGameRetCodeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbLoginGameRetCodeDescription(EPbLoginGameRetCode Val);

template <typename Char>
struct fmt::formatter<EPbLoginGameRetCode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbLoginGameRetCode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class LoginGameReq;
}  // namespace idlepb

/**
 * 登录游戏
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginGameReq
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString account;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString client_version;


    FPbLoginGameReq();
    FPbLoginGameReq(const idlepb::LoginGameReq& Right);
    void FromPb(const idlepb::LoginGameReq& Right);
    void ToPb(idlepb::LoginGameReq* Out) const;
    void Reset();
    void operator=(const idlepb::LoginGameReq& Right);
    bool operator==(const FPbLoginGameReq& Right) const;
    bool operator!=(const FPbLoginGameReq& Right) const;
     
};

namespace idlepb {
class LoginGameAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbLoginGameAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbLoginGameRetCode ret;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerData player_data;

    /** 是否为重连 (即服务器上玩家对象已经存在) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool is_relogin;


    FPbLoginGameAck();
    FPbLoginGameAck(const idlepb::LoginGameAck& Right);
    void FromPb(const idlepb::LoginGameAck& Right);
    void ToPb(idlepb::LoginGameAck* Out) const;
    void Reset();
    void operator=(const idlepb::LoginGameAck& Right);
    bool operator==(const FPbLoginGameAck& Right) const;
    bool operator!=(const FPbLoginGameAck& Right) const;
     
};

namespace idlepb {
class EnterLevelReq;
}  // namespace idlepb

/**
 * 进入
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEnterLevelReq
{
    GENERATED_BODY();


    FPbEnterLevelReq();
    FPbEnterLevelReq(const idlepb::EnterLevelReq& Right);
    void FromPb(const idlepb::EnterLevelReq& Right);
    void ToPb(idlepb::EnterLevelReq* Out) const;
    void Reset();
    void operator=(const idlepb::EnterLevelReq& Right);
    bool operator==(const FPbEnterLevelReq& Right) const;
    bool operator!=(const FPbEnterLevelReq& Right) const;
     
};

namespace idlepb {
class EnterLevelAck;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbEnterLevelAck
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    bool ok;


    FPbEnterLevelAck();
    FPbEnterLevelAck(const idlepb::EnterLevelAck& Right);
    void FromPb(const idlepb::EnterLevelAck& Right);
    void ToPb(idlepb::EnterLevelAck* Out) const;
    void Reset();
    void operator=(const idlepb::EnterLevelAck& Right);
    bool operator==(const FPbEnterLevelAck& Right) const;
    bool operator!=(const FPbEnterLevelAck& Right) const;
     
};
