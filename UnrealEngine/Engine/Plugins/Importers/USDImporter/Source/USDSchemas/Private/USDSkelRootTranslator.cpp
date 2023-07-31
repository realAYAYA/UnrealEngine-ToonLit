// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDSkelRootTranslator.h"

#if USE_USD_SDK

#include "MeshTranslationImpl.h"
#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDGroomTranslatorUtils.h"
#include "USDIntegrationUtils.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDSkeletalDataConversion.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimationUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GroomComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Serialization/BufferArchive.h"

#if WITH_EDITOR
	#include "AnimGraphNode_LiveLinkPose.h"
	#include "AnimNode_LiveLinkPose.h"
	#include "BlueprintCompilationManager.h"
	#include "EdGraph/EdGraph.h"
	#include "EdGraph/EdGraphNode.h"
	#include "K2Node_VariableGet.h"
	#include "Kismet2/BlueprintEditorUtils.h"
	#include "Kismet2/CompilerResultsLog.h"
	#include "MaterialEditingLibrary.h"
	#include "ObjectTools.h"
	#include "PhysicsAssetUtils.h"
#endif // WITH_EDITOR

#include "USDIncludesStart.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdShade/material.h"
	#include "pxr/usd/usdSkel/binding.h"
	#include "pxr/usd/usdSkel/bindingAPI.h"
	#include "pxr/usd/usdSkel/blendShapeQuery.h"
	#include "pxr/usd/usdSkel/cache.h"
	#include "pxr/usd/usdSkel/root.h"
	#include "pxr/usd/usdSkel/skeletonQuery.h"
	#include "pxr/usd/usdSkel/tokens.h"
#include "USDIncludesEnd.h"

#define LOCTEXT_NAMESPACE "UsdSkelRoot"

static bool bGeneratePhysicsAssets = true;
static FAutoConsoleVariableRef CVarGeneratePhysicsAssets(
	TEXT( "USD.GeneratePhysicsAssets" ),
	bGeneratePhysicsAssets,
	TEXT( "Whether to automatically generate and assign PhysicsAssets to generated SkeletalMeshes." ) );

namespace UsdSkelRootTranslatorImpl
{
#if WITH_EDITOR
	bool ProcessMaterials(
		const pxr::UsdPrim& UsdPrim,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
		USkeletalMesh* SkeletalMesh,
		UUsdAssetCache& AssetCache,
		float Time, EObjectFlags Flags,
		bool bSkeletalMeshHasMorphTargets
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UsdSkelRootTranslatorImpl::ProcessMaterials );

		if ( !SkeletalMesh )
		{
			return false;
		}

		TArray<UMaterialInterface*> ExistingAssignments;
		for ( const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials())
		{
			ExistingAssignments.Add( SkeletalMaterial.MaterialInterface );
		}

		TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo( UsdPrim, LODIndexToMaterialInfo, ExistingAssignments, AssetCache, Time, Flags );

		bool bMaterialsHaveChanged = false;

		uint32 SkeletalMeshSlotIndex = 0;
		for ( int32 LODIndex = 0; LODIndex < LODIndexToMaterialInfo.Num(); ++LODIndex )
		{
			const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToMaterialInfo[ LODIndex ].Slots;

			// We need to fill this in with the mapping from LOD material slots (i.e. sections) to the skeletal mesh's material slots
			FSkeletalMeshLODInfo* LODInfo = SkeletalMesh->GetLODInfo( LODIndex );
			if ( !LODInfo )
			{
				UE_LOG( LogUsd, Error, TEXT( "When processing materials for SkeletalMesh '%s', encountered no LOD info for LOD index %d!" ), *SkeletalMesh->GetName(), LODIndex );
				continue;
			}
			TArray<int32>& LODMaterialMap = LODInfo->LODMaterialMap;
			LODMaterialMap.Reserve(LODSlots.Num());

			for ( int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++SkeletalMeshSlotIndex )
			{
				const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[ LODSlotIndex ];

				UMaterialInterface* Material = nullptr;

				if ( UMaterialInterface** FoundMaterial = ResolvedMaterials.Find( &Slot ) )
				{
					Material = *FoundMaterial;
				}
				else
				{
					UE_LOG( LogUsd, Error, TEXT( "Failed to resolve material '%s' for slot '%d' of LOD '%d' for mesh '%s'" ), *Slot.MaterialSource, LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath( UsdPrim.GetPath() ) );
					continue;
				}

				if ( Material )
				{
					bool bNeedsRecompile = false;
					Material->GetMaterial()->SetMaterialUsage( bNeedsRecompile, MATUSAGE_SkeletalMesh );
					if ( bSkeletalMeshHasMorphTargets )
					{
						Material->GetMaterial()->SetMaterialUsage( bNeedsRecompile, MATUSAGE_MorphTargets );
					}
				}

				FName MaterialSlotName = *LexToString( SkeletalMeshSlotIndex );

				// Already have a material at that skeletal mesh slot, need to reassign
				if ( SkeletalMesh->GetMaterials().IsValidIndex( SkeletalMeshSlotIndex ) )
				{
					FSkeletalMaterial& ExistingMaterial = SkeletalMesh->GetMaterials()[ SkeletalMeshSlotIndex ];

					if ( ExistingMaterial.MaterialInterface != Material ||
						 ExistingMaterial.MaterialSlotName != MaterialSlotName ||
						 ExistingMaterial.ImportedMaterialSlotName != MaterialSlotName )
					{
						ExistingMaterial.MaterialInterface = Material;
						ExistingMaterial.MaterialSlotName = MaterialSlotName;
						ExistingMaterial.ImportedMaterialSlotName = MaterialSlotName;
						bMaterialsHaveChanged = true;
					}
				}
				// Add new material
				else
				{
					const bool bEnableShadowCasting = true;
					const bool bRecomputeTangents = false;
					SkeletalMesh->GetMaterials().Add( FSkeletalMaterial( Material, bEnableShadowCasting, bRecomputeTangents, MaterialSlotName, MaterialSlotName ) );
					bMaterialsHaveChanged = true;
				}

				// Already have a material at that LOD remap slot, need to reassign
				if ( LODMaterialMap.IsValidIndex( LODSlotIndex ) )
				{
					LODMaterialMap[ LODSlotIndex ] = SkeletalMeshSlotIndex;
				}
				// Add new material slot remap
				else
				{
					LODMaterialMap.Add( SkeletalMeshSlotIndex );
				}
			}
		}

		return bMaterialsHaveChanged;
	}

	FSHAHash ComputeSHAHash( const TArray<FSkeletalMeshImportData>& LODIndexToSkeletalMeshImportData, TArray<SkeletalMeshImportData::FBone>& ImportedBones )
	{
		FSHA1 HashState;

		for ( const FSkeletalMeshImportData& ImportData : LODIndexToSkeletalMeshImportData )
		{
			HashState.Update( ( uint8* ) ImportData.Points.GetData(), ImportData.Points.Num() * ImportData.Points.GetTypeSize() );
			HashState.Update( ( uint8* ) ImportData.Wedges.GetData(), ImportData.Wedges.Num() * ImportData.Wedges.GetTypeSize() );
			HashState.Update( ( uint8* ) ImportData.Faces.GetData(), ImportData.Faces.Num() * ImportData.Faces.GetTypeSize() );
			HashState.Update( ( uint8* ) ImportData.Influences.GetData(), ImportData.Influences.Num() * ImportData.Influences.GetTypeSize() );
		}

		// Hash the bones as well because it is possible for the mesh to be identical while only the bone configuration changed, and in that case we'd need new skeleton and ref skeleton
		// Maybe in the future (as a separate feature) we could split off the skeleton import so that it could vary independently of the skeletal mesh
		for ( const SkeletalMeshImportData::FBone& Bone : ImportedBones )
		{
			HashState.UpdateWithString( *Bone.Name, Bone.Name.Len() );
			HashState.Update( ( uint8* ) &Bone.Flags, sizeof( Bone.Flags ) );
			HashState.Update( ( uint8* ) &Bone.NumChildren, sizeof( Bone.NumChildren ) );
			HashState.Update( ( uint8* ) &Bone.ParentIndex, sizeof( Bone.ParentIndex ) );
			HashState.Update( ( uint8* ) &Bone.BonePos, sizeof( Bone.BonePos ) );
		}

		FSHAHash OutHash;

		HashState.Final();
		HashState.GetHash( &OutHash.Hash[0] );

		return OutHash;
	}

	FSHAHash ComputeSHAHash( const pxr::UsdSkelSkeletonQuery& InUsdSkeletonQuery, const pxr::UsdPrim& RootMotionPrim )
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UsdSkelRootTranslatorImpl::ComputeSHAHash_SkelQuery );

		FSHAHash OutHash;
		FSHA1 HashState;

		FScopedUsdAllocs Allocs;

		pxr::UsdSkelAnimQuery AnimQuery = InUsdSkeletonQuery.GetAnimQuery();
		if ( !AnimQuery )
		{
			return OutHash;
		}

		pxr::UsdPrim UsdPrim = InUsdSkeletonQuery.GetPrim();
		if ( !UsdPrim )
		{
			return OutHash;
		}

		pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();
		if ( !Stage )
		{
			return OutHash;
		}

		int32 InterpolationType = static_cast< int32 >( Stage->GetInterpolationType() );
		HashState.Update( ( uint8* ) &InterpolationType, sizeof( int32 ) );

		// Time samples for joint transforms
		std::vector<double> TimeData;
		AnimQuery.GetJointTransformTimeSamples( &TimeData );
		HashState.Update( ( uint8* ) TimeData.data(), TimeData.size() * sizeof( double ) );

		// Joint transform values
		pxr::VtArray<pxr::GfMatrix4d> JointTransforms;
		for ( double JointTimeSample : TimeData )
		{
			InUsdSkeletonQuery.ComputeJointLocalTransforms( &JointTransforms, JointTimeSample );
			HashState.Update( ( uint8* ) JointTransforms.data(), JointTransforms.size() * sizeof( pxr::GfMatrix4d ) );
		}

		// restTransforms
		pxr::VtArray<pxr::GfMatrix4d> Transforms;
		const bool bAtRest = true;
		InUsdSkeletonQuery.ComputeJointLocalTransforms( &Transforms, pxr::UsdTimeCode::EarliestTime(), bAtRest );
		HashState.Update( ( uint8* ) Transforms.data(), Transforms.size() * sizeof( pxr::GfMatrix4d ) );

		// bindTransforms
		InUsdSkeletonQuery.GetJointWorldBindTransforms( &Transforms );
		HashState.Update( ( uint8* ) Transforms.data(), Transforms.size() * sizeof( pxr::GfMatrix4d ) );

		// Time samples for blend shape curves
		AnimQuery.GetBlendShapeWeightTimeSamples( &TimeData );
		HashState.Update( ( uint8* ) TimeData.data(), TimeData.size() * sizeof( double ) );

		// Blend shape curve values
		pxr::VtArray< float > WeightsForSample;
		for ( double CurveTimeSample : TimeData )
		{
			AnimQuery.ComputeBlendShapeWeights( &WeightsForSample, pxr::UsdTimeCode( CurveTimeSample ) );
			HashState.Update( ( uint8* ) WeightsForSample.data(), WeightsForSample.size() * sizeof( float ) );
		}

		// If we're pulling root motion from anywhere, hash that too because if it changes we'll need to rebake
		// the AnimSequence asset
		if ( pxr::UsdGeomXformable Xformable{ RootMotionPrim } )
		{
			std::vector<double> TimeSamples;
			Xformable.GetTimeSamples( &TimeSamples );

			pxr::GfMatrix4d Transform{};
			bool bResetsXformSack = false;
			for ( double TimeSample : TimeSamples )
			{
				Xformable.GetLocalTransformation( &Transform, &bResetsXformSack, TimeSample );
				HashState.Update( ( uint8* ) Transform.data(), 16 * sizeof( double ) );
			}

			HashState.Update( ( uint8* ) &bResetsXformSack, sizeof( bool ) );
		}

		HashState.Final();
		HashState.GetHash( &OutHash.Hash[ 0 ] );

		return OutHash;
	}

	void SetMorphTargetWeight( USkeletalMeshComponent& SkeletalMeshComponent, const FString& MorphTargetName, float Weight )
	{
		USkeletalMesh* SkeletalMesh = SkeletalMeshComponent.GetSkeletalMeshAsset();

		// We try keeping a perfect correspondence between SkeletalMesh->GetMorphTargets() and SkeletalMeshComponent.ActiveMorphTargets
		int32 IndexInSkeletalMesh = INDEX_NONE;
		SkeletalMeshComponent.GetSkeletalMeshAsset()->FindMorphTargetAndIndex( *MorphTargetName, IndexInSkeletalMesh );
		if ( IndexInSkeletalMesh == INDEX_NONE )
		{
			return;
		}

		const UMorphTarget* MorphTarget = SkeletalMeshComponent.GetSkeletalMeshAsset()->GetMorphTargets()[ IndexInSkeletalMesh ];
		if ( !MorphTarget )
		{
			return;
		}

		int32 WeightIndex = INDEX_NONE;
		if ( SkeletalMeshComponent.ActiveMorphTargets.Contains( MorphTarget ) )
		{
			WeightIndex = SkeletalMeshComponent.ActiveMorphTargets[ MorphTarget ];
		}

		// Morph target is not at expected location (i.e. after CreateComponents, duplicate for PIE or undo/redo) --> Rebuild ActiveMorphTargets
		// This may lead to one frame of glitchiness, as we'll reset all weights to zero...
		if ( WeightIndex == INDEX_NONE )
		{
			SkeletalMeshComponent.ActiveMorphTargets.Reset();
			SkeletalMeshComponent.MorphTargetWeights.Reset();
			TArray<UMorphTarget*>& MorphTargets = SkeletalMesh->GetMorphTargets();
			for ( int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargets.Num(); ++MorphTargetIndex )
			{
				SkeletalMeshComponent.ActiveMorphTargets.Add( MorphTargets[ MorphTargetIndex ], MorphTargetIndex );
				SkeletalMeshComponent.MorphTargetWeights.Add( 0.0f ); // We'll update these right afterwards when we call UpdateComponents
			}

			WeightIndex = IndexInSkeletalMesh;
		}

		SkeletalMeshComponent.MorphTargetWeights[ WeightIndex ] = Weight;
	}

	bool LoadAllSkeletalData(
		pxr::UsdSkelCache& InSkeletonCache,
		const pxr::UsdSkelRoot& InSkeletonRoot,
		TArray<FSkeletalMeshImportData>& OutLODIndexToSkeletalMeshImportData,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& OutLODIndexToMaterialInfo,
		TArray<SkeletalMeshImportData::FBone>& OutSkeletonBones,
		FName& OutSkeletonName,
		UsdUtils::FBlendShapeMap* OutBlendShapes,
		TSet<FString>& InOutUsedMorphTargetNames,
		const TMap< FString, TMap< FString, int32 > >& InMaterialToPrimvarsUVSetNames,
		float InTime,
		const UUsdAssetCache& AssetCache,
		bool bInInterpretLODs,
		const pxr::TfToken& RenderContext,
		const pxr::TfToken& MaterialPurpose
	)
	{
		if ( !InSkeletonRoot )
		{
			return false;
		}

		FScopedUsdAllocs UsdAllocs;

		pxr::UsdStageRefPtr Stage = InSkeletonRoot.GetPrim().GetStage();
		const FUsdStageInfo StageInfo( Stage );

		std::vector< pxr::UsdSkelBinding > SkeletonBindings;
		InSkeletonCache.Populate( InSkeletonRoot, pxr::UsdTraverseInstanceProxies() );
		InSkeletonCache.ComputeSkelBindings( InSkeletonRoot, &SkeletonBindings, pxr::UsdTraverseInstanceProxies() );
		if ( SkeletonBindings.size() < 1 )
		{
			FUsdLogManager::LogMessage( EMessageSeverity::Warning,
										FText::Format( LOCTEXT("InvalidBinding", "SkelRoot {0} doesn't have any binding. No skinned mesh will be generated."),
										FText::FromString( UsdToUnreal::ConvertPath( InSkeletonRoot.GetPath() ) ) ) );
			return false;
		}

		// Note that there could be multiple skeleton bindings under the SkeletonRoot
		// For now, extract just the first one
		const pxr::UsdSkelBinding& SkeletonBinding = SkeletonBindings[0];
		const pxr::UsdSkelSkeleton& Skeleton = SkeletonBinding.GetSkeleton();
		if ( SkeletonBindings.size() > 1 )
		{
			UE_LOG(LogUsd, Warning, TEXT("Currently only a single skeleton is supported per UsdSkelRoot! '%s' will use skeleton '%s'"),
				*UsdToUnreal::ConvertPath( InSkeletonRoot.GetPrim().GetPath() ),
				*UsdToUnreal::ConvertPath( Skeleton.GetPrim().GetPath() )
			);
		}

		// Import skeleton data
		pxr::UsdSkelSkeletonQuery SkelQuery = InSkeletonCache.GetSkelQuery( Skeleton );
		{
			FSkeletalMeshImportData DummyImportData;
			const bool bSkeletonValid = UsdToUnreal::ConvertSkeleton( SkelQuery, DummyImportData );
			if ( !bSkeletonValid )
			{
				return false;
			}
			OutSkeletonBones = MoveTemp(DummyImportData.RefBonesBinary);
			OutSkeletonName = *UsdToUnreal::ConvertString( SkelQuery.GetSkeleton().GetPrim().GetName() );
		}

		TMap<int32, FSkeletalMeshImportData> LODIndexToSkeletalMeshImportDataMap;
		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfoMap;
		TSet<FString> ProcessedLODParentPaths;

		// Since we may need to switch variants to parse LODs, we could invalidate references to SkinningQuery objects, so we need
		// to keep track of these by path and construct one whenever we need them
		TArray<pxr::SdfPath> PathsToSkinnedPrims;
		for ( const pxr::UsdSkelSkinningQuery& SkinningQuery : SkeletonBinding.GetSkinningTargets() )
		{
			// In USD, the skinning target need not be a mesh, but for Unreal we are only interested in skinning meshes
			if ( pxr::UsdGeomMesh SkinningMesh = pxr::UsdGeomMesh( SkinningQuery.GetPrim() ) )
			{
				PathsToSkinnedPrims.Add(SkinningMesh.GetPrim().GetPath());
			}
		}

		TFunction<bool( const pxr::UsdGeomMesh&, int32 )> ConvertLOD =
		[
			&LODIndexToSkeletalMeshImportDataMap,
			&LODIndexToMaterialInfoMap,
			&SkelQuery,
			InTime,
			&InMaterialToPrimvarsUVSetNames,
			&InOutUsedMorphTargetNames,
			OutBlendShapes,
			&StageInfo,
			RenderContext,
			MaterialPurpose
		]
		( const pxr::UsdGeomMesh& LODMesh, int32 LODIndex )
		{
			pxr::UsdSkelSkinningQuery SkinningQuery = UsdUtils::CreateSkinningQuery( LODMesh, SkelQuery );
			if ( !SkinningQuery )
			{
				return true; // Continue trying other LODs
			}

			if ( LODMesh && LODMesh.ComputeVisibility() == pxr::UsdGeomTokens->invisible )
			{
				return true;
			}

			FSkeletalMeshImportData& LODImportData = LODIndexToSkeletalMeshImportDataMap.FindOrAdd( LODIndex );
			TArray<UsdUtils::FUsdPrimMaterialSlot>& LODSlots = LODIndexToMaterialInfoMap.FindOrAdd( LODIndex ).Slots;

			// BlendShape data is respective to point indices for each mesh in isolation, but we combine all points
			// into one FSkeletalMeshImportData per LOD, so we need to remap the indices using this
			uint32 NumPointsBeforeThisMesh = static_cast< uint32 >( LODImportData.Points.Num() );

			bool bSuccess = UsdToUnreal::ConvertSkinnedMesh(
				SkinningQuery,
				SkelQuery,
				LODImportData,
				LODSlots,
				InMaterialToPrimvarsUVSetNames,
				RenderContext,
				MaterialPurpose
			);
			if ( !bSuccess )
			{
				return true;
			}

			if ( OutBlendShapes )
			{
				pxr::UsdSkelBindingAPI SkelBindingAPI{ LODMesh.GetPrim() };
				pxr::UsdSkelBlendShapeQuery BlendShapeQuery{ SkelBindingAPI };
				if ( BlendShapeQuery )
				{
					for ( int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeQuery.GetNumBlendShapes(); ++BlendShapeIndex )
					{
						UsdToUnreal::ConvertBlendShape(
							BlendShapeQuery.GetBlendShape( BlendShapeIndex ),
							StageInfo,
							LODIndex,
							NumPointsBeforeThisMesh,
							InOutUsedMorphTargetNames,
							*OutBlendShapes
						);
					}
				}
			}

			return true;
		};

		// Actually parse all mesh data
		for ( const pxr::SdfPath& SkinnedPrimPath : PathsToSkinnedPrims )
		{
			pxr::UsdGeomMesh SkinnedMesh{ Stage->GetPrimAtPath( SkinnedPrimPath ) };
			if ( !SkinnedMesh )
			{
				continue;
			}

			pxr::UsdPrim ParentPrim = SkinnedMesh.GetPrim().GetParent();
			FString ParentPrimPath = UsdToUnreal::ConvertPath(ParentPrim.GetPath());

			bool bInterpretedLODs = false;
			if ( bInInterpretLODs && ParentPrim && !ProcessedLODParentPaths.Contains(ParentPrimPath))
			{
				// At the moment we only consider a single mesh per variant, so if multiple meshes tell us to process the same parent prim, we skip.
				// This check would also prevent us from getting in here in case we just have many meshes children of a same prim, outside
				// of a variant. In this case they don't fit the "one mesh per variant" pattern anyway, and we want to fallback to ignoring LODs
				ProcessedLODParentPaths.Add(ParentPrimPath);

				// WARNING: After this is called, references to objects that were inside any of the LOD Meshes will be invalidated!
				bInterpretedLODs = UsdUtils::IterateLODMeshes(ParentPrim, ConvertLOD);
			}

			if ( !bInterpretedLODs )
			{
				// Refresh reference to this prim as it could have been inside a variant that was temporarily switched by IterateLODMeshes
				ConvertLOD( pxr::UsdGeomMesh{ Stage->GetPrimAtPath( SkinnedPrimPath ) }, 0);
			}
		}

		// Place the LODs in order as we can't have e.g. LOD0 and LOD2 without LOD1, and there's no reason downstream code needs to care about
		// what LOD number these data originally wanted to be
		TMap<int32, int32> OldLODIndexToNewLODIndex;
		LODIndexToSkeletalMeshImportDataMap.KeySort( TLess<int32>() );
		OutLODIndexToSkeletalMeshImportData.Reset( LODIndexToSkeletalMeshImportDataMap.Num() );
		OutLODIndexToMaterialInfo.Reset( LODIndexToMaterialInfoMap.Num() );
		for ( TPair<int32, FSkeletalMeshImportData>& Entry : LODIndexToSkeletalMeshImportDataMap )
		{
			FSkeletalMeshImportData& ImportData = Entry.Value;
			if ( Entry.Value.Points.Num() == 0 )
			{
				continue;
			}

			const int32 OldLODIndex = Entry.Key;
			const int32 NewLODIndex = OutLODIndexToSkeletalMeshImportData.Add( MoveTemp( ImportData ) );
			OutLODIndexToMaterialInfo.Add( LODIndexToMaterialInfoMap[ OldLODIndex ] );

			// Keep track of these to remap blendshapes
			OldLODIndexToNewLODIndex.Add( OldLODIndex, NewLODIndex );
		}
		if ( OutBlendShapes )
		{
			for ( auto& Pair : *OutBlendShapes )
			{
				UsdUtils::FUsdBlendShape& BlendShape = Pair.Value;

				TSet<int32> NewLODIndexUsers;
				NewLODIndexUsers.Reserve(BlendShape.LODIndicesThatUseThis.Num());

				for ( int32 OldLODIndexUser : BlendShape.LODIndicesThatUseThis )
				{
					if ( int32* FoundNewLODIndex = OldLODIndexToNewLODIndex.Find( OldLODIndexUser ) )
					{
						NewLODIndexUsers.Add(*FoundNewLODIndex);
					}
					else
					{
						UE_LOG(LogUsd, Error, TEXT("Failed to remap blend shape '%s's LOD index '%d'"), *BlendShape.Name, OldLODIndexUser );
					}
				}

				BlendShape.LODIndicesThatUseThis = MoveTemp(NewLODIndexUsers);
			}
		}

		return true;
	}

	/** Warning: This function will temporarily switch the active LOD variant if one exists, so it's *not* thread safe! */
	void SetMaterialOverrides(
		const pxr::UsdPrim& SkelRootPrim,
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

		pxr::UsdSkelRoot SkelRoot{ SkelRootPrim };
		if ( !SkelRoot )
		{
			return;
		}
		pxr::SdfPath SkelRootPrimPath = SkelRootPrim.GetPath();
		pxr::UsdStageRefPtr Stage = SkelRoot.GetPrim().GetStage();

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

		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfoMap;
		TMap<int32, TSet<UsdUtils::FUsdPrimMaterialSlot>> CombinedSlotsForLODIndex;
		TFunction<bool( const pxr::UsdGeomMesh&, int32 )> IterateLODsLambda =
		[
			&LODIndexToMaterialInfoMap,
			&CombinedSlotsForLODIndex,
			Time,
			RenderContextToken,
			MaterialPurposeToken
		]
		( const pxr::UsdGeomMesh& LODMesh, int32 LODIndex )
		{
			if ( LODMesh && LODMesh.ComputeVisibility() == pxr::UsdGeomTokens->invisible )
			{
				return true;
			}

			TArray<UsdUtils::FUsdPrimMaterialSlot>& CombinedLODSlots = LODIndexToMaterialInfoMap.FindOrAdd( LODIndex ).Slots;
			TSet<UsdUtils::FUsdPrimMaterialSlot>& CombinedLODSlotsSet = CombinedSlotsForLODIndex.FindOrAdd( LODIndex );

			const bool bProvideMaterialIndices = false; // We have no use for material indices and it can be slow to retrieve, as it will iterate all faces
			UsdUtils::FUsdPrimMaterialAssignmentInfo LocalInfo = UsdUtils::GetPrimMaterialAssignments(
				LODMesh.GetPrim(),
				pxr::UsdTimeCode( Time ),
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);

			// Combine material slots in the same order that UsdToUnreal::ConvertSkinnedMesh does
			for ( UsdUtils::FUsdPrimMaterialSlot& LocalSlot : LocalInfo.Slots )
			{
				if ( !CombinedLODSlotsSet.Contains( LocalSlot ) )
				{
					CombinedLODSlots.Add( LocalSlot );
					CombinedLODSlotsSet.Add( LocalSlot );
				}
			}

			return true;
		};

		TSet<FString> ProcessedLODParentPaths;

		// Because we combine all skinning target meshes into a single skeletal mesh, we'll have to reconstruct the combined
		// material assignment info that this SkelRoot wants in order to compare with the existing assignments.
		pxr::UsdSkelCache SkeletonCache;
		SkeletonCache.Populate( SkelRoot, pxr::UsdTraverseInstanceProxies() );
		std::vector< pxr::UsdSkelBinding > SkeletonBindings;
		SkeletonCache.ComputeSkelBindings( SkelRoot, &SkeletonBindings, pxr::UsdTraverseInstanceProxies() );
		for ( const pxr::UsdSkelBinding& Binding : SkeletonBindings )
		{
			for ( const pxr::UsdSkelSkinningQuery& SkinningQuery : Binding.GetSkinningTargets() )
			{
				pxr::UsdPrim MeshPrim = SkinningQuery.GetPrim();
				pxr::UsdGeomMesh Mesh{ MeshPrim };
				if ( !Mesh )
				{
					continue;
				}
				pxr::SdfPath MeshPrimPath = MeshPrim.GetPath();

				pxr::UsdPrim ParentPrim = MeshPrim.GetParent();
				FString ParentPrimPath = UsdToUnreal::ConvertPath( ParentPrim.GetPath() );

				bool bInterpretedLODs = false;
				if ( bInterpretLODs && UsdUtils::IsGeomMeshALOD( MeshPrim ) && !ProcessedLODParentPaths.Contains( ParentPrimPath ) )
				{
					ProcessedLODParentPaths.Add( ParentPrimPath );

					bInterpretedLODs = UsdUtils::IterateLODMeshes( ParentPrim, IterateLODsLambda );
				}

				if ( !bInterpretedLODs )
				{
					// Refresh reference to this prim as it could have been inside a variant that was temporarily switched by IterateLODMeshes
					IterateLODsLambda( pxr::UsdGeomMesh{ Stage->GetPrimAtPath( MeshPrimPath ) }, 0 );
				}
			}

			// We only ever handle the first skeleton binding for now, so lets ignore material
			// overrides on the others
			break;
		}

		// Refresh reference to SkelRootPrim because variant switching potentially invalidated it
		pxr::UsdPrim ValidSkelRootPrim = Stage->GetPrimAtPath( SkelRootPrimPath );

		// Place the LODs in order as we can't have e.g. LOD0 and LOD2 without LOD1, and there's no reason downstream code needs to care about
		// what LOD number these data originally wanted to be
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToAssignments;
		LODIndexToMaterialInfoMap.KeySort( TLess<int32>() );
		for ( TPair<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo>& Entry : LODIndexToMaterialInfoMap )
		{
			LODIndexToAssignments.Add( MoveTemp( Entry.Value ) );
		}

		TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(
			ValidSkelRootPrim,
			LODIndexToAssignments,
			ExistingAssignments,
			AssetCache,
			Time,
			Flags
		);

		// Compare resolved materials with existing assignments, and create overrides if we need to
		uint32 SkeletalMeshSlotIndex = 0;
		for ( int32 LODIndex = 0; LODIndex < LODIndexToAssignments.Num(); ++LODIndex )
		{
			const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToAssignments[ LODIndex ].Slots;
			for ( int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++SkeletalMeshSlotIndex )
			{
				const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[ LODSlotIndex ];

				UMaterialInterface* Material = nullptr;
				if ( UMaterialInterface** FoundMaterial = ResolvedMaterials.Find( &Slot ) )
				{
					Material = *FoundMaterial;
				}
				else
				{
					UE_LOG( LogUsd, Error, TEXT( "Lost track of resolved material for slot '%d' of LOD '%d' for mesh '%s'" ), LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath( ValidSkelRootPrim.GetPath() ) );
					continue;
				}

				UMaterialInterface* ExistingMaterial = ExistingAssignments[ SkeletalMeshSlotIndex ];
				if ( ExistingMaterial == Material )
				{
					continue;
				}
				else
				{
					MeshComponent.SetMaterial( SkeletalMeshSlotIndex, Material );
				}
			}
		}
	}

	bool HasLODSkinningTargets( const pxr::UsdSkelRoot& SkelRoot )
	{
		FScopedUsdAllocs Allocs;

		pxr::UsdSkelCache SkeletonCache;
		SkeletonCache.Populate( SkelRoot, pxr::UsdTraverseInstanceProxies() );

		std::vector< pxr::UsdSkelBinding > SkeletonBindings;
		SkeletonCache.ComputeSkelBindings( SkelRoot, &SkeletonBindings, pxr::UsdTraverseInstanceProxies() );

		for ( const pxr::UsdSkelBinding& Binding : SkeletonBindings )
		{
			for ( const pxr::UsdSkelSkinningQuery& SkinningQuery : Binding.GetSkinningTargets() )
			{
				if ( UsdUtils::IsGeomMeshALOD( SkinningQuery.GetPrim() ) )
				{
					return true;
				}
			}
		}

		return false;
	}

	UPhysicsAsset* GenerateAndAssignPhysicsAsset( USkeletalMesh* SkeletalMesh, EObjectFlags Flags )
	{
#if WITH_EDITOR
		FName AssetName = MakeUniqueObjectName(
			GetTransientPackage(),
			UPhysicsAsset::StaticClass(),
			*FPaths::GetBaseFilename( TEXT( "PHYS_" ) + SkeletalMesh->GetName() )
		);

		UPhysicsAsset* Result = NewObject< UPhysicsAsset >(
			GetTransientPackage(),
			AssetName,
			Flags | EObjectFlags::RF_Public
		);

		FPhysAssetCreateParams NewBodyData;
		FText CreationErrorMessage;
		bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh( Result, SkeletalMesh, NewBodyData, CreationErrorMessage );
		if ( !bSuccess )
		{
			UE_LOG( LogUsd, Error, TEXT( "Couldn't create PhysicsAsset for SkeletalMesh '%s': %s" ),
				*SkeletalMesh->GetName(),
				*CreationErrorMessage.ToString()
			);

			ensure( ObjectTools::DeleteSingleObject( Result ) );

			Result = nullptr;
		}

		return Result;
#else
		return nullptr;
#endif // WITH_EDITOR
	}

	UAnimBlueprint* CreateAnimBlueprint(
		const FUsdSchemaTranslationContext& Context,
		const pxr::UsdPrim& Prim,
		bool bDelayRecompilation = false,  // Whether we should recompile within this function or not
		bool* bOutNeedsRecompile = nullptr // Whether this function wants the returned AnimBP to be recompiled
	)
	{
		if ( !Prim )
		{
			return nullptr;
		}

		FString PrimPath = UsdToUnreal::ConvertPath( Prim.GetPath() );
		FString PrimName = UsdToUnreal::ConvertString( Prim.GetName() );

		USkeletalMesh* SkeletalMesh = Cast< USkeletalMesh >( Context.AssetCache->GetAssetForPrim( PrimPath ) );
		if ( !SkeletalMesh )
		{
			return nullptr;
		}

		USkeleton* Skeleton = SkeletalMesh->GetSkeleton();
		if ( !Skeleton )
		{
			return nullptr;
		}

		// Fetch relevant attributes from prim, since we know it has the schema
		FString AnimBPPath;
		{
			FScopedUsdAllocs Allocs;

			if ( pxr::UsdAttribute Attr = Prim.GetAttribute( UnrealIdentifiers::UnrealAnimBlueprintPath ) )
			{
				std::string PathString;
				if ( Attr.Get( &PathString ) )
				{
					AnimBPPath = UsdToUnreal::ConvertString( PathString );
				}
			}
		}
		if ( AnimBPPath.IsEmpty() )
		{
			return nullptr;
		}

		bool bNeedRecompile = false;

		UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>( FSoftObjectPath( AnimBPPath ).TryLoad() );
		if ( !AnimBP )
		{
			return nullptr;
		}

		// Create transient AnimBP based on our template, so that we can assign it a proper skeleton
		const static FString DefaultAnimBPPath = TEXT( "/USDImporter/Blueprint/DefaultLiveLinkAnimBP.DefaultLiveLinkAnimBP" );
		if ( DefaultAnimBPPath == AnimBPPath )
		{
			const FString CacheKey = UsdToUnreal::ConvertPath( Prim.GetPrimPath() ) + TEXT( "_DefaultAnimBlueprint" );

			// Check if we can find an AnimBP for this prim in the asset cache (useful when doing Action->Import)
			bool bReusedAnimBP = false;
			if ( UAnimBlueprint* CachedAnimBP = Cast< UAnimBlueprint >( Context.AssetCache->GetCachedAsset( CacheKey ) ) )
			{
				if ( CachedAnimBP->TargetSkeleton == Skeleton )
				{
					AnimBP = CachedAnimBP;
					bReusedAnimBP = true;
				}
			}

			// We have to generate a new transient AnimBP
			if ( !bReusedAnimBP )
			{
				FName UniqueName = MakeUniqueObjectName(
					GetTransientPackage(),
					UAnimBlueprint::StaticClass(),
					*( PrimName + TEXT( "_DefaultAnimBlueprint" ) )
				);

				// Duplicate and never reuse these so that they can be assigned independent subject names if desired.
				// Its not as if scenes will have thousands of these anyway.
				AnimBP = DuplicateObject( AnimBP, GetTransientPackage(), UniqueName );

				// Patch up the flags here or else the rest of the engine code (and our plugin code) will get confused
				// as to what a non-transient asset in the transient package even means. It can lead to some crashes
				// when saving after import if we don't do this
				AnimBP->ClearFlags( AnimBP->GetFlags() );
				AnimBP->SetFlags( Skeleton->GetFlags() );

				AnimBP->TargetSkeleton = Skeleton;
				AnimBP->bIsTemplate = false;

				bNeedRecompile = true;

				Context.AssetCache->CacheAsset( CacheKey, AnimBP );
			}
		}
		// Path is pointing to an existing, persistent AnimBP
		else
		{
			// Force skeletons to be compatible if they aren't (we need both ways!)
			if ( !Skeleton->IsCompatible( AnimBP->TargetSkeleton ) )
			{
				AnimBP->TargetSkeleton->AddCompatibleSkeleton( Skeleton );
				Skeleton->AddCompatibleSkeleton( AnimBP->TargetSkeleton );

				if ( AnimBP->TargetSkeleton->GetReferenceSkeleton().GetRefBoneInfo() != Skeleton->GetReferenceSkeleton().GetRefBoneInfo() )
				{
					UE_LOG( LogUsd, Warning, TEXT( "Forcing AnimBlueprint '%s's Skeleton '%s' to be compatible with the Skeleton generated for prim '%s', but they may be different!" ),
						*AnimBP->GetPathName(),
						*AnimBP->TargetSkeleton->GetPathName(),
						*PrimName
					);
				}
			}
		}

		if ( bNeedRecompile && !bDelayRecompilation )
		{
			FCompilerResultsLog Results;
			FBPCompileRequest Request( AnimBP, EBlueprintCompileOptions::None, &Results );
			FBlueprintCompilationManager::CompileSynchronously( Request );
			bNeedRecompile = false;
		}

		if ( bOutNeedsRecompile )
		{
			*bOutNeedsRecompile = bNeedRecompile;
		}

		return AnimBP;
	}

	void UpdateLiveLinkProperties( const FUsdSchemaTranslationContext& Context, USkeletalMeshComponent* Component, const pxr::UsdPrim& Prim )
	{
		if ( !Component || !Component->GetSkeletalMeshAsset() || !Prim )
		{
			return;
		}

		FString PrimName = UsdToUnreal::ConvertString( Prim.GetName() );

		USkeleton* Skeleton = Component->GetSkeletalMeshAsset()->GetSkeleton();
		if ( !Skeleton )
		{
			return;
		}

		UAnimBlueprint* ExistingAnimBP = nullptr;
		if ( Component->AnimClass )
		{
			ExistingAnimBP = Cast<UAnimBlueprint>( Component->AnimClass->ClassGeneratedBy );
		}

		// Fetch relevant attributes from prim, since we know it has the schema
		FString SubjectName;
		FString AnimBPPath;
		{
			FScopedUsdAllocs Allocs;

			if ( pxr::UsdAttribute Attr = Prim.GetAttribute( UnrealIdentifiers::UnrealLiveLinkSubjectName ) )
			{
				std::string SubjectNameString;
				if ( Attr.Get( &SubjectNameString ) )
				{
					SubjectName = UsdToUnreal::ConvertString( SubjectNameString );
				}
			}

			if ( pxr::UsdAttribute Attr = Prim.GetAttribute( UnrealIdentifiers::UnrealAnimBlueprintPath ) )
			{
				std::string PathString;
				if ( Attr.Get( &PathString ) )
				{
					AnimBPPath = UsdToUnreal::ConvertString( PathString );
				}
			}
		}

		UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>( FSoftObjectPath( AnimBPPath ).TryLoad() );

		// Check if we need to change the AnimBP
		bool bNeedRecompile = false;
		if ( ExistingAnimBP != AnimBP )
		{
			const bool bDelayRecompilation = true;
			AnimBP = UsdSkelRootTranslatorImpl::CreateAnimBlueprint(
				Context,
				Prim,
				bDelayRecompilation,
				&bNeedRecompile
			);
		}

		// Apply subject name to live link pose AnimBlueprint node
		// Reference: UAnimationBlueprintLibrary::AddNodeAssetOverride
		if ( AnimBP )
		{
			TArray<UBlueprint*> BlueprintHierarchy;
			AnimBP->GetBlueprintHierarchyFromClass( AnimBP->GetAnimBlueprintGeneratedClass(), BlueprintHierarchy );

			TArray<UAnimGraphNode_LiveLinkPose*> LiveLinkNodes;

			for ( int32 BlueprintIndex = 0; BlueprintIndex < BlueprintHierarchy.Num(); ++BlueprintIndex )
			{
				UBlueprint* CurrentBlueprint = BlueprintHierarchy[ BlueprintIndex ];

				TArray<UEdGraph*> Graphs;
				CurrentBlueprint->GetAllGraphs( Graphs );

				for ( UEdGraph* Graph : Graphs )
				{
					for ( UEdGraphNode* Node : Graph->Nodes )
					{
						if ( UAnimGraphNode_LiveLinkPose* AnimNode = Cast<UAnimGraphNode_LiveLinkPose>( Node ) )
						{
							LiveLinkNodes.Add( AnimNode );
						}
					}
				}
			}

			if ( LiveLinkNodes.Num() > 1 && !ExistingAnimBP )
			{
				UE_LOG( LogUsd, Warning, TEXT( "Found more than one LiveLinkPose blueprint node on AnimBlueprint '%s's graphs."
					"Note that all of those nodes will have their LiveLink SubjectName's updated to '%s', as described for prim '%s'!" ),
					*AnimBP->GetPathName(),
					*SubjectName,
					*UsdToUnreal::ConvertPath( Prim.GetPrimPath() )
				);
			}

			for ( UAnimGraphNode_LiveLinkPose* Node : LiveLinkNodes )
			{
				const UEdGraphSchema* Schema = Node->GetSchema();
				if ( !Schema )
				{
					continue;
				}

				UEdGraphPin* SubjectNamePin = nullptr;
				for ( UEdGraphPin* Pin : Node->Pins )
				{
					if ( Pin->GetName() == GET_MEMBER_NAME_STRING_CHECKED( FAnimNode_LiveLinkPose, LiveLinkSubjectName ) )
					{
						SubjectNamePin = Pin;
						break;
					}
				}

				// The subject name pin is already connected to something...
				if ( SubjectNamePin->LinkedTo.Num() != 0 )
				{
					if ( !ExistingAnimBP )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Failed to update a LiveLinkPose node's 'Subject Name' to '%s' on AnimBlueprint '%s', "
							"because the pin is already connected to some other node. Disconnect it if you want it to be updated automatically." ),
							*SubjectName,
							*AnimBP->GetPathName()
						);
					}

					continue;
				}

				if ( SubjectNamePin )
				{
					// The pin type is FLiveLinkSubjectName, so we must create an instance of it and serialize it using
					// UScriptStruct::ExportText to generate a proper default value string
					FLiveLinkSubjectName Dummy;
					Dummy.Name = *SubjectName;

					FString ValueString;
					const void* Defaults = nullptr;
					const UObject* OwnerObject = nullptr;
					const int32 PortFlags = EPropertyPortFlags::PPF_None;
					const UObject* ExportRootScope = nullptr;
					FLiveLinkSubjectName::StaticStruct()->ExportText( ValueString, &Dummy, nullptr, nullptr, EPropertyPortFlags::PPF_None, nullptr );

					if ( !Schema->DoesDefaultValueMatch( *SubjectNamePin, ValueString ) )
					{
						SubjectNamePin->Modify();
						Schema->TrySetDefaultValue( *SubjectNamePin, ValueString );

						FBlueprintEditorUtils::MarkBlueprintAsModified( AnimBP );
						bNeedRecompile = true;
					}
				}
			}
		}

		if ( bNeedRecompile )
		{
			FCompilerResultsLog Results;
			FBPCompileRequest Request( AnimBP, EBlueprintCompileOptions::None, &Results );
			FBlueprintCompilationManager::CompileSynchronously( Request );

			// We need to force the component to update its anim after we regenerate the blueprint class
			Component->ClearAnimScriptInstance();
			Component->InitAnim( true );
		}

		if ( AnimBP != ExistingAnimBP )
		{
			// This can internally change AnimationMode, but lets revert it to what it was so that we can control it from
			// that single place in ::UpdateComponents
			EAnimationMode::Type OldMode = Component->GetAnimationMode();
			Component->SetAnimInstanceClass( AnimBP->GeneratedClass );
			Component->SetAnimationMode( OldMode );
		}
	}

	class FSkelRootCreateAssetsTaskChain : public FUsdSchemaTranslatorTaskChain
	{
	public:
		explicit FSkelRootCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath );

	protected:
		// Inputs
		UE::FSdfPath PrimPath;
		TSharedRef< FUsdSchemaTranslationContext > Context;

		// Outputs
		TArray<FSkeletalMeshImportData> LODIndexToSkeletalMeshImportData;
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfo;
		TArray<SkeletalMeshImportData::FBone> SkeletonBones;
		FName SkeletonName;
		UsdUtils::FBlendShapeMap NewBlendShapes;

		// Note that we want this to be case insensitive so that our UMorphTarget FNames are unique not only due to case differences
		TSet<FString> UsedMorphTargetNames;
		TUsdStore< pxr::UsdSkelCache > SkeletonCache;

		// Don't keep a live reference to the prim because other translators may mutate the stage in an ExclusiveSync translation step, invalidating the reference
		UE::FUsdPrim GetPrim() const { return Context->Stage.GetPrimAtPath( PrimPath ); }

		void SetupTasks();
	};

	FSkelRootCreateAssetsTaskChain::FSkelRootCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath )
		: PrimPath( InPrimPath )
		, Context( InContext )
	{
		SetupTasks();
	}

	void FSkelRootCreateAssetsTaskChain::SetupTasks()
	{
		// Ignore prims from disabled purposes
		if ( !EnumHasAllFlags( Context->PurposesToLoad, IUsdPrim::GetPurpose( GetPrim() ) ) )
		{
			return;
		}

		// To parse all LODs we need to actively switch variant sets to other variants (triggering prim loading/unloading and notices),
		// which could cause race conditions if other async translation tasks are trying to access those prims
		ESchemaTranslationLaunchPolicy LaunchPolicy = ESchemaTranslationLaunchPolicy::Async;
		if ( Context->bAllowInterpretingLODs && HasLODSkinningTargets( pxr::UsdSkelRoot( GetPrim() ) ) )
		{
			LaunchPolicy = ESchemaTranslationLaunchPolicy::ExclusiveSync;
		}

		// Create SkeletalMeshImportData (Async or ExclusiveSync)
		Do( LaunchPolicy,
			[ this ]()
			{
				// No point in importing blend shapes if the import context doesn't want them
				UsdUtils::FBlendShapeMap* OutBlendShapes = Context->BlendShapesByPath ? &NewBlendShapes : nullptr;

				TMap< FString, TMap< FString, int32 > > Unused;
				const TMap< FString, TMap< FString, int32 > >* MaterialToPrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex ? Context->MaterialToPrimvarToUVIndex : &Unused;

				pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
				if ( !Context->RenderContext.IsNone() )
				{
					RenderContextToken = UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
				}

				pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
				if ( !Context->MaterialPurpose.IsNone() )
				{
					MaterialPurposeToken = UnrealToUsd::ConvertToken( *Context->MaterialPurpose.ToString() ).Get();
				}

				const bool bContinueTaskChain = UsdSkelRootTranslatorImpl::LoadAllSkeletalData(
					SkeletonCache.Get(),
					pxr::UsdSkelRoot( GetPrim() ),
					LODIndexToSkeletalMeshImportData,
					LODIndexToMaterialInfo,
					SkeletonBones,
					SkeletonName,
					OutBlendShapes,
					UsedMorphTargetNames,
					*MaterialToPrimvarToUVIndex,
					Context->Time,
					*Context->AssetCache.Get(),
					Context->bAllowInterpretingLODs,
					RenderContextToken,
					MaterialPurposeToken
				);

				return bContinueTaskChain;
			} );

		// Create USkeletalMesh (Main thread)
		Then( ESchemaTranslationLaunchPolicy::Sync,
			[ this ]()
			{
				FString SkelRootPath = PrimPath.GetString();
				FSHAHash SkeletalMeshHash = UsdSkelRootTranslatorImpl::ComputeSHAHash( LODIndexToSkeletalMeshImportData, SkeletonBones );
				const FString SkeletalMeshHashString = SkeletalMeshHash.ToString();

				USkeletalMesh* SkeletalMesh = Cast< USkeletalMesh >( Context->AssetCache->GetCachedAsset( SkeletalMeshHashString ) );

				bool bIsNew = false;
				if ( !SkeletalMesh )
				{
					bIsNew = true;
					SkeletalMesh = UsdToUnreal::GetSkeletalMeshFromImportData(
						LODIndexToSkeletalMeshImportData,
						SkeletonBones,
						NewBlendShapes,
						Context->ObjectFlags,
						*FPaths::GetBaseFilename( SkelRootPath ),
						SkeletonName
					);
				}

				if ( SkeletalMesh )
				{
					if ( bIsNew )
					{
						const bool bMaterialsHaveChanged = UsdSkelRootTranslatorImpl::ProcessMaterials(
							GetPrim(),
							LODIndexToMaterialInfo,
							SkeletalMesh,
							*Context->AssetCache.Get(),
							Context->Time,
							Context->ObjectFlags,
							NewBlendShapes.Num() > 0
						);

						if ( bMaterialsHaveChanged )
						{
							const bool bRebuildAll = true;
							SkeletalMesh->UpdateUVChannelData( bRebuildAll );
						}

						UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( SkeletalMesh, TEXT( "USDAssetImportData" ) );
						ImportData->PrimPath = SkelRootPath;
						SkeletalMesh->SetAssetImportData(ImportData);

						Context->AssetCache->CacheAsset( SkeletalMeshHashString, SkeletalMesh );
						Context->AssetCache->CacheAsset( SkeletalMeshHashString + TEXT( "_Skeleton" ), SkeletalMesh->GetSkeleton() );
					}

					if ( bGeneratePhysicsAssets )
					{
						if ( !SkeletalMesh->GetPhysicsAsset() )
						{
							UPhysicsAsset* PhysicsAsset = UsdSkelRootTranslatorImpl::GenerateAndAssignPhysicsAsset(
								SkeletalMesh,
								Context->ObjectFlags
							);

							if ( PhysicsAsset )
							{
								Context->AssetCache->CacheAsset(
									SkeletalMeshHashString + TEXT( "_PhysicsAsset" ),
									PhysicsAsset
								);
							}
						}
					}
					else
					{
						// Actively clear this so that if we toggle the cvar and reload we'll clear our physics assets
						SkeletalMesh->SetPhysicsAsset( nullptr );
					}

					Context->AssetCache->LinkAssetToPrim( SkelRootPath, SkeletalMesh );

					// Track our Skeleton by the source skeleton prim path
					if ( USkeleton* Skeleton = SkeletalMesh->GetSkeleton() )
					{
						FScopedUsdAllocs Allocs;

						// Our SkeletonCache should already have been populated by our call to UsdSkelRootTranslatorImpl::LoadAllSkeletalData, so we should
						// be able to quickly query our skeleton again to find its path
						std::vector< pxr::UsdSkelBinding > SkeletonBindings;
						SkeletonCache.Get().ComputeSkelBindings( pxr::UsdSkelRoot( GetPrim() ), &SkeletonBindings, pxr::UsdTraverseInstanceProxies() );
						if ( SkeletonBindings.size() > 0 )
						{
							pxr::UsdSkelBinding& SkeletonBinding = SkeletonBindings[ 0 ];
							const pxr::UsdSkelSkeleton& UsdSkeleton = SkeletonBinding.GetSkeleton();
							Context->AssetCache->LinkAssetToPrim( UsdToUnreal::ConvertPath( UsdSkeleton.GetPrim().GetPrimPath() ), Skeleton );
						}
					}

					// We may be reusing a skeletal mesh we got in the cache, but we always need the BlendShapesByPath stored on the
					// actor to be up-to-date with the Skeletal Mesh that is actually being displayed
					if ( Context->BlendShapesByPath )
					{
						Context->BlendShapesByPath->Append( NewBlendShapes );
					}
				}

				// Continuing even if the mesh is not new as we currently don't add the SkelAnimation info to the mesh hash, so the animations
				// may have changed
				return true;
			} );

		// Create AnimBP asset if we need to
		Then( ESchemaTranslationLaunchPolicy::Sync,
			[ this ]()
			{
				UE::FUsdPrim Prim = GetPrim();

				// We need to *also* create the AnimBP within CreateAssets so that we will still generate it even if
				// we never call Create/UpdateComponents (e.g. importing without importing actors).
				const bool bPrimHasLiveLinkSchema = UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::LiveLinkAPI );
				if ( bPrimHasLiveLinkSchema )
				{
					TSharedPtr< FUsdSchemaTranslationContext > ContextPtr = Context;

					// When importing, we can't delay creating the AnimBP to the next frame as that will be after the
					// import. We don't need to worry about deadlocks though, because we will never *import* as a
					// response to a USD event: It's always an intentional function call, where the Python GIL is
					// properly handled
					if ( ContextPtr && ContextPtr->bIsImporting )
					{
						UsdSkelRootTranslatorImpl::CreateAnimBlueprint( *ContextPtr.Get(), Prim );
					}
					else
					{
						// HACK. c.f. the large comment on the analogous timer inside
						// FUsdSkelRootTranslator::UpdateComponents
						FTSTicker::GetCoreTicker().AddTicker(
							FTickerDelegate::CreateLambda( [ContextPtr, Prim]( float Time )
							{
								if ( ContextPtr )
								{
									UsdSkelRootTranslatorImpl::CreateAnimBlueprint( *ContextPtr.Get(), Prim );
								}

								// Returning false means this is a one-off, and won't repeat
								return false;
							})
						);
					}
				}

				return true;
			});

		// Create UAnimSequences (requires a completed USkeleton. Main thread as some steps of the animation compression require it)
		Then( ESchemaTranslationLaunchPolicy::Sync,
			[ this ]()
			{
				if ( !Context->bAllowParsingSkeletalAnimations )
				{
					return false;
				}

				USkeletalMesh* SkeletalMesh = Cast< USkeletalMesh >( Context->AssetCache->GetAssetForPrim( PrimPath.GetString() ) );
				if ( !SkeletalMesh )
				{
					return false;
				}

				FScopedUsdAllocs Allocs;

				if ( pxr::UsdSkelRoot SkeletonRoot{ GetPrim() } )
				{
					std::vector< pxr::UsdSkelBinding > SkeletonBindings;
					SkeletonCache.Get().Populate( SkeletonRoot, pxr::UsdTraverseInstanceProxies() );
					SkeletonCache.Get().ComputeSkelBindings( SkeletonRoot, &SkeletonBindings, pxr::UsdTraverseInstanceProxies() );

					for ( const pxr::UsdSkelBinding& Binding : SkeletonBindings )
					{
						const pxr::UsdSkelSkeleton& Skeleton = Binding.GetSkeleton();
						pxr::UsdSkelSkeletonQuery SkelQuery = SkeletonCache.Get().GetSkelQuery( Skeleton );
						pxr::UsdSkelAnimQuery AnimQuery = SkelQuery.GetAnimQuery();
						if ( !AnimQuery )
						{
							continue;
						}

						pxr::UsdPrim SkelAnimationPrim = AnimQuery.GetPrim();
						if ( !SkelAnimationPrim )
						{
							continue;
						}
						FString SkelAnimationPrimPath = UsdToUnreal::ConvertPath( SkelAnimationPrim.GetPath() );

						std::vector<double> JointTimeSamples;
						std::vector<double> BlendShapeTimeSamples;
						if ( ( !AnimQuery.GetJointTransformTimeSamples( &JointTimeSamples ) || JointTimeSamples.size() == 0 ) &&
							( NewBlendShapes.Num() == 0 || ( !AnimQuery.GetBlendShapeWeightTimeSamples( &BlendShapeTimeSamples ) || BlendShapeTimeSamples.size() == 0 ) ) )
						{
							continue;
						}

						pxr::UsdPrim RootMotionPrim = Context->RootMotionHandling == EUsdRootMotionHandling::UseMotionFromSkelRoot
							? SkeletonRoot.GetPrim()
							: Context->RootMotionHandling == EUsdRootMotionHandling::UseMotionFromSkeleton
								? Skeleton.GetPrim()
								: pxr::UsdPrim{};

						FSHAHash Hash = UsdSkelRootTranslatorImpl::ComputeSHAHash( SkelQuery, RootMotionPrim );
						FString HashString = Hash.ToString();
						UAnimSequence* AnimSequence = Cast< UAnimSequence >( Context->AssetCache->GetCachedAsset( HashString ) );

						if ( !AnimSequence || AnimSequence->GetSkeleton() != SkeletalMesh->GetSkeleton() )
						{
							FScopedUnrealAllocs UEAllocs;

							// The UAnimSequence can't be created with the RF_Transactional flag, or else it will be serialized without
							// Bone/CurveCompressionSettings. Undoing that transaction would call UAnimSequence::Serialize with nullptr values for both, which crashes.
							// Besides, this particular asset type is only ever created when we import to content folder assets (so never for realtime), and
							// in that case we don't need it to be transactional anyway
							AnimSequence = NewObject<UAnimSequence>( GetTransientPackage(), NAME_None, Context->ObjectFlags & ~EObjectFlags::RF_Transactional );
							AnimSequence->SetSkeleton( SkeletalMesh->GetSkeleton() );

							// This is read back in the USDImporter, so that if we ever import this AnimSequence we will always also import the SkeletalMesh for it
							AnimSequence->SetPreviewMesh( SkeletalMesh );

							TUsdStore<pxr::VtArray<pxr::UsdSkelSkinningQuery>> SkinningTargets = Binding.GetSkinningTargets();
							float LayerStartOffsetSeconds = 0.0f;
							UsdToUnreal::ConvertSkelAnim(
								SkelQuery,
								&SkinningTargets.Get(),
								&NewBlendShapes,
								Context->bAllowInterpretingLODs,
								RootMotionPrim,
								AnimSequence,
								&LayerStartOffsetSeconds
							);

							if (AnimSequence->GetDataModel()->GetNumBoneTracks() != 0 || AnimSequence->GetDataModel()->GetNumberOfFloatCurves() != 0 )
							{
								UUsdAnimSequenceAssetImportData* ImportData = NewObject< UUsdAnimSequenceAssetImportData >( AnimSequence, TEXT( "USDAssetImportData" ) );
								AnimSequence->AssetImportData = ImportData;

								ImportData->PrimPath = SkelAnimationPrimPath;
								ImportData->LayerStartOffsetSeconds = LayerStartOffsetSeconds;

								Context->AssetCache->CacheAsset( HashString, AnimSequence );
							}
							else
							{
								AnimSequence->MarkAsGarbage();
							}
						}

						if ( AnimSequence )
						{
							Context->AssetCache->LinkAssetToPrim( SkelAnimationPrimPath, AnimSequence );
						}

						// For now we shouldn't try to parse the SkelAnimations from skeletal bindings other than the first one as we only
						// ever actually parse the first skeleton from the SkelRoot (check LoadAllSkeletalData). This is not only a matter of
						// not wasting time, because as we would try reusing the first generated USkeleton for all other UAnimSequences,
						// they'd likely end up not being usable or crash due to unexpected number of joints/blend shapes/etc.
						break;
					}
				}

				return true;
			});
	}

#endif // WITH_EDITOR
}

void FUsdSkelRootTranslator::CreateAssets()
{
#if WITH_EDITOR
	// Importing skeletal meshes actually works in Standalone mode, but we intentionally block it here
	// to not confuse users as to why it doesn't work at runtime
	TSharedRef< UsdSkelRootTranslatorImpl::FSkelRootCreateAssetsTaskChain > AssetsTaskChain =
		MakeShared< UsdSkelRootTranslatorImpl::FSkelRootCreateAssetsTaskChain >( Context, PrimPath );

	Context->TranslatorTasks.Add( MoveTemp( AssetsTaskChain ) );
#endif // WITH_EDITOR
}

USceneComponent* FUsdSkelRootTranslator::CreateComponents()
{
	USceneComponent* SceneComponent = FUsdGeomXformableTranslator::CreateComponents();

#if WITH_EDITOR
	// Check if the prim has the GroomBinding schema and setup the component and assets necessary to bind the groom to the SkeletalMesh
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
#endif // WITH_EDITOR

	return SceneComponent;
}

void FUsdSkelRootTranslator::UpdateComponents( USceneComponent* SceneComponent )
{
	USkeletalMeshComponent* SkeletalMeshComponent = Cast< USkeletalMeshComponent >( SceneComponent );
	if ( !SkeletalMeshComponent )
	{
		return;
	}

	UE::FUsdPrim Prim = GetPrim();

	const bool bPrimHasLiveLinkSchema = UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::LiveLinkAPI );
	const bool bPrimHasControlRigSchema = UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::ControlRigAPI );

	bool bPrimHasLiveLinkEnabled = bPrimHasLiveLinkSchema;
	if ( bPrimHasLiveLinkSchema )
	{
		FScopedUsdAllocs Allocs;

		if ( pxr::UsdAttribute Attr = pxr::UsdPrim{ Prim }.GetAttribute( UnrealIdentifiers::UnrealLiveLinkEnabled ) )
		{
			bool bEnabled = true;
			if ( Attr.Get( &bEnabled ) )
			{
				bPrimHasLiveLinkEnabled = bEnabled;
			}
		}
	}

	SkeletalMeshComponent->SetAnimationMode(
		bPrimHasLiveLinkEnabled
			? EAnimationMode::AnimationBlueprint
			: Context->bSequencerIsAnimating
				? EAnimationMode::AnimationCustomMode
				: EAnimationMode::AnimationSingleNode
	);

	UE::FUsdPrim SkelAnimPrim = UsdUtils::FindFirstAnimationSource( Prim );
	if ( SkelAnimPrim )
	{
		UAnimSequence* TargetAnimSequence = Cast< UAnimSequence >( Context->AssetCache->GetAssetForPrim( SkelAnimPrim.GetPrimPath().GetString() ) );
		if ( TargetAnimSequence != SkeletalMeshComponent->AnimationData.AnimToPlay )
		{
			SkeletalMeshComponent->AnimationData.AnimToPlay = TargetAnimSequence;
			SkeletalMeshComponent->AnimationData.bSavedLooping = false;
			SkeletalMeshComponent->AnimationData.bSavedPlaying = false;
			SkeletalMeshComponent->SetAnimation( TargetAnimSequence );
		}
	}

	Super::UpdateComponents( SceneComponent );

	// We always want this, but need to be registered for this to work (Super::UpdateComponents should register us)
	const bool bNewUpdateState = true;
	SkeletalMeshComponent->SetUpdateAnimationInEditor( bNewUpdateState );

#if WITH_EDITOR
	// Re-set the skeletal mesh if we created a new one (maybe the hash changed, a skinned UsdGeomMesh was hidden, etc.)
	USkeletalMesh* TargetSkeletalMesh = Cast< USkeletalMesh >( Context->AssetCache->GetAssetForPrim( PrimPath.GetString() ) );
	if ( SkeletalMeshComponent->GetSkeletalMeshAsset() != TargetSkeletalMesh )
	{
		SkeletalMeshComponent->SetSkeletalMesh(TargetSkeletalMesh);

		// Handle material overrides
		if ( TargetSkeletalMesh )
		{
			TArray<UMaterialInterface*> ExistingAssignments;
			for ( FSkeletalMaterial& SkeletalMaterial : TargetSkeletalMesh->GetMaterials())
			{
				ExistingAssignments.Add( SkeletalMaterial.MaterialInterface );
			}

			UsdSkelRootTranslatorImpl::SetMaterialOverrides(
				Prim,
				ExistingAssignments,
				*SkeletalMeshComponent,
				*Context->AssetCache.Get(),
				Context->Time,
				Context->ObjectFlags,
				Context->bAllowInterpretingLODs,
				Context->RenderContext,
				Context->MaterialPurpose
			);
		}
	}

	if ( bPrimHasLiveLinkSchema )
	{
		TSharedPtr< FUsdSchemaTranslationContext > ContextPtr = Context;
		TStrongObjectPtr< USkeletalMeshComponent > PinnedSkelMeshComponent{ SkeletalMeshComponent };

		if ( ContextPtr && ContextPtr->bIsImporting )
		{
			UsdSkelRootTranslatorImpl::UpdateLiveLinkProperties( *ContextPtr.Get(), PinnedSkelMeshComponent.Get(), Prim );
		}
		else
		{
			// HACK: This is a temporary work-around for a GIL deadlock. See UE-156489 and UE-156370 for more details, but
			// the long-story short of it is that at this point we may have a callstack that originates from Python,
			// triggers a USD stage notice and causes C++ code to run as our stage actor is listening to them. If we
			// cause GC to run right now (which the DuplicateObject inside UpdateLiveLinkProperties will) we may cause
			// a deadlock, as the game thread still holds the GIL and a background reference collector thread may want
			// to acquire it too. What this does is run this part of the UpdateComponents on tick, that has a callstack
			// that doesn't originate from Python, and so doesn't have the GIL locked
			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda( [ContextPtr, PinnedSkelMeshComponent, Prim]( float Time )
				{
					if ( ContextPtr )
					{
						UsdSkelRootTranslatorImpl::UpdateLiveLinkProperties( *ContextPtr.Get(), PinnedSkelMeshComponent.Get(), Prim );
					}

					// Returning false means this is a one-off, and won't repeat
					return false;
				})
			);
		}
	}

	// Update the animation state
	// Don't try animating ourselves if the sequencer is animating as it will just overwrite the animation state on next
	// tick anyway, and all this would do is lead to flickering and other issues
	if ( !Context->bSequencerIsAnimating && SkeletalMeshComponent->GetSkeletalMeshAsset() && !bPrimHasLiveLinkEnabled )
	{
		if ( UAnimSequence* AnimSequence = Cast<UAnimSequence>( SkeletalMeshComponent->AnimationData.AnimToPlay.Get() ) )
		{
			UE::FSdfLayerOffset CombinedOffset;
			if ( !SkelAnimPrim )
			{
				SkelAnimPrim = UsdUtils::FindFirstAnimationSource( Prim );
			}
			if ( SkelAnimPrim )
			{
				CombinedOffset = UsdUtils::GetPrimToStageOffset( SkelAnimPrim );
			}

			double LayerStartOffsetSeconds = 0.0f;
			if ( UUsdAnimSequenceAssetImportData* ImportData = Cast<UUsdAnimSequenceAssetImportData>( AnimSequence->AssetImportData ) )
			{
				LayerStartOffsetSeconds = ImportData->LayerStartOffsetSeconds;
			}

			// Always change the mode here because the sequencer will change it back to AnimationCustomMode when animating
			SkeletalMeshComponent->SetAnimationMode( EAnimationMode::AnimationSingleNode );

			// Part of the CombinedOffset will be due to a framerate difference. We don't care about that part here though, so remove it
			const double TimeCodesPerSecondDifference = Context->Stage.GetTimeCodesPerSecond() / AnimSequence->ImportFileFramerate;
			CombinedOffset.Scale /= TimeCodesPerSecondDifference;

			// Always use the sequence's framerate here because we need to sample the UAnimSequence with in seconds, and that
			// asset may have been created when the stage had a different framesPerSecond (and was reused by the assets cache)
			// Use the import framerate here because we will need to change the sampling framerate of the sequence in order to get it
			// to match the target duration in seconds and the number of source frames.
			const double LayerTimeCode = ( ( Context->Time - CombinedOffset.Offset ) / CombinedOffset.Scale );
			const double AnimSequenceTime = LayerTimeCode / AnimSequence->ImportFileFramerate;
			SkeletalMeshComponent->SetPosition( AnimSequenceTime - LayerStartOffsetSeconds );

			SkeletalMeshComponent->TickAnimation( 0.f, false );
			SkeletalMeshComponent->RefreshBoneTransforms();
			SkeletalMeshComponent->RefreshFollowerComponents();
			SkeletalMeshComponent->UpdateComponentToWorld();
			SkeletalMeshComponent->FinalizeBoneTransform();
			SkeletalMeshComponent->MarkRenderTransformDirty();
			SkeletalMeshComponent->MarkRenderDynamicDataDirty();
		}
	}

	// If the prim has a GroomBinding schema, apply the target groom to its associated GroomComponent
	if ( UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::GroomBindingAPI ) )
	{
		UsdGroomTranslatorUtils::SetGroomFromPrim( Prim, *Context->AssetCache, SceneComponent );
	}
#endif // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK
