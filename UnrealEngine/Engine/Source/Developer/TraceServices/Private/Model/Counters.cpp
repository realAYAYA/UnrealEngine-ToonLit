// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/Counters.h"
#include "Model/CountersPrivate.h"
#include "AnalysisServicePrivate.h"

namespace TraceServices
{

const FName FCounterProvider::ProviderName("CounterProvider");

FCounter::FCounter(ILinearAllocator& Allocator, const TArray64<double>& InFrameStartTimes)
	: FrameStartTimes(InFrameStartTimes)
	, IntCounterData(Allocator)
	, DoubleCounterData(Allocator)
{

}

template<typename CounterType, typename EnumerationType>
static void EnumerateCounterValuesInternal(const TCounterData<CounterType>& CounterData, const TArray64<double>& FrameStartTimes, bool bResetEveryFrame, double IntervalStart, double IntervalEnd, bool bIncludeExternalBounds, TFunctionRef<void(double, EnumerationType)> Callback)
{
	if (!CounterData.Num())
	{
		return;
	}
	TArray64<double> NoFrameStartTimes;
	auto CounterIterator = bResetEveryFrame ? CounterData.GetIterator(FrameStartTimes) : CounterData.GetIterator(NoFrameStartTimes);
	bool bFirstValue = true;
	bool bFirstEnumeratedValue = true;
	CounterType LastValue = CounterType();
	double LastTime = 0.0;
	while (CounterIterator)
	{
		const TTuple<double, CounterType>& Current = *CounterIterator;
		double Time = Current.template Get<0>();
		CounterType CurrentValue = Current.template Get<1>();
		if (Time >= IntervalStart)
		{
			if (bFirstEnumeratedValue)
			{
				if (!bFirstValue && bIncludeExternalBounds)
				{
					Callback(LastTime, static_cast<EnumerationType>(LastValue));
				}
				bFirstEnumeratedValue = false;
			}
			if (Time > IntervalEnd)
			{
				if (bIncludeExternalBounds)
				{
					Callback(Time, static_cast<EnumerationType>(CurrentValue));
				}
				break;
			}
			Callback(Time, static_cast<EnumerationType>(CurrentValue));
		}
		LastTime = Time;
		LastValue = CurrentValue;
		bFirstValue = false;
		++CounterIterator;
	}
}

void FCounter::SetIsFloatingPoint(bool bInIsFloatingPoint)
{
	check(!ModCount);
	bIsFloatingPoint = bInIsFloatingPoint;
}

void FCounter::EnumerateValues(double IntervalStart, double IntervalEnd, bool bIncludeExternalBounds, TFunctionRef<void(double, int64)> Callback) const
{
	if (bIsFloatingPoint)
	{
		EnumerateCounterValuesInternal<double, int64>(DoubleCounterData, FrameStartTimes, bIsResetEveryFrame, IntervalStart, IntervalEnd, bIncludeExternalBounds, Callback);
	}
	else
	{
		EnumerateCounterValuesInternal<int64, int64>(IntCounterData, FrameStartTimes, bIsResetEveryFrame, IntervalStart, IntervalEnd, bIncludeExternalBounds, Callback);
	}
}

void FCounter::EnumerateFloatValues(double IntervalStart, double IntervalEnd, bool bIncludeExternalBounds, TFunctionRef<void(double, double)> Callback) const
{
	if (bIsFloatingPoint)
	{
		EnumerateCounterValuesInternal<double, double>(DoubleCounterData, FrameStartTimes, bIsResetEveryFrame, IntervalStart, IntervalEnd, bIncludeExternalBounds, Callback);
	}
	else
	{
		EnumerateCounterValuesInternal<int64, double>(IntCounterData, FrameStartTimes, bIsResetEveryFrame, IntervalStart, IntervalEnd, bIncludeExternalBounds, Callback);
	}
}

void FCounter::AddValue(double Time, int64 Value)
{
	if (bIsFloatingPoint)
	{
		DoubleCounterData.InsertOp(Time, CounterOpType_Add, double(Value));
	}
	else
	{
		IntCounterData.InsertOp(Time, CounterOpType_Add, Value);
	}
	++ModCount;
}

void FCounter::AddValue(double Time, double Value)
{
	if (bIsFloatingPoint)
	{
		DoubleCounterData.InsertOp(Time, CounterOpType_Add, Value);
	}
	else
	{
		IntCounterData.InsertOp(Time, CounterOpType_Add, int64(Value));
	}
	++ModCount;
}

void FCounter::SetValue(double Time, int64 Value)
{
	if (bIsFloatingPoint)
	{
		DoubleCounterData.InsertOp(Time, CounterOpType_Set, double(Value));
	}
	else
	{
		IntCounterData.InsertOp(Time, CounterOpType_Set, Value);
	}
	++ModCount;
}

void FCounter::SetValue(double Time, double Value)
{
	if (bIsFloatingPoint)
	{
		DoubleCounterData.InsertOp(Time, CounterOpType_Set, Value);
	}
	else
	{
		IntCounterData.InsertOp(Time, CounterOpType_Set, int64(Value));
	}
	++ModCount;
}

FCounterProvider::FCounterProvider(IAnalysisSession& InSession, IFrameProvider& InFrameProvider)
	: Session(InSession)
	, FrameProvider(InFrameProvider)
{
}

FCounterProvider::~FCounterProvider()
{
	for (const ICounter* Counter : Counters)
	{
		delete Counter;
	}
}

void FCounterProvider::EnumerateCounters(TFunctionRef<void(uint32, const ICounter&)> Callback) const
{
	uint32 Id = 0;
	for (const ICounter* Counter : Counters)
	{
		Callback(Id++, *Counter);
	}
}

bool FCounterProvider::ReadCounter(uint32 CounterId, TFunctionRef<void(const ICounter &)> Callback) const
{
	if (CounterId >= uint32(Counters.Num()))
	{
		return false;
	}
	Callback(*Counters[CounterId]);
	return true;
}

const ICounter* FCounterProvider::GetCounter(IEditableCounter* EditableCounter)
{
	return static_cast<FCounter*>(EditableCounter);
}

IEditableCounter* FCounterProvider::CreateEditableCounter()
{
	FCounter* Counter = new FCounter(Session.GetLinearAllocator(), FrameProvider.GetFrameStartTimes(TraceFrameType_Game));
	Counters.Add(Counter);
	return Counter;
}

void FCounterProvider::AddCounter(const ICounter* Counter)
{
	Counters.Add(Counter);
}

const ICounterProvider& ReadCounterProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<ICounterProvider>(FCounterProvider::ProviderName);
}

IEditableCounterProvider& EditCounterProvider(IAnalysisSession& Session)
{
	return *Session.EditProvider<IEditableCounterProvider>(FCounterProvider::ProviderName);
}

} // namespace TraceServices
