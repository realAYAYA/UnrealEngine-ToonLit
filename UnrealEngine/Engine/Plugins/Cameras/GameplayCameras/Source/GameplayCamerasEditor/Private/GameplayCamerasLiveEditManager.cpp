// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCamerasLiveEditManager.h"

#include "Core/CameraInstantiableObject.h"
#include "UObject/UObjectGlobals.h"

FGameplayCamerasLiveEditManager::FGameplayCamerasLiveEditManager()
{
}

void FGameplayCamerasLiveEditManager::CleanUp()
{
	for (auto It = Instantiations.CreateIterator(); It; ++It)
	{
		// Clean up invalid entries on keys and values.
		UObject* Obj = It->Key.Get();
		if (!Obj)
		{
			It.RemoveCurrent();
			continue;
		}
		for (auto ObjIt = It->Value.InstantiatedObjects.CreateIterator(); ObjIt; ++ObjIt)
		{
			if (!ObjIt->IsValid())
			{
				ObjIt.RemoveCurrent();
			}
		}

		// If the source object doesn't have any instantiated objects anymore, clear its flags.
		if (It->Value.InstantiatedObjects.IsEmpty())
		{
			if (UCameraInstantiableObject* SourceObject = Cast<UCameraInstantiableObject>(Obj))
			{
				SourceObject->SetInstantiationState(ECameraNodeInstantiationState::None);
			}
		}
	}
}

void FGameplayCamerasLiveEditManager::RegisterInstantiatedObjects(const TMap<UObject*, UObject*> InstantiatedObjects)
{
	for (const TPair<UObject*, UObject*>& Pair : InstantiatedObjects)
	{
		// If the objects are instantiable, set appropriate flags on both.
		if (UCameraInstantiableObject* SourceObject = Cast<UCameraInstantiableObject>(Pair.Key))
		{
			SourceObject->SetInstantiationState(ECameraNodeInstantiationState::HasInstantiations);

			UCameraInstantiableObject* InstantiatedObject = CastChecked<UCameraInstantiableObject>(Pair.Value);
			InstantiatedObject->SetInstantiationState(ECameraNodeInstantiationState::IsInstantiated);
		}
		
		FInstantiationInfo& Info = Instantiations.FindOrAdd(Pair.Key);
		Info.InstantiatedObjects.Add(Pair.Value);
	}
}

void FGameplayCamerasLiveEditManager::ForwardPropertyChange(const UObject* Object, const FPropertyChangedEvent& PropertyChangedEvent)
{
	FInstantiationInfo* Info = Instantiations.Find(Object);
	if (!Info)
	{
		return;
	}

	const UClass* ObjectClass = Object->GetClass();
	const FProperty* ChangedProperty = PropertyChangedEvent.MemberProperty;
	if (!ensure(ChangedProperty))
	{
		return;
	}

	bool bForwardChange = false;
	if (ChangedProperty->IsA<FBoolProperty>()
			|| ChangedProperty->IsA<FNumericProperty>()
			|| ChangedProperty->IsA<FStructProperty>())
	{
		bForwardChange = true;
	}
	if (!bForwardChange)
	{
		return;
	}

	const void* SourceValuePtr = ChangedProperty->ContainerPtrToValuePtr<void>(Object);
	for (auto It = Info->InstantiatedObjects.CreateIterator(); It; ++It)
	{
		if (UObject* Inst = It->Get())
		{
			ensure(Inst->GetClass() == ObjectClass);
			void* InstValuePtr = ChangedProperty->ContainerPtrToValuePtr<void>(Inst);
			ChangedProperty->CopyCompleteValue(InstValuePtr, SourceValuePtr);
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

