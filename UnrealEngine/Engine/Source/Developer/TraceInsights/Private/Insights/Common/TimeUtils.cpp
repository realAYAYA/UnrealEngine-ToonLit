// Copyright Epic Games, Inc. All Rights Reserved.

#include "Insights/Common/TimeUtils.h"
#include "Misc/StringBuilder.h"

#include <cmath>
#include <limits>

//#include "Insights/Log.h"

namespace TimeUtils
{

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeValue(const double Duration, const int32 NumDigits)
{
	if (std::isnan(Duration))
	{
		return TEXT("NaN");
	}

	if (NumDigits <= 0)
	{
		return FString::Printf(TEXT("%.0f"), Duration);
	}

#if !PLATFORM_USE_GENERIC_STRING_IMPLEMENTATION
	FString Str = FString::Printf(TEXT("%.*f"), NumDigits, Duration);
#else
	// proper resolution is tracked as UE-79534
	TCHAR FormatString[32];
	FCString::Snprintf(FormatString, sizeof(FormatString), TEXT("%%.%df"), NumDigits);
	FString Str = FString::Printf(FormatString, Duration);
#endif

	int32 Index = Str.Len() - 1;
	while (Index > 0 && Str[Index] == '0') --Index;
	while (Index > 0 && Str[Index] == '.') --Index;
	++Index;
	if (Index < Str.Len())
	{
		Str.LeftInline(Index, EAllowShrinking::No);
	}

	return Str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeAuto(const double InDuration, const int32 NumDigits)
{
	//TestTimeAutoFormatting();

	double Duration = InDuration;

	if (std::isnan(Duration))
	{
		return TEXT("NaN");
	}

	if (Duration == 0.0)
	{
		return TEXT("0");
	}

	TStringBuilder<64> StrBuilder;

	if (Duration < 0.0)
	{
		Duration = -Duration;
		StrBuilder.AppendChar(TEXT('-'));
	}

	if (Duration == DBL_MAX || Duration == std::numeric_limits<double>::infinity())
	{
		StrBuilder.AppendChar(TEXT('∞'));
	}
	else if (Duration < TimeUtils::Picosecond)
	{
		// (0 .. 1ps)
		return TEXT("~0");
	}
	else if (Duration < TimeUtils::Nanosecond)
	{
		// [1ps .. 1ns)
		if (Duration >= 999.5 * TimeUtils::Picosecond)
		{
			StrBuilder.Append(TEXT("1 ns"));
		}
		else
		{
			const double Picoseconds = Duration * 1000000000000.0 + 0.5;
			const int32 IntPicoseconds = static_cast<int32>(Picoseconds);
			ensure(IntPicoseconds <= 999);
			//if (IntPicoseconds > 999)
			//{
			//	StrBuilder.Append(TEXT("1 ns"));
			//}
			//else
			{
				StrBuilder.Appendf(TEXT("%d ps"), IntPicoseconds);
			}
		}
	}
	else if (Duration < TimeUtils::Microsecond)
	{
		// [1ns .. 1µs)
		if (Duration >= 999.5 * TimeUtils::Nanosecond)
		{
			StrBuilder.Append(TEXT("1 µs"));
		}
		else
		{
			const double Nanoseconds = Duration * 1000000000.0 + 0.5;
			const int32 IntNanoseconds = static_cast<int32>(Nanoseconds);
			ensure(IntNanoseconds <= 999);
			//if (IntNanoseconds > 999)
			//{
			//	StrBuilder.Append(TEXT("1 µs"));
			//}
			//else
			{
				StrBuilder.Appendf(TEXT("%d ns"), IntNanoseconds);
			}
		}
	}
	else if (Duration < TimeUtils::Milisecond)
	{
		// [1µs .. 1ms)
		const double Microseconds = Duration * 1000000.0;
		if (Microseconds >= 999.95)
		{
			StrBuilder.Append(TEXT("1 ms"));
		}
		else
		{
			StrBuilder.Append(FormatTimeValue(Microseconds, NumDigits));
			StrBuilder.Append(TEXT(" µs"));
		}
	}
	else if (Duration < TimeUtils::Second)
	{
		// [1ms .. 1s)
		const double Miliseconds = Duration * 1000.0;
		if (Miliseconds >= 999.95)
		{
			StrBuilder.Append(TEXT("1s"));
		}
		else
		{
			StrBuilder.Append(FormatTimeValue(Miliseconds, NumDigits));
			StrBuilder.Append(TEXT(" ms"));
		}
	}
	else if (Duration < TimeUtils::Minute)
	{
		// [1s .. 1m)
		if (Duration >= 59.95)
		{
			StrBuilder.Append(TEXT("1m"));
		}
		else
		{
			StrBuilder.Append(FormatTimeValue(Duration, NumDigits));
			StrBuilder.AppendChar(TEXT('s'));
		}
	}
	else if (Duration < TimeUtils::Hour)
	{
		// [1m .. 1h)
		const double Minutes = FMath::FloorToDouble(Duration / TimeUtils::Minute);
		StrBuilder.Appendf(TEXT("%dm"), static_cast<int32>(Minutes));
		Duration -= Minutes * TimeUtils::Minute;
		if (NumDigits <= 1)
		{
			const double Seconds = FMath::FloorToDouble(Duration / TimeUtils::Second);
			if (Seconds > 0.5)
			{
				StrBuilder.Appendf(TEXT(" %ds"), static_cast<int32>(Seconds));
			}
		}
		else
		{
			StrBuilder.AppendChar(TEXT(' '));
			StrBuilder.Append(FormatTimeValue(Duration, NumDigits - 1));
			StrBuilder.AppendChar(TEXT('s'));
		}
	}
	else if (Duration < TimeUtils::Day)
	{
		// [1h .. 1d)
		const double Hours = FMath::FloorToDouble(Duration / TimeUtils::Hour);
		StrBuilder.Appendf(TEXT("%dh"), static_cast<int32>(Hours));
		Duration -= Hours * TimeUtils::Hour;
		const double Minutes = FMath::FloorToDouble(Duration / TimeUtils::Minute);
		if (Minutes > 0.5)
		{
			StrBuilder.Appendf(TEXT(" %dm"), static_cast<int32>(Minutes));
		}
	}
	else
	{
		// [1d .. ∞)
		const double Days = FMath::FloorToDouble(Duration / TimeUtils::Day);
		StrBuilder.Appendf(TEXT("%dd"), static_cast<int32>(Days));
		Duration -= Days * TimeUtils::Day;
		const double Hours = FMath::FloorToDouble(Duration / TimeUtils::Hour);
		if (Hours > 0.5)
		{
			StrBuilder.Appendf(TEXT(" %dh"), static_cast<int32>(Hours));
		}
	}

	return FString(StrBuilder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeMs(const double InDuration, const int32 NumDigits, bool bAddTimeUnit)
{
	if (std::isnan(InDuration))
	{
		return TEXT("NaN");
	}

	if (FMath::IsNearlyZero(InDuration, TimeUtils::Picosecond))
	{
		return TEXT("0");
	}

	double Duration = InDuration;
	TStringBuilder<64> StrBuilder;

	if (Duration < 0.0)
	{
		Duration = -Duration;
		StrBuilder.AppendChar(TEXT('-'));
	}

	if (Duration == DBL_MAX || Duration == std::numeric_limits<double>::infinity())
	{
		StrBuilder.AppendChar(TEXT('∞'));
	}
	else
	{
		if (NumDigits <= 0)
		{
			StrBuilder.Appendf(TEXT("%.0f"), Duration * 1000.0);
		}
		else
		{
#if !PLATFORM_USE_GENERIC_STRING_IMPLEMENTATION
			StrBuilder.Appendf(TEXT("%.*f"), NumDigits, Duration * 1000.0);
#else
			// proper resolution is tracked as UE-79534
			TCHAR FormatString[32];
			FCString::Snprintf(FormatString, sizeof(FormatString), TEXT("%%.%df"), NumDigits);
			StrBuilder.Appendf(FormatString, Duration * 1000.0);
#endif
		}

		if (bAddTimeUnit)
		{
			StrBuilder.Append(TEXT(" ms"));
		}
	}

	return FString(StrBuilder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTime(const double InTime, const double Precision)
{
	if (std::isnan(InTime))
	{
		return TEXT("NaN");
	}

	if (FMath::IsNearlyZero(InTime, FMath::Max(TimeUtils::Picosecond, Precision / 10.0f)))
	{
		return TEXT("0");
	}

	double Time = InTime;
	TStringBuilder<64> StrBuilder;

	if (Time < 0.0)
	{
		Time = -Time;
		StrBuilder.AppendChar(TEXT('-'));
	}

	if (Time == DBL_MAX || Time == std::numeric_limits<double>::infinity())
	{
		StrBuilder.AppendChar(TEXT('∞'));
		return FString(StrBuilder);
	}

	bool bIsSpaceNeeded = false;

	int32 Days = static_cast<int32>(Time / TimeUtils::Day);
	if (Days > 0)
	{
		StrBuilder.Appendf(TEXT("%dd"), Days);
		bIsSpaceNeeded = true;
		Time -= static_cast<double>(Days) * TimeUtils::Day;
	}

	if (Precision >= TimeUtils::Day)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	int32 Hours = static_cast<int32>(Time / TimeUtils::Hour);
	if (Hours > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dh"), Hours);
		bIsSpaceNeeded = true;
		Time -= static_cast<double>(Hours) * TimeUtils::Hour;
	}

	if (Precision >= TimeUtils::Hour)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	int32 Minutes = static_cast<int32>(Time / TimeUtils::Minute);
	if (Minutes > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dm"), Minutes);
		bIsSpaceNeeded = true;
		Time -= static_cast<double>(Minutes) * TimeUtils::Minute;
	}

	if (Precision >= TimeUtils::Minute)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	//TestOptimizationIssue();

	////float Log10 = -FMath::LogX(10.0f, static_cast<float>(Precision));
	//double Log10 = -log10(Precision);
	//int32 Digits = (Log10 > 0) ? FMath::CeilToInt(Log10) : 0;

	static const double DigitThresholds[] =
	{
		TimeUtils::Second,              // 0 digits
		TimeUtils::Milisecond * 100,    // 1 digit
		TimeUtils::Milisecond * 10,     // 2 digits
		TimeUtils::Milisecond,          // 3 digits
		TimeUtils::Microsecond * 100,   // 4 digits
		TimeUtils::Microsecond * 10,    // 5 digits
		TimeUtils::Microsecond,         // 6 digits
		TimeUtils::Nanosecond * 100,    // 7 digits
		TimeUtils::Nanosecond * 10,     // 8 digits
		TimeUtils::Nanosecond,          // 9 digits
		TimeUtils::Picosecond * 100,    // 10 digits
		TimeUtils::Picosecond * 10,     // 11 digits
		TimeUtils::Picosecond,          // 12 digits
		0                               // 13 digits
	};
	int32 Digits = 0;
	while (Precision < DigitThresholds[Digits])
	{
		Digits++;
	}

	if (Digits == 0)
	{
		int32 Seconds = static_cast<int32>(Time / TimeUtils::Second + 0.5);
		if (Seconds > 0)
		{
			if (bIsSpaceNeeded)
			{
				StrBuilder.AppendChar(TEXT(' '));
			}
			StrBuilder.Appendf(TEXT("%ds"), Seconds);
		}
		else if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
	}
	//else if (Digits <= 9)
	//{
	//	if (bIsSpaceNeeded)
	//	{
	//		StrBuilder.AppendChar(TEXT(' '));
	//	}
	//	int32 Seconds = static_cast<int32>(Time / TimeUtils::Second);
	//	Time -= static_cast<double>(Seconds) * TimeUtils::Second;
	//	int64 SubSeconds = static_cast<int64>(Time * FMath::Pow(10.0f, Digits) + 0.5);
	//	StrBuilder.Appendf(TEXT("%d.%0*llds"), Seconds, Digits, SubSeconds);
	//}
	else
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
#if !PLATFORM_USE_GENERIC_STRING_IMPLEMENTATION
		StrBuilder.Appendf(TEXT("%.*fs"), Digits, Time);
#else
		// proper resolution is tracked as UE-79534
		TCHAR FormatString[32];
		FCString::Snprintf(FormatString, sizeof(FormatString), TEXT("%%.%dfs"), Digits);
		StrBuilder.Appendf(FormatString, Time);
#endif
	}

	return FString(StrBuilder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeHMS(const double Time, const double Precision)
{
	//TODO: DD:HH:MM:SS.mmm.uuu.nnn.ppp
	return FormatTime(Time, Precision);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SplitTime(const double InTime, FTimeSplit& OutTimeSplit)
{
	double Time = InTime;

	OutTimeSplit.bIsZero = false;
	OutTimeSplit.bIsNegative = false;
	OutTimeSplit.bIsInfinite = false;
	OutTimeSplit.bIsNaN = false;

	if (std::isnan(InTime))
	{
		OutTimeSplit.bIsNaN = true;
		return;
	}

	if (FMath::IsNearlyZero(Time, TimeUtils::Picosecond))
	{
		OutTimeSplit.bIsZero = true;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			OutTimeSplit.Units[Index] = 0;
		}
		return;
	}

	if (Time < 0.0)
	{
		Time = -Time;
		OutTimeSplit.bIsNegative = true;
	}

	if (Time == DBL_MAX || Time == std::numeric_limits<double>::infinity())
	{
		OutTimeSplit.bIsInfinite = true;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			OutTimeSplit.Units[Index] = 0;
		}
		return;
	}

	bool bIsZero = true; // assume true and see if any split unit is != 0

	const double Days = FMath::FloorToDouble(Time / TimeUtils::Day);
	OutTimeSplit.Days = static_cast<int32>(Days);
	if (OutTimeSplit.Days > 0)
	{
		Time -= Days * TimeUtils::Day;
		bIsZero = false;
	}

	const double Hours = FMath::FloorToDouble(Time / TimeUtils::Hour);
	OutTimeSplit.Hours = static_cast<int32>(Hours);
	if (OutTimeSplit.Hours > 0)
	{
		Time -= Hours * TimeUtils::Hour;
	}

	const double Minutes = FMath::FloorToDouble(Time / TimeUtils::Minute);
	OutTimeSplit.Minutes = static_cast<int32>(Minutes);
	if (OutTimeSplit.Minutes > 0)
	{
		Time -= Minutes * TimeUtils::Minute;
		bIsZero = false;
	}

	const double Seconds = FMath::FloorToDouble(Time / TimeUtils::Second);
	OutTimeSplit.Seconds = static_cast<int32>(Seconds);
	if (OutTimeSplit.Seconds > 0)
	{
		Time -= Seconds * TimeUtils::Second;
		bIsZero = false;
	}

	const double Miliseconds = FMath::FloorToDouble(Time / TimeUtils::Milisecond);
	OutTimeSplit.Miliseconds = static_cast<int32>(Miliseconds);
	if (OutTimeSplit.Miliseconds > 0)
	{
		Time -= Miliseconds * TimeUtils::Milisecond;
		bIsZero = false;
	}

	const double Microseconds = FMath::FloorToDouble(Time / TimeUtils::Microsecond);
	OutTimeSplit.Microseconds = static_cast<int32>(Microseconds);
	if (OutTimeSplit.Microseconds > 0)
	{
		Time -= Microseconds * TimeUtils::Microsecond;
		bIsZero = false;
	}

	const double Nanoseconds = FMath::FloorToDouble(Time / TimeUtils::Nanosecond);
	OutTimeSplit.Nanoseconds = static_cast<int32>(Nanoseconds);
	if (OutTimeSplit.Nanoseconds > 0)
	{
		Time -= Nanoseconds * TimeUtils::Nanosecond;
		bIsZero = false;
	}

	const double Picoseconds = FMath::FloorToDouble(Time / TimeUtils::Picosecond);
	OutTimeSplit.Picoseconds = static_cast<int32>(Picoseconds);
	if (OutTimeSplit.Picoseconds > 0)
	{
		Time -= Picoseconds * TimeUtils::Picosecond;
		bIsZero = false;
	}

	OutTimeSplit.bIsZero = bIsZero;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeSplit(const FTimeSplit& InTimeSplit, const double Precision)
{
	if (InTimeSplit.bIsNaN)
	{
		return TEXT("NaN");
	}

	if (InTimeSplit.bIsZero)
	{
		return TEXT("0");
	}

	if (InTimeSplit.bIsInfinite)
	{
		return InTimeSplit.bIsNegative ? TEXT("-∞") : TEXT("∞");
	}

	TStringBuilder<64> StrBuilder;

	if (InTimeSplit.bIsNegative)
	{
		StrBuilder.AppendChar(TEXT('-'));
	}

	bool bIsSpaceNeeded = false;

	if (InTimeSplit.Days > 0)
	{
		StrBuilder.Appendf(TEXT("%dd"), InTimeSplit.Days);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Day)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Hours > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dh"), InTimeSplit.Hours);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Hour)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Minutes > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dm"), InTimeSplit.Minutes);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Minute)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Seconds > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%ds"), InTimeSplit.Seconds);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Second)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Miliseconds > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dms"), InTimeSplit.Miliseconds);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Milisecond)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Microseconds > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dµs"), InTimeSplit.Microseconds);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Microsecond)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Nanoseconds > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dns"), InTimeSplit.Nanoseconds);
		bIsSpaceNeeded = true;
	}

	if (Precision >= TimeUtils::Nanosecond)
	{
		if (!bIsSpaceNeeded)
		{
			return TEXT("~0");
		}
		return FString(StrBuilder);
	}

	if (InTimeSplit.Picoseconds > 0)
	{
		if (bIsSpaceNeeded)
		{
			StrBuilder.AppendChar(TEXT(' '));
		}
		StrBuilder.Appendf(TEXT("%dps"), InTimeSplit.Picoseconds);
		bIsSpaceNeeded = true;
	}

	if (!bIsSpaceNeeded)
	{
		return TEXT("~0");
	}

	return FString(StrBuilder);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FString FormatTimeSplit(const double Time, const double Precision)
{
	FTimeSplit TimeSplit;
	SplitTime(Time, TimeSplit);
	return FormatTimeSplit(TimeSplit, Precision);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void TestTimeFormatting()
{
	double T1 = 1 * TimeUtils::Day + 2 * TimeUtils::Hour + 3 * TimeUtils::Minute + 4.567890123456789;
	FString S1 = FormatTime(T1, TimeUtils::Day);
	//UE_LOG(TraceInsights, Log, TEXT("D-T: %s"), *S1);
	FString S2 = FormatTime(T1, TimeUtils::Hour);
	//UE_LOG(TraceInsights, Log, TEXT("H-T: %s"), *S2);
	FString S3 = FormatTime(T1, TimeUtils::Minute);
	//UE_LOG(TraceInsights, Log, TEXT("M-T: %s"), *S3);
	for (double P = 10.0; P >= TimeUtils::Picosecond; P /= 10.0)
	{
		FString SP = FormatTime(T1, P);
		//UE_LOG(TraceInsights, Log, TEXT("P:%g T: %s"), P, *SP);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void TestTimeAutoFormatting()
{
	static bool Once = false;
	if (Once) return;
	Once = true;

	struct FTestTimeAutoData
	{
		double T;
		const TCHAR* Msg;
	};

	static const FTestTimeAutoData Data[] =
	{
		{ TimeUtils::Minute,                TEXT("1m") },
		{ TimeUtils::Second * 59.99,        TEXT("59.99s") },
		{ TimeUtils::Second * 59.95,        TEXT("59.95s") },
		{ 0, TEXT("[threshold 1m / 59.9s]") },
		{ TimeUtils::Second * 59.94,        TEXT("59.94s") },
		{ TimeUtils::Second * 59.9,         TEXT("59.9s") },
		{ TimeUtils::Second * 10,           TEXT("10s") },
		{ TimeUtils::Second,                TEXT("1s") },
		{ TimeUtils::Milisecond * 999.99,   TEXT("999.99ms") },
		{ TimeUtils::Milisecond * 999.95,   TEXT("999.95ms") },
		{ 0, TEXT("[threshold 1s / 999.9 ms]") },
		{ TimeUtils::Milisecond * 999.94,   TEXT("999.94ms") },
		{ TimeUtils::Milisecond * 999.9,    TEXT("999.9ms") },
		{ TimeUtils::Milisecond * 999,      TEXT("999ms") },
		{ TimeUtils::Milisecond * 100,      TEXT("100ms") },
		{ TimeUtils::Milisecond * 10,       TEXT("10ms") },
		{ TimeUtils::Milisecond * 1.55,     TEXT("1.55ms") },
		{ TimeUtils::Milisecond * 1.5,      TEXT("1.5ms") },
		{ TimeUtils::Milisecond * 1.05,     TEXT("1.05ms") },
		{ TimeUtils::Milisecond,            TEXT("1ms") },
		{ TimeUtils::Microsecond * 999.99,  TEXT("999.99µs") },
		{ TimeUtils::Microsecond * 999.95,  TEXT("999.95µs") },
		{ 0, TEXT("[threshold 1 ms / 999.9 µs]") },
		{ TimeUtils::Microsecond * 999.94,  TEXT("999.94µs") },
		{ TimeUtils::Microsecond * 999.9,   TEXT("999.9µs") },
		{ TimeUtils::Microsecond * 999,     TEXT("999µs") },
		{ TimeUtils::Microsecond * 100,     TEXT("100µs") },
		{ TimeUtils::Microsecond * 10,      TEXT("10µs") },
		{ TimeUtils::Microsecond,           TEXT("1µs") },
		{ TimeUtils::Nanosecond * 999.9,    TEXT("999.9ns") },
		{ TimeUtils::Nanosecond * 999.5,    TEXT("999.5ns") },
		{ 0, TEXT("[threshold 1 µs / 999 ns]") },
		{ TimeUtils::Nanosecond * 999.4,    TEXT("999.4ns") },
		{ TimeUtils::Nanosecond * 999,      TEXT("999ns") },
		{ TimeUtils::Nanosecond * 100,      TEXT("100ns") },
		{ TimeUtils::Nanosecond * 10,       TEXT("10ns") },
		{ TimeUtils::Nanosecond,            TEXT("1ns") },
		{ TimeUtils::Picosecond * 999.9,    TEXT("999.9ps") },
		{ TimeUtils::Picosecond * 999.5,    TEXT("999.5ps") },
		{ 0, TEXT("[threshold 1 ns / 999 ps]") },
		{ TimeUtils::Picosecond * 999.4,    TEXT("999.4ps") },
		{ TimeUtils::Picosecond * 999,      TEXT("999ps") },
		{ TimeUtils::Picosecond * 100,      TEXT("100ps") },
		{ TimeUtils::Picosecond * 10,       TEXT("10ps") },
		{ TimeUtils::Picosecond,            TEXT("1ps") },
		{ TimeUtils::Picosecond * 0.1,      TEXT("0.1ps") },
	};
	const int32 DataCount = sizeof(Data) / sizeof(FTestTimeAutoData);

	for (int32 Index = 0; Index < DataCount; ++Index)
	{
		FString Str = FormatTimeAuto(Data[Index].T);
		//UE_LOG(TraceInsights, Log, TEXT("%s : %s"), Data[Index].Msg, *Str);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

int32 GetNumDigits(const double Precision)
{
	//float Log10 = -log10(Precision);
	float Log10 = -FMath::LogX(10.0f, static_cast<float>(Precision));
	int32 D = (Log10 > 0) ? FMath::CeilToInt(Log10) : 0;
	return D;
}

UE_DISABLE_OPTIMIZATION_SHIP
int32 GetNumDigitsOptDisabled(const double Precision)
{
	//double Log10 = -log10(Precision);
	float Log10 = -FMath::LogX(10.0f, static_cast<float>(Precision));
	int32 D = (Log10 > 0) ? FMath::CeilToInt(Log10) : 0;
	return D;
}
UE_ENABLE_OPTIMIZATION_SHIP

UE_DISABLE_OPTIMIZATION_SHIP
void TestOptimizationIssue()
{
	constexpr double Ns = 0.000000001;
	int32 D1 = GetNumDigits(Ns);
	int32 D2 = GetNumDigitsOptDisabled(Ns);
	//UE_LOG(TraceInsights, Log, TEXT("D1 = %d, D2 = %d"), D1, D2);
	ensure(D1 == 9); // 10 ?
	ensure(D1 == D2);
}
UE_ENABLE_OPTIMIZATION_SHIP

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace TimeUtils
