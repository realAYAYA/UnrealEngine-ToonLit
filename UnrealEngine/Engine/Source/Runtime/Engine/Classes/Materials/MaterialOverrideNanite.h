// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "MaterialOverrideNanite.generated.h"

class ITargetPlatform;
class UMaterialInterface;

/**
 * Storage for nanite material override.
 * An override material can be selected, and the override material can be used according to the current settings.
 * We handle removing the override material and its dependencies from the cook on platforms where we can determine 
 * that the override material can never be used.
 */
USTRUCT(BlueprintType)
struct FMaterialOverrideNanite
{
	GENERATED_BODY()

	/** An override material which will be used when rendering with nanite. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Nanite, meta = (DisplayName = "Nanite Override Material"))
	TSoftObjectPtr<UMaterialInterface> OverrideMaterialRef;

	/** 
	 * Stored flag to set whether we apply this override.  
	 * This is useful when evaluating an override along a hierachy of settings.
	 * We default to true to always override.
	 */
	UPROPERTY()
	bool bEnableOverride = true;

	/** 
	 * This will return the cached override material pointer, if the override material is set and enabled, or nullptr otherwise.
	 * In a cooked game this will always return nullptr if the platform can't support nanite.
	 */
	UMaterialInterface* GetOverrideMaterial()
	{
#if WITH_EDITOR
		if (bIsRefreshRequested)
		{
			RefreshOverrideMaterial();
			bIsRefreshRequested = false;
		}
#endif
		return OverrideMaterial;
	}

	/** Serialize function as declared in the TStructOpsTypeTraits. */
	bool Serialize(FArchive& Ar);
	
	/** Call this from the owning object's PostLoad(). */
	void PostLoad();

#if WITH_EDITOR
	/** Call this from the owning object on edit changes. */
	void PostEditChange();
	/** Initialize the cached override material pointer according to platform support. Call this from the owning object's BeginCacheForCookedPlatformData(). */
	void LoadOverrideForPlatform(const ITargetPlatform* TargetPlatform);
	/** Clear the cached override material pointer. Call this from the owning object's ClearAllCachedCookedPlatformData() */
	void ClearOverride();
#endif

protected:
	/** Cached hard reference to override material which is only created if necessary. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	/** Return true if the platform can ever use the override. */
	bool CanUseOverride(EShaderPlatform ShaderPlatform) const;

#if WITH_EDITOR
	/** Set true if we need to call RefreshOverrideMaterial() before next access. */
	bool bIsRefreshRequested = false;

	/** Refresh the cached hard reference to the override material. */
	void RefreshOverrideMaterial();
#endif
};

template<>
struct TStructOpsTypeTraits<FMaterialOverrideNanite> : public TStructOpsTypeTraitsBase2<FMaterialOverrideNanite>
{
	enum
	{
		WithSerializer = true,
	};
};
