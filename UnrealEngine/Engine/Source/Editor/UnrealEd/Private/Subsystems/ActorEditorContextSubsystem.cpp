// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/ActorEditorContextSubsystem.h"
#include "GameFramework/Actor.h"
#include "IActorEditorContextClient.h"
#include "ScopedTransaction.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "ActorEditorContext"

UActorEditorContextSubsystem* UActorEditorContextSubsystem::Get()
{ 
	check(GEditor);
	return GEditor->GetEditorSubsystem<UActorEditorContextSubsystem>();
}

void UActorEditorContextSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	GEditor->OnLevelActorAdded().AddUObject(this, &UActorEditorContextSubsystem::ApplyContext);
}

void UActorEditorContextSubsystem::Deinitialize()
{
	GEditor->OnLevelActorAdded().RemoveAll(this);

	Super::Deinitialize();
}

void UActorEditorContextSubsystem::RegisterClient(IActorEditorContextClient* Client)
{
	if (Client && !Clients.Contains(Client))
	{
		Client->GetOnActorEditorContextClientChanged().AddUObject(this, &UActorEditorContextSubsystem::OnActorEditorContextClientChanged);
		Clients.Add(Client);
	}
}

void UActorEditorContextSubsystem::UnregisterClient(IActorEditorContextClient* Client)
{
	if (Client)
	{
		if (Clients.Remove(Client))
		{
			Client->GetOnActorEditorContextClientChanged().RemoveAll(this);
		}
	}
}

void UActorEditorContextSubsystem::ApplyContext(AActor* InActor)
{
	if (GIsReinstancing)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World)
	{
		return;
	}

	if (!InActor || (InActor->GetWorld() != World) || InActor->HasAnyFlags(RF_Transient) || InActor->IsChildActor())
	{
		return;
	}

	for (IActorEditorContextClient* Client : Clients)
	{
		Client->OnExecuteActorEditorContextAction(World, EActorEditorContextAction::ApplyContext, InActor);
	}
}

void UActorEditorContextSubsystem::ResetContext()
{
	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World)
	{
		return;
	}
		
	const FScopedTransaction Transaction(LOCTEXT("Reset Actor Editor Context", "Reset Actor Editor Context"));
	for (IActorEditorContextClient* Client : Clients)
	{
		Client->OnExecuteActorEditorContextAction(World, EActorEditorContextAction::ResetContext);
	}
	ActorEditorContextSubsystemChanged.Broadcast();
}

void UActorEditorContextSubsystem::ResetContext(IActorEditorContextClient* Client)
{
	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World)
	{
		return;
	}

	if (Clients.Contains(Client))
	{
		const FScopedTransaction Transaction(LOCTEXT("Reset Actor Editor Context", "Reset Actor Editor Context"));
		Client->OnExecuteActorEditorContextAction(World, EActorEditorContextAction::ResetContext);
		ActorEditorContextSubsystemChanged.Broadcast();
	}
}

void UActorEditorContextSubsystem::PushContext(bool bDuplicateContext)
{
	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World)
	{
		return;
	}

	for (IActorEditorContextClient* Client : Clients)
	{
		Client->OnExecuteActorEditorContextAction(World, bDuplicateContext ? EActorEditorContextAction::PushDuplicateContext : EActorEditorContextAction::PushContext);
	}
	ActorEditorContextSubsystemChanged.Broadcast();
}

void UActorEditorContextSubsystem::PopContext()
{
	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World)
	{
		return;
	}

	for (IActorEditorContextClient* Client : Clients)
	{
		Client->OnExecuteActorEditorContextAction(World, EActorEditorContextAction::PopContext);
	}
	ActorEditorContextSubsystemChanged.Broadcast();
}

void UActorEditorContextSubsystem::InitializeContextFromActor(AActor* Actor)
{
	UWorld* World = GetWorld();
	if (Clients.IsEmpty() || !World || !Actor)
	{
		return;
	}

	for (IActorEditorContextClient* Client : Clients)
	{
		Client->OnExecuteActorEditorContextAction(World, EActorEditorContextAction::InitializeContextFromActor, Actor);
	}
	ActorEditorContextSubsystemChanged.Broadcast();
}

TArray<IActorEditorContextClient*> UActorEditorContextSubsystem::GetDisplayableClients() const
{
	TArray<IActorEditorContextClient*> DisplayableClients;
	UWorld* World = GetWorld();
	if (!Clients.IsEmpty() && World)
	{
		for (IActorEditorContextClient* Client : Clients)
		{
			FActorEditorContextClientDisplayInfo Info;
			if (Client->GetActorEditorContextDisplayInfo(World, Info))
			{
				DisplayableClients.Add(Client);
			}
		}
	}
	return DisplayableClients;
}

UWorld* UActorEditorContextSubsystem::GetWorld() const
{
	return GEditor->GetEditorWorldContext().World();
}

void UActorEditorContextSubsystem::OnActorEditorContextClientChanged(IActorEditorContextClient* Client)
{
	if (IsRunningGame() || (GIsEditor && GEditor->GetPIEWorldContext()))
	{
		return;
	}

	ActorEditorContextSubsystemChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE