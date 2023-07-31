// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Misc/Optional.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartitionActorLoaderInterface.generated.h"

UINTERFACE()
class ENGINE_API UWorldPartitionActorLoaderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ENGINE_API IWorldPartitionActorLoaderInterface
{
	GENERATED_IINTERFACE_BODY()

#if WITH_EDITOR
public:
	/** Base class for actor loaders */
	class ENGINE_API ILoaderAdapter
	{
	public:
		ILoaderAdapter(UWorld* InWorld);
		virtual ~ILoaderAdapter();

		void Load();
		void Unload();
		bool IsLoaded() const;

		UWorld* GetWorld() const { return World; }

		bool GetUserCreated() const { return bUserCreated; }
		void SetUserCreated(bool bValue) { bUserCreated = bValue; }

		// Public interface
		virtual TOptional<FBox> GetBoundingBox() const { return TOptional<FBox>(); }
		virtual TOptional<FString> GetLabel() const { return TOptional<FString>(); }
		virtual TOptional<FColor> GetColor() const { return TOptional<FColor>(); }

		void OnActorDescContainerInitialize(UActorDescContainer* Container);
		void OnActorDescContainerUninitialize(UActorDescContainer* Container);

	protected:
		// Private interface
		virtual void ForEachActor(TFunctionRef<void(const FWorldPartitionHandle&)> InOperation) const =0;
		virtual bool ShouldActorBeLoaded(const FWorldPartitionHandle& Actor) const;

		void RegisterDelegates();
		void UnregisterDelegates();

		// Actors filtering
		bool PassActorDescFilter(const FWorldPartitionHandle& Actor) const;
		bool PassDataLayersFilter(const FWorldPartitionHandle& Actor) const;
		void RefreshLoadedState();

		void PostLoadedStateChanged(int32 NumLoads, int32 NumUnloads);
		void AddReferenceToActor(FWorldPartitionHandle& Actor);
		void RemoveReferenceToActor(FWorldPartitionHandle& Actor);
		void OnActorDataLayersEditorLoadingStateChanged(bool bFromUserOperation);		

	private:
		UWorld* World;

		uint8 bLoaded : 1;
		uint8 bUserCreated : 1;

		TMap<FGuid, TMap<FGuid, FWorldPartitionReference>> ActorReferences;
	};

	virtual ILoaderAdapter* GetLoaderAdapter() =0;
#endif
};