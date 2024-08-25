// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Math/GuardedInt.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
inline VInt::VInt(VHeapInt& N)
{
	if (N.IsInt32())
	{
		Value = VValue::FromInt32(N.AsInt32());
	}
	else
	{
		Value = N;
	}
}

inline VFloat VInt::ConvertToFloat() const
{
	if (Value.IsInt32())
	{
		return VFloat(Value.AsInt32());
	}

	return Value.StaticCast<VHeapInt>().ConvertToFloat();
}

inline VInt VInt::Add(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		const int64 Result64 = static_cast<int64>(Lhs.Value.AsInt32()) + static_cast<int64>(Rhs.Value.AsInt32());
		return VInt(Context, Result64);
	}
	else
	{
		return VInt::AddSlowPath(Context, Lhs, Rhs);
	}
}
inline VInt VInt::Sub(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		const int64 Result64 = static_cast<int64>(Lhs.Value.AsInt32()) - static_cast<int64>(Rhs.Value.AsInt32());
		return VInt(Context, Result64);
	}
	else
	{
		return VInt::SubSlowPath(Context, Lhs, Rhs);
	}
}
inline VInt VInt::Mul(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		const int64 Result64 = static_cast<int64>(Lhs.Value.AsInt32()) * static_cast<int64>(Rhs.Value.AsInt32());
		return VInt(Context, Result64);
	}
	else
	{
		return VInt::MulSlowPath(Context, Lhs, Rhs);
	}
}
inline VInt VInt::Div(FRunningContext Context, VInt Lhs, VInt Rhs, bool* bOutHasNonZeroRemainder /*= nullptr*/)
{
	checkf(!Rhs.IsZero(), TEXT("Division by 0 is undefined!"));
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		if (Rhs.Value.AsInt32() == -1 && Lhs.Value.AsInt32() == INT32_MIN)
		{
			if (bOutHasNonZeroRemainder)
			{
				*bOutHasNonZeroRemainder = false;
			}
			return VInt(Context, int64(INT32_MAX) + 1);
		}
		const int32 Lhs32 = Lhs.Value.AsInt32();
		const int32 Rhs32 = Rhs.Value.AsInt32();
		const int32 Result32 = Lhs32 / Rhs32;
		if (bOutHasNonZeroRemainder)
		{
			*bOutHasNonZeroRemainder = (Lhs32 != Rhs32 * Result32);
		}
		return VInt(Result32);
	}
	else
	{
		return VInt::DivSlowPath(Context, Lhs, Rhs, bOutHasNonZeroRemainder);
	}
}
inline VInt VInt::Mod(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	checkf(!Rhs.IsZero(), TEXT("Division by 0 is undefined!"));
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		if (Rhs.Value.AsInt32() == -1 || Rhs.Value.AsInt32() == 1)
		{
			return VInt(0);
		}
		const int32 Result32 = Lhs.Value.AsInt32() % Rhs.Value.AsInt32();
		return VInt(Result32);
	}
	else
	{
		return VInt::ModSlowPath(Context, Lhs, Rhs);
	}
}
inline VInt VInt::Neg(FRunningContext Context, VInt x)
{
	if (x.Value.IsInt32())
	{
		const int64 r64 = static_cast<int64>(x.Value.AsInt32());
		return VInt(Context, -r64);
	}
	return VInt::NegSlowPath(Context, x);
}
inline VInt VInt::Abs(FRunningContext Context, VInt x)
{
	if (x.Value.IsInt32())
	{
		const int64 r64 = static_cast<int64>(x.Value.AsInt32());
		return VInt(Context, r64 < 0 ? -r64 : r64);
	}
	return VInt::AbsSlowPath(Context, x);
}

template <typename ContextType>
inline bool VInt::Eq(ContextType Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		return Lhs.Value.AsInt32() == Rhs.Value.AsInt32();
	}
	else
	{
		return VInt::EqSlowPath(Context, Lhs, Rhs);
	}
}

inline bool VInt::Lt(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		return Lhs.Value.AsInt32() < Rhs.Value.AsInt32();
	}
	return VInt::LtSlowPath(Context, Lhs, Rhs);
}

inline bool VInt::Gt(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		return Lhs.Value.AsInt32() > Rhs.Value.AsInt32();
	}
	return VInt::GtSlowPath(Context, Lhs, Rhs);
}

inline bool VInt::Lte(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		return Lhs.Value.AsInt32() <= Rhs.Value.AsInt32();
	}
	return VInt::LteSlowPath(Context, Lhs, Rhs);
}

inline bool VInt::Gte(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() && Rhs.Value.IsInt32())
	{
		return Lhs.Value.AsInt32() >= Rhs.Value.AsInt32();
	}
	return VInt::GteSlowPath(Context, Lhs, Rhs);
}

inline VInt VInt::AddSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.IsInt64() && Rhs.IsInt64())
	{
		const int64 Lhs64 = Lhs.AsInt64();
		const int64 Rhs64 = Rhs.AsInt64();
		FGuardedInt64 Result = FGuardedInt64(Lhs64) + FGuardedInt64(Rhs64);
		if (Result.IsValid())
		{
			return VInt(Context, Result.GetChecked());
		}
	}

	VHeapInt& LhsHeapInt = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(Context, Rhs);
	return VInt(*VHeapInt::Add(Context, LhsHeapInt, RhsHeapInt));
}

inline VInt VInt::SubSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.IsInt64() && Rhs.IsInt64())
	{
		const int64 Lhs64 = Lhs.AsInt64();
		const int64 Rhs64 = Rhs.AsInt64();
		FGuardedInt64 Result = FGuardedInt64(Lhs64) - FGuardedInt64(Rhs64);
		if (Result.IsValid())
		{
			return VInt(Context, Result.GetChecked());
		}
	}
	VHeapInt& LhsHeapInt = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(Context, Rhs);
	return VInt(*VHeapInt::Sub(Context, LhsHeapInt, RhsHeapInt));
}

inline VInt VInt::MulSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.IsInt64() && Rhs.IsInt64())
	{
		const int64 Lhs64 = Lhs.AsInt64();
		const int64 Rhs64 = Rhs.AsInt64();
		FGuardedInt64 Result = FGuardedInt64(Lhs64) * FGuardedInt64(Rhs64);
		if (Result.IsValid())
		{
			return VInt(Context, Result.GetChecked());
		}
	}
	VHeapInt& LhsHeapInt = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(Context, Rhs);
	return VInt(*VHeapInt::Multiply(Context, LhsHeapInt, RhsHeapInt));
}

inline VInt VInt::DivSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs, bool* bOutHasNonZeroRemainder)
{
	if (Lhs.IsInt64() && Rhs.IsInt64())
	{
		const int64 Lhs64 = Lhs.AsInt64();
		const int64 Rhs64 = Rhs.AsInt64();
		FGuardedInt64 Result = FGuardedInt64(Lhs64) / FGuardedInt64(Rhs64);
		if (Result.IsValid())
		{
			if (bOutHasNonZeroRemainder)
			{
				*bOutHasNonZeroRemainder = (Lhs64 != Rhs64 * Result.GetChecked());
			}
			return VInt(Context, Result.GetChecked());
		}
	}
	VHeapInt& LhsHeapInt = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(Context, Rhs);
	return VInt(*VHeapInt::Divide(Context, LhsHeapInt, RhsHeapInt, bOutHasNonZeroRemainder));
}

inline VInt VInt::ModSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.IsInt64() && Rhs.IsInt64())
	{
		const int64 Lhs64 = Lhs.AsInt64();
		const int64 Rhs64 = Rhs.AsInt64();
		FGuardedInt64 Result = FGuardedInt64(Lhs64) % FGuardedInt64(Rhs64);
		if (Result.IsValid())
		{
			return VInt(Context, Result.GetChecked());
		}
	}
	VHeapInt& LhsHeapInt = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(Context, Rhs);
	return VInt(*VHeapInt::Modulo(Context, LhsHeapInt, RhsHeapInt));
}

template <typename ContextType>
inline bool VInt::EqSlowPath(ContextType Context, VInt Lhs, VInt Rhs)
{
	if (Lhs.Value.IsInt32() || Rhs.Value.IsInt32())
	{
		return false;
	}

	// TODO: To compare an inline int to a heap int, we have to allocate a heap int... this should be fixed somehow
	VHeapInt& LhsHeapInt = VInt::AsHeapInt(FRunningContext(Context), Lhs);
	VHeapInt& RhsHeapInt = VInt::AsHeapInt(FRunningContext(Context), Rhs);
	return VHeapInt::Equals(LhsHeapInt, RhsHeapInt);
}

inline VInt VInt::NegSlowPath(FRunningContext Context, VInt N)
{
	VHeapInt& NHeap = N.Value.StaticCast<VHeapInt>();
	return VInt(*VHeapInt::UnaryMinus(Context, NHeap));
}

inline VInt VInt::AbsSlowPath(FRunningContext Context, VInt N)
{
	VHeapInt& NHeap = N.Value.StaticCast<VHeapInt>();
	return VInt(NHeap.GetSign() ? *VHeapInt::UnaryMinus(Context, NHeap) : NHeap);
}

inline bool VInt::LtSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	VHeapInt& LhsHeap = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeap = VInt::AsHeapInt(Context, Rhs);
	return VHeapInt::Compare(LhsHeap, RhsHeap) == VHeapInt::ComparisonResult::LessThan;
}

inline bool VInt::GtSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	VHeapInt& LhsHeap = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeap = VInt::AsHeapInt(Context, Rhs);
	return VHeapInt::Compare(LhsHeap, RhsHeap) == VHeapInt::ComparisonResult::GreaterThan;
}

inline bool VInt::LteSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	VHeapInt& LhsHeap = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeap = VInt::AsHeapInt(Context, Rhs);
	const VHeapInt::ComparisonResult Result = VHeapInt::Compare(LhsHeap, RhsHeap);
	return Result == VHeapInt::ComparisonResult::LessThan || Result == VHeapInt::ComparisonResult::Equal;
}

inline bool VInt::GteSlowPath(FRunningContext Context, VInt Lhs, VInt Rhs)
{
	VHeapInt& LhsHeap = VInt::AsHeapInt(Context, Lhs);
	VHeapInt& RhsHeap = VInt::AsHeapInt(Context, Rhs);
	const VHeapInt::ComparisonResult Result = VHeapInt::Compare(LhsHeap, RhsHeap);
	return Result == VHeapInt::ComparisonResult::GreaterThan || Result == VHeapInt::ComparisonResult::Equal;
}

inline VHeapInt& VInt::AsHeapInt(FRunningContext Context, VInt N)
{
	return N.Value.IsInt32()
			 ? VHeapInt::FromInt64(Context, N.Value.AsInt32())
			 : N.Value.StaticCast<VHeapInt>();
}

inline bool VInt::IsInt64() const
{
	if (Value.IsInt32())
	{
		return true;
	}
	if (VHeapInt* HeapInt = Value.DynamicCast<VHeapInt>())
	{
		return HeapInt->IsInt64();
	}
	return false;
}

inline int64 VInt::AsInt64() const
{
	if (Value.IsInt32())
	{
		return static_cast<int64>(Value.AsInt32());
	}
	else
	{
		checkSlow(IsInt64());
		return Value.StaticCast<VHeapInt>().AsInt64();
	}
}

inline bool VInt::IsUint32() const
{
	if (IsInt64())
	{
		int64 I64 = AsInt64();
		return I64 >= 0 && static_cast<uint64>(I64) <= static_cast<uint64>(std::numeric_limits<uint32>::max());
	}

	return false;
}

inline uint32 VInt::AsUint32() const
{
	checkSlow(IsUint32());
	return static_cast<uint32>(AsInt64());
}

inline uint32 GetTypeHash(VInt Int)
{
	if (Int.Value.IsInt32())
	{
		return ::GetTypeHash(Int.Value.AsInt32());
	}
	if (Int.IsInt64())
	{
		return ::GetTypeHash(Int.AsInt64());
	}
	return GetTypeHash(Int.Value.StaticCast<VHeapInt>());
}
} // namespace Verse
#endif // WITH_VERSE_VM
