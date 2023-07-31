// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FeedbackContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/PropertyPortFlags.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Misc/SecureHash.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "AI/NavigationSystemBase.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapePrivate.h"
#include "PhysicsPublic.h"
#include "LandscapeDataAccess.h"
#include "DerivedDataCacheInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeMeshCollisionComponent.h"
#include "FoliageInstanceBase.h"
#include "InstancedFoliageActor.h"
#include "InstancedFoliage.h"
#include "AI/NavigationSystemHelpers.h"
#include "Engine/CollisionProfile.h"
#include "ProfilingDebugging/CookStats.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "EngineGlobals.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Physics/PhysicsFiltering.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceUtils.h"
#include "DynamicMeshBuilder.h"

#include "Chaos/ParticleHandle.h"
#include "Chaos/Vector.h"
#include "Chaos/Core.h"
#include "Chaos/HeightField.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/Experimental/ChaosDerivedData.h"
#include "PhysicsEngine/Experimental/ChaosCooking.h"
#include "Chaos/ChaosArchive.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

using namespace PhysicsInterfaceTypes;

// Global switch for whether to read/write to DDC for landscape cooked data
// It's a lot faster to compute than to request from DDC, so always skip.
bool GLandscapeCollisionSkipDDC = true;
static int32 CVarLandscapeShowCollisionMeshCurrentValue = 0;
static TAutoConsoleVariable<int32> CVarLandscapeShowCollisionMesh(
	TEXT("landscape.ShowCollisionMesh"),
	CVarLandscapeShowCollisionMeshCurrentValue,
	TEXT("Selects which heightfield to visualize when ShowFlags.Collision is used. 0 for simple, 1 for complex, 2 for editor only."),
	ECVF_RenderThreadSafe
);

#if ENABLE_COOK_STATS
namespace LandscapeCollisionCookStats
{
	static FCookStats::FDDCResourceUsageStats HeightfieldUsageStats;
	static FCookStats::FDDCResourceUsageStats MeshUsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		HeightfieldUsageStats.LogStats(AddStat, TEXT("LandscapeCollision.Usage"), TEXT("Heightfield"));
		MeshUsageStats.LogStats(AddStat, TEXT("LandscapeCollision.Usage"), TEXT("Mesh"));
	});
}
#endif

TMap<FGuid, ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef* > GSharedHeightfieldRefs;

ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef::FHeightfieldGeometryRef(FGuid& InGuid)
	: Guid(InGuid)
{
}

ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef::~FHeightfieldGeometryRef()
{
	// Remove ourselves from the shared map.
	GSharedHeightfieldRefs.Remove(Guid);
}

void ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(UsedChaosMaterials.GetAllocatedSize());

	if (Heightfield.IsValid())
	{
		TArray<uint8> Data;
		FMemoryWriter MemAr(Data);
		Chaos::FChaosArchive ChaosAr(MemAr);
		Heightfield->Serialize(ChaosAr);
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Data.Num());
	}

	if (HeightfieldSimple.IsValid())
	{
		TArray<uint8> Data;
		FMemoryWriter MemAr(Data);
		Chaos::FChaosArchive ChaosAr(MemAr);
		HeightfieldSimple->Serialize(ChaosAr);
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Data.Num());
	}
}

TMap<FGuid, ULandscapeMeshCollisionComponent::FTriMeshGeometryRef* > GSharedMeshRefs;

ULandscapeMeshCollisionComponent::FTriMeshGeometryRef::FTriMeshGeometryRef()
{}

ULandscapeMeshCollisionComponent::FTriMeshGeometryRef::FTriMeshGeometryRef(FGuid& InGuid)
	: Guid(InGuid)
{}

ULandscapeMeshCollisionComponent::FTriMeshGeometryRef::~FTriMeshGeometryRef()
{
	// Remove ourselves from the shared map.
	GSharedMeshRefs.Remove(Guid);
}

void ULandscapeMeshCollisionComponent::FTriMeshGeometryRef::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(UsedChaosMaterials.GetAllocatedSize());

	if (Trimesh.IsValid())
	{
		TArray<uint8> Data;
		FMemoryWriter MemAr(Data);
		Chaos::FChaosArchive ChaosAr(MemAr);
		Trimesh->Serialize(ChaosAr);
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(Data.Num());
	}
}

// Generate a new guid to force a recache of landscape collison derived data
#define LANDSCAPE_COLLISION_DERIVEDDATA_VER	TEXT("75E2F3A08BE44420813DD2F2AD34021D")

static FString GetHFDDCKeyString(const FName& Format, bool bDefMaterial, const FGuid& StateId, const TArray<UPhysicalMaterial*>& PhysicalMaterials)
{
	FGuid CombinedStateId;
	
	ensure(StateId.IsValid());

	if (bDefMaterial)
	{
		CombinedStateId = StateId;
	}
	else
	{
		// Build a combined state ID based on both the heightfield state and all physical materials.
		FBufferArchive CombinedStateAr;

		// Add main heightfield state
		FGuid HeightfieldState = StateId;
		CombinedStateAr << HeightfieldState;

		// Add physical materials
		for (UPhysicalMaterial* PhysicalMaterial : PhysicalMaterials)
		{
			FString PhysicalMaterialName = PhysicalMaterial->GetPathName().ToUpper();
			CombinedStateAr << PhysicalMaterialName;
		}

		uint32 Hash[5];
		FSHA1::HashBuffer(CombinedStateAr.GetData(), CombinedStateAr.Num(), (uint8*)Hash);
		CombinedStateId = FGuid(Hash[0] ^ Hash[4], Hash[1], Hash[2], Hash[3]);
	}

	FString InterfacePrefix = FString::Printf(TEXT("%s_%s"), TEXT("CHAOS"), Chaos::ChaosVersionGUID);

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	InterfacePrefix.Append(TEXT("_arm64"));
#endif

	const FString KeyPrefix = FString::Printf(TEXT("%s_%s_%s"), *InterfacePrefix, *Format.ToString(), (bDefMaterial ? TEXT("VIS") : TEXT("FULL")));
	return FDerivedDataCacheInterface::BuildCacheKey(*KeyPrefix, LANDSCAPE_COLLISION_DERIVEDDATA_VER, *CombinedStateId.ToString());
}

void ULandscapeHeightfieldCollisionComponent::OnRegister()
{
	Super::OnRegister();

	if (GetLandscapeProxy())
	{
		// AActor::GetWorld checks for Unreachable and BeginDestroyed
		UWorld* World = GetLandscapeProxy()->GetWorld();
		if (World)
		{
			ULandscapeInfo* Info = GetLandscapeInfo();
			if (Info)
			{
				Info->RegisterCollisionComponent(this);
			}
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::OnUnregister()
{
	Super::OnUnregister();

	if (GetLandscapeProxy())
	{
		// AActor::GetWorld checks for Unreachable and BeginDestroyed
		UWorld* World = GetLandscapeProxy()->GetWorld();

		// Game worlds don't have landscape infos
		if (World)
		{
			ULandscapeInfo* Info = GetLandscapeInfo();
			if (Info)
			{
				Info->UnregisterCollisionComponent(this);
			}
		}
	}
}

ECollisionEnabled::Type ULandscapeHeightfieldCollisionComponent::GetCollisionEnabled() const
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ALandscapeProxy* Proxy = GetLandscapeProxy();

		return Proxy->BodyInstance.GetCollisionEnabled();
	}
	return ECollisionEnabled::QueryAndPhysics;
}

ECollisionResponse ULandscapeHeightfieldCollisionComponent::GetCollisionResponseToChannel(ECollisionChannel Channel) const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetResponseToChannel(Channel);
}

ECollisionChannel ULandscapeHeightfieldCollisionComponent::GetCollisionObjectType() const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetObjectType();
}

const FCollisionResponseContainer& ULandscapeHeightfieldCollisionComponent::GetCollisionResponseToChannels() const
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();

	return Proxy->BodyInstance.GetResponseToChannels();
}

void ULandscapeHeightfieldCollisionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState(); // route OnCreatePhysicsState, skip PrimitiveComponent implementation

	if (!BodyInstance.IsValidBodyInstance())
	{
		CreateCollisionObject();

		// Debug display needs to update its representation, so we invalidate the collision component's render state : 
		MarkRenderStateDirty();

		if (IsValidRef(HeightfieldRef))
		{
			// Make transform for this landscape component PxActor
			FTransform LandscapeComponentTransform = GetComponentToWorld();
			FMatrix LandscapeComponentMatrix = LandscapeComponentTransform.ToMatrixWithScale();
			FVector LandscapeScale = LandscapeComponentMatrix.ExtractScaling();

			// Reorder the axes
			FVector TerrainX = LandscapeComponentMatrix.GetScaledAxis(EAxis::X);
			FVector TerrainY = LandscapeComponentMatrix.GetScaledAxis(EAxis::Y);
			FVector TerrainZ = LandscapeComponentMatrix.GetScaledAxis(EAxis::Z);
			LandscapeComponentMatrix.SetAxis(0, TerrainX);
			LandscapeComponentMatrix.SetAxis(2, TerrainY);
			LandscapeComponentMatrix.SetAxis(1, TerrainZ);
			
			const bool bCreateSimpleCollision = SimpleCollisionSizeQuads > 0;
			const float SimpleCollisionScale = bCreateSimpleCollision ? CollisionScale * CollisionSizeQuads / SimpleCollisionSizeQuads : 0;

			// Create the geometry
			FVector FinalScale(LandscapeScale.X * CollisionScale, LandscapeScale.Y * CollisionScale, LandscapeScale.Z * LANDSCAPE_ZSCALE);

			{
				FActorCreationParams Params;
				Params.InitialTM = LandscapeComponentTransform;
				Params.InitialTM.SetScale3D(FVector(0));
				Params.bQueryOnly = true;
				Params.bStatic = true;
				Params.Scene = GetWorld()->GetPhysicsScene();
				FPhysicsActorHandle PhysHandle;
				FPhysicsInterface::CreateActor(Params, PhysHandle);
				Chaos::FRigidBodyHandle_External& Body_External = PhysHandle->GetGameThreadAPI();

				Chaos::FShapesArray ShapeArray;
				TArray<TUniquePtr<Chaos::FImplicitObject>> Geoms;

				// First add complex geometry
				HeightfieldRef->Heightfield->SetScale(FinalScale * LandscapeComponentTransform.GetScale3D().GetSignVector());
				TUniquePtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>> ChaosHeightFieldFromCooked = MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(MakeSerializable(HeightfieldRef->Heightfield), Chaos::FRigidTransform3(FTransform::Identity));

				TUniquePtr<Chaos::FPerShapeData> NewShape = Chaos::FPerShapeData::CreatePerShapeData(ShapeArray.Num(), MakeSerializable(ChaosHeightFieldFromCooked));

				// Setup filtering
				FCollisionFilterData QueryFilterData, SimFilterData;
				CreateShapeFilterData(GetCollisionObjectType(), FMaskFilter(0), GetOwner()->GetUniqueID(), GetCollisionResponseToChannels(), GetUniqueID(), 0, QueryFilterData, SimFilterData, true, false, true);

				// Heightfield is used for simple and complex collision
				QueryFilterData.Word3 |= bCreateSimpleCollision ? EPDF_ComplexCollision : (EPDF_SimpleCollision | EPDF_ComplexCollision);
				SimFilterData.Word3 |= bCreateSimpleCollision ? EPDF_ComplexCollision : (EPDF_SimpleCollision | EPDF_ComplexCollision);

				NewShape->SetQueryData(QueryFilterData);
				NewShape->SetSimData(SimFilterData);
				NewShape->SetMaterials(HeightfieldRef->UsedChaosMaterials);

				Geoms.Emplace(MoveTemp(ChaosHeightFieldFromCooked));
				ShapeArray.Emplace(MoveTemp(NewShape));

				// Add simple geometry if necessary
				if(bCreateSimpleCollision)
				{
					FVector FinalSimpleCollisionScale(LandscapeScale.X* SimpleCollisionScale, LandscapeScale.Y* SimpleCollisionScale, LandscapeScale.Z* LANDSCAPE_ZSCALE);
					HeightfieldRef->HeightfieldSimple->SetScale(FinalSimpleCollisionScale);
					TUniquePtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>> ChaosSimpleHeightFieldFromCooked = MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(MakeSerializable(HeightfieldRef->HeightfieldSimple), Chaos::FRigidTransform3(FTransform::Identity));

					TUniquePtr<Chaos::FPerShapeData> NewSimpleShape = Chaos::FPerShapeData::CreatePerShapeData(ShapeArray.Num(), MakeSerializable(ChaosSimpleHeightFieldFromCooked));

					FCollisionFilterData QueryFilterDataSimple = QueryFilterData;
					FCollisionFilterData SimFilterDataSimple = SimFilterData;
					QueryFilterDataSimple.Word3 = (QueryFilterDataSimple.Word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;
					SimFilterDataSimple.Word3 = (SimFilterDataSimple.Word3 & ~EPDF_ComplexCollision) | EPDF_SimpleCollision;

					NewSimpleShape->SetQueryData(QueryFilterDataSimple);
					NewSimpleShape->SetSimData(SimFilterDataSimple);
					NewSimpleShape->SetMaterials(HeightfieldRef->UsedChaosMaterials);

					Geoms.Emplace(MoveTemp(ChaosSimpleHeightFieldFromCooked));
					ShapeArray.Emplace(MoveTemp(NewSimpleShape));
				}

#if WITH_EDITOR
				// Create a shape for a heightfield which is used only by the landscape editor
				if(!GetWorld()->IsGameWorld() && !GetOutermost()->bIsCookedForEditor)
				{
					HeightfieldRef->EditorHeightfield->SetScale(FinalScale * LandscapeComponentTransform.GetScale3D().GetSignVector());
					TUniquePtr<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>> ChaosEditorHeightFieldFromCooked = MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(MakeSerializable(HeightfieldRef->EditorHeightfield), Chaos::FRigidTransform3(FTransform::Identity));

					TUniquePtr<Chaos::FPerShapeData> NewEditorShape = Chaos::FPerShapeData::CreatePerShapeData(ShapeArray.Num(), MakeSerializable(ChaosEditorHeightFieldFromCooked));

					FCollisionResponseContainer CollisionResponse;
					CollisionResponse.SetAllChannels(ECollisionResponse::ECR_Ignore);
					CollisionResponse.SetResponse(ECollisionChannel::ECC_Visibility, ECR_Block);
					FCollisionFilterData QueryFilterDataEd, SimFilterDataEd;
					CreateShapeFilterData(ECollisionChannel::ECC_Visibility, FMaskFilter(0), GetOwner()->GetUniqueID(), CollisionResponse, GetUniqueID(), 0, QueryFilterDataEd, SimFilterDataEd, true, false, true);

					QueryFilterDataEd.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);

					NewEditorShape->SetQueryData(QueryFilterDataEd);
					NewEditorShape->SetSimData(SimFilterDataEd);

					Geoms.Emplace(MoveTemp(ChaosEditorHeightFieldFromCooked));
					ShapeArray.Emplace(MoveTemp(NewEditorShape));
				}
#endif // WITH_EDITOR
				// Push the shapes to the actor
				if(Geoms.Num() == 1)
				{
					Body_External.SetGeometry(MoveTemp(Geoms[0]));
				}
				else
				{
					Body_External.SetGeometry(MakeUnique<Chaos::FImplicitObjectUnion>(MoveTemp(Geoms)));
				}

				// Construct Shape Bounds
				for (auto& Shape : ShapeArray)
				{
					Chaos::FRigidTransform3 WorldTransform = Chaos::FRigidTransform3(Body_External.X(), Body_External.R());
					Shape->UpdateShapeBounds(WorldTransform);
				}



				Body_External.SetShapesArray(MoveTemp(ShapeArray));

				// Push the actor to the scene
				FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();

				// Set body instance data
				BodyInstance.PhysicsUserData = FPhysicsUserData(&BodyInstance);
				BodyInstance.OwnerComponent = this;
				BodyInstance.ActorHandle = PhysHandle;

				Body_External.SetUserData(&BodyInstance.PhysicsUserData);

				TArray<FPhysicsActorHandle> Actors;
				Actors.Add(PhysHandle);

				FPhysicsCommand::ExecuteWrite(PhysScene, [&]()
				{
					bool bImmediateAccelStructureInsertion = true;
					PhysScene->AddActorsToScene_AssumesLocked(Actors, bImmediateAccelStructureInsertion);
				});

				PhysScene->AddToComponentMaps(this, PhysHandle);
				if (BodyInstance.bNotifyRigidBodyCollision)
				{
					PhysScene->RegisterForCollisionEvents(this);
				}

			}
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::OnDestroyPhysicsState()
{
	Super::OnDestroyPhysicsState();
	
	if (FPhysScene_Chaos* PhysScene = GetWorld()->GetPhysicsScene())
	{
		FPhysicsActorHandle& ActorHandle = BodyInstance.GetPhysicsActorHandle();
		if (FPhysicsInterface::IsValid(ActorHandle))
		{
			PhysScene->RemoveFromComponentMaps(ActorHandle);
		}
		if (BodyInstance.bNotifyRigidBodyCollision)
		{
			PhysScene->UnRegisterForCollisionEvents(this);
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	if (!bWorldShift || !FPhysScene::SupportsOriginShifting())
	{
		RecreatePhysicsState();
	}
}

// Callback to flag scene proxy as dirty when cvar changes
static void OnLandscapeShowCollisionMeshChanged()
{
	const int32 Value = CVarLandscapeShowCollisionMesh.GetValueOnAnyThread();
	if (CVarLandscapeShowCollisionMeshCurrentValue != Value)
	{
		CVarLandscapeShowCollisionMeshCurrentValue = Value;

		for (ULandscapeHeightfieldCollisionComponent* LandscapeHeightfieldCollisionComponent : TObjectRange<ULandscapeHeightfieldCollisionComponent>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
		{
			LandscapeHeightfieldCollisionComponent->MarkRenderStateDirty();
		}
	}
}

// Registers callback with cvar
static FAutoConsoleVariableSink OnLandscapeCollisionHeightfieldChangedSink(FConsoleCommandDelegate::CreateStatic(&OnLandscapeShowCollisionMeshChanged));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EDITORONLY_DATA
FPrimitiveSceneProxy* ULandscapeHeightfieldCollisionComponent::CreateSceneProxy()
{
	class FLandscapeHeightfieldCollisionComponentSceneProxy final : public FPrimitiveSceneProxy
	{
	public:
		SIZE_T GetTypeHash() const override
		{
			static size_t UniquePointer;
			return reinterpret_cast<size_t>(&UniquePointer);
		}

		// Constructor exists to populate Vertices and Indices arrays which are
		// used to construct the collision mesh inside GetDynamicMeshElements
		FLandscapeHeightfieldCollisionComponentSceneProxy(const ULandscapeHeightfieldCollisionComponent* InComponent, const Chaos::FHeightField& InHeightfield, const FLinearColor& InWireframeColor)
			: FPrimitiveSceneProxy(InComponent)
		{
			const Chaos::FHeightField::FData<uint16>& GeomData = InHeightfield.GeomData; 
			const int32 NumRows = InHeightfield.GetNumRows();
			const int32 NumCols = InHeightfield.GetNumCols();
			const int32 NumVerts = NumRows * NumCols;
			const int32 NumTris = (NumRows - 1) * (NumCols - 1) * 2;
			Vertices.SetNumUninitialized(NumVerts);
			for (int32 I = 0; I < NumVerts; I++)
			{
				Chaos::FVec3 Point = GeomData.GetPointScaled(I);
				Vertices[I].Position = FVector3f(Point.X, Point.Y, Point.Z);
			}
			Indices.SetNumUninitialized(NumTris * 3);

			// Editor heightfields don't have material indices (hence, no holes), in which case InHeightfield.GeomData.MaterialIndices.Num() == 1 : 
			const int32 NumMaterialIndices = InHeightfield.GeomData.MaterialIndices.Num();
			const bool bHasMaterialIndices = (NumMaterialIndices > 1);
			check(!bHasMaterialIndices || (NumMaterialIndices == ((NumRows - 1) * (NumCols - 1))));

			int32 TriangleIdx = 0;
			for (int32 Y = 0; Y < (NumRows - 1); Y++)
			{
				for (int32 X = 0; X < (NumCols - 1); X++)
				{
					int32 DataIdx = X + Y * NumCols;
					bool bHole = false;

					if (bHasMaterialIndices)
					{
						// Material indices don't have the final row/column : 
						int32 MaterialIndicesDataIdx = X + Y * (NumCols - 1);
						uint8 LayerIdx = InHeightfield.GeomData.MaterialIndices[MaterialIndicesDataIdx];
						bHole = (LayerIdx == TNumericLimits<uint8>::Max());
					}

					if (bHole)
					{
						Indices[TriangleIdx + 0] = (X + 0) + (Y + 0) * NumCols;
						Indices[TriangleIdx + 1] = Indices[TriangleIdx + 0];
						Indices[TriangleIdx + 2] = Indices[TriangleIdx + 0];
					}
					else
					{
						Indices[TriangleIdx + 0] = (X + 0) + (Y + 0) * NumCols;
						Indices[TriangleIdx + 1] = (X + 1) + (Y + 1) * NumCols;
						Indices[TriangleIdx + 2] = (X + 1) + (Y + 0) * NumCols;
					}

					TriangleIdx += 3;

					if (bHole)
					{
						Indices[TriangleIdx + 0] = (X + 0) + (Y + 0) * NumCols;
						Indices[TriangleIdx + 1] = Indices[TriangleIdx + 0];
						Indices[TriangleIdx + 2] = Indices[TriangleIdx + 0];
					}
					else
					{
						Indices[TriangleIdx + 0] = (X + 0) + (Y + 0) * NumCols;
						Indices[TriangleIdx + 1] = (X + 0) + (Y + 1) * NumCols;
						Indices[TriangleIdx + 2] = (X + 1) + (Y + 1) * NumCols;
					}

					TriangleIdx += 3;
				}
			}

			WireframeMaterialInstance.Reset(new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
				InWireframeColor));
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			FMatrix LocalToWorldNoScale = GetLocalToWorld();
			LocalToWorldNoScale.RemoveScaling();

			const bool bDrawCollision = ViewFamily.EngineShowFlags.Collision && IsCollisionEnabled();

			if (bDrawCollision && AllowDebugViewmodes() && WireframeMaterialInstance.IsValid())
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						// Set up mesh builder
						FDynamicMeshBuilder MeshBuilder(View->GetFeatureLevel());
						MeshBuilder.AddVertices(Vertices);
						MeshBuilder.AddTriangles(Indices);

						MeshBuilder.GetMesh(LocalToWorldNoScale, WireframeMaterialInstance.Get(), SDPG_World, false, false, ViewIndex, Collector);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			// Should we draw this because collision drawing is enabled, and we have collision
			const bool bShowForCollision = View->Family->EngineShowFlags.Collision && IsCollisionEnabled();

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) || bShowForCollision;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = false;
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return FPrimitiveSceneProxy::GetAllocatedSize(); }

	private:
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
		TUniquePtr<FColoredMaterialRenderProxy> WireframeMaterialInstance = nullptr;
	};

	FLandscapeHeightfieldCollisionComponentSceneProxy* Proxy = nullptr;

	if (HeightfieldRef.IsValid() && IsValidRef(HeightfieldRef))
	{
		const Chaos::FHeightField* LocalHeightfield = nullptr;
		FLinearColor WireframeColor;

		switch (static_cast<EHeightfieldSource>(CVarLandscapeShowCollisionMesh.GetValueOnGameThread()))
		{
		case EHeightfieldSource::Simple:
			if (HeightfieldRef->HeightfieldSimple.IsValid())
			{
				LocalHeightfield = HeightfieldRef->HeightfieldSimple.Get();
			}
			else if (HeightfieldRef->Heightfield.IsValid())
			{
				LocalHeightfield = HeightfieldRef->Heightfield.Get();
			}

			WireframeColor = FColor(157, 149, 223, 255);
			break;

		case EHeightfieldSource::Complex:
			if (HeightfieldRef->Heightfield.IsValid())
			{
				LocalHeightfield = HeightfieldRef->Heightfield.Get();
			}

			WireframeColor = FColor(0, 255, 255, 255);
			break;

		case EHeightfieldSource::Editor:
			if (HeightfieldRef->EditorHeightfield.IsValid())
			{
				LocalHeightfield = HeightfieldRef->EditorHeightfield.Get();
			}

			WireframeColor = FColor(157, 223, 149, 255);
			break;

		default:
			UE_LOG(LogLandscape, Warning, TEXT("Invalid Value for CVar landscape.ShowCollisionMesh"));
		}

		if (LocalHeightfield != nullptr)
		{
			Proxy = new FLandscapeHeightfieldCollisionComponentSceneProxy(this, *LocalHeightfield, WireframeColor);
		}
	}

	return Proxy;
}
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EDITORONLY_DATA

void ULandscapeHeightfieldCollisionComponent::CreateCollisionObject()
{
	LLM_SCOPE(ELLMTag::ChaosLandscape);

	// If we have not created a heightfield yet - do it now.
	if (!IsValidRef(HeightfieldRef))
	{
		UWorld* World = GetWorld();

#if WITH_EDITOR
		const bool bNeedsEditorHeightField = !World->IsGameWorld() && !GetOutermost()->bIsCookedForEditor;
#endif // WITH_EDITOR
		FHeightfieldGeometryRef* ExistingHeightfieldRef = nullptr;
		bool bCheckDDC = true;

		if (!HeightfieldGuid.IsValid())
		{
			HeightfieldGuid = FGuid::NewGuid();
			bCheckDDC = false;
		}
		else
		{
			// Look for a heightfield object with the current Guid (this occurs with PIE)
			ExistingHeightfieldRef = GSharedHeightfieldRefs.FindRef(HeightfieldGuid);
		}

#if WITH_EDITOR
		// Use existing heightfield except if it is missing its editor heightfield and the component needs it.
		if (ExistingHeightfieldRef && (!bNeedsEditorHeightField || ExistingHeightfieldRef->EditorHeightfield != nullptr))
#else // WITH_EDITOR
		if (ExistingHeightfieldRef)
#endif // !WITH_EDITOR
		{
			HeightfieldRef = ExistingHeightfieldRef;
		}
		else
		{
#if WITH_EDITOR
			// This should only occur if a level prior to VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING 
			// was resaved using a commandlet and not saved in the editor, or if a PhysicalMaterial asset was deleted.
			if (CookedPhysicalMaterials.Num() == 0 || CookedPhysicalMaterials.Contains(nullptr))
			{
				bCheckDDC = false;
			}

			// Prepare heightfield data
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
			CookCollisionData(PhysicsFormatName, false, bCheckDDC, CookedCollisionData, CookedPhysicalMaterials);

			// The World will clean up any speculatively-loaded data we didn't end up using.
			SpeculativeDDCRequest.Reset();
#endif //WITH_EDITOR

			if (CookedCollisionData.Num())
			{
				HeightfieldRef = GSharedHeightfieldRefs.Add(HeightfieldGuid, new FHeightfieldGeometryRef(HeightfieldGuid));

				// Create heightfields
				{
					FMemoryReader Reader(CookedCollisionData);
					Chaos::FChaosArchive Ar(Reader);
					bool bContainsSimple = false;
					Ar << bContainsSimple;
					Ar << HeightfieldRef->Heightfield;

					if(bContainsSimple)
					{
						Ar << HeightfieldRef->HeightfieldSimple;
					}
				}

				// Register materials
				for(UPhysicalMaterial* PhysicalMaterial : CookedPhysicalMaterials)
				{
					//TODO: Figure out why we are getting into a state like this (PhysicalMaterial == nullptr) in the first place. Potentially a loading issue
					if (PhysicalMaterial)
					{
						//todo: total hack until we get landscape fully converted to chaos
						HeightfieldRef->UsedChaosMaterials.Add(PhysicalMaterial->GetPhysicsMaterial());
					}
				}

				// Release cooked collison data
				// In cooked builds created collision object will never be deleted while component is alive, so we don't need this data anymore
				if(FPlatformProperties::RequiresCookedData() || World->IsGameWorld())
				{
					CookedCollisionData.Empty();
				}

#if WITH_EDITOR
				// Create heightfield for the landscape editor (no holes in it)
				if(bNeedsEditorHeightField)
				{
					TArray<UPhysicalMaterial*> CookedMaterialsEd;
					if(CookCollisionData(PhysicsFormatName, true, bCheckDDC, CookedCollisionDataEd, CookedMaterialsEd))
					{
						FMemoryReader Reader(CookedCollisionDataEd);
						Chaos::FChaosArchive Ar(Reader);

						// Don't actually care about this but need to strip it out of the data
						bool bContainsSimple = false;
						Ar << bContainsSimple;
						Ar << HeightfieldRef->EditorHeightfield;

						CookedCollisionDataEd.Empty();
					}
				}
#endif //WITH_EDITOR
			}
		}
	}
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::SpeculativelyLoadAsyncDDCCollsionData()
{
	if (GetLinkerUEVersion() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS && !GLandscapeCollisionSkipDDC)
	{
		UWorld* World = GetWorld();
		if (World && HeightfieldGuid.IsValid() && CookedPhysicalMaterials.Num() > 0 && GSharedHeightfieldRefs.FindRef(HeightfieldGuid) == nullptr)
		{
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());

			FString Key = GetHFDDCKeyString(PhysicsFormatName, false, HeightfieldGuid, CookedPhysicalMaterials);
			uint32 Handle = GetDerivedDataCacheRef().GetAsynchronous(*Key, GetPathName());
			check(!SpeculativeDDCRequest.IsValid());
			SpeculativeDDCRequest = MakeShareable(new FAsyncPreRegisterDDCRequest(Key, Handle));
			World->AsyncPreRegisterDDCRequests.Add(SpeculativeDDCRequest);
		}
	}
}

bool ULandscapeHeightfieldCollisionComponent::CookCollisionData(const FName& Format, bool bUseDefMaterial, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ULandscapeHeightfieldCollisionComponent::CookCollisionData);

	if (GetOutermost()->bIsCookedForEditor)
	{
		return true;
	}

	// Use existing cooked data unless !bCheckDDC in which case the data must be rebuilt.
	if (bCheckDDC && OutCookedData.Num() > 0)
	{
		return true;
	}

	COOK_STAT(auto Timer = LandscapeCollisionCookStats::HeightfieldUsageStats.TimeSyncWork());
	 
	bool Succeeded = false;
	TArray<uint8> OutData;

	// we have 2 versions of collision objects
	const int32 CookedDataIndex = bUseDefMaterial ? 0 : 1;

	if (!GLandscapeCollisionSkipDDC && bCheckDDC && HeightfieldGuid.IsValid())
	{
		// Ensure that content was saved with physical materials before using DDC data
		if (GetLinkerUEVersion() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS)
		{
			FString DDCKey = GetHFDDCKeyString(Format, bUseDefMaterial, HeightfieldGuid, InOutMaterials);

			// Check if the speculatively-loaded data loaded and is what we wanted
			if (SpeculativeDDCRequest.IsValid() && DDCKey == SpeculativeDDCRequest->GetKey())
			{
				// If we have a DDC request in flight, just time the synchronous cycles used.
				COOK_STAT(auto WaitTimer = LandscapeCollisionCookStats::HeightfieldUsageStats.TimeAsyncWait());
				SpeculativeDDCRequest->WaitAsynchronousCompletion();
				bool bSuccess = SpeculativeDDCRequest->GetAsynchronousResults(OutCookedData);
				// World will clean up remaining reference
				SpeculativeDDCRequest.Reset();
				if (bSuccess)
				{
					COOK_STAT(Timer.Cancel());
					COOK_STAT(WaitTimer.AddHit(OutCookedData.Num()));
					bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
					return true;
				}
				else
				{
					// If the DDC request failed, then we waited for nothing and will build the resource anyway. Just ignore the wait timer and treat it all as sync time.
					COOK_STAT(WaitTimer.Cancel());
				}
			}

			if (GetDerivedDataCacheRef().GetSynchronous(*DDCKey, OutCookedData, GetPathName()))
			{
				COOK_STAT(Timer.AddHit(OutCookedData.Num()));
				bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
				return true;
			}
		}
	}

	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (!Proxy || !Proxy->GetRootComponent())
	{
		// We didn't actually build anything, so just track the cycles.
		COOK_STAT(Timer.TrackCyclesOnly());
		return false;
	}

	UPhysicalMaterial* DefMaterial = Proxy->DefaultPhysMaterial ? Proxy->DefaultPhysMaterial : GEngine->DefaultPhysMaterial;

	// GetComponentTransform() might not be initialized at this point, so use landscape transform
	const FVector LandscapeScale = Proxy->GetRootComponent()->GetRelativeScale3D();
	const bool bIsMirrored = (LandscapeScale.X*LandscapeScale.Y*LandscapeScale.Z) < 0.f;

	const bool bGenerateSimpleCollision = SimpleCollisionSizeQuads > 0 && !bUseDefMaterial;

	const int32 CollisionSizeVerts = CollisionSizeQuads + 1;
	const int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
	const int32 NumSamples = FMath::Square(CollisionSizeVerts);
	const int32 NumSimpleSamples = FMath::Square(SimpleCollisionSizeVerts);

	const uint16* Heights = (const uint16*)CollisionHeightData.LockReadOnly();
	check(CollisionHeightData.GetElementCount() == NumSamples + NumSimpleSamples);
	const uint16* SimpleHeights = Heights + NumSamples;

	// Physical material data from layer system
	const uint8* DominantLayers = nullptr;
	const uint8* SimpleDominantLayers = nullptr;
	if (DominantLayerData.GetElementCount())
	{
		DominantLayers = (const uint8*)DominantLayerData.LockReadOnly();
		check(DominantLayerData.GetElementCount() == NumSamples + NumSimpleSamples);
		SimpleDominantLayers = DominantLayers + NumSamples;
	}

	// Physical material data from render material graph
	const uint8* RenderPhysicalMaterialIds = nullptr;
	const uint8* SimpleRenderPhysicalMaterialIds = nullptr;
	if (PhysicalMaterialRenderData.GetElementCount())
	{
		RenderPhysicalMaterialIds = (const uint8*)PhysicalMaterialRenderData.LockReadOnly();
		check(PhysicalMaterialRenderData.GetElementCount() == NumSamples + NumSimpleSamples);
		SimpleRenderPhysicalMaterialIds = RenderPhysicalMaterialIds + NumSamples;
	}

	// List of materials which is actually used by heightfield
	InOutMaterials.Empty();

	// Generate material indices
	TArray<uint8> MaterialIndices;
	MaterialIndices.Reserve(NumSamples + NumSimpleSamples);

	auto ResolveMaterials = [&MaterialIndices, &bIsMirrored, &bUseDefMaterial, &DefMaterial, &InOutMaterials, this](int32 InCollisionVertExtent, const uint8* InDominantLayers, const uint8* InRenderMaterialIds)
	{
		for(int32 RowIndex = 0; RowIndex < InCollisionVertExtent; RowIndex++)
		{
			for(int32 ColIndex = 0; ColIndex < InCollisionVertExtent; ColIndex++)
			{
				const int32 SrcSampleIndex = (RowIndex * InCollisionVertExtent) + (bIsMirrored ? (InCollisionVertExtent - ColIndex - 1) : ColIndex);

				// Materials are not relevant on the last row/column because they are per-triangle and the last row/column don't own any
				if(RowIndex < InCollisionVertExtent - 1 &&
				   ColIndex < InCollisionVertExtent - 1)
				{
					int32 MaterialIndex = 0; // Default physical material.
					if(!bUseDefMaterial)
					{
						uint8 DominantLayerIdx = InDominantLayers ? InDominantLayers[SrcSampleIndex] : -1;
						ULandscapeLayerInfoObject* Layer = ComponentLayerInfos.IsValidIndex(DominantLayerIdx) ? ToRawPtr(ComponentLayerInfos[DominantLayerIdx]) : nullptr;

						if(Layer == ALandscapeProxy::VisibilityLayer)
						{
							// If it's a hole, use the final index
							MaterialIndex = TNumericLimits<uint8>::Max();
						}
						else if(InRenderMaterialIds)
						{
							uint8 RenderIdx = InRenderMaterialIds[SrcSampleIndex];
							UPhysicalMaterial* DominantMaterial = RenderIdx > 0 ? ToRawPtr(PhysicalMaterialRenderObjects[RenderIdx - 1]) : DefMaterial;
							MaterialIndex = InOutMaterials.AddUnique(DominantMaterial);
						}
						else
						{
							UPhysicalMaterial* DominantMaterial = Layer && Layer->PhysMaterial ? ToRawPtr(Layer->PhysMaterial) : DefMaterial;
							MaterialIndex = InOutMaterials.AddUnique(DominantMaterial);
						}
					}
					MaterialIndices.Add(MaterialIndex);
				}
			}
		}
	};

	ResolveMaterials(CollisionSizeVerts, DominantLayers, RenderPhysicalMaterialIds);
	ResolveMaterials(SimpleCollisionSizeVerts, SimpleDominantLayers, SimpleRenderPhysicalMaterialIds);

	TUniquePtr<Chaos::FHeightField> Heightfield = nullptr;
	TUniquePtr<Chaos::FHeightField> HeightfieldSimple = nullptr;

	FMemoryWriter Writer(OutData);
	Chaos::FChaosArchive Ar(Writer);

	bool bSerializeGenerateSimpleCollision = bGenerateSimpleCollision;
	Ar << bSerializeGenerateSimpleCollision;

	const int32 NumCollisionCells = FMath::Square(CollisionSizeQuads);
	const int32 NumSimpleCollisionCells = FMath::Square(SimpleCollisionSizeQuads);

	TArrayView<const uint16> ComplexHeightView(Heights, NumSamples);
	TArrayView<uint8> ComplexMaterialIndicesView(MaterialIndices.GetData(), NumCollisionCells);
	Heightfield = MakeUnique<Chaos::FHeightField>(ComplexHeightView, ComplexMaterialIndicesView, CollisionSizeVerts, CollisionSizeVerts, Chaos::FVec3(1));
	Ar << Heightfield;
	if(bGenerateSimpleCollision)
	{
		TArrayView<const uint16> SimpleHeightView(SimpleHeights, NumSimpleSamples);
		TArrayView<uint8> SimpleMaterialIndicesView(MaterialIndices.GetData() + NumCollisionCells, NumSimpleCollisionCells);
		HeightfieldSimple = MakeUnique<Chaos::FHeightField>(SimpleHeightView, SimpleMaterialIndicesView, SimpleCollisionSizeVerts, SimpleCollisionSizeVerts, Chaos::FVec3(1));
		Ar << HeightfieldSimple;
	}

	Succeeded = true;

	if (CollisionHeightData.IsLocked())
	{
		CollisionHeightData.Unlock();
	}
	if (DominantLayerData.IsLocked())
	{
		DominantLayerData.Unlock();
	}
	if (PhysicalMaterialRenderData.IsLocked())
	{
		PhysicalMaterialRenderData.Unlock();
	}

	if (Succeeded)
	{
		COOK_STAT(Timer.AddMiss(OutData.Num()));
		OutCookedData.SetNumUninitialized(OutData.Num());
		FMemory::Memcpy(OutCookedData.GetData(), OutData.GetData(), OutData.Num());

		if (!GLandscapeCollisionSkipDDC && bShouldSaveCookedDataToDDC[CookedDataIndex] && HeightfieldGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(Format, bUseDefMaterial, HeightfieldGuid, InOutMaterials), OutCookedData, GetPathName());
			bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
		}
	}
	else
	{
		// if we failed to build the resource, just time the cycles we spent.
		COOK_STAT(Timer.TrackCyclesOnly());
		OutCookedData.Empty();
		InOutMaterials.Empty();
	}

	return Succeeded;
}

bool ULandscapeMeshCollisionComponent::CookCollisionData(const FName& Format, bool bUseDefMaterial, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const
{
	// Use existing cooked data unless !bCheckDDC in which case the data must be rebuilt.
	if (bCheckDDC && OutCookedData.Num() > 0)
	{
		return true;
	}

	COOK_STAT(auto Timer = LandscapeCollisionCookStats::MeshUsageStats.TimeSyncWork());
	// we have 2 versions of collision objects
	const int32 CookedDataIndex = bUseDefMaterial ? 0 : 1;

	if (!GLandscapeCollisionSkipDDC && bCheckDDC)
	{
		// Ensure that content was saved with physical materials before using DDC data
		if (GetLinkerUEVersion() >= VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS && MeshGuid.IsValid())
		{
			FString DDCKey = GetHFDDCKeyString(Format, bUseDefMaterial, MeshGuid, InOutMaterials);

			// Check if the speculatively-loaded data loaded and is what we wanted
			if (SpeculativeDDCRequest.IsValid() && DDCKey == SpeculativeDDCRequest->GetKey())
			{
				// If we have a DDC request in flight, just time the synchronous cycles used.
				COOK_STAT(auto WaitTimer = LandscapeCollisionCookStats::MeshUsageStats.TimeAsyncWait());
				SpeculativeDDCRequest->WaitAsynchronousCompletion();
				bool bSuccess = SpeculativeDDCRequest->GetAsynchronousResults(OutCookedData);
				// World will clean up remaining reference
				SpeculativeDDCRequest.Reset();
				if (bSuccess)
				{
					COOK_STAT(Timer.Cancel());
					COOK_STAT(WaitTimer.AddHit(OutCookedData.Num()));
					bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
					return true;
				}
				else
				{
					// If the DDC request failed, then we waited for nothing and will build the resource anyway. Just ignore the wait timer and treat it all as sync time.
					COOK_STAT(WaitTimer.Cancel());
				}
			}

			if (GetDerivedDataCacheRef().GetSynchronous(*DDCKey, OutCookedData, GetPathName()))
			{
				COOK_STAT(Timer.AddHit(OutCookedData.Num()));
				bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
				return true;
			}
		}
	}
	
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	UPhysicalMaterial* DefMaterial = (Proxy && Proxy->DefaultPhysMaterial != nullptr) ? Proxy->DefaultPhysMaterial : GEngine->DefaultPhysMaterial;

	// List of materials which is actually used by trimesh
	InOutMaterials.Empty();

	TArray<FVector3f>		Vertices;
	TArray<FTriIndices>		Indices;
	TArray<uint16>			MaterialIndices;

	const int32 CollisionSizeVerts = CollisionSizeQuads + 1;
	const int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
	const int32 NumVerts = FMath::Square(CollisionSizeVerts);
	const int32 NumSimpleVerts = FMath::Square(SimpleCollisionSizeVerts);

	const uint16* Heights = (const uint16*)CollisionHeightData.LockReadOnly();
	const uint16* XYOffsets = (const uint16*)CollisionXYOffsetData.LockReadOnly();
	check(CollisionHeightData.GetElementCount() == NumVerts + NumSimpleVerts);
	check(CollisionXYOffsetData.GetElementCount() == NumVerts * 2);

	const uint8* DominantLayers = nullptr;
	if (DominantLayerData.GetElementCount() > 0)
	{
		DominantLayers = (const uint8*)DominantLayerData.LockReadOnly();
	}

	// Scale all verts into temporary vertex buffer.
	Vertices.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; i++)
	{
		int32 X = i % CollisionSizeVerts;
		int32 Y = i / CollisionSizeVerts;
		Vertices[i].Set(X + ((float)XYOffsets[i * 2] - 32768.f) * LANDSCAPE_XYOFFSET_SCALE, Y + ((float)XYOffsets[i * 2 + 1] - 32768.f) * LANDSCAPE_XYOFFSET_SCALE, ((float)Heights[i] - 32768.f) * LANDSCAPE_ZSCALE);
	}

	const int32 NumTris = FMath::Square(CollisionSizeQuads) * 2;
	Indices.SetNumUninitialized(NumTris);
	if (DominantLayers)
	{
		MaterialIndices.SetNumUninitialized(NumTris);
	}

	int32 TriangleIdx = 0;
	for (int32 y = 0; y < CollisionSizeQuads; y++)
	{
		for (int32 x = 0; x < CollisionSizeQuads; x++)
		{
			int32 DataIdx = x + y * CollisionSizeVerts;
			bool bHole = false;

			int32 MaterialIndex = 0; // Default physical material.
			if (!bUseDefMaterial && DominantLayers)
			{
				uint8 DominantLayerIdx = DominantLayers[DataIdx];
				if (ComponentLayerInfos.IsValidIndex(DominantLayerIdx))
				{
					ULandscapeLayerInfoObject* Layer = ComponentLayerInfos[DominantLayerIdx];
					if (Layer == ALandscapeProxy::VisibilityLayer)
					{
						// If it's a hole, override with the hole flag.
						bHole = true;
					}
					else
					{
						UPhysicalMaterial* DominantMaterial = Layer && Layer->PhysMaterial ? ToRawPtr(Layer->PhysMaterial) : DefMaterial;
						MaterialIndex = InOutMaterials.AddUnique(DominantMaterial);
					}
				}
			}

			FTriIndices& TriIndex1 = Indices[TriangleIdx];
			if (bHole)
			{
				TriIndex1.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex1.v1 = TriIndex1.v0;
				TriIndex1.v2 = TriIndex1.v0;
			}
			else
			{
				TriIndex1.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex1.v1 = (x + 1) + (y + 1) * CollisionSizeVerts;
				TriIndex1.v2 = (x + 1) + (y + 0) * CollisionSizeVerts;
			}

			if (DominantLayers)
			{
				MaterialIndices[TriangleIdx] = MaterialIndex;
			}
			TriangleIdx++;

			FTriIndices& TriIndex2 = Indices[TriangleIdx];
			if (bHole)
			{
				TriIndex2.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex2.v1 = TriIndex2.v0;
				TriIndex2.v2 = TriIndex2.v0;
			}
			else
			{
				TriIndex2.v0 = (x + 0) + (y + 0) * CollisionSizeVerts;
				TriIndex2.v1 = (x + 0) + (y + 1) * CollisionSizeVerts;
				TriIndex2.v2 = (x + 1) + (y + 1) * CollisionSizeVerts;
			}

			if (DominantLayers)
			{
				MaterialIndices[TriangleIdx] = MaterialIndex;
			}
			TriangleIdx++;
		}
	}

	CollisionHeightData.Unlock();
	CollisionXYOffsetData.Unlock();
	if (DominantLayers)
	{
		DominantLayerData.Unlock();
	}

	// Add the default physical material to be used used when we have no dominant data.
	if (InOutMaterials.Num() == 0)
	{
		InOutMaterials.Add(DefMaterial);
	}

	TArray<uint8> OutData;
	bool Result = false;

	FCookBodySetupInfo CookInfo;
	FTriMeshCollisionData& MeshDesc = CookInfo.TriangleMeshDesc;
	MeshDesc.bFlipNormals = true;
	MeshDesc.Vertices = MoveTemp(Vertices);
	MeshDesc.Indices = MoveTemp(Indices);
	MeshDesc.MaterialIndices = MoveTemp(MaterialIndices);
	CookInfo.bCookTriMesh = true;
	TArray<int32> FaceRemap;
	TArray<int32> VertexRemap;
	TUniquePtr<Chaos::FTriangleMeshImplicitObject> Trimesh = Chaos::Cooking::BuildSingleTrimesh(MeshDesc, FaceRemap, VertexRemap);

	if(Trimesh.IsValid())
	{
		FMemoryWriter Ar(OutData);
		Chaos::FChaosArchive ChaosAr(Ar);
		ChaosAr << Trimesh;
		Result = OutData.Num() > 0;
	}

	if (Result)
	{
		COOK_STAT(Timer.AddMiss(OutData.Num()));
		OutCookedData.SetNumUninitialized(OutData.Num());
		FMemory::Memcpy(OutCookedData.GetData(), OutData.GetData(), OutData.Num());

		if (!GLandscapeCollisionSkipDDC && bShouldSaveCookedDataToDDC[CookedDataIndex] && MeshGuid.IsValid())
		{
			GetDerivedDataCacheRef().Put(*GetHFDDCKeyString(Format, bUseDefMaterial, MeshGuid, InOutMaterials), OutCookedData, GetPathName());
			bShouldSaveCookedDataToDDC[CookedDataIndex] = false;
		}
	}
	else
	{
		// We didn't actually build anything, so just track the cycles.
		COOK_STAT(Timer.TrackCyclesOnly());
		OutCookedData.Empty();
		InOutMaterials.Empty();
	}

	return Result;
}
#endif //WITH_EDITOR

void ULandscapeMeshCollisionComponent::CreateCollisionObject()
{
	// If we have not created a heightfield yet - do it now.
	if (!IsValidRef(MeshRef))
	{
		FTriMeshGeometryRef* ExistingMeshRef = nullptr;
		bool bCheckDDC = true;

		if (!MeshGuid.IsValid())
		{
			MeshGuid = FGuid::NewGuid();
			bCheckDDC = false;
		}
		else
		{
			// Look for a heightfield object with the current Guid (this occurs with PIE)
			ExistingMeshRef = GSharedMeshRefs.FindRef(MeshGuid);
		}

		if (ExistingMeshRef)
		{
			MeshRef = ExistingMeshRef;
		}
		else
		{
#if WITH_EDITOR
		    // This should only occur if a level prior to VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING 
		    // was resaved using a commandlet and not saved in the editor, or if a PhysicalMaterial asset was deleted.
		    if (CookedPhysicalMaterials.Num() == 0 || CookedPhysicalMaterials.Contains(nullptr))
		    {
			    bCheckDDC = false;
		    }

			// Create cooked physics data
			static FName PhysicsFormatName(FPlatformProperties::GetPhysicsFormat());
			CookCollisionData(PhysicsFormatName, false, bCheckDDC, CookedCollisionData, CookedPhysicalMaterials);
#endif // WITH_EDITOR

			if (CookedCollisionData.Num())
			{
				MeshRef = GSharedMeshRefs.Add(MeshGuid, new FTriMeshGeometryRef(MeshGuid));

				// Create physics objects
				FMemoryReader Reader(CookedCollisionData);
				Chaos::FChaosArchive Ar(Reader);
				Ar << MeshRef->Trimesh;

				for (UPhysicalMaterial* PhysicalMaterial : CookedPhysicalMaterials)
				{
					MeshRef->UsedChaosMaterials.Add(PhysicalMaterial->GetPhysicsMaterial());
				}

				// Release cooked collison data
				// In cooked builds created collision object will never be deleted while component is alive, so we don't need this data anymore
				if (FPlatformProperties::RequiresCookedData() || GetWorld()->IsGameWorld())
				{
					CookedCollisionData.Empty();
				}

#if WITH_EDITOR
				// Create collision mesh for the landscape editor (no holes in it)
				if (!GetWorld()->IsGameWorld())
				{
					TArray<UPhysicalMaterial*> CookedMaterialsEd;
					if (CookCollisionData(PhysicsFormatName, true, bCheckDDC, CookedCollisionDataEd, CookedMaterialsEd))
					{
						FMemoryReader EdReader(CookedCollisionData);
						Chaos::FChaosArchive EdAr(EdReader);
						EdAr << MeshRef->EditorTrimesh;
					}
				}
#endif //WITH_EDITOR
			}
		}
	}
}


ULandscapeMeshCollisionComponent::ULandscapeMeshCollisionComponent()
{
	// make landscape always create? 
	bAlwaysCreatePhysicsState = true;
}

ULandscapeMeshCollisionComponent::~ULandscapeMeshCollisionComponent() = default;

struct FMeshCollisionInitHelper
{
	FMeshCollisionInitHelper() = delete;
	FMeshCollisionInitHelper(TRefCountPtr<ULandscapeMeshCollisionComponent::FTriMeshGeometryRef> InMeshRef, UWorld* InWorld, UPrimitiveComponent* InComponent, FBodyInstance* InBodyInstance)
		: ComponentToWorld(FTransform::Identity)
		, ComponentScale(FVector::OneVector)
		, CollisionScale(1.0f)
		, MeshRef(InMeshRef)
		, World(InWorld)
		, Component(InComponent)
		, TargetInstance(InBodyInstance)
	{
		check(World);
		PhysScene = World->GetPhysicsScene();
		check(PhysScene);
		check(Component);
		check(TargetInstance);
	}

	void SetComponentScale3D(const FVector& InScale)
	{
		ComponentScale = InScale;
	}

	void SetCollisionScale(float InScale)
	{
		CollisionScale = InScale;
	}

	void SetComponentToWorld(const FTransform& InTransform)
	{
		ComponentToWorld = InTransform;
	}

	void SetFilters(const FCollisionFilterData& InQueryFilter, const FCollisionFilterData& InSimulationFilter)
	{
		QueryFilter = InQueryFilter;
		SimulationFilter = InSimulationFilter;
	}

	void SetEditorFilter(const FCollisionFilterData& InFilter)
	{
		QueryFilterEd = InFilter;
	}

	bool IsGeometryValid() const
	{
		return MeshRef->Trimesh.IsValid();
	}

	void CreateActors()
	{
		Chaos::FShapesArray ShapeArray;
		TArray<TUniquePtr<Chaos::FImplicitObject>> Geometries;
		
		FActorCreationParams Params;
		Params.InitialTM = ComponentToWorld;
		Params.InitialTM.SetScale3D(FVector::OneVector);
		Params.bQueryOnly = true;
		Params.bStatic = true;
		Params.Scene = PhysScene;

		FPhysicsInterface::CreateActor(Params, ActorHandle);

		FVector Scale = FVector(ComponentScale.X * CollisionScale, ComponentScale.Y * CollisionScale, ComponentScale.Z);

		TSharedPtr<Chaos::FTriangleMeshImplicitObject, ESPMode::ThreadSafe> SharedPtrForRefCount(nullptr); // Not shared trimesh, no need for ref counting trimesh.
		{
			TUniquePtr<Chaos::TImplicitObjectScaled<Chaos::FTriangleMeshImplicitObject>> ScaledTrimesh = MakeUnique<Chaos::TImplicitObjectScaled<Chaos::FTriangleMeshImplicitObject>>(MakeSerializable(MeshRef->Trimesh), SharedPtrForRefCount, Scale);
			TUniquePtr<Chaos::FPerShapeData> NewShape = Chaos::FPerShapeData::CreatePerShapeData(ShapeArray.Num(), MakeSerializable(ScaledTrimesh));

			NewShape->SetQueryData(QueryFilter);
			NewShape->SetSimData(SimulationFilter);
			NewShape->SetCollisionTraceType(Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseComplexAsSimple);
			NewShape->SetMaterials(MeshRef->UsedChaosMaterials);

			Geometries.Emplace(MoveTemp(ScaledTrimesh));
			ShapeArray.Emplace(MoveTemp(NewShape));
		}

#if WITH_EDITOR
		if(!World->IsGameWorld())
		{
			TUniquePtr<Chaos::TImplicitObjectScaled<Chaos::FTriangleMeshImplicitObject>> ScaledTrimeshEd = MakeUnique<Chaos::TImplicitObjectScaled<Chaos::FTriangleMeshImplicitObject>>(MakeSerializable(MeshRef->EditorTrimesh), SharedPtrForRefCount, Scale);
			TUniquePtr<Chaos::FPerShapeData> NewEdShape = Chaos::FPerShapeData::CreatePerShapeData(ShapeArray.Num(), MakeSerializable(ScaledTrimeshEd));

			NewEdShape->SetQueryData(QueryFilterEd);
			NewEdShape->SetSimEnabled(false);
			NewEdShape->SetCollisionTraceType(Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseComplexAsSimple);
			NewEdShape->SetMaterial(GEngine->DefaultPhysMaterial->GetPhysicsMaterial());

			Geometries.Emplace(MoveTemp(ScaledTrimeshEd));
			ShapeArray.Emplace(MoveTemp(NewEdShape));
		}
#endif // WITH_EDITOR

		if(Geometries.Num() == 1)
		{
			ActorHandle->GetGameThreadAPI().SetGeometry(MoveTemp(Geometries[0]));
		}
		else
		{
			ActorHandle->GetGameThreadAPI().SetGeometry(MakeUnique<Chaos::FImplicitObjectUnion>(MoveTemp(Geometries)));
		}

		for(TUniquePtr<Chaos::FPerShapeData>& Shape : ShapeArray)
		{
			Chaos::FRigidTransform3 WorldTransform = Chaos::FRigidTransform3(ActorHandle->GetGameThreadAPI().X(), ActorHandle->GetGameThreadAPI().R());
			Shape->UpdateShapeBounds(WorldTransform);
		}

		ActorHandle->GetGameThreadAPI().SetShapesArray(MoveTemp(ShapeArray));

		TargetInstance->PhysicsUserData = FPhysicsUserData(TargetInstance);
		TargetInstance->OwnerComponent = Component;
		TargetInstance->ActorHandle = ActorHandle;

		ActorHandle->GetGameThreadAPI().SetUserData(&TargetInstance->PhysicsUserData);
	}

	void AddToScene()
	{
		check(PhysScene);

		TArray<FPhysicsActorHandle> Actors;
		Actors.Add(ActorHandle);

		FPhysicsCommand::ExecuteWrite(PhysScene, [&]()
		{
			PhysScene->AddActorsToScene_AssumesLocked(Actors, true);
		});
		PhysScene->AddToComponentMaps(Component, ActorHandle);

		if(TargetInstance->bNotifyRigidBodyCollision)
		{
			PhysScene->RegisterForCollisionEvents(Component);
		}
	}

private:
	FTransform ComponentToWorld;
	FVector ComponentScale;
	float CollisionScale;
	TRefCountPtr<ULandscapeMeshCollisionComponent::FTriMeshGeometryRef> MeshRef;
	FPhysScene* PhysScene;
	FCollisionFilterData QueryFilter;
	FCollisionFilterData SimulationFilter;
	FCollisionFilterData QueryFilterEd;
	UWorld* World;
	UPrimitiveComponent* Component;
	FBodyInstance* TargetInstance;

	FPhysicsActorHandle ActorHandle;
};


void ULandscapeMeshCollisionComponent::OnCreatePhysicsState()
{
	USceneComponent::OnCreatePhysicsState(); // route OnCreatePhysicsState, skip PrimitiveComponent implementation

	if (!BodyInstance.IsValidBodyInstance())
	{
		// This will do nothing, because we create trimesh at component PostLoad event, unless we destroyed it explicitly
		CreateCollisionObject();

		if (IsValidRef(MeshRef))
		{
			FMeshCollisionInitHelper Initializer(MeshRef, GetWorld(), this, &BodyInstance);

			// Make transform for this landscape component PxActor
			FTransform LandscapeComponentTransform = GetComponentToWorld();
			FMatrix LandscapeComponentMatrix = LandscapeComponentTransform.ToMatrixWithScale();

			FVector LandscapeScale = LandscapeComponentMatrix.ExtractScaling();

			Initializer.SetComponentToWorld(LandscapeComponentTransform);
			Initializer.SetComponentScale3D(LandscapeScale);
			Initializer.SetCollisionScale(CollisionScale);

			if(Initializer.IsGeometryValid())
			{
				// Setup filtering
				FCollisionFilterData QueryFilterData, SimFilterData;
				CreateShapeFilterData(GetCollisionObjectType(), FMaskFilter(0), GetOwner()->GetUniqueID(), GetCollisionResponseToChannels(), GetUniqueID(), 0, QueryFilterData, SimFilterData, false, false, true);
				QueryFilterData.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);
				SimFilterData.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);

				Initializer.SetFilters(QueryFilterData, SimFilterData);

#if WITH_EDITOR
				FCollisionResponseContainer EdResponse;
				EdResponse.SetAllChannels(ECollisionResponse::ECR_Ignore);
				EdResponse.SetResponse(ECollisionChannel::ECC_Visibility, ECR_Block);
						FCollisionFilterData QueryFilterDataEd, SimFilterDataEd;
				CreateShapeFilterData(ECollisionChannel::ECC_Visibility, FMaskFilter(0), GetOwner()->GetUniqueID(), EdResponse, GetUniqueID(), 0, QueryFilterDataEd, SimFilterDataEd, true, false, true);
						QueryFilterDataEd.Word3 |= (EPDF_SimpleCollision | EPDF_ComplexCollision);

				Initializer.SetEditorFilter(QueryFilterDataEd);
#endif // WITH_EDITOR

				Initializer.CreateActors();
				Initializer.AddToScene();
			}
			else
			{
				UE_LOG(LogLandscape, Warning, TEXT("ULandscapeMeshCollisionComponent::OnCreatePhysicsState(): TriMesh invalid"));
			}
		}
	}
}

void ULandscapeMeshCollisionComponent::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	if (!bWorldShift || !FPhysScene::SupportsOriginShifting())
	{
		RecreatePhysicsState();
	}
}

void ULandscapeMeshCollisionComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->CollisionComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

#if WITH_EDITOR
uint32 ULandscapeHeightfieldCollisionComponent::ComputeCollisionHash() const
{
	uint32 Hash = 0;
		
	Hash = HashCombine(GetTypeHash(SimpleCollisionSizeQuads), Hash);
	Hash = HashCombine(GetTypeHash(CollisionSizeQuads), Hash);
	Hash = HashCombine(GetTypeHash(CollisionScale), Hash);

	FTransform ComponentTransform = GetComponentToWorld();
	Hash = FCrc::MemCrc32(&ComponentTransform, sizeof(ComponentTransform));

	const void* HeightBuffer = CollisionHeightData.LockReadOnly();
	Hash = FCrc::MemCrc32(HeightBuffer, CollisionHeightData.GetBulkDataSize(), Hash);
	CollisionHeightData.Unlock();

	const void* DominantBuffer = DominantLayerData.LockReadOnly();
	Hash = FCrc::MemCrc32(DominantBuffer, DominantLayerData.GetBulkDataSize(), Hash);
	DominantLayerData.Unlock();

	const void* PhysicalMaterialBuffer = PhysicalMaterialRenderData.LockReadOnly();
	Hash = FCrc::MemCrc32(PhysicalMaterialBuffer, PhysicalMaterialRenderData.GetBulkDataSize(), Hash);
	PhysicalMaterialRenderData.Unlock();

	return Hash;
}

void ULandscapeHeightfieldCollisionComponent::UpdateHeightfieldRegion(int32 ComponentX1, int32 ComponentY1, int32 ComponentX2, int32 ComponentY2)
{
	if (IsValidRef(HeightfieldRef))
	{
		// If we're currently sharing this data with a PIE session, we need to make a new heightfield.
		if (HeightfieldRef->GetRefCount() > 1)
		{
			RecreateCollision();
			return;
		}

		if(!BodyInstance.ActorHandle)
		{
			return;
		}

		// We don't lock the async scene as we only set the geometry in the sync scene's RigidActor.
		// This function is used only during painting for line traces by the painting tools.
		FPhysicsActorHandle PhysActorHandle = BodyInstance.GetPhysicsActorHandle();

		FPhysicsCommand::ExecuteWrite(PhysActorHandle, [&](const FPhysicsActorHandle& Actor)
		{
			int32 CollisionSizeVerts = CollisionSizeQuads + 1;
			int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
	
			bool bIsMirrored = GetComponentToWorld().GetDeterminant() < 0.f;
	
			uint16* Heights = (uint16*)CollisionHeightData.Lock(LOCK_READ_ONLY);
			check(CollisionHeightData.GetElementCount() == (FMath::Square(CollisionSizeVerts) + FMath::Square(SimpleCollisionSizeVerts)));
	
			int32 HeightfieldY1 = ComponentY1;
			int32 HeightfieldX1 = (bIsMirrored ? ComponentX1 : (CollisionSizeVerts - ComponentX2 - 1));
			int32 DstVertsX = ComponentX2 - ComponentX1 + 1;
			int32 DstVertsY = ComponentY2 - ComponentY1 + 1;
			TArray<uint16> Samples;
			Samples.AddZeroed(DstVertsX*DstVertsY);

			for(int32 RowIndex = 0; RowIndex < DstVertsY; RowIndex++)
			{
				for(int32 ColIndex = 0; ColIndex < DstVertsX; ColIndex++)
				{
					int32 SrcX = bIsMirrored ? (ColIndex + ComponentX1) : (ComponentX2 - ColIndex);
					int32 SrcY = RowIndex + ComponentY1;
					int32 SrcSampleIndex = (SrcY * CollisionSizeVerts) + SrcX;
					check(SrcSampleIndex < FMath::Square(CollisionSizeVerts));
					int32 DstSampleIndex = (RowIndex * DstVertsX) + ColIndex;

					Samples[DstSampleIndex] = Heights[SrcSampleIndex];
				}
			}

			CollisionHeightData.Unlock();

			HeightfieldRef->EditorHeightfield->EditHeights(Samples, HeightfieldY1, HeightfieldX1, DstVertsY, DstVertsX);

			// Rebuild geometry to update local bounds, and update in acceleration structure.
			const Chaos::FImplicitObjectUnion& Union = PhysActorHandle->GetGameThreadAPI().Geometry()->GetObjectChecked<Chaos::FImplicitObjectUnion>();
			TArray<TUniquePtr<Chaos::FImplicitObject>> NewGeometry;
			for (const TUniquePtr<Chaos::FImplicitObject>& Object : Union.GetObjects())
			{
				const Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>& TransformedHeightField = Object->GetObjectChecked<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>();
				NewGeometry.Emplace(MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>>(TransformedHeightField.Object(), TransformedHeightField.GetTransform()));
			}
			PhysActorHandle->GetGameThreadAPI().SetGeometry(MakeUnique<Chaos::FImplicitObjectUnion>(MoveTemp(NewGeometry)));

			FPhysScene* PhysScene = GetWorld()->GetPhysicsScene();
			PhysScene->UpdateActorInAccelerationStructure(PhysActorHandle);
		});
	}
}
#endif// WITH_EDITOR

void ULandscapeHeightfieldCollisionComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	if (Proxy)
	{
		Proxy->CollisionComponents.Remove(this);
	}

	Super::DestroyComponent(bPromoteChildren);
}

FBoxSphereBounds ULandscapeHeightfieldCollisionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return CachedLocalBox.TransformBy(LocalToWorld);
}

void ULandscapeHeightfieldCollisionComponent::BeginDestroy()
{
	HeightfieldRef = NULL;
	HeightfieldGuid = FGuid();
	Super::BeginDestroy();
}

void ULandscapeMeshCollisionComponent::BeginDestroy()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		MeshRef = NULL;
		MeshGuid = FGuid();
	}

	Super::BeginDestroy();
}

bool ULandscapeHeightfieldCollisionComponent::RecreateCollision()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
#if WITH_EDITOR
		uint32 NewHash = ComputeCollisionHash();
		if (bPhysicsStateCreated && NewHash == CollisionHash && CollisionHash != 0 && bEnableCollisionHashOptim)
		{
			return false;
		}
		CollisionHash = NewHash;
#endif // WITH_EDITOR
		TRefCountPtr<FHeightfieldGeometryRef> HeightfieldRefLifetimeExtender = HeightfieldRef; // Ensure heightfield data is alive until removed from physics world
		HeightfieldRef = nullptr; // Ensure data will be recreated
		HeightfieldGuid = FGuid();

		RecreatePhysicsState();

		MarkRenderStateDirty();
	}
	return true;
}

#if WITH_EDITORONLY_DATA
void ULandscapeHeightfieldCollisionComponent::SnapFoliageInstances()
{
	SnapFoliageInstances(FBox(FVector(-WORLD_MAX), FVector(WORLD_MAX)));
}

void ULandscapeHeightfieldCollisionComponent::SnapFoliageInstances(const FBox& InInstanceBox)
{
	UWorld* ComponentWorld = GetWorld();
	for (TActorIterator<AInstancedFoliageActor> It(ComponentWorld); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		const auto BaseId = IFA->InstanceBaseCache.GetInstanceBaseId(this);
		if (BaseId == FFoliageInstanceBaseCache::InvalidBaseId)
		{
			continue;
		}
			
		IFA->ForEachFoliageInfo([this, IFA, BaseId, &InInstanceBox](UFoliageType* Settings, FFoliageInfo& MeshInfo)
		{
			const auto* InstanceSet = MeshInfo.ComponentHash.Find(BaseId);
			if (InstanceSet)
			{
				float TraceExtentSize = Bounds.SphereRadius * 2.f + 10.f; // extend a little
				FVector TraceVector = GetOwner()->GetRootComponent()->GetComponentTransform().GetUnitAxis(EAxis::Z) * TraceExtentSize;

				TArray<int32> InstancesToRemove;
				TSet<UHierarchicalInstancedStaticMeshComponent*> AffectedFoliageComponents;
				
				bool bIsMeshInfoDirty = false;
				for (int32 InstanceIndex : *InstanceSet)
				{
					FFoliageInstance& Instance = MeshInfo.Instances[InstanceIndex];

					// Test location should remove any Z offset
					FVector TestLocation = FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER
						? Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, -Instance.ZOffset))
						: Instance.Location;

					if (InInstanceBox.IsInside(TestLocation))
					{
						FVector Start = TestLocation + TraceVector;
						FVector End = TestLocation - TraceVector;

						TArray<FHitResult> Results;
						UWorld* World = GetWorld();
						check(World);
						// Editor specific landscape heightfield uses ECC_Visibility collision channel
						World->LineTraceMultiByObjectType(Results, Start, End, FCollisionObjectQueryParams(ECollisionChannel::ECC_Visibility), FCollisionQueryParams(SCENE_QUERY_STAT(FoliageSnapToLandscape), true));

						bool bFoundHit = false;
						for (const FHitResult& Hit : Results)
						{
							if (Hit.Component == this)
							{
								bFoundHit = true;
								if ((TestLocation - Hit.Location).SizeSquared() > KINDA_SMALL_NUMBER)
								{
									IFA->Modify();

									bIsMeshInfoDirty = true;

									// Remove instance location from the hash. Do not need to update ComponentHash as we re-add below.
									MeshInfo.InstanceHash->RemoveInstance(Instance.Location, InstanceIndex);

									// Update the instance editor data
									Instance.Location = Hit.Location;

									if (Instance.Flags & FOLIAGE_AlignToNormal)
									{
										// Remove previous alignment and align to new normal.
										Instance.Rotation = Instance.PreAlignRotation;
										Instance.AlignToNormal(Hit.Normal, Settings->AlignMaxAngle);
									}

									// Reapply the Z offset in local space
									if (FMath::Abs(Instance.ZOffset) > KINDA_SMALL_NUMBER)
									{
										Instance.Location = Instance.GetInstanceWorldTransform().TransformPosition(FVector(0, 0, Instance.ZOffset));
									}

									// Todo: add do validation with other parameters such as max/min height etc.

									MeshInfo.SetInstanceWorldTransform(InstanceIndex, Instance.GetInstanceWorldTransform(), false);
									// Re-add the new instance location to the hash
									MeshInfo.InstanceHash->InsertInstance(Instance.Location, InstanceIndex);
								}
								break;
							}
						}

						if (!bFoundHit)
						{
							// Couldn't find new spot - remove instance
							InstancesToRemove.Add(InstanceIndex);
							bIsMeshInfoDirty = true;
						}

						if (bIsMeshInfoDirty && (MeshInfo.GetComponent() != nullptr))
						{
							AffectedFoliageComponents.Add(MeshInfo.GetComponent());
						}
					}
				}

				// Remove any unused instances
				MeshInfo.RemoveInstances(InstancesToRemove, true);

				for (UHierarchicalInstancedStaticMeshComponent* FoliageComp : AffectedFoliageComponents)
				{
					FoliageComp->InvalidateLightingCache();
				}
			}
			return true; // continue iterating
		});
	}
}
#endif // WITH_EDITORONLY_DATA

bool ULandscapeMeshCollisionComponent::RecreateCollision()
{
	TRefCountPtr<FTriMeshGeometryRef> TriMeshLifetimeExtender = nullptr; // Ensure heightfield data is alive until removed from physics world

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		TriMeshLifetimeExtender = MeshRef;
		MeshRef = nullptr; // Ensure data will be recreated
		MeshGuid = FGuid();
		CachedHeightFieldSamples.Heights.Empty();
		CachedHeightFieldSamples.Holes.Empty();
	}

	return Super::RecreateCollision();
}

void ULandscapeHeightfieldCollisionComponent::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	if (Ar.UEVer() >= VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
		// Cook data here so CookedPhysicalMaterials is always up to date
		if (Ar.IsCooking() && !HasAnyFlags(RF_ClassDefaultObject))
		{
			FName Format = Ar.CookingTarget()->GetPhysicsFormat(nullptr);
			CookCollisionData(Format, false, true, CookedCollisionData, CookedPhysicalMaterials);
		}
	}
#endif// WITH_EDITOR

	// this will also serialize CookedPhysicalMaterials
	Super::Serialize(Ar);

	if (Ar.UEVer() < VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA
		CollisionHeightData.Serialize(Ar, this);
		DominantLayerData.Serialize(Ar, this);
#endif//WITH_EDITORONLY_DATA
	}
	else
	{
		bool bCooked = Ar.IsCooking() || (FPlatformProperties::RequiresCookedData() && Ar.IsSaving());
		Ar << bCooked;

		if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
		{
			UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physics data was not cooked into %s."), *GetFullName());
		}

		if (bCooked)
		{
			CookedCollisionData.BulkSerialize(Ar);
		}
		else
		{
#if WITH_EDITORONLY_DATA
			// For PIE, we won't need the source height data if we already have a shared reference to the heightfield
			if (!(Ar.GetPortFlags() & PPF_DuplicateForPIE) || !HeightfieldGuid.IsValid() || GSharedMeshRefs.FindRef(HeightfieldGuid) == nullptr)
			{
				CollisionHeightData.Serialize(Ar, this);
				DominantLayerData.Serialize(Ar, this);

				if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::LandscapePhysicalMaterialRenderData)
				{
					PhysicalMaterialRenderData.Serialize(Ar, this);
				}
			}
#endif//WITH_EDITORONLY_DATA
		}
	}
}

void ULandscapeMeshCollisionComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UEVer() < VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA
		// conditional serialization in later versions
		CollisionXYOffsetData.Serialize(Ar, this);
#endif// WITH_EDITORONLY_DATA
	}

	// Physics cooking mesh data
	bool bCooked = false;
	if (Ar.UEVer() >= VER_UE4_ADD_COOKED_TO_LANDSCAPE)
	{
		bCooked = Ar.IsCooking();
		Ar << bCooked;
	}

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogPhysics, Fatal, TEXT("This platform requires cooked packages, and physics data was not cooked into %s."), *GetFullName());
	}

	if (bCooked)
	{
		// triangle mesh cooked data should be serialized in ULandscapeHeightfieldCollisionComponent
	}
	else if (Ar.UEVer() >= VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING)
	{
#if WITH_EDITORONLY_DATA		
		// we serialize raw collision data only with non-cooked content
		CollisionXYOffsetData.Serialize(Ar, this);
#endif// WITH_EDITORONLY_DATA
	}
}

#if WITH_EDITOR
void ULandscapeHeightfieldCollisionComponent::PostEditImport()
{
	Super::PostEditImport();

	if (!GetLandscapeProxy()->HasLayersContent())
	{
		// Reinitialize physics after paste
		if (CollisionSizeQuads > 0)
		{
			RecreateCollision();
		}
	}
}

void ULandscapeHeightfieldCollisionComponent::PostEditUndo()
{
	Super::PostEditUndo();

    // Landscape Layers are updates are delayed and done in  ALandscape::TickLayers
	if (!GetLandscapeProxy()->HasLayersContent())
	{
		// Reinitialize physics after undo
		if (CollisionSizeQuads > 0)
		{
			RecreateCollision();
		}

		FNavigationSystem::UpdateComponentData(*this);
	}
}
#endif // WITH_EDITOR

bool ULandscapeHeightfieldCollisionComponent::IsShown(const FEngineShowFlags& ShowFlags) const
{
	return ShowFlags.Landscape;
}

bool ULandscapeHeightfieldCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(IsInGameThread());
	if(IsValidRef(HeightfieldRef) && HeightfieldRef->Heightfield)
	{
		FTransform HFToW = GetComponentTransform();
		if(HeightfieldRef->HeightfieldSimple)
		{
			const float SimpleCollisionScale = CollisionScale * CollisionSizeQuads / SimpleCollisionSizeQuads;
			HFToW.MultiplyScale3D(FVector(SimpleCollisionScale, SimpleCollisionScale, LANDSCAPE_ZSCALE));
			GeomExport.ExportChaosHeightField(HeightfieldRef->HeightfieldSimple.Get(), HFToW);
		}
		else
		{
			HFToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, LANDSCAPE_ZSCALE));
			GeomExport.ExportChaosHeightField(HeightfieldRef->Heightfield.Get(), HFToW);
		}
	}

	return false;
}

void ULandscapeHeightfieldCollisionComponent::GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const
{
	// note that this function can get called off game thread
	if (CachedHeightFieldSamples.IsEmpty() == false)
	{
		FTransform HFToW = GetComponentTransform();
		HFToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, LANDSCAPE_ZSCALE));

		GeomExport.ExportChaosHeightFieldSlice(CachedHeightFieldSamples, HeightfieldRowsCount, HeightfieldColumnsCount, HFToW, SliceBox);
	}
}

ENavDataGatheringMode ULandscapeHeightfieldCollisionComponent::GetGeometryGatheringMode() const
{ 
	ALandscapeProxy* Proxy = GetLandscapeProxy();
	return Proxy ? Proxy->NavigationGeometryGatheringMode : ENavDataGatheringMode::Default;
}

void ULandscapeHeightfieldCollisionComponent::PrepareGeometryExportSync()
{
	if(IsValidRef(HeightfieldRef) && HeightfieldRef->Heightfield.Get() && CachedHeightFieldSamples.IsEmpty())
	{
		const UWorld* World = GetWorld();

		if(World != nullptr)
		{
			HeightfieldRowsCount = HeightfieldRef->Heightfield->GetNumRows();
			HeightfieldColumnsCount = HeightfieldRef->Heightfield->GetNumCols();
			const int32 HeightsCount = HeightfieldRowsCount * HeightfieldColumnsCount;

			if(CachedHeightFieldSamples.Heights.Num() != HeightsCount)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_NavMesh_ExportChaosHeightField_saveCells);

				CachedHeightFieldSamples.Heights.SetNumUninitialized(HeightsCount);
				for(int32 Index = 0; Index < HeightsCount; ++Index)
				{
					CachedHeightFieldSamples.Heights[Index] = HeightfieldRef->Heightfield->GetHeight(Index);
				}

				const int32 HolesCount = (HeightfieldRowsCount-1) * (HeightfieldColumnsCount-1);
				CachedHeightFieldSamples.Holes.SetNumUninitialized(HolesCount);
				for(int32 Index = 0; Index < HolesCount; ++Index)
				{
					CachedHeightFieldSamples.Holes[Index] = HeightfieldRef->Heightfield->IsHole(Index);
				}
			}
		}
	}
}

bool ULandscapeMeshCollisionComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	check(IsInGameThread());

	if (IsValidRef(MeshRef))
	{
		FTransform MeshToW = GetComponentTransform();
		MeshToW.MultiplyScale3D(FVector(CollisionScale, CollisionScale, 1.f));

		if (MeshRef->Trimesh != nullptr)
		{
			GeomExport.ExportChaosTriMesh(MeshRef->Trimesh.Get(), MeshToW);
		}
	}

	return false;
}

void ULandscapeHeightfieldCollisionComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// PostLoad of the landscape can decide to recreate collision, in which case this components checks are irrelevant
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsValid(this))
	{
		bShouldSaveCookedDataToDDC[0] = true;
		bShouldSaveCookedDataToDDC[1] = true;

		ALandscapeProxy* LandscapeProxy = GetLandscapeProxy();
		if (ensure(LandscapeProxy) && GIsEditor)
		{
			// This is to ensure that component relative location is exact section base offset value
			FVector LocalRelativeLocation = GetRelativeLocation();
			float CheckRelativeLocationX = float(SectionBaseX - LandscapeProxy->LandscapeSectionOffset.X);
			float CheckRelativeLocationY = float(SectionBaseY - LandscapeProxy->LandscapeSectionOffset.Y);
			if (!FMath::IsNearlyEqual(CheckRelativeLocationX, LocalRelativeLocation.X, UE_DOUBLE_KINDA_SMALL_NUMBER) ||
				!FMath::IsNearlyEqual(CheckRelativeLocationY, LocalRelativeLocation.Y, UE_DOUBLE_KINDA_SMALL_NUMBER))
			{
				UE_LOG(LogLandscape, Warning, TEXT("ULandscapeHeightfieldCollisionComponent RelativeLocation disagrees with its section base, attempted automated fix: '%s', %f,%f vs %f,%f."),
					*GetFullName(), LocalRelativeLocation.X, LocalRelativeLocation.Y, CheckRelativeLocationX, CheckRelativeLocationY);
				LocalRelativeLocation.X = CheckRelativeLocationX;
				LocalRelativeLocation.Y = CheckRelativeLocationY;
				SetRelativeLocation_Direct(LocalRelativeLocation);
			}
		}

		UWorld* World = GetWorld();
		if (World && World->IsGameWorld())
		{
			SpeculativelyLoadAsyncDDCCollsionData();
		}
	}
#endif // WITH_EDITOR
}

void ULandscapeHeightfieldCollisionComponent::PreSave(const class ITargetPlatform* TargetPlatform)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::PreSave(TargetPlatform);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void ULandscapeHeightfieldCollisionComponent::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	if (!ObjectSaveContext.IsProceduralSave())
	{
#if WITH_EDITOR
		ALandscapeProxy* Proxy = GetLandscapeProxy();
		if (Proxy && Proxy->bBakeMaterialPositionOffsetIntoCollision)
		{
			if (!RenderComponent->GrassData->HasData() || RenderComponent->IsGrassMapOutdated())
			{
				if (!RenderComponent->CanRenderGrassMap())
				{
					RenderComponent->GetMaterialInstance(0, false)->GetMaterialResource(GetWorld()->FeatureLevel)->FinishCompilation();
				}
				RenderComponent->RenderGrassMap();
			}
		}
#endif// WITH_EDITOR
	}
}

#if WITH_EDITOR
void ULandscapeInfo::UpdateAllAddCollisions()
{
	XYtoAddCollisionMap.Reset();

	// Don't recreate add collisions if the landscape is not registered. This can happen during Undo.
	if (GetLandscapeProxy())
	{
		for (auto It = XYtoComponentMap.CreateIterator(); It; ++It)
		{
			const ULandscapeComponent* const Component = It.Value();
			if (ensure(Component))
			{
				const FIntPoint ComponentBase = Component->GetSectionBase() / ComponentSizeQuads;

				const FIntPoint NeighborsKeys[8] =
				{
					ComponentBase + FIntPoint(-1, -1),
					ComponentBase + FIntPoint(+0, -1),
					ComponentBase + FIntPoint(+1, -1),
					ComponentBase + FIntPoint(-1, +0),
					ComponentBase + FIntPoint(+1, +0),
					ComponentBase + FIntPoint(-1, +1),
					ComponentBase + FIntPoint(+0, +1),
					ComponentBase + FIntPoint(+1, +1)
				};

				// Search for Neighbors...
				for (int32 i = 0; i < 8; ++i)
				{
					ULandscapeComponent* NeighborComponent = XYtoComponentMap.FindRef(NeighborsKeys[i]);

					// UpdateAddCollision() treats a null CollisionComponent as an empty hole
					if (!NeighborComponent || !NeighborComponent->CollisionComponent.IsValid())
					{
						UpdateAddCollision(NeighborsKeys[i]);
					}
				}
			}
		}
	}
}

void ULandscapeInfo::UpdateAddCollision(FIntPoint LandscapeKey)
{
	FLandscapeAddCollision& AddCollision = XYtoAddCollisionMap.FindOrAdd(LandscapeKey);

	// 8 Neighbors...
	// 0 1 2
	// 3   4
	// 5 6 7
	FIntPoint NeighborsKeys[8] =
	{
		LandscapeKey + FIntPoint(-1, -1),
		LandscapeKey + FIntPoint(+0, -1),
		LandscapeKey + FIntPoint(+1, -1),
		LandscapeKey + FIntPoint(-1, +0),
		LandscapeKey + FIntPoint(+1, +0),
		LandscapeKey + FIntPoint(-1, +1),
		LandscapeKey + FIntPoint(+0, +1),
		LandscapeKey + FIntPoint(+1, +1)
	};

	// Todo: Use data accessor not collision

	ULandscapeHeightfieldCollisionComponent* NeighborCollisions[8];
	// Search for Neighbors...
	for (int32 i = 0; i < 8; ++i)
	{
		ULandscapeComponent* Comp = XYtoComponentMap.FindRef(NeighborsKeys[i]);
		if (Comp)
		{
			ULandscapeHeightfieldCollisionComponent* NeighborCollision = Comp->CollisionComponent.Get();
			// Skip cooked because CollisionHeightData not saved during cook
			if (NeighborCollision && !NeighborCollision->GetOutermost()->bIsCookedForEditor)
			{
				NeighborCollisions[i] = NeighborCollision;
			}
			else
			{
				NeighborCollisions[i] = NULL;
			}
		}
		else
		{
			NeighborCollisions[i] = NULL;
		}
	}

	uint8 CornerSet = 0;
	uint16 HeightCorner[4];

	// Corner Cases...
	if (NeighborCollisions[0])
	{
		uint16* Heights = (uint16*)NeighborCollisions[0]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[0]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1;
		NeighborCollisions[0]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[2])
	{
		uint16* Heights = (uint16*)NeighborCollisions[2]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[2]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 1;
		NeighborCollisions[2]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[5])
	{
		uint16* Heights = (uint16*)NeighborCollisions[5]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[5]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1 << 2;
		NeighborCollisions[5]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[7])
	{
		uint16* Heights = (uint16*)NeighborCollisions[7]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[7]->CollisionSizeQuads + 1;
		HeightCorner[3] = Heights[0];
		CornerSet |= 1 << 3;
		NeighborCollisions[7]->CollisionHeightData.Unlock();
	}

	// Other cases...
	if (NeighborCollisions[1])
	{
		uint16* Heights = (uint16*)NeighborCollisions[1]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[1]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1;
		HeightCorner[1] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 1;
		NeighborCollisions[1]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[3])
	{
		uint16* Heights = (uint16*)NeighborCollisions[3]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[3]->CollisionSizeQuads + 1;
		HeightCorner[0] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1;
		HeightCorner[2] = Heights[CollisionSizeVerts - 1 + (CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 2;
		NeighborCollisions[3]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[4])
	{
		uint16* Heights = (uint16*)NeighborCollisions[4]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[4]->CollisionSizeQuads + 1;
		HeightCorner[1] = Heights[0];
		CornerSet |= 1 << 1;
		HeightCorner[3] = Heights[(CollisionSizeVerts - 1)*CollisionSizeVerts];
		CornerSet |= 1 << 3;
		NeighborCollisions[4]->CollisionHeightData.Unlock();
	}
	if (NeighborCollisions[6])
	{
		uint16* Heights = (uint16*)NeighborCollisions[6]->CollisionHeightData.Lock(LOCK_READ_ONLY);
		int32 CollisionSizeVerts = NeighborCollisions[6]->CollisionSizeQuads + 1;
		HeightCorner[2] = Heights[0];
		CornerSet |= 1 << 2;
		HeightCorner[3] = Heights[(CollisionSizeVerts - 1)];
		CornerSet |= 1 << 3;
		NeighborCollisions[6]->CollisionHeightData.Unlock();
	}

	// Fill unset values
	// First iteration only for valid values distance 1 propagation
	// Second iteration fills left ones...
	FillCornerValues(CornerSet, HeightCorner);
	//check(CornerSet == 15);

	FIntPoint SectionBase = LandscapeKey * ComponentSizeQuads;

	// Transform Height to Vectors...
	FTransform LtoW = GetLandscapeProxy()->LandscapeActorToWorld();
	AddCollision.Corners[0] = LtoW.TransformPosition(FVector(SectionBase.X                     , SectionBase.Y                     , LandscapeDataAccess::GetLocalHeight(HeightCorner[0])));
	AddCollision.Corners[1] = LtoW.TransformPosition(FVector(SectionBase.X + ComponentSizeQuads, SectionBase.Y                     , LandscapeDataAccess::GetLocalHeight(HeightCorner[1])));
	AddCollision.Corners[2] = LtoW.TransformPosition(FVector(SectionBase.X                     , SectionBase.Y + ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[2])));
	AddCollision.Corners[3] = LtoW.TransformPosition(FVector(SectionBase.X + ComponentSizeQuads, SectionBase.Y + ComponentSizeQuads, LandscapeDataAccess::GetLocalHeight(HeightCorner[3])));
}

void ULandscapeHeightfieldCollisionComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	int32 CollisionSizeVerts = CollisionSizeQuads + 1;
	int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
	int32 NumHeights = FMath::Square(CollisionSizeVerts) + FMath::Square(SimpleCollisionSizeVerts);
	check(CollisionHeightData.GetElementCount() == NumHeights);

	uint16* Heights = (uint16*)CollisionHeightData.Lock(LOCK_READ_ONLY);

	Out.Logf(TEXT("%sCustomProperties CollisionHeightData "), FCString::Spc(Indent));
	for (int32 i = 0; i < NumHeights; i++)
	{
		Out.Logf(TEXT("%d "), Heights[i]);
	}

	CollisionHeightData.Unlock();
	Out.Logf(TEXT("\r\n"));

	int32 NumDominantLayerSamples = DominantLayerData.GetElementCount();
	check(NumDominantLayerSamples == 0 || NumDominantLayerSamples == NumHeights);

	if (NumDominantLayerSamples > 0)
	{
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Lock(LOCK_READ_ONLY);

		Out.Logf(TEXT("%sCustomProperties DominantLayerData "), FCString::Spc(Indent));
		for (int32 i = 0; i < NumDominantLayerSamples; i++)
		{
			Out.Logf(TEXT("%02x"), DominantLayerSamples[i]);
		}

		DominantLayerData.Unlock();
		Out.Logf(TEXT("\r\n"));
	}
}

void ULandscapeHeightfieldCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("CollisionHeightData")))
	{
		int32 CollisionSizeVerts = CollisionSizeQuads + 1;
		int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
		int32 NumHeights = FMath::Square(CollisionSizeVerts) + FMath::Square(SimpleCollisionSizeVerts);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		uint16* Heights = (uint16*)CollisionHeightData.Realloc(NumHeights);
		FMemory::Memzero(Heights, sizeof(uint16)*NumHeights);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumHeights)
			{
				Heights[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionHeightData.Unlock();

		if (i != NumHeights)
		{
			Warn->Log(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("DominantLayerData")))
	{
		int32 NumDominantLayerSamples = FMath::Square(CollisionSizeQuads + 1);

		DominantLayerData.Lock(LOCK_READ_WRITE);
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Realloc(NumDominantLayerSamples);
		FMemory::Memzero(DominantLayerSamples, NumDominantLayerSamples);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (SourceText[0] && SourceText[1])
		{
			if (i < NumDominantLayerSamples)
			{
				DominantLayerSamples[i++] = FParse::HexDigit(SourceText[0]) * 16 + FParse::HexDigit(SourceText[1]);
			}
			SourceText += 2;
		}

		DominantLayerData.Unlock();

		if (i != NumDominantLayerSamples)
		{
			Warn->Log(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
}

void ULandscapeMeshCollisionComponent::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	Super::ExportCustomProperties(Out, Indent);

	uint16* XYOffsets = (uint16*)CollisionXYOffsetData.Lock(LOCK_READ_ONLY);
	int32 NumOffsets = FMath::Square(CollisionSizeQuads + 1) * 2;
	check(CollisionXYOffsetData.GetElementCount() == NumOffsets);

	Out.Logf(TEXT("%sCustomProperties CollisionXYOffsetData "), FCString::Spc(Indent));
	for (int32 i = 0; i < NumOffsets; i++)
	{
		Out.Logf(TEXT("%d "), XYOffsets[i]);
	}

	CollisionXYOffsetData.Unlock();
	Out.Logf(TEXT("\r\n"));
}

void ULandscapeMeshCollisionComponent::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("CollisionHeightData")))
	{
		int32 CollisionSizeVerts = CollisionSizeQuads + 1;
		int32 SimpleCollisionSizeVerts = SimpleCollisionSizeQuads > 0 ? SimpleCollisionSizeQuads + 1 : 0;
		int32 NumHeights = FMath::Square(CollisionSizeVerts) + FMath::Square(SimpleCollisionSizeVerts);

		CollisionHeightData.Lock(LOCK_READ_WRITE);
		uint16* Heights = (uint16*)CollisionHeightData.Realloc(NumHeights);
		FMemory::Memzero(Heights, sizeof(uint16)*NumHeights);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumHeights)
			{
				Heights[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionHeightData.Unlock();

		if (i != NumHeights)
		{
			Warn->Log(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("DominantLayerData")))
	{
		int32 NumDominantLayerSamples = FMath::Square(CollisionSizeQuads + 1);

		DominantLayerData.Lock(LOCK_READ_WRITE);
		uint8* DominantLayerSamples = (uint8*)DominantLayerData.Realloc(NumDominantLayerSamples);
		FMemory::Memzero(DominantLayerSamples, NumDominantLayerSamples);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (SourceText[0] && SourceText[1])
		{
			if (i < NumDominantLayerSamples)
			{
				DominantLayerSamples[i++] = FParse::HexDigit(SourceText[0]) * 16 + FParse::HexDigit(SourceText[1]);
			}
			SourceText += 2;
		}

		DominantLayerData.Unlock();

		if (i != NumDominantLayerSamples)
		{
			Warn->Log(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
	else if (FParse::Command(&SourceText, TEXT("CollisionXYOffsetData")))
	{
		int32 NumOffsets = FMath::Square(CollisionSizeQuads + 1) * 2;

		CollisionXYOffsetData.Lock(LOCK_READ_WRITE);
		uint16* Offsets = (uint16*)CollisionXYOffsetData.Realloc(NumOffsets);
		FMemory::Memzero(Offsets, sizeof(uint16)*NumOffsets);

		FParse::Next(&SourceText);
		int32 i = 0;
		while (FChar::IsDigit(*SourceText))
		{
			if (i < NumOffsets)
			{
				Offsets[i++] = FCString::Atoi(SourceText);
				while (FChar::IsDigit(*SourceText))
				{
					SourceText++;
				}
			}

			FParse::Next(&SourceText);
		}

		CollisionXYOffsetData.Unlock();

		if (i != NumOffsets)
		{
			Warn->Log(*NSLOCTEXT("Core", "SyntaxError", "Syntax Error").ToString());
		}
	}
}

#endif // WITH_EDITOR

ULandscapeInfo* ULandscapeHeightfieldCollisionComponent::GetLandscapeInfo() const
{
	return GetLandscapeProxy()->GetLandscapeInfo();
}

ALandscapeProxy* ULandscapeHeightfieldCollisionComponent::GetLandscapeProxy() const
{
	return CastChecked<ALandscapeProxy>(GetOuter());
}

FIntPoint ULandscapeHeightfieldCollisionComponent::GetSectionBase() const
{
	return FIntPoint(SectionBaseX, SectionBaseY);
}

void ULandscapeHeightfieldCollisionComponent::SetSectionBase(FIntPoint InSectionBase)
{
	SectionBaseX = InSectionBase.X;
	SectionBaseY = InSectionBase.Y;
}

ULandscapeHeightfieldCollisionComponent::ULandscapeHeightfieldCollisionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetGenerateOverlapEvents(false);
	CastShadow = false;
	bUseAsOccluder = true;
	bAllowCullDistanceVolume = false;
	Mobility = EComponentMobility::Static;
	bCanEverAffectNavigation = true;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	HeightfieldRowsCount = -1;
	HeightfieldColumnsCount = -1;

	// landscape collision components should be deterministically created and therefor are addressable over the network
	SetNetAddressable();
}

ULandscapeHeightfieldCollisionComponent::ULandscapeHeightfieldCollisionComponent(FVTableHelper& Helper)
	: Super(Helper)
{

}

ULandscapeHeightfieldCollisionComponent::~ULandscapeHeightfieldCollisionComponent() = default;

ULandscapeComponent* ULandscapeHeightfieldCollisionComponent::GetRenderComponent() const
{
	return RenderComponent.Get();
}

TOptional<float> ULandscapeHeightfieldCollisionComponent::GetHeight(float X, float Y, EHeightfieldSource HeightFieldSource)
{
	TOptional<float> Height;
	const float ZScale = GetComponentTransform().GetScale3D().Z * LANDSCAPE_ZSCALE;

	if (!IsValidRef(HeightfieldRef))
	{
		return Height;
	}
	
	Chaos::FHeightField* HeightField = nullptr;
	
	switch(HeightFieldSource)
	{
	case EHeightfieldSource::Simple:
		HeightField = HeightfieldRef->HeightfieldSimple.Get(); 
		break;
	case EHeightfieldSource::Complex:
		HeightField = HeightfieldRef->Heightfield.Get(); 
		break;
#if WITH_EDITORONLY_DATA		
	case EHeightfieldSource::Editor:
		HeightField = HeightfieldRef->EditorHeightfield.Get();
		break;
#endif 
	}
	
	if (HeightField)
	{
		Height = HeightField->GetHeightAt({ X, Y });
	}

	return Height;
}

struct FHeightFieldAccessor
{
	FHeightFieldAccessor(const ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef& InGeometryRef)
	: GeometryRef(InGeometryRef)
	, NumX(InGeometryRef.Heightfield.IsValid() ? InGeometryRef.Heightfield->GetNumCols() : 0)
	, NumY(InGeometryRef.Heightfield.IsValid() ? InGeometryRef.Heightfield->GetNumRows() : 0)
	{
	}

	float GetUnscaledHeight(int32 X, int32 Y) const
	{
		return GeometryRef.Heightfield->GetHeight(X, Y);
	}

	uint8 GetMaterialIndex(int32 X, int32 Y) const
	{
		return GeometryRef.Heightfield->GetMaterialIndex(X, Y);
	}

	const ULandscapeHeightfieldCollisionComponent::FHeightfieldGeometryRef& GeometryRef;
	const int32 NumX = 0;
	const int32 NumY = 0;
};

bool ULandscapeHeightfieldCollisionComponent::FillHeightTile(TArrayView<float> Heights, int32 Offset, int32 Stride) const
{
	if (!IsValidRef(HeightfieldRef))
	{
		return false;
	}

	FHeightFieldAccessor Accessor(*HeightfieldRef.GetReference());

	const int32 LastTiledIndex = Offset + FMath::Max(0, Accessor.NumX - 1) + Stride * FMath::Max(0, Accessor.NumY - 1);
	if (!Heights.IsValidIndex(LastTiledIndex))
	{
		return false;
	}

	const FTransform& WorldTransform = GetComponentToWorld();
	const float ZScale = WorldTransform.GetScale3D().Z * LANDSCAPE_ZSCALE;

	// Write all values to output array
	for (int32 y = 0; y < Accessor.NumY; ++y)
	{
		for (int32 x = 0; x < Accessor.NumX; ++x)
		{
			const float CurrHeight = Accessor.GetUnscaledHeight(x, y);
			const float WorldHeight = WorldTransform.TransformPositionNoScale(FVector(0, 0, CurrHeight * ZScale)).Z;

			// write output
			const int32 WriteIndex = Offset + y * Stride + x;
			Heights[WriteIndex] = WorldHeight;
		}
	}

	return true;
}

bool ULandscapeHeightfieldCollisionComponent::FillMaterialIndexTile(TArrayView<uint8> Materials, int32 Offset, int32 Stride) const
{
	if (!IsValidRef(HeightfieldRef))
	{
		return false;
	}

	FHeightFieldAccessor Accessor(*HeightfieldRef.GetReference());

	const int32 LastTiledIndex = Offset + FMath::Max(0, Accessor.NumX - 1) + Stride * FMath::Max(0, Accessor.NumY - 1);
	if (!Materials.IsValidIndex(LastTiledIndex))
	{
		return false;
	}

	// Write all values to output array
	for (int32 y = 0; y < Accessor.NumY; ++y)
	{
		for (int32 x = 0; x < Accessor.NumX; ++x)
		{
			// write output
			const int32 WriteIndex = Offset + y * Stride + x;
			Materials[WriteIndex] = Accessor.GetMaterialIndex(x, y);
		}
	}

	return true;
}

void ULandscapeHeightfieldCollisionComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(CookedCollisionData) + CookedCollisionData.GetAllocatedSize() + sizeof(HeightfieldRowsCount) + sizeof(HeightfieldColumnsCount));
	
	if (IsValidRef(HeightfieldRef))
	{
		HeightfieldRef->GetResourceSizeEx(CumulativeResourceSize);
	}

	CachedHeightFieldSamples.GetResourceSizeEx(CumulativeResourceSize);
}

void ULandscapeMeshCollisionComponent::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (IsValidRef(MeshRef))
	{
		MeshRef->GetResourceSizeEx(CumulativeResourceSize);
	}
}


TOptional<float> ALandscapeProxy::GetHeightAtLocation(FVector Location, EHeightfieldSource HeightFieldSource) const
{
	TOptional<float> Height;
	if (ULandscapeInfo* Info = GetLandscapeInfo())
	{
		const FVector ActorSpaceLocation = LandscapeActorToWorld().InverseTransformPosition(Location);
		const FIntPoint Key = FIntPoint(FMath::FloorToInt(ActorSpaceLocation.X / ComponentSizeQuads), FMath::FloorToInt(ActorSpaceLocation.Y / ComponentSizeQuads));
		ULandscapeHeightfieldCollisionComponent* Component = Info->XYtoCollisionComponentMap.FindRef(Key);
		if (Component)
		{
			const FVector ComponentSpaceLocation = Component->GetComponentToWorld().InverseTransformPosition(Location);
			const TOptional<float> LocalHeight = Component->GetHeight(ComponentSpaceLocation.X, ComponentSpaceLocation.Y, HeightFieldSource);
			if (LocalHeight.IsSet())
			{
				Height = Component->GetComponentToWorld().TransformPositionNoScale(FVector(0, 0, LocalHeight.GetValue())).Z;
			}
		}
	}
	return Height;
}

void ALandscapeProxy::GetHeightValues(int32& SizeX, int32& SizeY, TArray<float> &ArrayValues) const
{			
	SizeX = 0;
	SizeY = 0;
	ArrayValues.SetNum(0);
	
	// Exit if we have no landscape data
	if (LandscapeComponents.Num() == 0 || CollisionComponents.Num() == 0)
	{
		return;
	}

	// find index coordinate range for landscape
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;

	for (ULandscapeComponent* LandscapeComponent : LandscapeComponents)
	{
		// expecting a valid pointer to a landscape component
		if (!LandscapeComponent)
		{
			return;
		}

		// #todo(dmp): should we be using ULandscapeHeightfieldCollisionComponent.CollisionSizeQuads (or HeightFieldData->GetNumCols)
		MinX = FMath::Min(LandscapeComponent->SectionBaseX, MinX);
		MinY = FMath::Min(LandscapeComponent->SectionBaseY, MinY);
		MaxX = FMath::Max(LandscapeComponent->SectionBaseX + LandscapeComponent->ComponentSizeQuads, MaxX);
		MaxY = FMath::Max(LandscapeComponent->SectionBaseY + LandscapeComponent->ComponentSizeQuads, MaxY);		
	}

	if (MinX == MAX_int32)
	{
		return;
	}			
		
	SizeX = (MaxX - MinX + 1);
	SizeY = (MaxY - MinY + 1);
	ArrayValues.SetNumUninitialized(SizeX * SizeY);
	
	for (ULandscapeHeightfieldCollisionComponent *CollisionComponent : CollisionComponents)
	{
		// Make sure we have a valid collision component and a heightfield
		if (!CollisionComponent || !IsValidRef(CollisionComponent->HeightfieldRef))
		{
			SizeX = 0;
			SizeY = 0;
			ArrayValues.SetNum(0);
			return;
		}

		TUniquePtr<Chaos::FHeightField> &HeightFieldData = CollisionComponent->HeightfieldRef->Heightfield;

		// If we are expecting height data, but it isn't there, clear the return array, and exit
		if (!HeightFieldData.IsValid())
		{
			SizeX = 0;
			SizeY = 0;
			ArrayValues.SetNum(0);
			return;
		}

		const int32 BaseX = CollisionComponent->SectionBaseX - MinX;
		const int32 BaseY = CollisionComponent->SectionBaseY - MinY;

		const int32 NumX = HeightFieldData->GetNumCols();
		const int32 NumY = HeightFieldData->GetNumRows();

		const FTransform& ComponentToWorld = CollisionComponent->GetComponentToWorld();
		const float ZScale = ComponentToWorld.GetScale3D().Z * LANDSCAPE_ZSCALE;

		// Write all values to output array
		for (int32 x = 0; x < NumX; ++x)
		{
			for (int32 y = 0; y < NumY; ++y)
			{
				const float CurrHeight = HeightFieldData->GetHeight(x, y) * ZScale;				
				const float WorldHeight = ComponentToWorld.TransformPositionNoScale(FVector(0, 0, CurrHeight)).Z;
				
				// write output
				const int32 WriteX = BaseX + x;
				const int32 WriteY = BaseY + y;
				const int32 Idx = WriteY * SizeX + WriteX;
				ArrayValues[Idx] = WorldHeight;
			}
		}
	}
}
