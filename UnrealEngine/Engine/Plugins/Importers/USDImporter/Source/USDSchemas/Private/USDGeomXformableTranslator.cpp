// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDGeomXformableTranslator.h"

#include "UnrealUSDWrapper.h"
#include "USDAssetCache.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDGeomMeshTranslator.h"
#include "USDIntegrationUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDPrimConversion.h"
#include "USDSchemasModule.h"
#include "USDTypesConversion.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/LightComponentBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "LiveLinkComponentController.h"
#include "LiveLinkRole.h"
#include "Modules/ModuleManager.h"
#include "Roles/LiveLinkTransformRole.h"
#include "StaticMeshAttributes.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdGeomXformable.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
	#include "pxr/usd/kind/registry.h"
	#include "pxr/usd/usd/modelAPI.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usd/primRange.h"
	#include "pxr/usd/usd/variantSets.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/pointInstancer.h"
	#include "pxr/usd/usdGeom/subset.h"
	#include "pxr/usd/usdGeom/xformable.h"
	#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "USDIncludesEnd.h"

static bool GCollapsePrimsWithoutKind = true;
static FAutoConsoleVariableRef CVarCollapsePrimsWithoutKind(
	TEXT( "USD.CollapsePrimsWithoutKind" ),
	GCollapsePrimsWithoutKind,
	TEXT( "Allow collapsing prims that have no authored 'Kind' value" ) );

static int32 GMaxNumVerticesCollapsedMesh = 5000000;
static FAutoConsoleVariableRef CVarMaxNumVerticesCollapsedMesh(
	TEXT( "USD.MaxNumVerticesCollapsedMesh" ),
	GMaxNumVerticesCollapsedMesh,
	TEXT( "Maximum number of vertices that a combined Mesh can have for us to collapse it into a single StaticMesh" ) );

static bool GEnableCollision = true;
static FAutoConsoleVariableRef CVarEnableCollision(
	TEXT( "USD.EnableCollision" ),
	GEnableCollision,
	TEXT( "Whether to have collision enabled for spawned components and generated meshes" ) );

namespace UE::UsdXformableTranslatorImpl::Private
{
	void SetUpSceneComponentForLiveLink( const FUsdSchemaTranslationContext& Context, USceneComponent* Component, const pxr::UsdPrim& Prim )
	{
		if ( !Component || !Prim )
		{
			return;
		}

		AActor* Parent = Component->GetOwner();
		if ( !Parent )
		{
			return;
		}

		USceneComponent* RootComponent = Parent->GetRootComponent();
		if ( !RootComponent )
		{
			return;
		}

		ULiveLinkComponentController* Controller = nullptr;
		{
			// We would have to traverse all top-level actor components to know if our component is set up for live link already
			// or not, so this just helps us make that a little bit faster. Its important because UpdateComponents (which calls us)
			// is the main function that is called to animate components, so it can be spammed in case this prim has animations
			static TMap<TWeakObjectPtr<USceneComponent>, TWeakObjectPtr<ULiveLinkComponentController>> LiveLinkEnabledComponents;
			if ( ULiveLinkComponentController* ExistingController = LiveLinkEnabledComponents.FindRef( Component ).Get() )
			{
				// We found an existing controller we created to track this component, so use that
				Controller = ExistingController;
			}
			// We don't know of any controllers handling this component yet, get a new one
			else
			{
				TArray<ULiveLinkComponentController*> LiveLinkComponents;
				Parent->GetComponents( LiveLinkComponents );

				for ( ULiveLinkComponentController* LiveLinkComponent : LiveLinkComponents )
				{
					if ( LiveLinkComponent->GetControlledComponent( ULiveLinkTransformRole::StaticClass() ) == Component )
					{
						// We found some other controller handling this component somehow, use that
						Controller = LiveLinkComponent;
						break;
					}
				}

				if ( !Controller )
				{
					// We'll get a warning from the live link controller component in case the component its controlling is not movable
					Component->Mobility = EComponentMobility::Movable;

					Controller = NewObject<ULiveLinkComponentController>( Parent, NAME_None, Context.ObjectFlags );
					Controller->bUpdateInEditor = true;

					// Important because of how ULiveLinkComponentController::TickComponent also checks for the sequencer
					// tag to try and guess if the controlled component is a spawnable
					Controller->bDisableEvaluateLiveLinkWhenSpawnable = false;

					Parent->AddInstanceComponent( Controller );
					Controller->RegisterComponent();
				}

				if ( Controller )
				{
					LiveLinkEnabledComponents.Add( Component, Controller );
				}
			}
		}

		// Configure controller with our desired parameters
		if ( Controller )
		{
			FScopedUsdAllocs Allocs;

			FLiveLinkSubjectRepresentation SubjectRepresentation;
			SubjectRepresentation.Role = ULiveLinkTransformRole::StaticClass();

			if ( pxr::UsdAttribute Attr = Prim.GetAttribute( UnrealIdentifiers::UnrealLiveLinkSubjectName ) )
			{
				std::string SubjectName;
				if ( Attr.Get( &SubjectName ) )
				{
					SubjectRepresentation.Subject = FName{ *UsdToUnreal::ConvertString( SubjectName ) };
				}
			}
			Controller->SetSubjectRepresentation( SubjectRepresentation );

			// This should be done after setting the subject representation to ensure that the LiveLink component's ControllerMap has a transform controller
			Controller->SetControlledComponent( ULiveLinkTransformRole::StaticClass(), Component );

			if ( pxr::UsdAttribute Attr = Prim.GetAttribute( UnrealIdentifiers::UnrealLiveLinkEnabled ) )
			{
				bool bEnabled = true;
				if ( Attr.Get( &bEnabled ) )
				{
					Controller->bEvaluateLiveLink = bEnabled;
				}
			}
		}
	}

	void RemoveLiveLinkFromComponent( USceneComponent* Component )
	{
		if ( !Component )
		{
			return;
		}

		AActor* Parent = Component->GetOwner();
		if ( !Parent )
		{
			return;
		}

		TArray<ULiveLinkComponentController*> LiveLinkComponents;
		Parent->GetComponents( LiveLinkComponents );

		for ( ULiveLinkComponentController* LiveLinkComponent : LiveLinkComponents )
		{
			if ( LiveLinkComponent->GetControlledComponent( ULiveLinkTransformRole::StaticClass() ) == Component )
			{
				LiveLinkComponent->SetControlledComponent( ULiveLinkTransformRole::StaticClass(), nullptr );
				Parent->RemoveInstanceComponent( LiveLinkComponent );
				break;
			}
		}
	}
}

class FUsdGeomXformableCreateAssetsTaskChain : public FBuildStaticMeshTaskChain
{
public:
	explicit FUsdGeomXformableCreateAssetsTaskChain( const TSharedRef< FUsdSchemaTranslationContext >& InContext, const UE::FSdfPath& InPrimPath )
		: FBuildStaticMeshTaskChain( InContext, InPrimPath )
	{
		SetupTasks();
	}

protected:
	virtual void SetupTasks() override;
};

void FUsdGeomXformableCreateAssetsTaskChain::SetupTasks()
{
	FScopedUnrealAllocs UnrealAllocs;

	// Create mesh description (Async)
	Do( ESchemaTranslationLaunchPolicy::Async,
		[ this ]() -> bool
		{
			// We will never have multiple LODs of meshes that were collapsed together, as LOD'd meshes don't collapse. So just parse the mesh we get as LOD0
			LODIndexToMeshDescription.Reset(1);
			LODIndexToMaterialInfo.Reset(1);

			FMeshDescription& AddedMeshDescription = LODIndexToMeshDescription.Emplace_GetRef();
			UsdUtils::FUsdPrimMaterialAssignmentInfo& AssignmentInfo = LODIndexToMaterialInfo.Emplace_GetRef();

			TMap< FString, TMap< FString, int32 > > Unused;
			TMap< FString, TMap< FString, int32 > >* MaterialToPrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex ? Context->MaterialToPrimvarToUVIndex : &Unused;

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if ( !Context->RenderContext.IsNone() )
			{
				RenderContextToken = UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
			}

			// We're going to put Prim's transform and visibility on the component, so we don't need to bake it into the combined mesh
			const bool bSkipRootPrimTransformAndVis = true;

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
				bSkipRootPrimTransformAndVis
			);

			return !AddedMeshDescription.IsEmpty();
		} );

	FBuildStaticMeshTaskChain::SetupTasks();
}

void FUsdGeomXformableTranslator::CreateAssets()
{
	if ( !CollapsesChildren( ECollapsingType::Assets ) )
	{
		// We only have to create assets when our children are collapsed together
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomMeshTranslator::CreateAssets );

	Context->TranslatorTasks.Add( MakeShared< FUsdGeomXformableCreateAssetsTaskChain >( Context, PrimPath ) );
}

FUsdGeomXformableTranslator::FUsdGeomXformableTranslator( TSubclassOf< USceneComponent > InComponentTypeOverride, TSharedRef< FUsdSchemaTranslationContext > InContext, const UE::FUsdTyped& InSchema )
	: FUsdSchemaTranslator( InContext, InSchema )
	, ComponentTypeOverride( InComponentTypeOverride )
{
}

USceneComponent* FUsdGeomXformableTranslator::CreateComponents()
{
	USceneComponent* Component = CreateComponentsEx( {}, {} );

	// We pulled UpdateComponents outside CreateComponentsEx as in some cases we don't want to do it
	// right away (like on FUsdGeomPointInstancerTranslator::CreateComponents)
	UpdateComponents( Component );
	return Component;
}

USceneComponent* FUsdGeomXformableTranslator::CreateComponentsEx( TOptional< TSubclassOf< USceneComponent > > ComponentType, TOptional< bool > bNeedsActor )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomXformableTranslator::CreateComponentsEx );

	if ( !Context->IsValid() )
	{
		return nullptr;
	}

	UE::FUsdPrim Prim = GetPrim();
	if ( !Prim )
	{
		return nullptr;
	}

	FScopedUnrealAllocs UnrealAllocs;

	if ( !bNeedsActor.IsSet() )
	{
		// Don't add components to the AUsdStageActor or the USDStageImport 'scene actor'
		UE::FUsdPrim ParentPrim = Prim.GetParent();
		bool bIsTopLevelPrim = ParentPrim.IsValid() && ParentPrim.IsPseudoRoot();

		// If we don't have any parent prim with a type that generates a component, we are still technically a top-level prim
		if ( !bIsTopLevelPrim )
		{
			bool bHasParentComponent = false;
			while ( ParentPrim.IsValid() )
			{
				if ( UE::FUsdGeomXformable( ParentPrim ) )
				{
					bHasParentComponent = true;
					break;
				}

				ParentPrim = ParentPrim.GetParent();
			}
			if ( !bHasParentComponent )
			{
				bIsTopLevelPrim = true;
			}
		}

		auto PrimNeedsActor = []( const UE::FUsdPrim& UsdPrim ) -> bool
		{
			return  UsdPrim.IsPseudoRoot() ||
					UsdPrim.IsModel() ||
					UsdPrim.IsGroup() ||
					UsdUtils::HasCompositionArcs( UsdPrim ) ||
					UsdPrim.HasAttribute( TEXT( "unrealCameraPrimName" ) );  // If we have this, then we correspond to the root component
																			 // of an exported ACineCameraActor. Let's create an actual
																			 // CineCameraActor here so that our child camera prim can just
																			 // take it's UCineCameraComponent instead
		};

		bNeedsActor =
		(
			bIsTopLevelPrim ||
			Context->ParentComponent == nullptr ||
			PrimNeedsActor( Prim )
		);

		// We don't want to start a component hierarchy if one of our child will break it by being an actor
		if ( !bNeedsActor.GetValue() )
		{
			TFunction< bool( const UE::FUsdPrim& ) > RecursiveChildPrimsNeedsActor;
			RecursiveChildPrimsNeedsActor = [ PrimNeedsActor, &RecursiveChildPrimsNeedsActor ]( const UE::FUsdPrim& UsdPrim ) -> bool
			{
				const bool bTraverseInstanceProxies = true;
				for ( const pxr::UsdPrim& Child : UsdPrim.GetFilteredChildren( bTraverseInstanceProxies ) )
				{
					if ( PrimNeedsActor( UE::FUsdPrim( Child ) ) )
					{
						return true;
					}
					else if ( RecursiveChildPrimsNeedsActor( UE::FUsdPrim( Child ) ) )
					{
						return true;
					}
				}

				return false;
			};

			bNeedsActor = RecursiveChildPrimsNeedsActor( UE::FUsdPrim( Prim ) );
		}
	}

	USceneComponent* SceneComponent = nullptr;
	UObject* ComponentOuter = nullptr;

	// Can't have public or standalone on spawned actors and components because that
	// will lead to asserts when trying to collect them during a level change, or when
	// trying to replace them (right-clicking from the world outliner)
	EObjectFlags ComponentFlags = Context->ObjectFlags & ~RF_Standalone & ~RF_Public;

	if ( bNeedsActor.GetValue() )
	{
		// Spawn actor
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags = ComponentFlags;
		SpawnParameters.OverrideLevel =  Context->Level;
		SpawnParameters.Name = Prim.GetName();
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested; // Will generate a unique name in case of a conflict

		UClass* ActorClass = UsdUtils::GetActorTypeForPrim( Prim );
		AActor* SpawnedActor = Context->Level->GetWorld()->SpawnActor( ActorClass, nullptr, SpawnParameters );

		if ( SpawnedActor )
		{
#if WITH_EDITOR
			const bool bMarkDirty = false;
			SpawnedActor->SetActorLabel( Prim.GetName().ToString(), bMarkDirty );

			// If our AUsdStageActor is in a hidden level/layer and we spawn actors, they should also be hidden
			if ( Context->ParentComponent )
			{
				if ( AActor* ParentActor = Context->ParentComponent->GetOwner() )
				{
					SpawnedActor->bHiddenEdLevel = ParentActor->bHiddenEdLevel;
					SpawnedActor->bHiddenEdLayer = ParentActor->bHiddenEdLayer;
				}
			}
#endif // WITH_EDITOR

			// Hack to show transient actors in world outliner
			if (SpawnedActor->HasAnyFlags(EObjectFlags::RF_Transient))
			{
				SpawnedActor->Tags.AddUnique( TEXT("SequencerActor") );
			}

			SceneComponent = SpawnedActor->GetRootComponent();

			ComponentOuter = SpawnedActor;
		}
	}
	else
	{
		ComponentOuter = Context->ParentComponent;
	}

	if ( !ComponentOuter )
	{
		UE_LOG( LogUsd, Warning, TEXT("Invalid outer when trying to create SceneComponent for prim (%s)"), *PrimPath.GetString() );
		return nullptr;
	}

	if ( !SceneComponent )
	{
		if ( !ComponentType.IsSet() )
		{
			if ( ComponentTypeOverride.IsSet() )
			{
				ComponentType = ComponentTypeOverride.GetValue();
			}
			else
			{
				ComponentType = UsdUtils::GetComponentTypeForPrim( Prim );

				// For now only upgrade actual scene components to static mesh components (important because skeletal mesh components will also fit this
				// criteria but we don't want to use a static mesh component for those)
				if ( CollapsesChildren( ECollapsingType::Assets ) && ComponentType.IsSet() && ComponentType.GetValue() == USceneComponent::StaticClass() )
				{
					// If we're a type that collapses assets, we should probably be a static mesh component as we only really collapse static meshes together right now.
					// We can't just check if there's a static mesh for this prim on the cache, because the prims with meshes could be potentially invisible (and so
					// we don't have parsed their meshes yet), so here we traverse our child hierarchy and if we have any chance of ever generating a Mesh, we go
					// for a static mesh component
					TArray< UE::FUsdPrim > ChildMeshPrims = UsdUtils::GetAllPrimsOfType( Prim, TEXT( "UsdGeomMesh" ) );
					if ( ChildMeshPrims.Num() > 0 )
					{
						ComponentType = UStaticMeshComponent::StaticClass();
					}
				}
				// If this is a component for a point instancer that just collapsed itself into a static mesh, just make
				// a static mesh component that can receive it
				else if ( Context->bCollapseTopLevelPointInstancers && pxr::UsdPrim{ Prim }.IsA<pxr::UsdGeomPointInstancer>() )
				{
					ComponentType = UStaticMeshComponent::StaticClass();
				}
			}
		}

		if ( ComponentType.IsSet() && ComponentType.GetValue() != nullptr )
		{
			const FName ComponentName = MakeUniqueObjectName( ComponentOuter, ComponentType.GetValue(), FName( Prim.GetName() ) );
			SceneComponent = NewObject< USceneComponent >( ComponentOuter, ComponentType.GetValue(), ComponentName, ComponentFlags );

			if ( AActor* Owner = SceneComponent->GetOwner() )
			{
				Owner->AddInstanceComponent( SceneComponent );
			}
		}
	}

	if ( SceneComponent )
	{
		if ( !GEnableCollision )
		{
			// In most cases this will have no benefit memory-wise, as regular UStaticMeshComponents build their physics meshes anyway
			// when registering, regardless of these. HISM components will *not* build them though, so disabling the cvar may lead
			// to some memory savings
			if ( UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>( SceneComponent ) )
			{
				PrimComp->SetCollisionEnabled( ECollisionEnabled::NoCollision );
				PrimComp->SetCollisionProfileName( UCollisionProfile::NoCollision_ProfileName );
			}
		}

		if ( !SceneComponent->GetOwner()->GetRootComponent() )
		{
			SceneComponent->GetOwner()->SetRootComponent( SceneComponent );
		}

		// If we're spawning into a level that is being streamed in, our construction scripts will be rerun, and may want to set the scene component
		// location again. Since our spawned actors are already initialized, that may trigger a warning about the component not being movable,
		// so we must force them movable here
		const bool bIsAssociating = Context->Level && Context->Level->bIsAssociatingLevel;
		const bool bParentIsMovable = Context->ParentComponent && Context->ParentComponent->Mobility == EComponentMobility::Movable;

		// Don't call SetMobility as it would trigger a reregister, queuing unnecessary rhi commands since this is a brand new component
		if ( bIsAssociating || bParentIsMovable )
		{
			SceneComponent->Mobility = EComponentMobility::Movable;
		}
		else
		{
			SceneComponent->Mobility = UsdUtils::IsAnimated( Prim )
				? EComponentMobility::Movable
				: SceneComponent->IsA<ULightComponentBase>()	// Lights need to be stationary by default
					? EComponentMobility::Stationary
					: EComponentMobility::Static;
		}

		// Attach to parent
		// Do this before UpdatingComponents as we may need to use the parent transform to set a world transform directly
		// (in case of resetXformStack). Besides, this is more consistent anyway as during stage updates we'll call
		// UpdateComponents with all the components already attached
		SceneComponent->AttachToComponent( Context->ParentComponent, FAttachmentTransformRules::KeepRelativeTransform );

		if ( !SceneComponent->IsRegistered() )
		{
			SceneComponent->RegisterComponent();
		}
	}

	return SceneComponent;
}

void FUsdGeomXformableTranslator::UpdateComponents( USceneComponent* SceneComponent )
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomXformableTranslator::UpdateComponents );

	if ( SceneComponent )
	{
		SceneComponent->Modify();

		// UsdToUnreal::ConvertXformable will set a new transform, which will emit warnings during PIE/Runtime if the component
		// has Static mobility, so here we unregister, set the new transform value, and reregister below
		const bool bStaticMobility = SceneComponent->Mobility == EComponentMobility::Static;
		if ( bStaticMobility && SceneComponent->IsRegistered() )
		{
			SceneComponent->UnregisterComponent();
		}

		// If the user modified a mesh parameter (e.g. vertex color), the hash will be different and it will become a separate asset
		// so we must check for this and assign the new StaticMesh
		bool bHasMultipleLODs = false;
		if ( UStaticMeshComponent* StaticMeshComponent = Cast< UStaticMeshComponent >( SceneComponent ) )
		{
			UStaticMesh* PrimStaticMesh = Cast< UStaticMesh >( Context->AssetCache->GetAssetForPrim( PrimPath.GetString() ) );
			if ( PrimStaticMesh )
			{
				bHasMultipleLODs = PrimStaticMesh->GetNumLODs() > 1;
			}

			if ( PrimStaticMesh != StaticMeshComponent->GetStaticMesh() )
			{
				// Need to make sure the mesh's resources are initialized here as it may have just been built in another thread
				// Only do this if required though, as this mesh could using these resources currently (e.g. PIE and editor world sharing the mesh)
				if ( PrimStaticMesh && !PrimStaticMesh->AreRenderingResourcesInitialized() )
				{
					PrimStaticMesh->InitResources();
				}

				if ( StaticMeshComponent->IsRegistered() )
				{
					StaticMeshComponent->UnregisterComponent();
				}

				StaticMeshComponent->SetStaticMesh( PrimStaticMesh );

				// We can't register yet, as UsdToUnreal::ConvertXformable below us may want to move the component.
				// We'll always re-register when needed below, though.
			}
		}

		UE::FUsdPrim Prim = GetPrim();

		// Handle LiveLink, but only i we're not a skeletal mesh component: The SkelRootTranslator will deal with the
		// skeletal version of the LiveLink configuration, we only handle setting up LiveLink for simple transforms
		if ( !SceneComponent->IsA<USkeletalMeshComponent>() )
		{
			if ( UsdUtils::PrimHasSchema( Prim, UnrealIdentifiers::LiveLinkAPI ) )
			{
				UE::UsdXformableTranslatorImpl::Private::SetUpSceneComponentForLiveLink( Context.Get(), SceneComponent, Prim );
			}
			else
			{
				UE::UsdXformableTranslatorImpl::Private::RemoveLiveLinkFromComponent( SceneComponent );
			}
		}

		// Only put the transform into the component if we haven't parsed LODs for our static mesh: The Mesh transforms will already be baked
		// into the mesh at that case, as each LOD could technically have a separate transform
		if ( !Context->bAllowInterpretingLODs || !bHasMultipleLODs )
		{
			// Don't update the component's transform if this is already factored in as root motion within the AnimSequence
			bool bConvertTransform = true;
			if ( Prim.IsA( TEXT( "SkelRoot" ) ) && Context->RootMotionHandling == EUsdRootMotionHandling::UseMotionFromSkelRoot )
			{
				bConvertTransform = false;
			}

			UsdToUnreal::ConvertXformable( Context->Stage, pxr::UsdGeomXformable( Prim ), *SceneComponent, Context->Time, bConvertTransform );
		}

		// Note how we should only register if we unregistered ourselves: If we did this every time we would
		// register too early during the process of duplicating into PIE, and that would prevent a future RegisterComponent
		// call from naturally creating the required render state
		if ( !SceneComponent->IsRegistered() )
		{
			SceneComponent->RegisterComponent();
		}
	}
}

bool FUsdGeomXformableTranslator::CollapsesChildren( ECollapsingType CollapsingType ) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdGeomXformableTranslator::CollapsesChildren );

	if ( Context->InfoCache.IsValid() )
	{
		return Context->InfoCache->DoesPathCollapseChildren( PrimPath, CollapsingType );
	}

	bool bCollapsesChildren = false;

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdPrim Prim = GetPrim();
	pxr::UsdModelAPI Model{ pxr::UsdTyped( Prim ) };

	if ( Model )
	{
		EUsdDefaultKind PrimKind = UsdUtils::GetDefaultKind( Prim );
		bCollapsesChildren = Context->KindsToCollapse != EUsdDefaultKind::None && ( EnumHasAnyFlags( Context->KindsToCollapse, PrimKind ) || ( PrimKind == EUsdDefaultKind::None && GCollapsePrimsWithoutKind ) );

		if ( !bCollapsesChildren )
		{
			// Temp support for the prop kind
			bCollapsesChildren = Model.IsKind( pxr::TfToken( "prop" ), pxr::UsdModelAPI::KindValidationNone );
		}

		if ( bCollapsesChildren )
		{
			IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >( TEXT("USDSchemas") );

			// TODO: This can be optimized in order to make FUsdInfoCache::RebuildCacheForSubtree faster: If we have a child prim that we know doesn't collapse,
			// any of our parents should be able to know they can't collapse *us* either.
			// This is somewhat niche though: Realistically to waste time here a prim and its children need to have a kind that allows collapsing, and also not be able to collapse.
			// Also, if any of these prims *does* manage to collapse, FUsdInfoCache will already not actually query the subtree children if they can collapse or not anymore,
			// and just consider them collapsed by the parent
			TArray< TUsdStore< pxr::UsdPrim > > ChildXformPrims = UsdUtils::GetAllPrimsOfType( Prim, pxr::TfType::Find< pxr::UsdGeomXformable >() );
			for ( const TUsdStore< pxr::UsdPrim >& ChildXformPrim : ChildXformPrims )
			{
				if ( TSharedPtr< FUsdSchemaTranslator > SchemaTranslator = UsdSchemasModule.GetTranslatorRegistry().CreateTranslatorForSchema( Context, UE::FUsdTyped( ChildXformPrim.Get() ) ) )
				{
					if ( !SchemaTranslator->CanBeCollapsed( CollapsingType ) )
					{
						return false;
					}
				}
			}
		}
	}

	if ( bCollapsesChildren )
	{
		TArray< TUsdStore< pxr::UsdPrim > > ChildGeomMeshes = UsdUtils::GetAllPrimsOfType( Prim, pxr::TfType::Find< pxr::UsdGeomMesh >() );

		// Don't collapse children if they have different Nanite override values
		bool bChildrenWantNanite = false;
		bool bOtherChildHasNaniteOpinion = false;
		for ( const TUsdStore< pxr::UsdPrim >& StoredPrim : ChildGeomMeshes )
		{
			pxr::UsdPrim ChildGeomMeshPrim = StoredPrim.Get();
			if ( !pxr::UsdGeomMesh{ ChildGeomMeshPrim } )
			{
				continue;
			}

			if ( pxr::UsdAttribute NaniteOverride = ChildGeomMeshPrim.GetAttribute( UnrealIdentifiers::UnrealNaniteOverride ) )
			{
				pxr::TfToken OverrideValue;
				if ( NaniteOverride.Get( &OverrideValue ) )
				{
					const bool bChildWantsNanite = ( OverrideValue == UnrealIdentifiers::UnrealNaniteOverrideEnable );

					if ( bOtherChildHasNaniteOpinion && ( bChildWantsNanite != bChildrenWantNanite ) )
					{
						UE_LOG( LogUsd, Log, TEXT( "Not collapsing down from prim '%s' as child meshes have different values for the '%s' attribute" ),
							*PrimPath.GetString(),
							*UsdToUnreal::ConvertToken( UnrealIdentifiers::UnrealNaniteOverride )
						);
						return false;
					}
					else if ( !bOtherChildHasNaniteOpinion )
					{
						bChildrenWantNanite = bChildWantsNanite;
						bOtherChildHasNaniteOpinion = true;
					}
				}
			}
		}


		// We only support collapsing GeomMeshes for now and we only want to do it when there are multiple meshes as the resulting mesh is considered unique
		if ( ChildGeomMeshes.Num() < 2 )
		{
			bCollapsesChildren = false;
		}
		else if ( Context->InfoCache.IsValid() )
		{
			TOptional<uint64> NumExpectedVertices = Context->InfoCache->GetSubtreeVertexCount( PrimPath );
			if ( !NumExpectedVertices.IsSet() || NumExpectedVertices.GetValue() > GMaxNumVerticesCollapsedMesh )
			{
				bCollapsesChildren = false;
			}

			if ( bChildrenWantNanite )
			{
				TOptional<uint64> NumExpectedMaterialSlots = Context->InfoCache->GetSubtreeMaterialSlotCount( PrimPath ).Get( 0 );

				// Note that we wont try to prevent collapsing in general if the combined mesh would have a triangle count above the threshold but too many material slots:
				// We'll just disable Nanite with a message on the log instead.
				// This because not only is it difficult to estimate the total number of UE triangles we'll get from the combined USD Mesh prims, but also because
				// it doesn't really work very well: Imagine we have the Kitchen Set scene (that collapses to like 500 material slots) and we put the threshold just under
				// the combined number of triangles. This means we can't collapse then, as the combined mesh would want Nanite (as it has more triangles than the threshold)
				// but has too many material slots. If we prevent if from collapsing, what do we do with the uncollapsed meshes then?
				//  - If we enable Nanite for them its a bit unexpected because now we'll have a bunch of tiny meshes that for some reason have Nanite enabled;
				//  - If we don't enable Nanite for them, then what is the benefit? Now the mesh hasn't collapsed but we don't have Nanite anywhere anyway...
				// At least for the case below (with the explicit overrides) we would end up with some meshes having Nanite, according to how the user set them.
				// In the future we could expose the collapsing controls on the stage actor to let the user control this a bit better
				const int32 MaxNumSections = 64; // There is no define for this, but it's checked for on NaniteBuilder.cpp, FBuilderModule::Build
				if ( !NumExpectedMaterialSlots.IsSet() || NumExpectedMaterialSlots.GetValue() > MaxNumSections )
				{
					UE_LOG( LogUsd, Log, TEXT( "Not collapsing down from prim '%s' as child meshes want Nanite to be abled but the generated static mesh would have more than '%d' material slots" ),
						*PrimPath.GetString(),
						MaxNumSections
					);
					bCollapsesChildren = false;
				}
			}
		}
	}

	return bCollapsesChildren;
}

bool FUsdGeomXformableTranslator::CanBeCollapsed( ECollapsingType CollapsingType ) const
{
	FScopedUsdAllocs UsdAllocs;

	FString PrimPathStr = PrimPath.GetString();

	pxr::UsdPrim UsdPrim{ GetPrim() };
	if ( !UsdPrim )
	{
		return false;
	}

	if ( UsdUtils::IsAnimated( UsdPrim ) )
	{
		return false;
	}

	if ( UsdUtils::PrimHasSchema( UsdPrim, UnrealIdentifiers::LiveLinkAPI ) )
	{
		return false;
	}

	return true;
}

#endif // #if USE_USD_SDK
