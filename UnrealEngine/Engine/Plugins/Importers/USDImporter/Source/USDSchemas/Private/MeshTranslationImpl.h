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

class UUsdAssetCache;
class UMaterialInterface;
class UMeshComponent;
namespace UsdUtils
{
	struct FUsdPrimMaterialSlot;
	struct FUsdPrimMaterialAssignmentInfo;
}

/** Implementation that can be shared between the SkelRoot translator and GeomMesh translators */
namespace MeshTranslationImpl
{
	/** Retrieves the target materials described on AssignmentInfo, considering that the previous material assignment on the mesh was ExistingAssignments */
	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolveMaterialAssignmentInfo(
		const pxr::UsdPrim& UsdPrim,
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& AssignmentInfo,
		const TArray<UMaterialInterface*>& ExistingAssignments,
		UUsdAssetCache& AssetCache,
		float Time,
		EObjectFlags Flags
	);

	/**
	 * Sets the material overrides on MeshComponent according to the material assignments of the UsdGeomMesh Prim.
	 * Warning: This function will temporarily switch the active LOD variant if one exists, so it's *not* thread safe!
	 */
	void SetMaterialOverrides(
		const pxr::UsdPrim& Prim,
		const TArray<UMaterialInterface*>& ExistingAssignments,
		UMeshComponent& MeshComponent,
		UUsdAssetCache& AssetCache,
		float Time,
		EObjectFlags Flags,
		bool bInterpretLODs,
		const FName& RenderContext,
		const FName& MaterialPurpose
	);
}

#endif // #if USE_USD_SDK