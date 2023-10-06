// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectBlueprintFunctionLibrary.h"
#include "Engine/Engine.h"
#include "SmartObjectSubsystem.h"
#include "BlackboardKeyType_SOClaimHandle.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectBlueprintFunctionLibrary)

//----------------------------------------------------------------------//
// USmartObjectBlueprintFunctionLibrary 
//----------------------------------------------------------------------//
FSmartObjectClaimHandle USmartObjectBlueprintFunctionLibrary::GetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName)
{
	if (BlackboardComponent == nullptr)
	{
		return {};
	}
	return BlackboardComponent->GetValue<UBlackboardKeyType_SOClaimHandle>(KeyName);
}

void USmartObjectBlueprintFunctionLibrary::SetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName, const FSmartObjectClaimHandle Value)
{
	if (BlackboardComponent == nullptr)
	{
		return;
	}
	const FBlackboard::FKey KeyID = BlackboardComponent->GetKeyID(KeyName);
	BlackboardComponent->SetValue<UBlackboardKeyType_SOClaimHandle>(KeyID, Value);
}

bool USmartObjectBlueprintFunctionLibrary::AddOrRemoveSmartObject(AActor* SmartObjectActor, const bool bAdd)
{
	return AddOrRemoveMultipleSmartObjects({SmartObjectActor}, bAdd);
}

bool USmartObjectBlueprintFunctionLibrary::AddSmartObject(AActor* SmartObjectActor)
{
	return AddOrRemoveMultipleSmartObjects({SmartObjectActor}, /*bAdd*/true);
}

bool USmartObjectBlueprintFunctionLibrary::AddMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors)
{
	return AddOrRemoveMultipleSmartObjects(SmartObjectActors, /*bAdd*/true);
}

bool USmartObjectBlueprintFunctionLibrary::RemoveSmartObject(AActor* SmartObjectActor)
{
	return AddOrRemoveMultipleSmartObjects({SmartObjectActor}, /*bAdd*/false);
}

bool USmartObjectBlueprintFunctionLibrary::RemoveMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors)
{
	return AddOrRemoveMultipleSmartObjects(SmartObjectActors, /*bAdd*/false);
}

bool USmartObjectBlueprintFunctionLibrary::AddOrRemoveMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors, const bool bAdd)
{
	bool bSuccess = true;
	if (SmartObjectActors.IsEmpty())
	{
		return bSuccess;
	}

	USmartObjectSubsystem* Subsystem = nullptr;
	for (const AActor* SmartObjectActor : SmartObjectActors)
	{
		if (SmartObjectActor == nullptr)
		{
			UE_LOG(LogSmartObject, Warning, TEXT("Null actor found and skipped"))
			bSuccess = false;
			continue;
		}

		if (Subsystem == nullptr)
		{
			Subsystem = USmartObjectSubsystem::GetCurrent(SmartObjectActor->GetWorld());
			if (Subsystem == nullptr)
			{
				UE_LOG(LogSmartObject, Warning, TEXT("Unable to find SmartObjectSubsystem for the provided actors."))
				return false;
			}
		}

		bSuccess = bAdd ? Subsystem->RegisterSmartObjectActor(*SmartObjectActor) : Subsystem->RemoveSmartObjectActor(*SmartObjectActor) && bSuccess;
	}

	return bSuccess;
}

bool USmartObjectBlueprintFunctionLibrary::SetSmartObjectEnabled(AActor* SmartObjectActor, const bool bEnabled)
{
	return SetMultipleSmartObjectsEnabled({SmartObjectActor}, bEnabled);
}

FSmartObjectClaimHandle USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsClaimed(
	UObject* WorldContextObject,
	const FSmartObjectSlotHandle SlotHandle,
	const AActor* UserActor)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		return Subsystem->MarkSlotAsClaimed(SlotHandle, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}

	return FSmartObjectClaimHandle::InvalidHandle;
}

const USmartObjectBehaviorDefinition* USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsOccupied(
	UObject* WorldContextObject,
	const FSmartObjectClaimHandle ClaimHandle,
	const TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		Subsystem->MarkSlotAsOccupied(ClaimHandle, DefinitionClass);
	}

	return nullptr;
}

bool USmartObjectBlueprintFunctionLibrary::MarkSmartObjectSlotAsFree(
	UObject* WorldContextObject,
	const FSmartObjectClaimHandle ClaimHandle)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		return Subsystem->MarkSlotAsFree(ClaimHandle);	
	}

	return false;
}

bool USmartObjectBlueprintFunctionLibrary::SetMultipleSmartObjectsEnabled(const TArray<AActor*>& SmartObjectActors, const bool bEnabled)
{
	bool bSuccess = true;
	if (SmartObjectActors.IsEmpty())
	{
		return bSuccess;
	}

	USmartObjectSubsystem* Subsystem = nullptr;
	for (const AActor* SmartObjectActor : SmartObjectActors)
	{
		if (SmartObjectActor == nullptr)
		{
			UE_LOG(LogSmartObject, Warning, TEXT("Null actor found and skipped"))
			bSuccess = false;
			continue;
		}

		if (Subsystem == nullptr)
		{
			Subsystem = USmartObjectSubsystem::GetCurrent(SmartObjectActor->GetWorld());
			if (Subsystem == nullptr)
			{
				UE_LOG(LogSmartObject, Warning, TEXT("Unable to find SmartObjectSubsystem for the provided actors."))
				return false;
			}
		}

		bSuccess = Subsystem->SetSmartObjectActorEnabled(*SmartObjectActor, bEnabled) && bSuccess;
	}

	return bSuccess;
}

void USmartObjectBlueprintFunctionLibrary::SetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, const FSmartObjectClaimHandle& Value)
{
	if (UBlackboardComponent* BlackboardComp = UBTFunctionLibrary::GetOwnersBlackboard(NodeOwner))
	{
		BlackboardComp->SetValue<UBlackboardKeyType_SOClaimHandle>(Key.SelectedKeyName, Value);
	}
}

FSmartObjectClaimHandle USmartObjectBlueprintFunctionLibrary::GetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key)
{
	UBlackboardComponent* BlackboardComp = UBTFunctionLibrary::GetOwnersBlackboard(NodeOwner);
	return BlackboardComp ? BlackboardComp->GetValue<UBlackboardKeyType_SOClaimHandle>(Key.SelectedKeyName) : FSmartObjectClaimHandle::InvalidHandle;
}
