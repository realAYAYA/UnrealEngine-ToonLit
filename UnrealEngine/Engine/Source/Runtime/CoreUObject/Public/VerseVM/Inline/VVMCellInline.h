// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeCompatibleBytes.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMHeap.h"
#include "VerseVM/VVMWeakKeyMapGuard.h"

namespace Verse
{
inline const VEmergentType* VCell::GetEmergentType() const
{
	return FHeap::EmergentTypeOffsetToPtr(EmergentTypeOffset);
}

inline const VCppClassInfo* VCell::GetCppClassInfo() const
{
	return GetEmergentType()->CppClassInfo;
}

template <typename CastType>
bool VCell::IsA() const
{
	static_assert(std::is_base_of_v<VCell, CastType>);
	return GetCppClassInfo()->IsA(&CastType::StaticCppClassInfo);
}

template <typename CastType>
const CastType& VCell::StaticCast() const
{
	checkf(IsA<CastType>(),
		TEXT("Expected object of type %s, but got object of type %s."),
		*CastType::StaticCppClassInfo.DebugName(),
		*DebugName());
	return *static_cast<const CastType*>(this);
}

template <typename CastType>
CastType& VCell::StaticCast()
{
	checkf(IsA<CastType>(),
		TEXT("Expected object of type %s, but got object of type %s."),
		*CastType::StaticCppClassInfo.DebugName(),
		*DebugName());
	return *static_cast<CastType*>(this);
}

template <typename CastType>
CastType* VCell::DynamicCast()
{
	return IsA<CastType>() ? static_cast<CastType*>(this) : nullptr;
}

template <typename CastType>
CastType* VCell::DynamicCast() const
{
	return IsA<CastType>() ? static_cast<CastType*>(this) : nullptr;
}

inline uint32 GetTypeHash(VCell& Cell)
{
	return Cell.GetEmergentType()->CppClassInfo->GetTypeHash(&Cell);
}

template <typename TVisitor>
void VCell::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.VisitEmergentType(GetEmergentType());
	if ((GCData.load(std::memory_order_relaxed) & GCDataIsWeakKeyBit))
	{
		FWeakKeyMapGuard Guard(FHeapPageHeader::Get(this));
		if (FWeakKeyMap* Map = Guard.TryGet())
		{
			Map->Visit(this, Visitor);
		}
		else
		{
			GCData &= ~GCDataIsWeakKeyBit;
		}
	}
}

DEFINE_TRIVIAL_VISIT_REFERENCES(VHeapValue);
} // namespace Verse
#endif // WITH_VERSE_VM
