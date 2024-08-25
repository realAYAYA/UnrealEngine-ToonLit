// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDExtractedGeometryDataHandle.h"
#include "ChaosVDGeometryDataComponent.h"
#include "ChaosVDHeightfieldMeshGenerator.h"
#include "ChaosVDMeshComponentPool.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "ObjectsWaitingGeometryList.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/ImplicitObjectType.h"
#include "Containers/Ticker.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

#include "UDynamicMesh.h"
#include "Components/ChaosVDInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/WeakObjectPtr.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "UObject/UObjectGlobals.h"

class UChaosVDStaticMeshComponent;
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

typedef TWeakObjectPtr<UMeshComponent> FMeshComponentWeakPtr;
typedef TSharedPtr<FChaosVDExtractedGeometryDataHandle> FExtractedGeometryHandle;

/*
 * Generates Dynamic mesh components and dynamic meshes based on Chaos implicit object data
 */
class FChaosVDGeometryBuilder : public FGCObject, public TSharedFromThis<FChaosVDGeometryBuilder>
{
public:

	FChaosVDGeometryBuilder()
	{
		FInstancedStaticMeshDelegates::OnInstanceIndexUpdated.AddRaw(this, &FChaosVDGeometryBuilder::HandleStaticMeshComponentInstanceIndexUpdated);
	}

	virtual ~FChaosVDGeometryBuilder() override
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GameThreadTickDelegate);

		FInstancedStaticMeshDelegates::OnInstanceIndexUpdated.RemoveAll(this);

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
	
	void Initialize(const TWeakPtr<FChaosVDScene>& ChaosVDScene);

	/** Creates Dynamic Mesh components for each object within the provided Implicit object
	 *	@param InImplicitObject : Implicit object to process
	 *	@param Owner Actor who will own the generated components
	 *	@param OutMeshDataHandles Array containing all the generated components
	 *	@param DesiredLODCount Number of LODs to generate for the mesh.
	 *	@param MeshIndex Index of the current component being processed. This is useful when this method is called recursively
	 *	@param InTransform to apply to the generated components/geometry
	 */
	
	template<typename MeshType>
	void CreateMeshesFromImplicitObject(const Chaos::FImplicitObject* InImplicitObject, AActor* Owner, TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>& OutMeshDataHandles, const int32 DesiredLODCount = 0, const Chaos::FRigidTransform3& InTransform = Chaos::FRigidTransform3(), const int32 MeshIndex = 0)
	{
		// To start set the leaf and the root to the same ptr. If the object is an union, in the subsequent recursive call the leaf will be set correctly
		CreateMeshesFromImplicit_Internal<MeshType>(InImplicitObject, InImplicitObject, Owner, OutMeshDataHandles, DesiredLODCount, InTransform, MeshIndex);
	}
	
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

	/**
	 * Evaluates the provided transform's scale, and returns true if the scale has a negative component
	 * @param InTransform Transform to evaluate
	 */
	static bool HasNegativeScale(const Chaos::FRigidTransform3& InTransform);

private:
	
	template<typename MeshType>
	void CreateMeshesFromImplicit_Internal(const Chaos::FImplicitObject* InRootImplicitObject,const Chaos::FImplicitObject* InLeafImplicitObject, AActor* Owner, TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>& OutMeshDataHandles, const int32 DesiredLODCount = 0, const Chaos::FRigidTransform3& InTransform = Chaos::FRigidTransform3(), const int32 MeshIndex = 0);
	
public:
	/**
	 * Return true if we have cached geometry for the provided Geometry Key
	 * @param GeometryKey Cache key for the geometry we are looking for
	 */
	bool HasGeometryInCache(uint32 GeometryKey);
	bool HasGeometryInCache_AssumesLocked(uint32 GeometryKey) const;

	/** Returns an already mesh for the provided implicit object if exists, otherwise returns null
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 */
	template<typename MeshType>
	MeshType* GetCachedMeshForImplicit(const uint32 GeometryCacheKey);

private:
	/** Creates a Dynamic Mesh for the provided Implicit object and generator, and then caches it to be reused later
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 * @param MeshGenerator Generator class with the data and rules to create the mesh
	 */
	UDynamicMesh* CreateAndCacheDynamicMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator);

	/** Creates a Dynamic Mesh for the provided Implicit object and generator, and then caches it to be reused later
	 * @param GeometryCacheKey Key to be used to find this geometry in the cache
	 * @param MeshGenerator Generator class with the data and rules to create the mesh
	 * @param LODsToGenerateNum Number of LODs to generate for this static mesh
	 */
	UStaticMesh* CreateAndCacheStaticMesh(const uint32 GeometryCacheKey, UE::Geometry::FMeshShapeGenerator& MeshGenerator, const int32 LODsToGenerateNum = 0);

	/** Takes a Mesh component ptr and initializes it to be used with the provided owner
	 * @param Owner Actor that will own the provided Mesh Component
	 * @param MeshComponent Component to initialize
	 */
	template <class ComponentType>
    bool InitializeMeshComponent(AActor* Owner, ComponentType* MeshComponent);

	template <class ComponentType>
	void SetMeshComponentMaterial(EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, ComponentType* MeshComponent);

public:
	/**
	 * Finds or creates a Mesh component for the geometry data handle provided, and add a new instance of that geometry to it
	 * @param InOwningParticleData Particle data from which the implicit object is from
	 * @param InExtractedGeometryDataHandle Handle to the extracted geometry data the new component will use
	 * */
	template<typename ComponentType>
	TSharedPtr<FChaosVDMeshDataInstanceHandle> CreateMeshDataInstance(const FChaosVDParticleDataWrapper& InOwningParticleData, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle);

	/**
	 * Finds or creates a Mesh component compatible with the provided mesh data handle, and updates the handle to use that new component.
	 * This is used when data in the handle changed and becomes no longer compatible with the mesh component in use.
	 * @param HandleToUpdate Instance handle we need to update to a new component
	 * @param MeshAttributes Attributes of the mesh that the new component needs to be compativle with
	 * */
	template<typename ComponentType>
	void UpdateMeshDataInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> HandleToUpdate, EChaosVDMeshAttributesFlags MeshAttributes);

	/**
	 * Destroys a Mesh component that will not longer be used.
	 * If pooling is enabled, the component will be reset and added back to the pool
	 * @param MeshComponent Component to Destroy
	 * */
	void DestroyMeshComponent(UMeshComponent* MeshComponent);

private:

	/** Gets a ptr to a fully initialized Mesh component compatible with the provided geometry handle and mesh attribute flags, ready to accept a new mesh instance
	 * @param GeometryDataHandle Handle with the data of the geometry to be used for the new instance
	 * @param MeshAttributes Set of flags that the mesh component needs to be compatible with
	 */
	template<typename ComponentType>
	ComponentType* GetMeshComponentForNewInstance(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& GeometryDataHandle, EChaosVDMeshAttributesFlags MeshAttributes);

	/** Gets a reference to the correct instanced static mesh component cache that is compatible with the provided mesh attribute flags
	 * @param MeshAttributeFlags GeometryHandle this component will render
	 */
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*>& GetInstancedStaticMeshComponentCacheMap(EChaosVDMeshAttributesFlags MeshAttributeFlags);

	/** Gets any available instanced static mesh component that is compatible with the provided mesh attributes and component type
	 * @param InExtractedGeometryDataHandle GeometryHandle this component will render
	 * @param MeshComponentsContainerActor Actor that will owns the mesh component
	 * @param MeshComponentAttributeFlags Set of flags that the mesh component needs to be compatible with
	 * @param bOutIsNewComponent True if the component we are returning is new. False if it is an existing one but that can take a new mesh instance
	 */
	template <typename ComponentType>
	ComponentType* GetAvailableInstancedStaticMeshComponent(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle, AActor* MeshComponentsContainerActor, EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, bool& bOutIsNewComponent);

	/** Gets any available mesh component that is compatible with the provided mesh attributes and component type
	 * @param InExtractedGeometryDataHandle GeometryHandle this component will render
	 * @param MeshComponentsContainerActor Actor that will owns the mesh component
	 * @param MeshComponentAttributeFlags Set of flags that the mesh component needs to be compatible with
	 * @param bOutIsNewComponent True if the component we are returning is new. False if it is an existing one but that can take a new mesh instance
	 */
	template <typename ComponentType>
	ComponentType* GetAvailableMeshComponent(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle, AActor* MeshComponentsContainerActor, EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, bool& bOutIsNewComponent);

	/**
	 * Applies a mesh to a mesh component based on its type
	 * @param MeshComponent Mesh component to apply the mesh to
	 * @param GeometryKey Key to find the mesh to apply in the cache
	 */
	bool ApplyMeshToComponentFromKey(TWeakObjectPtr<UMeshComponent> MeshComponent, const uint32 GeometryKey);

public:
	/** Creates a mesh generator for the provided Implicit object which will be used to create a Static Mesh or Dynamic Mesh
	 * @param InImplicit ImplicitObject used a data source for the mesh generator
	 * @param SimpleShapesComplexityFactor Factor used to reduce or increase the complexity (number of triangles generated) of simple shapes (Sphere/Capsule)
	 */
	TSharedPtr<UE::Geometry::FMeshShapeGenerator> CreateMeshGeneratorForImplicitObject(const Chaos::FImplicitObject* InImplicit, float SimpleShapesComplexityFactor = 1.0f);

private:

	const Chaos::FImplicitObject* UnpackImplicitObject(const Chaos::FImplicitObject* InImplicitObject, Chaos::FRigidTransform3& InOutTransform) const;
	
public:
	/** Re-adjust the provided transform if needed, so it can be visualized properly with its generated mesh */
	void AdjustedTransformForImplicit(const Chaos::FImplicitObject* InImplicit, FTransform& OutAdjustedTransform);

private:

	/** Extracts data from an implicit object in a format CVD can use, and starts the Mesh generation process if needed
	 * @return Returns a handle to the generated data that can be used to access the generated mesh when ready
	 */
	template <typename MeshType>
	TSharedPtr<FChaosVDExtractedGeometryDataHandle> ExtractGeometryDataForImplicit(const Chaos::FImplicitObject* InImplicitObject, const Chaos::FRigidTransform3& InTransform, const int32 Index);

	/** Returns true if the implicit object if of one of the types we need to unpack before generating a mesh for it */
	bool ImplicitObjectNeedsUnpacking(const Chaos::FImplicitObject* InImplicitObject) const;

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

public:
	/**
	 * Add a mesh component to the waiting list for Geometry. This needs to be called before dispatching a generation job for new Geometry
	 * @param GeometryKey Key of the geometry that is being generated (or will be generated)
	 * @param MeshComponent Component where the geometry needs to be applied
	 */
	void AddMeshComponentWaitingForGeometry(uint32 GeometryKey, TWeakObjectPtr<UMeshComponent> MeshComponent) const;

	/**
	 * Add a mesh component to the waiting list for Geometry. This needs to be called before dispatching a generation job for new Geometry
	 * @param GeometryKey Key of the geometry that is being generated (or will be generated)
	 * @param MeshComponent Component where the geometry needs to be applied
	 */
	void RemoveMeshComponentWaitingForGeometry(uint32 GeometryKey, TWeakObjectPtr<UMeshComponent> MeshComponent) const;

private:

	/** Handles any changes to the indexes of created instanced mesh components we are managing, making corrections/updates as needed */
	void HandleStaticMeshComponentInstanceIndexUpdated(UInstancedStaticMeshComponent* InComponent, TArrayView<const FInstancedStaticMeshDelegates::FInstanceIndexUpdateData> InIndexUpdates);

	/** Map containing already generated dynamic mesh for any given implicit object */
	TMap<uint32, TObjectPtr<UDynamicMesh>> DynamicMeshCacheMap;

	/** Map containing already generated static mesh for any given implicit object */
	TMap<uint32, TObjectPtr<UStaticMesh>> StaticMeshCacheMap;

	/** Set of all geometry keys of the Meshes that are being generated but not ready yet */
	TSet<uint32> GeometryBeingGeneratedByKey;
	
	/** Used to lock Read or Writes to the Geometry cache and in flight job tracking containers */
	FRWLock GeometryCacheRWLock;

	/** Handle to the ticker used to ticker the Geometry Builder in the game thread*/
	FTSTicker::FDelegateHandle GameThreadTickDelegate;

	/** Object containing all the meshes component waiting for geometry, by geometry key*/
	TUniquePtr<FObjectsWaitingGeometryList<FMeshComponentWeakPtr>> MeshComponentsWaitingForGeometry;

	/** Object containing all the meshes component waiting for geometry, by geometry key*/
	TUniquePtr<FObjectsWaitingGeometryList<FExtractedGeometryHandle>> HandlesWaitingForGeometry;

	/** Map containing already initialized Instanced static mesh components for any given geometry key */
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*> InstancedMeshComponentByGeometryKey;

	/** Map containing already initialized Instanced static mesh components ready to be use with translucent materials, for any given geometry key */
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*> TranslucentInstancedMeshComponentByGeometryKey;

	/** Map containing already initialized Instanced static mesh components for mesh instances that required a negative scale transform, for any given geometry key */
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*> MirroredInstancedMeshComponentByGeometryKey;

	/** Map containing already initialized Instanced static mesh components for mesh instances that required a negative scale transform and use a translucent material, for any given geometry key */
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*> TranslucentMirroredInstancedMeshComponentByGeometryKey;

	/** Instance of uninitialized mesh components pool */
	FChaosVDMeshComponentPool ComponentMeshPool;

	/** Weak Ptr to the CVD scene owning this geometry builder */
	TWeakPtr<FChaosVDScene> SceneWeakPtr;
	
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

template <typename MeshType>
TSharedPtr<FChaosVDExtractedGeometryDataHandle> FChaosVDGeometryBuilder::ExtractGeometryDataForImplicit(const Chaos::FImplicitObject* InImplicitObject, const Chaos::FRigidTransform3& InTransform, const int32 Index)
{
	const uint32 ImplicitObjectHash = InImplicitObject->GetTypeHash();

	Chaos::FRigidTransform3 ExtractedTransform = InTransform;
	const bool bNeedsUnpack = ImplicitObjectNeedsUnpacking(InImplicitObject);
	if (const Chaos::FImplicitObject* ImplicitObjectToProcess = bNeedsUnpack ? UnpackImplicitObject(InImplicitObject, ExtractedTransform) : InImplicitObject)
	{
		TSharedPtr<FChaosVDExtractedGeometryDataHandle> MeshDataHandle = MakeShared<FChaosVDExtractedGeometryDataHandle>();

		const uint32 GeometryKey = ImplicitObjectToProcess->GetTypeHash();		
		MeshDataHandle->SetGeometryKey(GeometryKey);
	
		// For the Component data key, we need the hash of the implicit as it is (packed) because we will need to match it when looking for shape data
		MeshDataHandle->SetDataComponentKey(bNeedsUnpack ? ImplicitObjectHash : GeometryKey);

		AdjustedTransformForImplicit(ImplicitObjectToProcess, ExtractedTransform);
		MeshDataHandle->SetTransform(ExtractedTransform);

		if (!HasGeometryInCache(GeometryKey))
		{
			if (TSharedPtr<UE::Geometry::FMeshShapeGenerator> MeshGenerator = CreateMeshGeneratorForImplicitObject(ImplicitObjectToProcess))
			{
				DispatchCreateAndCacheMeshForImplicitAsync<MeshType>(GeometryKey, MeshGenerator);
			}
			else
			{
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed create geometry data handle | Failed to generate a valid mesh generator from the implicit object. | Geometry Key [%u] "), ANSI_TO_TCHAR(__FUNCTION__), GeometryKey);
				return nullptr;
			}
		}

		return MeshDataHandle;
	}

	return nullptr;
}

template <typename MeshType>
void FChaosVDGeometryBuilder::CreateMeshesFromImplicit_Internal(const Chaos::FImplicitObject* InRootImplicitObject, const Chaos::FImplicitObject* InLeafImplicitObject, AActor* Owner, TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>& OutMeshDataHandles, const int32 DesiredLODCount, const Chaos::FRigidTransform3& InTransform, const int32 MeshIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FChaosVDGeometryBuilder::CreateMeshesFromImplicit_Internal);

	using namespace Chaos;

	const EImplicitObjectType InnerType = GetInnerType(InRootImplicitObject->GetType());
	
	if (InnerType == ImplicitObjectType::Union || InnerType == ImplicitObjectType::UnionClustered)
	{
		if (const FImplicitObjectUnion* Union = InLeafImplicitObject->template AsA<FImplicitObjectUnion>())
		{
			for (int32 ObjectIndex = 0; ObjectIndex < Union->GetObjects().Num(); ++ObjectIndex)
			{
				const FImplicitObjectPtr& UnionImplicit = Union->GetObjects()[ObjectIndex];

				CreateMeshesFromImplicitObject<MeshType>(UnionImplicit.GetReference(), Owner, OutMeshDataHandles, DesiredLODCount, InTransform, ObjectIndex);	
			}
		}

		return;
	}

	if (InnerType == ImplicitObjectType::Transformed)
	{
		if (const TImplicitObjectTransformed<FReal, 3>* Transformed = InLeafImplicitObject->template GetObject<TImplicitObjectTransformed<FReal, 3>>())
		{
			CreateMeshesFromImplicitObject<MeshType>(Transformed->GetTransformedObject(), Owner, OutMeshDataHandles, DesiredLODCount, Transformed->GetTransform(), MeshIndex);
		}
		
		return;
	}

	if (const TSharedPtr<FChaosVDExtractedGeometryDataHandle> MeshDataHandle = ExtractGeometryDataForImplicit<MeshType>(InLeafImplicitObject, InTransform, MeshIndex))
	{	
		MeshDataHandle->SetImplicitObject(InLeafImplicitObject);
		MeshDataHandle->SetImplicitObjectIndex(MeshIndex);
		MeshDataHandle->SetRootImplicitObject(InRootImplicitObject);

		OutMeshDataHandles.Add(MeshDataHandle);
	}
}

template <typename MeshType>
MeshType* FChaosVDGeometryBuilder::GetCachedMeshForImplicit(const uint32 GeometryCacheKey)
{
	if constexpr (std::is_same_v<MeshType, UDynamicMesh>)
	{
		if (const TObjectPtr<UDynamicMesh>* MeshPtrPtr = DynamicMeshCacheMap.Find(GeometryCacheKey))
		{
			return MeshPtrPtr->Get();
		}
	}
	else if constexpr (std::is_same_v<MeshType, UStaticMesh>)
	{
		if (const TObjectPtr<UStaticMesh>* MeshPtrPtr = StaticMeshCacheMap.Find(GeometryCacheKey))
		{
			return MeshPtrPtr->Get();
		}
	}

	return nullptr;
}

template <typename ComponentType>
bool FChaosVDGeometryBuilder::InitializeMeshComponent(AActor* Owner, ComponentType* MeshComponent)
{
	if (!ensure(MeshComponent))
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Create mesh component | Component Is Null. "), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	if (Owner)
	{
		Owner->AddOwnedComponent(MeshComponent);
		Owner->AddInstanceComponent(MeshComponent);
		MeshComponent->RegisterComponent();
		MeshComponent->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

		MeshComponent->bSelectable = true;
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Register Component | Owner Is Null. "), ANSI_TO_TCHAR(__FUNCTION__));
		return false;
	}

	return true;
}

template <typename ComponentType>
void FChaosVDGeometryBuilder::SetMeshComponentMaterial(EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, ComponentType* MeshComponent)
{
	UMaterialInstanceDynamic* Material = nullptr;

	Material = FChaosVDGeometryComponentUtils::CreateMaterialInstance(FChaosVDGeometryComponentUtils::GetMaterialTypeForComponent<ComponentType>(MeshComponentAttributeFlags));

	ensure(Material);

	MeshComponent->SetMaterial(0, Material);
}


template <typename ComponentType>
ComponentType* FChaosVDGeometryBuilder::GetAvailableInstancedStaticMeshComponent(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle, AActor* MeshComponentsContainerActor, EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, bool& bOutIsNewComponent)
{
	// Get the correct Instanced Mesh Component from the existing cache
	TMap<uint32, UChaosVDInstancedStaticMeshComponent*>& InstancedMeshComponentMapToSearch = GetInstancedStaticMeshComponentCacheMap(MeshComponentAttributeFlags);

	if (UChaosVDInstancedStaticMeshComponent** FoundInstancedMeshComponent = InstancedMeshComponentMapToSearch.Find(InExtractedGeometryDataHandle->GetGeometryKey()))
	{
		bOutIsNewComponent = false;
		return Cast<ComponentType>(*FoundInstancedMeshComponent);
	}
	else
	{
		// If no exiting component meets our requirements, get a new one form the pool
		ComponentType* Component = ComponentMeshPool.AcquireMeshComponent<ComponentType>(MeshComponentsContainerActor, InExtractedGeometryDataHandle->GetName());
		if (!InitializeMeshComponent<ComponentType>(MeshComponentsContainerActor, Component))
		{
			return nullptr;
		}

		// If this is a Instanced Static Mesh Component, make sure we set the reverse culling flag correctly
		// And the material
		if (UChaosVDInstancedStaticMeshComponent* AsInstancedMeshComponent = Component)
		{
			AsInstancedMeshComponent->bReverseCulling = EnumHasAnyFlags(MeshComponentAttributeFlags, EChaosVDMeshAttributesFlags::MirroredGeometry);
			SetMeshComponentMaterial<ComponentType>(MeshComponentAttributeFlags, AsInstancedMeshComponent);
		}

		bOutIsNewComponent = true;

		InstancedMeshComponentMapToSearch.Add(InExtractedGeometryDataHandle->GetGeometryKey(), Component);

		return Component;
	}
}

template <typename ComponentType>
ComponentType* FChaosVDGeometryBuilder::GetAvailableMeshComponent(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle, AActor* MeshComponentsContainerActor, EChaosVDMeshAttributesFlags MeshComponentAttributeFlags, bool& bOutIsNewComponent)
{
	ComponentType* MeshComponent = nullptr;
	if constexpr (std::is_base_of_v<UInstancedStaticMeshComponent, ComponentType>)
	{
		MeshComponent = GetAvailableInstancedStaticMeshComponent<ComponentType>(InExtractedGeometryDataHandle, MeshComponentsContainerActor, MeshComponentAttributeFlags, bOutIsNewComponent);
	}
	else
	{
		MeshComponent = ComponentMeshPool.AcquireMeshComponent<ComponentType>(MeshComponentsContainerActor, InExtractedGeometryDataHandle->GetName());
		if (!InitializeMeshComponent<ComponentType>(MeshComponentsContainerActor, MeshComponent))
		{
			return nullptr;
		}

		SetMeshComponentMaterial(MeshComponentAttributeFlags, MeshComponent);

		bOutIsNewComponent = true;
	}

	return MeshComponent;
}


template <typename ComponentType>
TSharedPtr<FChaosVDMeshDataInstanceHandle> FChaosVDGeometryBuilder::CreateMeshDataInstance(const FChaosVDParticleDataWrapper& InOwningParticleData, const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InExtractedGeometryDataHandle)
{
	static_assert(std::is_base_of_v<UChaosVDStaticMeshComponent, ComponentType> || std::is_base_of_v<UChaosVDInstancedStaticMeshComponent, ComponentType>, "CreateMeshComponentsFromImplicit Only supports CVD versions of Static MeshComponent and Instanced Static Mesh Component");

	if (!InExtractedGeometryDataHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Create mesh Component | Handle is invalid. "), ANSI_TO_TCHAR(__FUNCTION__));
		return nullptr;
	}

	const FTransform ExtractedGeometryTransform = InExtractedGeometryDataHandle->GetRelativeTransform();

	EChaosVDMeshAttributesFlags MeshComponentAttributeFlags = EChaosVDMeshAttributesFlags::None;
	if (HasNegativeScale(ExtractedGeometryTransform))
	{
		EnumAddFlags(MeshComponentAttributeFlags, EChaosVDMeshAttributesFlags::MirroredGeometry);
	}

	TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshComponentHandle;
	ComponentType* Component = GetMeshComponentForNewInstance<ComponentType>(InExtractedGeometryDataHandle, MeshComponentAttributeFlags);

	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(Component))
	{
		constexpr bool bIsWorldSpace = true;
		const FTransform OwningParticleTransform(InOwningParticleData.ParticlePositionRotation.MR, InOwningParticleData.ParticlePositionRotation.MX);
		MeshComponentHandle = CVDGeometryComponent->AddMeshInstance(OwningParticleTransform, bIsWorldSpace, InExtractedGeometryDataHandle, InOwningParticleData.ParticleIndex, InOwningParticleData.SolverID);
		
		MeshComponentHandle->SetGeometryBuilder(AsWeak());
	}

	return MeshComponentHandle;
}

template <typename ComponentType>
void FChaosVDGeometryBuilder::UpdateMeshDataInstance(TSharedPtr<FChaosVDMeshDataInstanceHandle> HandleToUpdate, EChaosVDMeshAttributesFlags MeshAttributes)
{
	static_assert(std::is_base_of_v<UChaosVDStaticMeshComponent, ComponentType> || std::is_base_of_v<UChaosVDInstancedStaticMeshComponent, ComponentType>, "CreateMeshComponentsFromImplicit Only supports CVD versions of Static MeshComponent and Instanced Static Mesh Component");

	if (!HandleToUpdate.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Update Mesh Data Handle | Mesh Data Handle is invalid. "), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}
	
	TSharedPtr<FChaosVDExtractedGeometryDataHandle> ExtractedGeometryDataHandle = HandleToUpdate->GetGeometryHandle();
	if (!ExtractedGeometryDataHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Update Mesh Data Handle | Geometry Data Handle is invalid. "), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	ComponentType* Component = GetMeshComponentForNewInstance<ComponentType>(HandleToUpdate->GetGeometryHandle(), MeshAttributes);

	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(Component))
	{
		constexpr bool bIsWorldSpace = true;
		CVDGeometryComponent->AddMeshInstanceForHandle(HandleToUpdate, HandleToUpdate->GetWorldTransform(), bIsWorldSpace, HandleToUpdate->GetGeometryHandle(), HandleToUpdate->GetOwningParticleID(), HandleToUpdate->GetOwningSolverID());
	}
}

template <typename ComponentType>
ComponentType* FChaosVDGeometryBuilder::GetMeshComponentForNewInstance(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& GeometryDataHandle, EChaosVDMeshAttributesFlags MeshAttributes)
{
	if (!GeometryDataHandle.IsValid())
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To obtain Mesh Component | Geometry Data Handle is invalid. "), ANSI_TO_TCHAR(__FUNCTION__));
		return nullptr;
	}

	AActor* MeshComponentsContainerActor = nullptr;
	
	if (const TSharedPtr<FChaosVDScene> CVDScene = SceneWeakPtr.Pin())
	{
		MeshComponentsContainerActor = CVDScene->GetMeshComponentsContainerActor();
	}

	if (!MeshComponentsContainerActor)
	{
		return nullptr;
	}

	bool bIsNew = false;
	ComponentType* Component = GetAvailableMeshComponent<ComponentType>(GeometryDataHandle, MeshComponentsContainerActor, MeshAttributes, bIsNew);

	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(Component))
	{
		if (!CVDGeometryComponent->IsMeshReady())
		{
			AddMeshComponentWaitingForGeometry(GeometryDataHandle->GetGeometryKey(), Component);
		}

		if (bIsNew)
		{
			CVDGeometryComponent->OnComponentEmpty()->AddRaw(this, &FChaosVDGeometryBuilder::DestroyMeshComponent);			
			CVDGeometryComponent->SetMeshComponentAttributeFlags(MeshAttributes);
		}	
	}

	return Component;
}

template <typename MeshType>
void FChaosVDGeometryBuilder::DispatchCreateAndCacheMeshForImplicitAsync(const uint32 GeometryKey, TSharedPtr<UE::Geometry::FMeshShapeGenerator> MeshGenerator, const int32 LODsToGenerateNum)
{
	{
		FWriteScopeLock WriteLock(GeometryCacheRWLock);
		if (GeometryBeingGeneratedByKey.Contains(GeometryKey))
		{
			return;
		}

		GeometryBeingGeneratedByKey.Add(GeometryKey);
	}
	
	TSharedPtr<FGeometryGenerationTask> GenerationTask = MakeShared<FGeometryGenerationTask>(AsWeak(), MeshGenerator, GeometryKey, LODsToGenerateNum);
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
		}
		else if constexpr (std::is_same_v<MeshType, UStaticMesh>)
		{
			BuilderPtr->CreateAndCacheStaticMesh(GeometryKey, *MeshGenerator.Get(), LODsToGenerateNum);
		}

		{
			FWriteScopeLock WriteLock(BuilderPtr->GeometryCacheRWLock);
			BuilderPtr->GeometryBeingGeneratedByKey.Remove(GeometryKey);
		}
	}
}
