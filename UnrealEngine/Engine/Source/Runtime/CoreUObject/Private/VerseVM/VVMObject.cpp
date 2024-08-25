// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMObject.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMClassInline.h"
#include "VerseVM/Inline/VVMObjectInline.h"
#include "VerseVM/Inline/VVMShapeInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMFunction.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMVar.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VObject);
TGlobalTrivialEmergentTypePtr<&VObject::StaticCppClassInfo> VObject::GlobalTrivialEmergentType;

template <typename TVisitor>
void VObject::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumIndexedFields = GetEmergentType()->Shape->NumIndexedFields;
		Visitor.BeginArray(TEXT("Data"), ScratchNumIndexedFields);
		Visitor.Visit(Data, Data + ScratchNumIndexedFields);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(Data, Data + GetEmergentType()->Shape->NumIndexedFields);
	}
}

bool VObject::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!IsStruct())
	{
		return this == Other;
	}

	if (!Other->IsA<VObject>())
	{
		return false;
	}

	if (GetEmergentType()->Type != Other->GetEmergentType()->Type)
	{
		return false;
	}

	if (GetEmergentType()->Shape->Fields.Num() != Other->GetEmergentType()->Shape->Fields.Num())
	{
		return false;
	}

	// TODO: Optimize for when objects share emergent type
	VObject& OtherObject = Other->StaticCast<VObject>();
	for (VShape::FieldsMap::TConstIterator It = GetEmergentType()->Shape->Fields; It; ++It)
	{
		VValue FieldValue = OtherObject.LoadField(Context, *It.Key().Get());
		if (!FieldValue)
		{
			return false;
		}

		if (!VValue::Equal(Context, LoadField(Context, *It.Key().Get()), FieldValue, HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

// TODO: Make this (And all other container TypeHash funcs) handle placeholders appropriately
uint32 VObject::GetTypeHashImpl()
{
	if (!IsStruct())
	{
		return PointerHash(this);
	}

	// Hash nominal type
	uint32 Result = PointerHash(GetEmergentType()->Type.Get());
	for (VShape::FieldsMap::TConstIterator It = GetEmergentType()->Shape->Fields; It; ++It)
	{
		// Hash Field Name
		::HashCombineFast(Result, GetTypeHash(It.Key()));

		// Hash Value
		It.Value().Type == EFieldType::Constant ? ::HashCombineFast(Result, GetTypeHash(It.Value().Value)) : ::HashCombineFast(Result, GetTypeHash(Data[It.Value().Index]));
	}
	return Result;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
