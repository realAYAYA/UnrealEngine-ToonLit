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
UCLASS()
class UNREALED_API UActorEditorContextSubsystem : public UEditorSubsystem
{
public:

	GENERATED_BODY()

	static UActorEditorContextSubsystem* Get();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterClient(IActorEditorContextClient* Client);
	void UnregisterClient(IActorEditorContextClient* Client);
	void ResetContext();
	void ResetContext(IActorEditorContextClient* Client);
	void PushContext();
	void PopContext();
	TArray<IActorEditorContextClient*> GetDisplayableClients() const;
	FOnActorEditorContextSubsystemChanged& OnActorEditorContextSubsystemChanged() { return ActorEditorContextSubsystemChanged; }

private:

	UWorld* GetWorld() const;
	void OnActorEditorContextClientChanged(IActorEditorContextClient* Client);
	void ApplyContext(AActor* InActor);

	FOnActorEditorContextSubsystemChanged ActorEditorContextSubsystemChanged;
	TArray<IActorEditorContextClient*> Clients;
};