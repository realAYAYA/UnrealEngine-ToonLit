// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectBlueprintFunctionLibrary.h"
#include "Engine/Engine.h"
#include "SmartObjectSubsystem.h"
#include "BlackboardKeyType_SOClaimHandle.h"
#include "SmartObjectComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTFunctionLibrary.h"
#include "Types/TargetingSystemTypes.h"

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

FSmartObjectClaimHandle USmartObjectBlueprintFunctionLibrary::SmartObjectClaimHandle_Invalid()
{
	return FSmartObjectClaimHandle::InvalidHandle;
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
	const AActor* UserActor,
	ESmartObjectClaimPriority ClaimPriority)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		return Subsystem->MarkSlotAsClaimed(SlotHandle, ClaimPriority, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
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

bool USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInComponent(const FSmartObjectRequestFilter& Filter, USmartObjectComponent* SmartObjectComponent, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor)
{
	if (SmartObjectComponent)
	{
		if (const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(SmartObjectComponent->GetWorld()))
		{
			return Subsystem->FindSmartObjectsInList(Filter, { SmartObjectComponent->GetOwner() }, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));	
		}
	}

	return false;
}

bool USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInActor(const FSmartObjectRequestFilter& Filter, AActor* SearchActor, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor)
{
	if (SearchActor)
	{
		if (const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(SearchActor->GetWorld()))
    	{
    		return Subsystem->FindSmartObjectsInList(Filter, { SearchActor }, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));	
    	}
	}
	return false;
}

bool USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInTargetingRequest(UObject* WorldContextObject, const FSmartObjectRequestFilter& Filter, const FTargetingRequestHandle TargetingHandle, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor /*= nullptr*/)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		return Subsystem->FindSmartObjectsInTargetingRequest(Filter, TargetingHandle, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}
	return false;
}

bool USmartObjectBlueprintFunctionLibrary::FindSmartObjectsInList(UObject* WorldContextObject, const FSmartObjectRequestFilter& Filter, const TArray<AActor*>& ActorList, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor /*= nullptr*/)
{
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (const USmartObjectSubsystem* Subsystem = USmartObjectSubsystem::GetCurrent(World))
	{
		return Subsystem->FindSmartObjectsInList(Filter, ActorList, OutResults, FConstStructView::Make(FSmartObjectActorUserData(UserActor)));
	}
	return false;
}

FString USmartObjectBlueprintFunctionLibrary::Conv_SmartObjectClaimHandleToString(const FSmartObjectClaimHandle& Result)
{
	return LexToString(Result);
}

FString USmartObjectBlueprintFunctionLibrary::Conv_SmartObjectRequestResultToString(const FSmartObjectRequestResult& Result)
{
	return LexToString(Result);
}

FString USmartObjectBlueprintFunctionLibrary::Conv_SmartObjectDefinitionToString(const USmartObjectDefinition* Definition)
{
	if (Definition)
	{
		return LexToString(*Definition);	
	}

	UE_LOG(LogSmartObject, Error, TEXT("Attempted to convert null SmartObjectDefinition to string!"));
	
	static const FString InvalidDefinitionString = TEXT("INVALID");
	return InvalidDefinitionString;
}

FString USmartObjectBlueprintFunctionLibrary::Conv_SmartObjectHandleToString(const FSmartObjectHandle& Handle)
{
	return LexToString(Handle);
}

bool USmartObjectBlueprintFunctionLibrary::NotEqual_SmartObjectHandleSmartObjectHandle(const FSmartObjectHandle& A, const FSmartObjectHandle& B)
{
	return A != B;
}

bool USmartObjectBlueprintFunctionLibrary::Equal_SmartObjectHandleSmartObjectHandle(const FSmartObjectHandle& A, const FSmartObjectHandle& B)
{
	return A == B;
}

bool USmartObjectBlueprintFunctionLibrary::IsValidSmartObjectHandle(const FSmartObjectHandle& Handle)
{
	return Handle.IsValid();
}

FString USmartObjectBlueprintFunctionLibrary::Conv_SmartObjectSlotHandleToString(const FSmartObjectSlotHandle& Handle)
{
	return LexToString(Handle);
}

bool USmartObjectBlueprintFunctionLibrary::Equal_SmartObjectSlotHandleSmartObjectSlotHandle(const FSmartObjectSlotHandle& A, const FSmartObjectSlotHandle& B)
{
	return A == B;
}

bool USmartObjectBlueprintFunctionLibrary::NotEqual_SmartObjectSlotHandleSmartObjectSlotHandle(const FSmartObjectSlotHandle& A, const FSmartObjectSlotHandle& B)
{
	return A != B;
}

bool USmartObjectBlueprintFunctionLibrary::IsValidSmartObjectSlotHandle(const FSmartObjectSlotHandle& Handle)
{
	return Handle.IsValid();
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
