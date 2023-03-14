// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MeshComponent.h"
#include "Chaos/Defines.h"
#include "Field/FieldSystem.h"
#include "Field/FieldSystemActor.h"
#include "Field/FieldSystemNodes.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "GeometryCollectionProxyData.h"
#include "GeometryCollectionObject.h"
#include "GeometryCollectionEditorSelection.h"
#include "GeometryCollection/RecordedTransformTrack.h"
#include "Templates/UniquePtr.h"
#include "Chaos/ChaosGameplayEventDispatcher.h"
#include "Chaos/ChaosNotifyHandlerInterface.h"
#include "Chaos/ChaosSolverComponentTypes.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "EngineDefines.h"
#include "Math/MathFwd.h"
#include "PhysxUserData.h"

#include "GeometryCollectionComponent.generated.h"

struct FGeometryCollectionConstantData;
struct FGeometryCollectionDynamicData;
class UGeometryCollectionComponent;
class UBoxComponent;
class UGeometryCollectionCache;
class UChaosPhysicalMaterial;
class AChaosSolverActor;
struct FGeometryCollectionEmbeddedExemplar;
class UInstancedStaticMeshComponent;
class FGeometryCollectionDecayDynamicFacade;
struct FGeometryCollectionDecayContext;
struct FDamageCollector;
class AGeometryCollectionISMPoolActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChaosBreakEvent, const FChaosBreakEvent&, BreakEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChaosRemovalEvent, const FChaosRemovalEvent&, RemovalEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChaosCrumblingEvent, const FChaosCrumblingEvent&, CrumbleEvent);

namespace GeometryCollection
{
	enum class ESelectionMode : uint8
	{
		None = 0,
		AllGeometry,
		InverseGeometry,
		Siblings,
		Neighbors,
		Parent,
		Children,
		Level,
		Leaves,
		Clusters
	};
}

USTRUCT()
struct FGeomComponentCacheParameters
{
	GENERATED_BODY()

	FGeomComponentCacheParameters();

	// Cache mode, whether disabled, playing or recording
	UPROPERTY(EditAnywhere, Category = Cache)
	EGeometryCollectionCacheType CacheMode;

	// The cache to target when recording or playing
	UPROPERTY(EditAnywhere, Category = Cache)
	TObjectPtr<UGeometryCollectionCache> TargetCache;

	// Cache mode, whether disabled, playing or recording
	UPROPERTY(EditAnywhere, Category = Cache)
	float ReverseCacheBeginTime;

	// Whether to buffer collisions during recording
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Record Collision Data"))
	bool SaveCollisionData;

	// Whether to generate collisions during playback
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Generate Collision Data during Playback"))
	bool DoGenerateCollisionData;

	// Maximum size of the collision buffer
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Collision Data Size Maximum"))
	int32 CollisionDataSizeMax;

	// Spatial hash collision data
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Spatial Hash Collision Data"))
	bool DoCollisionDataSpatialHash;

	// Spatial hash radius for collision data
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Spatial Hash Radius"))
	float CollisionDataSpatialHashRadius;

	// Maximum number of collisions per cell
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Maximum Number of Collisions Per Cell"))
	int32 MaxCollisionPerCell;

	// Whether to buffer breakings during recording
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Record Breaking Data"))
	bool SaveBreakingData;

	// Whether to generate breakings during playback
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Generate Breaking Data during Playback"))
	bool DoGenerateBreakingData;

	// Maximum size of the breaking buffer
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Breaking Data Size Maximum"))
	int32 BreakingDataSizeMax;

	// Spatial hash breaking data
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Spatial Hash Breaking Data"))
	bool DoBreakingDataSpatialHash;

	// Spatial hash radius for breaking data
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Spatial Hash Radius"))
	float BreakingDataSpatialHashRadius;

	// Maximum number of breaking per cell
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Maximum Number of Breakings Per Cell"))
	int32 MaxBreakingPerCell;

	// Whether to buffer trailings during recording
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Record Trailing Data"))	
	bool SaveTrailingData;

	// Whether to generate trailings during playback
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Generate Trailing Data during Playback"))
	bool DoGenerateTrailingData;

	// Maximum size of the trailing buffer
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Trailing Data Size Maximum"))
	int32 TrailingDataSizeMax;

	// Minimum speed to record trailing
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Trailing Minimum Speed Threshold"))
	float TrailingMinSpeedThreshold;

	// Minimum volume to record trailing
	UPROPERTY(EditAnywhere, Category = Cache, meta = (DisplayName = "Trailing Minimum Volume Threshold"))
	float TrailingMinVolumeThreshold;
};

namespace GeometryCollection
{
	/** Type of updates used at the end of an edit operation. */
	enum class EEditUpdate : uint8
	{
		/** No update. */
		None = 0,
		/** Mark the rest collection as changed. */
		Rest = 1 << 0,
		/** Recreate the physics state (proxy). */
		Physics = 1 << 1,
		/** Reset the dynamic collection. */
		Dynamic = 1 << 2,
		/** Mark the rest collection as changed, and recreate the physics state (proxy). */
		RestPhysics = Rest | Physics,
		/** Reset dynamic collection, mark the rest collection as changed, and recreate the physics state (proxy). */
		RestPhysicsDynamic = Rest | Physics | Dynamic,
	};
	ENUM_CLASS_FLAGS(EEditUpdate);
}

/**
*	FGeometryCollectionEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*
*	This will force any simulating geometry collection out of the
*	solver so it can be edited and afterwards will recreate the proxy
*	The update can also be specified to reset the dynamic collection
*/
class GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionEdit
{
public:
	/**
	 * @param InComponent				The component to edit
	 * @param EditUpdate				What parts of the geometry collection to update
	 * @param bShapeIsUnchanged			Override indicating the overall shape of the geometry and clusters is unchanged, even if the rest collection changed.  Useful to e.g., not re-compute convex hulls when we don't need to.
	 */
	FGeometryCollectionEdit(UGeometryCollectionComponent* InComponent, GeometryCollection::EEditUpdate EditUpdate = GeometryCollection::EEditUpdate::RestPhysics, bool bShapeIsUnchanged = false);
	~FGeometryCollectionEdit();

	UGeometryCollection* GetRestCollection();

private:
	UGeometryCollectionComponent* Component;
	const GeometryCollection::EEditUpdate EditUpdate;
	bool bHadPhysicsState;
	bool bShapeIsUnchanged;
};

#if WITH_EDITOR
class GEOMETRYCOLLECTIONENGINE_API FScopedColorEdit
{
public:
	FScopedColorEdit(UGeometryCollectionComponent* InComponent, bool bForceUpdate = false);
	~FScopedColorEdit();

	void SetShowBoneColors(bool ShowBoneColorsIn);
	bool GetShowBoneColors() const;

	void SetEnableBoneSelection(bool ShowSelectedBonesIn);
	bool GetEnableBoneSelection() const;

	bool IsBoneSelected(int BoneIndex) const;
	void Sanitize();
	void SetSelectedBones(const TArray<int32>& SelectedBonesIn);
	void AppendSelectedBones(const TArray<int32>& SelectedBonesIn);
	void ToggleSelectedBones(const TArray<int32>& SelectedBonesIn, bool bAdd, bool bSnapToLevel = true);
	void AddSelectedBone(int32 BoneIndex);
	void ClearSelectedBone(int32 BoneIndex);
	const TArray<int32>& GetSelectedBones() const;
	void ResetBoneSelection();
	void SelectBones(GeometryCollection::ESelectionMode SelectionMode);
	void FilterSelectionToLevel(bool bPreferLowestOnly = false);
	int32 GetMaxSelectedLevel(bool bOnlyRigid) const;
	bool IsSelectionValidAtLevel(int32 TargetLevel) const;

	bool IsBoneHighlighted(int BoneIndex) const;
	void SetHighlightedBones(const TArray<int32>& HighlightedBonesIn, bool bHighlightChildren = false);
	void AddHighlightedBone(int32 BoneIndex);
	const TArray<int32>& GetHighlightedBones() const;
	void ResetHighlightedBones();

	void SetLevelViewMode(int ViewLevel);
	int GetViewLevel();

private:
	void UpdateBoneColors();

	bool bUpdated;


	UGeometryCollectionComponent * Component;
	static TArray<FLinearColor> RandomColors;
};

#endif

//Provides copy on write functionality:
//GetArray (const access)
//GetArrayCopyOnWrite
//GetArrayRest (gives original rest value)
//This generates pointers to arrays marked private. Macro assumes getters are public
//todo(ocohen): may want to take in a static name
#define COPY_ON_WRITE_ATTRIBUTE(Type, Name, Group)								\
FORCEINLINE const TManagedArray<Type>& Get##Name##Array() const 				\
{																				\
	return Indirect##Name##Array ?												\
		*Indirect##Name##Array : RestCollection->GetGeometryCollection()->Name;	\
}																				\
FORCEINLINE TManagedArray<Type>& Get##Name##ArrayCopyOnWrite()					\
{																				\
	if(!Indirect##Name##Array)													\
	{																			\
		static FName StaticName(#Name);											\
		DynamicCollection->AddAttribute<Type>(StaticName, Group);				\
		DynamicCollection->CopyAttribute(										\
			*RestCollection->GetGeometryCollection(), StaticName, Group);		\
		Indirect##Name##Array =													\
			&DynamicCollection->ModifyAttribute<Type>(StaticName, Group);		\
		CopyOnWriteAttributeList.Add(											\
			reinterpret_cast<FManagedArrayBase**>(&Indirect##Name##Array));		\
	}																			\
	return *Indirect##Name##Array;												\
}																				\
FORCEINLINE void Reset##Name##ArrayDynamic()									\
{																				\
	Indirect##Name##Array = NULL;												\
}																				\
FORCEINLINE const TManagedArray<Type>& Get##Name##ArrayRest() const				\
{																				\
	return RestCollection->GetGeometryCollection()->Name;						\
}																				\
private:																		\
	TManagedArray<Type>* Indirect##Name##Array;									\
public:

/**
 * Raw struct to serialize for network. We need to custom netserialize to optimize
 * the vector serialize as much as possible and rather than have the property system
 * iterate an array of reflected structs we handle everything in the NetSerialize for
 * the container (FGeometryCollectionClusterRep)
 */
struct FGeometryCollectionClusterRep
{
	struct FClusterState
	{
		static constexpr uint8 StateMask = 0b111;
		static constexpr uint8 StateOffset = 0;
		static constexpr uint8 InternalClusterMask = 0b1;
		static constexpr uint8 InternalClusterOffset = 3;

		void SetMaskedValue(uint8 Val, uint8 Mask, uint8 Offset)
		{
			Value &= ~Mask;
			Value |= ((Val & Mask) << Offset);
		}
		
		uint8 GetMaskedValue(uint8 Mask, uint8 Offset) const
		{
			return (Value >> Offset) & Mask;
		}
		
		void SetObjectState(Chaos::EObjectStateType State)
		{
			SetMaskedValue((uint8)State, StateMask, StateOffset);
		}

		void SetInternalCluster(bool bInternalCluster)
		{
			SetMaskedValue(bInternalCluster?1:0, InternalClusterMask, InternalClusterOffset);
		}
		
		Chaos::EObjectStateType GetObjectState()
		{
			return (Chaos::EObjectStateType)GetMaskedValue(StateMask, StateOffset);
		}

		bool IsInternalCluster() const
		{
			return (bool)GetMaskedValue(InternalClusterMask, InternalClusterOffset);
		}
		
		uint8 Value = 0;
	};
	
	FVector_NetQuantize100 Position;
	FVector_NetQuantize100 LinearVelocity;
	FVector_NetQuantize100 AngularVelocity;
	FQuat Rotation;
	uint16 ClusterIdx;		  // index of the cluster or one of its child if the cluster is internal ( see ClusterState)
	FClusterState ClusterState;

	bool ClusterChanged(const FGeometryCollectionClusterRep& Other) const
	{
		return Other.ClusterState.Value != ClusterState.Value
			|| Other.ClusterIdx != ClusterIdx
			|| Other.Position != Position
			|| Other.LinearVelocity != LinearVelocity
			|| Other.AngularVelocity != AngularVelocity
			|| Other.Rotation != Rotation;
	}
};

struct FGeometryCollectionActivatedCluster
{
	FGeometryCollectionActivatedCluster() = default;
	FGeometryCollectionActivatedCluster(uint16 Index, const FVector& InitialLinearVel, const FVector& InitialAngularVel )
		: ActivatedIndex(Index)
		, InitialLinearVelocity(InitialLinearVel)
		, InitialAngularVelocity(InitialAngularVel)
	{}

	uint16 ActivatedIndex;
	FVector_NetQuantize100 InitialLinearVelocity;
	FVector_NetQuantize100 InitialAngularVelocity;

	bool operator==(const FGeometryCollectionActivatedCluster& Other) const
	{
		return ActivatedIndex == Other.ActivatedIndex;
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FGeometryCollectionActivatedCluster& ActivatedCluster)
{
	Ar << ActivatedCluster.ActivatedIndex;
	Ar << ActivatedCluster.InitialLinearVelocity;
	Ar << ActivatedCluster.InitialAngularVelocity;

	return Ar;
}

/**
 * Replicated data for a geometry collection when bEnableReplication is true for
 * that component. See UGeomtryCollectionComponent::UpdateRepData
 */
USTRUCT()
struct FGeometryCollectionRepData
{
	GENERATED_BODY()

	FGeometryCollectionRepData()
		: Version(0)
	{

	}

	//Array of one off pieces that became activated
	TArray<FGeometryCollectionActivatedCluster> OneOffActivated;

	// Array of cluster data requires to synchronize clients
	TArray<FGeometryCollectionClusterRep> Clusters;

	// Version counter, every write to the rep data is a new state so Identical only references this version
	// as there's no reason to compare the Poses array.
	int32 Version;

	// Just test version to skip having to traverse the whole pose array for replication
	bool Identical(const FGeometryCollectionRepData* Other, uint32 PortFlags) const;
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

	void Reset()
	{
		OneOffActivated.Reset();
		Clusters.Reset();
	}

};

template<>
struct TStructOpsTypeTraits<FGeometryCollectionRepData> : public TStructOpsTypeTraitsBase2<FGeometryCollectionRepData>
{
	enum
	{
		WithNetSerializer = true,
		WithIdentical = true,
	};
};

/**
*	GeometryCollectionComponent
*/
UCLASS(meta = (BlueprintSpawnableComponent))
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollectionComponent : public UMeshComponent, public IChaosNotifyHandlerInterface
{
	GENERATED_UCLASS_BODY()
	friend class FGeometryCollectionEdit;
#if WITH_EDITOR
	friend class FScopedColorEdit;
#endif
	friend class FGeometryCollectionCommands;

public:

	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderDynamicData_Concurrent() override;
	FORCEINLINE void SetRenderStateDirty() { bRenderStateDirty = true; }
	virtual void OnActorEnableCollisionChanged() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type ReasonEnd) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void InitializeComponent() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	DECLARE_MULTICAST_DELEGATE(FOnGeometryCollectionPropertyChangedMulticaster)
	FOnGeometryCollectionPropertyChangedMulticaster OnGeometryCollectionPropertyChanged;
	typedef FOnGeometryCollectionPropertyChangedMulticaster::FDelegate FOnGeometryCollectionPropertyChanged;

	/** Register / Unregister delegates called when the skeletal mesh property is changed */
	FDelegateHandle RegisterOnGeometryCollectionPropertyChanged(const FOnGeometryCollectionPropertyChanged& Delegate);
	void UnregisterOnGeometryCollectionPropertyChanged(FDelegateHandle Handle);
#endif
	//~ Begin UActorComponent Interface. 


	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FBoxSphereBounds CalcLocalBounds() const { return LocalBounds; }

	virtual bool HasAnySockets() const override;
	virtual bool DoesSocketExist(FName InSocketName) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	virtual void QuerySupportedSockets(TArray<FComponentSocketDescription>& OutSockets) const override;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void AsyncPhysicsTickComponent(float DeltaTime, float SimTime) override;
	//~ Begin USceneComponent Interface.


	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void OnRegister() override;
	virtual FBodyInstance* GetBodyInstance(FName BoneName = NAME_None, bool bGetWelded = true, int32 Index = -1) const override;
	virtual void SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision) override;
	virtual bool CanEditSimulatePhysics() override;
	virtual void SetSimulatePhysics(bool bEnabled) override;
	virtual void AddForce(FVector Force, FName BoneName = NAME_None, bool bAccelChange = false) override;
	virtual void AddForceAtLocation(FVector Force, FVector WorldLocation, FName BoneName = NAME_None) override;
	virtual void AddImpulse(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false) override;
	virtual void AddImpulseAtLocation(FVector Impulse, FVector WorldLocation, FName BoneName = NAME_None) override;
	virtual void AddRadialForce(FVector Origin, float Radius, float Strength, ERadialImpulseFalloff Falloff, bool bAccelChange = false) override;
	virtual void AddRadialImpulse(FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange = false) override;
	virtual void AddTorqueInRadians(FVector Torque, FName BoneName = NAME_None, bool bAccelChange = false) override;
	virtual void PostLoad() override;
	//~ End UPrimitiveComponent Interface.


	//~ Begin UMeshComponent Interface.	
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	//~ End UMeshComponent Interface.

	/** Chaos RBD Solver override. Will use the world's default solver actor if null. */
	UPROPERTY(EditAnywhere, Category = "ChaosPhysics", meta = (DisplayName = "Chaos Solver"))
	TObjectPtr<AChaosSolverActor> ChaosSolverActor;

	/**
	 * Apply an external strain to specific piece of the geometry collection
	 * @param ItemIndex item index ( from HitResult) of the piece to apply strain on
	 * @param Location world location of where to apply the strain
	 * @param Radius radius from the location point to apply the strain to ( using the center of mass of the pieces )
	 * @param PropagationDepth How many level of connection to follow to propagate the strain through
	 * @param PropagationFactor when using propagation, the factor to multiply the strain from one level to the other, allowing falloff effect
	 * @param Strain strain / damage to apply 
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyExternalStrain(int32 ItemIndex, const FVector& Location, float Radius = 0, int32 PropagationDepth = 0, float PropagationFactor = 1, float Strain = 0);

	/**
	 * Apply an internal strain to specific piece of the geometry collection
	 * @param ItemIndex item index ( from HitResult) of the piece to apply strain on
	 * @param Location world location of where to apply the strain
	 * @param Radius radius from the location point to apply the strain to ( using the center of mass of the pieces )
	 * @param PropagationDepth How many level of connection to follow to propagate the strain through
	 * @param PropagationFactor when using propagation, the factor to multiply the strain from one level to the other, allowing falloff effect
	 * @param Strain strain / damage to apply 
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyInternalStrain(int32 ItemIndex, const FVector& Location, float Radius = 0, int32 PropagationDepth = 0, float PropagationFactor = 1, float Strain = 0);
	
	/**
	 * Crumbe a cluster into all its pieces
	 * @param ItemIndex item index ( from HitResult) of the cluster to crumble
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void CrumbleCluster(int32 ItemIndex);

	/**
	* Crumbe active clusters for this entire geometry collection
	* this will apply to internal and regular clusters
	*/
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void CrumbleActiveClusters();

	/**
	* this will remove anchors on all the pieces ( inlcuding the static and kinematic initial states ones ) of the geometry colection
	*/
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void RemoveAllAnchors();

	/**
	 * Apply linear velocity on breaking pieces for a specific cluster
	 * If ItemIndex does not represent a cluster this will do nothing  
	 * @param ItemIndex item index ( from HitResult) of the cluster owning the breaking pieces to apply velocity on
	 * @param LinearVelocity linear velocity to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyBreakingLinearVelocity(int32 ItemIndex, const FVector& LinearVelocity);

	/**
	 * Apply linear velocity on breaking pieces for a specific cluster
	 * If ItemIndex does not represent a cluster this will do nothing  
	 * @param ItemIndex item index ( from HitResult) of the cluster owning the breaking pieces to apply velocity on
	 * @param AngularVelocity linear velocity to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyBreakingAngularVelocity(int32 ItemIndex, const FVector& AngularVelocity);
	
	/**
	 * Apply linear velocity on specific piece 
	 * @param ItemIndex item index ( from HitResult) of the piece to apply velocity on
	* @param LinearVelocity linear velocity to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyLinearVelocity(int32 ItemIndex, const FVector& LinearVelocity);

	/**
	 * Apply angular velocity on specific piece 
	 * @param ItemIndex item index ( from HitResult) of the piece to apply velocity on
	* @param AngularVelocity linear velocity to apply
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void ApplyAngularVelocity(int32 ItemIndex, const FVector& AngularVelocity);

	/**
	 * Get the initial level of a specific piece
	 * Initial level means the level as it is in the unbroken state 
	 * @param ItemIndex item index ( from HitResult) of the cluster to get level from
	 * @param Level of the piece ( 0 for root level and positive for the rest ) 
	 */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	int32 GetInitialLevel(int32 ItemIndex);
	
	/**
	* Get mass and extent of a specific piece
	* @param ItemIndex item index ( from HitResult) of the cluster to get level from
	* @param Level of the piece ( 0 for root level and positive for the rest )
	*/
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void GetMassAndExtents(int32 ItemIndex, float& OutMass, FBox& OutExtents);

	/** RestCollection */
	UFUNCTION(BlueprintCallable, Category = "ChaosPhysics")
	void SetRestCollection(const UGeometryCollection * RestCollectionIn);


	FORCEINLINE const UGeometryCollection* GetRestCollection() const { return RestCollection; }

	FORCEINLINE FGeometryCollectionEdit EditRestCollection(GeometryCollection::EEditUpdate EditUpdate = GeometryCollection::EEditUpdate::RestPhysics, bool bShapeIsUnchanged = false) { return FGeometryCollectionEdit(this, EditUpdate, bShapeIsUnchanged); }
#if WITH_EDITOR
	FORCEINLINE FScopedColorEdit EditBoneSelection(bool bForceUpdate = false) { return FScopedColorEdit(this, bForceUpdate); }

	/** Propagate bone selection to embedded geometry components. */
	void SelectEmbeddedGeometry();
#endif

	/** API for getting at geometry collection data */
	FORCEINLINE int32 GetNumElements(FName Group) const
	{
		int32 Size = RestCollection->NumElements(Group);	//assume rest collection has the group and is connected to dynamic.
		return Size > 0 ? Size : DynamicCollection->NumElements(Group);	//if not, maybe dynamic has the group
	}

	// Update cached bounds; used e.g. when updating the exploded view of the geometry collection
	void UpdateCachedBounds();

	// Vertices Group
	COPY_ON_WRITE_ATTRIBUTE(FVector3f, Vertex, FGeometryCollection::VerticesGroup) 	//GetVertexArray, GetVertexArrayCopyOnWrite, GetVertexArrayRest
	COPY_ON_WRITE_ATTRIBUTE(TArray<FVector2f>, UVs, FGeometryCollection::VerticesGroup)		//GetUVsArray
	COPY_ON_WRITE_ATTRIBUTE(FLinearColor, Color, FGeometryCollection::VerticesGroup)//GetColorArray
	COPY_ON_WRITE_ATTRIBUTE(FVector3f, TangentU, FGeometryCollection::VerticesGroup)	//GetTangentUArray
	COPY_ON_WRITE_ATTRIBUTE(FVector3f, TangentV, FGeometryCollection::VerticesGroup)	//...
	COPY_ON_WRITE_ATTRIBUTE(FVector3f, Normal, FGeometryCollection::VerticesGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, BoneMap, FGeometryCollection::VerticesGroup)

	// Faces Group
	COPY_ON_WRITE_ATTRIBUTE(FIntVector, Indices, FGeometryCollection::FacesGroup)
	COPY_ON_WRITE_ATTRIBUTE(bool, Visible, FGeometryCollection::FacesGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, MaterialIndex, FGeometryCollection::FacesGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, MaterialID, FGeometryCollection::FacesGroup)

	// Geometry Group
	COPY_ON_WRITE_ATTRIBUTE(int32, TransformIndex, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(FBox, BoundingBox, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(float, InnerRadius, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(float, OuterRadius, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, VertexStart, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, VertexCount, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, FaceStart, FGeometryCollection::GeometryGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, FaceCount, FGeometryCollection::GeometryGroup)

	// Material Group
	COPY_ON_WRITE_ATTRIBUTE(FGeometryCollectionSection, Sections, FGeometryCollection::MaterialGroup)

	// Transform group
	COPY_ON_WRITE_ATTRIBUTE(FString, BoneName, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(FLinearColor, BoneColor, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(FTransform, Transform, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, Parent, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(TSet<int32>, Children, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, SimulationType, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, TransformToGeometryIndex, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, StatusFlags, FTransformCollection::TransformGroup)
	COPY_ON_WRITE_ATTRIBUTE(int32, ExemplarIndex, FTransformCollection::TransformGroup)


	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = "ChaosPhysics")
	TObjectPtr<const UGeometryCollection> RestCollection;

	UPROPERTY(EditAnywhere, NoClear, BlueprintReadOnly, Category = "ChaosPhysics")
	TArray<TObjectPtr<const AFieldSystemActor>> InitializationFields;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "GeometryCollection now abides the bSimulatePhysics flag from the base class."))
	bool Simulating_DEPRECATED;

	ESimulationInitializationState InitializationState;

	/** ObjectType defines how to initialize the rigid objects state, Kinematic, Sleeping, Dynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	EObjectStateTypeEnum ObjectType;

	/** If ForceMotionBlur is on, motion blur will always be active, even if the GeometryCollection is at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	bool bForceMotionBlur;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	bool EnableClustering;

	/** Maximum level for cluster breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	int32 ClusterGroupIndex;

	/** Maximum level for cluster breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Clustering")
	int32 MaxClusterLevel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Damage", meta = (EditCondition = "!bUseSizeSpecificDamageThreshold"))
	TArray<float> DamageThreshold;

	/** Damage threshold for clusters at different levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Damage")
	bool bUseSizeSpecificDamageThreshold;

	/** Data about how damage propagation shoudl behave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Damage")
	FGeometryCollectionDamagePropagationData DamagePropagationData;

	/** Allow removal on sleep for the instance if the rest collection has it enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Removal")
	bool bAllowRemovalOnSleep;

	/** Allow removal on break for the instance if the rest collection has it enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Removal")
	bool bAllowRemovalOnBreak;
	
	/** */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Connection types are defined on the asset now."))
	EClusterConnectionTypeEnum ClusterConnectionType_DEPRECATED;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	int32 CollisionGroup;

	/** Fraction of collision sample particles to keep */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Collisions")
	float CollisionSampleFraction;

	/** Uniform linear ether drag. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use PhysicalMaterial instead."))
	float LinearEtherDrag_DEPRECATED;

	/** Uniform angular ether drag. */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Use PhysicalMaterial instead."))
	float AngularEtherDrag_DEPRECATED;

	/** Physical Properties */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Physical material now derived from render materials, for instance overrides use PhysicalMaterialOverride."))
	TObjectPtr<const UChaosPhysicalMaterial> PhysicalMaterial_DEPRECATED;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	EInitialVelocityTypeEnum InitialVelocityType;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialLinearVelocity;

	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|Initial Velocity")
	FVector InitialAngularVelocity;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Physical material now derived from render materials, for instance overrides use Colliisons PhysicalMaterialOverride."))
	TObjectPtr<UPhysicalMaterial> PhysicalMaterialOverride_DEPRECATED;

	UPROPERTY()
	FGeomComponentCacheParameters CacheParameters;

	/** Optional transforms to initialize scene proxy if difference from the RestCollection. */
	UPROPERTY()
	TArray<FTransform> RestTransforms;

	/**
	*  SetDynamicState
	*    This function will dispatch a command to the physics thread to apply
	*    a kinematic to dynamic state change for the geo collection particles within the field.
	*
	*	 @param Radius Radial influence from the position
	*    @param Position The location of the command
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "Field", DisplayName = "Set Dynamic State")
	void ApplyKinematicField(UPARAM(DisplayName = "Field Radius") float Radius, 
							 UPARAM(DisplayName = "Center Position") FVector Position);

	/**
	*  AddPhysicsField
	*    This function will dispatch a command to the physics thread to apply
	*    a generic evaluation of a user defined transient field network. See documentation,
	*    for examples of how to recreate variations of the above generic
	*    fields using field networks
	*
	*	 @param Enabled Is this force enabled for evaluation.
	*    @param Target Type of field supported by the solver.
	*    @param MetaData Meta data used to assist in evaluation
	*    @param Field Base evaluation node for the field network.
	*
	*/
	UFUNCTION(BlueprintCallable, Category = "Field", DisplayName = "Add Physics Field")
	void ApplyPhysicsField(UPARAM(DisplayName = "Enable Field") bool Enabled, 
						   UPARAM(DisplayName = "Physics Type") EGeometryCollectionPhysicsTypeEnum Target, 
						   UPARAM(DisplayName = "Meta Data") UFieldSystemMetaData* MetaData, 
						   UPARAM(DisplayName = "Field Node") UFieldNodeBase* Field);

	/**
	* Blueprint event
	*/

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNotifyGeometryCollectionPhysicsStateChange, UGeometryCollectionComponent*, FracturedComponent);

	UPROPERTY(BlueprintAssignable, Category = "Game|Damage")
	FNotifyGeometryCollectionPhysicsStateChange NotifyGeometryCollectionPhysicsStateChange;

	bool GetIsObjectDynamic() const;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNotifyGeometryCollectionPhysicsLoadingStateChange, UGeometryCollectionComponent*, FracturedComponent);
	UPROPERTY(BlueprintAssignable, Category = "Game|Loading")
	FNotifyGeometryCollectionPhysicsLoadingStateChange NotifyGeometryCollectionPhysicsLoadingStateChange;
	
	bool GetIsObjectLoading() { return IsObjectLoading; }

	/**
	*
	*/
	/* ---------------------------------------------------------------------------------------- */
	
	bool GetNotifyTrailing() const { return bNotifyTrailing; }

	void SetShowBoneColors(bool ShowBoneColorsIn) { bShowBoneColors = ShowBoneColorsIn; }
	bool GetShowBoneColors() const { return bShowBoneColors; }
	bool GetEnableBoneSelection() const { return bEnableBoneSelection; }
	
	bool GetSuppressSelectionMaterial() const { return RestCollection->GetGeometryCollection()->HasAttribute("Hide", FGeometryCollection::TransformGroup); }
	
	FORCEINLINE const int GetBoneSelectedMaterialID() const { return RestCollection->GetBoneSelectedMaterialIndex(); }
	
#if WITH_EDITORONLY_DATA
	FORCEINLINE const TArray<int32>& GetSelectedBones() const { return SelectedBones; }
	FORCEINLINE const TArray<int32>& GetHighlightedBones() const { return HighlightedBones; }
#endif

	FPhysScene_Chaos* GetInnerChaosScene() const;
	AChaosSolverActor* GetPhysicsSolverActor() const;

	const FGeometryCollectionPhysicsProxy* GetPhysicsProxy() const { return PhysicsProxy; }
	FGeometryCollectionPhysicsProxy* GetPhysicsProxy() { return PhysicsProxy; }

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	/** Enable/disable the scene proxy per transform selection mode. When disabled the per material id default selection is used instead. */
	void EnableTransformSelectionMode(bool bEnable);
#endif  // #if GEOMETRYCOLLECTION_EDITOR_SELECTION

#if UE_ENABLE_DEBUG_DRAWING
	/** Force render after constant data changes (such as visibility, or hitproxy subsections). Will also work while paused. */
	void ForceRenderUpdateConstantData() { MarkRenderStateDirty(); }

	/** Force render after dynamic data changes (such as transforms). Will also work while paused. */
	void ForceRenderUpdateDynamicData() { MarkRenderDynamicDataDirty(); }
#endif  // UE_ENABLE_DEBUG_DRAWING

	/**/
	const TArray<bool>& GetDisabledFlags() const { return DisabledFlags; }

	virtual void OnCreatePhysicsState() override;
	void RegisterAndInitializePhysicsProxy();
	virtual void OnDestroyPhysicsState() override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

	// Mirrored from the proxy on a sync
	//TManagedArray<int32> RigidBodyIds;
	TArray<bool> DisabledFlags;
	int32 BaseRigidBodyIndex;
	int32 NumParticlesAdded;

	/** Changes whether or not this component will get future break notifications. */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void SetNotifyBreaks(bool bNewNotifyBreaks);

	/** Changes whether or not this component will get future removal notifications. */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void SetNotifyRemovals(bool bNewNotifyRemovals);

	/** Changes whether or not this component will get future break notifications. */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	void SetNotifyCrumblings(bool bNewNotifyCrumblings);
	
	/** Overrideable native notification */
	virtual void NotifyBreak(const FChaosBreakEvent& Event) {};

	/** Overrideable native notification */
	virtual void NotifyRemoval(const FChaosRemovalEvent& Event) {};

	UPROPERTY(BlueprintAssignable, Category = "Chaos")
	FOnChaosBreakEvent OnChaosBreakEvent;

	UPROPERTY(BlueprintAssignable, Category = "Chaos")
	FOnChaosRemovalEvent OnChaosRemovalEvent;

	UPROPERTY(BlueprintAssignable, Category = "Chaos")
	FOnChaosCrumblingEvent OnChaosCrumblingEvent;

	void DispatchBreakEvent(const FChaosBreakEvent& Event);

	void DispatchRemovalEvent(const FChaosRemovalEvent& Event);

	void DispatchCrumblingEvent(const FChaosCrumblingEvent& Event);

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadWrite, Interp, Category = "Chaos")
	float DesiredCacheTime;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadWrite, Category = "Chaos")
	bool CachePlayback;

	bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;

	/** Gets the physical material to use for this geometry collection, taking into account instance overrides and render materials */
	UPhysicalMaterial* GetPhysicalMaterial() const;

	/** Update component structure to reflect any changes to the embedded geometry */
	void InitializeEmbeddedGeometry();
	
	/** Update instanced static mesh components to reflect internal embedded geometry state. */
	void RefreshEmbeddedGeometry();

#if WITH_EDITOR
	void SetEmbeddedGeometrySelectable(bool bSelectableIn);
	int32 EmbeddedIndexToTransformIndex(const UInstancedStaticMeshComponent* ISMComponent, int32 InstanceIndex) const;
#endif

#if WITH_EDITORONLY_DATA
	const FDamageCollector* GetRunTimeDataCollector() const;
#endif
	
	// #todo should this only be available in editor?
	void SetRestState(TArray<FTransform>&& InRestTransforms);

	/** Set the dynamic state for all bodies in the DynamicCollection. */
	void SetDynamicState(const Chaos::EObjectStateType& NewDynamicState);

	/** Set transforms for all bodies in the DynamicCollection. */
	void SetInitialTransforms(const TArray<FTransform>& InitialTransforms);

	/** Modify DynamicCollection transform hierarchy to effect cluster breaks releasing the specified indices. */
	void SetInitialClusterBreaks(const TArray<int32>& ReleaseIndices);

	/** Used by Niagara DI to query global matrices rather than recalculating them again */
	const TArray<FMatrix>& GetGlobalMatrices() { return GlobalMatrices; }

	const FGeometryDynamicCollection* GetDynamicCollection() const { return DynamicCollection.Get(); }
	FGeometryDynamicCollection* GetDynamicCollection() { return DynamicCollection.Get(); } // TEMP HACK?

public:
	UPROPERTY(BlueprintAssignable, Category = "Collision")
	FOnChaosPhysicsCollision OnChaosPhysicsCollision;
	
	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Physics Collision"), Category = "Collision")
	void ReceivePhysicsCollision(const FChaosPhysicsCollisionInfo& CollisionInfo);

	// IChaosNotifyHandlerInterface
	virtual void DispatchChaosPhysicsCollisionBlueprintEvents(const FChaosPhysicsCollisionInfo& CollisionInfo) override;
	
	/** If true, this component will generate breaking events that other systems may subscribe to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bNotifyBreaks;

	/** If true, this component will generate collision events that other systems may subscribe to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bNotifyCollisions;

	/** If true, this component will generate trailing events that other systems may subscribe to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bNotifyTrailing;

	/** If true, this component will generate removal events that other systems may subscribe to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bNotifyRemovals;

	/** If true, this component will generate crumbling events that other systems may subscribe to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bNotifyCrumblings;

	/** If this and bNotifyCrumblings are true, the crumbling events will contain released children indices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General", meta = (EditCondition = "bNotifyCrumblings"))
	bool bCrumblingEventIncludesChildren;
	
	/** If true, this component will save linear and angular velocities on its DynamicCollection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChaosPhysics|General")
	bool bStoreVelocities;

protected:
	/** Display Bone Colors instead of assigned materials */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ChaosPhysics|General")
	bool bShowBoneColors;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "ChaosPhysics|General")
	bool bEnableRunTimeDataCollection;
	
	UPROPERTY(BlueprintReadOnly, Category = "ChaosPhysics|General")
	FGuid RunTimeDataCollectionGuid;
#endif
	
	/** ISM pool to use to render the geometry collection - only works for unfractured geometry collections  */
	UPROPERTY(EditAnywhere, Category = "ChaosPhysics|Rendering", meta = (DisplayName = "ISM Pool"))
	TObjectPtr<AGeometryCollectionISMPoolActor> ISMPool;

	int32 ISMPoolMeshGroupIndex = INDEX_NONE;

	/** Populate the static geometry structures for the render thread. */
	void InitConstantData(FGeometryCollectionConstantData* ConstantData) const;

	/** Populate the dynamic particle data for the render thread. */
	FGeometryCollectionDynamicData* InitDynamicData(bool bInitialization);

	/** Reset the dynamic collection from the current rest state. */
	void ResetDynamicCollection();

	/** Combine the commands from the input field assets */
	void GetInitializationCommands(TArray<FFieldSystemCommand>& CombinedCommmands);

	/** Issue a field command for the physics thread */
	void DispatchFieldCommand(const FFieldSystemCommand& InCommand);

	void CalculateLocalBounds();
	void CalculateGlobalMatrices();
	FBox ComputeBounds(const FMatrix& LocalToWorldWithScale) const;

	void RegisterForEvents();
	void UpdateRBCollisionEventRegistration();
	void UpdateBreakEventRegistration();
	void UpdateRemovalEventRegistration();
	void UpdateCrumblingEventRegistration();
	
	/* Per-instance override to enable/disable replication for the geometry collection */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Network)
	bool bEnableReplication;

	/** 
	 * Enables use of ReplicationAbandonAfterLevel to stop providing network updates to
	 * clients when the updated particle is of a level higher then specified.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Network)
	bool bEnableAbandonAfterLevel;

	/**
	 * If replicating - the cluster level to stop sending corrections for geometry collection chunks.
	 * recommended for smaller leaf levels when the size of the objects means they are no longer
	 * gameplay relevant to cut down on required bandwidth to update a collection.
	 * @see bEnableAbandonAfterLevel
	 */ 
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "GeometryCollection now uses ReplicationAbandonAfterLevel instead of ReplicationAbandonClusterLevel."))
	int32 ReplicationAbandonClusterLevel_DEPRECATED;

	/**
	* If replicating - the cluster level after which replication will not happen 
	* @see bEnableAbandonAfterLevel
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Network)
	int32 ReplicationAbandonAfterLevel;

	UPROPERTY(Replicated)
	FGeometryCollectionRepData RepData;

	/** Called post solve to allow authoritative components to update their replication data */
	void UpdateRepData();

	/** Clear all rep data, this is required if the physics proxy has been recreated */
	void ResetRepData();

private:

	bool bRenderStateDirty;
	bool bEnableBoneSelection;
	int ViewLevel;

	uint32 NavmeshInvalidationTimeSliceIndex;
	bool IsObjectDynamic;
	bool IsObjectLoading;

	FCollisionFilterData InitialSimFilter;
	FCollisionFilterData InitialQueryFilter;
	FPhysxUserData PhysicsUserData;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<int32> SelectedBones;

	UPROPERTY(Transient)
	TArray<int32> HighlightedBones;
#endif

	TArray<FMatrix> GlobalMatrices;
	FBox LocalBounds;

	mutable FBoxSphereBounds ComponentSpaceBounds;

	float CurrentCacheTime;
	TArray<bool> EventsPlayed;

	FGeometryCollectionPhysicsProxy* PhysicsProxy;
	TUniquePtr<FGeometryDynamicCollection> DynamicCollection;
	TArray<FManagedArrayBase**> CopyOnWriteAttributeList;

	// Temporary dummies to interface with Physx expectations of the SQ syatem
	friend class FGeometryCollectionSQAccelerator;
	FBodyInstance DummyBodyInstance;

	// Temporary storage for body setup in order to initialise a dummy body instance
	UPROPERTY(Transient)
	TObjectPtr<UBodySetup> DummyBodySetup;

#if WITH_EDITORONLY_DATA
	// Tracked editor actor that owns the original component so we can write back recorded caches
	// from PIE.
	UPROPERTY(Transient)
	TObjectPtr<AActor> EditorActor;
#endif
	void SwitchRenderModels(const AActor* Actor);

	UChaosGameplayEventDispatcher* EventDispatcher;

#if GEOMETRYCOLLECTION_EDITOR_SELECTION
	bool bIsTransformSelectionModeEnabled;
#endif  // #if GEOMETRYCOLLECTION_EDITOR_SELECTION

	/** The information of all the embedded instanced static meshes */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> EmbeddedGeometryComponents;

#if WITH_EDITORONLY_DATA
	TArray<TArray<int32>> EmbeddedBoneMaps;
	TArray<int32> EmbeddedInstanceIndex;
#endif

	bool IsEmbeddedGeometryValid() const;
	void ClearEmbeddedGeometry();

	/** return true is an ISM pool is set and the feature is enabled */
	bool CanUseISMPool() const;

	/** If an ISM pool is set, register to it  */
	void RegisterToISMPool();

	/** If an ISM pool is set, unregister to it  */
	void UnregisterFromISMPool();

	/** update ISM transforms */
	void RefreshISMPoolInstances();


	void IncrementSleepTimer(float DeltaTime);
	void IncrementBreakTimer(float DeltaTime);
	bool CalculateInnerSphere(int32 TransformIndex, UE::Math::TSphere<double>& SphereOut) const;
	void UpdateDecay(int32 TransformIdx, float UpdatedDecay, bool UseClusterCrumbling, bool HasDynamicInternalClusterParent, FGeometryCollectionDecayContext& ContextInOut);
	void ProcessRepData();

	void UpdateAttachedChildrenTransform() const;
	
	void BuildInitialFilterData();

	/** The clusters we need to replicate */
	TUniquePtr<TSet<Chaos::FPBDRigidClusteredParticleHandle*>> ClustersToRep;

	/** One off activation is processed in the same order as server so remember the last one we processed */
	int32 OneOffActivatedProcessed = 0;
	int32 VersionProcessed = INDEX_NONE;
	double LastHardsnapTimeInMs = 0;

	/** True if GeometryCollection transforms have changed from previous tick. */
	bool bIsMoving;
};
