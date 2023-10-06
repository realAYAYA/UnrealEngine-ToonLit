// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif

#include "MaterialOverrideNanite.generated.h"

class UMaterialInterface;
enum EShaderPlatform : uint16;

/**
 * Storage for nanite material override.
 * An override material can be selected, and the override material can be used according to the current settings.
 * We handle removing the override material and its dependencies from the cook on platforms where we can determine 
 * that the override material can never be used.
 */
USTRUCT()
struct FMaterialOverrideNanite
{
	GENERATED_BODY()

	/** 
	 * This will return the cached override material pointer, if the override material is set, or nullptr otherwise.
	 * In a cooked game this will always return nullptr if the platform can't support nanite.
	 */
	UMaterialInterface* GetOverrideMaterial() const
	{
#if WITH_EDITORONLY_DATA
		return OverrideMaterialEditor;
#else
		return OverrideMaterial;
#endif
	}

	/** Setup the object directly. */
	void SetOverrideMaterial(UMaterialInterface* InMaterial, bool bInOverride);

	/** Serialize function as declared in the TStructOpsTypeTraits. */
	bool Serialize(FArchive& Ar);

	/** 
	 * Resolve and fixup anylegacy soft pointer. 
	 * Call this from the owning object's PostLoad(). 
	 * Returns true if any fixup was done.
	 */
	bool FixupLegacySoftReference(UObject* OptionalOwner = nullptr);

	/** 
	 * Stored flag to set whether we apply this override.  
	 * This is useful when evaluating an override along a hierachy of settings.
	 * We default to true to always override.
	 */
	UPROPERTY()
	bool bEnableOverride = true;

#if WITH_EDITORONLY_DATA
	/** 
	 * EditorOnly version of the OverrideMaterial reference.
	 * This is a hard reference, but is editoronly. We rely on -skiponlyeditoronly to avoid pulling this editoronly hard reference into the cook.
	 */
	UPROPERTY(EditAnywhere, Category = Nanite, meta = (DisplayName = "Nanite Override Material"))
	TObjectPtr<UMaterialInterface> OverrideMaterialEditor;
#endif

protected:
	/** 
	 * Reference to our override material.
	 * This is only non-null in cooked packages, and is only non-null for cooked platforms that support nanite.	
	 * Note that we skip default serialization and use special logic inside Serialize().
	 */
	UPROPERTY(SkipSerialization)
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	/** Legacy editor soft reference that has been replaced by OverrideMaterialEditor. */
	UPROPERTY(SkipSerialization)
	TSoftObjectPtr<UMaterialInterface> OverrideMaterialRef;
};

template<>
struct TStructOpsTypeTraits<FMaterialOverrideNanite> : public TStructOpsTypeTraitsBase2<FMaterialOverrideNanite>
{
	enum
	{
		WithSerializer = true,
	};
};
