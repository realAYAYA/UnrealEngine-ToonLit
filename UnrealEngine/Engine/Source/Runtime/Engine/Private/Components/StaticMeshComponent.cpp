// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StaticMeshComponent.h"

#include "BodySetupEnums.h"
#include "Modules/ModuleManager.h"
#include "Engine/MapBuildDataRegistry.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Misc/ConfigCacheIni.h"
#include "RenderUtils.h"
#include "UObject/ObjectSaveContext.h"
#include "SceneInterface.h"
#include "UObject/Package.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/FortniteSeasonBranchObjectVersion.h"
#include "Engine/CollisionProfile.h"
#include "ContentStreaming.h"
#include "ComponentReregisterContext.h"
#include "UObject/UObjectAnnotation.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "StaticMeshComponentLODInfo.h"
#include "StaticMeshResources.h"
#include "StaticMeshSceneProxy.h"
#include "Net/UnrealNetwork.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#if WITH_EDITOR
#include "Collision.h"
#include "ObjectCacheEventSink.h"
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "Rendering/StaticLightingSystemInterface.h"
#include "Streaming/ActorTextureStreamingBuildDataComponent.h"
#endif
#include "LightMap.h"
#include "ShadowMap.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Engine/StaticMeshSocket.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavigationRelevantData.h"
#include "PhysicsEngine/BodySetup.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Rendering/NaniteResources.h"
#include "NaniteVertexFactory.h"
#include "StaticMeshSceneProxyDesc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StaticMeshComponent)

#define LOCTEXT_NAMESPACE "StaticMeshComponent"

DECLARE_MEMORY_STAT( TEXT( "StaticMesh VxColor Inst Mem" ), STAT_InstVertexColorMemory, STATGROUP_MemoryStaticMesh );
DECLARE_MEMORY_STAT( TEXT( "StaticMesh PreCulled Index Memory" ), STAT_StaticMeshPreCulledIndexMemory, STATGROUP_MemoryStaticMesh );

extern int32 GEnableNaniteMaterialOverrides;

FStaticMeshComponentInstanceData::FStaticMeshComponentInstanceData(const UStaticMeshComponent* SourceComponent)
	: FPrimitiveComponentInstanceData(SourceComponent)
	, StaticMesh(SourceComponent->GetStaticMesh())
{
	for (const FStaticMeshComponentLODInfo& LODDataEntry : SourceComponent->LODData)
	{
		CachedStaticLighting.Add(LODDataEntry.MapBuildDataId);
	}

	// Backup the texture streaming data.
	StreamingTextureData = SourceComponent->StreamingTextureData;
#if WITH_EDITORONLY_DATA
	MaterialStreamingRelativeBoxes = SourceComponent->MaterialStreamingRelativeBoxes;
#endif

	// Cache instance vertex colors
	for (int32 LODIndex = 0; LODIndex < SourceComponent->LODData.Num(); ++LODIndex)
	{
		const FStaticMeshComponentLODInfo& LODInfo = SourceComponent->LODData[LODIndex];

		// Note: we don't need to check LODInfo.PaintedVertices here since it's not always required.
		if (LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0)
		{
			AddVertexColorData(LODInfo, LODIndex);
		}
	}
}

bool FStaticMeshComponentInstanceData::ContainsData() const 
{
	return Super::ContainsData() 
		|| StreamingTextureData.Num() > 0 
		|| CachedStaticLighting.Num() > 0 
#if WITH_EDITORONLY_DATA
		|| MaterialStreamingRelativeBoxes.Num() > 0 
#endif
		|| StaticMesh != nullptr;
}

void FStaticMeshComponentInstanceData::ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) 
{
	Super::ApplyToComponent(Component, CacheApplyPhase);
	if (CacheApplyPhase == ECacheApplyPhase::PostUserConstructionScript)
	{
		CastChecked<UStaticMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}
}

void FStaticMeshComponentInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(StaticMesh);
}

/** Add vertex color data for a specified LOD before RerunConstructionScripts is called */
void FStaticMeshComponentInstanceData::AddVertexColorData(const struct FStaticMeshComponentLODInfo& LODInfo, uint32 LODIndex)
{
	if (VertexColorLODs.Num() <= (int32)LODIndex)
	{
		VertexColorLODs.SetNum(LODIndex + 1);
	}
	FStaticMeshVertexColorLODData& VertexColorData = VertexColorLODs[LODIndex];
	VertexColorData.LODIndex = LODIndex;
	VertexColorData.PaintedVertices = LODInfo.PaintedVertices;
	LODInfo.OverrideVertexColors->GetVertexColors(VertexColorData.VertexBufferColors);
}

/** Re-apply vertex color data after RerunConstructionScripts is called */
bool FStaticMeshComponentInstanceData::ApplyVertexColorData(UStaticMeshComponent* StaticMeshComponent) const
{
	bool bAppliedAnyData = false;

	if (StaticMeshComponent != NULL)
	{
		StaticMeshComponent->SetLODDataCount(VertexColorLODs.Num(), StaticMeshComponent->LODData.Num());

		// Its possible that we have recreated LODs in SetLODDataCount that existed prior
		// to reconstruction, but not *rebuilt* them because static lighting usage was clobbered
		// by the construction script. In this case we should recover the GUIDs we had before
		// so we dont end up creating new (non-deterministic) data
		for(int32 LODIndex = 0; LODIndex < StaticMeshComponent->LODData.Num(); ++LODIndex)
		{
			FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[LODIndex];
			if(CachedStaticLighting.IsValidIndex(LODIndex))
			{
				LODInfo.MapBuildDataId = CachedStaticLighting[LODIndex];
			}
		}

		for (int32 LODDataIndex = 0; LODDataIndex < VertexColorLODs.Num(); ++LODDataIndex)
		{
			const FStaticMeshVertexColorLODData& VertexColorLODData = VertexColorLODs[LODDataIndex];
			uint32 LODIndex = VertexColorLODData.LODIndex;

			if (StaticMeshComponent->LODData.IsValidIndex(LODIndex))
			{
				FStaticMeshComponentLODInfo& LODInfo = StaticMeshComponent->LODData[LODIndex];
				// this component could have been constructed from a template
				// that had its own vert color overrides; so before we apply
				// the instance's color data, we need to clear the old
				// vert colors (so we can properly call InitFromColorArray())
				StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
				// may not be null at the start (could have been initialized 
				// from a  component template with vert coloring), but should
				// be null at this point, after RemoveInstanceVertexColorsFromLOD()
				if (LODInfo.OverrideVertexColors == NULL && VertexColorLODData.VertexBufferColors.Num() > 0)
				{
					LODInfo.PaintedVertices = VertexColorLODData.PaintedVertices;

					LODInfo.OverrideVertexColors = new FColorVertexBuffer;
					LODInfo.OverrideVertexColors->InitFromColorArray(VertexColorLODData.VertexBufferColors);
						
					check(LODInfo.OverrideVertexColors->GetStride() > 0);
					BeginInitResource(LODInfo.OverrideVertexColors);
					bAppliedAnyData = true;
				}
			}
		}
	}

	return bAppliedAnyData;
}

UStaticMeshComponent::UStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	// check BaseEngine.ini for profile setup
	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	WireframeColorOverride = FColor(255, 255, 255, 255);

	MinLOD = 0;
	bOverrideLightMapRes = false;
	OverriddenLightMapRes = 64;
	SubDivisionStepSize = 32;
	bUseSubDivisions = true;
	StreamingDistanceMultiplier = 1.0f;
	bBoundsChangeTriggersStreamingDataRebuild = true;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;
	bOverrideNavigationExport = false;
	bForceNavigationObstacle = true;
	bDisallowMeshPaintPerInstance = false;
	bForceNaniteForMasked = false;
	bDisallowNanite = false;
	bForceDisableNanite = false;
	bEvaluateWorldPositionOffset = true;
	bWorldPositionOffsetWritesVelocity = true;
	bEvaluateWorldPositionOffsetInRayTracing = false;
	bInitialEvaluateWorldPositionOffset = false;
	bMipLevelCallbackRegistered = false;
	DistanceFieldIndirectShadowMinVisibility = .1f;
	GetBodyInstance()->bAutoWeld = true;	//static mesh by default has auto welding

#if WITH_EDITORONLY_DATA
	SelectedEditorSection = INDEX_NONE;
	SectionIndexPreview = INDEX_NONE;
	SelectedEditorMaterial = INDEX_NONE;
	MaterialIndexPreview = INDEX_NONE;
	StaticMeshImportVersion = BeforeImportStaticMeshVersionWasAdded;
	bCustomOverrideVertexColorPerLOD = false;
	bDisplayVertexColors = false;
	bDisplayPhysicalMaterialMasks = false;
	bDisplayNaniteFallbackMesh = false;
#endif
}

UStaticMeshComponent::~UStaticMeshComponent()
{
	// Empty, but required because we don't want to have to include LightMap.h and ShadowMap.h in StaticMeshComponent.h, and they are required to compile FLightMapRef and FShadowMapRef
}

/// @cond DOXYGEN_WARNINGS

void UStaticMeshComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DOREPLIFETIME( UStaticMeshComponent, StaticMesh );
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

/// @endcond

void UStaticMeshComponent::OnRep_StaticMesh(class UStaticMesh* OldStaticMesh)
{
	// Only do stuff if this actually changed from the last local value
	if (OldStaticMesh != StaticMesh)
	{
		// Properly handle replicated StaticMesh property change by putting the old value back
		// and applying the modification through a proper call to SetStaticMesh.
		UStaticMesh* NewStaticMesh = StaticMesh;

		// Put back the old value with minimal logic involved
		SetStaticMeshInternal(OldStaticMesh);
		
		// Go through all the logic required to properly apply a new static mesh.
		SetStaticMesh(NewStaticMesh);
	}
}

bool UStaticMeshComponent::HasAnySockets() const
{
	return (GetStaticMesh() != NULL) && (GetStaticMesh()->Sockets.Num() > 0);
}

void UStaticMeshComponent::QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const
{
	if (GetStaticMesh() != NULL)
	{
		for (int32 SocketIdx = 0; SocketIdx < GetStaticMesh()->Sockets.Num(); ++SocketIdx)
		{
			if (UStaticMeshSocket* Socket = GetStaticMesh()->Sockets[SocketIdx])
			{
				new (OutSockets) FComponentSocketDescription(Socket->SocketName, EComponentSocketType::Socket);
			}
		}
	}
}

FString UStaticMeshComponent::GetDetailedInfoInternal() const
{
	FString Result;  

	if( GetStaticMesh() != NULL )
	{
		Result = GetStaticMesh()->GetPathName( NULL );
	}
	else
	{
		Result = TEXT("No_StaticMesh");
	}

	return Result;  
}


void UStaticMeshComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{	
	UStaticMeshComponent* This = CastChecked<UStaticMeshComponent>(InThis);
	FPlatformMisc::Prefetch(This, offsetof(UStaticMeshComponent, LODData));
	Super::AddReferencedObjects(This, Collector);

	for (FStaticMeshComponentLODInfo& LodInfo : This->LODData)
	{
		if (LodInfo.OverrideMapBuildData)
		{
			LodInfo.OverrideMapBuildData->AddReferencedObjects(Collector);
		}
	}
}


void UStaticMeshComponent::Serialize(FArchive& Ar)
{
	NotifyIfStaticMeshChanged();

	LLM_SCOPE(ELLMTag::StaticMesh);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteSeasonBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

	const bool bBeforeRemappedEvaluateWorldPositionOffset = (Ar.CustomVer(FFortniteSeasonBranchObjectVersion::GUID) < FFortniteSeasonBranchObjectVersion::RemappedEvaluateWorldPositionOffsetInRayTracing
		&& Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::RemappedEvaluateWorldPositionOffsetInRayTracing);

	// When bEvaluateWorldPositionOffsetInRayTracing was named bEvaluateWorldPositionOffset the default value was false and now it is true. 
	// Therefore if the default was not set in a blueprint it needs to be changed for old assets before Super::Serialize(Ar);
	if (Ar.IsLoading() && bBeforeRemappedEvaluateWorldPositionOffset && !GetArchetype()->IsInBlueprint())
	{
		bEvaluateWorldPositionOffset = false; 
	}

	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.IsCooking())
	{
		// LODData's OwningComponent can be NULL for a component created via SpawnActor off of a blueprint default (LODData will be created without a call to SetLODDataCount)
		for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
		{
			LODData[LODIndex].OwningComponent = this;
		}
	}
#endif

	Ar << LODData;

	if (Ar.IsLoading())
	{
		for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
		{
			LODData[ LODIndex ].OwningComponent = this;
		}
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UEVer() < VER_UE4_COMBINED_LIGHTMAP_TEXTURES)
	{
		check(GetSceneData().AttachmentCounter.GetValue() == 0);
		// Irrelevant lights were incorrect before VER_UE4_TOSS_IRRELEVANT_LIGHTS
		IrrelevantLights_DEPRECATED.Empty();
	}

	if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
	{
		FMeshMapBuildLegacyData LegacyComponentData;

		for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
		{
			if (LODData[LODIndex].LegacyMapBuildData)
			{
				LODData[LODIndex].LegacyMapBuildData->IrrelevantLights = IrrelevantLights_DEPRECATED;
				LegacyComponentData.Data.Emplace(LODData[LODIndex].MapBuildDataId, LODData[LODIndex].LegacyMapBuildData);
				LODData[LODIndex].LegacyMapBuildData = nullptr;
			}
		}

		GComponentsWithLegacyLightmaps.AddAnnotation(this, MoveTemp(LegacyComponentData));
	}

	if (Ar.UEVer() < VER_UE4_AUTO_WELDING)
	{
		GetBodyInstance()->bAutoWeld = false;	//existing content may rely on no auto welding
	}
#endif

	if (Ar.IsLoading() && bBeforeRemappedEvaluateWorldPositionOffset)
	{
		bEvaluateWorldPositionOffsetInRayTracing = bEvaluateWorldPositionOffset;
		bEvaluateWorldPositionOffset = true; // Default WPO evaluation on
	}

	NotifyIfStaticMeshChanged();

	// NOTE: Must come after NotifyIfStaticMeshChanged to avoid an ensure in GetStaticMesh during cook.
	// If the component has bUseDefaultCollision set to true and we are a Blueprint component, then our BodyInstance will 
	// be saved with the CollisionProfile data copied from our StaticMesh, but the CollisionProfileName will be unchanged 
	// and can be anything. But! UPrimitiveComponent::Serialize calls BodyInstance.FixupData which will replace the loaded 
	// profile data with the named profile data. This would get replaced again (with the correct values) in OnRegister, 
	// but we need the profile to be set up correctly immediately after load because, if the component is a blueprint 
	// component, the blueprint may attempt to write into the CollisionProfile's ResponsesArray at an index that no longer 
	// exists (e.g., the StaticMesh is BlockAll which has 8 elements by default, but the Component is NoCollision which has 2).
	// See FORT-506503 for more context
	if (Ar.IsLoading() && IsTemplate() && bUseDefaultCollision)
	{
		UpdateCollisionFromStaticMesh();
		BodyInstance.FixupData(this);
	}

	if (Ar.IsLoading())
	{
		bInitialEvaluateWorldPositionOffset = bEvaluateWorldPositionOffset;
	}
}

void UStaticMeshComponent::PostApplyToComponent()
{
	NotifyIfStaticMeshChanged();

	Super::PostApplyToComponent();
}

void UStaticMeshComponent::PostReinitProperties()
{
	NotifyIfStaticMeshChanged();

	Super::PostReinitProperties();

	bInitialEvaluateWorldPositionOffset = bEvaluateWorldPositionOffset;
}

void UStaticMeshComponent::PostInitProperties()
{
	NotifyIfStaticMeshChanged();

	Super::PostInitProperties();

	for (int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		LODData[LODIndex].OwningComponent = this;
	}

	bInitialEvaluateWorldPositionOffset = bEvaluateWorldPositionOffset;
}

bool UStaticMeshComponent::AreNativePropertiesIdenticalTo( UObject* Other ) const
{
	bool bNativePropertiesAreIdentical = Super::AreNativePropertiesIdenticalTo( Other );
	UStaticMeshComponent* OtherSMC = CastChecked<UStaticMeshComponent>(Other);

	if( bNativePropertiesAreIdentical )
	{
		// Components are not identical if they have lighting information.
		if( LODData.Num() || OtherSMC->LODData.Num() )
		{
			bNativePropertiesAreIdentical = false;
		}
	}
	
	return bNativePropertiesAreIdentical;
}

#if WITH_EDITORONLY_DATA
void UStaticMeshComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UStaticMeshComponent::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	CachePaintedDataIfNecessary();
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
void UStaticMeshComponent::CheckForErrors()
{
	Super::CheckForErrors();

	const FCoreTexts& CoreTexts = FCoreTexts::Get();

	// Get the mesh owner's name.
	AActor* Owner = GetOwner();
	FString OwnerName(*(CoreTexts.None.ToString()));
	if ( Owner )
	{
		OwnerName = Owner->GetName();
	}

	if (GetStaticMesh() != nullptr && GetStaticMesh()->IsNaniteEnabled() != 0)
	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.Virtual.Enable"));
		if (CVar->GetInt() == 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("MeshName"), FText::FromString(GetStaticMesh()->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_NaniteNoVSM", "Static mesh '{MeshName}' uses Nanite but Virtual Shadow Maps are not enabled in the project settings. Nanite geometry does not support stationary light shadows, and may yield poor visual quality and reduced performance. Nanite geometry works best with virtual shadow maps enabled. See release notes."), Arguments)));
		}
	}

	// Make sure any simplified meshes can still find their high res source mesh
	if( GetStaticMesh() != NULL && GetStaticMesh()->GetRenderData())
	{
		int32 ZeroTriangleElements = 0;

		// Check for element material index/material mismatches
		for (int32 LODIndex = 0; LODIndex < GetStaticMesh()->GetRenderData()->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& MeshLODData = GetStaticMesh()->GetRenderData()->LODResources[LODIndex];
			for (int32 SectionIndex = 0; SectionIndex < MeshLODData.Sections.Num(); SectionIndex++)
			{
				FStaticMeshSection& Element = MeshLODData.Sections[SectionIndex];
				if (Element.NumTriangles == 0 && !GetStaticMesh()->IsNaniteEnabled())
				{
					ZeroTriangleElements++;
				}
			}
		}

		if (OverrideMaterials.Num() > GetStaticMesh()->GetStaticMaterials().Num())
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("OverridenCount"), OverrideMaterials.Num());
			Arguments.Add(TEXT("ReferencedCount"), GetStaticMesh()->GetStaticMaterials().Num());
			Arguments.Add(TEXT("MeshName"), FText::FromString(GetStaticMesh()->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_MoreMaterialsThanReferenced", "More overridden materials ({OverridenCount}) on static mesh component than are referenced ({ReferencedCount}) in source mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::MoreMaterialsThanReferenced));
		}

		if (ZeroTriangleElements > 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ElementCount"), ZeroTriangleElements);
			Arguments.Add(TEXT("MeshName"), FText::FromString(GetStaticMesh()->GetName()));
			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_ElementsWithZeroTriangles", "{ElementCount} element(s) with zero triangles in static mesh '{MeshName}'" ), Arguments ) ))
				->AddToken(FMapErrorToken::Create(FMapErrors::ElementsWithZeroTriangles));
		}
	}

	if (!GetStaticMesh() && (!Owner || !Owner->IsA(AWorldSettings::StaticClass())))	// Ignore worldsettings
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(Owner))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_StaticMeshNull", "Static mesh actor has NULL StaticMesh property")))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticMeshNull));
	}

	if ( BodyInstance.bSimulatePhysics && GetStaticMesh() != NULL && GetStaticMesh()->GetBodySetup() != NULL && GetStaticMesh()->GetBodySetup()->AggGeom.GetElementCount() == 0)
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT( "MapCheck_Message_SimulatePhyNoSimpleCollision", "{0} : Using bSimulatePhysics but StaticMesh has not simple collision."), FText::FromString(GetName()) ) ));
	}

	// Warn if component with collision enabled, but no collision data
	if (GetStaticMesh() != NULL && GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		int32 NumSectionsWithCollision = GetStaticMesh()->GetNumSectionsWithCollision();
		int32 NumCollisionPrims = (GetStaticMesh()->GetBodySetup() != nullptr) ? GetStaticMesh()->GetBodySetup()->AggGeom.GetElementCount() : 0;

		if (NumSectionsWithCollision == 0 && NumCollisionPrims == 0)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
			Arguments.Add(TEXT("StaticMeshName"), FText::FromString(GetStaticMesh()->GetName()));

			FMessageLog("MapCheck").Warning()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_CollisionEnabledNoCollisionGeom", "Collision enabled but StaticMesh ({StaticMeshName}) has no simple or complex collision."), Arguments)))
				->AddToken(FMapErrorToken::Create(FMapErrors::CollisionEnabledNoCollisionGeom));
		}
	}

	if( Mobility == EComponentMobility::Movable &&
		CastShadow && 
		bCastDynamicShadow && 
		IsRegistered() && 
		Bounds.SphereRadius > 2000.0f &&
		IsStaticLightingAllowed())
	{
		// Large shadow casting objects that create preshadows will cause a massive performance hit, since preshadows are meant for small shadow casters.
		FMessageLog("MapCheck").PerformanceWarning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_ActorLargeShadowCaster", "Large actor receives a pre-shadow and will cause an extreme performance hit unless bCastDynamicShadow is set to false.") ))
			->AddToken(FMapErrorToken::Create(FMapErrors::ActorLargeShadowCaster));
	}
}
#endif

FBoxSphereBounds UStaticMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (GetStaticMesh())
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = GetStaticMesh()->GetBounds().TransformBy(LocalToWorld);
		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}

void UStaticMeshComponent::PropagateLightingScenarioChange()
{
	FComponentRecreateRenderStateContext Context(this);
}

const FMeshMapBuildData* UStaticMeshComponent::GetMeshMapBuildData(const FStaticMeshComponentLODInfo& LODInfo, bool bCheckForResourceCluster) const
{
	if (!GetStaticMesh() || !GetStaticMesh()->GetRenderData())
	{
		return NULL;
	}
	else
	{
		// Check that the static-mesh hasn't been changed to be incompatible with the cached light-map.
		int32 NumLODs = GetStaticMesh()->GetRenderData()->LODResources.Num();
		bool bLODsShareStaticLighting = GetStaticMesh()->GetRenderData()->bLODsShareStaticLighting;

		if (!bLODsShareStaticLighting && NumLODs != LODData.Num())
		{
			return NULL;
		}
	}

	if (LODInfo.OverrideMapBuildData)
	{
		return LODInfo.OverrideMapBuildData.Get();
	}

	AActor* Owner = GetOwner();

	if (Owner)
	{
		ULevel* OwnerLevel = Owner->GetLevel();

		if (OwnerLevel && OwnerLevel->OwningWorld)
		{
			ULevel* ActiveLightingScenario = OwnerLevel->OwningWorld->GetActiveLightingScenario();
			UMapBuildDataRegistry* MapBuildData = NULL;

			if (ActiveLightingScenario && ActiveLightingScenario->MapBuildData)
			{
				MapBuildData = ActiveLightingScenario->MapBuildData;
			}
			else if (OwnerLevel->MapBuildData)
			{
				MapBuildData = OwnerLevel->MapBuildData;
			}

			if (MapBuildData)
			{
				return bCheckForResourceCluster ? MapBuildData->GetMeshBuildData(LODInfo.MapBuildDataId) : MapBuildData->GetMeshBuildDataDuringBuild(LODInfo.MapBuildDataId);
			}
		}
	}
	
	return NULL;
}

void UStaticMeshComponent::NotifyIfStaticMeshChanged()
{
#if WITH_EDITOR
	if (KnownStaticMesh != StaticMesh)
	{
		// Remove delegates from our previous mesh 
		if (KnownStaticMesh)
		{
			if (bMipLevelCallbackRegistered)
			{
				KnownStaticMesh->RemoveMipLevelChangeCallback(this);
				bMipLevelCallbackRegistered = false;
			}
		}

		KnownStaticMesh = StaticMesh;

		FObjectCacheEventSink::NotifyStaticMeshChanged_Concurrent(GetStaticMeshComponentInterface());

		// Update this component streaming data.
		IStreamingManager::Get().NotifyPrimitiveUpdated(this);
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR

void UStaticMeshComponent::OutdatedKnownStaticMeshDetected() const
{
	ensureMsgf(
		KnownStaticMesh == StaticMesh, 
		TEXT("StaticMesh property overwritten for component %s without a call to NotifyIfStaticMeshChanged(). KnownStaticMesh (%p) != StaticMesh (%p - %s)"),
		*GetFullName(),
		KnownStaticMesh,
		StaticMesh.Get(),
		StaticMesh ? *StaticMesh->GetFullName() : TEXT("nullptr")
		);

	// This is a last resort, call the notification now
	UStaticMeshComponent* MutableThis = const_cast<UStaticMeshComponent*>(this);
	MutableThis->NotifyIfStaticMeshChanged();
}

void UStaticMeshComponent::InitializeComponent()
{
	NotifyIfStaticMeshChanged();

	Super::InitializeComponent();
}

void UStaticMeshComponent::PostDuplicate(bool bDuplicateForPIE)
{
	NotifyIfStaticMeshChanged();

	Super::PostDuplicate(bDuplicateForPIE);
}

void UStaticMeshComponent::PostEditImport()
{
	NotifyIfStaticMeshChanged();

	Super::PostEditImport();
}

#endif // #if WITH_EDITOR

void UStaticMeshComponent::OnRegister()
{
	NotifyIfStaticMeshChanged();

	UpdateCollisionFromStaticMesh();

#if WITH_EDITORONLY_DATA
	//Remap the override materials if the import version is different
	//We do the remap here because if the UStaticMeshComponent is already load when
	//a static mesh get re-imported the postload will not be call.
	if (GetStaticMesh() && StaticMeshImportVersion != GetStaticMesh()->ImportVersion)
	{
		if (OverrideMaterials.Num())
		{
			uint32 MaterialMapKey = ( (uint32)((StaticMeshImportVersion & 0xffff) << 16) | (uint32)(GetStaticMesh()->ImportVersion & 0xffff));
			for (const FMaterialRemapIndex &MaterialRemapIndex : GetStaticMesh()->MaterialRemapIndexPerImportVersion)
			{
				if (MaterialRemapIndex.ImportVersionKey == MaterialMapKey)
				{
					const TArray<int32> &RemapMaterials = MaterialRemapIndex.MaterialRemap;
					TArray<UMaterialInterface*> OldOverrideMaterials = OverrideMaterials;
					OverrideMaterials.Empty();
					for (int32 MaterialIndex = 0; MaterialIndex < OldOverrideMaterials.Num(); ++MaterialIndex)
					{
						if (!RemapMaterials.IsValidIndex(MaterialIndex))
						{
							continue; //TODO is it allow check() instead
						}
						int32 RemapIndex = RemapMaterials[MaterialIndex];
						if (RemapIndex >= OverrideMaterials.Num())
						{
							//Allocate space
							OverrideMaterials.AddZeroed((RemapIndex - OverrideMaterials.Num()) + 1);
						}
						OverrideMaterials[RemapIndex] = OldOverrideMaterials[MaterialIndex];
					}
					break;
				}
			}
		}
		StaticMeshImportVersion = GetStaticMesh()->ImportVersion;
	}
#endif //WITH_EDITORONLY_DATA

	Super::OnRegister();

	// World transform might have changes causing negative determinant which changes the culling mode
	PrecachePSOs();
}

void UStaticMeshComponent::OnUnregister()
{
	Super::OnUnregister();
}

void UStaticMeshComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UStaticMeshComponent::RequiresGameThreadEndOfFrameRecreate() const
{
	return false;
}

void UStaticMeshComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	LLM_SCOPE(ELLMTag::StaticMesh);
	Super::CreateRenderState_Concurrent(Context);
}

void UStaticMeshComponent::OnCreatePhysicsState()
{
	Super::OnCreatePhysicsState();

	bNavigationRelevant = IsNavigationRelevant();
	FNavigationSystem::UpdateComponentData(*this);
}

void UStaticMeshComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();

	bNavigationRelevant = IsNavigationRelevant();
	FNavigationSystem::UpdateComponentData(*this);
}

#if WITH_EDITORONLY_DATA

/** Return the total number of LOD sections in the LOD resources */
static int32 GetNumberOfElements(const TIndirectArray<FStaticMeshLODResources>& LODResources)
{
	int32 Count = 0;
	for (int32 LODIndex = 0; LODIndex < LODResources.Num(); ++LODIndex)
	{
		Count += LODResources[LODIndex].Sections.Num();
	}
	return Count;
}

/**
 *	Pack the texture into data ready for saving. Also ensures a single entry per texture.
 *
 *	@param	TextureStreamingContainer [in,out]	Contains the list of textures referred by all components. The array index maps to UTexture2D::LevelIndex.
 *	@param	UnpackedData			  [in,out]	The unpacked data, emptied after the function executes.
 *	@param	StreamingTextureData	  [out]		The resulting packed data.
 *	@param	RefBounds				  [in]		The reference bounds used to packed the relative bounds.
 */
static void PackStreamingTextureData(ITextureStreamingContainer* TextureStreamingContainer, TArray<FStreamingRenderAssetPrimitiveInfo>& UnpackedData, TArray<FStreamingTextureBuildInfo>& StreamingTextureData, const FBoxSphereBounds& RefBounds)
{
	StreamingTextureData.Empty();

	while (UnpackedData.Num())
	{
		FStreamingRenderAssetPrimitiveInfo Info = UnpackedData[0];
		UnpackedData.RemoveAtSwap(0);

		// Merge with any other lod section using the same texture.
		for (int32 Index = 0; Index < UnpackedData.Num(); ++Index)
		{
			const FStreamingRenderAssetPrimitiveInfo& CurrInfo = UnpackedData[Index];

			if (CurrInfo.RenderAsset == Info.RenderAsset)
			{
				Info.Bounds = Union(Info.Bounds, CurrInfo.Bounds);
				// Take the max scale since it relates to higher texture resolution.
				Info.TexelFactor = FMath::Max<float>(Info.TexelFactor, CurrInfo.TexelFactor);

				UnpackedData.RemoveAtSwap(Index);
				--Index;
			}
		}

		FStreamingTextureBuildInfo PackedInfo;
		PackedInfo.PackFrom(TextureStreamingContainer, RefBounds, Info);
		StreamingTextureData.Push(PackedInfo);
	}
}

#endif

bool UStaticMeshComponent::GetMaterialStreamingData(int32 MaterialIndex, FPrimitiveMaterialInfo& MaterialData) const
{ 
	if (GetStaticMesh())
	{
		MaterialData.Material = GetMaterial(MaterialIndex);
		MaterialData.UVChannelData = GetStaticMesh()->GetUVChannelData(MaterialIndex);
#if WITH_EDITORONLY_DATA
		MaterialData.PackedRelativeBox = MaterialStreamingRelativeBoxes.IsValidIndex(MaterialIndex) ?  MaterialStreamingRelativeBoxes[MaterialIndex] : PackedRelativeBox_Identity;
#else
		MaterialData.PackedRelativeBox = PackedRelativeBox_Identity;
#endif
	}
	return MaterialData.IsValid();
}

#if WITH_EDITOR
bool UStaticMeshComponent::RemapActorTextureStreamingBuiltDataToLevel(const UActorTextureStreamingBuildDataComponent* InActorTextureBuildData)
{
	check(InActorTextureBuildData);
	check(InActorTextureBuildData->GetOwner() == GetOwner());

	ULevel* Level = GetOwner()->GetLevel();
	if (!Level || !bIsActorTextureStreamingBuiltData)
	{
		return false;
	}

	FString TextureName;
	FGuid TextureGuid;
	for (FStreamingTextureBuildInfo& BuildInfo : StreamingTextureData)
	{
		uint16 TextureLevelIndex = InvalidRegisteredStreamableTexture;
		if (InActorTextureBuildData->GetStreamableTexture(BuildInfo.TextureLevelIndex, TextureName, TextureGuid))
		{
			TextureLevelIndex = Level->RegisterStreamableTexture(TextureName, TextureGuid);
		}
		if (TextureLevelIndex == InvalidRegisteredStreamableTexture)
		{
			// If remapping failed, invalidate built texture streaming data (this should not happen with newly generated texture streaming build data)
			UE_LOG(LogStaticMesh, Warning, TEXT("Clearing invalid texture streaming built data for %s"), *GetFullName());
			ClearStreamingTextureData();
			return false;
		}
		// Update BuildInfo's TextureLevelIndex
		BuildInfo.TextureLevelIndex = TextureLevelIndex;
	}
	return true;
}

uint32 UStaticMeshComponent::ComputeHashTextureStreamingBuiltData() const
{
	uint32 Hash = 0;
	for (const FStreamingTextureBuildInfo& Data : StreamingTextureData)
	{
		Hash = FCrc::TypeCrc32(Data.ComputeHash(), Hash);
	}
	return Hash;
}
#endif

bool UStaticMeshComponent::BuildTextureStreamingDataImpl(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources, bool& bOutSupportsBuildTextureStreamingData)
{
	bOutSupportsBuildTextureStreamingData = false;

	bool bBuildDataValid = true;

#if WITH_EDITORONLY_DATA // Only rebuild the data in editor 
	if (FPlatformProperties::HasEditorOnlyData())
	{
		AActor* ComponentActor = GetOwner();

		const bool bCanBuildTextureStreamingData = FApp::CanEverRender();
		if (!bCanBuildTextureStreamingData)
		{
			bBuildDataValid = false;
		}
		if (!bIgnoreInstanceForTextureStreaming && Mobility == EComponentMobility::Static && GetStaticMesh() && GetStaticMesh()->GetRenderData() && !bHiddenInGame && bCanBuildTextureStreamingData)
		{
			// First generate the bounds. Will be used in the texture streaming build and also in the debug viewmode.
			const int32 NumMaterials = GetNumMaterials();

			// Build the material bounds if in full rebuild or if the data is incomplete.
			if ((BuildType == TSB_MapBuild) || (BuildType == TSB_ActorBuild) || (BuildType == TSB_ViewMode && MaterialStreamingRelativeBoxes.Num() != NumMaterials))
			{
				// Build the material bounds.
				MaterialStreamingRelativeBoxes.Empty(NumMaterials);
				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					MaterialStreamingRelativeBoxes.Add(PackRelativeBox(Bounds.GetBox(), GetStaticMesh()->GetMaterialBox(MaterialIndex, GetComponentTransform())));
				}

				// Update since proxy has a copy of the material bounds.
				MarkRenderStateDirty();
			}
			else if (MaterialStreamingRelativeBoxes.Num() != NumMaterials)
			{
				bBuildDataValid = false; 
			}

			// The texture build data can only be recomputed on a map build because of how the the level StreamingTextureGuids are handled.
			if ((BuildType == TSB_MapBuild) || (BuildType == TSB_ActorBuild))
			{
				ITextureStreamingContainer* TextureStreamingContainer = nullptr;
				if (ComponentActor)
				{
					if (BuildType == TSB_ActorBuild)
					{
						TextureStreamingContainer = ComponentActor->FindComponentByClass<UActorTextureStreamingBuildDataComponent>();
					}
					else
					{
						TextureStreamingContainer = ComponentActor->GetLevel();
					}
				}
				if (TextureStreamingContainer)
				{
					// Get the data without any component scaling as the built data does not include scale.
					FStreamingTextureLevelContext LevelContext(QualityLevel, FeatureLevel, true); // Use the boxes that were just computed!
					TArray<FStreamingRenderAssetPrimitiveInfo> UnpackedData;
					GetStreamingTextureInfoInner(LevelContext, nullptr, 1.f, UnpackedData);
					PackStreamingTextureData(TextureStreamingContainer, UnpackedData, StreamingTextureData, Bounds);
					bOutSupportsBuildTextureStreamingData = true;
				}
				else
				{
					UE_LOG(LogStaticMesh, Warning, TEXT("No texture streaming container found : Can't build texture streaming data for %s"), *GetFullName());
					bBuildDataValid = false;
				}
			}
			else if (StreamingTextureData.Num() == 0)
			{
				// Reset the validity here even if the bounds don't fit as the material might not use any streaming textures.
				// This is required as the texture streaming build only mark levels as dirty if they have texture related data.
				bBuildDataValid = true; 

				// In that case, check if the component refers a streaming texture. If so, the build data is missing.
				TArray<UMaterialInterface*> UsedMaterials;
				GetUsedMaterials(UsedMaterials);

				// Reset the validity here even if the bounds don't fit as the material might not use any streaming textures.
				// This is required as the texture streaming build only mark levels as dirty if they have texture related data.
				bBuildDataValid = true; 

				for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					FPrimitiveMaterialInfo MaterialData;
					if (GetMaterialStreamingData(MaterialIndex, MaterialData) && UsedMaterials.Contains(MaterialData.Material))
					{
						check(MaterialData.Material);

						// Sometimes there is missing data because the fallback density is 0
						if (MaterialData.Material->UseAnyStreamingTexture() && MaterialData.UVChannelData->LocalUVDensities[0] > 0)
						{
							bBuildDataValid = false; 
							break;
						}
					}
				}
			}

			// Generate the build reference guids
			if (StreamingTextureData.Num())
			{
				DependentResources.Add(GetStaticMesh()->GetLightingGuid());

				TArray<UMaterialInterface*> UsedMaterials;
				GetUsedMaterials(UsedMaterials);
				for (UMaterialInterface* UsedMaterial : UsedMaterials)
				{
					// Materials not having the RF_Public are instances created dynamically.
					if (UsedMaterial && UsedMaterial->UseAnyStreamingTexture() && UsedMaterial->GetOutermost() != GetTransientPackage() && UsedMaterial->HasAnyFlags(RF_Public))
					{
						TArray<FGuid> MaterialGuids;
						UsedMaterial->GetLightingGuidChain(false, MaterialGuids);
						DependentResources.Append(MaterialGuids);
					}
				}
			}
		}
		else // Otherwise clear any data.
		{
			ClearStreamingTextureData();
		}
	}

	// Make sure to clear invalid streaming texture data
	if ((BuildType == TSB_MapBuild || BuildType == TSB_ActorBuild) && !bOutSupportsBuildTextureStreamingData)
	{
		ClearStreamingTextureData();
	}
#endif
	return bBuildDataValid;
}

#if WITH_EDITOR
void UStaticMeshComponent::ClearStreamingTextureData()
{
	StreamingTextureData.Empty();

	if (MaterialStreamingRelativeBoxes.Num())
	{
		MaterialStreamingRelativeBoxes.Empty();
		MarkRenderStateDirty(); // Update since proxy has a copy of the material bounds.
	}
}
#endif

float UStaticMeshComponent::GetTextureStreamingTransformScale() const
{
	return GetComponentTransform().GetMaximumAxisScale();
}

void UStaticMeshComponent::GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const
{
	if (bIgnoreInstanceForTextureStreaming || !GetStaticMesh() || GetStaticMesh()->IsCompiling() || !GetStaticMesh()->HasValidRenderData())
	{
		return;
	}

	// Since GetTextureStreamingTransformScale can be slow for certain component types, only call it if necessary
	TOptional<float> LazyTransformScale;
	auto GetTransformScale = [this, &LazyTransformScale]()
	{
		if (!LazyTransformScale.IsSet())
		{
			LazyTransformScale = GetTextureStreamingTransformScale();
		}
		return *LazyTransformScale;
	};
	
	if (!CanSkipGetTextureStreamingRenderAssetInfo())
	{
		GetStreamingTextureInfoInner(LevelContext, Mobility == EComponentMobility::Static ? &StreamingTextureData : nullptr, StreamingDistanceMultiplier, OutStreamingRenderAssets);
	}

	// Process the lightmaps and shadowmaps entries.
	for (int32 LODIndex = 0; LODIndex < LODData.Num(); ++LODIndex)
	{
		const FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];
		const FMeshMapBuildData* MeshMapBuildData = GetMeshMapBuildData(LODInfo);
		FLightMap2D* Lightmap = MeshMapBuildData && MeshMapBuildData->LightMap ? MeshMapBuildData->LightMap->GetLightMap2D() : NULL;
		uint32 LightmapIndex = AllowHighQualityLightmaps(LevelContext.GetFeatureLevel()) ? 0 : 1;
		if (Lightmap && Lightmap->IsValid(LightmapIndex))
		{
			const FVector2D& Scale = Lightmap->GetCoordinateScale();
			if (Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER)
			{
				const float TexelFactor = GetStaticMesh()->GetLightmapUVDensity() / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingRenderAssets) FStreamingRenderAssetPrimitiveInfo(Lightmap->GetTexture(LightmapIndex), Bounds, TexelFactor, PackedRelativeBox_Identity);
				new (OutStreamingRenderAssets) FStreamingRenderAssetPrimitiveInfo(Lightmap->GetAOMaterialMaskTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
				new (OutStreamingRenderAssets) FStreamingRenderAssetPrimitiveInfo(Lightmap->GetSkyOcclusionTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
			}
		}

		FShadowMap2D* Shadowmap = MeshMapBuildData && MeshMapBuildData->ShadowMap ? MeshMapBuildData->ShadowMap->GetShadowMap2D() : NULL;
		if (Shadowmap && Shadowmap->IsValid())
		{
			const FVector2D& Scale = Shadowmap->GetCoordinateScale();
			if (Scale.X > UE_SMALL_NUMBER && Scale.Y > UE_SMALL_NUMBER)
			{
				const float TexelFactor = GetStaticMesh()->GetLightmapUVDensity() / FMath::Min(Scale.X, Scale.Y);
				new (OutStreamingRenderAssets) FStreamingRenderAssetPrimitiveInfo(Shadowmap->GetTexture(), Bounds, TexelFactor, PackedRelativeBox_Identity);
			}
		}
	}

	if (GetStaticMesh()->RenderResourceSupportsStreaming() && (GetStaticMesh()->GetRenderAssetType() == EStreamableRenderAssetType::StaticMesh))
	{
		const float TexelFactor = ForcedLodModel > 0 ?
			-(GetStaticMesh()->GetRenderData()->LODResources.Num() - ForcedLodModel + 1) :
			(IsRegistered() ? Bounds.SphereRadius * 2.f : 0.f);
		new (OutStreamingRenderAssets) FStreamingRenderAssetPrimitiveInfo(GetStaticMesh(), Bounds, TexelFactor, PackedRelativeBox_Identity, true, false);
	}
}

UBodySetup* UStaticMeshComponent::GetBodySetup()
{
	if (GetStaticMesh())
	{
		return GetStaticMesh()->GetBodySetup();
	}

	return nullptr;
}

bool UStaticMeshComponent::CanEditSimulatePhysics()
{
	if (UBodySetup* BodySetup = GetBodySetup())
	{
		return (BodySetup->AggGeom.GetElementCount() > 0) || (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple);
	}
	else
	{
		return false;
	}
}

FColor UStaticMeshComponent::GetWireframeColor() const
{
	if(bOverrideWireframeColor)
	{
		return WireframeColorOverride;
	}
	else
	{
		if(Mobility == EComponentMobility::Static)
		{
			return FColor(0, 255, 255, 255);
		}
		else if(Mobility == EComponentMobility::Stationary)
		{
			return FColor(128, 128, 255, 255);
		}
		else // Movable
		{
			if(BodyInstance.bSimulatePhysics)
			{
				return FColor(0, 255, 128, 255);
			}
			else
			{
				return FColor(255, 0, 255, 255);
			}
		}
	}
}


bool UStaticMeshComponent::DoesSocketExist(FName InSocketName) const 
{
	return (GetSocketByName(InSocketName)  != NULL);
}

#if WITH_EDITOR

bool UStaticMeshComponent::IsCompiling() const
{
	return GetStaticMesh() && GetStaticMesh()->IsCompiling();
}

bool UStaticMeshComponent::ShouldRenderSelected() const
{
	const bool bShouldRenderSelected = UMeshComponent::ShouldRenderSelected();
	return bShouldRenderSelected || bDisplayVertexColors || bDisplayPhysicalMaterialMasks;
}
#endif // WITH_EDITOR

class UStaticMeshSocket const* UStaticMeshComponent::GetSocketByName(FName InSocketName) const
{
	UStaticMeshSocket const* Socket = NULL;

	if( GetStaticMesh() )
	{
		Socket = GetStaticMesh()->FindSocket( InSocketName );
	}

	return Socket;
}

FTransform UStaticMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketWorldTransform;
			if ( Socket->GetSocketTransform(SocketWorldTransform, this) )
			{
				switch(TransformSpace)
				{
					case RTS_World:
					{
						return SocketWorldTransform;
					}
					case RTS_Actor:
					{
						if( const AActor* Actor = GetOwner() )
						{
							return SocketWorldTransform.GetRelativeTransform( GetOwner()->GetTransform() );
						}
						break;
					}
					case RTS_Component:
					{
						return SocketWorldTransform.GetRelativeTransform(GetComponentTransform());
					}
				}
			}
		}
	}

	return Super::GetSocketTransform(InSocketName, TransformSpace);
}

#if WITH_EDITORONLY_DATA
bool UStaticMeshComponent::RequiresOverrideVertexColorsFixup()
{
	UStaticMesh* Mesh = GetStaticMesh();
	if (!Mesh)
	{
		return false;
	}

	if ( !Mesh->GetRenderData())
	{
		return false;
	}

	if (Mesh->GetRenderData()->DerivedDataKey == StaticMeshDerivedDataKey)
	{
		return false;
	}

	if (LODData.Num() == 0)
	{
		return false;
	}

	FStaticMeshComponentLODInfo& LOD = LODData[0];
	if (!LOD.OverrideVertexColors)
	{
		return false;
	}

	int32 NumOverrideVertices = LOD.OverrideVertexColors->GetNumVertices();
	if (NumOverrideVertices == 0)
	{
		return false;
	}

	int32 NumPaintedVertices = LOD.PaintedVertices.Num();
	if (NumPaintedVertices == 0)
	{
		return false;
	}

	return true;
}

void UStaticMeshComponent::SetSectionPreview(int32 InSectionIndexPreview)
{
	if (SectionIndexPreview != InSectionIndexPreview)
	{
		SectionIndexPreview = InSectionIndexPreview;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::SetMaterialPreview(int32 InMaterialIndexPreview)
{
	if (MaterialIndexPreview != InMaterialIndexPreview)
	{
		MaterialIndexPreview = InMaterialIndexPreview;
		MarkRenderStateDirty();
	}
}
#endif

void UStaticMeshComponent::RemoveInstanceVertexColorsFromLOD( int32 LODToRemoveColorsFrom )
{
	if (GetStaticMesh() && LODToRemoveColorsFrom < GetStaticMesh()->GetNumLODs() && LODToRemoveColorsFrom < LODData.Num())
	{
		FStaticMeshComponentLODInfo& CurrentLODInfo = LODData[LODToRemoveColorsFrom];

		CurrentLODInfo.ReleaseOverrideVertexColorsAndBlock();
		CurrentLODInfo.PaintedVertices.Empty();

#if WITH_EDITORONLY_DATA
		StaticMeshDerivedDataKey = GetStaticMesh()->GetRenderData()->DerivedDataKey;
#endif
	}
}

#if WITH_EDITORONLY_DATA
void UStaticMeshComponent::RemoveInstanceVertexColors()
{
	for ( int32 i=0; i < GetStaticMesh()->GetNumLODs(); i++ )
	{
		RemoveInstanceVertexColorsFromLOD( i );
	}
}

void UStaticMeshComponent::CopyInstanceVertexColorsIfCompatible( const UStaticMeshComponent* SourceComponent )
{
	// The static mesh assets have to match, currently.
	if (( GetStaticMesh()->GetPathName() == SourceComponent->GetStaticMesh()->GetPathName() ) &&
		( SourceComponent->LODData.Num() != 0 ))
	{
		Modify();

		bool bIsRegistered = IsRegistered();
		FComponentReregisterContext ReregisterContext(this);
		if (bIsRegistered)
		{
			FlushRenderingCommands(); // don't sync threads unless we have to
		}
		// Remove any and all vertex colors from the target static mesh, if they exist.
		RemoveInstanceVertexColors();

		int32 NumSourceLODs = SourceComponent->GetStaticMesh()->GetNumLODs();

		// This this will set up the LODData for all the LODs
		SetLODDataCount( NumSourceLODs, NumSourceLODs );

		// Copy vertex colors from Source to Target (this)
		for ( int32 CurrentLOD = 0; CurrentLOD != NumSourceLODs; CurrentLOD++ )
		{
			FStaticMeshLODResources& SourceLODModel = SourceComponent->GetStaticMesh()->GetRenderData()->LODResources[CurrentLOD];
			if (SourceComponent->LODData.IsValidIndex(CurrentLOD))
			{
				const FStaticMeshComponentLODInfo& SourceLODInfo = SourceComponent->LODData[CurrentLOD];

				FStaticMeshLODResources& TargetLODModel = GetStaticMesh()->GetRenderData()->LODResources[CurrentLOD];
				FStaticMeshComponentLODInfo& TargetLODInfo = LODData[CurrentLOD];

				if ( SourceLODInfo.OverrideVertexColors != nullptr )
				{
					// Copy vertex colors from source to target.
					const FColorVertexBuffer* SourceColorBuffer = SourceLODInfo.OverrideVertexColors;

					TArray< FColor > CopiedColors;
					for ( uint32 ColorVertexIndex = 0; ColorVertexIndex < SourceColorBuffer->GetNumVertices(); ColorVertexIndex++ )
					{
						CopiedColors.Add( SourceColorBuffer->VertexColor( ColorVertexIndex ) );
					}

					if (TargetLODInfo.OverrideVertexColors != nullptr || CopiedColors.Num() > 0)
					{
						TargetLODInfo.CleanUp();
						TargetLODInfo.OverrideVertexColors = new FColorVertexBuffer;
						TargetLODInfo.OverrideVertexColors->InitFromColorArray( CopiedColors );

						check(TargetLODInfo.OverrideVertexColors->GetStride() > 0);
						BeginInitResource( TargetLODInfo.OverrideVertexColors );
					}
				}
			}
		}

		CachePaintedDataIfNecessary();
		StaticMeshDerivedDataKey = GetStaticMesh()->GetRenderData()->DerivedDataKey;

		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::CachePaintedDataIfNecessary()
{
	// Only cache the vertex positions if we're in the editor
	if ( GIsEditor && GetStaticMesh() )
	{
		// Iterate over each component LOD info checking for the existence of override colors
		int32 NumLODs = GetStaticMesh()->GetNumLODs();
		for ( TArray<FStaticMeshComponentLODInfo>::TIterator LODIter( LODData ); LODIter; ++LODIter )
		{
			FStaticMeshComponentLODInfo& CurCompLODInfo = *LODIter;

			// Workaround for a copy-paste bug. If the number of painted vertices is <= 1 we know the data is garbage.
			if ( CurCompLODInfo.PaintedVertices.Num() <= 1 )
			{
				CurCompLODInfo.PaintedVertices.Empty();
			}

			// If the mesh has override colors but no cached vertex positions, then the current vertex positions should be cached to help preserve instanced vertex colors during mesh tweaks
			// NOTE: We purposefully do *not* cache the positions if cached positions already exist, as this would result in the loss of the ability to fixup the component if the source mesh
			// were changed multiple times before a fix-up operation was attempted
			if ( CurCompLODInfo.OverrideVertexColors && 
				 CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0 &&
				 CurCompLODInfo.PaintedVertices.Num() == 0 &&
				 LODIter.GetIndex() < NumLODs ) 
			{
				FStaticMeshLODResources* CurRenderData = &(GetStaticMesh()->GetRenderData()->LODResources[ LODIter.GetIndex() ]);
				if ( CurRenderData->GetNumVertices() == CurCompLODInfo.OverrideVertexColors->GetNumVertices() )
				{
					// Cache the data.
					CurCompLODInfo.PaintedVertices.Reserve( CurRenderData->GetNumVertices() );
					for ( int32 VertIndex = 0; VertIndex < CurRenderData->GetNumVertices(); ++VertIndex )
					{
						FPaintedVertex* Vertex = new( CurCompLODInfo.PaintedVertices ) FPaintedVertex;
						Vertex->Position = FVector(CurRenderData->VertexBuffers.PositionVertexBuffer.VertexPosition( VertIndex ));
						Vertex->Normal = (FVector4)CurRenderData->VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ( VertIndex );
						Vertex->Color = CurCompLODInfo.OverrideVertexColors->VertexColor( VertIndex );
					}
				}
				else
				{					
					// At this point we can't resolve the colors, so just discard any isolated data we still have
					if (CurCompLODInfo.OverrideVertexColors && CurCompLODInfo.OverrideVertexColors->GetNumVertices() > 0)
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Level requires re-saving! Outdated vertex color overrides have been discarded for %s %s LOD%d. "), *GetFullName(), *GetStaticMesh()->GetFullName(), LODIter.GetIndex());
						CurCompLODInfo.ReleaseOverrideVertexColorsAndBlock();
					}
					else
					{
						UE_LOG(LogStaticMesh, Warning, TEXT("Unable to cache painted data for mesh component. Vertex color overrides will be lost if the mesh is modified. %s %s LOD%d."), *GetFullName(), *GetStaticMesh()->GetFullName(), LODIter.GetIndex() );
					}
				}
			}
		}
	}
}

bool UStaticMeshComponent::FixupOverrideColorsIfNecessary( bool bRebuildingStaticMesh )
{
	// Detect if there is a version mismatch between the source mesh and the component. If so, the component's LODs potentially
	// need to have their override colors updated to match changes in the source mesh.

	if ( RequiresOverrideVertexColorsFixup() )
	{
		double StartFixupTime = FPlatformTime::Seconds();

		// Check if we are building the static mesh.  If so we dont need to reregister this component as its already unregistered and will be reregistered
		// when the static mesh is done building.  Having nested reregister contexts is not supported.
		if( bRebuildingStaticMesh )
		{
			PrivateFixupOverrideColors();
		}
		else
		{
			// Detach this component because rendering changes are about to be applied
			FComponentReregisterContext ReregisterContext( this );
			PrivateFixupOverrideColors();
		}

		AActor* Owner = GetOwner();

		if (Owner)
		{
			ULevel* Level = Owner->GetLevel();

			if (Level)
			{
				Level->FixupOverrideVertexColorsTimeMS += (uint64)((FPlatformTime::Seconds() - StartFixupTime) * 1000.0);
				Level->FixupOverrideVertexColorsCount++;
			}
		}

		return true;
	}

	return false;
}
#endif // WITH_EDITORONLY_DATA

void UStaticMeshComponent::InitResources()
{
	for(int32 LODIndex = 0; LODIndex < LODData.Num(); LODIndex++)
	{
		FStaticMeshComponentLODInfo &LODInfo = LODData[LODIndex];
		if(LODInfo.OverrideVertexColors)
		{
			BeginInitResource(LODInfo.OverrideVertexColors);
			INC_DWORD_STAT_BY( STAT_InstVertexColorMemory, LODInfo.OverrideVertexColors->GetAllocatedSize() );
		}
	}
}

void InitStaticMeshVertexFactoryComponents(
	const FStaticMeshVertexBuffers& VertexBuffers,
	FLocalVertexFactory* VertexFactory,
	int32 LightMapCoordinateIndex,
	bool bOverrideColorVertexBuffer,
	FLocalVertexFactory::FDataType& OutData)
{
	VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, OutData, LightMapCoordinateIndex);
	if (bOverrideColorVertexBuffer)
	{
		FColorVertexBuffer::BindDefaultColorVertexBuffer(VertexFactory, OutData, FColorVertexBuffer::NullBindStride::FColorSizeForComponentOverride);
	}
	else
	{
		VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, OutData);
	}
}

void UStaticMeshComponent::CollectPSOPrecacheDataImpl(
	const FVertexFactoryType* VFType, 
	const FPSOPrecacheParams& BasePrecachePSOParams, 
	GetPSOVertexElementsFn GetVertexElements,
	FMaterialInterfacePSOPrecacheParamsList& OutParams) const
{
	check(StaticMesh != nullptr && StaticMesh->GetRenderData() != nullptr);

	UWorld* World = GetWorld();
	ERHIFeatureLevel::Type FeatureLevel = World ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;

	bool bSupportsManualVertexFetch = VFType->SupportsManualVertexFetch(GMaxRHIFeatureLevel);
	bool bAnySectionCastsShadows = false;
	int32 MeshMinLOD = GetStaticMesh()->GetMinLODIdx();

	FPSOPrecacheVertexFactoryDataPerMaterialIndexList VFTypesPerMaterialIndex;
	FStaticMeshLODResourcesArray& LODResources = GetStaticMesh()->GetRenderData()->LODResources;
	for (int32 LODIndex = MeshMinLOD; LODIndex < LODResources.Num(); ++LODIndex)
	{
		FStaticMeshLODResources& LODRenderData = LODResources[LODIndex];
		FVertexDeclarationElementList VertexElements;
		if (!bSupportsManualVertexFetch)
		{
			GetVertexElements(LODRenderData, LODIndex, bSupportsManualVertexFetch, VertexElements);
		}

		for (FStaticMeshSection& RenderSection : LODRenderData.Sections)
		{
			bAnySectionCastsShadows |= RenderSection.bCastShadow;

			int16 MaterialIndex = RenderSection.MaterialIndex;
			FPSOPrecacheVertexFactoryDataPerMaterialIndex* VFsPerMaterial = VFTypesPerMaterialIndex.FindByPredicate(
				[MaterialIndex](const FPSOPrecacheVertexFactoryDataPerMaterialIndex& Other) { return Other.MaterialIndex == MaterialIndex; });
			if (VFsPerMaterial == nullptr)
			{
				VFsPerMaterial = &VFTypesPerMaterialIndex.AddDefaulted_GetRef();
				VFsPerMaterial->MaterialIndex = RenderSection.MaterialIndex;
			}

			if (bSupportsManualVertexFetch)
			{
				VFsPerMaterial->VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(VFType));
			}
			else
			{	
				VFsPerMaterial->VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(VFType, VertexElements));
			}			
		}
	}

	bool bIsLocalToWorldDeterminantNegative = GetRenderMatrix().Determinant() < 0;

	FPSOPrecacheParams PrecachePSOParams = BasePrecachePSOParams;
	PrecachePSOParams.bCastShadow = bAnySectionCastsShadows;
	PrecachePSOParams.bReverseCulling = PrecachePSOParams.bReverseCulling || bReverseCulling != bIsLocalToWorldDeterminantNegative;
	PrecachePSOParams.bForceLODModel = ForcedLodModel > 0;

	for (FPSOPrecacheVertexFactoryDataPerMaterialIndex& VFsPerMaterial : VFTypesPerMaterialIndex)
	{
		UMaterialInterface* MaterialInterface = GetMaterial(VFsPerMaterial.MaterialIndex);
		if (MaterialInterface == nullptr)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		FMaterialInterfacePSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
		ComponentParams.MaterialInterface = MaterialInterface;
		ComponentParams.VertexFactoryDataList = VFsPerMaterial.VertexFactoryDataList;
		ComponentParams.PSOPrecacheParams = PrecachePSOParams;
	}

	UMaterialInterface* OverlayMaterialInterface = GetOverlayMaterial();
	if (OverlayMaterialInterface && VFTypesPerMaterialIndex.Num() != 0)
	{
		// Overlay is rendered with the same set of VFs
		FMaterialInterfacePSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
		
		ComponentParams.MaterialInterface = OverlayMaterialInterface;
		ComponentParams.VertexFactoryDataList = VFTypesPerMaterialIndex[0].VertexFactoryDataList;
		ComponentParams.PSOPrecacheParams = PrecachePSOParams;
		ComponentParams.PSOPrecacheParams.bCastShadow = false;
	}
}

void UStaticMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	if (StaticMesh == nullptr || StaticMesh->GetRenderData() == nullptr)
	{
		return;
	}

	int32 LightMapCoordinateIndex = StaticMesh->GetLightMapCoordinateIndex();

	auto SMC_GetElements = [LightMapCoordinateIndex, &LODData = this->LODData](const FStaticMeshLODResources& LODRenderData, int32 LODIndex, bool bSupportsManualVertexFetch, FVertexDeclarationElementList& Elements)
	{
		int32 NumTexCoords = (int32)LODRenderData.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
		int32 LODLightMapCoordinateIndex = LightMapCoordinateIndex < NumTexCoords ? LightMapCoordinateIndex : NumTexCoords - 1;
		bool bOverrideColorVertexBuffer = LODIndex < LODData.Num() && LODData[LODIndex].OverrideVertexColors != nullptr;
		FLocalVertexFactory::FDataType Data;
		InitStaticMeshVertexFactoryComponents(LODRenderData.VertexBuffers, nullptr /*VertexFactory*/, LODLightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
		FLocalVertexFactory::GetVertexElements(GMaxRHIFeatureLevel, EVertexInputStreamType::Default, bSupportsManualVertexFetch, Data, Elements);
	};
	
	if (ShouldCreateNaniteProxy())
	{
		if (NaniteLegacyMaterialsSupported())
		{
			CollectPSOPrecacheDataImpl(&Nanite::FVertexFactory::StaticType, BasePrecachePSOParams, SMC_GetElements, OutParams);
		}

		if (NaniteComputeMaterialsSupported())
		{
			CollectPSOPrecacheDataImpl(&FNaniteVertexFactory::StaticType, BasePrecachePSOParams, SMC_GetElements, OutParams);
		}
	}
	else
	{
		CollectPSOPrecacheDataImpl(&FLocalVertexFactory::StaticType, BasePrecachePSOParams, SMC_GetElements, OutParams);
	}
}

#if WITH_EDITOR
void UStaticMeshComponent::PrivateFixupOverrideColors()
{
	if (!GetStaticMesh() || !GetStaticMesh()->GetRenderData())
	{
		return;
	}

	const uint32 NumLODs = GetStaticMesh()->GetRenderData()->LODResources.Num();

	if (NumLODs == 0)
	{
		return;
	}

	// Initialize override vertex colors on any new LODs which have just been created
	SetLODDataCount(NumLODs, LODData.Num());
	bool UpdateStaticMeshDeriveDataKey = false;
	FStaticMeshComponentLODInfo& LOD0Info = LODData[0];
	if (!bCustomOverrideVertexColorPerLOD && LOD0Info.OverrideVertexColors == nullptr)
	{
		return;
	}

	FStaticMeshLODResources& SourceRenderData = GetStaticMesh()->GetRenderData()->LODResources[0];
	for (uint32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];
		FStaticMeshLODResources& CurRenderData = GetStaticMesh()->GetRenderData()->LODResources[LODIndex];
		TArray<FColor> NewOverrideColors;
		if (bCustomOverrideVertexColorPerLOD)
		{
			if (LODInfo.PaintedVertices.Num() > 0)
			{
				//Use the existing LOD custom paint and remap it on the new mesh
				RemapPaintedVertexColors(
					LODInfo.PaintedVertices,
					nullptr,
					SourceRenderData.VertexBuffers.PositionVertexBuffer,
					SourceRenderData.VertexBuffers.StaticMeshVertexBuffer,
					CurRenderData.VertexBuffers.PositionVertexBuffer,
					&CurRenderData.VertexBuffers.StaticMeshVertexBuffer,
					NewOverrideColors
					);
			}
		}
		else if(LOD0Info.PaintedVertices.Num() > 0)
		{
			RemapPaintedVertexColors(
				LOD0Info.PaintedVertices,
				nullptr,
				SourceRenderData.VertexBuffers.PositionVertexBuffer,
				SourceRenderData.VertexBuffers.StaticMeshVertexBuffer,
				CurRenderData.VertexBuffers.PositionVertexBuffer,
				&CurRenderData.VertexBuffers.StaticMeshVertexBuffer,
				NewOverrideColors
				);
		}

		LODInfo.CleanUp();
		if (NewOverrideColors.Num())
		{
			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			LODInfo.OverrideVertexColors->InitFromColorArray(NewOverrideColors);

			// Update the PaintedVertices array
			const int32 NumVerts = CurRenderData.GetNumVertices();
			check(NumVerts == NewOverrideColors.Num());

			LODInfo.PaintedVertices.Empty(NumVerts);
			for (int32 VertIndex = 0; VertIndex < NumVerts; ++VertIndex)
			{
				FPaintedVertex* Vertex = new(LODInfo.PaintedVertices) FPaintedVertex;
				Vertex->Position = FVector(CurRenderData.VertexBuffers.PositionVertexBuffer.VertexPosition(VertIndex));
				Vertex->Normal = (FVector4)CurRenderData.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertIndex);
				Vertex->Color = LODInfo.OverrideVertexColors->VertexColor(VertIndex);
			}

			BeginInitResource(LODInfo.OverrideVertexColors);
			UpdateStaticMeshDeriveDataKey = true;
		}
	}

	if (UpdateStaticMeshDeriveDataKey)
	{
		StaticMeshDerivedDataKey = GetStaticMesh()->GetRenderData()->DerivedDataKey;
	}
}
#endif // WITH_EDITOR

float GKeepPreCulledIndicesThreshold = .95f;

FAutoConsoleVariableRef CKeepPreCulledIndicesThreshold(
	TEXT("r.KeepPreCulledIndicesThreshold"),
	GKeepPreCulledIndicesThreshold,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

void UStaticMeshComponent::UpdatePreCulledData(int32 LODIndex, const TArray<uint32>& PreCulledData, const TArray<int32>& NumTrianglesPerSection)
{
	const FStaticMeshLODResources& StaticMeshLODResources = GetStaticMesh()->GetRenderData()->LODResources[LODIndex];

	int32 NumOriginalTriangles = 0;
	int32 NumVisibleTriangles = 0;

	for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
	{
		const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
		NumOriginalTriangles += Section.NumTriangles;
		NumVisibleTriangles += NumTrianglesPerSection[SectionIndex];
	}

	if (NumVisibleTriangles / (float)NumOriginalTriangles < GKeepPreCulledIndicesThreshold)
	{
		SetLODDataCount(LODIndex + 1, LODData.Num());

		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		//@todo - game thread
		check(IsInRenderingThread());
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(PreCulledData, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledIndexBuffer.InitResource(FRHICommandListImmediate::Get());

		INC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());

		int32 FirstIndex = 0;

		for (int32 SectionIndex = 0; SectionIndex < StaticMeshLODResources.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = StaticMeshLODResources.Sections[SectionIndex];
			FPreCulledStaticMeshSection PreCulledSection;
			PreCulledSection.FirstIndex = FirstIndex;
			PreCulledSection.NumTriangles = NumTrianglesPerSection[SectionIndex];
			FirstIndex += PreCulledSection.NumTriangles * 3;
			LODData[LODIndex].PreCulledSections.Add(PreCulledSection);
		}
	}
	else if (LODIndex < LODData.Num())
	{
		LODData[LODIndex].PreCulledIndexBuffer.ReleaseResource();
		TArray<uint32> EmptyIndices;
		LODData[LODIndex].PreCulledIndexBuffer.SetIndices(EmptyIndices, EIndexBufferStride::AutoDetect);
		LODData[LODIndex].PreCulledSections.Empty(StaticMeshLODResources.Sections.Num());
	}
}

void UStaticMeshComponent::ReleaseResources()
{
	for(int32 LODIndex = 0;LODIndex < LODData.Num();LODIndex++)
	{
		LODData[LODIndex].BeginReleaseOverrideVertexColors();
		DEC_DWORD_STAT_BY(STAT_StaticMeshPreCulledIndexMemory, LODData[LODIndex].PreCulledIndexBuffer.GetAllocatedSize());
		BeginReleaseResource(&LODData[LODIndex].PreCulledIndexBuffer);
	}

	DetachFence.BeginFence();
}

void UStaticMeshComponent::BeginDestroy()
{
	if (bMipLevelCallbackRegistered && GetStaticMesh())
	{
		GetStaticMesh()->RemoveMipLevelChangeCallback(this);
		bMipLevelCallbackRegistered = false;
	}

	Super::BeginDestroy();
	ReleaseResources();

#if WITH_EDITOR
	// The object cache needs to be notified when we're getting destroyed
	FObjectCacheEventSink::NotifyStaticMeshChanged_Concurrent(GetStaticMeshComponentInterface());
#endif // WITH_EDITOR
}

void UStaticMeshComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	for (int32 LODIdx = 0; LODIdx < LODData.Num(); ++LODIdx)
	{
		Out.Logf(TEXT("%sCustomProperties "), FCString::Spc(Indent));

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIdx];

		if ((LODInfo.PaintedVertices.Num() > 0) || LODInfo.OverrideVertexColors)
		{
			Out.Logf( TEXT("CustomLODData LOD=%d "), LODIdx );
		}

		// Export the PaintedVertices array
		if (LODInfo.PaintedVertices.Num() > 0)
		{
			FString	ValueStr;
			LODInfo.ExportText(ValueStr);
			Out.Log(ValueStr);
		}
		
		// Export the OverrideVertexColors buffer
		if(LODInfo.OverrideVertexColors && LODInfo.OverrideVertexColors->GetNumVertices() > 0)
		{
			FString	Value;
			LODInfo.OverrideVertexColors->ExportText(Value);

			Out.Log(Value);
		}
		Out.Logf(TEXT("\r\n"));
	}
}

void UStaticMeshComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	// First thing that should be done after importing properties is to
	// make sure notification is sent if the static mesh property has been modified.
	NotifyIfStaticMeshChanged();

	check(SourceText);
	check(Warn);

	if (FParse::Command(&SourceText,TEXT("CustomLODData")))
	{
		int32 MaxLODIndex = -1;
		int32 LODIndex;
		FString TmpStr;

		static const TCHAR LODString[] = TEXT("LOD=");
		if (FParse::Value(SourceText, LODString, LODIndex))
		{
			TmpStr = FString::Printf(TEXT("%d"), LODIndex);
			SourceText += TmpStr.Len() + (UE_ARRAY_COUNT(LODString) - 1); // without the zero terminator

			// See if we need to add a new element to the LODData array
			if (LODIndex > MaxLODIndex)
			{
				SetLODDataCount(LODIndex + 1, LODIndex + 1);
				MaxLODIndex = LODIndex;
			}
		}

		FStaticMeshComponentLODInfo& LODInfo = LODData[LODIndex];

		// Populate the PaintedVertices array
		LODInfo.ImportText(&SourceText);

		// Populate the OverrideVertexColors buffer
		if (const TCHAR* VertColorStr = FCString::Stristr(SourceText, TEXT("ColorVertexData")))
		{
			SourceText = VertColorStr;

			// this component could have been constructed from a template that
			// had its own vert color overrides; so before we apply the
			// custom color data, we need to clear the old vert colors (so
			// we can properly call ImportText())
			RemoveInstanceVertexColorsFromLOD(LODIndex);

			// may not be null at the start (could have been initialized 
			// from a blueprint component template with vert coloring), but 
			// should be null by this point, after RemoveInstanceVertexColorsFromLOD()
			check(LODInfo.OverrideVertexColors == nullptr);

			LODInfo.OverrideVertexColors = new FColorVertexBuffer;
			LODInfo.OverrideVertexColors->ImportText(SourceText);
			check(LODInfo.OverrideVertexColors->GetStride() > 0);
		}
	}
}

#if WITH_EDITOR

void UStaticMeshComponent::PreEditUndo()
{
	Super::PreEditUndo();

	// Undo can result in a resize of LODData which can calls ~FStaticMeshComponentLODInfo.
	// To safely delete FStaticMeshComponentLODInfo::OverrideVertexColors we
	// need to make sure the RT thread has no access to it any more.
	check(!IsRegistered());
	ReleaseResources();
	DetachFence.Wait();
}

void UStaticMeshComponent::PostEditUndo()
{
	NotifyIfStaticMeshChanged();

	// If the StaticMesh was also involved in this transaction, it may need reinitialization first
	// In this case, the StaticMesh will have PostEditUndo called later in this transaction, which is too late to register this component
	if (GetStaticMesh() && !GetStaticMesh()->AreRenderingResourcesInitialized())
	{
		// We need to recreate the render state of any components using the static mesh before modifying its rendering resources
		// However, we must not create the rendering state of any components in the transaction which have had PreEditUndo called and must not be referenced by the rendering thread until their PostEditUndo
		// FStaticMeshComponentRecreateRenderStateContext handles this by only recreating rendering state if the component had rendering state created in the first place.
		FStaticMeshComponentRecreateRenderStateContext RecreateContext(GetStaticMesh(), false, false);
		GetStaticMesh()->InitResources();
	}

	// The component's light-maps are loaded from the transaction, so their resources need to be reinitialized.
	InitResources();

	// Debug check command trying to track down undo related uninitialized resource
	if (GetStaticMesh() != NULL && GetStaticMesh()->GetRenderData() && GetStaticMesh()->GetRenderData()->LODResources.Num() > 0)
	{
		FRenderResource* Resource = &GetStaticMesh()->GetRenderData()->LODResources[GetStaticMesh()->GetRenderData()->GetCurrentFirstLODIdx(0)].IndexBuffer;
		ENQUEUE_RENDER_COMMAND(ResourceCheckCommand)(
			[Resource](FRHICommandList& RHICmdList)
			{
				check( Resource->IsInitialized() );
			}
		);
	}
	Super::PostEditUndo();
}

void UStaticMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	NotifyIfStaticMeshChanged();

	// Ensure that OverriddenLightMapRes is a factor of 4
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes + 3 & ~3,4);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (((PropertyThatChanged->GetName().Contains(TEXT("OverriddenLightMapRes")) ) && (bOverrideLightMapRes == true)) ||
			(PropertyThatChanged->GetName().Contains(TEXT("bOverrideLightMapRes")) ))
		{
			InvalidateLightingCache();
		}

		if ( PropertyThatChanged->GetName() == TEXT("StaticMesh") )
		{
			InvalidateLightingCache();

			RecreatePhysicsState();

			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());

			// Broadcast that the static mesh has changed
			OnStaticMeshChangedEvent.Broadcast(this);

			// If the static mesh changed, then the component needs a texture streaming rebuild.
			StreamingTextureData.Empty();
			
			if (OverrideMaterials.Num())
			{
				// Static mesh was switched so we should clean up the override materials
				CleanUpOverrideMaterials();
			}
		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UStaticMeshComponent, OverrideMaterials))
		{
			// If the owning actor is part of a cluster flag it as dirty
			IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
			Utilities->HandleActorModified(GetOwner());

			// If the materials changed, then the component needs a texture streaming rebuild.
			StreamingTextureData.Empty();
		}
	}

	FBodyInstanceEditorHelpers::EnsureConsistentMobilitySimulationSettingsOnPostEditChange(this, PropertyChangedEvent);

	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);

	// Ensure properties are in sane range.
	SubDivisionStepSize = FMath::Clamp( SubDivisionStepSize, 1, 128 );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UStaticMeshComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, bCastDistanceFieldIndirectShadow))
		{
			return Mobility != EComponentMobility::Static && CastShadow && bCastDynamicShadow;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, DistanceFieldIndirectShadowMinVisibility))
		{
			return Mobility != EComponentMobility::Static && bCastDistanceFieldIndirectShadow && CastShadow && bCastDynamicShadow;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, bOverrideDistanceFieldSelfShadowBias))
		{
			return bAffectDistanceFieldLighting;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UStaticMeshComponent, DistanceFieldSelfShadowBias))
		{
			return bOverrideDistanceFieldSelfShadowBias && bAffectDistanceFieldLighting;
		}
	}

	return Super::CanEditChange(InProperty);
}

void UStaticMeshComponent::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
}

bool UStaticMeshComponent::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	return !GetStaticMesh() || !GetStaticMesh()->IsCompiling();
}

#endif // WITH_EDITOR

bool UStaticMeshComponent::SupportsDefaultCollision()
{
	return GetStaticMesh() && GetBodySetup() == GetStaticMesh()->GetBodySetup();
}

bool UStaticMeshComponent::SupportsDitheredLODTransitions(ERHIFeatureLevel::Type FeatureLevel)
{
	if (!AllowDitheredLODTransition(FeatureLevel))
	{
		return false;
	}
		
	// Only support dithered transitions if all materials do.
	TArray<class UMaterialInterface*> Materials = GetMaterials();
	for (UMaterialInterface* Material : Materials)
	{
		if (Material && !Material->IsDitheredLODTransition())
		{
			return false;
		}
	}
	return true;
}

void UStaticMeshComponent::UpdateCollisionFromStaticMesh()
{
	// The collision will be updated once the static mesh finish building
	if (GetStaticMesh() && GetStaticMesh()->IsCompiling())
	{
		return;
	}

	if(bUseDefaultCollision && SupportsDefaultCollision())
	{
		if (UBodySetup* BodySetup = GetBodySetup())
		{
			BodyInstance.UseExternalCollisionProfile(BodySetup);	//static mesh component by default uses the same collision profile as its static mesh
		}
	}
}

void UStaticMeshComponent::PostLoad()
{
	LLM_SCOPE(ELLMTag::StaticMesh);
	NotifyIfStaticMeshChanged();

	// need to postload the StaticMesh because super initializes variables based on GetStaticLightingType() which we override and use from the StaticMesh
	if (GetStaticMesh())
	{
		GetStaticMesh()->ConditionalPostLoad();
	}

	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	// Remap the materials array if the static mesh materials may have been remapped to remove zero triangle sections.
	// This will cause async static mesh compilation to stall but only if a fixup is actually required.
	if (GetStaticMesh() && GetLinkerUEVersion() < VER_UE4_REMOVE_ZERO_TRIANGLE_SECTIONS && OverrideMaterials.Num())
	{
		if (GetStaticMesh()->HasValidRenderData()
			&& GetStaticMesh()->GetRenderData()->MaterialIndexToImportIndex.Num())
		{
			TArray<TObjectPtr<UMaterialInterface>> OldMaterials;
			const TArray<int32>& MaterialIndexToImportIndex = GetStaticMesh()->GetRenderData()->MaterialIndexToImportIndex;

			Exchange(OverrideMaterials, OldMaterials);
			OverrideMaterials.Empty(MaterialIndexToImportIndex.Num());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialIndexToImportIndex.Num(); ++MaterialIndex)
			{
				UMaterialInterface* Material = NULL;
				int32 OldMaterialIndex = MaterialIndexToImportIndex[MaterialIndex];
				if (OldMaterials.IsValidIndex(OldMaterialIndex))
				{
					Material = OldMaterials[OldMaterialIndex];
				}
				OverrideMaterials.Add(Material);
			}
		}

		if (OverrideMaterials.Num() > GetStaticMesh()->GetStaticMaterials().Num())
		{
			OverrideMaterials.RemoveAt(GetStaticMesh()->GetStaticMaterials().Num(), OverrideMaterials.Num() - GetStaticMesh()->GetStaticMaterials().Num());
		}
	}

	// If currently compiling, those will be called once the static mesh compilation has finished
	if (GetStaticMesh() && !GetStaticMesh()->IsCompiling())
	{
		CachePaintedDataIfNecessary();
		
		FixupOverrideColorsIfNecessary();
	}
#endif // #if WITH_EDITORONLY_DATA

	// Empty after potential editor fix-up when we don't care about re-saving, e.g. game or client
	if (!GIsEditor && !IsRunningCommandlet())
	{
		for (FStaticMeshComponentLODInfo& LOD : LODData)
		{
			LOD.PaintedVertices.Empty();
		}
	}

	// Legacy content may contain a lightmap resolution of 0, which was valid when vertex lightmaps were supported, but not anymore with only texture lightmaps
	OverriddenLightMapRes = FMath::Max(OverriddenLightMapRes, 4);

	// Initialize the resources for the freshly loaded component.
	InitResources();

	// If currently compiling, those will be called once the static mesh compilation has finished
	if (GetStaticMesh() && !GetStaticMesh()->IsCompiling())
	{
		// Precache PSOs for the used materials
		PrecachePSOs();
	}
}

bool UStaticMeshComponent::IsPostLoadThreadSafe() const
{
	return false;
}

bool UStaticMeshComponent::ShouldCreateRenderState() const
{
	if (!Super::ShouldCreateRenderState())
	{
		UE_LOG(LogStaticMesh, Verbose, TEXT("ShouldCreateRenderState returned false for %s (Base class was false)"), *GetFullName());
		return false;
	}

	// It is especially important to avoid creating a render state for an invalid or compiling static mesh.
	// The shader compiler might try to replace materials on a component that has a render state but doesn't 
	// even have a render proxy which would cause huge game-thread stalls in render state recreation code that 
	// doesn't have to be run in the first place.
	if (GetStaticMesh() == nullptr)
	{
		UE_LOG(LogStaticMesh, Verbose, TEXT("ShouldCreateRenderState returned false for %s (StaticMesh is null)"), *GetFullName());
		return false;
	}

	// The render state will be recreated after compilation finishes in case it is skipped here.
	if (GetStaticMesh()->IsCompiling())
	{
		UE_LOG(LogStaticMesh, Verbose, TEXT("ShouldCreateRenderState returned false for %s (StaticMesh is not ready)"), *GetFullName());
		return false;
	}
	
	return true;
}

bool UStaticMeshComponent::ShouldCreatePhysicsState() const
{
	// The physics state will be recreated after compilation finishes in case it is skipped here.
	return Super::ShouldCreatePhysicsState() && GetStaticMesh() && !GetStaticMesh()->IsCompiling();
}

void UStaticMeshComponent::SetStaticMeshInternal(UStaticMesh* NewMesh)
{
	StaticMesh = NewMesh;

	NotifyIfStaticMeshChanged();
}

bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh)
{
	// Do nothing if we are already using the supplied static mesh
	if(NewMesh == GetStaticMesh())
	{
		return false;
	}

	// Don't allow changing static meshes if "static" and registered
	AActor* Owner = GetOwner();
	if (UWorld * World = GetWorld())
	{
		if (World->HasBegunPlay() && !AreDynamicDataChangesAllowed() && Owner != nullptr)
		{
			FMessageLog("PIE").Warning(FText::Format(LOCTEXT("SetMeshOnStatic", "Calling SetStaticMesh on '{0}' but Mobility is Static."),
				FText::FromString(GetPathName())));
			return false;
		}
	}

	SetStaticMeshInternal(NewMesh);

	if (StaticMesh != nullptr && !GetStaticMesh()->IsCompiling() && StaticMesh->GetRenderData() != nullptr && FApp::CanEverRender() && !StaticMesh->HasAnyFlags(RF_ClassDefaultObject))
	{
		checkf(StaticMesh->GetRenderData()->IsInitialized(), TEXT("Uninitialized Renderdata for Mesh: %s, Mesh NeedsLoad: %i, Mesh NeedsPostLoad: %i, Mesh Loaded: %i, Mesh NeedInit: %i, Mesh IsDefault: %i")
			, *StaticMesh->GetFName().ToString()
			, StaticMesh->HasAnyFlags(RF_NeedLoad)
			, StaticMesh->HasAnyFlags(RF_NeedPostLoad)
			, StaticMesh->HasAnyFlags(RF_LoadCompleted)
			, StaticMesh->HasAnyFlags(RF_NeedInitialization)
			, StaticMesh->HasAnyFlags(RF_ClassDefaultObject)
		);
	}

#if UE_WITH_PSO_PRECACHING
	PrecachePSOs();
#endif // UE_WITH_PSO_PRECACHING

	// Need to send this to render thread at some point
	if (IsRenderStateCreated())
	{
		MarkRenderStateDirty();
	}
	// If we didn't have a valid StaticMesh assigned before
	// our render state might not have been created so
	// do it now.
	else if (ShouldCreateRenderState())
	{
		RecreateRenderState_Concurrent();
	}

	// Update physics representation right away
	RecreatePhysicsState();

	// update navigation relevancy
	bNavigationRelevant = IsNavigationRelevant();

	// Update this component streaming data.
	IStreamingManager::Get().NotifyPrimitiveUpdated(this);

	// Since we have new mesh, we need to update bounds
	UpdateBounds();

	// Mark cached material parameter names dirty
	MarkCachedMaterialParameterNameIndicesDirty();

#if WITH_EDITOR
	// Broadcast that the static mesh has changed
	OnStaticMeshChangedEvent.Broadcast(this);
#endif

#if WITH_EDITORONLY_DATA
	if (GetStaticMesh())
	{
		StaticMeshImportVersion = GetStaticMesh()->ImportVersion;
	}
#endif

	return true;
}

const Nanite::FResources* UStaticMeshComponent::GetNaniteResources() const
{
	if (OnGetNaniteResources().IsBound())
	{
		return OnGetNaniteResources().Execute();
	}
	else if (GetStaticMesh() && GetStaticMesh()->GetRenderData())
	{
		return GetStaticMesh()->GetRenderData()->NaniteResourcesPtr.Get();
	}

	return nullptr;
}

namespace Nanite
{
	template<class T> 
	bool ShouldCreateNaniteProxy(const T& Component, FMaterialAudit* OutNaniteMaterials);

	template<class T> 
	bool HasValidNaniteData(const T& Component)
	{
		const FResources* NaniteResources = Component.GetNaniteResources();
		return NaniteResources != nullptr ? NaniteResources->PageStreamingStates.Num() > 0 : false;
	}

	template<class T> 
	bool UseNaniteOverrideMaterials(const T& Component, bool bDoingMaterialAudit) 
	{
		// Check for valid data on this SMC and support for Nanite material overrides
		return (bDoingMaterialAudit || ShouldCreateNaniteProxy(Component, nullptr)) && GEnableNaniteMaterialOverrides != 0;
	}
}

bool UStaticMeshComponent::HasValidNaniteData() const
{
	return Nanite::HasValidNaniteData(*this);
}

bool FStaticMeshSceneProxyDesc::HasValidNaniteData() const
{
	return Nanite::HasValidNaniteData(*this);
}

bool UStaticMeshComponent::UseNaniteOverrideMaterials(bool bDoingMaterialAudit) const
{
	return Nanite::UseNaniteOverrideMaterials(*this, bDoingMaterialAudit);	
}

bool UStaticMeshComponent::UseNaniteOverrideMaterials() const
{
	return UseNaniteOverrideMaterials(false);
}

bool FStaticMeshSceneProxyDesc::UseNaniteOverrideMaterials(bool bDoingMaterialAudit) const
{
	return Nanite::UseNaniteOverrideMaterials(*this, bDoingMaterialAudit);	
}

void UStaticMeshComponent::SetForcedLodModel(int32 NewForcedLodModel)
{
	if (ForcedLodModel != NewForcedLodModel)
	{
		ForcedLodModel = NewForcedLodModel;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::SetDistanceFieldSelfShadowBias(float NewValue)
{
	if (DistanceFieldSelfShadowBias != NewValue && GetScene() != nullptr)
	{
		// Update game thread data
		DistanceFieldSelfShadowBias = NewValue;

		// Skip when this doesn't have a valid static mesh 
		if (!GetStaticMesh())
		{
			return;
		}

		float NewBias = FMath::Max(
			bOverrideDistanceFieldSelfShadowBias ? DistanceFieldSelfShadowBias : GetStaticMesh()->DistanceFieldSelfShadowBias,
			0.f);

		// Update render thread data
		ENQUEUE_RENDER_COMMAND(UpdateDFSelfShadowBiasCmd)(
			[NewBias, PrimitiveSceneProxy = SceneProxy](FRHICommandList&)
		{
			if (PrimitiveSceneProxy)
			{
				PrimitiveSceneProxy->SetDistanceFieldSelfShadowBias_RenderThread(NewBias);
			}
		});

		// Queue an update to GPU data
		GetScene()->UpdatePrimitiveDistanceFieldSceneData_GameThread(this);
	}
}

void UStaticMeshComponent::SetEvaluateWorldPositionOffsetInRayTracing(bool NewValue)
{
	// Skip when this doesn't have a valid static mesh or a valid scene
	if (!GetStaticMesh() || GetScene() == nullptr || SceneProxy == nullptr)
	{
		return;
	}

	const bool bHasChanged = bEvaluateWorldPositionOffsetInRayTracing != NewValue;

	// Update game thread data
	bEvaluateWorldPositionOffsetInRayTracing = NewValue;

	// Nanite doesn't support this hint yet, and the following code only works with regular SM proxies
	if (SceneProxy->IsNaniteMesh())
	{
		return;
	}

	if (bHasChanged)
	{
		// Update render thread data
		ENQUEUE_RENDER_COMMAND(UpdateEvaluateWPORTCmd)
		([NewValue, Scene = GetScene(), PrimitiveSceneProxy = static_cast<FStaticMeshSceneProxy*>(SceneProxy)](FRHICommandList& RHICmdList)
		{
			PrimitiveSceneProxy->SetEvaluateWorldPositionOffsetInRayTracing(RHICmdList, NewValue);
		});
	}
}

void UStaticMeshComponent::SetEvaluateWorldPositionOffset(bool NewValue)
{
	if (bEvaluateWorldPositionOffset != NewValue)
	{
		// Update game thread data
		bEvaluateWorldPositionOffset = NewValue;

		// make sure this has a valid static mesh and a valid scene
		if (GetStaticMesh() && GetScene() && SceneProxy)
		{
			// Update render thread data
			SceneProxy->SetEvaluateWorldPositionOffset_GameThread(NewValue);
			// We need to trigger bounds updates (see FPrimitiveSceneProxy::SetTransform) and shadow invalidations
			MarkRenderTransformDirty();
		}
	}
}

void UStaticMeshComponent::SetWorldPositionOffsetDisableDistance(int32 NewValue)
{
	if (WorldPositionOffsetDisableDistance != NewValue)
	{
		// Update game thread data
		WorldPositionOffsetDisableDistance = NewValue;

		// make sure this has a valid static mesh and a valid scene
		if (GetStaticMesh() && GetScene() && SceneProxy)
		{
			// Update render thread data
			SceneProxy->SetWorldPositionOffsetDisableDistance_GameThread(NewValue);
			// We need to trigger bounds updates (see FPrimitiveSceneProxy::SetTransform) and shadow invalidations
			MarkRenderTransformDirty();
		}
	}
}

void UStaticMeshComponent::SetReverseCulling(bool ReverseCulling)
{
	if (ReverseCulling != bReverseCulling)
	{
		bReverseCulling = ReverseCulling;
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::SetForceDisableNanite(bool bInForceDisableNanite)
{
	bForceDisableNanite = bInForceDisableNanite;

	// Check if we now need to recreate our scene proxy
	if (SceneProxy != nullptr && SceneProxy->IsNaniteMesh() != ShouldCreateNaniteProxy())
	{
		MarkRenderStateDirty();
	}
}

void UStaticMeshComponent::GetLocalBounds(FVector& Min, FVector& Max) const
{
	if (GetStaticMesh())
	{
		FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();
		Min = MeshBounds.Origin - MeshBounds.BoxExtent;
		Max = MeshBounds.Origin + MeshBounds.BoxExtent;
	}
}

void UStaticMeshComponent::SetCollisionProfileName(FName InCollisionProfileName, bool bUpdateOverlaps)
{
	Super::SetCollisionProfileName(InCollisionProfileName, bUpdateOverlaps);
	bUseDefaultCollision = false;
}

bool UStaticMeshComponent::UsesOnlyUnlitMaterials() const
{
	if (GetStaticMesh() && GetStaticMesh()->GetRenderData())
	{
		// Figure out whether any of the sections has a lit material assigned.
		bool bUsesOnlyUnlitMaterials = true;
		for (int32 LODIndex = 0; bUsesOnlyUnlitMaterials && LODIndex < GetStaticMesh()->GetRenderData()->LODResources.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& LOD = GetStaticMesh()->GetRenderData()->LODResources[LODIndex];
			for (int32 ElementIndex=0; bUsesOnlyUnlitMaterials && ElementIndex<LOD.Sections.Num(); ElementIndex++)
			{
				UMaterialInterface*	MaterialInterface	= GetMaterial(LOD.Sections[ElementIndex].MaterialIndex);
				UMaterial*			Material			= MaterialInterface ? MaterialInterface->GetMaterial() : NULL;

				bUsesOnlyUnlitMaterials = Material && Material->GetShadingModels().IsUnlit();
			}
		}
		return bUsesOnlyUnlitMaterials;
	}
	else
	{
		return false;
	}
}


bool UStaticMeshComponent::GetLightMapResolution( int32& Width, int32& Height ) const
{
	bool bPadded = false;
	if (GetStaticMesh())
	{
		// Use overridden per component lightmap resolution.
		if( bOverrideLightMapRes )
		{
			Width	= OverriddenLightMapRes;
			Height	= OverriddenLightMapRes;
		}
		// Use the lightmap resolution defined in the static mesh.
		else
		{
			Width	= GetStaticMesh()->GetLightMapResolution();
			Height	= GetStaticMesh()->GetLightMapResolution();
		}
		bPadded = true;
	}
	// No associated static mesh!
	else
	{
		Width	= 0;
		Height	= 0;
	}

	return bPadded;
}


void UStaticMeshComponent::GetEstimatedLightMapResolution(int32& Width, int32& Height) const
{
	if (GetStaticMesh())
	{
		bool bUseSourceMesh = false;

		// Use overridden per component lightmap resolution.
		// If the overridden LM res is > 0, then this is what would be used...
		if (bOverrideLightMapRes == true)
		{
			if (OverriddenLightMapRes != 0)
			{
				Width	= OverriddenLightMapRes;
				Height	= OverriddenLightMapRes;
			}
		}
		else
		{
			bUseSourceMesh = true;
		}

		// Use the lightmap resolution defined in the static mesh.
		if (bUseSourceMesh == true)
		{
			Width	= GetStaticMesh()->GetLightMapResolution();
			Height	= GetStaticMesh()->GetLightMapResolution();
		}

		// If it was not set by anything, give it a default value...
		if (Width == 0)
		{
			int32 TempInt = 0;
			verify(GConfig->GetInt(TEXT("DevOptions.StaticLighting"), TEXT("DefaultStaticMeshLightingRes"), TempInt, GLightmassIni));

			Width	= TempInt;
			Height	= TempInt;
		}
	}
	else
	{
		Width	= 0;
		Height	= 0;
	}
}


int32 UStaticMeshComponent::GetStaticLightMapResolution() const
{
	int32 Width, Height;
	GetLightMapResolution(Width, Height);
	return FMath::Max<int32>(Width, Height);
}

bool UStaticMeshComponent::HasValidSettingsForStaticLighting(bool bOverlookInvalidComponents) const
{
	if (bOverlookInvalidComponents && !GetStaticMesh())
	{
		// Return true for invalid components, this is used during the map check where those invalid components will be warned about separately
		return true;
	}
	else
	{
		int32 LightMapWidth = 0;
		int32 LightMapHeight = 0;
		GetLightMapResolution(LightMapWidth, LightMapHeight);

		return Super::HasValidSettingsForStaticLighting(bOverlookInvalidComponents) 
			&& GetStaticMesh()
			&& UsesTextureLightmaps(LightMapWidth, LightMapHeight);
	}
}

bool UStaticMeshComponent::UsesTextureLightmaps(int32 InWidth, int32 InHeight) const
{
	return (
		(HasLightmapTextureCoordinates()) &&
		(InWidth > 0) && 
		(InHeight > 0)
		);
}

bool UStaticMeshComponent::HasLightmapTextureCoordinates() const
{
	const UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh != nullptr &&
		Mesh->GetLightMapCoordinateIndex() >= 0 &&
		Mesh->GetRenderData() != nullptr &&
		Mesh->GetRenderData()->LODResources.Num() > 0)
	{
		int32 MeshMinLOD = Mesh->GetMinLODIdx();
		MeshMinLOD = FMath::Min(MeshMinLOD,  Mesh->GetRenderData()->LODResources.Num() - 1);
		
		return ((uint32)Mesh->GetLightMapCoordinateIndex() < Mesh->GetRenderData()->LODResources[MeshMinLOD].VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords());
	}
	return false;
}

void UStaticMeshComponent::GetTextureLightAndShadowMapMemoryUsage(int32 InWidth, int32 InHeight, int32& OutLightMapMemoryUsage, int32& OutShadowMapMemoryUsage) const
{
	// Stored in texture.
	const float MIP_FACTOR = 1.33f;
	OutShadowMapMemoryUsage = FMath::TruncToInt(MIP_FACTOR * InWidth * InHeight); // G8

	UWorld* World = GetWorld();
	ERHIFeatureLevel::Type FeatureLevel = World ? World->GetFeatureLevel() : GMaxRHIFeatureLevel;

	if (AllowHighQualityLightmaps(FeatureLevel))
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_HQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight); // DXT5
	}
	else
	{
		OutLightMapMemoryUsage = FMath::TruncToInt(NUM_LQ_LIGHTMAP_COEF * MIP_FACTOR * InWidth * InHeight / 2); // DXT1
	}
}


void UStaticMeshComponent::GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const
{
	// Zero initialize.
	ShadowMapMemoryUsage = 0;
	LightMapMemoryUsage = 0;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth = 0;
	int32	LightMapHeight = 0;
	GetLightMapResolution(LightMapWidth, LightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && GetStaticMesh())
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		if (UsesTextureLightmaps(LightMapWidth, LightMapHeight) == true)
		{
			GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, LightMapMemoryUsage, ShadowMapMemoryUsage);
		}
	}
}


bool UStaticMeshComponent::GetEstimatedLightAndShadowMapMemoryUsage( 
	int32& TextureLightMapMemoryUsage, int32& TextureShadowMapMemoryUsage,
	int32& VertexLightMapMemoryUsage, int32& VertexShadowMapMemoryUsage,
	int32& StaticLightingResolution, bool& bIsUsingTextureMapping, bool& bHasLightmapTexCoords) const
{
	TextureLightMapMemoryUsage	= 0;
	TextureShadowMapMemoryUsage	= 0;
	VertexLightMapMemoryUsage	= 0;
	VertexShadowMapMemoryUsage	= 0;
	bIsUsingTextureMapping		= false;
	bHasLightmapTexCoords		= false;

	// Cache light/ shadow map resolution.
	int32 LightMapWidth			= 0;
	int32 LightMapHeight		= 0;
	GetEstimatedLightMapResolution(LightMapWidth, LightMapHeight);
	StaticLightingResolution = LightMapWidth;

	int32 TrueLightMapWidth		= 0;
	int32 TrueLightMapHeight	= 0;
	GetLightMapResolution(TrueLightMapWidth, TrueLightMapHeight);

	// Determine whether static mesh/ static mesh component has static shadowing.
	if (HasStaticLighting() && GetStaticMesh())
	{
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bHasLightmapTexCoords = HasLightmapTextureCoordinates();
		// Determine whether we are using a texture or vertex buffer to store precomputed data.
		bIsUsingTextureMapping = UsesTextureLightmaps(TrueLightMapWidth, TrueLightMapHeight);
		// Stored in texture.
		GetTextureLightAndShadowMapMemoryUsage(LightMapWidth, LightMapHeight, TextureLightMapMemoryUsage, TextureShadowMapMemoryUsage);

		return true;
	}

	return false;
}

int32 UStaticMeshComponent::GetNumMaterials() const
{
	// @note : you don't have to consider Materials.Num()
	// that only counts if overridden and it can't be more than GetStaticMesh()->Materials. 
	if(GetStaticMesh())
	{
		return GetStaticMesh()->GetStaticMaterials().Num();
	}
	else
	{
		return 0;
	}
}

int32 UStaticMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	return GetStaticMesh() ? GetStaticMesh()->GetMaterialIndex(MaterialSlotName) : -1;
}

TArray<FName> UStaticMeshComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetStaticMaterials().Num(); ++MaterialIndex)
		{
			const FStaticMaterial &StaticMaterial = Mesh->GetStaticMaterials()[MaterialIndex];
			MaterialNames.Add(StaticMaterial.MaterialSlotName);
		}
	}
	return MaterialNames;
}

bool UStaticMeshComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) >= 0;
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 MaterialIndex, bool bDoingNaniteMaterialAudit) const
{
	UMaterialInterface* OutMaterial = nullptr;

	// If we have a base materials array, use that
	if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
	{
		OutMaterial = OverrideMaterials[MaterialIndex];
	}
	// Otherwise get from static mesh
	else if (GetStaticMesh())
	{
		OutMaterial = GetStaticMesh()->GetMaterial(MaterialIndex);
	}

	if (OutMaterial)
	{
		//@note FH: temporary preemptive PostLoad until zenloader load ordering improvements
		OutMaterial->ConditionalPostLoad();

		// If we have a nanite override, use that
		if (UseNaniteOverrideMaterials(bDoingNaniteMaterialAudit))
		{
			UMaterialInterface* NaniteOverride = OutMaterial->GetNaniteOverride();
			OutMaterial = NaniteOverride != nullptr ? NaniteOverride : OutMaterial;
		}

	}

	return OutMaterial;
}

UMaterialInterface* UStaticMeshComponent::GetMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, false);
}

UMaterialInterface* UStaticMeshComponent::GetNaniteAuditMaterial(int32 MaterialIndex) const
{
	return GetMaterial(MaterialIndex, true);
}

UMaterialInterface* UStaticMeshComponent::GetEditorMaterial(int32 MaterialIndex) const
{
	// Same logic as GetMaterial() but without the nanite override.
	// This makes it easier to see and edit the material which is actually set.
	if (OverrideMaterials.IsValidIndex(MaterialIndex) && OverrideMaterials[MaterialIndex])
	{
		return OverrideMaterials[MaterialIndex];
	}
	else if (GetStaticMesh())
	{
		return GetStaticMesh()->GetMaterial(MaterialIndex);
	}
	return nullptr;
}

void UStaticMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(UStaticMeshComponent::GetUsedMaterials);

	if (GetStaticMesh())
	{
		GetStaticMesh()->GetUsedMaterials(OutMaterials, [this](int32 Index) { return GetMaterial(Index); });
		if (OutMaterials.Num() > 0)
		{
			UMaterialInterface* OverlayMaterialInterface = GetOverlayMaterial();
			if (OverlayMaterialInterface != nullptr)
			{
				OutMaterials.Add(OverlayMaterialInterface);
			}
		}
	}
}

#if WITH_EDITOR
bool UStaticMeshComponent::GetMaterialPropertyPath(int32 ElementIndex, UObject*& OutOwner, FString& OutPropertyPath, FProperty*& OutProperty)
{
	if (OverrideMaterials.IsValidIndex(ElementIndex))
	{
		OutOwner = this;
		OutPropertyPath = FString::Printf(TEXT("%s[%d]"), GET_MEMBER_NAME_STRING_CHECKED(UMeshComponent, OverrideMaterials), ElementIndex);
		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(UMeshComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMeshComponent, OverrideMaterials))))
		{
			OutProperty = ArrayProperty->Inner;
		}

		return true;
	}

	if (GetStaticMesh())
	{
		OutOwner = GetStaticMesh();
		OutPropertyPath = FString::Printf(TEXT("%s[%d].%s"), *UStaticMesh::GetStaticMaterialsName().ToString(), ElementIndex, GET_MEMBER_NAME_STRING_CHECKED(FStaticMaterial, MaterialInterface));
		OutProperty = FStaticMaterial::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FStaticMaterial, MaterialInterface));

		return true;
	}

	return false;
}
#endif // WITH_EDITOR

int32 UStaticMeshComponent::GetBlueprintCreatedComponentIndex() const
{
	int32 ComponentIndex = 0;
	for (const UActorComponent* Component : GetOwner()->BlueprintCreatedComponents)
	{
		if(Component == this)
		{
			return ComponentIndex;
		}

		ComponentIndex++;
	}

	return INDEX_NONE;
}

TStructOnScope<FActorComponentInstanceData> UStaticMeshComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData = MakeStructOnScope<FActorComponentInstanceData, FStaticMeshComponentInstanceData>(this);
	return InstanceData;
}

void UStaticMeshComponent::ApplyComponentInstanceData(FStaticMeshComponentInstanceData* StaticMeshInstanceData)
{
	check(StaticMeshInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.

	if (GetStaticMesh() != StaticMeshInstanceData->StaticMesh)
	{
		return;
	}

	const int32 NumLODLightMaps = StaticMeshInstanceData->CachedStaticLighting.Num();

	if (HasStaticLighting() && NumLODLightMaps > 0)
	{
		// See if data matches current state
		if (StaticMeshInstanceData->GetComponentTransform().Equals(GetComponentTransform(), 1.e-3f))
		{
			SetLODDataCount(NumLODLightMaps, NumLODLightMaps);

			for (int32 i = 0; i < NumLODLightMaps; ++i)
			{
				LODData[i].MapBuildDataId = StaticMeshInstanceData->CachedStaticLighting[i];
			}
		}
		else
		{
			// Only warn if static lighting is enabled in the project
			if (IsStaticLightingAllowed())
			{
				UE_ASSET_LOG(LogStaticMesh, Warning, this,
					TEXT("Cached component instance data transform did not match!  Discarding cached lighting data which will cause lighting to be unbuilt.\n%s\nCurrent: %s Cached: %s"),
					*GetPathName(),
					*GetComponentTransform().ToString(),
					*StaticMeshInstanceData->GetComponentTransform().ToString());
			}
		}
	}

	if (!bDisallowMeshPaintPerInstance)
	{
		FComponentReregisterContext ReregisterStaticMesh(this);
		StaticMeshInstanceData->ApplyVertexColorData(this);
	}

	// Restore the texture streaming data.
	StreamingTextureData = StaticMeshInstanceData->StreamingTextureData;
#if WITH_EDITORONLY_DATA
	MaterialStreamingRelativeBoxes = StaticMeshInstanceData->MaterialStreamingRelativeBoxes;
#endif
}

bool UStaticMeshComponent::IsHLODRelevant() const
{
	if (HasAnyFlags(RF_Transient)
#if WITH_EDITOR 
		&& !(GetOwner() && GetOwner()->IsInLevelInstance())			// Treat components in LI as HLOD relevant for the sake of visualisation modes
#endif
		)
	{
		return false;
	}

	if (!GetStaticMesh())
	{
		return false;
	}

	if (!IsVisible())
	{
		return false;
	}

	if (Mobility == EComponentMobility::Movable)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (IsVisualizationComponent())
	{
		return false;
	}

	if (!bEnableAutoLODGeneration)
	{
		return false;
	}
#endif

	return true;
}

bool UStaticMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	const FVector Scale3D = GetComponentToWorld().GetScale3D();

	if (!Scale3D.IsZero())
	{
		if (const UStaticMesh* Mesh = GetStaticMesh())
		{
			if (ensureMsgf(!Mesh->IsCompiling(), TEXT("%s is not considered relevant to navigation until associated mesh is compiled."), *GetFullName()))
			{
				if (const UNavCollisionBase* NavCollision = Mesh->GetNavCollision())
				{
					if (ShouldExportAsObstacle(*NavCollision))
					{
						// skip default export
						return false;
					}

					const bool bHasData = NavCollision->ExportGeometry(GetComponentToWorld(), GeomExport);
					if (bHasData)
					{
						// skip default export
						return false;
					}
				}
			}
		}
	}

	return true;
}

UMaterialInterface* UStaticMeshComponent::GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const
{
	UMaterialInterface* Result = nullptr;
	SectionIndex = 0;

	UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh && Mesh->GetRenderData() && FaceIndex >= 0)
	{
		// Get the info for the LOD that is used for collision
		int32 LODIndex = Mesh->LODForCollision;
		FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
		if (RenderData->LODResources.IsValidIndex(LODIndex))
		{
			const FStaticMeshLODResources& LODResource = RenderData->LODResources[LODIndex];

			// Look for section that corresponds to the supplied face
			int32 TotalFaceCount = 0;
			for (int32 SectionIdx = 0; SectionIdx < LODResource.Sections.Num(); SectionIdx++)
			{
				const FStaticMeshSection& Section = LODResource.Sections[SectionIdx];
				// Only count faces if collision is enabled
				if (Section.bEnableCollision)
				{
					TotalFaceCount += Section.NumTriangles;

					if (FaceIndex < TotalFaceCount)
					{
						// Get the current material for it, from this component
						Result = GetMaterial(Section.MaterialIndex);
						SectionIndex = SectionIdx;
						break;
					}
				}
			}
		}
	}

	return Result;
}


void UStaticMeshComponent::RegisterLODStreamingCallback(FLODStreamingCallback&& Callback, int32 LODIdx, float TimeoutSecs, bool bOnStreamIn)
{
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		if (LODIdx < 0)
		{
			LODIdx = Mesh->GetMinLODIdx(true);
		}
		Mesh->RegisterMipLevelChangeCallback(this, LODIdx, TimeoutSecs, bOnStreamIn, MoveTemp(Callback));
		bMipLevelCallbackRegistered = true;
	}
}

void UStaticMeshComponent::RegisterLODStreamingCallback(FLODStreamingCallback&& CallbackStreamingStart, FLODStreamingCallback&& CallbackStreamingDone, float TimeoutStartSecs, float TimeoutDoneSecs)
{
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		Mesh->RegisterMipLevelChangeCallback(this, TimeoutStartSecs, MoveTemp(CallbackStreamingStart), TimeoutDoneSecs, MoveTemp(CallbackStreamingDone));
		bMipLevelCallbackRegistered = true;
	}
}

bool UStaticMeshComponent::PrestreamMeshLODs(float Seconds)
{
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		static IConsoleVariable* CVarAllowFastForceResident = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.AllowFastForceResident"));
		Mesh->bIgnoreStreamingMipBias = CVarAllowFastForceResident && CVarAllowFastForceResident->GetInt();
		Mesh->SetForceMipLevelsToBeResident(Seconds);
		return IStreamingManager::Get().GetRenderAssetStreamingManager().FastForceFullyResident(Mesh);
	}
	return false;
}

bool UStaticMeshComponent::IsNavigationRelevant() const
{
	if (const UStaticMesh* Mesh = GetStaticMesh())
	{
		// Pending compilation, update to the the navigation system will be done once compilation finishes.
		return !Mesh->IsCompiling() && Mesh->IsNavigationRelevant() && Super::IsNavigationRelevant();
	}

	return false;
}

FBox UStaticMeshComponent::GetNavigationBounds() const
{
	if (const UStaticMesh* Mesh = GetStaticMesh())
	{
		if (ensureMsgf(!Mesh->IsCompiling(), TEXT("%s is not considered relevant to navigation until associated mesh is compiled."), *GetFullName()))
		{
			return Mesh->GetNavigationBounds(GetComponentTransform());
		}
	}

	return Super::GetNavigationBounds();
}

void UStaticMeshComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	Super::GetNavigationData(Data);

	const FVector Scale3D = GetComponentToWorld().GetScale3D();
	if (!Scale3D.IsZero())
	{
		if (const UStaticMesh* Mesh = GetStaticMesh())
		{
			if (ensureMsgf(!Mesh->IsCompiling(), TEXT("%s is not considered relevant to navigation until associated mesh is compiled."), *GetFullName()))
			{
				if (UNavCollisionBase* NavCollision = Mesh->GetNavCollision())
				{
					if (ShouldExportAsObstacle(*NavCollision))
					{
						NavCollision->GetNavigationModifier(Data.Modifiers, GetComponentTransform());
					}
				}
			}
		}
	}
}

bool UStaticMeshComponent::ShouldExportAsObstacle(const UNavCollisionBase& InNavCollision) const
{
	return bOverrideNavigationExport ? bForceNavigationObstacle : InNavCollision.IsDynamicObstacle();
}

bool UStaticMeshComponent::IsShown(const FEngineShowFlags& ShowFlags) const
{
	return ShowFlags.StaticMeshes;
}

#if WITH_EDITOR

// UpdateBounds is currently the closest place we can get from the StaticMesh property
// being overwritten by a CDO construction from SpawnActor because PostInitProperties is 
// not currently being called during subobjects property copies.
void UStaticMeshComponent::UpdateBounds()
{
	NotifyIfStaticMeshChanged();

	Super::UpdateBounds();
}

void UStaticMeshComponent::PostStaticMeshCompilation()
{
	CachePaintedDataIfNecessary();

	FixupOverrideColorsIfNecessary(true);

	PrecachePSOs();

	UpdateCollisionFromStaticMesh();

	RecreatePhysicsState();

	FNavigationSystem::UpdateComponentData(*this);

	if (IsRegistered())
	{
		FStaticLightingSystemInterface::OnPrimitiveComponentUnregistered.Broadcast(this);
		if (HasValidSettingsForStaticLighting(false))
		{
			FStaticLightingSystemInterface::OnPrimitiveComponentRegistered.Broadcast(this);
		}

		if (ShouldCreateRenderState())
		{
			RecreateRenderState_Concurrent();
		}
	}
}

namespace ComponentIsTouchingSelectionHelpers
{

const FStaticMeshLODResources* GetRenderLOD(const UStaticMesh& StaticMesh)
{
	// Get the lowest available render LOD.
	// If the Minimum LOD index is not zero, their might be a lower LOD available, but it will not be used for rendering in the viewport.
	const int32 MinLODIdx = StaticMesh.GetMinLODIdx();
	return StaticMesh.GetRenderData()->GetCurrentFirstLOD(MinLODIdx);
}

enum class ECheckSectionBoundsResult
{
	Valid,           // Section data is available, continue with triangle tests.
	InvalidContinue, // Section data is not available, but continue checking the other sections.
	InvalidFail      // Section data is not available, and the selection test needs to fail because of it.
};

ECheckSectionBoundsResult CheckSectionBounds(const FStaticMeshSection& Section, const FIndexArrayView& Indices, const FPositionVertexBuffer& Vertices,
                                             const bool bMustEncompassEntireComponent)
{
	// Not sure if empty sections are valid, but if they are then we need to ignore them for the triangle tests.
	// Otherwise the checks for encompassing the entire component are producing false negatives and/or positives.
	if (Section.NumTriangles == 0)
	{
		return ECheckSectionBoundsResult::InvalidContinue;
	}

	// Vertex and Index buffers are not guaranteed to be present and/or complete.
	// Thus, we need to check their bounds before we use them to test triangle intersections.
	if (static_cast<int32>(Section.FirstIndex + Section.NumTriangles * 3) > Indices.Num() || Section.MaxVertexIndex >= Vertices.GetNumVertices())
	{
		// If the entire component must to be encompassed, not having data for a section means we cannot verify that the section is included, and
		// this entire selection test fails. Otherwise, we just continue and check the other sections.
		return bMustEncompassEntireComponent ? ECheckSectionBoundsResult::InvalidFail : ECheckSectionBoundsResult::InvalidContinue;
	}

	return ECheckSectionBoundsResult::Valid;
}

} // namespace StaticMeshComponent_SelectionHelpers


void UStaticMeshComponent::OnMeshRebuild(bool bRenderDataChanged)
{
	if (bRenderDataChanged)
	{
		// Fixup their override colors if necessary.
		// Also invalidate lighting. *** WARNING components may be reattached here! ***
		FixupOverrideColorsIfNecessary(true);
		InvalidateLightingCache();
	}
	else
	{
		// No change in RenderData, still re-register components with preview static lighting system as ray tracing geometry has been recreated
		// When RenderData is changed, this is handled by InvalidateLightingCache()
		FStaticLightingSystemInterface::OnPrimitiveComponentUnregistered.Broadcast(this);
		if (HasValidSettingsForStaticLighting(false))
		{
			FStaticLightingSystemInterface::OnPrimitiveComponentRegistered.Broadcast(this);
		}
	}
}

bool UStaticMeshComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && GetStaticMesh() != nullptr && GetStaticMesh()->HasValidRenderData())
	{
		// If the bounds are fully contains assume the static mesh component is fully contained
		if (Super::ComponentIsTouchingSelectionBox(InSelBBox, bConsiderOnlyBSP, true))
		{
			return true;
		}

		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		if (Super::ComponentIsTouchingSelectionBox(InSelBBox, bConsiderOnlyBSP, false))
		{
			TArray<FVector> TriVertices;
			TriVertices.SetNumUninitialized(3);

			const FStaticMeshLODResources* LOD = ComponentIsTouchingSelectionHelpers::GetRenderLOD(*GetStaticMesh());
			if (!LOD)
			{
				return false;
			}

			const FIndexArrayView Indices = LOD->IndexBuffer.GetArrayView();
			const FPositionVertexBuffer& Vertices = LOD->VertexBuffers.PositionVertexBuffer;

			for (const FStaticMeshSection& Section : LOD->Sections)
			{
				switch (ComponentIsTouchingSelectionHelpers::CheckSectionBounds(Section, Indices, Vertices, bMustEncompassEntireComponent))
				{
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::Valid:
					/* Proceed with triangle tests. */
					break;
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::InvalidContinue:
					/* Skip this mesh section, and continue with next one. */
					continue;
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::InvalidFail:
					/* Invalid data; fail test. */
					return false;
				}

				// Iterate over each triangle.
				const int32 SectionIndicesEnd = static_cast<int32>(Section.FirstIndex + Section.NumTriangles * 3);
				for (int32 TriFirstVertexIndex = Section.FirstIndex; TriFirstVertexIndex < SectionIndicesEnd; TriFirstVertexIndex += 3)
				{
					for (int32 i = 0; i < 3; i++)
					{
						const int32 VertexIndex = Indices[TriFirstVertexIndex + i];
						const FVector LocalPosition(Vertices.VertexPosition(VertexIndex));
						TriVertices[i] = GetComponentTransform().TransformPosition(LocalPosition);
					}

					// Check if the triangle is colliding with the bounding box.
					const FSeparatingAxisPointCheck ThePointCheck(TriVertices, InSelBBox.GetCenter(), InSelBBox.GetExtent(), false);

					if (!bMustEncompassEntireComponent && ThePointCheck.bHit)
					{
						// Needn't encompass entire component: any intersection, we consider as touching
						return true;
					}
					if (bMustEncompassEntireComponent && !ThePointCheck.bHit)
					{
						// Must encompass entire component: any non intersection, we consider as not touching
						return false;
					}
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}

bool UStaticMeshComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	if (!bConsiderOnlyBSP && GetStaticMesh() && GetStaticMesh()->HasValidRenderData())
	{
		// Check if we are even inside it's bounding box, if we are not, there is no way we colliding via the more advanced checks we will do.
		bool bIsFullyContained = false;
		if (InFrustum.IntersectBox(Bounds.Origin, Bounds.BoxExtent, bIsFullyContained))
		{
			// If the bounds are fully contains assume the static mesh component is fully contained
			if (bIsFullyContained)
			{
				return true;
			}

			const FStaticMeshLODResources* LOD = ComponentIsTouchingSelectionHelpers::GetRenderLOD(*GetStaticMesh());
			if (!LOD)
			{
				return false;
			}

			const FIndexArrayView Indices = LOD->IndexBuffer.GetArrayView();
			const FPositionVertexBuffer& Vertices = LOD->VertexBuffers.PositionVertexBuffer;

			const FTransform& TransformToWorld = GetComponentTransform();
			for (const FStaticMeshSection& Section : LOD->Sections)
			{
				switch (ComponentIsTouchingSelectionHelpers::CheckSectionBounds(Section, Indices, Vertices, bMustEncompassEntireComponent))
				{
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::Valid:
					/* Proceed with triangle tests. */
					break;
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::InvalidContinue:
					/* Skip this mesh section, and continue with next one. */
					continue;
				case ComponentIsTouchingSelectionHelpers::ECheckSectionBoundsResult::InvalidFail:
					/* Invalid data; fail test. */
					return false;
				}

				// Iterate over each triangle.
				const int32 SectionIndicesEnd = static_cast<int32>(Section.FirstIndex + Section.NumTriangles * 3);
				for (int32 TriFirstVertexIndex = Section.FirstIndex; TriFirstVertexIndex < SectionIndicesEnd; TriFirstVertexIndex += 3)
				{
					FVector PointA(Vertices.VertexPosition(Indices[TriFirstVertexIndex]));
					FVector PointB(Vertices.VertexPosition(Indices[TriFirstVertexIndex + 1]));
					FVector PointC(Vertices.VertexPosition(Indices[TriFirstVertexIndex + 2]));

					PointA = TransformToWorld.TransformPosition(PointA);
					PointB = TransformToWorld.TransformPosition(PointB);
					PointC = TransformToWorld.TransformPosition(PointC);

					bool bFullyContained = false;
					bool bIntersect = InFrustum.IntersectTriangle(PointA, PointB, PointC, bFullyContained);

					if (!bMustEncompassEntireComponent && bIntersect)
					{
						// Needn't encompass entire component: any intersection, we consider as touching
						return true;
					}
					if (bMustEncompassEntireComponent && !bFullyContained)
					{
						// Must encompass entire component: any non intersection, we consider as not touching
						return false;
					}
				}
			}

			// Either:
			// a) It must encompass the entire component and all points were intersected (return true), or;
			// b) It needn't encompass the entire component but no points were intersected (return false)
			return bMustEncompassEntireComponent;
		}
	}

	return false;
}

#endif // #if WITH_EDITOR


//////////////////////////////////////////////////////////////////////////
// StaticMeshComponentLODInfo



/** Default constructor */
FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo()
	: LegacyMapBuildData(NULL)
	, OverrideVertexColors(NULL)
	, OwningComponent(NULL)
{
	// MapBuildDataId will be deserialized
}

FStaticMeshComponentLODInfo::FStaticMeshComponentLODInfo(UStaticMeshComponent* InOwningComponent)
	: LegacyMapBuildData(NULL)
	, OverrideVertexColors(NULL)
	, OwningComponent(InOwningComponent)
{
	// MapBuildDataId is invalid for newly created FStaticMeshComponentLODInfo
	// Will be assigned a valid GUID if we ever need to store data in the MapBuildData for it.
	// See CreateMapBuildDataId()
}

bool FStaticMeshComponentLODInfo::CreateMapBuildDataId(int32 LodIndex)
{
	if (!MapBuildDataId.IsValid())
	{
		if (LodIndex == 0 || OwningComponent == nullptr)
		{
			MapBuildDataId = FGuid::NewGuid();
		}
		else
		{
			FString GuidBaseString = OwningComponent->LODData[0].MapBuildDataId.ToString(EGuidFormats::Digits);
			GuidBaseString += TEXT("LOD_") + FString::FromInt(LodIndex);

			FSHA1 Sha;
			Sha.Update((uint8*)*GuidBaseString, GuidBaseString.Len() * sizeof(TCHAR));
			Sha.Final();
			// Retrieve the hash and use it to construct a pseudo-GUID.
			uint32 Hash[5];
			Sha.GetHash((uint8*)Hash);
			MapBuildDataId = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
		}

		return true;
	}

	return false;
}

/** Destructor */
FStaticMeshComponentLODInfo::~FStaticMeshComponentLODInfo()
{
	// Note: OverrideVertexColors had BeginReleaseResource called in UStaticMeshComponent::BeginDestroy, 
	// And waits on a fence for that command to complete in UStaticMeshComponent::IsReadyForFinishDestroy,
	// So we know it is safe to delete OverrideVertexColors here (RT can't be referencing it anymore)
	CleanUp();
}

void FStaticMeshComponentLODInfo::CleanUp()
{
	if(OverrideVertexColors)
	{
		DEC_DWORD_STAT_BY( STAT_InstVertexColorMemory, OverrideVertexColors->GetAllocatedSize() );
	}

	FColorVertexBuffer* LocalOverrideVertexColors = OverrideVertexColors;
	OverrideVertexColors = nullptr;
	PaintedVertices.Empty();

	if (LocalOverrideVertexColors != nullptr)
	{
		ENQUEUE_RENDER_COMMAND(FStaticMeshComponentLODInfoCleanUp)(
		[LocalOverrideVertexColors](FRHICommandList&)
		{
			LocalOverrideVertexColors->ReleaseResource();
			delete LocalOverrideVertexColors;
		});
	}
}

void FStaticMeshComponentLODInfo::BeginReleaseOverrideVertexColors()
{
	if (OverrideVertexColors)
	{
		// enqueue a rendering command to release
		BeginReleaseResource(OverrideVertexColors);
	}
}

void FStaticMeshComponentLODInfo::ReleaseOverrideVertexColorsAndBlock()
{
	if (OverrideVertexColors)
	{
		// The RT thread has no access to it any more so it's safe to delete it.
		CleanUp();
		// Ensure the RT no longer accessed the data, might slow down
		FlushRenderingCommands();
	}
}

void FStaticMeshComponentLODInfo::ExportText(FString& ValueStr)
{
	ValueStr += FString::Printf(TEXT("PaintedVertices(%d)="), PaintedVertices.Num());

	// Rough approximation
	ValueStr.Reserve(ValueStr.Len() + PaintedVertices.Num() * 125);

	// Export the Position, Normal and Color info for each vertex
	for(int32 i = 0; i < PaintedVertices.Num(); ++i)
	{
		FPaintedVertex& Vert = PaintedVertices[i];

		ValueStr += FString::Printf(TEXT("((Position=(X=%.6f,Y=%.6f,Z=%.6f),"), Vert.Position.X, Vert.Position.Y, Vert.Position.Z);
		ValueStr += FString::Printf(TEXT("(Normal=(X=%d,Y=%d,Z=%d,W=%d),"), Vert.Normal.X, Vert.Normal.Y, Vert.Normal.Z, Vert.Normal.W);
		ValueStr += FString::Printf(TEXT("(Color=(B=%d,G=%d,R=%d,A=%d))"), Vert.Color.B, Vert.Color.G, Vert.Color.R, Vert.Color.A);

		// Seperate each vertex entry with a comma
		if ((i + 1) != PaintedVertices.Num())
		{
			ValueStr += TEXT(",");
		}
	}

	ValueStr += TEXT(" ");
}

void FStaticMeshComponentLODInfo::ImportText(const TCHAR** SourceText)
{
	FString TmpStr;
	int32 VertCount;
	if (FParse::Value(*SourceText, TEXT("PaintedVertices("), VertCount))
	{
		TmpStr = FString::Printf(TEXT("%d"), VertCount);
		*SourceText += TmpStr.Len() + 18;

		FString SourceTextStr = *SourceText;
		TArray<FString> Tokens;
		int32 TokenIdx = 0;
		bool bValidInput = true;

		// Tokenize the text
		SourceTextStr.ParseIntoArray(Tokens, TEXT(","), false);

		// There should be 11 tokens per vertex
		check(Tokens.Num() * 11 >= VertCount);

		PaintedVertices.AddUninitialized(VertCount);

		for (int32 Idx = 0; Idx < VertCount; ++Idx)
		{
			// Position
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Position.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Position.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Position.Z);
			// Normal
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("X="), PaintedVertices[Idx].Normal.X);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Y="), PaintedVertices[Idx].Normal.Y);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("Z="), PaintedVertices[Idx].Normal.Z);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("W="), PaintedVertices[Idx].Normal.W);
			// Color
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("B="), PaintedVertices[Idx].Color.B);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("G="), PaintedVertices[Idx].Color.G);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("R="), PaintedVertices[Idx].Color.R);
			bValidInput &= FParse::Value(*Tokens[TokenIdx++], TEXT("A="), PaintedVertices[Idx].Color.A);

			// Verify that the info for this vertex was read correctly
			check(bValidInput);
		}

		// Advance the text pointer past all of the data we just read
		int32 LODDataStrLen = 0;
		for (int32 Idx = 0; Idx < TokenIdx - 1; ++Idx)
		{
			LODDataStrLen += Tokens[Idx].Len() + 1;
		}
		*SourceText += LODDataStrLen;
	}
}

int32 GKeepKeepOverrideVertexColorsOnCPU = 1;
FAutoConsoleVariableRef CKeepOverrideVertexColorsOnCPU(
	TEXT("r.KeepOverrideVertexColorsOnCPU"),
	GKeepKeepOverrideVertexColorsOnCPU,
	TEXT("Keeps a CPU copy of override vertex colors.  May be required for some blueprints / object spawning."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

FArchive& operator<<(FArchive& Ar,FStaticMeshComponentLODInfo& I)
{
	const uint8 OverrideColorsStripFlag = 1;
	bool bStrippedOverrideColors = false;
#if WITH_EDITORONLY_DATA
	if( Ar.IsCooking() )
	{
		// Check if override color should be stripped too	
		int32 LODIndex = 0;
		for( ; LODIndex < I.OwningComponent->LODData.Num() && &I != &I.OwningComponent->LODData[ LODIndex ]; LODIndex++ )
		{}
		check( LODIndex < I.OwningComponent->LODData.Num() );

		bStrippedOverrideColors = true;

		if (I.OverrideVertexColors &&
			I.OwningComponent->GetStaticMesh() &&
			I.OwningComponent->GetStaticMesh()->GetRenderData() &&
			I.OwningComponent->GetStaticMesh()->GetRenderData()->LODResources.IsValidIndex(LODIndex))
		{
			const FStaticMeshLODResources& StaticMeshLODResources = I.OwningComponent->GetStaticMesh()->GetRenderData()->LODResources[LODIndex];
			const int32 StaticMeshVertexBufferCount = StaticMeshLODResources.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();
			if (StaticMeshVertexBufferCount == I.OverrideVertexColors->GetNumVertices())
			{
				bStrippedOverrideColors = false;
			}
			else if (StaticMeshVertexBufferCount == 0)
			{
				// StaticMeshVertexBuffer is not available when StaticMesh loaded from a cooked build made with IsDataStrippedForServer()
				// TODO: Could be using PKG_ServerSideOnly but PKG_ServerSideOnly flag is not currently being set
				if (I.OwningComponent->GetStaticMesh()->GetPackage()->HasAllPackagesFlags(PKG_Cooked))
				{
					// Calculate VertexCount by iterating section data we do have access to
					uint32 MaxMaxVertexIndex = 0;
					const FStaticMeshSectionArray& SectionsList = StaticMeshLODResources.Sections;
					for (const FStaticMeshSection& Section : SectionsList)
					{
						MaxMaxVertexIndex = FMath::Max<uint32>(Section.MaxVertexIndex, MaxMaxVertexIndex);
					}

					++MaxMaxVertexIndex;

					if (I.OverrideVertexColors->GetNumVertices() == MaxMaxVertexIndex)
					{
						bStrippedOverrideColors = false;
					}
				}
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	FStripDataFlags StripFlags( Ar, bStrippedOverrideColors ? OverrideColorsStripFlag : 0 );

	if( !StripFlags.IsAudioVisualDataStripped() )
	{
		if (Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MapBuildDataSeparatePackage)
		{
			I.MapBuildDataId = FGuid::NewGuid();
			I.LegacyMapBuildData = new FMeshMapBuildData();
			Ar << I.LegacyMapBuildData->LightMap;
			Ar << I.LegacyMapBuildData->ShadowMap;
		}
		else
		{
			Ar << I.MapBuildDataId;
		}
	}

	if( !StripFlags.IsClassDataStripped( OverrideColorsStripFlag ) )
	{
		// Bulk serialization (new method)
		//Avoid saving empty override vertex colors buffer. When loading, this variable will be override by the serialization.
		uint8 bLoadVertexColorData = (I.OverrideVertexColors != nullptr && I.OverrideVertexColors->GetNumVertices() > 0 && I.OverrideVertexColors->GetStride() > 0);
		Ar << bLoadVertexColorData;

		if(bLoadVertexColorData)
		{
			if(Ar.IsLoading())
			{
				check(!I.OverrideVertexColors);
				I.OverrideVertexColors = new FColorVertexBuffer;
			}

			//we want to discard the vertex colors after rhi init when in cooked/client builds.
			const bool bNeedsCPUAccess = !Ar.IsLoading() || GIsEditor || IsRunningCommandlet() || (GKeepKeepOverrideVertexColorsOnCPU != 0);
			check(I.OverrideVertexColors != nullptr);
			I.OverrideVertexColors->Serialize(Ar, bNeedsCPUAccess);
			
			if (Ar.IsLoading())
			{
				//When IsSaving, we cannot have this situation since it is check before
				if (I.OverrideVertexColors->GetNumVertices() <= 0 || I.OverrideVertexColors->GetStride() <= 0)
				{
					UE_LOG(LogStaticMesh, Log, TEXT("Loading a staticmesh component that is flag with override vertex color buffer, but the buffer is empty after loading(serializing) it. Resave the map to fix the component override vertex color data."));
					//Avoid saving an empty array
					delete I.OverrideVertexColors;
					I.OverrideVertexColors = nullptr;
				}
				else
				{
					BeginInitResource(I.OverrideVertexColors);
				}
			}
		}
	}

	// Serialize out cached vertex information if necessary.
	if (!StripFlags.IsEditorDataStripped() && !(Ar.IsFilterEditorOnly() && Ar.IsCountingMemory()) && !Ar.IsObjectReferenceCollector())
	{
		Ar << I.PaintedVertices;
	}

	return Ar;
}

void UStaticMeshComponent::GetPrimitiveStats(FPrimitiveStats& PrimitiveStats) const
{
	if (StaticMesh)
	{
		PrimitiveStats.NbTriangles = StaticMesh->GetNumTriangles(PrimitiveStats.ForLOD);
	}
}

#if WITH_EDITOR
void FActorStaticMeshComponentInterface::OnMeshRebuild(bool bRenderDataChanged)
{
	UStaticMeshComponent::GetStaticMeshComponent(this)->OnMeshRebuild(bRenderDataChanged);
}

void FActorStaticMeshComponentInterface::PostStaticMeshCompilation()
{
	UStaticMeshComponent::GetStaticMeshComponent(this)->PostStaticMeshCompilation();
}
#endif

UStaticMesh* FActorStaticMeshComponentInterface::GetStaticMesh() const
{
	return UStaticMeshComponent::GetStaticMeshComponent(this)->GetStaticMesh();
}

IPrimitiveComponent* FActorStaticMeshComponentInterface::GetPrimitiveComponentInterface() 
{
	return UStaticMeshComponent::GetStaticMeshComponent(this)->GetPrimitiveComponentInterface();
}


#undef LOCTEXT_NAMESPACE

