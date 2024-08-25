// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "LandscapeBlueprintBrushBase.h"
#include "LandscapeEditTypes.h"

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

	virtual UTextureRenderTarget2D* RenderLayer_Native(const FLandscapeBrushParameters& InParameters) override;

	// Adds the brush to the given landscape, removing it from any previous one. This differs from SetOwningLandscape
	// in that SetOwningLandscape is called by the landscape itself from AddBrushToLayer to update the manager.
	UFUNCTION(BlueprintCallable, Category = LandscapeManager)
	virtual void SetTargetLandscape(ALandscape* InOwningLandscape);

	// For use by the owned patch objects.
	/**
	 * Gets the transform from a point in the heightmap (where x and y are pixel coordinates,
	 * aka coordinates of the associated vertex, and z is the height as stored in the height
	 * map, currently a 16 bit integer) to world point based on the current landscape transform.
	 */
	virtual FTransform GetHeightmapCoordsToWorld() { return HeightmapCoordsToWorld; }

	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	bool ContainsPatch(ULandscapePatchComponent* Patch) const;

	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	void AddPatch(ULandscapePatchComponent* Patch);

	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	bool RemovePatch(ULandscapePatchComponent* Patch);

	/** 
	 * Gets the index of a particular patch in the manager's stack of patches (later indices get applied after
	 * earlier ones.
	 */
	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	int32 GetIndexOfPatch(const ULandscapePatchComponent* Patch) const;

	/**
	 * Moves patch to given index in the list of patches held by the manager (so that it is applied at 
	 * a particular time relative to the others).
	 */
	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	void MovePatchToIndex(ULandscapePatchComponent* Patch, int32 Index);

#if WITH_EDITOR
	/**
	 * A helper cleanup method to fix things if something goes wrong in saving and owned patches do not have
	 * the correct patch manager pointer back. Public so that it can be called from a console command.
	 */
	void FixOwnedPatchBackPointers();

	/**
	 * Marks that the patch manager was modified during a construction script rerun where it might 
	 * not be able to mark itself dirty (if it was done during loading).
	 */
	void MarkModifiedInConstructionScript();

	/**
	 * Dirties the manager if it was modified in a construction script but was unable to mark itself
	 * dirty. Meant to be used by cleanup commands.
	 */
	void MarkDirtyIfModifiedInConstructionScript();

	// ALandscapeBlueprintBrushBase
	UE_DEPRECATED(5.3, "Use AffectsWeightmapLayer")
	virtual bool IsAffectingWeightmapLayer(const FName& InLayerName) const override;
	virtual bool AffectsWeightmapLayer(const FName& InLayerName) const override;
	virtual bool AffectsVisibilityLayer() const override;
	virtual void SetOwningLandscape(class ALandscape* InOwningLandscape) override;

	// AActor
	virtual void CheckForErrors() override;

	// UObject
	virtual void PostEditUndo() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
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

private:
	bool bIssuedPatchOwnershipWarning = false;

	// The interaction of automatic patch registration and construction script reruns could end
	// up modifying the manager during a load if things had to be fixed up, but the manager might
	// not end up being marked dirty. This is dangerous as it can result in unstable patch ordering,
	// so we want to detect this case.
	bool bDirtiedByConstructionScript = false;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
