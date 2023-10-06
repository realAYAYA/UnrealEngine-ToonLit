// Copyright Epic Games, Inc. All Rights Reserved.


#include "ConstraintsScripting.h"
#include "TransformConstraint.h"
#include "TransformableHandle.h"
#include "ConstraintsManager.h"
#include "TransformableRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConstraintsScripting)

UConstraintsManager* UConstraintsScriptingLibrary::GetManager(UWorld* InWorld)
{
	UConstraintsManager* ConstraintsManager = UConstraintsManager::Get(InWorld);
	//add ue_log
	return ConstraintsManager;
}

UTransformableComponentHandle* UConstraintsScriptingLibrary::CreateTransformableComponentHandle(
	UWorld* InWorld, USceneComponent* InSceneComponent, const FName& InSocketName)
{
	UConstraintsManager* ConstraintsManager = UConstraintsManager::Get(InWorld);
	if (ConstraintsManager)
	{
		UTransformableComponentHandle* Handle = FTransformConstraintUtils::CreateHandleForSceneComponent(InSceneComponent, InSocketName, ConstraintsManager);
		return Handle;

	}
	return nullptr;
}

UTransformableHandle* UConstraintsScriptingLibrary::CreateTransformableHandle(UWorld* InWorld, UObject* InObject, const FName& InAttachmentName)
{
	UConstraintsManager* ConstraintsManager = UConstraintsManager::Get(InWorld);
	if (!ConstraintsManager)
	{
		return nullptr;
	}
	
	// look for customized transform handle
	const FTransformableRegistry& Registry = FTransformableRegistry::Get();
	if (const FTransformableRegistry::CreateHandleFuncT CreateFunction = Registry.GetCreateFunction(InObject->GetClass()))
	{
		return CreateFunction(ConstraintsManager, InObject, InAttachmentName);
	}
	
	return nullptr;
}


UTickableTransformConstraint* UConstraintsScriptingLibrary::CreateFromType(
	UWorld* InWorld,
	const ETransformConstraintType InType)
{
	UTickableTransformConstraint* Constraint = FTransformConstraintUtils::CreateFromType(InWorld, InType);
	return Constraint;
}

bool UConstraintsScriptingLibrary::AddConstraint(UWorld* InWorld, UTransformableHandle* InParentHandle, UTransformableHandle* InChildHandle,
	UTickableTransformConstraint *InConstraint, const bool bMaintainOffset)
{
	bool Val =  FTransformConstraintUtils::AddConstraint(InWorld, InParentHandle, InChildHandle,InConstraint, bMaintainOffset);
	return Val;
}

TArray<UTickableConstraint*> UConstraintsScriptingLibrary::GetConstraintsArray(UWorld* InWorld)
{
	TArray<UTickableConstraint*> Constraints;
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	const TArray< TObjectPtr<UTickableConstraint> >& ConstraintsArray = Controller.GetConstraintsArray();
	for (const TObjectPtr<UTickableConstraint>& Constraint : ConstraintsArray)
	{
		Constraints.Add(Constraint.Get());
	}
	return Constraints;
}

bool UConstraintsScriptingLibrary::RemoveConstraint(UWorld* InWorld, int32 InIndex)
{
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	return Controller.RemoveConstraint(InIndex);
}

bool UConstraintsScriptingLibrary::RemoveThisConstraint(UWorld* InWorld, UTickableConstraint* InTickableConstraint)
{
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	const TArray< TObjectPtr<UTickableConstraint> >& ConstraintsArray = Controller.GetConstraintsArray();
	for (int32 Index = 0; Index < ConstraintsArray.Num(); ++Index)
	{
		if (InTickableConstraint == ConstraintsArray[Index])
		{
			return Controller.RemoveConstraint(Index);

		}
	}
	return false;
}
