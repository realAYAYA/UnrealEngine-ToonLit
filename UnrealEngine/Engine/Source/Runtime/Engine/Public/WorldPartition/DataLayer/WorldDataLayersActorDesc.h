// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/DataLayer/DataLayerType.h"

class ENGINE_API FDataLayerInstanceDesc
{
public:
	FDataLayerInstanceDesc();
	void Init(class UDataLayerInstance* DataLayerInstance);
	friend FArchive& operator<<(FArchive& Ar, FDataLayerInstanceDesc& DataLayerInstanceDesc);
	friend bool operator == (const FDataLayerInstanceDesc& Lhs, const FDataLayerInstanceDesc& Rhs);
	friend bool operator < (const FDataLayerInstanceDesc& Lhs, const FDataLayerInstanceDesc& Rhs);

	FName GetName() const { return Name; }
	FName GetParentName() const { return ParentName; }
	bool IsUsingAsset() const { return bIsUsingAsset; }
	FName GetAssetPath() const { return AssetPath; }
	class UDataLayerAsset* GetAsset() const;
	EDataLayerType GetDataLayerType() const;
	FString GetShortName() const;

private:
	// DataLayerInstance Name
	FName Name;
	// Parent DataLayerInstance Name
	FName ParentName;
	// We can't rely on a valid AssetPath to determine if DataLayerInstance is UDataLayerInstanceWithAsset
	bool bIsUsingAsset;

	//~ Begin UDataLayerInstanceWithAsset
	// DataLayer Asset Path 
	FName AssetPath;
	//~ End UDataLayerInstanceWithAsset

	//~ Begin UDeprecatedDataLayerInstance
	// Runtime or Editor
	bool bIsRuntime;
	// Label
	FString ShortName;
	//~ End UDeprecatedDataLayerInstance
};

/**
 * ActorDesc for AWorldDataLayers actors.
 */
class ENGINE_API FWorldDataLayersActorDesc : public FWorldPartitionActorDesc
{
public:
	FWorldDataLayersActorDesc();
	bool IsValid() const { return bIsValid; }
	const TArray<FDataLayerInstanceDesc>& GetDataLayerInstances() const { return DataLayerInstances; }
	const FDataLayerInstanceDesc* GetDataLayerInstanceFromInstanceName(FName InDataLayerInstanceName) const;
	const FDataLayerInstanceDesc* GetDataLayerInstanceFromAssetPath(FName InDataLayerAssetPath) const;

protected:
	//~ Begin FWorldPartitionActorDesc Interface.
	virtual void Init(const AActor* InActor) override;
	virtual bool Equals(const FWorldPartitionActorDesc* Other) const override;
	virtual void Serialize(FArchive& Ar) override;
	virtual bool IsResaveNeeded() const override { return !IsValid(); }
	virtual bool IsRuntimeRelevant(const FActorContainerID& InContainerID) const override;
	//~ End FWorldPartitionActorDesc Interface.

private:
	TArray<FDataLayerInstanceDesc> DataLayerInstances;
	bool bIsValid;
};
#endif
