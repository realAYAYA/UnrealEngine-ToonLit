// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMUTF8String.h"
#include "VerseVM/VVMUnreachable.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
inline VShape::VEntry::VEntry(const VShape::VEntry& Other)
	: Type(Other.Type)
{
	switch (Type)
	{
		case EFieldType::Offset:
			Index = Other.Index;
			break;
		case EFieldType::Constant:
			new (&Value) TWriteBarrier<VValue>(Other.Value);
			break;
	}
}

inline VShape::VEntry::VEntry()
	: Index(0)
	, Type(EFieldType::Offset) {}

inline VShape::VEntry::VEntry(FAccessContext Context, VValue InConstant)
	: Value(Context, InConstant)
	, Type(EFieldType::Constant) {}

inline bool VShape::VEntry::operator==(const VShape::VEntry& Other) const
{
	if (Type != Other.Type)
	{
		return false;
	}
	switch (Type)
	{
		case EFieldType::Offset:
			return Index == Other.Index;
		case EFieldType::FProperty:
			return Property == Other.Property;
		case EFieldType::Constant:
			return VValue::Equal(FRunningContextPromise(), Value.Get(), Other.Value.Get(),
				[](VValue Left, VValue Right) {
					checkSlow(!Left.IsPlaceholder());
					checkSlow(!Right.IsPlaceholder());
				});
	}
}

inline bool VShape::FFieldsMapKeyFuncs::Matches(KeyInitType A, KeyInitType B)
{
	return A == B;
}

inline bool VShape::FFieldsMapKeyFuncs::Matches(KeyInitType A, const VUniqueString& B)
{
	return *(A.Get()) == B;
}

inline uint32 VShape::FFieldsMapKeyFuncs::GetKeyHash(KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 VShape::FFieldsMapKeyFuncs::GetKeyHash(const VUniqueString& Key)
{
	return GetTypeHash(Key);
}

inline const VShape::VEntry* VShape::GetField(FAllocationContext Context, const VUniqueString& Name) const
{
	return Fields.FindByHash(GetTypeHash(Name), Name);
}

inline uint64 VShape::GetNumFields() const
{
	return Fields.Num();
}

inline bool VShape::operator==(const VShape& Other) const
{
	return Fields.OrderIndependentCompareEqual(Other.Fields);
}

inline uint32 GetTypeHash(const VShape::VEntry& Field)
{
	switch (Field.Type)
	{
		case Verse::EFieldType::Offset:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), ::GetTypeHash(Field.Index));
		case Verse::EFieldType::Constant:
			return HashCombineFast(::GetTypeHash(static_cast<int8>(Field.Type)), GetTypeHash(Field.Value.Get()));
		default:
			break;
	}
	VERSE_UNREACHABLE();
}

inline uint32 GetTypeHash(const VShape& Shape)
{
	uint32 Hash = 0;
	for (auto It : Shape.Fields)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(It.Key));
		Hash = HashCombineFast(Hash, GetTypeHash(It.Value));
	}
	return Hash;
}

} // namespace Verse
#endif // WITH_VERSE_VM
