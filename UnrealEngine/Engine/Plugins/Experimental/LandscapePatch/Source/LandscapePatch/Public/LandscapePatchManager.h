// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LandscapeBlueprintBrushBase.h"

#include "LandscapePatchManager.generated.h"

class ALandscape;
class ULandscapePatchComponent;

/**
 * Acts as the "blueprint brush" as far as the owning edit layer is concerned. In reality, has its contained
 * patches edit the height/weight maps.
 */
//~ The alternative to this approach is to have the individual patches act as independent brushes, which
//~ we currently don't want to do because we think it will clutter the brush interface and may lose opportunities
//~ for optimization... 
UCLASS()
class LANDSCAPEPATCH_API ALandscapePatchManager : public ALandscapeBlueprintBrushBase
{
	GENERATED_BODY()

public:

	ALandscapePatchManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// These get called by the landscape system to apply the patches to the height/weight maps.
	virtual void Initialize_Native(const FTransform& InLandscapeTransform,
		const FIntPoint& InLandscapeSize,
		const FIntPoint& InLandscapeRenderTargetSize) override;
	virtual UTextureRenderTarget2D* Render_Native(bool InIsHeightmap,
		UTextureRenderTarget2D* InCombinedResult,
		const FName& InWeightmapLayerName) override;

	// Adds the brush to the given landscape, removing it from any previous one. This differs from SetOwningLandscape
	// in that SetOwningLandscape is called by the landscape itself from AddBrushToLayer to update the manager.
	virtual void SetTargetLandscape(ALandscape* InOwningLandscape);

	// For use by the owned patch objects.
	/**
	 * Gets the transform from a point in the heightmap (where x and y are pixel coordinates,
	 * aka coordinates of the associated vertex, and z is the height as stored in the height
	 * map, currently a 16 bit integer) to world point based on the current landscape transform.
	 */
	virtual FTransform GetHeightmapCoordsToWorld() { return HeightmapCoordsToWorld; }

	bool ContainsPatch(TObjectPtr<ULandscapePatchComponent> Patch) const;

	void AddPatch(TObjectPtr<ULandscapePatchComponent> Patch);

	bool RemovePatch(TObjectPtr<ULandscapePatchComponent> Patch);

	/** 
	 * Gets the index of a particular patch in the manager's stack of patches (later indices get applied after
	 * earlier ones.
	 */
	int32 GetIndexOfPatch(TObjectPtr<const ULandscapePatchComponent> Patch) const;

	/**
	 * Moves patch to given index in the list of patches held by the manager (so that it is applied at 
	 * a particular time relative to the others).
	 */
	void MovePatchToIndex(TObjectPtr<ULandscapePatchComponent> Patch, int32 Index);
	
#if WITH_EDITOR
	// ALandscapeBlueprintBrushBase
	virtual bool IsAffectingWeightmapLayer(const FName& InLayerName) const override;
	virtual void SetOwningLandscape(class ALandscape* InOwningLandscape) override;

	// UObject
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual bool IsEditorOnly() const override { return true; }
	virtual bool NeedsLoadForClient() const override { return false; }
	virtual bool NeedsLoadForServer() const override { return false; }

protected:
	
	UPROPERTY()
	TArray<TSoftObjectPtr<ULandscapePatchComponent>> PatchComponents;

	UPROPERTY()
	FTransform HeightmapCoordsToWorld;

#if WITH_EDITORONLY_DATA
	//~ This is transient because SetOwningLandscape is called in ALandscape::PostLoad.
	/**
	 * The owning landscape.
	 */
	UPROPERTY(EditAnywhere, Category = Landscape, Transient, meta = (DisplayName = "Landscape"))
	TObjectPtr<ALandscape> DetailPanelLandscape = nullptr;
#endif
};