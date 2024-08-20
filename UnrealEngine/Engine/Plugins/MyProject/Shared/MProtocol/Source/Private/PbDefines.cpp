#include "PbDefines.h"
#include "defines.pb.h"



bool CheckEPbItemQualityValid(int32 Val)
{
    return idlepb::ItemQuality_IsValid(Val);
}

const TCHAR* GetEPbItemQualityDescription(EPbItemQuality Val)
{
    switch (Val)
    {
        case EPbItemQuality::EQ_None: return TEXT("其他");
        case EPbItemQuality::EQ_White: return TEXT("白");
        case EPbItemQuality::EQ_Green: return TEXT("绿");
        case EPbItemQuality::EQ_Blue: return TEXT("蓝");
        case EPbItemQuality::EQ_Purple: return TEXT("紫");
        case EPbItemQuality::EQ_Orange: return TEXT("橙");
        case EPbItemQuality::EQ_Red: return TEXT("红");
    }
    return TEXT("UNKNOWN");
}

bool CheckEPbPeriodTypeValid(int32 Val)
{
    return idlepb::PeriodType_IsValid(Val);
}

const TCHAR* GetEPbPeriodTypeDescription(EPbPeriodType Val)
{
    switch (Val)
    {
        case EPbPeriodType::PT_None: return TEXT("未知");
        case EPbPeriodType::PT_Day: return TEXT("日");
        case EPbPeriodType::PT_Week: return TEXT("周");
        case EPbPeriodType::PT_Mouth: return TEXT("月");
        case EPbPeriodType::PT_Year: return TEXT("年");
    }
    return TEXT("UNKNOWN");
}