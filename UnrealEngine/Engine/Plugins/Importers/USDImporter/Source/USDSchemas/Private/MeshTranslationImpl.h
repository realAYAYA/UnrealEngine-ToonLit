// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if USE_USD_SDK

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "USDIncludesStart.h"
#include "pxr/pxr.h"
#include "USDIncludesEnd.h"

PXR_NAMESPACE_OPEN_SCOPE
	class UsdPrim;
PXR_NAMESPACE_CLOSE_SCOPE

class FUsdInfoCache;
class UMaterialInterface;
class UMeshComponent;
class UUsdAssetCache2;
class UUsdMeshAssetUserData;
namespace UsdUtils
{
	struct FUsdPrimMaterialSlot;
	struct FUsdPrimMaterialAssignmentInfo;
}

/** Implementation that can be shared between the Skeleton translator and GeomMesh translators */
namespace MeshTranslationImpl
{
	/** Resolves the material assignments in AssignmentInfo, returning an UMaterialInterface for each material slot */
	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolveMaterialAssignmentInfo(
		const pxr::UsdPrim& UsdPrim,
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& AssignmentInfo,
		UUsdAssetCache2& AssetCache,
		FUsdInfoCache& InfoCache,
		EObjectFlags Flags,
		bool bReuseIdenticalAssets
	);

	/**
	 * Sets the material overrides on MeshComponent according to the material assignments of the UsdGeomMesh Prim.
	 * Warning: This function will temporarily switch the active LOD variant if one exists, so it's *not* thread safe!
	 */
	void SetMaterialOverrides(
		const pxr::UsdPrim& Prim,
		const TArray<UMaterialInterface*>& ExistingAssignments,
		UMeshComponent& MeshComponent,
		UUsdAssetCache2& AssetCache,
		FUsdInfoCache& InfoCache,
		float Time,
		EObjectFlags Flags,
		bool bInterpretLODs,
		const FName& RenderContext,
		const FName& MaterialPurpose,
		bool bReuseIdenticalAssets
	);

	void RecordSourcePrimsForMaterialSlots(
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
		UUsdMeshAssetUserData* UserData
	);

	enum class EUsdReferenceMaterialProperties
	{
		None = 0,
		Translucent = 1,
		VT = 2,
		TwoSided = 4
	};
	ENUM_CLASS_FLAGS(EUsdReferenceMaterialProperties)

	// Returns one of the alternatives of the UsdPreviewSurface reference material depending on the material overrides
	// provided, and nullptr otherwise
	UMaterialInterface* GetReferencePreviewSurfaceMaterial(EUsdReferenceMaterialProperties ReferenceMaterialProperties);

	// Returns the VT version of the provided UsdPreviewSurface ReferenceMaterial. Returns the provided ReferenceMaterial back if
	// it is already a VT-capable reference material, and returns nullptr if ReferenceMaterial isn't one of our reference material
	// alternatives.
	// Example: Receives UsdPreviewSurfaceTwoSided -> Returns UsdPreviewSurfaceTwoSidedVT
	// Example: Receives UsdPreviewSurfaceTwoSidedVT -> Returns UsdPreviewSurfaceTwoSidedVT
	// Example: Receives SomeOtherReferenceMaterial -> Returns nullptr
	UMaterialInterface* GetVTVersionOfReferencePreviewSurfaceMaterial(UMaterialInterface* ReferenceMaterial);

	// Returns the two-sided version of the provided UsdPreviewSurface ReferenceMaterial. Returns the provided ReferenceMaterial
	// back if it is already a two-sided-capable reference material, and returns nullptr if ReferenceMaterial isn't one of our reference
	// material alternatives.
	// Example: Receives UsdPreviewSurfaceTranslucent -> Returns UsdPreviewSurfaceTwoSidedTranslucent
	// Example: Receives UsdPreviewSurfaceTwoSidedTranslucent -> Returns UsdPreviewSurfaceTwoSidedTranslucent
	// Example: Receives SomeOtherReferenceMaterial -> Returns nullptr
	UMaterialInterface* GetTwoSidedVersionOfReferencePreviewSurfaceMaterial(UMaterialInterface* ReferenceMaterial);

	// Returns whether the Material is one of the UsdPreviewSurface reference materials (which can be reassigned by the
	// user on a per project basis)
	bool IsReferencePreviewSurfaceMaterial(UMaterialInterface* Material);
}	 // namespace MeshTranslationImpl

#endif	  // #if USE_USD_SDK
