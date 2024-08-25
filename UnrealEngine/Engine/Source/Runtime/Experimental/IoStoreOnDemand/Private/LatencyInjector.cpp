// Copyright Epic Games, Inc. All Rights Reserved.

#include "LatencyInjector.h"

#include "Containers/UnrealString.h"
#include "HAL/PlatformTime.h"
#include "Misc/Parse.h"
#include "String/ParseTokens.h"

#if IAS_WITH_LATENCY_INJECTOR

namespace UE::IO::IAS
{

////////////////////////////////////////////////////////////////////////////////
namespace Injector
{

static int64 CycleBase;
static int64 CycleFreq;
static int32 MinMs;
static int32 MaxMs;
static int32 FailPercent;
static int32 FailState;
////////////////////////////////////////////////////////////////////////////////
static void InitializeLatency(const TCHAR* CommandLine)
{
	// -Ias.AddLatencyMs=130   : add 130ms of latency
	// -Ias.AddLatencyMs=40/90 : random latency between 40ms and 90ms

	FString Value;
	if (!FParse::Value(CommandLine, TEXT("Ias.AddLatencyMs="), Value))
	{
		return;
	}

	TArray<int32> Values;
	UE::String::ParseTokens(Value, TEXT("/"), [&Values] (const FStringView& Token)
	{
		int32 Int = -1;
		LexFromString(Int, Token.GetData());
		Values.Add((Int >= 0) ? Int : -1);
	});

	for (; Values.Num() < 2; Values.Add(-1));
	FLatencyInjector::Set(Values[0], Values[1]);
}

////////////////////////////////////////////////////////////////////////////////
static void InitializeFailure(const TCHAR* CommandLine)
{
	// -Ias.ForceFailPercent=40  : 40% chance that a request will fail
	// -Ias.ForceFailPercent=0   : invalid
	// -Ias.ForceFailPercent=101 : invalid

	FString Value;
	if (!FParse::Value(CommandLine, TEXT("Ias.ForceFailPercent="), Value))
	{
		return;
	}

	int32 Percent = -1;
	LexFromString(Percent, *Value);
	if (Percent < 1 || Percent > 100)
		return;

	FLatencyInjector::SetFailureRate(Percent);
}

}; // namespace Injector



////////////////////////////////////////////////////////////////////////////////
void FLatencyInjector::Initialize(const TCHAR* CommandLine)
{
	using namespace Injector;
	InitializeLatency(CommandLine);
	InitializeFailure(CommandLine);
}

////////////////////////////////////////////////////////////////////////////////
void FLatencyInjector::Set(int32 MinMs, int32 MaxMs)
{
	if (MinMs < 0)
	{
		Injector::MinMs = Injector::MaxMs = 0;
		return;
	}

	if (MaxMs < 0)
	{
		MaxMs = MinMs;
	}
	else if (MinMs > MaxMs)
	{
		Swap(MinMs, MaxMs);
	}

	Injector::MinMs = MinMs;
	Injector::MaxMs = MaxMs;
}

////////////////////////////////////////////////////////////////////////////////
void FLatencyInjector::SetFailureRate(int32 Percent)
{
	using namespace Injector;
	FailPercent = Percent;
	FailState = int32(FPlatformTime::Cycles64()) | 1;
}

////////////////////////////////////////////////////////////////////////////////
bool FLatencyInjector::Begin(EType, uint32& Param)
{
	using namespace Injector;

	if (FailPercent)
	{
		FailState *= 0xac564b05;
		if (int32 Value = ((FailState & 0xff) * 100) / 0xff; Value < FailPercent)
		{
			return false;
		}
	}

	if (MaxMs <= 0 || MinMs < 0)
	{
		Param = 0;
		return true;
	}

	int64 Cycles = FPlatformTime::Cycles64();

	if (CycleBase == 0)
	{
		CycleBase = Cycles;
		CycleFreq = int64(1.0 / FPlatformTime::GetSecondsPerCycle());
	}

	uint64 Bias = MaxMs - MinMs;
	if (Bias)
	{
		Bias = (Cycles * 0x369dea0f31a53f85ull) % Bias;
	}
	Bias += MinMs;
	Bias *= CycleFreq;
	Bias /= 1000;

	Param = uint32(Cycles + Bias - CycleBase);
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FLatencyInjector::HasExpired(uint32 Param)
{
	if (Param == 0)
	{
		return true;
	}

	int64 Cycles = FPlatformTime::Cycles64();
	return Cycles >= (int64(Param) + Injector::CycleBase);
}

} // namespace UE::IO::IAS

#endif // IAS_WITH_LATENCY_INJECTOR
