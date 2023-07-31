// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/LightWeightInstanceManager.h"
#include "Templates/SharedPointer.h"

#include "Actor.h"

//#include "LightWeightInstanceSubsystem.generated.h"

ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogLightWeightInstance, Log, Warning);

//DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(FActorInstanceHandle, FOnActorReady, FActorInstanceHandle, InHandle);

struct ENGINE_API FLightWeightInstanceSubsystem
{
	friend struct FActorInstanceHandle;
	friend class ALightWeightInstanceManager;

	static FCriticalSection GetFunctionCS;

	static FLightWeightInstanceSubsystem& Get()
	{
		if (!LWISubsystem)
		{
			FScopeLock Lock(&GetFunctionCS);
			if (!LWISubsystem)
			{
				LWISubsystem = MakeShareable(new FLightWeightInstanceSubsystem());
			}
		}
		return *LWISubsystem;
	}

	FLightWeightInstanceSubsystem();
	~FLightWeightInstanceSubsystem();

	// Returns the instance manager that handles the given handle
	ALightWeightInstanceManager* FindLightWeightInstanceManager(const FActorInstanceHandle& Handle) const;

	// Returns the instance manager that handles actors of type ActorClass in level Level
	ALightWeightInstanceManager* FindLightWeightInstanceManager(UClass* ActorClass, const UDataLayerInstance* Layer) const;

	// Returns the instance manager that handles instances of type Class that live in Level
	UFUNCTION(Server, Unreliable)
	ALightWeightInstanceManager* FindOrAddLightWeightInstanceManager(UClass* ActorClass, const UDataLayerInstance* Layer, UWorld* World);

	// Returns the actor specified by Handle. This may require loading and creating the actor object.
	AActor* FetchActor(const FActorInstanceHandle& Handle);

	// Returns the actor specified by Handle if it exists. Returns nullptr if it doesn't
	AActor* GetActor_NoCreate(const FActorInstanceHandle& Handle) const;

	// Returns the class of the actor specified by Handle.
	UClass* GetActorClass(const FActorInstanceHandle& Handle);

	FVector GetLocation(const FActorInstanceHandle& Handle);

	FString GetName(const FActorInstanceHandle& Handle);

	ULevel* GetLevel(const FActorInstanceHandle& Handle);

	// Returns true if the object represented by Handle is in InLevel
	bool IsInLevel(const FActorInstanceHandle& Handle, const ULevel* InLevel);

	// Returns a handle to a new light weight instance that represents an object of type ActorClass
	FActorInstanceHandle CreateNewLightWeightInstance(UClass* ActorClass, FLWIData* InitData, UDataLayerInstance* Layer, UWorld* World);

	// deletes the instance identified by Handle
	void DeleteInstance(const FActorInstanceHandle& Handle);

	// Returns true if the handle can return an object that implements the interface U
	template<typename U>
	bool IsInterfaceSupported(const FActorInstanceHandle& Handle) const
	{
		if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
		{
			return InstanceManager->IsInterfaceSupported<U>();
		}

		return false;
	}

	// Returns an object that implements the interface I for Handle
	template<typename I>
	I* FetchInterfaceObject(const FActorInstanceHandle& Handle)
	{
		if (ALightWeightInstanceManager* InstanceManager = FindLightWeightInstanceManager(Handle))
		{
			return InstanceManager->FetchInterfaceObject<I>(Handle);
		}

		return nullptr;
	}

protected:
	// Returns the class of the instance manager best suited to support instances of type ActorClass
	UClass* FindBestInstanceManagerClass(const UClass* ActorClass);

	// Returns the index associated with Manager
	int32 GetManagerIndex(const ALightWeightInstanceManager* Manager) const;

	// Returns the light weight instance manager at index Index
	const ALightWeightInstanceManager* GetManagerAt(int32 Index) const;

private:
	/** Application singleton */
	static TSharedPtr<FLightWeightInstanceSubsystem, ESPMode::ThreadSafe> LWISubsystem;

	// TODO: preallocate the size of this based on a config variable
	UPROPERTY()
	TArray<ALightWeightInstanceManager*> LWInstanceManagers;

#ifdef WITH_EDITOR
private:
	FDelegateHandle OnLevelActorAddedHandle;
	FDelegateHandle OnLevelActorDeletedHandle;
#endif // WITH_EDITOR
};
