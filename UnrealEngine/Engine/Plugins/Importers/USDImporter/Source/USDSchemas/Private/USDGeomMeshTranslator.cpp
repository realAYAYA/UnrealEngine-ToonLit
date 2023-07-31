// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDGeomMeshTranslator.h"

#if USE_USD_SDK

#include "MeshTranslationImpl.h"
#include "UnrealUSDWrapper.h"
#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDGroomTranslatorUtils.h"
#include "USDIntegrationUtils.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Async/Async.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "GroomComponent.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "RHI.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "GeometryCacheTrackUSD.h"
#include "GeometryCacheUSDComponent.h"
#include "IMeshBuilderModule.h"
#endif // WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/usd/usd/editContext.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/stage.h"
	#include "pxr/usd/usd/typed.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/xformable.h"
	#include "pxr/usd/usdShade/material.h"
#include "USDIncludesEnd.h"

#if WITH_EDITOR  // The GeometryCacheStreamer module is editor-only
// Can toggle on/off to compare performance with StaticMesh instead of GeometryCache
static bool GUseGeometryCacheUSD = true;

static FAutoConsoleVariableRef CVarUsdUseGeometryCache(
	TEXT("USD.UseGeometryCache"),
	GUseGeometryCacheUSD,
	TEXT("Use GeometryCache instead of static meshes for loading animated meshes"));

#endif // WITH_EDITOR

static float GMeshNormalRepairThreshold = 0.05f;
static FAutoConsoleVariableRef CVarMeshNormalRepairThreshold(
	TEXT( "USD.MeshNormalRepairThreshold" ),
	GMeshNormalRepairThreshold,
	TEXT( "We will try repairing up to this fraction of a Mesh's normals when invalid. If a Mesh has more invalid normals than this, we will recompute all of them. Defaults to 0.05 (5% of all normals)." ) );

namespace UsdGeomMeshTranslatorImpl
{
	bool ShouldEnableNanite(
		const TArray<FMeshDescription>& LODIndexToMeshDescription,
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
		const FUsdSchemaTranslationContext& Context,
		const UE::FUsdPrim& Prim
	)
	{
		if ( LODIndexToMeshDescription.Num() < 1 )
		{
			return false;
		}

		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrim UsdPrim{ Prim };
		if ( !UsdPrim )
		{
			return false;
		}

		bool bHasNaniteOverrideEnabled = false;

		// We want Nanite because of an override on Prim
		if ( pxr::UsdAttribute NaniteOverride = UsdPrim.GetAttribute( UnrealIdentifiers::UnrealNaniteOverride ) )
		{
			pxr::TfToken OverrideValue;
			if ( NaniteOverride.Get( &OverrideValue ) )
			{
				if ( OverrideValue == UnrealIdentifiers::UnrealNaniteOverrideEnable )
				{
					bHasNaniteOverrideEnabled = true;
					UE_LOG( LogUsd, Log, TEXT( "Trying to enable Nanite for mesh generated for prim '%s' as the '%s' attribute is set to '%s'" ),
						*Prim.GetPrimPath().GetString(),
						*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverride ),
						*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverrideEnable )
					);

				}
				else if ( OverrideValue == UnrealIdentifiers::UnrealNaniteOverrideDisable )
				{
					UE_LOG( LogUsd, Log, TEXT( "Not enabling Nanite for mesh generated for prim '%s' as the '%s' attribute is set to '%s'" ),
						*Prim.GetPrimPath().GetString(),
						*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverride ),
						*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverrideDisable )
					);
					return false;
				}
			}
		}

		// We want Nanite because the mesh is large enough for the threshold, which is set to something valid
		if ( !bHasNaniteOverrideEnabled )
		{
			const int32 NumTriangles = LODIndexToMeshDescription[ 0 ].Triangles().Num();
			if ( NumTriangles >= Context.NaniteTriangleThreshold )
			{
				UE_LOG( LogUsd, Verbose, TEXT( "Trying to enable Nanite for mesh generated for prim '%s' as it has '%d' triangles, and the threshold is '%d'" ),
					*Prim.GetPrimPath().GetString(),
					NumTriangles,
					Context.NaniteTriangleThreshold
				);
			}
			else
			{
				UE_LOG( LogUsd, Verbose, TEXT( "Not enabling Nanite for mesh generated for prim '%s' as it has '%d' triangles, and the threshold is '%d'" ),
					*Prim.GetPrimPath().GetString(),
					NumTriangles,
					Context.NaniteTriangleThreshold
				);
				return false;
			}
		}

		// Don't enable Nanite if we have more than one LOD. This means the Mesh came from the LOD variant set setup, and
		// we're considering the LOD setup "stronger" than the Nanite override: If you have all that LOD variant set situation you
		// likely don't want Nanite for one of the LOD meshes anyway, as that doesn't really make any sense.
		// If the user wants to have Nanite within the variant set all they would otherwise need is to name the variant set something
		// else other than LOD.
		if ( LODIndexToMeshDescription.Num() > 1 )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Not enabling Nanite for mesh generated for prim '%s' as it has more than one generated LOD (and so came from a LOD variant set setup)" ),
				*Prim.GetPrimPath().GetString()
			);
			return false;
		}

		// We cannot enable Nanite here due to a limit on the number of material slots
		if ( LODIndexToMaterialInfo.Num() > 0 )
		{
			const int32 NumSections = LODIndexToMaterialInfo[0].Slots.Num();
			const int32 MaxNumSections = 64; // There is no define for this, but it's checked for on NaniteBuilder.cpp, FBuilderModule::Build
			if ( NumSections > MaxNumSections )
			{
				UE_LOG( LogUsd, Warning, TEXT( "Not enabling Nanite for mesh generated for prim '%s' as LOD0 has '%d' material slots, which is above the Nanite limit of '%d'" ),
					*Prim.GetPrimPath().GetString(),
					NumSections,
					MaxNumSections
				);
				return false;
			}
		}

#if !WITH_EDITOR
		UE_LOG( LogUsd, Warning, TEXT( "Not enabling Nanite for mesh generated for prim '%s' as we can't setup Nanite during runtime" ),
			*Prim.GetPrimPath().GetString()
		);
		return false;
#endif

		return true;
	}

	bool IsAnimated( const pxr::UsdPrim& Prim )
	{
		FScopedUsdAllocs UsdAllocs;

		bool bHasAttributesTimeSamples = false;
		{
			constexpr bool bIncludeInherited = false;
			pxr::TfTokenVector GeomMeshAttributeNames = pxr::UsdGeomMesh::GetSchemaAttributeNames( bIncludeInherited );
			pxr::TfTokenVector GeomPointBasedAttributeNames = pxr::UsdGeomPointBased::GetSchemaAttributeNames( bIncludeInherited );

			GeomMeshAttributeNames.reserve( GeomMeshAttributeNames.size() + GeomPointBasedAttributeNames.size() );
			GeomMeshAttributeNames.insert( GeomMeshAttributeNames.end(), GeomPointBasedAttributeNames.begin(), GeomPointBasedAttributeNames.end() );

			for ( const pxr::TfToken& AttributeName : GeomMeshAttributeNames )
			{
				const pxr::UsdAttribute& Attribute = Prim.GetAttribute( AttributeName );

				if ( Attribute && Attribute.ValueMightBeTimeVarying() )
				{
					bHasAttributesTimeSamples = true;
					break;
				}
			}
		}

		return bHasAttributesTimeSamples;
	}

	/** Returns true if material infos have changed on the StaticMesh */
	bool ProcessMaterials( const pxr::UsdPrim& UsdPrim, const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo, UStaticMesh& StaticMesh, UUsdAssetCache& AssetCache, float Time, EObjectFlags Flags )
	{
		bool bMaterialAssignementsHaveChanged = false;

		TArray<UMaterialInterface*> ExistingAssignments;
		for ( const FStaticMaterial& StaticMaterial : StaticMesh.GetStaticMaterials() )
		{
			ExistingAssignments.Add(StaticMaterial.MaterialInterface);
		}

		TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(UsdPrim, LODIndexToMaterialInfo, ExistingAssignments, AssetCache, Time, Flags );

		uint32 StaticMeshSlotIndex = 0;
		for ( int32 LODIndex = 0; LODIndex < LODIndexToMaterialInfo.Num(); ++LODIndex )
		{
			const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToMaterialInfo[ LODIndex ].Slots;

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
					UE_LOG(LogUsd, Error, TEXT("Failed to resolve material '%s' for slot '%d' of LOD '%d' for mesh '%s'"), *Slot.MaterialSource, LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath(UsdPrim.GetPath()));
					continue;
				}

				// Create and set the static material
				FStaticMaterial StaticMaterial( Material, *LexToString( StaticMeshSlotIndex ) );
				if ( !StaticMesh.GetStaticMaterials().IsValidIndex( StaticMeshSlotIndex ) )
				{
					StaticMesh.GetStaticMaterials().Add( MoveTemp( StaticMaterial ) );
					bMaterialAssignementsHaveChanged = true;
				}
				else if ( !( StaticMesh.GetStaticMaterials()[ StaticMeshSlotIndex ] == StaticMaterial ) )
				{
					StaticMesh.GetStaticMaterials()[ StaticMeshSlotIndex ] = MoveTemp( StaticMaterial );
					bMaterialAssignementsHaveChanged = true;
				}

#if WITH_EDITOR
				// Setup the section map so that our LOD material index is properly mapped to the static mesh material index
				// At runtime we don't ever parse these variants as LODs so we don't need this
				if ( StaticMesh.GetSectionInfoMap().IsValidSection( LODIndex, LODSlotIndex ) )
				{
					FMeshSectionInfo MeshSectionInfo = StaticMesh.GetSectionInfoMap().Get( LODIndex, LODSlotIndex );

					if ( MeshSectionInfo.MaterialIndex != StaticMeshSlotIndex )
					{
						MeshSectionInfo.MaterialIndex = StaticMeshSlotIndex;
						StaticMesh.GetSectionInfoMap().Set( LODIndex, LODSlotIndex, MeshSectionInfo );

						bMaterialAssignementsHaveChanged = true;
					}
				}
				else
				{
					FMeshSectionInfo MeshSectionInfo;
					MeshSectionInfo.MaterialIndex = StaticMeshSlotIndex;

					StaticMesh.GetSectionInfoMap().Set( LODIndex, LODSlotIndex, MeshSectionInfo );

					bMaterialAssignementsHaveChanged = true;
				}
#endif // WITH_EDITOR
			}
		}

#if WITH_EDITOR
		StaticMesh.GetOriginalSectionInfoMap().CopyFrom( StaticMesh.GetSectionInfoMap() );
#endif // WITH_EDITOR

		return bMaterialAssignementsHaveChanged;
	}

#if WITH_EDITOR
	// #ueent_todo: Merge the code with ProcessMaterials
	bool ProcessGeometryCacheMaterials( const pxr::UsdPrim& UsdPrim, const TArray< UsdUtils::FUsdPrimMaterialAssignmentInfo >& LODIndexToMaterialInfo, UGeometryCache& GeometryCache, UUsdAssetCache& AssetCache, float Time, EObjectFlags Flags)
	{
		bool bMaterialAssignementsHaveChanged = false;

		uint32 StaticMeshSlotIndex = 0;
		for ( int32 LODIndex = 0; LODIndex < LODIndexToMaterialInfo.Num(); ++LODIndex )
		{
			const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToMaterialInfo[ LODIndex ].Slots;

			for ( int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++StaticMeshSlotIndex )
			{
				const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[ LODSlotIndex ];
				UMaterialInterface* Material = nullptr;

				switch ( Slot.AssignmentType )
				{
				case UsdUtils::EPrimAssignmentType::DisplayColor:
				{
					FScopedUsdAllocs Allocs;

					// Try reusing an already created DisplayColor material
					if ( UMaterialInstanceConstant* ExistingMaterial = Cast< UMaterialInstanceConstant >( AssetCache.GetCachedAsset( Slot.MaterialSource ) ) )
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

								// Leave PrimPath as empty as it likely will be reused by many prims
								UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MaterialInstance, TEXT( "USDAssetImportData" ) );
								MaterialInstance->AssetImportData = ImportData;
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

				// Fallback to this UsdGeomMesh DisplayColor material if present
				if ( Material == nullptr )
				{
					FScopedUsdAllocs Allocs;

					// Try reusing an already created DisplayColor material
					if ( UMaterialInstanceConstant* ExistingMaterial = Cast< UMaterialInstanceConstant >( AssetCache.GetCachedAsset( Slot.MaterialSource ) ) )
					{
						Material = ExistingMaterial;
					}

					// Need to create a new DisplayColor material
					if ( Material == nullptr )
					{
						if ( TOptional< UsdUtils::FDisplayColorMaterial > DisplayColorDesc = UsdUtils::ExtractDisplayColorMaterial( pxr::UsdGeomMesh( UsdPrim ) ) )
						{
							UMaterialInstance* MaterialInstance = nullptr;

							if ( GIsEditor )  // Editor, PIE => true; Standlone, packaged => false
							{
								MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceConstant( DisplayColorDesc.GetValue() );

								// Leave PrimPath as empty as it likely will be reused by many prims
								UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MaterialInstance, TEXT( "USDAssetImportData" ) );
								MaterialInstance->AssetImportData = ImportData;
							}
							else
							{
								MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceDynamic( DisplayColorDesc.GetValue() );
							}

							AssetCache.CacheAsset( Slot.MaterialSource, MaterialInstance );
							Material = MaterialInstance;
						}
					}
				}

				if ( !GeometryCache.Materials.IsValidIndex( StaticMeshSlotIndex ) )
				{
					GeometryCache.Materials.Add( Material );
					bMaterialAssignementsHaveChanged = true;
				}
				else if ( !( GeometryCache.Materials[ StaticMeshSlotIndex ] == Material ) )
				{
					GeometryCache.Materials[ StaticMeshSlotIndex ] = Material;
					bMaterialAssignementsHaveChanged = true;
				}
			}
		}

		return bMaterialAssignementsHaveChanged;
	}
#endif // WITH_EDITOR

	// If UsdMesh is a LOD, will parse it and all of the other LODs, and and place them in OutLODIndexToMeshDescription and OutLODIndexToMaterialInfo.
	// Note that these other LODs will be hidden in other variants, and won't show up on traversal unless we actively switch the variants (which we do here).
	// We use a separate function for this because there is a very specific set of conditions where we successfully can do this, and we
	// want to fall back to just parsing UsdMesh as a simple single-LOD mesh if we fail.
	bool TryLoadingMultipleLODs(
		const pxr::UsdTyped& UsdMesh,
		TArray<FMeshDescription>& OutLODIndexToMeshDescription,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& OutLODIndexToMaterialInfo,
		const UsdToUnreal::FUsdMeshConversionOptions& Options
	)
	{
		FScopedUsdAllocs Allocs;

		pxr::UsdPrim UsdMeshPrim = UsdMesh.GetPrim();
		if ( !UsdMeshPrim )
		{
			return false;
		}

		pxr::UsdPrim ParentPrim = UsdMeshPrim.GetParent();
		if ( !ParentPrim )
		{
			return false;
		}

		TMap<int32, FMeshDescription> LODIndexToMeshDescriptionMap;
		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfoMap;

		UsdToUnreal::FUsdMeshConversionOptions OptionsCopy = Options;

		TFunction<bool( const pxr::UsdGeomMesh&, int32 )> ConvertLOD = [ & ]( const pxr::UsdGeomMesh& LODMesh, int32 LODIndex )
		{
			FMeshDescription TempMeshDescription;
			UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;

			FStaticMeshAttributes StaticMeshAttributes( TempMeshDescription );
			StaticMeshAttributes.Register();

			bool bSuccess = true;

			// The user can't manually hide/unhide a particular LOD in the engine after it is imported, so we should probably bake
			// the particular LOD visibility into the combined mesh. Note how we don't use computed visibility here, as we only really
			// care if this mesh in particular has been marked as invisible
			pxr::TfToken Visibility;
			pxr::UsdAttribute VisibilityAttr = LODMesh.GetVisibilityAttr();
			if ( VisibilityAttr && VisibilityAttr.Get( &Visibility, Options.TimeCode ) && Visibility == pxr::UsdGeomTokens->inherited )
			{
				// If we're interpreting LODs we must bake the transform from each LOD Mesh into the vertices, because there's no guarantee
				// all LODs have the same transform, so we can't just put the transforms directly on the component. If we are not interpreting
				// LODs we can do that though
				// TODO: Handle resetXformOp here
				bool bResetXformStack = false;
				FTransform MeshTransform = FTransform::Identity;
				bSuccess &= UsdToUnreal::ConvertXformable( LODMesh.GetPrim().GetStage(), LODMesh, MeshTransform, Options.TimeCode.GetValue(), &bResetXformStack );

				if ( bSuccess )
				{
					OptionsCopy.AdditionalTransform = MeshTransform * Options.AdditionalTransform;

					bSuccess &= UsdToUnreal::ConvertGeomMesh(
						LODMesh,
						TempMeshDescription,
						TempMaterialInfo,
						OptionsCopy
					);
				}
			}

			if ( bSuccess )
			{
				LODIndexToMeshDescriptionMap.Add( LODIndex, MoveTemp( TempMeshDescription ) );
				LODIndexToMaterialInfoMap.Add( LODIndex, MoveTemp( TempMaterialInfo ) );
			}

			return true;
		};
		bool bFoundLODs = UsdUtils::IterateLODMeshes( ParentPrim, ConvertLOD );

		// Place them in order as we can't have e.g. LOD0 and LOD2 without LOD1, and there's no reason downstream code needs to care about this
		OutLODIndexToMeshDescription.Reset( LODIndexToMeshDescriptionMap.Num() );
		OutLODIndexToMaterialInfo.Reset( LODIndexToMaterialInfoMap.Num() );
		LODIndexToMeshDescriptionMap.KeySort( TLess<int32>() );
		for ( TPair<int32, FMeshDescription>& Entry : LODIndexToMeshDescriptionMap )
		{
			const int32 OldLODIndex = Entry.Key;
			OutLODIndexToMeshDescription.Add( MoveTemp( Entry.Value ) );
			OutLODIndexToMaterialInfo.Add( MoveTemp( LODIndexToMaterialInfoMap[ OldLODIndex ] ) );
		}

		return bFoundLODs;
	}

	void LoadMeshDescriptions(
		pxr::UsdTyped UsdMesh,
		TArray<FMeshDescription>& OutLODIndexToMeshDescription,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& OutLODIndexToMaterialInfo,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		bool bInterpretLODs = false
	)
	{
		if ( !UsdMesh )
		{
			return;
		}

		FScopedUsdAllocs Allocs;

		pxr::UsdPrim Prim = UsdMesh.GetPrim();
		pxr::UsdStageRefPtr Stage = Prim.GetStage();
		pxr::SdfPath Path = Prim.GetPrimPath();

		bool bInterpretedLODs = false;
		if ( bInterpretLODs )
		{
			bInterpretedLODs = TryLoadingMultipleLODs( UsdMesh, OutLODIndexToMeshDescription, OutLODIndexToMaterialInfo, Options );

			// Have to be very careful here as flipping through LODs invalidates prim references, so we need to
			// re-acquire them
			UsdMesh = pxr::UsdGeomMesh{ Stage->GetPrimAtPath( Path ) };
		}

		// If we've managed to interpret LODs, we won't place our mesh transform on the static mesh component itself
		// (c.f. FUsdGeomXformableTranslator::UpdateComponents), and will instead expect it to be baked into the mesh.
		// So here we do that
		bool bSuccess = true;
		FTransform MeshTransform = FTransform::Identity;
		if ( bInterpretedLODs && OutLODIndexToMeshDescription.Num() > 1 )
		{
			// TODO: Handle resetXformOp here
			bool bResetXformStack = false;
			bSuccess &= UsdToUnreal::ConvertXformable( Stage, UsdMesh, MeshTransform, Options.TimeCode.GetValue(), &bResetXformStack );
		}

		if ( !bInterpretedLODs )
		{
			FMeshDescription TempMeshDescription;
			UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;

			FStaticMeshAttributes StaticMeshAttributes( TempMeshDescription );
			StaticMeshAttributes.Register();

			if ( bSuccess )
			{
				UsdToUnreal::FUsdMeshConversionOptions OptionsCopy = Options;
				OptionsCopy.AdditionalTransform = MeshTransform * Options.AdditionalTransform;

				bSuccess &= UsdToUnreal::ConvertGeomMesh(
					pxr::UsdGeomMesh{ UsdMesh },
					TempMeshDescription,
					TempMaterialInfo,
					OptionsCopy
				);
			}

			if ( bSuccess )
			{
				OutLODIndexToMeshDescription = { MoveTemp( TempMeshDescription ) };
				OutLODIndexToMaterialInfo = { MoveTemp( TempMaterialInfo ) };
			}
		}
	}

	void RepairNormalsAndTangents( const FString& PrimPath, FMeshDescription& MeshDescription )
	{
		FStaticMeshConstAttributes Attributes{ MeshDescription };
		TArrayView<const FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals().GetRawArray();

		// Similar to FStaticMeshOperations::AreNormalsAndTangentsValid but we don't care about tangents since we never
		// read those from USD
		uint64 InvalidNormalCount = 0;
		for ( const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs() )
		{
			if ( VertexInstanceNormals[ VertexInstanceID ].IsNearlyZero() || VertexInstanceNormals[ VertexInstanceID ].ContainsNaN() )
			{
				++InvalidNormalCount;
			}
		}
		if ( InvalidNormalCount == 0 )
		{
			return;
		}

		const float InvalidNormalFraction = ( float ) InvalidNormalCount / ( float ) VertexInstanceNormals.Num();

		// We always need to do this at this point as ComputeTangentsAndNormals will end up computing tangents anyway
		// and our triangle tangents are always invalid
		FStaticMeshOperations::ComputeTriangleTangentsAndNormals( MeshDescription );

		const static FString MeshNormalRepairThresholdText = TEXT( "USD.MeshNormalRepairThreshold" );

		// Make sure our normals can be rebuilt from MeshDescription::InitializeAutoGeneratedAttributes in case some tool needs them.
		// Always force-compute tangents here as we never have them anyway. If we don't force them to be recomputed we'll get
		// the worst of both worlds as some of these will be arbitrarily recomputed anyway, and some will be left invalid
		EComputeNTBsFlags Options = EComputeNTBsFlags::UseMikkTSpace | EComputeNTBsFlags::Tangents;
		if ( InvalidNormalFraction >= GMeshNormalRepairThreshold )
		{
			Options |= EComputeNTBsFlags::Normals;
			UE_LOG( LogUsd, Warning, TEXT( "%f%% of the normals from Mesh prim '%s' are invalid. This is at or above the threshold of '%f%%' (configurable via the cvar '%s'), so normals will be discarded and fully recomputed." ),
				InvalidNormalFraction * 100.0f,
				*PrimPath,
				GMeshNormalRepairThreshold * 100.0f,
				*MeshNormalRepairThresholdText
			);
		}
		else if ( InvalidNormalFraction > 0 )
		{
			UE_LOG( LogUsd, Warning, TEXT( "%f%% of the normals from Mesh prim '%s' are invalid. This is below the threshold of '%f%%' (configurable via the cvar '%s'), so the invalid normals will be repaired." ),
				InvalidNormalFraction * 100.0f,
				*PrimPath,
				GMeshNormalRepairThreshold * 100.0f,
				*MeshNormalRepairThresholdText
			);
		}
		FStaticMeshOperations::ComputeTangentsAndNormals( MeshDescription, Options );
	}

	UStaticMesh* CreateStaticMesh( TArray<FMeshDescription>& LODIndexToMeshDescription, FUsdSchemaTranslationContext& Context, const FString& MeshName, const bool bShouldEnableNanite, bool& bOutIsNew )
	{
		UStaticMesh* StaticMesh = nullptr;

		bool bHasValidMeshDescription = false;

		FSHAHash AllLODHash;
		{
			FSHA1 SHA1;

			for (const FMeshDescription& MeshDescription : LODIndexToMeshDescription )
			{
				FSHAHash LODHash = FStaticMeshOperations::ComputeSHAHash( MeshDescription );
				SHA1.Update( &LODHash.Hash[0], sizeof(LODHash.Hash) );

				if ( !MeshDescription.IsEmpty() )
				{
					bHasValidMeshDescription = true;
				}
			}

			// Put whether we want Nanite or not within the hash, so that the user could have one instance of the mesh without Nanite and another
			// with Nanite if they want to (using the override parameters). This also nicely handles a couple of edge cases:
			//	- What if we change a mesh from having Nanite disabled to enabled, or vice-versa (e.g. by changing the threshold)? We'd reuse the mesh from the asset cache
			//    in that case, so we'd need to rebuild it;
			//  - What if multiple meshes on the scene hash the same, but only one of them has a Nanite override attribute?
			// If we always enabled Nanite when either the mesh from the asset cache or the new prim wanted it, we wouldn't be able to turn Nanite off from
			// a single mesh that once had it enabled: It would always find the old Nanite-enabled mesh on the cache and leave it enabled.
			// If we always set Nanite to whatever the current prim wants, we could handle a single mesh turning Nanite on/off alright, but then we can't handle
			// the case where multiple meshes on the scene hash the same and only one of them has the override: The last prim would win, and they'd all be randomly either
			// enabled or disabled.
			// Note that we could also fix these problems by trying to check if a mesh is reused due to being in the cache from an old instance of the stage, or due to being used by
			// another prim, but that doesn't seem like a good path to go down
			// Additionally, hashing this bool also prevents us from having to force-rebuild a mesh to switch its Nanite flag, which could be tricky to do since some of
			// these build steps are async/thread-pool based.
			SHA1.Update( reinterpret_cast< const uint8* >( &bShouldEnableNanite ), sizeof( bShouldEnableNanite ) );

			// Hash the threshhold so that if we update it and reload we'll regenerate static meshes
			SHA1.Update( reinterpret_cast< const uint8* >( &GMeshNormalRepairThreshold ), sizeof( GMeshNormalRepairThreshold ) );

			SHA1.Final();
			SHA1.GetHash(&AllLODHash.Hash[0]);
		}

		StaticMesh = Cast< UStaticMesh >( Context.AssetCache->GetCachedAsset( AllLODHash.ToString() ) );

		if ( !StaticMesh && bHasValidMeshDescription )
		{
			bOutIsNew = true;

			FName AssetName = MakeUniqueObjectName( GetTransientPackage(), UStaticMesh::StaticClass(), *FPaths::GetBaseFilename( MeshName ) );
			StaticMesh = NewObject< UStaticMesh >( GetTransientPackage(), AssetName, Context.ObjectFlags | EObjectFlags::RF_Public );

#if WITH_EDITOR
			for ( int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex )
			{
				FMeshDescription& MeshDescription = LODIndexToMeshDescription[LODIndex];

				RepairNormalsAndTangents( MeshName, MeshDescription );

				FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
				SourceModel.BuildSettings.bGenerateLightmapUVs = false;
				SourceModel.BuildSettings.bRecomputeNormals = false;
				SourceModel.BuildSettings.bRecomputeTangents = false;
				SourceModel.BuildSettings.bBuildReversedIndexBuffer = false;

				FMeshDescription* StaticMeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
				check( StaticMeshDescription );
				*StaticMeshDescription = MoveTemp( MeshDescription );
			}
#endif // WITH_EDITOR

			StaticMesh->SetLightingGuid();

			Context.AssetCache->CacheAsset( AllLODHash.ToString(), StaticMesh );
		}
		else
		{
			//FPlatformMisc::LowLevelOutputDebugStringf( TEXT("Mesh found in cache %s\n"), *StaticMesh->GetName() );
			bOutIsNew = false;
		}

		return StaticMesh;
	}

	void PreBuildStaticMesh( UStaticMesh& StaticMesh )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UsdGeomMeshTranslatorImpl::PreBuildStaticMesh );

		if ( StaticMesh.GetRenderData())
		{
			StaticMesh.ReleaseResources();
			StaticMesh.ReleaseResourcesFence.Wait();
		}

		StaticMesh.SetRenderData(MakeUnique< FStaticMeshRenderData >());
		StaticMesh.CreateBodySetup();
	}

	bool BuildStaticMesh( UStaticMesh& StaticMesh, const FStaticFeatureLevel& FeatureLevel, TArray<FMeshDescription>& LODIndexToMeshDescription )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UsdGeomMeshTranslatorImpl::BuildStaticMesh );

		if ( LODIndexToMeshDescription.Num() == 0 )
		{
			return false;
		}

#if WITH_EDITOR
		ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
		ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
		check( RunningPlatform );

		const FStaticMeshLODSettings& LODSettings = RunningPlatform->GetStaticMeshLODSettings();
		StaticMesh.GetRenderData()->Cache( RunningPlatform, &StaticMesh, LODSettings );
#else
		StaticMesh.GetRenderData()->AllocateLODResources( LODIndexToMeshDescription.Num() );

		// Build render data from each mesh description
		for ( int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex )
		{
			FStaticMeshLODResources& LODResources = StaticMesh.GetRenderData()->LODResources[ LODIndex ];

			FMeshDescription& MeshDescription = LODIndexToMeshDescription[ LODIndex ];
			TVertexInstanceAttributesConstRef< FVector4f > MeshDescriptionColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4f>( MeshAttribute::VertexInstance::Color );

			// Compute normals here if necessary because they're not going to be computed via the regular static mesh build pipeline at runtime
			// (i.e. StaticMeshBuilder is not available at runtime)
			// We need polygon info because ComputeTangentsAndNormals uses it to repair the invalid vertex normals/tangents
			// Can't calculate just the required polygons as ComputeTangentsAndNormals is parallel and we can't guarantee thread-safe access patterns
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals( MeshDescription );
			FStaticMeshOperations::ComputeTangentsAndNormals( MeshDescription, EComputeNTBsFlags::UseMikkTSpace );

			// Manually set this as it seems the UStaticMesh only sets this whenever the mesh is serialized, which we won't do
			LODResources.bHasColorVertexData = MeshDescriptionColors.GetNumElements() > 0;

			StaticMesh.BuildFromMeshDescription( MeshDescription, LODResources );
		}

#endif // WITH_EDITOR

		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable( TEXT( "USD.EnableCollision" ) );
		bool bEnableCollision = CVar && CVar->GetBool();

		if ( StaticMesh.GetBodySetup())
		{
			if ( bEnableCollision )
			{
				StaticMesh.GetBodySetup()->CreatePhysicsMeshes();
			}
			else
			{
				StaticMesh.GetBodySetup()->DefaultInstance.SetCollisionEnabled( ECollisionEnabled::NoCollision );
				StaticMesh.GetBodySetup()->DefaultInstance.SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );
			}
		}
		return true;
	}

	void PostBuildStaticMesh( UStaticMesh& StaticMesh, const TArray<FMeshDescription>& LODIndexToMeshDescription )
	{
		// For runtime builds, the analogue for this stuff is already done from within BuildFromMeshDescriptions
		TRACE_CPUPROFILER_EVENT_SCOPE( UsdGeomMeshTranslatorImpl::PostBuildStaticMesh );

		StaticMesh.InitResources();

#if WITH_EDITOR
		// Fetch the MeshDescription from the StaticMesh because we'll have moved it away from LODIndexToMeshDescription CreateStaticMesh
		if ( const FMeshDescription* MeshDescription = StaticMesh.GetMeshDescription( 0 ) )
		{
			StaticMesh.GetRenderData()->Bounds = MeshDescription->GetBounds();
		}
		StaticMesh.CalculateExtendedBounds();
		StaticMesh.ClearMeshDescriptions(); // Clear mesh descriptions to reduce memory usage, they are kept only in bulk data form
#else
		// Fetch the MeshDescription from the imported LODIndexToMeshDescription as StaticMesh.GetMeshDescription is editor-only
		StaticMesh.GetRenderData()->Bounds = LODIndexToMeshDescription[ 0 ].GetBounds();
		StaticMesh.CalculateExtendedBounds();
#endif // WITH_EDITOR
	}

#if WITH_EDITOR
	void GeometryCacheDataForMeshDescription( FGeometryCacheMeshData& OutMeshData, FMeshDescription& MeshDescription );

	void GetGeometryCacheDataTimeCodeRange( const UE::FUsdStage& Stage, const FString& PrimPath, int32& OutStartFrame, int32& OutEndFrame )
	{
		if ( !Stage || PrimPath.IsEmpty() )
		{
			return;
		}

		FScopedUsdAllocs Allocs;

		pxr::UsdPrim UsdPrim = pxr::UsdPrim{ Stage.GetPrimAtPath( UE::FSdfPath{ *PrimPath } ) };
		if ( !UsdPrim )
		{
			return;
		}

		constexpr bool bIncludeInherited = false;
		pxr::TfTokenVector GeomMeshAttributeNames = pxr::UsdGeomMesh::GetSchemaAttributeNames( bIncludeInherited );
		pxr::TfTokenVector GeomPointBasedAttributeNames = pxr::UsdGeomPointBased::GetSchemaAttributeNames( bIncludeInherited );

		GeomMeshAttributeNames.reserve( GeomMeshAttributeNames.size() + GeomPointBasedAttributeNames.size() );
		GeomMeshAttributeNames.insert( GeomMeshAttributeNames.end(), GeomPointBasedAttributeNames.begin(), GeomPointBasedAttributeNames.end() );

		TOptional<double> MinStartTimeCode;
		TOptional<double> MaxEndTimeCode;

		for ( const pxr::TfToken& AttributeName : GeomMeshAttributeNames )
		{
			if ( const pxr::UsdAttribute& Attribute = UsdPrim.GetAttribute( AttributeName ) )
			{
				std::vector<double> TimeSamples;
				if ( Attribute.GetTimeSamples( &TimeSamples ) && TimeSamples.size() > 0 )
				{
					MinStartTimeCode = FMath::Min( MinStartTimeCode.Get( TNumericLimits<double>::Max() ), TimeSamples[ 0 ] );
					MaxEndTimeCode = FMath::Max( MaxEndTimeCode.Get( TNumericLimits<double>::Lowest() ), TimeSamples[ TimeSamples.size() - 1 ] );
				}
			}
		}

		if ( MinStartTimeCode.IsSet() && MaxEndTimeCode.IsSet() )
		{
			OutStartFrame = FMath::FloorToInt( MinStartTimeCode.GetValue() );
			OutEndFrame = FMath::CeilToInt( MaxEndTimeCode.GetValue() );
		}
	}

	UGeometryCache* CreateGeometryCache( const FString& InPrimPath, TArray< FMeshDescription >& LODIndexToMeshDescription, TSharedRef< FUsdSchemaTranslationContext> Context, bool& bOutIsNew )
	{
		UGeometryCache* GeometryCache = nullptr;

		bool bHasValidMeshDescription = false;

		FSHAHash AllLODHash;
		FSHA1 SHA1;
		for ( const FMeshDescription& MeshDescription : LODIndexToMeshDescription )
		{
			FSHAHash LODHash = FStaticMeshOperations::ComputeSHAHash( MeshDescription );
			SHA1.Update( &LODHash.Hash[ 0 ], sizeof( LODHash.Hash ) );

			if ( !MeshDescription.IsEmpty() )
			{
				bHasValidMeshDescription = true;
			}
		}
		SHA1.Final();
		SHA1.GetHash( &AllLODHash.Hash[ 0 ] );

		GeometryCache = Cast< UGeometryCache >( Context->AssetCache->GetCachedAsset( AllLODHash.ToString() ) );

		if ( !GeometryCache && bHasValidMeshDescription )
		{
			bOutIsNew = true;

			const FName AssetName = MakeUniqueObjectName( GetTransientPackage(), UGeometryCache::StaticClass(), *FPaths::GetBaseFilename( InPrimPath ) );
			GeometryCache = NewObject< UGeometryCache >( GetTransientPackage(), AssetName, Context->ObjectFlags | EObjectFlags::RF_Public );

			const UE::FUsdStage& Stage = Context->Stage;

			TMap< FString, TMap< FString, int32 > > Unused;
			TMap< FString, TMap< FString, int32 > >* MaterialToPrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex ? Context->MaterialToPrimvarToUVIndex : &Unused;

			// Fetch the animated mesh start/end frame as they may be different from just the stage's start and end time codes
			int32 StartFrame = FMath::FloorToInt( Stage.GetStartTimeCode() );
			int32 EndFrame = FMath::CeilToInt( Stage.GetEndTimeCode() );
			UsdGeomMeshTranslatorImpl::GetGeometryCacheDataTimeCodeRange( Stage, InPrimPath, StartFrame, EndFrame );

			// The GeometryCache module expects the end frame to be one past the last animation frame
			EndFrame += 1;

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if ( !Context->RenderContext.IsNone() )
			{
				RenderContextToken = UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
			}

			UsdToUnreal::FUsdMeshConversionOptions Options;
			Options.PurposesToLoad = Context->PurposesToLoad;
			Options.bMergeIdenticalMaterialSlots = Context->bMergeIdenticalMaterialSlots;
			Options.RenderContext = RenderContextToken;

			// Create and configure a new USDTrack to be added to the GeometryCache
			UGeometryCacheTrackUsd* UsdTrack = NewObject< UGeometryCacheTrackUsd >( GeometryCache );
			UsdTrack->Initialize(
				Stage,
				InPrimPath,
				Context->RenderContext,
				*MaterialToPrimvarToUVIndex,
				StartFrame,
				EndFrame,
				[InPrimPath, Options]( const TWeakObjectPtr<UGeometryCacheTrackUsd> TrackPtr, float Time, FGeometryCacheMeshData& OutMeshData ) mutable
				{
					UGeometryCacheTrackUsd* Track = TrackPtr.Get();
					if ( !Track )
					{
						return false;
					}

					if ( !Track->CurrentStagePinned )
					{
						return false;
					}

					UE::FUsdPrim Prim = Track->CurrentStagePinned.GetPrimAtPath( UE::FSdfPath{ *Track->PrimPath } );
					if ( !Prim )
					{
						return false;
					}

					// Get MeshDescription associated with the prim
					// #ueent_todo: Replace MeshDescription with RawMesh to optimize
					TArray< FMeshDescription > LODIndexToMeshDescription;
					TArray< UsdUtils::FUsdPrimMaterialAssignmentInfo > LODIndexToMaterialInfo;
					const bool bAllowInterpretingLODs = false;  // GeometryCaches don't have LODs, so we will never do this

					// Need a local copy of Options to set the TimeCode since this function is called from multiple worker threads
					UsdToUnreal::FUsdMeshConversionOptions LocalOptions(Options);
					LocalOptions.TimeCode = pxr::UsdTimeCode{ Time };
					LocalOptions.MaterialToPrimvarToUVIndex = &Track->MaterialToPrimvarToUVIndex;

					UsdGeomMeshTranslatorImpl::LoadMeshDescriptions(
						pxr::UsdTyped( Prim ),
						LODIndexToMeshDescription,
						LODIndexToMaterialInfo,
						LocalOptions,
						bAllowInterpretingLODs
					);

					// Convert the MeshDescription to MeshData, first LOD only
					for ( FMeshDescription& MeshDescription : LODIndexToMeshDescription )
					{
						if ( !MeshDescription.IsEmpty() )
						{
							// Compute the normals and tangents for the mesh
							const float ComparisonThreshold = THRESH_POINTS_ARE_SAME;

							// This function make sure the Polygon Normals Tangents Binormals are computed and also remove degenerated triangle from the render mesh description.
							FStaticMeshOperations::ComputeTriangleTangentsAndNormals( MeshDescription, ComparisonThreshold );

							// Compute any missing normals or tangents.
							// Static meshes always blend normals of overlapping corners.
							EComputeNTBsFlags ComputeNTBsOptions = EComputeNTBsFlags::BlendOverlappingNormals;
							ComputeNTBsOptions |= EComputeNTBsFlags::IgnoreDegenerateTriangles;
							ComputeNTBsOptions |= EComputeNTBsFlags::UseMikkTSpace;

							FStaticMeshOperations::ComputeTangentsAndNormals( MeshDescription, ComputeNTBsOptions );

							UsdGeomMeshTranslatorImpl::GeometryCacheDataForMeshDescription( OutMeshData, MeshDescription );

							return true;
						}
					}
					return false;
				}
			);

			GeometryCache->AddTrack( UsdTrack );

			TArray< FMatrix > Mats;
			Mats.Add( FMatrix::Identity );
			Mats.Add( FMatrix::Identity );

			TArray< float > MatTimes;
			MatTimes.Add( 0.0f );
			MatTimes.Add( 0.0f );
			UsdTrack->SetMatrixSamples( Mats, MatTimes );

			Context->AssetCache->CacheAsset( AllLODHash.ToString(), GeometryCache );
		}
		else
		{
			bOutIsNew = false;
		}

		return GeometryCache;
	}

	// #ueent_todo: Replace MeshDescription with RawMesh and also make it work with StaticMesh
	void GeometryCacheDataForMeshDescription( FGeometryCacheMeshData& OutMeshData, FMeshDescription& MeshDescription )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( GeometryCacheDataForMeshDescription );

		OutMeshData.Positions.Reset();
		OutMeshData.TextureCoordinates.Reset();
		OutMeshData.TangentsX.Reset();
		OutMeshData.TangentsZ.Reset();
		OutMeshData.Colors.Reset();
		OutMeshData.Indices.Reset();

		OutMeshData.MotionVectors.Reset();
		OutMeshData.BatchesInfo.Reset();
		OutMeshData.BoundingBox.Init();

		OutMeshData.VertexInfo.bHasColor0 = true;
		OutMeshData.VertexInfo.bHasTangentX = true;
		OutMeshData.VertexInfo.bHasTangentZ = true;
		OutMeshData.VertexInfo.bHasUV0 = true;
		OutMeshData.VertexInfo.bHasMotionVectors = false;

		FStaticMeshAttributes MeshDescriptionAttributes( MeshDescription );

		TVertexAttributesConstRef< FVector3f > VertexPositions = MeshDescriptionAttributes.GetVertexPositions();
		TVertexInstanceAttributesConstRef< FVector3f > VertexInstanceNormals = MeshDescriptionAttributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesConstRef< FVector3f > VertexInstanceTangents = MeshDescriptionAttributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesConstRef< float > VertexInstanceBinormalSigns = MeshDescriptionAttributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesConstRef< FVector4f > VertexInstanceColors = MeshDescriptionAttributes.GetVertexInstanceColors();
		TVertexInstanceAttributesConstRef< FVector2f > VertexInstanceUVs = MeshDescriptionAttributes.GetVertexInstanceUVs();

		const int32 NumVertices = MeshDescription.Vertices().Num();
		const int32 NumTriangles = MeshDescription.Triangles().Num();
		const int32 NumMeshDataVertices = NumTriangles * 3;

		OutMeshData.Positions.Reserve( NumVertices );
		OutMeshData.Indices.Reserve( NumMeshDataVertices );
		OutMeshData.TangentsZ.Reserve( NumMeshDataVertices );
		OutMeshData.Colors.Reserve( NumMeshDataVertices );
		OutMeshData.TextureCoordinates.Reserve( NumMeshDataVertices );

		FBox BoundingBox( EForceInit::ForceInitToZero );
		int32 MaterialIndex = 0;
		int32 VertexIndex = 0;
		for ( FPolygonGroupID PolygonGroupID : MeshDescription.PolygonGroups().GetElementIDs() )
		{
			// Skip empty polygon groups
			if ( MeshDescription.GetNumPolygonGroupPolygons( PolygonGroupID ) == 0 )
			{
				continue;
			}

			FGeometryCacheMeshBatchInfo BatchInfo;
			BatchInfo.StartIndex = OutMeshData.Indices.Num();
			BatchInfo.MaterialIndex = MaterialIndex++;

			int32 TriangleCount = 0;
			for ( FPolygonID PolygonID : MeshDescription.GetPolygonGroupPolygonIDs( PolygonGroupID ) )
			{
				for ( FTriangleID TriangleID : MeshDescription.GetPolygonTriangles( PolygonID ) )
				{
					for ( FVertexInstanceID VertexInstanceID : MeshDescription.GetTriangleVertexInstances( TriangleID ) )
					{
						const FVector3f& Position = VertexPositions[ MeshDescription.GetVertexInstanceVertex( VertexInstanceID ) ];
						OutMeshData.Positions.Add( Position );
						BoundingBox += (FVector)Position;

						OutMeshData.Indices.Add( VertexIndex++ );

						FPackedNormal Normal = VertexInstanceNormals[ VertexInstanceID ];
						Normal.Vector.W = VertexInstanceBinormalSigns[ VertexInstanceID ] < 0 ? -127 : 127;
						OutMeshData.TangentsZ.Add( Normal );
						OutMeshData.TangentsX.Add( VertexInstanceTangents[ VertexInstanceID ] );

						const bool bSRGB = true;
						OutMeshData.Colors.Add( FLinearColor( VertexInstanceColors[ VertexInstanceID ] ).ToFColor( bSRGB ) );

						// Supporting only one UV channel
						const int32 UVIndex = 0;
						OutMeshData.TextureCoordinates.Add( VertexInstanceUVs.Get( VertexInstanceID, UVIndex ) );
					}

					++TriangleCount;
				}
			}

			OutMeshData.BoundingBox = (FBox3f)BoundingBox;

			BatchInfo.NumTriangles = TriangleCount;
			OutMeshData.BatchesInfo.Add( BatchInfo );
		}
	}
#endif // WITH_EDITOR
}

FBuildStaticMeshTaskChain::FBuildStaticMeshTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath )
	: PrimPath( InPrimPath )
	, Context( InContext )
{
}

void FBuildStaticMeshTaskChain::SetupTasks()
{
	// Ignore meshes from disabled purposes
	if ( !EnumHasAllFlags( Context->PurposesToLoad, IUsdPrim::GetPurpose( GetPrim() ) ) )
	{
		return;
	}

	// Create static mesh (Main thread)
	Do( ESchemaTranslationLaunchPolicy::Sync,
		[ this ]()
		{
			// Force load MeshBuilderModule so that it's ready for the async tasks
#if WITH_EDITOR
			FModuleManager::LoadModuleChecked< IMeshBuilderModule >( TEXT("MeshBuilder") );
#endif // WITH_EDITOR

			const FString PrimPathString = PrimPath.GetString();

			// It's useful to have the LOD Mesh prims be named "LOD0", "LOD1", etc. within the LOD variants so that we
			// can easily tell which Mesh is actually meant to be the LOD mesh (in case there are more Meshes in each
			// variant or other Meshes outside of the variant), but it's not ideal to have all the generated assets end
			// up imported as "SM_LOD0_22", "SM_LOD0_23", etc. So here we fetch the parent prim name in case we're a LOD
			FString MeshName;
			if ( Context->bAllowInterpretingLODs && UsdUtils::IsGeomMeshALOD( GetPrim() ) )
			{
				MeshName = PrimPath.GetParentPath().GetString();
			}
			else
			{
				MeshName = PrimPathString;
			}

			bool bIsNew = true;
			const bool bShouldEnableNanite = UsdGeomMeshTranslatorImpl::ShouldEnableNanite( LODIndexToMeshDescription, LODIndexToMaterialInfo, *Context, GetPrim() );
			StaticMesh = UsdGeomMeshTranslatorImpl::CreateStaticMesh( LODIndexToMeshDescription, *Context, MeshName, bShouldEnableNanite, bIsNew );

			if ( StaticMesh )
			{
				Context->AssetCache->LinkAssetToPrim( PrimPathString, StaticMesh );

#if WITH_EDITOR
				StaticMesh->NaniteSettings.bEnabled = bShouldEnableNanite;

				if ( bIsNew )
				{
					UUsdAssetImportData* ImportData = NewObject<UUsdAssetImportData>( StaticMesh, TEXT( "UUSDAssetImportData" ) );
					ImportData->PrimPath = PrimPathString;
					StaticMesh->AssetImportData = ImportData;
				}
#endif // WITH_EDITOR


				// Only process the materials if we own the mesh. If it's new we know we do
#if WITH_EDITOR
				UUsdAssetImportData* ImportData = Cast<UUsdAssetImportData>( StaticMesh->AssetImportData );
				if ( ImportData && ImportData->PrimPath == PrimPathString )
#endif // WITH_EDITOR
				{
					if ( !bIsNew )
					{
						// We may change material assignments
						StaticMesh->Modify();
					}

					const bool bMaterialsHaveChanged = UsdGeomMeshTranslatorImpl::ProcessMaterials(
						GetPrim(),
						LODIndexToMaterialInfo,
						*StaticMesh,
						*Context->AssetCache.Get(),
						Context->Time,
						Context->ObjectFlags
					);

					if ( bMaterialsHaveChanged )
					{
						const bool bRebuildAll = true;

#if WITH_EDITOR
						StaticMesh->UpdateUVChannelData( bRebuildAll );
#else
						// UpdateUVChannelData doesn't do anything without the editor
						for ( FStaticMaterial& Material : StaticMesh->GetStaticMaterials() )
						{
							Material.UVChannelData.bInitialized = true;
						}
#endif // WITH_EDITOR
					}
				}
			}

			// Only need to continue building the mesh if we just created it
			return bIsNew;
		} );

#if WITH_EDITOR
	// Commit mesh description (Async)
	Then( ESchemaTranslationLaunchPolicy::Async,
		[ this ]()
		{
			UStaticMesh::FCommitMeshDescriptionParams Params;
			Params.bMarkPackageDirty = false;
			Params.bUseHashAsGuid = true;

			for ( int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex )
			{
				StaticMesh->CommitMeshDescription( LODIndex, Params );
			}

			return true;
		} );
#endif // WITH_EDITOR

	// PreBuild static mesh (Main thread)
	Then( ESchemaTranslationLaunchPolicy::Sync,
		[ this ]()
		{
			RecreateRenderStateContextPtr = MakeShared<FStaticMeshComponentRecreateRenderStateContext>( StaticMesh, true, true );

			UsdGeomMeshTranslatorImpl::PreBuildStaticMesh( *StaticMesh );

			return true;
		} );

	// Build static mesh (Async)
	Then( ESchemaTranslationLaunchPolicy::Async,
		[ this ]() mutable
		{
			FStaticFeatureLevel FeatureLevel = GMaxRHIFeatureLevel;

			UWorld* World = Context->Level ? Context->Level->GetWorld() : nullptr;
			if ( !World )
			{
				World = GWorld;
			}
			if ( World )
			{
				FeatureLevel = World->FeatureLevel;
			}

			if ( !UsdGeomMeshTranslatorImpl::BuildStaticMesh( *StaticMesh, FeatureLevel, LODIndexToMeshDescription ) )
			{
				// Build failed, discard the mesh
				StaticMesh = nullptr;

				return false;
			}

			return true;
		} );

	// PostBuild static mesh (Main thread)
	Then( ESchemaTranslationLaunchPolicy::Sync,
		[ this ]()
		{
			UsdGeomMeshTranslatorImpl::PostBuildStaticMesh( *StaticMesh, LODIndexToMeshDescription );

			RecreateRenderStateContextPtr.Reset();

			return true;
		} );
}

FGeomMeshCreateAssetsTaskChain::FGeomMeshCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath, const FTransform& InAdditionalTransform )
	: FBuildStaticMeshTaskChain( InContext, InPrimPath )
	, AdditionalTransform( InAdditionalTransform )
{
	SetupTasks();
}

void FGeomMeshCreateAssetsTaskChain::SetupTasks()
{
	FScopedUnrealAllocs UnrealAllocs;

	// To parse all LODs we need to actively switch variant sets to other variants (triggering prim loading/unloading and notices),
	// which could cause race conditions if other async translation tasks are trying to access those prims
	ESchemaTranslationLaunchPolicy LaunchPolicy = ESchemaTranslationLaunchPolicy::Async;
	if ( Context->bAllowInterpretingLODs && UsdUtils::IsGeomMeshALOD( GetPrim() ) )
	{
		LaunchPolicy = ESchemaTranslationLaunchPolicy::ExclusiveSync;
	}

	// Create mesh descriptions (Async or ExclusiveSync)
	Do( LaunchPolicy,
		[ this ]() -> bool
		{
			TMap< FString, TMap< FString, int32 > > Unused;
			TMap< FString, TMap< FString, int32 > >* MaterialToPrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex ? Context->MaterialToPrimvarToUVIndex : &Unused;

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if ( !Context->RenderContext.IsNone() )
			{
				RenderContextToken = UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
			}

			UsdToUnreal::FUsdMeshConversionOptions Options;
			Options.TimeCode = Context->Time;
			Options.PurposesToLoad = Context->PurposesToLoad;
			Options.RenderContext = RenderContextToken;
			Options.MaterialToPrimvarToUVIndex = MaterialToPrimvarToUVIndex;
			Options.bMergeIdenticalMaterialSlots = Context->bMergeIdenticalMaterialSlots;
			Options.AdditionalTransform = AdditionalTransform;

			UsdGeomMeshTranslatorImpl::LoadMeshDescriptions(
				pxr::UsdTyped( GetPrim() ),
				LODIndexToMeshDescription,
				LODIndexToMaterialInfo,
				Options,
				Context->bAllowInterpretingLODs
			);

			// If we have at least one valid LOD, we should keep going
			for ( const FMeshDescription& MeshDescription : LODIndexToMeshDescription )
			{
				if ( !MeshDescription.IsEmpty() )
				{
					return true;
				}
			}
			return false;
		} );

	FBuildStaticMeshTaskChain::SetupTasks();
}

#if WITH_EDITOR
class FGeometryCacheCreateAssetsTaskChain : public FBuildStaticMeshTaskChain
{
public:
	explicit FGeometryCacheCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath )
		: FBuildStaticMeshTaskChain( InContext, InPrimPath )
	{
		SetupTasks();
	}

protected:
	virtual void SetupTasks() override;
};

void FGeometryCacheCreateAssetsTaskChain::SetupTasks()
{
	FScopedUnrealAllocs UnrealAllocs;

	// Create mesh descriptions (Async or ExclusiveSync)
	Do( ESchemaTranslationLaunchPolicy::Async,
		[ this ]() -> bool
		{
			// Always hash the mesh at the same time because it may be animated, and
			// otherwise we may think it's a new asset just because the context is at a different timecode (e.g. if we reload)
			// TODO: Hash all timecodes, or else our mesh may change at t=5 and we never reload it because we only hash t=0
			const bool bAllowInterpretingLODs = false;  // GeometryCaches don't have LODs
			TMap< FString, TMap< FString, int32 > > Unused;
			TMap< FString, TMap< FString, int32 > >* MaterialToPrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex ? Context->MaterialToPrimvarToUVIndex : &Unused;

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if ( !Context->RenderContext.IsNone() )
			{
				RenderContextToken = UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
			}

			UsdToUnreal::FUsdMeshConversionOptions Options;
			Options.TimeCode = UsdUtils::GetEarliestTimeCode();
			Options.PurposesToLoad = Context->PurposesToLoad;
			Options.RenderContext = RenderContextToken;
			Options.MaterialToPrimvarToUVIndex = MaterialToPrimvarToUVIndex;
			Options.bMergeIdenticalMaterialSlots = Context->bMergeIdenticalMaterialSlots;

			UsdGeomMeshTranslatorImpl::LoadMeshDescriptions(
				pxr::UsdTyped( GetPrim() ),
				LODIndexToMeshDescription,
				LODIndexToMaterialInfo,
				Options,
				bAllowInterpretingLODs
			);

			// If we have at least one valid LOD, we should keep going
			for ( const FMeshDescription& MeshDescription : LODIndexToMeshDescription )
			{
				if ( !MeshDescription.IsEmpty() )
				{
					return true;
				}
			}
			return false;
		} );

	// Create the GeometryCache (Main thread)
	Then( ESchemaTranslationLaunchPolicy::Sync,
		[ this ]()
		{
			bool bIsNew = true;
			const FString PrimPathString = PrimPath.GetString();
			UGeometryCache* GeometryCache = UsdGeomMeshTranslatorImpl::CreateGeometryCache( PrimPathString, LODIndexToMeshDescription, Context, bIsNew );

			if ( bIsNew && GeometryCache )
			{
				UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( GeometryCache, TEXT( "UUSDAssetImportData" ) );
				ImportData->PrimPath = PrimPathString;
				GeometryCache->AssetImportData = ImportData;
			}

			bool bMaterialsHaveChanged = false;
			if ( GeometryCache )
			{
				Context->AssetCache->LinkAssetToPrim( PrimPathString, GeometryCache );

				// Only process the materials if we own the GeometryCache. If it's new we know we do
				UUsdAssetImportData* ImportData = Cast< UUsdAssetImportData >( GeometryCache->AssetImportData );
				if ( ImportData && ImportData->PrimPath == PrimPathString )
				{
					bMaterialsHaveChanged = UsdGeomMeshTranslatorImpl::ProcessGeometryCacheMaterials( GetPrim(), LODIndexToMaterialInfo, *GeometryCache, *Context->AssetCache.Get(), Context->Time, Context->ObjectFlags );
				}
			}

			const bool bContinueTaskChain = ( bIsNew || bMaterialsHaveChanged );
			return bContinueTaskChain;
		} );
}
#endif // WITH_EDITOR

void FUsdGeomMeshTranslator::CreateAssets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomMeshTranslator::CreateAssets );

#if WITH_EDITOR
	if ( GUseGeometryCacheUSD && UsdGeomMeshTranslatorImpl::IsAnimated( GetPrim() ) )
	{
		// Create the GeometryCache TaskChain
		TSharedRef< FGeometryCacheCreateAssetsTaskChain > AssetsTaskChain = MakeShared< FGeometryCacheCreateAssetsTaskChain >( Context, PrimPath );

		Context->TranslatorTasks.Add( MoveTemp( AssetsTaskChain ) );
	}
	else
#endif // WITH_EDITOR
	{
		TSharedRef< FGeomMeshCreateAssetsTaskChain > AssetsTaskChain = MakeShared< FGeomMeshCreateAssetsTaskChain >( Context, PrimPath );

		Context->TranslatorTasks.Add( MoveTemp( AssetsTaskChain ) );
	}
}

USceneComponent* FUsdGeomMeshTranslator::CreateComponents()
{
	TOptional< TSubclassOf< USceneComponent > > ComponentType;

#if WITH_EDITOR
	// Force animated meshes as GeometryCache
	if ( GUseGeometryCacheUSD && UsdGeomMeshTranslatorImpl::IsAnimated( GetPrim() ) )
	{
		ComponentType = UGeometryCacheUsdComponent::StaticClass();
	}
#endif // WITH_EDITOR

	USceneComponent* SceneComponent = CreateComponentsEx( ComponentType, {} );
	UpdateComponents( SceneComponent );

	const FString PrimPathString = PrimPath.GetString();

	// Handle material overrides
	// Note: This can be here and not in USDGeomXformableTranslator because there is no way that a collapsed mesh prim could end up with a material override
	if ( UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>( SceneComponent ) )
	{
		if ( UStaticMesh* StaticMesh = Cast< UStaticMesh >( Context->AssetCache->GetAssetForPrim( PrimPathString ) ) )
		{
			TArray<UMaterialInterface*> ExistingAssignments;
			for ( FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials() )
			{
				ExistingAssignments.Add( StaticMaterial.MaterialInterface );
			}

			MeshTranslationImpl::SetMaterialOverrides(
				GetPrim(),
				ExistingAssignments,
				*StaticMeshComponent,
				*Context->AssetCache.Get(),
				Context->Time,
				Context->ObjectFlags,
				Context->bAllowInterpretingLODs,
				Context->RenderContext,
				Context->MaterialPurpose
			);
		}
	}
	else if ( UGeometryCacheComponent* Component = Cast<UGeometryCacheComponent>( SceneComponent ) )
	{
		if ( UGeometryCache* GeometryCache = Cast< UGeometryCache >( Context->AssetCache->GetAssetForPrim( PrimPathString ) ) )
		{
			// Geometry caches don't support LODs
			const bool bAllowInterpretingLODs = false;

			MeshTranslationImpl::SetMaterialOverrides(
				GetPrim(),
				GeometryCache->Materials,
				*Component,
				*Context->AssetCache.Get(),
				Context->Time,
				Context->ObjectFlags,
				bAllowInterpretingLODs,
				Context->RenderContext,
				Context->MaterialPurpose
			);

#if WITH_EDITOR
			// Check if the prim has the GroomBinding schema and setup the component and assets necessary to bind the groom to the GeometryCache
			if ( UsdUtils::PrimHasSchema( GetPrim(), UnrealIdentifiers::GroomBindingAPI ) )
			{
				UsdGroomTranslatorUtils::CreateGroomBindingAsset( GetPrim(), *( Context->AssetCache ), Context->ObjectFlags );

				// For the groom binding to work, the GroomComponent must be a child of the SceneComponent
				// so the Context ParentComponent is set to the SceneComponent temporarily
				TGuardValue< USceneComponent* > ParentComponentGuard{ Context->ParentComponent, SceneComponent };
				const bool bNeedsActor = false;
				UGroomComponent* GroomComponent = Cast< UGroomComponent >( CreateComponentsEx( TSubclassOf< USceneComponent >( UGroomComponent::StaticClass() ), bNeedsActor ) );
				if ( GroomComponent )
				{
					UpdateComponents( SceneComponent );
				}
			}
#endif // !WITH_EDITOR
		}
	}

	return SceneComponent;
}

void FUsdGeomMeshTranslator::UpdateComponents( USceneComponent* SceneComponent )
{
	if ( SceneComponent )
	{
		SceneComponent->Modify();
	}

	if (
#if WITH_EDITOR
		!GUseGeometryCacheUSD &&
#endif // !WITH_EDITOR
		UsdGeomMeshTranslatorImpl::IsAnimated( GetPrim() )
	)
	{
		// The assets might have changed since our attributes are animated
		// Note that we must wait for these to complete as they make take a while and we want to
		// reassign our new static meshes when we get to FUsdGeomXformableTranslator::UpdateComponents
		CreateAssets();
		Context->CompleteTasks();
	}

#if WITH_EDITOR
	// Set the initial GeometryCache on the GeometryCacheUsdComponent
	if ( UGeometryCacheUsdComponent* GeometryCacheUsdComponent = Cast< UGeometryCacheUsdComponent >( SceneComponent ) )
	{
		UGeometryCache* GeometryCache = Cast< UGeometryCache >( Context->AssetCache->GetAssetForPrim( PrimPath.GetString() ) );

		bool bShouldRegister = false;
		if ( GeometryCache != GeometryCacheUsdComponent->GetGeometryCache() )
		{
			bShouldRegister = true;

			if ( GeometryCacheUsdComponent->IsRegistered() )
			{
				GeometryCacheUsdComponent->UnregisterComponent();
			}

			// Skip the extra handling in SetGeometryCache
			GeometryCacheUsdComponent->GeometryCache = GeometryCache;
		}

		float TimeCode = Context->Time;
		if ( FMath::IsNaN( TimeCode ) )
		{
			int32 StartFrame = FMath::FloorToInt( Context->Stage.GetStartTimeCode() );
			int32 EndFrame = FMath::CeilToInt( Context->Stage.GetEndTimeCode() );
			UsdGeomMeshTranslatorImpl::GetGeometryCacheDataTimeCodeRange( Context->Stage, PrimPath.GetString(), StartFrame, EndFrame );

			TimeCode = static_cast< float >( StartFrame );
		}

		// This is the main call responsible for animating the geometry cache.
		// It needs to happen after setting the geometry cache and before registering, because we must force the
		// geometry cache to register itself at Context->Time so that it will synchronously load that frame right away.
		// Otherwise the geometry cache will start at t=0 regardless of Context->Time
		GeometryCacheUsdComponent->SetManualTick( true );
		GeometryCacheUsdComponent->TickAtThisTime( TimeCode, true, false, true );

		// Note how we should only register if our geometry cache changed: If we did this every time we would
		// register too early during the process of duplicating into PIE, and that would prevent a future RegisterComponent
		// call from naturally creating the required render state
		if ( bShouldRegister && !GeometryCacheUsdComponent->IsRegistered() )
		{
			GeometryCacheUsdComponent->RegisterComponent();
		}

		// If the prim has a GroomBinding schema, apply the target groom to its associated GroomComponent
		if ( UsdUtils::PrimHasSchema( GetPrim(), UnrealIdentifiers::GroomBindingAPI ) )
		{
			UsdGroomTranslatorUtils::SetGroomFromPrim( GetPrim(), *Context->AssetCache, SceneComponent );
		}
	}
#endif // WITH_EDITOR

	Super::UpdateComponents( SceneComponent );
}

bool FUsdGeomMeshTranslator::CollapsesChildren( ECollapsingType CollapsingType ) const
{
	// We can't claim we collapse anything here since we'll just parse the mesh for this prim and that's it,
	// otherwise the translation context wouldn't spawn translators for our child prims.
	// Another approach would be to actually recursively collapse our child mesh prims, but that leads to a few
	// issues. For example this translator could end up globbing a child Mesh prim, while the translation context
	// could simultaneously spawn other translators that could also end up accounting for that same mesh.
	// Generally Gprims shouldn't be nested into each other anyway (see https://graphics.pixar.com/usd/release/glossary.html#usdglossary-gprim)
	// so it's likely best to just not collapse anything here.
	return false;
}

bool FUsdGeomMeshTranslator::CanBeCollapsed( ECollapsingType CollapsingType ) const
{
	UE::FUsdPrim Prim = GetPrim();

	// Don't collapse if our final UStaticMesh would have multiple LODs
	if ( Context->bAllowInterpretingLODs &&
		 CollapsingType == ECollapsingType::Assets &&
		 UsdUtils::IsGeomMeshALOD( Prim ) )
	{
		return false;
	}

	return Super::CanBeCollapsed( CollapsingType );
}

#endif // #if USE_USD_SDK
