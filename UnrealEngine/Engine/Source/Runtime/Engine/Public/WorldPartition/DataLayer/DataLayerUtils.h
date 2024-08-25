// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "WorldPartition/DataLayer/DataLayerType.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerInstanceNames.h"
#include "WorldPartition/WorldPartitionStreamingGeneration.h"

class UDataLayerManager;
class FDataLayerInstanceDesc;
class FWorldDataLayersActorDesc;
class FWorldPartitionActorDesc;
class UActorDescContainer;
class AWorldDataLayers;

class FDataLayerUtils
{
public:
#if WITH_EDITOR
	static const TCHAR* GetDataLayerIconName(EDataLayerType DataLayerType)
	{
		static constexpr const TCHAR* IconNameByType[static_cast<int>(EDataLayerType::Size)] = { TEXT("DataLayer.Runtime") , TEXT("DataLayer.Editor"), TEXT("") };
		return IconNameByType[static_cast<uint32>(DataLayerType)];
	}

	UE_DEPRECATED(5.4, "Use ResolveDataLayerInstanceNames instead")
	static ENGINE_API TArray<FName> ResolvedDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const FWorldPartitionActorDesc* InActorDesc, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs = TArray<const FWorldDataLayersActorDesc*>())
	{
		return ResolveDataLayerInstanceNames(InDataLayerManager, InActorDesc, InWorldDataLayersActorDescs).ToArray();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	static ENGINE_API bool ResolveRuntimeDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const class FWorldPartitionActorDescView& InActorDescView, const FStreamingGenerationActorDescViewMap& ActorDescViewMap, TArray<FName>& OutRuntimeDataLayerInstanceNames) { return false; }

	UE_DEPRECATED(5.4, "This function is no longer used")
	static ENGINE_API TArray<const FWorldDataLayersActorDesc*> FindWorldDataLayerActorDescs(const FStreamingGenerationActorDescViewMap& ActorDescViewMap) { return TArray<const FWorldDataLayersActorDesc*>(); }

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	static ENGINE_API FDataLayerInstanceNames ResolveDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const FWorldPartitionActorDesc* InActorDesc, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs = TArray<const FWorldDataLayersActorDesc*>());

	static ENGINE_API bool ResolveRuntimeDataLayerInstanceNames(const UDataLayerManager* InDataLayerManager, const IWorldPartitionActorDescInstanceView& InActorDescView, const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, FDataLayerInstanceNames& OutRuntimeDataLayerInstanceNames);

	static ENGINE_API const FDataLayerInstanceDesc* GetDataLayerInstanceDescFromInstanceName(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& DataLayerInstanceName);

	static ENGINE_API const FDataLayerInstanceDesc* GetDataLayerInstanceDescFromAssetPath(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs, const FName& DataLayerAssetPath);

	static ENGINE_API bool AreWorldDataLayersActorDescsSane(const TArray<const FWorldDataLayersActorDesc*>& InWorldDataLayersActorDescs);

	static ENGINE_API FString GenerateUniqueDataLayerShortName(const UDataLayerManager* InDataLayerManager, const FString& InNewShortName);

	static ENGINE_API bool SetDataLayerShortName(UDataLayerInstance* InDataLayerInstance, const FString& InNewShortName);

	static ENGINE_API bool FindDataLayerByShortName(const UDataLayerManager* InDataLayerManager, const FString& InShortName, TSet<UDataLayerInstance*>& OutDataLayerInstances);
#endif

	static FString GetSanitizedDataLayerShortName(FString InShortName)
	{
		return InShortName.TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));
	}
};