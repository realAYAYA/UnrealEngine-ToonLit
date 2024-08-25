// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"

#include "ActorEditorContextSubsystem.generated.h"

struct IActorEditorContextClient;
class AActor;

DECLARE_MULTICAST_DELEGATE(FOnActorEditorContextSubsystemChanged);

/**
* UActorEditorContextSubsystem 
*/
UCLASS(MinimalAPI)
class UActorEditorContextSubsystem : public UEditorSubsystem
{
public:

	GENERATED_BODY()

	static UNREALED_API UActorEditorContextSubsystem* Get();

	UNREALED_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UNREALED_API virtual void Deinitialize() override;

	UNREALED_API void RegisterClient(IActorEditorContextClient* Client);
	UNREALED_API void UnregisterClient(IActorEditorContextClient* Client);
	UNREALED_API void ResetContext();
	UNREALED_API void ResetContext(IActorEditorContextClient* Client);
	UNREALED_API void PushContext(bool bDuplicateContext = false);
	UNREALED_API void PopContext();
	UNREALED_API void InitializeContextFromActor(AActor* Actor);
	UNREALED_API TArray<IActorEditorContextClient*> GetDisplayableClients() const;
	FOnActorEditorContextSubsystemChanged& OnActorEditorContextSubsystemChanged() { return ActorEditorContextSubsystemChanged; }

private:

	UNREALED_API UWorld* GetWorld() const;
	UNREALED_API void OnActorEditorContextClientChanged(IActorEditorContextClient* Client);
	UNREALED_API void ApplyContext(AActor* InActor);

	FOnActorEditorContextSubsystemChanged ActorEditorContextSubsystemChanged;
	TArray<IActorEditorContextClient*> Clients;
};
