#pragma once
#include "ZFmt.h"
#include "PbNet.generated.h"



/**
*/
UENUM(BlueprintType)
enum class EPbRpcMessageOp : uint8
{
    RpcMessageOp_Notify = 0 UMETA(DisplayName="通知"),
    RpcMessageOp_Request = 1 UMETA(DisplayName="请求"),
    RpcMessageOp_Response = 2 UMETA(DisplayName="回应"),
};
constexpr EPbRpcMessageOp EPbRpcMessageOp_Min = EPbRpcMessageOp::RpcMessageOp_Notify;
constexpr EPbRpcMessageOp EPbRpcMessageOp_Max = EPbRpcMessageOp::RpcMessageOp_Response;
constexpr int32 EPbRpcMessageOp_ArraySize = static_cast<int32>(EPbRpcMessageOp_Max) + 1;
MPROTOCOL_API bool CheckEPbRpcMessageOpValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRpcMessageOpDescription(EPbRpcMessageOp Val);

template <typename Char>
struct fmt::formatter<EPbRpcMessageOp, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRpcMessageOp& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbRpcErrorCode : uint8
{
    RpcErrorCode_Ok = 0 UMETA(DisplayName="正常"),
    RpcErrorCode_Unknown = 1 UMETA(DisplayName="未知错误"),
    RpcErrorCode_Unimplemented = 2 UMETA(DisplayName="接口未实现"),
    RpcErrorCode_Timeout = 3 UMETA(DisplayName="调用超时"),
    RpcErrorCode_ReqDataError = 4 UMETA(DisplayName="请求数据错误"),
    RpcErrorCode_RspDataError = 5 UMETA(DisplayName="返回数据错误"),
};
constexpr EPbRpcErrorCode EPbRpcErrorCode_Min = EPbRpcErrorCode::RpcErrorCode_Ok;
constexpr EPbRpcErrorCode EPbRpcErrorCode_Max = EPbRpcErrorCode::RpcErrorCode_RspDataError;
constexpr int32 EPbRpcErrorCode_ArraySize = static_cast<int32>(EPbRpcErrorCode_Max) + 1;
MPROTOCOL_API bool CheckEPbRpcErrorCodeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbRpcErrorCodeDescription(EPbRpcErrorCode Val);

template <typename Char>
struct fmt::formatter<EPbRpcErrorCode, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbRpcErrorCode& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class PbRpcMessage;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPbRpcMessage
{
    GENERATED_BODY();

    /** RPC消息类型 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRpcMessageOp op;

    /** 序号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 sn;

    /** 错误码 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    EPbRpcErrorCode error_code;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 rpc_id;

    /** body 的 message type id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 body_type_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    TArray<uint8> body_data;


    FPbPbRpcMessage();
    FPbPbRpcMessage(const idlepb::PbRpcMessage& Right);
    void FromPb(const idlepb::PbRpcMessage& Right);
    void ToPb(idlepb::PbRpcMessage* Out) const;
    void Reset();
    void operator=(const idlepb::PbRpcMessage& Right);
    bool operator==(const FPbPbRpcMessage& Right) const;
    bool operator!=(const FPbPbRpcMessage& Right) const;
     
};
