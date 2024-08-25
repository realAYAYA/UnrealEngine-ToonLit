// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartitionSettings.generated.h"

UCLASS(config = Engine, defaultconfig, DisplayName = "World Partition")
class ENGINE_API UWorldPartitionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldPartitionSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UWorldPartitionSettings* Get() { return CastChecked<UWorldPartitionSettings>(UWorldPartitionSettings::StaticClass()->GetDefaultObject()); }

	EWorldPartitionDataLayersLogicOperator GetNewMapsDataLayersLogicOperator() const { return NewMapsDataLayersLogicOperator; }
	bool GetNewMapsEnableWorldPartition() const { return bNewMapsEnableWorldPartition; }
	bool GetNewMapsEnableWorldPartitionStreaming() const { return bNewMapsEnableWorldPartitionStreaming; }
	
	TSubclassOf<UWorldPartitionEditorHash> GetEditorHashDefaultClass() const { return EditorHashDefaultClass; }
	TSubclassOf<UWorldPartitionRuntimeHash> GetRuntimeHashDefaultClass() const { return RuntimeHashDefaultClass; }

protected:
	/** Set the default logical operator for actor data layers activation for new maps */
	UPROPERTY(EditAnywhere, Config, Category = WorldPartition)
	EWorldPartitionDataLayersLogicOperator NewMapsDataLayersLogicOperator = EWorldPartitionDataLayersLogicOperator::Or;

	/** Set the default to whether enable world partition for new maps created in the content broswer */
	UPROPERTY(EditAnywhere, Config, Category = WorldPartition)
	bool bNewMapsEnableWorldPartition = false;

	/** Set the default to whether enable world partition streaming for new maps created in the content broswer */
	UPROPERTY(EditAnywhere, Config, Category = WorldPartition)
	bool bNewMapsEnableWorldPartitionStreaming = true;

	/** Set the default editor hash class to use for the editor */
	UPROPERTY(Config)
	TSubclassOf<UWorldPartitionEditorHash> EditorHashDefaultClass;

	/** Set the default runtime hash class to use for new maps  */
	UPROPERTY(EditAnywhere, Config, NoClear, Category = WorldPartition)
	TSubclassOf<UWorldPartitionRuntimeHash> RuntimeHashDefaultClass;
};