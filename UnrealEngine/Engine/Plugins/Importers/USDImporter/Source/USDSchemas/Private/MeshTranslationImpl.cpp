// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshTranslationImpl.h"

#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDGeomMeshConversion.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDTypesConversion.h"

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/stage.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/xformable.h"
	#include "pxr/usd/usdShade/material.h"
#include "USDIncludesEnd.h"

TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> MeshTranslationImpl::ResolveMaterialAssignmentInfo( const pxr::UsdPrim& UsdPrim, const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& AssignmentInfo, const TArray<UMaterialInterface*>& ExistingAssignments, UUsdAssetCache& AssetCache, float Time, EObjectFlags Flags )
{
	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials;

	uint32 GlobalResolvedMaterialIndex = 0;
	for ( int32 InfoIndex = 0; InfoIndex < AssignmentInfo.Num(); ++InfoIndex )
	{
		const TArray< UsdUtils::FUsdPrimMaterialSlot >& Slots = AssignmentInfo[ InfoIndex ].Slots;

		for ( int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex, ++GlobalResolvedMaterialIndex )
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = Slots[ SlotIndex ];
			UMaterialInterface* Material = nullptr;

			switch ( Slot.AssignmentType )
			{
			case UsdUtils::EPrimAssignmentType::DisplayColor:
			{
				FScopedUsdAllocs Allocs;

				// Try reusing an already created DisplayColor material
				if ( UMaterialInterface* ExistingMaterial = Cast<UMaterialInterface>( AssetCache.GetCachedAsset( Slot.MaterialSource ) ) )
				{
					Material = ExistingMaterial;
				}

				// Need to create a new DisplayColor material
				if ( Material == nullptr )
				{
					if ( TOptional< UsdUtils::FDisplayColorMaterial > DisplayColorDesc = UsdUtils::FDisplayColorMaterial::FromString( Slot.MaterialSource ) )
					{
						UMaterialInstance* MaterialInstance = nullptr;

						if ( GIsEditor )  // Editor, PIE => true; Standlone, packaged => false
						{
							MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceConstant( DisplayColorDesc.GetValue() );
#if WITH_EDITOR
							// Leave PrimPath as empty as it likely will be reused by many prims
							UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MaterialInstance, TEXT( "USDAssetImportData" ) );
							MaterialInstance->AssetImportData = ImportData;
#endif // WITH_EDITOR
						}
						else
						{
							MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceDynamic( DisplayColorDesc.GetValue() );
						}

						AssetCache.CacheAsset( Slot.MaterialSource, MaterialInstance );
						Material = MaterialInstance;
					}
				}

				break;
			}
			case UsdUtils::EPrimAssignmentType::MaterialPrim:
			{
				FScopedUsdAllocs Allocs;

				// Check first or else we may get a warning
				if ( pxr::SdfPath::IsValidPathString( UnrealToUsd::ConvertString( *Slot.MaterialSource ).Get() ) )
				{
					pxr::SdfPath MaterialPrimPath = UnrealToUsd::ConvertPath( *Slot.MaterialSource ).Get();

					// TODO: This may break if MaterialPrimPath targets a prim inside a LOD variant that is disabled...
					TUsdStore< pxr::UsdPrim > MaterialPrim = UsdPrim.GetStage()->GetPrimAtPath( MaterialPrimPath );
					if ( MaterialPrim.Get() )
					{
						Material = Cast< UMaterialInterface >( AssetCache.GetAssetForPrim( UsdToUnreal::ConvertPath( MaterialPrim.Get().GetPrimPath() ) ) );
					}
				}

				break;
			}
			case UsdUtils::EPrimAssignmentType::UnrealMaterial:
			{
				Material = Cast< UMaterialInterface >( FSoftObjectPath( Slot.MaterialSource ).TryLoad() );
				break;
			}
			case UsdUtils::EPrimAssignmentType::None:
			default:
			{
				ensure( false );
				break;
			}
			}

			ResolvedMaterials.Add( &Slot, Material );
		}
	}

	return ResolvedMaterials;
}

void MeshTranslationImpl::SetMaterialOverrides(
	const pxr::UsdPrim& Prim,
	const TArray<UMaterialInterface*>& ExistingAssignments,
	UMeshComponent& MeshComponent,
	UUsdAssetCache& AssetCache,
	float Time,
	EObjectFlags Flags,
	bool bInterpretLODs,
	const FName& RenderContext,
	const FName& MaterialPurpose
)
{
	FScopedUsdAllocs Allocs;

	pxr::UsdGeomMesh Mesh{ Prim };
	if ( !Mesh )
	{
		return;
	}
	pxr::SdfPath PrimPath = Prim.GetPath();
	pxr::UsdStageRefPtr Stage = Prim.GetStage();

	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if ( !RenderContext.IsNone() )
	{
		RenderContextToken = UnrealToUsd::ConvertToken( *RenderContext.ToString() ).Get();
	}

	pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
	if ( !MaterialPurpose.IsNone() )
	{
		MaterialPurposeToken = UnrealToUsd::ConvertToken( *MaterialPurpose.ToString() ).Get();
	}

	TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToAssignments;
	const bool bProvideMaterialIndices = false; // We have no use for material indices and it can be slow to retrieve, as it will iterate all faces

	// Extract material assignment info from prim if it is a LOD mesh
	bool bInterpretedLODs = false;
	if ( bInterpretLODs && UsdUtils::IsGeomMeshALOD( Prim ) )
	{
		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToAssignmentsMap;
		TFunction<bool( const pxr::UsdGeomMesh&, int32 )> IterateLODs = [ & ]( const pxr::UsdGeomMesh& LODMesh, int32 LODIndex )
		{
			UsdUtils::FUsdPrimMaterialAssignmentInfo LODInfo = UsdUtils::GetPrimMaterialAssignments(
				LODMesh.GetPrim(),
				pxr::UsdTimeCode( Time ),
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);
			LODIndexToAssignmentsMap.Add( LODIndex, LODInfo );
			return true;
		};

		pxr::UsdPrim ParentPrim = Prim.GetParent();
		bInterpretedLODs = UsdUtils::IterateLODMeshes( ParentPrim, IterateLODs );

		if ( bInterpretedLODs )
		{
			LODIndexToAssignmentsMap.KeySort( TLess<int32>() );
			for ( TPair<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo>& Entry : LODIndexToAssignmentsMap )
			{
				LODIndexToAssignments.Add( MoveTemp( Entry.Value ) );
			}
		}
	}

	// Refresh reference to Prim because variant switching potentially invalidated it
	pxr::UsdPrim ValidPrim = Stage->GetPrimAtPath( PrimPath );

	// Extract material assignment info from prim if its *not* a LOD mesh, or if we failed to parse LODs
	if ( !bInterpretedLODs )
	{
		LODIndexToAssignments = {
			UsdUtils::GetPrimMaterialAssignments(
				ValidPrim,
				pxr::UsdTimeCode( Time ),
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			)
		};
	}

	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(
		ValidPrim,
		LODIndexToAssignments,
		ExistingAssignments,
		AssetCache,
		Time,
		Flags
	);

	// Compare resolved materials with existing assignments, and create overrides if we need to
	uint32 StaticMeshSlotIndex = 0;
	for ( int32 LODIndex = 0; LODIndex < LODIndexToAssignments.Num(); ++LODIndex )
	{
		const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToAssignments[ LODIndex ].Slots;
		for ( int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++StaticMeshSlotIndex )
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[ LODSlotIndex ];

			UMaterialInterface* Material = nullptr;
			if ( UMaterialInterface** FoundMaterial = ResolvedMaterials.Find( &Slot ) )
			{
				Material = *FoundMaterial;
			}
			else
			{
				UE_LOG( LogUsd, Error, TEXT( "Lost track of resolved material for slot '%d' of LOD '%d' for mesh '%s'" ), LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath( Prim.GetPath() ) );
				continue;
			}

			UMaterialInterface* ExistingMaterial = ExistingAssignments[ StaticMeshSlotIndex ];
			if ( ExistingMaterial == Material )
			{
				continue;
			}
			else
			{
				MeshComponent.SetMaterial( StaticMeshSlotIndex, Material );
			}
		}
	}
}

#endif // #if USE_USD_SDK