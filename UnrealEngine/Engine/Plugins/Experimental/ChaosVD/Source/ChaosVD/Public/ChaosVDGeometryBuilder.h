// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDConvexMeshGenerator.h"
#include "ChaosVDHeightfieldMeshGenerator.h"
#include "ChaosVDTriMeshGenerator.h"
#include "Chaos/HeightField.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/ImplicitObjectType.h"
#include "Containers/Ticker.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Generators/CapsuleGenerator.h"
#include "Generators/MinimalBoxMeshGenerator.h"
#include "Generators/SphereGenerator.h"
#include "UDynamicMesh.h"
#include "Tasks/Task.h"
#include "UObject/GCObject.h"
#include "UObject/UObjectGlobals.h"

class AActor;
class UDynamicMesh;
class UDynamicMeshComponent;

namespace UE
{
	namespace Geometry
	{
		class FMeshShapeGenerator;
		class FDynamicMesh3;
	}
}

/*
 * Generates Dynamic mesh components and dynamic meshes based on Chaos implicit object data
 */
class FChaosVDGeometryBuilder : public FGCObject, public TSharedFromThis<FChaosVDGeometryBuilder>
{
public:

	FChaosVDGeometryBuilder()
	{
		GameThreadTickDelegate = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FChaosVDGeometryBuilder::GameThreadTick));
	}

	virtual ~FChaosVDGeometryBuilder() override
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GameThreadTickDelegate);

		for (const TPair<uint32, TObjectPtr<UStaticMesh>>& StaticMeshByKey : StaticMeshCacheMap)
		{
			if (StaticMeshByKey.Value)
			{
				StaticMeshByKey.Value->ClearFlags(RF_Standalone);
				StaticMeshByKey.Value->MarkAsGarbage();
			}
		}

		for (const TPair<uint32, TObjectPtr<UDynamicMesh>>& DynamicMeshByKey : DynamicMeshCacheMap)
		{
			if (DynamicMeshByKey.Value)
			{
				DynamicMeshByKey.Value->ClearFlags(RF_Standalone);
				DynamicMeshByKey.Value->MarkAsGarbage();
			}
		}
	}

	/** Creates Dynamic Mesh components for each object within the provided Implicit object
	 *	@param InImplicitObject : Implicit object to process
	 *	@param Owner Actor who will own the generated components
	 *	@param OutMeshComponents Array containing all the generated components
	 *	@param Index Index of the current component being processed. This is useful when this method is called recursively
	 *	@param Transform to apply to the generated components/geometry
	 */
	template<typename MeshType, typename ComponentType>
	void CreateMeshComponentsFromImplicit(const Chaos::FImplicitObject* InImplicitObject, AActor* Owner, TArray<TWeakObjectPtr<UMeshComponent>>& OutMeshComponents, Chaos::FRigidTransform3& Transform, const int32 Index = 0, const int32 DesiredLODCount = 0);
	
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FChaosVDGeometryBuilder");
	}

	/**
	 * Evaluates an Implicit objects and returns true if it contains an object of the specified type 
	 * @param InImplicitObject Object to evaluate
	 * @param ImplicitTypeToCheck Object type to compare against
	 * @return 
	 */
	static bool DoesImplicitContainType(const Chaos::FImplicitObject* InImplicitObject, const Chaos::EImplicitObjectType ImplicitTypeToCheck);

private:

	/**
	 * Evaluates the provided transform's scale, and returns true if the scale has a negative component
	 * @param InTransform Transform to evaluate
	 */
	bool HasNegativeScale(const Chaos::FRigidTransform3& InTransform) const;

	/** Creates a Dynamic Mesh for the provided Implicit object and generator, and then caches it to be reused later
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 * @param MeshGenerator Generator class with the data and rules to create the mesh
	 */
	UDynamicMesh* CreateAndCacheDynamicMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator);

	/** Returns an already mesh for the provided implicit object if exists, otherwise returns null
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 */
	template<typename MeshType>
	MeshType* GetCachedMeshForImplicit(const uint32 GeometryCacheKey);

	/** Creates a Dynamic Mesh for the provided Implicit object and generator, and then caches it to be reused later
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 * @param MeshGenerator Generator class with the data and rules to create the mesh
	 */
	UStaticMesh* CreateAndCacheStaticMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator, const int32 LODsToGenerateNum = 0);

	/**
	 * Creates an empty DynamicMeshComponent/Static Mesh Component or Instanced mesh component and adds it to the actor
	 * @param Owner Actor who will own the component
	 * @param Name Name of the component. It has to be unique within the components in the owner actor
	 * @param Transform (Optional) Transform to apply as Relative Transform in the component after its creating and attachment to the provided actor
	 * */
	template<typename ComponentType>
	ComponentType* CreateMeshComponent(AActor* Owner, const FString& Name, const Chaos::FRigidTransform3& Transform) const;

	/**
	 * Applies a mesh to a mesh component based on its type
	 * @param MeshComponent Mesh component to apply the mesh to
	 * @param GeometryKey Key to find the mesh to apply in the cache
	 */
	void ApplyMeshToComponentFromKey(TWeakObjectPtr<UMeshComponent> MeshComponent, const uint32 GeometryKey);

	/**
	 * Creates a Mesh from the provided Implicit object geometry data. This is a async operation, and the mesh will be assigned to the component once is ready
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 * @param MeshGenerator Generator class that has the data to generate the mesh
	 * @param LODsToGenerateNum Num of LODs to Generate. Not all mesh types support this
	 * */
	template<typename MeshType>
	void DispatchCreateAndCacheMeshForImplicitAsync(const uint32 GeometryCacheKey, TSharedPtr<UE::Geometry::FMeshShapeGenerator> MeshGenerator, const int32 LODsToGenerateNum = 0);

	/* Process an FImplicitObject and returns de desired geometry type. Could be directly the shape or another version of the implicit */
	template <bool bIsInstanced, typename GeometryType>
	const GeometryType* GetGeometry(const Chaos::FImplicitObject* InImplicit, const bool bIsScaled, Chaos::FRigidTransform3& OutTransform) const;

	/** Process an FImplicitObject and returns de desired geometry type based on the packed object flags. Could be directly the shape or another version of the implicit */
	template<typename GeometryType>
	const GeometryType* GetGeometryBasedOnPackedType(const Chaos::FImplicitObject* InImplicitObject, Chaos::FRigidTransform3& Transform, const Chaos::EImplicitObjectType PackedType) const;

	/** Tick method of this Geometry builder. Used to do everything that needs to be performed in the GT, like applying the generated meshes to mesh component */
	bool GameThreadTick(float DeltaTime);

	/**
	 * Add a mesh component to the waiting list for Geometry. This needs to be called before dispatching a generation job for new Geometry
	 * @param GeometryKey Key of the geometry that is being generated (or will be generated)
	 * @param MesComponent Component where the geometry needs to be applied
	 * @param LODsToGenerateNum How many LODs we expect the generated mesh to have. Used to check we are using the correct mesh component type
	 */
	void RegisterMeshComponentWaitingForGeometry(uint32 GeometryKey, TWeakObjectPtr<UMeshComponent> MesComponent, const int32 LODsToGenerateNum);

	/** Map containing already generated dynamic mesh for any given implicit object */
	TMap<uint32, TObjectPtr<UDynamicMesh>> DynamicMeshCacheMap;

	/** Map containing already generated static mesh for any given implicit object */
	TMap<uint32, TObjectPtr<UStaticMesh>> StaticMeshCacheMap;

	/** Map containing all the meshes component waiting for geometry, by geometry key*/
	TMap<uint32, TArray<TWeakObjectPtr<UMeshComponent>>> MeshComponentsWaitingForGeometryByKey;

	/** Set of all geometry keys of the Meshes that are being generated but not ready yet */
	TSet<uint32> GeometryBeingGeneratedByKey;
	
	/** Used to locks Read or Writes to the Geometry cache and in flight job tracking containers */
	FRWLock RWLock;

	/** Handle to the ticker used to ticker the Geometry Builder in the game thread*/
	FTSTicker::FDelegateHandle GameThreadTickDelegate;

	/** Queue of geometry keys already generated and waiting to be applied */
	TQueue<uint32, EQueueMode::Mpsc> GeometryReadyToApplyQueue;

	friend class FGeometryGenerationTask;

};

/** Used to execute each individual Geometry Generation task using the data with which was constructed.
 * It allows the task to skip the actual generation attempt if the Geometry builder instance goes away which happens when the tool is closed
 */
class FGeometryGenerationTask
{
public:
	FGeometryGenerationTask(const TWeakPtr<FChaosVDGeometryBuilder>& InBuilder, const TSharedPtr<UE::Geometry::FMeshShapeGenerator>& InGenerator, const uint32 GeometryKey,
		const int32 LODsToGenerateNum)
		: Builder(InBuilder),
		  MeshGenerator(InGenerator),
		  GeometryKey(GeometryKey),
		  LODsToGenerateNum(LODsToGenerateNum)
	{
	}

	template <typename MeshType>
	void GenerateGeometry();

private:
	TWeakPtr<FChaosVDGeometryBuilder> Builder;
	TSharedPtr<UE::Geometry::FMeshShapeGenerator> MeshGenerator;
	uint32 GeometryKey;
	int32 LODsToGenerateNum;
};

template <typename MeshType, typename ComponentType>
void FChaosVDGeometryBuilder::CreateMeshComponentsFromImplicit(const Chaos::FImplicitObject* InImplicitObject, AActor* Owner, TArray<TWeakObjectPtr<UMeshComponent>>& OutMeshComponents, Chaos::FRigidTransform3& Transform, const int32 Index, const int32 DesiredLODCount)
{
	static_assert(std::is_same_v<MeshType, UStaticMesh> || std::is_same_v<MeshType, UDynamicMesh>, "CreateMeshComponentsFromImplicit Only supports DynamicMesh and Static Mesh");
	static_assert(std::is_same_v<ComponentType, UStaticMeshComponent> || std::is_same_v<ComponentType, UInstancedStaticMeshComponent> || std::is_same_v<MeshType, UDynamicMeshComponent>, "CreateMeshComponentsFromImplicit Only supports DynamicMeshComponent, Static MeshComponent and Instanced Static Mesh Component");

	// We could have you make the Mesh type as template and infer the component type based on that, but we also want to be able to create Static Meshes with either Instanced or normal static mesh components
	constexpr bool bHasValidCombinationForStaticMesh =  std::is_same_v<MeshType, UStaticMesh> && (std::is_same_v<ComponentType, UStaticMeshComponent> || std::is_same_v<ComponentType, UInstancedStaticMeshComponent>);
	constexpr bool bHasValidCombinationForDynamicMesh =  std::is_same_v<MeshType, UDynamicMesh> && std::is_same_v<ComponentType, UDynamicMeshComponent>;
	static_assert(bHasValidCombinationForStaticMesh || bHasValidCombinationForDynamicMesh , "Incorrect Component type for Mesh type. Did you use a Dynamic Mesh with a Static Mesh component type?.");

	using namespace Chaos;

	const EImplicitObjectType InnerType = GetInnerType(InImplicitObject->GetType());
	const EImplicitObjectType PackedType = InImplicitObject->GetType();
	
	if (InnerType == ImplicitObjectType::Union)
	{
		const FImplicitObjectUnion* Union = InImplicitObject->template GetObject<FImplicitObjectUnion>();

		for (int i = 0; i < Union->GetObjects().Num(); ++i)
		{
			const TUniquePtr<FImplicitObject>& UnionImplicit = Union->GetObjects()[i];

			CreateMeshComponentsFromImplicit<MeshType, ComponentType>(UnionImplicit.Get(), Owner, OutMeshComponents, Transform, i, DesiredLODCount);	
		}

		return;
	}

	if (InnerType ==  ImplicitObjectType::Transformed)
	{
		const TImplicitObjectTransformed<FReal, 3>* Transformed = InImplicitObject->template GetObject<TImplicitObjectTransformed<FReal, 3>>();
		FRigidTransform3 TransformCopy = Transformed->GetTransform();
		CreateMeshComponentsFromImplicit<MeshType, ComponentType>(Transformed->GetTransformedObject(), Owner, OutMeshComponents,TransformCopy, Index, DesiredLODCount);
		return;
	}

	ComponentType* MeshComponent = nullptr;
	MeshType* Mesh = nullptr;
	switch (InnerType)
	{
		case ImplicitObjectType::Sphere:
		{
			const Chaos::TSphere<FReal, 3>* Sphere = InImplicitObject->template GetObject<Chaos::TSphere<FReal, 3>>();

			const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("Sphere"), FString::FromInt(Index)});

			MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, Transform);

			const uint32 GeometryKey = Sphere->GetTypeHash();
			Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);

			if (Mesh)
			{
				ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
			}
			else
			{
				TSharedPtr<UE::Geometry::FSphereGenerator> SphereGen = MakeShared<UE::Geometry::FSphereGenerator>();
				SphereGen->Radius = Sphere->GetRadius();
				SphereGen->NumTheta = 50;
				SphereGen->NumPhi = 50;
				SphereGen->bPolygroupPerQuad = false;

				RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
				DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, SphereGen);
			}

			break;
		}
		case ImplicitObjectType::Box:
		{
			const Chaos::TBox<FReal, 3>* Box = InImplicitObject->template GetObject<Chaos::TBox<FReal, 3>>();

			const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("Box"), FString::FromInt(Index)});
			MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, Transform);

			const uint32 GeometryKey = Box->GetTypeHash();
			Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);

			if (Mesh)
			{
				ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
			}
			else
			{
				TSharedPtr<UE::Geometry::FMinimalBoxMeshGenerator> BoxGen = MakeShared<UE::Geometry::FMinimalBoxMeshGenerator>();
				UE::Geometry::FOrientedBox3d OrientedBox;
				OrientedBox.Frame = UE::Geometry::FFrame3d(Box->Center());
				OrientedBox.Extents = Box->Extents() * 0.5;
				BoxGen->Box = OrientedBox;

				RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
				DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, BoxGen);
			}

			break;
		}
		case ImplicitObjectType::Plane:
			break;
		case ImplicitObjectType::Capsule:
		{
			const FCapsule* Capsule = InImplicitObject->template GetObject<FCapsule>();

			const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("Capsule"), FString::FromInt(Index)});
			const FRigidTransform3 StartingTransform;
			MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, StartingTransform);

			// Re-adjust the location so the pivot is not the center of the capsule, and transform it based on the provided transform
			const FVector FinalLocation = Transform.TransformPosition(Capsule->GetCenter() - Capsule->GetAxis() * Capsule->GetSegment().GetLength() * 0.5f);
			const FQuat Rotation = FRotationMatrix::MakeFromZ(Capsule->GetAxis()).Rotator().Quaternion();

			MeshComponent->SetRelativeRotation(Transform.GetRotation() * Rotation);
			MeshComponent->SetRelativeLocation(FinalLocation);
			MeshComponent->SetRelativeScale3D(Transform.GetScale3D());

			const uint32 GeometryKey = Capsule->GetTypeHash();
			Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);

			if (Mesh)
			{
				ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
			}
			else
			{
				TSharedPtr<UE::Geometry::FCapsuleGenerator> CapsuleGenerator = MakeShared<UE::Geometry::FCapsuleGenerator>();
				CapsuleGenerator->Radius = FMath::Max(FMathf::ZeroTolerance, Capsule->GetRadius());
				CapsuleGenerator->SegmentLength = FMath::Max(FMathf::ZeroTolerance, Capsule->GetSegment().GetLength());
				CapsuleGenerator->NumHemisphereArcSteps = 12;
				CapsuleGenerator->NumCircleSteps = 12;
				CapsuleGenerator->bPolygroupPerQuad = false;

				RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
				DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, CapsuleGenerator);
			}
			break;
		}
		case ImplicitObjectType::LevelSet:
		{
			//TODO: Implement
			break;
		}
		break;
		case ImplicitObjectType::Convex:
		{
			if (const FConvex* Convex = GetGeometryBasedOnPackedType<FConvex>(InImplicitObject, Transform, PackedType))
			{
				const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("Convex"), FString::FromInt(Index)});
				MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, Transform);
				
				const uint32 GeometryKey = Convex->GetTypeHash();
				Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);
	
				if (Mesh)
				{
					ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
				}
				else
				{
					TSharedPtr<FChaosVDConvexMeshGenerator> ConvexMeshGen = MakeShared<FChaosVDConvexMeshGenerator>();
					ConvexMeshGen->GenerateFromConvex(*Convex);

					RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
					DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey ,ConvexMeshGen);
				}
			}

			break;
		}
		case ImplicitObjectType::TaperedCylinder:
			break;
		case ImplicitObjectType::Cylinder:
			break;
		case ImplicitObjectType::TriangleMesh:
		{
			if (const FTriangleMeshImplicitObject* TriangleMesh = GetGeometryBasedOnPackedType<FTriangleMeshImplicitObject>(InImplicitObject, Transform, PackedType))
			{
				const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("Trimesh"), FString::FromInt(Index)});
				MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, Transform);

				const uint32 GeometryKey = TriangleMesh->GetTypeHash();
				Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);
	
				if (Mesh)
				{
					ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
				}
				else
				{
					TSharedPtr<FChaosVDTriMeshGenerator> TriMeshGen = MakeShared<FChaosVDTriMeshGenerator>();
					TriMeshGen->bReverseOrientation = true;
					TriMeshGen->GenerateFromTriMesh(*TriangleMesh);

					RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
					DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, TriMeshGen);
				}
			}
				
			break;
		}
		case ImplicitObjectType::HeightField:
		{
			if (const FHeightField* HeightField = GetGeometryBasedOnPackedType<FHeightField>(InImplicitObject, Transform, PackedType))
			{
				const FString Name = FString::Format(TEXT("{0} - {1}"), {TEXT("HeightField"), FString::FromInt(Index)});
				MeshComponent = CreateMeshComponent<ComponentType>(Owner, Name, Transform);

				const uint32 GeometryKey = HeightField->GetTypeHash();
				Mesh = GetCachedMeshForImplicit<MeshType>(GeometryKey);
	
				if (Mesh)
				{
					ApplyMeshToComponentFromKey(MeshComponent, GeometryKey);
				}
				else
				{
					TSharedPtr<FChaosVDHeightFieldMeshGenerator> HeightFieldMeshGen = MakeShared<FChaosVDHeightFieldMeshGenerator>();
					HeightFieldMeshGen->bReverseOrientation = false;
					HeightFieldMeshGen->GenerateFromHeightField(*HeightField);

					RegisterMeshComponentWaitingForGeometry(GeometryKey, MeshComponent, DesiredLODCount);
					DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, HeightFieldMeshGen, DesiredLODCount);
				}

				return;
			}
		
			break;
		}
		default:
			break;
		}

		if (MeshComponent != nullptr)
		{
			OutMeshComponents.Add(MeshComponent);
		}
}

template <typename MeshType>
MeshType* FChaosVDGeometryBuilder::GetCachedMeshForImplicit(const uint32 GeometryCacheKey)
{
	if constexpr (std::is_same_v<MeshType, UDynamicMesh>)
	{
		if (TObjectPtr<MeshType>* MeshPtrPtr = DynamicMeshCacheMap.Find(GeometryCacheKey))
		{
			return *MeshPtrPtr;
		}
	}
	else if constexpr (std::is_same_v<MeshType, UStaticMesh>)
	{
		if (TObjectPtr<MeshType>* MeshPtrPtr = StaticMeshCacheMap.Find(GeometryCacheKey))
		{
			return *MeshPtrPtr;
		}
	}

	return nullptr;
}

template <typename ComponentType>
ComponentType* FChaosVDGeometryBuilder::CreateMeshComponent(AActor* Owner, const FString& Name, const Chaos::FRigidTransform3& Transform) const
{
	ComponentType* MeshComponent = NewObject<ComponentType>(Owner, *Name);

	if (Owner)
	{
		MeshComponent->RegisterComponent();
		MeshComponent->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		Owner->AddInstanceComponent(MeshComponent);
	}

	MeshComponent->bSelectable = true;

	if constexpr (std::is_same_v<ComponentType, UDynamicMeshComponent> || std::is_same_v<ComponentType, UStaticMeshComponent>)
	{
		MeshComponent->SetRelativeTransform(Transform);
	}
	else if constexpr (std::is_same_v<ComponentType, UInstancedStaticMeshComponent>)
	{
		// If we have negative scale we need to force reverse the culling mode in this component otherwise the faces will be inverted
		MeshComponent->SetReverseCulling(HasNegativeScale(Transform));
		MeshComponent->AddInstance(Transform);
	}

	return MeshComponent;
}

template <typename MeshType>
void FChaosVDGeometryBuilder::DispatchCreateAndCacheMeshForImplicitAsync(const uint32 GeometryKey, TSharedPtr<UE::Geometry::FMeshShapeGenerator> MeshGenerator, const int32 LODsToGenerateNum)
{
	{
		FReadScopeLock ReadLock(RWLock);
		if (GeometryBeingGeneratedByKey.Contains(GeometryKey))
		{
			return;
		}
	}

	{
		FWriteScopeLock WriteLock(RWLock);
		GeometryBeingGeneratedByKey.Add(GeometryKey);
	}
	
	TSharedPtr<FGeometryGenerationTask> GenerationTask = MakeShared<FGeometryGenerationTask>(AsWeak(), MeshGenerator, GeometryKey,LODsToGenerateNum);
	UE::Tasks::Launch(TEXT("GeometryGeneration"),
		[GenerationTask]()
		{
			GenerationTask->GenerateGeometry<MeshType>();
		});
}

template <bool bIsInstanced, typename GeometryType>
const GeometryType* FChaosVDGeometryBuilder::GetGeometry(const Chaos::FImplicitObject* InImplicitObject, const bool bIsScaled, Chaos::FRigidTransform3& OutTransform) const
{
	if (bIsScaled)
	{
		if (const Chaos::TImplicitObjectScaled<GeometryType, bIsInstanced>* ImplicitScaled = InImplicitObject->template GetObject<Chaos::TImplicitObjectScaled<GeometryType, bIsInstanced>>())
		{
			OutTransform.SetScale3D(ImplicitScaled->GetScale());
			return ImplicitScaled->GetUnscaledObject()->template GetObject<GeometryType>();
		}
	}
	else
	{
		if (bIsInstanced)
		{
			const Chaos::TImplicitObjectInstanced<GeometryType>* ImplicitInstanced = InImplicitObject->template GetObject<Chaos::TImplicitObjectInstanced<GeometryType>>();
			return ImplicitInstanced->GetInnerObject()->template GetObject<GeometryType>();
		}
		else
		{
			return InImplicitObject->template GetObject<GeometryType>();
		}
	}
	
	return nullptr;
}

template <typename GeometryType>
const GeometryType* FChaosVDGeometryBuilder::GetGeometryBasedOnPackedType(const Chaos::FImplicitObject* InImplicitObject, Chaos::FRigidTransform3& Transform,  const Chaos::EImplicitObjectType PackedType) const
{
	using namespace Chaos;

	const bool bIsInstanced = IsInstanced(PackedType);
	const bool bIsScaled = IsScaled(PackedType);

	if (bIsInstanced)
	{
		return GetGeometry<true,GeometryType>(InImplicitObject, bIsScaled, Transform);
	}
	else
	{
		return GetGeometry<false, GeometryType>(InImplicitObject, bIsScaled, Transform);
	}

	return nullptr;
}

template <typename MeshType>
void FGeometryGenerationTask::GenerateGeometry()
{
	if (const TSharedPtr<FChaosVDGeometryBuilder> BuilderPtr = Builder.Pin())
	{
		if constexpr (std::is_same_v<MeshType, UDynamicMesh>)
		{
			ensureMsgf(LODsToGenerateNum == 0, TEXT("LOD Generation is only suppoted with static meshes | [%d] LODs were requested for a dynamic mesh when 0 is expected"), LODsToGenerateNum);
			BuilderPtr->CreateAndCacheDynamicMesh(GeometryKey, *MeshGenerator.Get());

			BuilderPtr->GeometryReadyToApplyQueue.Enqueue(GeometryKey);
		}
		else if constexpr (std::is_same_v<MeshType, UStaticMesh>)
		{
			BuilderPtr->CreateAndCacheStaticMesh(GeometryKey, *MeshGenerator.Get(), LODsToGenerateNum);

			BuilderPtr->GeometryReadyToApplyQueue.Enqueue(GeometryKey);
		}

		{
			FWriteScopeLock WriteLock(BuilderPtr->RWLock);
			BuilderPtr->GeometryBeingGeneratedByKey.Remove(GeometryKey);
		}
	}
}
