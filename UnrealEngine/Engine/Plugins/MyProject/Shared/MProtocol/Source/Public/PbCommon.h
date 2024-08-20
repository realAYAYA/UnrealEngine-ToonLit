#pragma once
#include "ZFmt.h"
#include "PbDefines.h"
#include "PbNet.h"
#include "PbCommon.generated.h"


namespace idlepb {
class Int64Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInt64Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 v1;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 v2;


    FPbInt64Pair();
    FPbInt64Pair(const idlepb::Int64Pair& Right);
    void FromPb(const idlepb::Int64Pair& Right);
    void ToPb(idlepb::Int64Pair* Out) const;
    void Reset();
    void operator=(const idlepb::Int64Pair& Right);
    bool operator==(const FPbInt64Pair& Right) const;
    bool operator!=(const FPbInt64Pair& Right) const;
     
};

namespace idlepb {
class Int32Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInt32Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 v1;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 v2;


    FPbInt32Pair();
    FPbInt32Pair(const idlepb::Int32Pair& Right);
    void FromPb(const idlepb::Int32Pair& Right);
    void ToPb(idlepb::Int32Pair* Out) const;
    void Reset();
    void operator=(const idlepb::Int32Pair& Right);
    bool operator==(const FPbInt32Pair& Right) const;
    bool operator!=(const FPbInt32Pair& Right) const;
     
};

namespace idlepb {
class StringInt32Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbStringInt32Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString str;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 value;


    FPbStringInt32Pair();
    FPbStringInt32Pair(const idlepb::StringInt32Pair& Right);
    void FromPb(const idlepb::StringInt32Pair& Right);
    void ToPb(idlepb::StringInt32Pair* Out) const;
    void Reset();
    void operator=(const idlepb::StringInt32Pair& Right);
    bool operator==(const FPbStringInt32Pair& Right) const;
    bool operator!=(const FPbStringInt32Pair& Right) const;
     
};

namespace idlepb {
class Int32Int64Pair;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbInt32Int64Pair
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int32 v32;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 v64;


    FPbInt32Int64Pair();
    FPbInt32Int64Pair(const idlepb::Int32Int64Pair& Right);
    void FromPb(const idlepb::Int32Int64Pair& Right);
    void ToPb(idlepb::Int32Int64Pair* Out) const;
    void Reset();
    void operator=(const idlepb::Int32Int64Pair& Right);
    bool operator==(const FPbInt32Int64Pair& Right) const;
    bool operator!=(const FPbInt32Int64Pair& Right) const;
     
};


/**
*/
UENUM(BlueprintType)
enum class EPbReplicatedLevelType : uint8
{
    RLT_Local = 0 UMETA(DisplayName="自己所在客户端"),
    RLT_Offical = 1 UMETA(DisplayName="官服"),
    RLT_Private = 2 UMETA(DisplayName="私服"),
};
constexpr EPbReplicatedLevelType EPbReplicatedLevelType_Min = EPbReplicatedLevelType::RLT_Local;
constexpr EPbReplicatedLevelType EPbReplicatedLevelType_Max = EPbReplicatedLevelType::RLT_Private;
constexpr int32 EPbReplicatedLevelType_ArraySize = static_cast<int32>(EPbReplicatedLevelType_Max) + 1;
MPROTOCOL_API bool CheckEPbReplicatedLevelTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbReplicatedLevelTypeDescription(EPbReplicatedLevelType Val);

template <typename Char>
struct fmt::formatter<EPbReplicatedLevelType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbReplicatedLevelType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 系统信息显示样式
*/
UENUM(BlueprintType)
enum class EPbSystemNoticeStyle : uint8
{
    SystemNoticeStyle_None = 0 UMETA(DisplayName="未知"),
    SystemNoticeStyle_Dialog = 1 UMETA(DisplayName="弹框提示"),
    SystemNoticeStyle_ScreenCenter = 2 UMETA(DisplayName="屏幕中央提示"),
};
constexpr EPbSystemNoticeStyle EPbSystemNoticeStyle_Min = EPbSystemNoticeStyle::SystemNoticeStyle_None;
constexpr EPbSystemNoticeStyle EPbSystemNoticeStyle_Max = EPbSystemNoticeStyle::SystemNoticeStyle_ScreenCenter;
constexpr int32 EPbSystemNoticeStyle_ArraySize = static_cast<int32>(EPbSystemNoticeStyle_Max) + 1;
MPROTOCOL_API bool CheckEPbSystemNoticeStyleValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSystemNoticeStyleDescription(EPbSystemNoticeStyle Val);

template <typename Char>
struct fmt::formatter<EPbSystemNoticeStyle, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSystemNoticeStyle& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
*/
UENUM(BlueprintType)
enum class EPbSystemNoticeType : uint8
{
    SNT = 0 UMETA(DisplayName="未知"),
    SNT_AddItem = 7 UMETA(DisplayName="添加道具"),
};
constexpr EPbSystemNoticeType EPbSystemNoticeType_Min = EPbSystemNoticeType::SNT;
constexpr EPbSystemNoticeType EPbSystemNoticeType_Max = EPbSystemNoticeType::SNT_AddItem;
constexpr int32 EPbSystemNoticeType_ArraySize = static_cast<int32>(EPbSystemNoticeType_Max) + 1;
MPROTOCOL_API bool CheckEPbSystemNoticeTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbSystemNoticeTypeDescription(EPbSystemNoticeType Val);

template <typename Char>
struct fmt::formatter<EPbSystemNoticeType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbSystemNoticeType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};

namespace idlepb {
class PlayerData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 player_id;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FString player_name;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 last_online_date;

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    int64 create_date;


    FPbPlayerData();
    FPbPlayerData(const idlepb::PlayerData& Right);
    void FromPb(const idlepb::PlayerData& Right);
    void ToPb(idlepb::PlayerData* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerData& Right);
    bool operator==(const FPbPlayerData& Right) const;
    bool operator!=(const FPbPlayerData& Right) const;
     
};

namespace idlepb {
class PlayerSaveData;
}  // namespace idlepb

/**
*/
USTRUCT(BlueprintType)
struct MPROTOCOL_API FPbPlayerSaveData
{
    GENERATED_BODY();

    /**  */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MProtocol")
    FPbPlayerData player_data;


    FPbPlayerSaveData();
    FPbPlayerSaveData(const idlepb::PlayerSaveData& Right);
    void FromPb(const idlepb::PlayerSaveData& Right);
    void ToPb(idlepb::PlayerSaveData* Out) const;
    void Reset();
    void operator=(const idlepb::PlayerSaveData& Right);
    bool operator==(const FPbPlayerSaveData& Right) const;
    bool operator!=(const FPbPlayerSaveData& Right) const;
     
};
