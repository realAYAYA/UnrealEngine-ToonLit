// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMHeapInt.h"
#include "VVMValue.h"
#include "VerseVM/VVMFloat.h"
#include <cstdint>

namespace Verse
{
struct VInt
{
	explicit VInt(VValue InValue)
		: Value(InValue)
	{
		checkSlow(InValue.IsInt());
	}
	explicit VInt(int32 InInt32)
		: Value(VValue::FromInt32(InInt32))
	{
	}
	VInt(int64) = delete; // nb: use a constructor that takes F*Context explicitly
	VInt(FAllocationContext Context, int64 Int64)
	{
		if (Int64 >= INT32_MIN && Int64 <= INT32_MAX)
		{
			Value = VValue::FromInt32(static_cast<int32>(Int64));
		}
		else
		{
			Value = VHeapInt::FromInt64(Context, Int64);
		}
	}
	VInt(VHeapInt& N);

	bool IsZero() const
	{
		if (Value.IsInt32())
		{
			return Value.AsInt32() == 0;
		}
		VHeapInt& HeapInt = Value.StaticCast<VHeapInt>();
		return HeapInt.IsZero();
	}

	bool IsNegative() const
	{
		if (Value.IsInt32())
		{
			return Value.AsInt32() < 0;
		}
		VHeapInt& HeapInt = Value.StaticCast<VHeapInt>();
		return HeapInt.GetSign();
	}

	bool IsInt64() const;
	int64 AsInt64() const;

	bool IsUint32() const;
	uint32 AsUint32() const;

	VFloat ConvertToFloat() const;

	static VInt Add(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt Sub(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt Mul(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt Div(FRunningContext Context, VInt Lhs, VInt Rhs, bool* bOutHasNonZeroRemainder = nullptr);
	static VInt Mod(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt Neg(FRunningContext Context, VInt N);
	static VInt Abs(FRunningContext Context, VInt N);
	template <typename ContextType>
	static bool Eq(ContextType Context, VInt Lhs, VInt Rhs);
	static bool Gt(FRunningContext Context, VInt Lhs, VInt Rhs);
	static bool Lt(FRunningContext Context, VInt Lhs, VInt Rhs);
	static bool Gte(FRunningContext Context, VInt Lhs, VInt Rhs);
	static bool Lte(FRunningContext Context, VInt Lhs, VInt Rhs);

	friend uint32 GetTypeHash(VInt Int);

private:
	friend struct VValue;

	VValue Value;

	VFloat ConvertToFloatSlowPath() const;

	static VInt AddSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt SubSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt MulSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt DivSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs, bool* bOutHasNonZeroRemainder);
	static VInt ModSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs);
	static VInt NegSlowPath(FRunningContext Context, VInt N);
	static VInt AbsSlowPath(FRunningContext Context, VInt N);
	template <typename ContextType>
	static bool EqSlowPath(ContextType Context, VInt Lhs, VInt Rhs);
	static bool LtSlowPath(FRunningContext, VInt Lhs, VInt Rhs);
	static bool GtSlowPath(FRunningContext, VInt Lhs, VInt Rhs);
	static bool LteSlowPath(FRunningContext, VInt Lhs, VInt Rhs);
	static bool GteSlowPath(FRunningContext, VInt Lhs, VInt Rhs);

	static VHeapInt& AsHeapInt(FRunningContext, VInt N);
};

} // namespace Verse
#endif // WITH_VERSE_VM
