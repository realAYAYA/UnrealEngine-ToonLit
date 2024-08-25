// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMIntInline.h"
#include "VerseVM/VVMEnumerator.h"
#include "VerseVM/VVMFalse.h"
#include "VerseVM/VVMFloat.h"
#include "VerseVM/VVMInt.h"
#include "VerseVM/VVMOpResult.h"
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMValue.h"

namespace Verse
{
inline VValue VRestValue::Get(FAllocationContext Context)
{
	if (Value.Get().IsRoot())
	{
		Value.Set(Context, VValue::Placeholder(VPlaceholder::New(Context, Value.Get().GetSplitDepth())));
		return Value.Get();
	}
	return Value.Get().Follow();
}

inline bool VRestValue::operator==(const VRestValue& Other) const
{
	if (Value.Get().IsRoot() && Other.Value.Get().IsRoot())
	{
		return this == &Other;
	}
	return Value.Get() == Other.Value.Get();
}

inline VValue::VValue(VCell& Cell)
	: Cell(&Cell)
{
	checkSlow(IsCell());
	checkSlow(!IsCellOfType<VPlaceholder>());
}

inline VValue::VValue(UObject* Object)
	: EncodedBits(BitCast<uint64>(Object) | UObjectTag)
{
	checkSlow(IsUObject());
}

inline VValue::VValue(VInt Int)
	: VValue(Int.Value)
{
}

inline bool VValue::IsInt() const
{
	return IsInt32() || IsCellOfType<VHeapInt>();
}

inline VInt VValue::AsInt() const
{
	return VInt(*this);
}

inline bool VValue::IsUint32() const
{
	return IsInt() && AsInt().IsUint32();
}

inline uint32 VValue::AsUint32() const
{
	return AsInt().AsUint32();
}

inline VValue VValue::FromBool(bool Bool)
{
	return Bool ? VValue(*GlobalTruePtr.Get()) : VValue(*GlobalFalsePtr.Get());
}

inline VValue VValue::Follow()
{
	checkSlow(!IsRoot());
	if (IsPlaceholder())
	{
		return AsPlaceholder().Follow();
	}
	return *this;
}

inline VPlaceholder& VValue::GetRootPlaceholder()
{
	return AsPlaceholder().Follow().AsPlaceholder();
}

inline void VValue::EnqueueSuspension(FRunningContext Context, VSuspension& Suspension)
{
	GetRootPlaceholder().EnqueueSuspension(Context, Suspension);
}

inline bool VValue::IsLogic() const
{
	return Cell == GlobalFalsePtr.Get() || Cell == GlobalTruePtr.Get();
}

inline bool VValue::AsBool() const
{
	check(IsLogic());
	return Cell == GlobalTruePtr.Get();
}

inline bool VValue::IsEnumerator() const
{
	return IsCellOfType<VEnumerator>();
}

template <typename ObjectType>
bool VValue::IsCellOfType() const
{
	return IsCell() && AsCell().IsA<ObjectType>();
}

template <typename ObjectType>
ObjectType& VValue::StaticCast() const
{
	return AsCell().StaticCast<ObjectType>();
}

template <typename ObjectType>
ObjectType* VValue::DynamicCast() const
{
	return IsCell() ? AsCell().DynamicCast<ObjectType>() : nullptr;
}

inline uint32 GetTypeHash(VValue Value)
{
	V_DIE_IF(Value.IsPlaceholder() || Value.IsUninitialized());
	if (Value.IsFloat())
	{
		const Verse::VFloat ValueAsFloat = Value.AsFloat();
		// Normalize any input NaN to a consistent standard for a "pure" NaN form.
		if (ValueAsFloat.IsNaN())
		{
			return ::GetTypeHash(VFloat::NaN().AsDouble());
		}
		// Handles -0 VS +0 representations; just normalizes it to +0 if necessary.
		// `AsDouble` calls `NormalizeSignedZero`.
		return ::GetTypeHash(ValueAsFloat.AsDouble());
	}
	else if (Value.IsInt())
	{
		return GetTypeHash(Value.AsInt());
	}
	else if (Value.IsLogic())
	{
		return Value.AsBool() ? PointerHash(Verse::GlobalTruePtr.Get()) : PointerHash(Verse::GlobalFalsePtr.Get());
	}
	else if (Value.IsCell())
	{
		return GetTypeHash(Value.AsCell());
	}
	else if (Value.IsUObject())
	{
		return ::GetTypeHash(Value.AsUObject());
	}
	else if (Value.IsChar())
	{
		return ::GetTypeHash(Value.AsChar());
	}
	else if (Value.IsChar32())
	{
		return ::GetTypeHash(Value.AsChar32());
	}
	else
	{
		VERSE_UNREACHABLE();
	}
}

inline uint32 GetTypeHash(const VPlaceholder& Placeholder)
{
	// We're `const_cast`-ing here because `Follow` is non-const for the case where
	// eventually we want to do path compression on the `VValue` within. In that case, the
	// value would have to be modified since the underlying pointers would be re-pointed.
	return GetTypeHash(const_cast<VPlaceholder&>(Placeholder).Follow());
}

inline uint32 GetTypeHash(VRestValue RestValue)
{
	return GetTypeHash(RestValue.Value.Get());
}

template <typename ContextType>
inline FOpResult VValue::Melt(ContextType Context, VValue Value)
{
	if (Value.IsPlaceholder())
	{
		return {FOpResult::ShouldSuspend, Value};
	}
	else if (Value.IsCell() && Value.AsCell().IsDeeplyMutable())
	{
		return Value.AsCell().Melt(FRunningContext(Context));
	}
	return {FOpResult::Normal, Value};
}

template <typename ContextType>
inline FOpResult VValue::Freeze(ContextType Context, VValue Value)
{
	if (Value.IsPlaceholder())
	{
		V_DIE("Freezing does not support non-concrete values!");
	}
	else if (Value.IsCell() && Value.AsCell().IsDeeplyMutable())
	{
		return Value.AsCell().Freeze(FRunningContext(Context));
	}
	return {FOpResult::Normal, Value};
}

} // namespace Verse
#endif // WITH_VERSE_VM
