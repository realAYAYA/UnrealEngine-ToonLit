// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#include "Containers/Map.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/Filter/WorldPartitionActorFilter.h"

class ULevelInstanceSubsystem;
class UActorDescContainer;
class IStreamingGenerationErrorHandler;
enum class ELevelInstanceRuntimeBehavior : uint8;

/**
 * ActorDesc for Actors that are part of a LevelInstanceActor Level.
 */
class FLevelInstanceActorDesc : public FWorldPartitionActorDesc
{
public:
	ENGINE_API FLevelInstanceActorDesc();
	ENGINE_API virtual ~FLevelInstanceActorDesc() override;

	ENGINE_API virtual bool IsContainerInstance() const override;
	virtual EWorldPartitionActorFilterType GetContainerFilterType() const { return IsContainerInstance() ? EWorldPartitionActorFilterType::Loading : EWorldPartitionActorFilterType::None; }
	virtual FName GetContainerPackage() const override { return WorldAsset.GetLongPackageFName(); }
	ENGINE_API virtual bool GetContainerInstance(FContainerInstance& OutContainerInstance) const override;
	virtual const FWorldPartitionActorFilter* GetContainerFilter() const override { return &Filter; }
	ENGINE_API virtual void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const override;

protected:
	ENGINE_API virtual void Init(const AActor* InActor) override;
	ENGINE_API virtual void Init(const FWorldPartitionActorDescInitData& DescData) override;
	ENGINE_API virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	ENGINE_API virtual void TransferFrom(const FWorldPartitionActorDesc* From) override;
	virtual uint32 GetSizeOf() const override { return sizeof(FLevelInstanceActorDesc); }
	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void SetContainer(UActorDescContainer* InContainer, UWorld* InWorldContext) override;

	FSoftObjectPath WorldAsset;
	FTransform LevelInstanceTransform;
	ELevelInstanceRuntimeBehavior DesiredRuntimeBehavior;

	TWeakObjectPtr<UActorDescContainer> LevelInstanceContainer;
	TWeakObjectPtr<UWorld> LevelInstanceContainerWorldContext;

	FWorldPartitionActorFilter Filter;
	bool bIsContainerInstance;
private:
	ENGINE_API bool IsContainerInstanceInternal() const;
	ENGINE_API void RegisterContainerInstance(UWorld* InWorldContext);
	ENGINE_API void UnregisterContainerInstance();
	ENGINE_API void UpdateBounds();
};
#endif
