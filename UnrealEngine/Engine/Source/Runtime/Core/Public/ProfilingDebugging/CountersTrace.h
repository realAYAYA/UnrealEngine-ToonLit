// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/PreprocessorHelpers.h"
#include "Misc/Build.h"
#include "Templates/IsArrayOrRefOfTypeByPredicate.h"
#include "Trace/Config.h"
#include "Trace/Detail/Channel.h"
#include "Trace/Detail/Channel.inl"
#include "Trace/Trace.h"

#include <atomic>

#if !defined(COUNTERSTRACE_ENABLED)
#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
#define COUNTERSTRACE_ENABLED 1
#else
#define COUNTERSTRACE_ENABLED 0
#endif
#endif

enum ETraceCounterType
{
	TraceCounterType_Int = 0,
	TraceCounterType_Float = 1,
};

enum ETraceCounterDisplayHint
{
	TraceCounterDisplayHint_None = 0,
	TraceCounterDisplayHint_Memory = 1,
};

enum ETraceCounterNameType
{
	TraceCounterNameType_Static = 0, // TCounter is allowed to keep a pointer to InCounterName string
	TraceCounterNameType_Dynamic = 0x10, // TCounter needs to copy the InCounterName string
	TraceCounterNameType_AllocNameCopy = 0x20, // TCounter has allocated a copy of the InCounterName string
};

#if COUNTERSTRACE_ENABLED

UE_TRACE_CHANNEL_EXTERN(CountersChannel, CORE_API);

struct FCountersTrace
{
	CORE_API static uint16 OutputInitCounter(const TCHAR* CounterName, ETraceCounterType CounterType, ETraceCounterDisplayHint CounterDisplayHint);
	CORE_API static void OutputSetValue(uint16 CounterId, int64 Value);
	CORE_API static void OutputSetValue(uint16 CounterId, double Value);

	CORE_API static const TCHAR* AllocAndCopyCounterName(const TCHAR* InCounterName);
	CORE_API static void FreeCounterName(const TCHAR* InCounterName);

	template<typename ValueType, ETraceCounterType CounterType, typename StoredType = ValueType>
	class TCounter
	{
	public:
		TCounter() = delete;

		template<int N>
		TCounter(const TCHAR(&InCounterName)[N], ETraceCounterDisplayHint InCounterDisplayHint)
			: Value(0)
			, CounterName(InCounterName) // assumes that InCounterName is a static string
			, CounterId(0)
			, CounterDisplayHint(InCounterDisplayHint)
		{
			CounterId = OutputInitCounter(InCounterName, CounterType, CounterDisplayHint);
		}

		TCounter(ETraceCounterNameType InCounterNameType, const TCHAR* InCounterName, ETraceCounterDisplayHint InCounterDisplayHint)
			: Value(0)
			, CounterName(InCounterName)
			, CounterId(0)
			, CounterDisplayHint(InCounterDisplayHint)
		{
			CounterId = OutputInitCounter(InCounterName, CounterType, CounterDisplayHint);

			if (CounterId == 0 && InCounterNameType == TraceCounterNameType_Dynamic)
			{
				// Store counter name for late init. Needs a copy as InCounterName pointer might not be valid later.
				CounterName = AllocAndCopyCounterName(InCounterName);
				CounterDisplayHint = ETraceCounterDisplayHint(uint8(CounterDisplayHint) | uint8(TraceCounterNameType_AllocNameCopy));
			}
		}

		~TCounter()
		{
			if (uint8(CounterDisplayHint) & uint8(TraceCounterNameType_AllocNameCopy))
			{
				FreeCounterName(CounterName);
			}
		}

		void LateInit()
		{
			uint32 OldId = CounterId.load();
			if (!OldId)
			{
				uint32 NewId = OutputInitCounter(CounterName, CounterType, ETraceCounterDisplayHint(uint8(CounterDisplayHint) & 0xF));
				CounterId.compare_exchange_weak(OldId, NewId);
			}
		}

		ValueType Get() const
		{
			return Value;
		}

		void Set(ValueType InValue)
		{
			if (Value != InValue)
			{
				Value = InValue;
				if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
				{
					LateInit();
					OutputSetValue(uint16(CounterId), Value);
				}
			}
		}

		void Add(ValueType InValue)
		{
			if (InValue != 0)
			{
				Value += InValue;
				if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
				{
					LateInit();
					OutputSetValue(uint16(CounterId), Value);
				}
			}
		}

		void Subtract(ValueType InValue)
		{
			if (InValue != 0)
			{
				Value -= InValue;
				if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
				{
					LateInit();
					OutputSetValue(uint16(CounterId), Value);
				}
			}
		}

		void Increment()
		{
			++Value;
			if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
			{
				LateInit();
				OutputSetValue(uint16(CounterId), Value);
			}
		}

		void Decrement()
		{
			--Value;
			if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CountersChannel))
			{
				LateInit();
				OutputSetValue(uint16(CounterId), Value);
			}
		}

	private:
		StoredType Value;
		const TCHAR* CounterName;
		std::atomic<uint32> CounterId;
		ETraceCounterDisplayHint CounterDisplayHint;
	};

	using FCounterInt = TCounter<int64, TraceCounterType_Int>;
	using FCounterAtomicInt = TCounter<int64, TraceCounterType_Int, std::atomic<int64>>;
	using FCounterFloat = TCounter<double, TraceCounterType_Float>;
};

#define __TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	static_assert(std::is_const_v<std::remove_reference_t<decltype(CounterDisplayName)>>, "CounterDisplayName string must be a const TCHAR array."); \
	static_assert(TIsArrayOrRefOfTypeByPredicate<decltype(CounterDisplayName), TIsCharEncodingCompatibleWithTCHAR>::Value, "CounterDisplayName string must be a TCHAR array.");

#define __TRACE_DECLARE_INLINE_COUNTER(CounterDisplayName, CounterType, CounterDisplayHint) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	static FCountersTrace::CounterType PREPROCESSOR_JOIN(__TraceCounter, __LINE__)(CounterDisplayName, CounterDisplayHint);

#define TRACE_INT_VALUE(CounterDisplayName, Value) \
	__TRACE_DECLARE_INLINE_COUNTER(CounterDisplayName, FCounterInt, TraceCounterDisplayHint_None) \
	PREPROCESSOR_JOIN(__TraceCounter, __LINE__).Set(Value);

#define TRACE_FLOAT_VALUE(CounterDisplayName, Value) \
	__TRACE_DECLARE_INLINE_COUNTER(CounterDisplayName, FCounterFloat, TraceCounterDisplayHint_None) \
	PREPROCESSOR_JOIN(__TraceCounter, __LINE__).Set(Value);

#define TRACE_MEMORY_VALUE(CounterDisplayName, Value) \
	__TRACE_DECLARE_INLINE_COUNTER(CounterDisplayName, FCounterInt, TraceCounterDisplayHint_Memory) \
	PREPROCESSOR_JOIN(__TraceCounter, __LINE__).Set(Value);

#define TRACE_DECLARE_INT_COUNTER(CounterName, CounterDisplayName) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	FCountersTrace::FCounterInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName)(CounterDisplayName, TraceCounterDisplayHint_None);

#define TRACE_DECLARE_ATOMIC_INT_COUNTER(CounterName, CounterDisplayName) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	FCountersTrace::FCounterAtomicInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName)(CounterDisplayName, TraceCounterDisplayHint_None);

#define TRACE_DECLARE_INT_COUNTER_EXTERN(CounterName) \
	extern FCountersTrace::FCounterInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName);

#define TRACE_DECLARE_ATOMIC_INT_COUNTER_EXTERN(CounterName) \
	extern FCountersTrace::FCounterAtomicInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName);

#define TRACE_DECLARE_FLOAT_COUNTER(CounterName, CounterDisplayName) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	FCountersTrace::FCounterFloat PREPROCESSOR_JOIN(__GTraceCounter, CounterName)(CounterDisplayName, TraceCounterDisplayHint_None);

#define TRACE_DECLARE_FLOAT_COUNTER_EXTERN(CounterName) \
	extern FCountersTrace::FCounterFloat PREPROCESSOR_JOIN(__GTraceCounter, CounterName);

#define TRACE_DECLARE_MEMORY_COUNTER(CounterName, CounterDisplayName) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	FCountersTrace::FCounterInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName)(CounterDisplayName, TraceCounterDisplayHint_Memory);

#define TRACE_DECLARE_MEMORY_COUNTER_EXTERN(CounterName) \
	TRACE_DECLARE_INT_COUNTER_EXTERN(CounterName)

#define TRACE_DECLARE_ATOMIC_MEMORY_COUNTER(CounterName, CounterDisplayName) \
	__TRACE_CHECK_COUNTER_NAME(CounterDisplayName) \
	FCountersTrace::FCounterAtomicInt PREPROCESSOR_JOIN(__GTraceCounter, CounterName)(CounterDisplayName, TraceCounterDisplayHint_Memory);

#define TRACE_DECLARE_ATOMIC_MEMORY_COUNTER_EXTERN(CounterName) \
	TRACE_DECLARE_ATOMIC_INT_COUNTER_EXTERN(CounterName)

#define TRACE_COUNTER_SET(CounterName, Value) \
	PREPROCESSOR_JOIN(__GTraceCounter, CounterName).Set(Value);

#define TRACE_COUNTER_ADD(CounterName, Value) \
	PREPROCESSOR_JOIN(__GTraceCounter, CounterName).Add(Value);

#define TRACE_COUNTER_SUBTRACT(CounterName, Value) \
	PREPROCESSOR_JOIN(__GTraceCounter, CounterName).Subtract(Value);

#define TRACE_COUNTER_INCREMENT(CounterName) \
	PREPROCESSOR_JOIN(__GTraceCounter, CounterName).Increment();

#define TRACE_COUNTER_DECREMENT(CounterName) \
	PREPROCESSOR_JOIN(__GTraceCounter, CounterName).Decrement();

#else

#define TRACE_INT_VALUE(CounterDisplayName, Value)
#define TRACE_FLOAT_VALUE(CounterDisplayName, Value)
#define TRACE_MEMORY_VALUE(CounterDisplayName, Value)
#define TRACE_DECLARE_INT_COUNTER(CounterName, CounterDisplayName)
#define TRACE_DECLARE_INT_COUNTER_EXTERN(CounterName)
#define TRACE_DECLARE_ATOMIC_INT_COUNTER(CounterName, CounterDisplayName)
#define TRACE_DECLARE_ATOMIC_INT_COUNTER_EXTERN(CounterName)
#define TRACE_DECLARE_FLOAT_COUNTER(CounterName, CounterDisplayName)
#define TRACE_DECLARE_FLOAT_COUNTER_EXTERN(CounterName)
#define TRACE_DECLARE_MEMORY_COUNTER(CounterName, CounterDisplayName)
#define TRACE_DECLARE_MEMORY_COUNTER_EXTERN(CounterName)
#define TRACE_DECLARE_ATOMIC_MEMORY_COUNTER(CounterName, CounterDisplayName)
#define TRACE_DECLARE_ATOMIC_MEMORY_COUNTER_EXTERN(CounterName)
#define TRACE_COUNTER_SET(CounterName, Value)
#define TRACE_COUNTER_ADD(CounterName, Value)
#define TRACE_COUNTER_SUBTRACT(CounterName, Value)
#define TRACE_COUNTER_INCREMENT(CounterName)
#define TRACE_COUNTER_DECREMENT(CounterName)

#endif