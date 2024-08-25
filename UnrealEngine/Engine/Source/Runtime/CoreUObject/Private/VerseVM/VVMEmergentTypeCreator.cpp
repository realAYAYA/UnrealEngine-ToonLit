// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEmergentTypeCreator.h"

#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/VVMUniqueCreator.h"
#include "VerseVM/VVMUnreachable.h"

namespace Verse
{

TGlobalHeapPtr<VEmergentType> VEmergentTypeCreator::EmergentTypeForEmergentType;
TGlobalHeapPtr<VEmergentType> VEmergentTypeCreator::EmergentTypeForType;
TLazyInitialized<VUniqueCreator<VEmergentType>> VEmergentTypeCreator::UniqueCreator;
bool VEmergentTypeCreator::bIsInitialized;

VEmergentType* VEmergentTypeCreator::GetOrCreate(FAllocationContext Context, VType* Type, VCppClassInfo* CppClassInfo)
{
	return UniqueCreator->GetOrCreate<VEmergentType, VType*>(Context, Type, CppClassInfo);
};

VEmergentType* VEmergentTypeCreator::GetOrCreate(FAllocationContext Context, VShape* InShape, VType* Type, VCppClassInfo* CppClassInfo)
{
	return UniqueCreator->GetOrCreate<VEmergentType, VShape*, VType*>(Context, InShape, Type, CppClassInfo);
};

void VEmergentTypeCreator::Initialize()
{
	/*
	   Need to setup

	   EmergentTypeForEmergentType : VCell(EmergentTypeForEmergentType), Type(TypeForEmergentType)
	   EmergentTypeForType         : VCell(EmergentTypeForEmergentType), Type(TypeForType)
	   TypeForEmergentType         : VCell(EmergentTypeForType)
	   TypeForType                 : VCell(EmergentTypeForType)
	*/
	if (!bIsInitialized)
	{
		FRunningContext::Create([](FRunningContext Context) {
			EmergentTypeForEmergentType.Set(Context, VEmergentType::NewIncomplete(Context, &VEmergentType::StaticCppClassInfo));
			EmergentTypeForEmergentType->SetEmergentType(Context, EmergentTypeForEmergentType.Get());

			EmergentTypeForType.Set(Context, VEmergentType::NewIncomplete(Context, &VType::StaticCppClassInfo));
			EmergentTypeForType->SetEmergentType(Context, EmergentTypeForEmergentType.Get());

			VTrivialType::Initialize(Context);

			EmergentTypeForEmergentType->Type.Set(Context, VTrivialType::Singleton.Get());
			EmergentTypeForType->Type.Set(Context, VTrivialType::Singleton.Get());

			UniqueCreator->Add(Context, EmergentTypeForEmergentType.Get());
			UniqueCreator->Add(Context, EmergentTypeForType.Get());

			bIsInitialized = true;
		});
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
