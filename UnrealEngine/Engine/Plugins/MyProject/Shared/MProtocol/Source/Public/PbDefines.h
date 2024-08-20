#pragma once
#include "ZFmt.h"
#include "PbDefines.generated.h"



/**
 * 道具品质
*/
UENUM(BlueprintType)
enum class EPbItemQuality : uint8
{
    EQ_None = 0 UMETA(DisplayName="其他"),
    EQ_White = 1 UMETA(DisplayName="白"),
    EQ_Green = 2 UMETA(DisplayName="绿"),
    EQ_Blue = 3 UMETA(DisplayName="蓝"),
    EQ_Purple = 4 UMETA(DisplayName="紫"),
    EQ_Orange = 5 UMETA(DisplayName="橙"),
    EQ_Red = 6 UMETA(DisplayName="红"),
};
constexpr EPbItemQuality EPbItemQuality_Min = EPbItemQuality::EQ_None;
constexpr EPbItemQuality EPbItemQuality_Max = EPbItemQuality::EQ_Red;
constexpr int32 EPbItemQuality_ArraySize = static_cast<int32>(EPbItemQuality_Max) + 1;
MPROTOCOL_API bool CheckEPbItemQualityValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbItemQualityDescription(EPbItemQuality Val);

template <typename Char>
struct fmt::formatter<EPbItemQuality, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbItemQuality& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};


/**
 * 刷新周期类型
*/
UENUM(BlueprintType)
enum class EPbPeriodType : uint8
{
    PT_None = 0 UMETA(DisplayName="未知"),
    PT_Day = 1 UMETA(DisplayName="日"),
    PT_Week = 2 UMETA(DisplayName="周"),
    PT_Mouth = 3 UMETA(DisplayName="月"),
    PT_Year = 4 UMETA(DisplayName="年"),
};
constexpr EPbPeriodType EPbPeriodType_Min = EPbPeriodType::PT_None;
constexpr EPbPeriodType EPbPeriodType_Max = EPbPeriodType::PT_Year;
constexpr int32 EPbPeriodType_ArraySize = static_cast<int32>(EPbPeriodType_Max) + 1;
MPROTOCOL_API bool CheckEPbPeriodTypeValid(int32 Val);
MPROTOCOL_API const TCHAR* GetEPbPeriodTypeDescription(EPbPeriodType Val);

template <typename Char>
struct fmt::formatter<EPbPeriodType, Char>
{
    constexpr auto parse(ZU16FormatParseContext& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }
    template <typename FormatContext>
    auto format(const EPbPeriodType& V, FormatContext& ctx) const -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), TEXT("{}"), static_cast<int32>(V));
    }
};
