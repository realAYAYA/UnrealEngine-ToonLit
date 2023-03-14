// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "WorldPartitionMiniMap.generated.h"

/**
 * A mini map to preview the world in world partition window. (editor-only)
 */
UCLASS(hidecategories = (Actor, Advanced, Display, Events, Object, Attachment, Info, Input, Blueprint, Layers, Tags, Replication, Physics, Cooking), notplaceable)
class ENGINE_API AWorldPartitionMiniMap : public AInfo
{
	GENERATED_BODY()

private:
#if WITH_EDITOR
	virtual bool ActorTypeSupportsDataLayer() const final { return false; }
#endif

public:
	AWorldPartitionMiniMap(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const final { return true; }
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual bool IsLockLocation() const { return true; }
	virtual bool IsUserManaged() const final { return false; }
	virtual void CheckForErrors() override;
	FBox GetMiniMapWorldBounds() const;
	void GetMiniMapResolution(int32& OutMinimapImageSizeX, int32& OutMinimapImageSizeY, int32& OutWorldUnitsPerPixel) const;
#endif

	/* WorldBounds for MinMapTexture */
	UPROPERTY(VisibleAnywhere, Category = WorldPartitionMiniMap, AdvancedDisplay)
	FBox MiniMapWorldBounds;

	/* UVOffset used to setup Virtual Texture */
	UPROPERTY(VisibleAnywhere, Category = WorldPartitionMiniMap, AdvancedDisplay)
	FBox2D UVOffset;

	/* MiniMap Texture for displaying on world partition window */
	UPROPERTY(VisibleAnywhere, Category = WorldPartitionMiniMap)
	TObjectPtr<UTexture2D> MiniMapTexture;

	/* Datalayers excluded from MiniMap rendering */
	UPROPERTY(EditAnywhere, Category = WorldPartitionMiniMap)
	TSet<FActorDataLayer> ExcludedDataLayers;

	/**
	 * Target world units per pixel for the minimap texture. 
	 * May not end up being the final minimap accuracy if the resulting texture resolution is unsupported.
	 */
	UPROPERTY(EditAnywhere, Category = WorldPartitionMiniMap, meta = (UIMin = "10", UIMax = "100000"), AdvancedDisplay)
	int32 WorldUnitsPerPixel;

	/**
	 * Size of the loading region that will be used when iterating over the whole map during the minimap build process.
	 * A smaller size may help reduce blurriness as it will put less pressure on various graphics pools, at the expanse of an increase in processing time. 
	 */
	UPROPERTY(EditAnywhere, Category = WorldPartitionMiniMap, meta = (UIMin = "3200", UIMax = "204800"), AdvancedDisplay)
	int32 BuilderCellSize;

	/* Specifies which component of the scene rendering should be output to the minimap texture. */
	UPROPERTY(EditAnywhere, Category = WorldPartitionMiniMap, AdvancedDisplay)
	TEnumAsByte<ESceneCaptureSource> CaptureSource;

	/* Number of frames to render before each capture in order to warmup various rendering systems (VT/Nanite/etc). */
	UPROPERTY(EditAnywhere, Category = WorldPartitionMiniMap, meta = (UIMin = "0", UIMax = "10"), AdvancedDisplay)
	uint32 CaptureWarmupFrames;

private:
	UPROPERTY()
	int32 MiniMapTileSize_DEPRECATED = 0;
};