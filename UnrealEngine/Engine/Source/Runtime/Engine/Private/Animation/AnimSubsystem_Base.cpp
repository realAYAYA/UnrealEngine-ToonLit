// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSubsystem_Base.h"
#include "Animation/AnimSubsystem.h"
#include "Animation/AnimClassInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimSubsystem_Base)

void FAnimSubsystem_Base::OnPostLoadDefaults(FAnimSubsystemPostLoadDefaultsContext& InContext)
{
	PatchValueHandlers(InContext.DefaultAnimInstance->GetClass());
}

void FAnimSubsystem_Base::PatchValueHandlers(UClass* InClass)
{
	ExposedValueHandlers.Reset();

	void* SparseClassData = const_cast<void*>(InClass->GetSparseClassData(EGetSparseClassDataMethod::ReturnIfNull));
	check(SparseClassData);
	for (TFieldIterator<FProperty> ItParam(InClass->GetSparseClassDataStruct()); ItParam; ++ItParam)
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(*ItParam))
		{
			if (StructProperty->Struct->IsChildOf(FAnimNodeExposedValueHandler::StaticStruct()))
			{
				FExposedValueHandler& NewHandler = ExposedValueHandlers.AddDefaulted_GetRef();
				NewHandler.HandlerStruct = StructProperty->Struct;
				NewHandler.Handler = StructProperty->ContainerPtrToValuePtr<FAnimNodeExposedValueHandler>(SparseClassData);
				NewHandler.Handler->Initialize(InClass);
			}
		}
	}
}