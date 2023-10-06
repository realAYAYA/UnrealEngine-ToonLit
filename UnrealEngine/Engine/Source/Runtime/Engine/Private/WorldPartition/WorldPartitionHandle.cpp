// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/ActorDescContainerCollection.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#if WITH_EDITOR
/**
* FWorldPartitionImplBase
*/
TUniquePtr<FWorldPartitionActorDesc>* FWorldPartitionImplBase::GetActorDesc(UActorDescContainer* Container, const FGuid& ActorGuid)
{
	if (TUniquePtr<FWorldPartitionActorDesc>** ActorDescPtr = Container->ActorsByGuid.Find(ActorGuid))
	{
		return *ActorDescPtr;
	}

	return nullptr;
}

UActorDescContainer* FWorldPartitionImplBase::GetActorDescContainer(TUniquePtr<FWorldPartitionActorDesc>* ActorDesc)
{
	return ActorDesc ? ActorDesc->Get()->GetContainer() : nullptr;
}

bool FWorldPartitionImplBase::IsActorDescLoaded(FWorldPartitionActorDesc* ActorDesc)
{
	return ActorDesc->IsLoaded();
}

/**
* FWorldPartitionLoadingContext
*/
FWorldPartitionLoadingContext::FImmediate FWorldPartitionLoadingContext::DefaultContext;
FWorldPartitionLoadingContext::IContext* FWorldPartitionLoadingContext::ActiveContext = &DefaultContext;

void FWorldPartitionLoadingContext::LoadAndRegisterActor(FWorldPartitionActorDesc* ActorDesc)
{
#if DO_CHECK
	FWorldPartitionActorDesc::FRegisteringUnregisteringGuard Guard(ActorDesc);
#endif

	ActiveContext->RegisterActor(ActorDesc);
}

void FWorldPartitionLoadingContext::UnloadAndUnregisterActor(FWorldPartitionActorDesc* ActorDesc)
{
#if DO_CHECK
	FWorldPartitionActorDesc::FRegisteringUnregisteringGuard Guard(ActorDesc);
#endif

	ActiveContext->UnregisterActor(ActorDesc);
}

/**
* IContext
*/
FWorldPartitionLoadingContext::IContext::IContext()
{
	check(ActiveContext == &DefaultContext);
	ActiveContext = this;
}

FWorldPartitionLoadingContext::IContext::~IContext()
{
	check(ActiveContext == this);
	ActiveContext = &DefaultContext;
}

/**
* FImmediate
*/
void FWorldPartitionLoadingContext::FImmediate::RegisterActor(FWorldPartitionActorDesc* ActorDesc)
{
	TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);

	if (AActor* Actor = ActorDesc->Load())
	{
		UActorDescContainer* Container = ActorDesc->GetContainer();
		check(Container);

		const FTransform& ContainerTransform = Container->GetInstanceTransform();
		const FTransform* ContainerTransformPtr = ContainerTransform.Equals(FTransform::Identity) ? nullptr : &ContainerTransform;

		Actor->GetLevel()->AddLoadedActor(Actor, ContainerTransformPtr);
	}
}

void FWorldPartitionLoadingContext::FImmediate::UnregisterActor(FWorldPartitionActorDesc* ActorDesc)
{
	// Set GIsEditorLoadingPackage to avoid dirtying the Actor package if Modify() is called during the unload sequence
	TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);

	// When cleaning up worlds, actors are already marked as garbage at this point, so no need to remove them from the world
	if (AActor* Actor = ActorDesc->GetActor(); IsValid(Actor))
	{
		UActorDescContainer* Container = ActorDesc->GetContainer();
		check(Container);

		const FTransform& ContainerTransform = Container->GetInstanceTransform();
		const FTransform* ContainerTransformPtr = ContainerTransform.Equals(FTransform::Identity) ? nullptr : &ContainerTransform;

		Actor->GetLevel()->RemoveLoadedActor(Actor, ContainerTransformPtr);

		ActorDesc->Unload();
	}	
}

/**
* FDeferred
*/
FWorldPartitionLoadingContext::FDeferred::~FDeferred()
{
	TMap<UActorDescContainer*, FContainerOps> LocalContainerOps = MoveTemp(ContainerOps);

	while(LocalContainerOps.Num())
	{
		for (auto& [Container, ContainerOp] : LocalContainerOps)
		{
			const FTransform& ContainerTransform = Container->GetInstanceTransform();
			const FTransform* ContainerTransformPtr = ContainerTransform.Equals(FTransform::Identity) ? nullptr : &ContainerTransform;

			auto CreateActorList = [](TArray<AActor*>& ActorList, const TSet<FWorldPartitionActorDesc*>& SourceList) -> ULevel*
			{
				ActorList.Empty(SourceList.Num());

				ULevel* Level = nullptr;
				for (FWorldPartitionActorDesc* ActorDesc : SourceList)
				{
					// When cleaning up worlds, actors are already marked as garbage at this point, so no need to remove them from the world
					if (AActor* Actor = ActorDesc->GetActor(); IsValid(Actor))
					{
						ActorList.Add(Actor);

						ULevel* ActorLevel = Actor->GetLevel();
						check(!Level || (Level == ActorLevel));
						Level = ActorLevel;
					}
				}

				return Level;
			};

			if (ContainerOp.Registrations.Num())
			{
				TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);

				TArray<AActor*> ActorList;
				if (ULevel* Level = CreateActorList(ActorList, ContainerOp.Registrations))
				{
					Level->AddLoadedActors(ActorList, ContainerTransformPtr);
				}
			}

			if (ContainerOp.Unregistrations.Num())
			{
				// Set GIsEditorLoadingPackage to avoid dirtying the Actor package if Modify() is called during the unload sequence
				TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);

				TArray<AActor*> ActorList;
				if (ULevel* Level = CreateActorList(ActorList, ContainerOp.Unregistrations))
				{
					Level->RemoveLoadedActors(ActorList, ContainerTransformPtr);
				}

				for (FWorldPartitionActorDesc* ActorDesc : ContainerOp.Unregistrations)
				{
					ActorDesc->Unload();
				}
			}
		}

		// Continue with potentially new registrations/unregistrations that may have happenned during previous cycle
		LocalContainerOps = MoveTemp(ContainerOps);
	}
}

void FWorldPartitionLoadingContext::FDeferred::RegisterActor(FWorldPartitionActorDesc* ActorDesc)
{
	TGuardValue<bool> IsEditorLoadingPackageGuard(GIsEditorLoadingPackage, true);

	check(ActorDesc);
	if (ActorDesc->Load())
	{
		UActorDescContainer* Container = ActorDesc->GetContainer();
		check(Container);

		bool bIsAlreadyInSetPtr;
		ContainerOps.FindOrAdd(Container).Registrations.Add(ActorDesc, &bIsAlreadyInSetPtr);
		check(!bIsAlreadyInSetPtr);
		NumRegistrations++;

		check(!ContainerOps.FindChecked(Container).Unregistrations.Contains(ActorDesc));
	}
}

void FWorldPartitionLoadingContext::FDeferred::UnregisterActor(FWorldPartitionActorDesc* ActorDesc)
{
	check(ActorDesc);
	if (AActor* Actor = ActorDesc->GetActor(); IsValid(Actor))
	{
		UActorDescContainer* Container = ActorDesc->GetContainer();
		check(Container);

		bool bIsAlreadyInSetPtr;
		ContainerOps.FindOrAdd(Container).Unregistrations.Add(ActorDesc, &bIsAlreadyInSetPtr);
		check(!bIsAlreadyInSetPtr);
		NumUnregistrations++;

		check(!ContainerOps.FindChecked(Container).Registrations.Contains(ActorDesc));

		bNeedsClearTransactions |= Actor->GetTypedOuter<UWorld>()->HasAnyFlags(RF_Transactional);
	}
}

/**
* FWorldPartitionHandleImpl
*/
void FWorldPartitionHandleImpl::IncRefCount(FWorldPartitionActorDesc* ActorDesc)
{
	ActorDesc->IncSoftRefCount();
}

void FWorldPartitionHandleImpl::DecRefCount(FWorldPartitionActorDesc* ActorDesc)
{
	ActorDesc->DecSoftRefCount();
}

TWorldPartitionHandle<FWorldPartitionReferenceImpl> FWorldPartitionHandleImpl::ToReference(const TWorldPartitionHandle<FWorldPartitionHandleImpl>& Source)
{
	TWorldPartitionHandle<FWorldPartitionReferenceImpl> Result;
	Result = Source;
	return Result;
}

/**
* FWorldPartitionReferenceImpl
*/
void FWorldPartitionReferenceImpl::IncRefCount(FWorldPartitionActorDesc* ActorDesc)
{
	if (ActorDesc->IncHardRefCount() == 1)
	{
		FWorldPartitionLoadingContext::LoadAndRegisterActor(ActorDesc);
	}
}

void FWorldPartitionReferenceImpl::DecRefCount(FWorldPartitionActorDesc* ActorDesc)
{
	if (ActorDesc->DecHardRefCount() == 0)
	{
		FWorldPartitionLoadingContext::UnloadAndUnregisterActor(ActorDesc);
	}
}

TWorldPartitionHandle<FWorldPartitionHandleImpl> FWorldPartitionReferenceImpl::ToHandle(const TWorldPartitionHandle<FWorldPartitionReferenceImpl>& Source)
{
	TWorldPartitionHandle<FWorldPartitionHandleImpl> Result;
	Result = Source;
	return Result;
}
#endif
