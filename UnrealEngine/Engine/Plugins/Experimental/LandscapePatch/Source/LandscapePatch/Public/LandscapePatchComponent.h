// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/SceneComponent.h"

#include "LandscapePatchComponent.generated.h"

class ALandscape;
class ALandscapePatchManager;
class UTextureRenderTarget2D;

/**
 * Base class for landscape patches: components that can be attached to meshes and moved around to make
 * the meshes affect the landscape around themselves.
 */
//~ TODO: Although this doesn't generate geometry, we are likely to change this to inherit from UPrimitiveComponent
//~ so that we can use render proxies for passing along data to the render thread or perhaps for visualization.
UCLASS(Blueprintable, BlueprintType, Abstract)
class LANDSCAPEPATCH_API ULandscapePatchComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	ULandscapePatchComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Initialize_Native(const FTransform& InLandscapeTransform,
		const FIntPoint& InLandscapeSize,
		const FIntPoint& InLandscapeRenderTargetSize) {}
	virtual UTextureRenderTarget2D* Render_Native(bool InIsHeightmap,
		UTextureRenderTarget2D* InCombinedResult,
		const FName& InWeightmapLayerName) { return InCombinedResult; }

	virtual bool IsAffectingWeightmapLayer(const FName& InLayerName) const { return false; }

	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	void RequestLandscapeUpdate();

	/**
	 * Allows the patch to be disabled, so that it no longer affects the landscape. This can be useful
	 * when deleting the patch is undesirable, usually when the disabling is temporary.
	 */
	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	void SetIsEnabled(bool bEnabledIn);

	/**
	 * @return false if the patch is marked as disabled and therefore can't affect the landscape.
	 */
	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	virtual bool IsEnabled() const { return bIsEnabled; }

	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	FTransform GetLandscapeHeightmapCoordsToWorld() const;

	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	virtual void SetLandscape(ALandscape* NewLandscape);

	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	virtual void SetPatchManager(ALandscapePatchManager* NewPatchManager);

	UFUNCTION(BlueprintCallable, Category = "LandscapePatch")
	virtual ALandscapePatchManager* GetPatchManager() const;

	void ApplyComponentInstanceData(struct FLandscapePatchComponentInstanceData* ComponentInstanceData);

	// For now we keep the patches largely editor-only, since we don't yet support runtime landscape editing.
	// The above functions are also editor-only (and don't work at runtime), but can't be in WITH_EDITOR blocks
	// so that they can be called from non-editor-only classes in editor contexts.
#if WITH_EDITOR

	// USceneComponent
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;

	// UActorComponent
	virtual void OnComponentCreated() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;
	virtual void GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;

	// UObject
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsPostLoadThreadSafe() const override { return true; }
	virtual void PostLoad();
#endif
	virtual bool IsEditorOnly() const override { return true; }
	virtual bool NeedsLoadForClient() const override { return false; }
	virtual bool NeedsLoadForServer() const override { return false; }

protected:
	/**
	 * Move the patch to be the last processed patch in the current patch manager. This is a way to
	 * reorder patches relative to each other.
	 */
	UFUNCTION(CallInEditor, Category = Initialization)
	void MoveToTop();

	UPROPERTY(EditAnywhere, Category = Settings)
	TSoftObjectPtr<ALandscape> Landscape = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	TSoftObjectPtr<ALandscapePatchManager> PatchManager = nullptr;

	/**
	 * When false, patch does not affect the landscape. Useful for temporarily disabling the patch.
	 */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bIsEnabled = true;

	// Determines whether the height patch was made by copying a different height patch.
	bool bWasCopy = false;

	// This is true for existing height patches right after they are loaded, so that we can ignore
	// the first OnRegister call. It remains false from the first OnRegiter call onward, even
	// if the component is unregistered.
	bool bLoadedButNotYetRegistered = false;
private:
	// Starts as false and gets set to true in construction, so gets used to set bWasCopy
	// by checking the indicator value at the start of construction.
	UPROPERTY()
	bool bPropertiesCopiedIndicator = false;

	// Used to properly transition to a different manager when editing it via the detail panel.
	UPROPERTY()
	TSoftObjectPtr<ALandscapePatchManager> PreviousPatchManager = nullptr;

#if WITH_EDITOR
	// Used to avoid spamming warning messages
	bool bGaveMissingPatchManagerWarning = false;
	bool bGaveNotInPatchManagerWarning = false;
	bool bGaveMissingLandscapeWarning = false;
	void ResetWarnings();
#endif

	friend struct FLandscapePatchComponentInstanceData;
};

/** Used to store some extra data during RerunConstructionScripts, namely the component's position in the patch manager. */
USTRUCT()
struct FLandscapePatchComponentInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()

	FLandscapePatchComponentInstanceData() = default;
	FLandscapePatchComponentInstanceData(const ULandscapePatchComponent* SourceComponent);

	virtual ~FLandscapePatchComponentInstanceData() = default;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		Super::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<ULandscapePatchComponent>(Component)->ApplyComponentInstanceData(this);
	}

	UPROPERTY()
	int32 IndexInManager = -1;

#if WITH_EDITORONLY_DATA

	// Used so that we don't spam warning messages while rerunning construction scripts on a patch
	// that triggers one of the warnings.
	UPROPERTY()
	bool bGaveMissingPatchManagerWarning = false;
	UPROPERTY()
	bool bGaveNotInPatchManagerWarning = false;
	UPROPERTY()
	bool bGaveMissingLandscapeWarning = false;
#endif
};