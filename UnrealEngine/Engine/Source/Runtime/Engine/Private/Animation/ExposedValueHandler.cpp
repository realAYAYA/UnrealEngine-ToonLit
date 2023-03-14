// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/ExposedValueHandler.h"
#include "UObject/Class.h"
#include "Animation/AnimSubsystem_PropertyAccess.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ExposedValueHandler)

void FExposedValueHandler::ClassInitialization(TArray<FExposedValueHandler>& InHandlers, UClass* InClass)
{
	if(const FAnimSubsystem_PropertyAccess* Subsystem = IAnimClassInterface::GetFromClass(InClass)->FindSubsystem<FAnimSubsystem_PropertyAccess>())
	{
		const FPropertyAccessLibrary& PropertyAccessLibrary = Subsystem->GetLibrary();

		for(FExposedValueHandler& Handler : InHandlers)
		{
			Handler.Initialize(InClass, PropertyAccessLibrary);
		}
	}
}

void FExposedValueHandler::Initialize(UClass* InClass, const FPropertyAccessLibrary& InPropertyAccessLibrary)
{
	if (BoundFunction != NAME_None)
	{
		// This cached function is nullptr when the CDO is initially serialized, or (in editor) when the class has been
		// recompiled and any instances have been re-instanced. When new instances are spawned, this function is
		// duplicated (it is a FProperty) onto those instances so we dont pay the cost of the FindFunction() call
#if !WITH_EDITOR
		if (Function == nullptr)
#endif
		{
			// we cant call FindFunction on anything but the game thread as it accesses a shared map in the object's class
			check(IsInGameThread());
			Function = InClass->FindFunctionByName(BoundFunction);
			check(Function);
		}
	}
	else
	{
		Function = nullptr;
	}

	// Cache property access library
	PropertyAccessLibrary = &InPropertyAccessLibrary;
}

void FExposedValueHandler::Execute(const FAnimationBaseContext& Context) const
{
	if (Function != nullptr)
	{
		Context.AnimInstanceProxy->GetAnimInstanceObject()->ProcessEvent(Function, nullptr);
	}

	if(CopyRecords.Num() > 0)
	{
		if(PropertyAccessLibrary != nullptr)
		{
			UObject* AnimInstanceObject = Context.AnimInstanceProxy->GetAnimInstanceObject();
			PropertyAccess::FCopyBatchId CopyBatch((int32)EAnimPropertyAccessCallSite::WorkerThread_Unbatched);
			for(const FExposedValueCopyRecord& CopyRecord : CopyRecords)
			{
				PropertyAccess::ProcessCopy(AnimInstanceObject, *PropertyAccessLibrary, CopyBatch, CopyRecord.CopyIndex, [&CopyRecord](const FProperty* InProperty, void* InAddress)
				{
					if(CopyRecord.PostCopyOperation == EPostCopyOperation::LogicalNegateBool)
					{
						bool bValue = static_cast<const FBoolProperty*>(InProperty)->GetPropertyValue(InAddress);
						static_cast<const FBoolProperty*>(InProperty)->SetPropertyValue(InAddress, !bValue);
					}
				});
			}
		}
	}
}

