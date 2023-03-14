// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDGeomPointInstancerTranslator.h"

#include "MeshTranslationImpl.h"
#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDGeomMeshTranslator.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDSchemasModule.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/UsdTyped.h"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

#if USE_USD_SDK

#include "Async/Async.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"

#include "USDIncludesStart.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/stage.h"
	#include "pxr/usd/usdGeom/camera.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/pointInstancer.h"
	#include "pxr/usd/usdGeom/xform.h"
	#include "pxr/usd/usdGeom/xformable.h"
#include "USDIncludesEnd.h"

#define LOCTEXT_NAMESPACE "USDGeomPointInstancer"

namespace UsdGeomPointInstancerTranslatorImpl
{
	void ApplyPointInstanceTransforms( UInstancedStaticMeshComponent* Component, TArray<FTransform>& InstanceTransforms )
	{
		if ( Component )
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ApplyPointInstanceTransforms);

			Component->AddInstances( InstanceTransforms, false );
		}
	}

	void SetStaticMesh( UStaticMesh* StaticMesh, UStaticMeshComponent& MeshComponent )
	{
		if ( StaticMesh == MeshComponent.GetStaticMesh() )
		{
			return;
		}

		if ( MeshComponent.IsRegistered() )
		{
			MeshComponent.UnregisterComponent();
		}

		if ( StaticMesh )
		{
			StaticMesh->CreateBodySetup(); // BodySetup is required for HISM component
		}

		MeshComponent.SetStaticMesh( StaticMesh );

		MeshComponent.RegisterComponent();
	}
}

FUsdGeomPointInstancerCreateAssetsTaskChain::FUsdGeomPointInstancerCreateAssetsTaskChain(
	const TSharedRef< FUsdSchemaTranslationContext >& InContext,
	const UE::FSdfPath& InPrimPath,
	bool bInIgnoreTopLevelTransformAndVisibility
)
	: FBuildStaticMeshTaskChain( InContext, InPrimPath )
	, bIgnoreTopLevelTransformAndVisibility( bInIgnoreTopLevelTransformAndVisibility )
{
	SetupTasks();
}

void FUsdGeomPointInstancerCreateAssetsTaskChain::SetupTasks()
{
	FScopedUnrealAllocs UnrealAllocs;

	// Create mesh description (Async)
	Do( ESchemaTranslationLaunchPolicy::Async,
		[this]() -> bool
		{
			// TODO: Restore support for LOD prototypes
			LODIndexToMeshDescription.Reset( 1 );
			LODIndexToMaterialInfo.Reset( 1 );

			FMeshDescription& AddedMeshDescription = LODIndexToMeshDescription.Emplace_GetRef();
			UsdUtils::FUsdPrimMaterialAssignmentInfo& AssignmentInfo = LODIndexToMaterialInfo.Emplace_GetRef();

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

			UsdToUnreal::ConvertGeomMeshHierarchy(
				GetPrim(),
				AddedMeshDescription,
				AssignmentInfo,
				Options,
				bIgnoreTopLevelTransformAndVisibility
			);

			return !AddedMeshDescription.IsEmpty();
		});

	FBuildStaticMeshTaskChain::SetupTasks();
}

void FUsdGeomPointInstancerTranslator::CreateAssets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomPointInstancerTranslator::CreateAssets );

	FScopedUsdAllocs Allocs;

	pxr::UsdPrim Prim = GetPrim();
	pxr::UsdGeomPointInstancer PointInstancer( Prim );
	if ( !PointInstancer )
	{
		return;
	}

	// If another FUsdGeomXformableTranslator is collapsing the point instancer prim, it will do so by calling
	// UsdToUnreal::ConvertGeomMeshHierarchy which will consume the prim directly.
	// This case right here is if we're collapsing *ourselves*, where we'll essentially pretend we're a single static mesh.
	if ( Context->bCollapseTopLevelPointInstancers )
	{
		// Don't bake our actual point instancer's transform or visibility into the mesh as its nice to have these on the static mesh component instead
		const bool bIgnoreTopLevelTransformAndVisibility = true;
		Context->TranslatorTasks.Add( MakeShared< FUsdGeomPointInstancerCreateAssetsTaskChain >( Context, PrimPath, bIgnoreTopLevelTransformAndVisibility ) );
	}
	// Otherwise we're just going to spawn HISM components instead
	else
	{
		const pxr::UsdRelationship& Prototypes = PointInstancer.GetPrototypesRel();

		pxr::SdfPathVector PrototypePaths;
		if ( !Prototypes.GetTargets( &PrototypePaths ) )
		{
			return;
		}

		for ( const pxr::SdfPath& PrototypePath : PrototypePaths )
		{
			pxr::UsdPrim PrototypeUsdPrim = Prim.GetStage()->GetPrimAtPath( PrototypePath );
			UE::FSdfPath UEPrototypePath{ PrototypePath };

			if ( !PrototypeUsdPrim )
			{
				UE_LOG( LogUsd, Warning, TEXT( "Failed to find prototype '%s' for PointInstancer '%s' when collapsing assets" ), *UEPrototypePath.GetString(), *PrimPath.GetString() );
				continue;
			}

			if ( Context->bAllowInterpretingLODs && UsdUtils::DoesPrimContainMeshLODs( PrototypeUsdPrim ) )
			{
				// We have to provide one of the LOD meshes to the task chain, so find the path to one
				UE::FSdfPath ChildMeshPath;
				pxr::UsdPrimSiblingRange PrimRange = PrototypeUsdPrim.GetChildren();
				for ( pxr::UsdPrimSiblingRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt )
				{
					const pxr::UsdPrim& Child = *PrimRangeIt;
					if ( pxr::UsdGeomMesh ChildMesh{ Child } )
					{
						ChildMeshPath = UE::FSdfPath{ Child.GetPrimPath() };
						break;
					}
				}

				// This is in charge of baking in 'Prim's transform into the generated static mesh for the prototype, as it
				// otherwise wouldn't end up anywhere else. Note that in the default/export case 'Prim' (the prim that actually contains the LOD
				// variant set) is schema-less (and so not an Xform), but this is just in case the user manually made it an Xform instead
				FTransform AdditionalUESpaceTransform = FTransform::Identity;
				if ( pxr::UsdGeomXform ParentXform{ PrototypeUsdPrim } )
				{
					// Skip this LOD mesh if its invisible
					pxr::TfToken Visibility;
					pxr::UsdAttribute VisibilityAttr = ParentXform.GetVisibilityAttr();
					if ( VisibilityAttr && VisibilityAttr.Get( &Visibility, Context->Time ) && Visibility == pxr::UsdGeomTokens->invisible )
					{
						continue;
					}

					// TODO: Handle the resetXformStack op for LOD parents
					bool bOutResetTransformStack = false;
					UsdToUnreal::ConvertXformable( Prim.GetStage(), ParentXform, AdditionalUESpaceTransform, Context->Time, &bOutResetTransformStack );
				}

				Context->TranslatorTasks.Add( MakeShared< FGeomMeshCreateAssetsTaskChain >( Context, ChildMeshPath, AdditionalUESpaceTransform ) );
			}
			else
			{
				// Fully bake the prototype top level transform and visibility into its own static mesh
				const bool bIgnoreTopLevelTransformAndVisibility = false;
				Context->TranslatorTasks.Add( MakeShared< FUsdGeomPointInstancerCreateAssetsTaskChain >( Context, UEPrototypePath, bIgnoreTopLevelTransformAndVisibility ) );
			}
		}
	}
}

USceneComponent* FUsdGeomPointInstancerTranslator::CreateComponents()
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomPointInstancerTranslator::CreateComponents );

	// If we're collapsing ourselves, we're really just a collapsed Xform prim, so let that translator handle it
	if ( Context->bCollapseTopLevelPointInstancers )
	{
		return FUsdGeomXformableTranslator::CreateComponents();
	}

	// Otherwise, the plan here is to create an USceneComponent that corresponds to the PointInstancer prim itself, and then spawn a child
	// HISM component for each prototype.
	// We always request a scene component here explicitly or else we'll be upgraded to a static mesh component by the mechanism that
	// handles collapsed meshes/static mesh components for the GeomXFormable translator.
	USceneComponent* MainSceneComponent = CreateComponentsEx( { USceneComponent::StaticClass() }, {} );
	if ( !MainSceneComponent )
	{
		return MainSceneComponent;
	}
	UpdateComponents( MainSceneComponent );

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim Prim = GetPrim();
	pxr::UsdGeomPointInstancer PointInstancer( Prim );
	if ( !PointInstancer )
	{
		return MainSceneComponent;
	}

	pxr::SdfPathVector PrototypePaths;
	if ( !PointInstancer.GetPrototypesRel().GetTargets( &PrototypePaths ) )
	{
		return MainSceneComponent;
	}

	UUsdAssetCache& AssetCache = *Context->AssetCache.Get();

	// Lets pretend ParentComponent is pointing to the parent USceneComponent while we create the child HISMs, so they get
	// automatically attached to it as children
	TGuardValue< USceneComponent* > ParentComponentGuard{ Context->ParentComponent, MainSceneComponent };

	TArray<TFuture<TTuple<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>>>> Tasks;
	FScopedSlowTask PrototypePathsSlowTask( ( float ) PrototypePaths.size(), LOCTEXT( "GeomPointCreateComponents", "Creating HierarchicalInstancedStaticMeshComponents for point instancers" ) );
	for ( int32 PrototypeIndex = 0; PrototypeIndex < PrototypePaths.size(); ++PrototypeIndex )
	{
		PrototypePathsSlowTask.EnterProgressFrame();

		const pxr::SdfPath& PrototypePath = PrototypePaths[ PrototypeIndex ];
		FString PrototypePathStr = UsdToUnreal::ConvertPath( PrototypePath );

		pxr::UsdPrim PrototypeUsdPrim = Prim.GetStage()->GetPrimAtPath( PrototypePath );
		if ( !PrototypeUsdPrim )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Failed to find prototype '%s' for PointInstancer '%s' when creating components" ), *PrototypePathStr, *PrimPath.GetString() );
			continue;
		}

		// If our prototype was a LOD mesh we will have used the path of one of the actual LOD meshes to start the FGeomMeshCreateAssetsTaskChain,
		// so we have to look for our resulting mesh with the same path
		if ( Context->bAllowInterpretingLODs && UsdUtils::DoesPrimContainMeshLODs( PrototypeUsdPrim ) )
		{
			pxr::UsdPrimSiblingRange PrimRange = PrototypeUsdPrim.GetChildren();
			for ( pxr::UsdPrimSiblingRange::iterator PrimRangeIt = PrimRange.begin(); PrimRangeIt != PrimRange.end(); ++PrimRangeIt )
			{
				const pxr::UsdPrim& Child = *PrimRangeIt;
				if ( pxr::UsdGeomMesh ChildMesh{ Child } )
				{
					PrototypeUsdPrim = Child;
					PrototypePathStr = UsdToUnreal::ConvertPath( Child.GetPrimPath() );
					break;
				}
			}
		}

		using UHISMComponent = UHierarchicalInstancedStaticMeshComponent;

		FUsdGeomXformableTranslator XformableTranslator{ UHISMComponent::StaticClass(), Context, UE::FUsdTyped( PrototypeUsdPrim ) };

		const bool bNeedsActor = false;
		UHISMComponent* HISMComponent = Cast<UHISMComponent>( XformableTranslator.CreateComponentsEx( { UHISMComponent::StaticClass() }, bNeedsActor ) );
		if ( !HISMComponent )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Failed to generate HISM component for prototype '%s' for PointInstancer '%s'" ), *PrototypePathStr, *PrimPath.GetString() );
			continue;
		}

		UStaticMesh* StaticMesh = Cast< UStaticMesh >( AssetCache.GetAssetForPrim( PrototypePathStr ) );
		UsdGeomPointInstancerTranslatorImpl::SetStaticMesh( StaticMesh, *HISMComponent );

		// Evaluating point instancer can take a long time and is thread-safe. Move to async task while we work on something else.
		pxr::UsdTimeCode TimeCode{ Context->Time };
		FUsdStageInfo StageInfo{ Prim.GetStage() };
		Tasks.Emplace(
			Async(
				EAsyncExecution::ThreadPool,
				[ TimeCode, StageInfo, PointInstancer, PrototypeIndex, HISMComponent ]()
				{
					TArray<FTransform> InstanceTransforms;
					UsdUtils::GetPointInstancerTransforms( StageInfo, PointInstancer, PrototypeIndex, TimeCode, InstanceTransforms );

					return MakeTuple( HISMComponent, MoveTemp( InstanceTransforms ) );
				}
			)
		);

		// Handle material overrides
		if ( StaticMesh )
		{
#if WITH_EDITOR
			// If the prim paths match, it means that it was this prim that created (and so "owns") the static mesh,
			// so its material assignments will already be directly on the mesh. If they differ, we're using some other prim's mesh,
			// so we may need material overrides on our component
			UUsdAssetImportData* UsdImportData = Cast<UUsdAssetImportData>( StaticMesh->AssetImportData );
			if ( UsdImportData && UsdImportData->PrimPath != PrototypePathStr )
#endif // WITH_EDITOR
			{
				TArray<UMaterialInterface*> ExistingAssignments;
				for ( FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials() )
				{
					ExistingAssignments.Add( StaticMaterial.MaterialInterface );
				}

				MeshTranslationImpl::SetMaterialOverrides(
					PrototypeUsdPrim,
					ExistingAssignments,
					*HISMComponent,
					AssetCache,
					Context->Time,
					Context->ObjectFlags,
					Context->bAllowInterpretingLODs,
					Context->RenderContext,
					Context->MaterialPurpose
				);
			}
		}
	}

	// Wait on and assign results of the point instancer.
	for ( auto& Future : Tasks )
	{
		TTuple<UHierarchicalInstancedStaticMeshComponent*, TArray<FTransform>> Result{ Future.Get() };
		UsdGeomPointInstancerTranslatorImpl::ApplyPointInstanceTransforms( Result.Key, Result.Value );
	}

	return MainSceneComponent;
}

void FUsdGeomPointInstancerTranslator::UpdateComponents( USceneComponent* PointInstancerRootComponent )
{
	Super::UpdateComponents( PointInstancerRootComponent );
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK

