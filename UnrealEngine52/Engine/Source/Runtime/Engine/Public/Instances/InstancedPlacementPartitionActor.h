// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISMPartition/ISMPartitionActor.h"
#include "Instances/InstancedPlacementClientInfo.h"

#include "InstancedPlacementPartitionActor.generated.h"

class UInstancedPlacementClientSettings;

/**
 * The base class used by any editor placement of instanced objects, which holds any relevant runtime data for the placed instances.
 */
UCLASS()
class ENGINE_API AInstancedPlacementPartitionActor : public AISMPartitionActor
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

#if WITH_EDITOR	
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override;

	void SetGridGuid(const FGuid& InGuid);

	using FClientDescriptorFunc = FClientPlacementInfo::FClientDescriptorFunc;
	FClientPlacementInfo* PreAddClientInstances(const FGuid& ClientGuid, const FString& InClientDisplayString, FClientDescriptorFunc RegisterDefinitionFunc);
	void PostAddClientInstances();
	void NotifySettingsObjectChanged(UInstancedPlacementClientSettings* InSettingsObject);
#endif

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid PlacementGridGuid;

	// Placed Client info by corresponding client Guid
	TMap<FGuid, TUniqueObj<FClientPlacementInfo>> PlacedClientInfo;
#endif
};
