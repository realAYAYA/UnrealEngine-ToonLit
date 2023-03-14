// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Crc.h"
#include "Rendering/NaniteResources.h"
#include "InstanceUniformShaderParameters.h"
#include "GeometryCollection/ManagedArray.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Chaos/ChaosSolverActor.h"

#include "GeometryCollectionObject.generated.h"

class UMaterialInterface;
class UGeometryCollectionCache;
class FGeometryCollection;
struct FManagedArrayCollection;
struct FGeometryCollectionSection;
struct FSharedSimulationParameters;
class UDataflow;

USTRUCT(BlueprintType)
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionSource
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource", meta=(AllowedClasses="/Script/Engine.StaticMesh, /Script/Engine.SkeletalMesh, /Script/GeometryCollectionEngine.GeometryCollection"))
	FSoftObjectPath SourceGeometryObject;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource")
	FTransform LocalTransform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource")
	TArray<TObjectPtr<UMaterialInterface>> SourceMaterial;

	/** Whether source materials should be duplicated to create slots for internal materials. Does not apply if the source is a GeometryCollection. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource")
	bool bAddInternalMaterials = true;

	/** Whether individual source mesh components should be split into separate pieces of geometry based on mesh connectivity. If checked, triangles that are not topologically connected will be assigned separate bones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource")
	bool bSplitComponents = false;

	// TODO: add primtive custom data
};

USTRUCT(BlueprintType)
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionAutoInstanceMesh
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AutoInstance", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath StaticMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AutoInstance")
	TArray<TObjectPtr<UMaterialInterface>> Materials;
};

USTRUCT(BlueprintType)
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionEmbeddedExemplar
{
	GENERATED_BODY()

	FGeometryCollectionEmbeddedExemplar() 
		: StaticMeshExemplar(FString(TEXT("None")))
		, StartCullDistance(0.0f)
		, EndCullDistance(0.0f)
		, InstanceCount(0)
	{ };
	
	FGeometryCollectionEmbeddedExemplar(FSoftObjectPath NewExemplar)
		: StaticMeshExemplar(NewExemplar)
		, StartCullDistance(0.0f)
		, EndCullDistance(0.0f)
		, InstanceCount(0)
	{ }

	UPROPERTY(EditAnywhere, Category = "EmbeddedExemplar", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath StaticMeshExemplar;

	UPROPERTY(EditAnywhere, Category = "EmbeddedExemplar")
	float StartCullDistance;

	UPROPERTY(EditAnywhere, Category = "EmbeddedExemplar")
	float EndCullDistance;

	UPROPERTY(VisibleAnywhere, Category = "EmbeddedExemplar")
	int32 InstanceCount;
};

USTRUCT()
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionLevelSetData
{
	GENERATED_BODY()

	FGeometryCollectionLevelSetData();

	/*
	*  Resolution on the smallest axes for the level set. (def: 5)
	*/
	UPROPERTY(EditAnywhere, Category = "LevelSet")
	int32 MinLevelSetResolution;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(EditAnywhere, Category = "LevelSet")
	int32 MaxLevelSetResolution;

	/*
	*  Resolution on the smallest axes for the level set. (def: 5)
	*/
	UPROPERTY(EditAnywhere, Category = "LevelSet")
	int32 MinClusterLevelSetResolution;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(EditAnywhere, Category = "LevelSet")
	int32 MaxClusterLevelSetResolution;
};


USTRUCT()
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionCollisionParticleData
{
	GENERATED_BODY()

	FGeometryCollectionCollisionParticleData();

	/**
	 * Number of particles on the triangulated surface to use for collisions.
	 */
	UPROPERTY(EditAnywhere, Category = "Particle")
	float CollisionParticlesFraction;

	/**
	 * Max number of particles.
	 */
	UPROPERTY(EditAnywhere, Category = "Particle")
	int32 MaximumCollisionParticles;
};



USTRUCT()
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionCollisionTypeData
{
	GENERATED_BODY()

	FGeometryCollectionCollisionTypeData();

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	ECollisionTypeEnum CollisionType;

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	EImplicitTypeEnum ImplicitType;

	/*
	*  LevelSet Resolution data for rasterization.
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions", meta = (EditCondition = "ImplicitType == EImplicitTypeEnum::Chaos_Implicit_LevelSet", EditConditionHides))
	FGeometryCollectionLevelSetData LevelSet;

	/*
	*  Collision Particle data for surface samples during Particle-LevelSet collisions.
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions", meta = (EditCondition = "CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric", EditConditionHides))
	FGeometryCollectionCollisionParticleData CollisionParticles;

	/*
	*  Uniform scale on the collision body. (def: 0)
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	float CollisionObjectReductionPercentage;

	/**
	* A collision margin is a fraction of size used by some boxes and convex shapes to improve collision detection results.
	* The core geometry of shapes that support a margin are reduced in size by the margin, and the margin
	* is added back on during collision detection. The net result is a shape of the same size but with rounded corners.
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions", meta = (EditCondition = "ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Convex || ImplicitType == EImplicitTypeEnum::Chaos_Implicit_Box", EditConditionHides))
	float CollisionMarginFraction;

};


USTRUCT()
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionSizeSpecificData
{
	GENERATED_BODY()

	FGeometryCollectionSizeSpecificData();

	/** The max size these settings apply to*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	float MaxSize;

	/*
	* Collision Shapes allow kfor multiple collision types per rigid body. 
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	TArray<FGeometryCollectionCollisionTypeData> CollisionShapes;

#if WITH_EDITORONLY_DATA
	/*
	 *  CollisionType defines how to initialize the rigid collision structures.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.CollisionType instead."))
	ECollisionTypeEnum CollisionType_DEPRECATED;

	/*
	 *  CollisionType defines how to initialize the rigid collision structures.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.ImplicitType instead."))
	EImplicitTypeEnum ImplicitType_DEPRECATED;

	/*
	 *  Resolution on the smallest axes for the level set. (def: 5)
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.LevelSet.MinLevelSetResolution instead."))
	int32 MinLevelSetResolution_DEPRECATED;

	/*
	 *  Resolution on the smallest axes for the level set. (def: 10)
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.LevelSet.MaxLevelSetResolution instead."))
	int32 MaxLevelSetResolution_DEPRECATED;

	/*
	 *  Resolution on the smallest axes for the level set. (def: 5)
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.LevelSet.MinClusterLevelSetResolution instead."))
	int32 MinClusterLevelSetResolution_DEPRECATED;

	/*
	 *  Resolution on the smallest axes for the level set. (def: 10)
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.LevelSet.MaxClusterLevelSetResolution instead."))
	int32 MaxClusterLevelSetResolution_DEPRECATED;

	/*
	 *  Resolution on the smallest axes for the level set. (def: 10)
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.CollisionObjectReductionPercentage instead."))
	int32 CollisionObjectReductionPercentage_DEPRECATED;

	/**
	 * Number of particles on the triangulated surface to use for collisions.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.CollisionParticlesFraction instead."))
	float CollisionParticlesFraction_DEPRECATED;

	/**
	 * Max number of particles.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use Collision.MaximumCollisionParticles instead."))
	int32 MaximumCollisionParticles_DEPRECATED;
#endif

	/**
	 * Max number of particles.
	 */
	UPROPERTY(EditAnywhere, Category = "Collisions")
	int32 DamageThreshold;

	bool Serialize(FArchive& Ar);
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif
};

template<>
struct TStructOpsTypeTraits<FGeometryCollectionSizeSpecificData> : public TStructOpsTypeTraitsBase2<FGeometryCollectionSizeSpecificData>
{
	enum
	{
		WithSerializer = true,
#if WITH_EDITORONLY_DATA
		WithPostSerialize = true
#endif
	};
};

class FGeometryCollectionNaniteData
{
public:
	GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionNaniteData();
	GEOMETRYCOLLECTIONENGINE_API ~FGeometryCollectionNaniteData();

	FORCEINLINE bool IsInitialized()
	{
		return bIsInitialized;
	}

	/** Serialization. */
	void Serialize(FArchive& Ar, UGeometryCollection* Owner);

	/** Initialize the render resources. */
	void InitResources(UGeometryCollection* Owner);

	/** Releases the render resources. */
	GEOMETRYCOLLECTIONENGINE_API void ReleaseResources();

	Nanite::FResources NaniteResource;

private:
	bool bIsInitialized = false;
};


USTRUCT(BlueprintType)
struct GEOMETRYCOLLECTIONENGINE_API FGeometryCollectionDamagePropagationData
{
public:
	GENERATED_BODY()

	/** Whether or not damage propagation is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Propagation")
	bool bEnabled = true;

	/** factor of the remaining strain propagated through the connection graph after a piece breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Propagation")
	float BreakDamagePropagationFactor = 1.0f;

	/** factor of the received strain propagated throug the connection graph if the piece did not break. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Propagation")
	float ShockDamagePropagationFactor = 0.0f;
};

/**
* UGeometryCollectionObject (UObject)
*
* UObject wrapper for the FGeometryCollection
*
*/
UCLASS(BlueprintType, customconstructor)
class GEOMETRYCOLLECTIONENGINE_API UGeometryCollection : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UGeometryCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UObject Interface */
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
#endif
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	/** End UObject Interface */

	void Serialize(FArchive& Ar);
#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif

#if WITH_EDITOR
	void EnsureDataIsCooked(bool bInitResources = true, bool bIsTransacting = false);
#endif

	/** Accessors for internal geometry collection */
	void SetGeometryCollection(TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollectionIn) { GeometryCollection = GeometryCollectionIn; }
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe>       GetGeometryCollection() { return GeometryCollection; }
	const TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GetGeometryCollection() const { return GeometryCollection; }

	/** Return collection to initial (ie. empty) state. */
	void Reset();
	
	int32 AppendGeometry(const UGeometryCollection & Element, bool ReindexAllMaterials = false, const FTransform& TransformRoot = FTransform::Identity);
	int32 NumElements(const FName& Group) const;
	void RemoveElements(const FName& Group, const TArray<int32>& SortedDeletionList);

	FORCEINLINE bool HasNaniteData() const
	{
		return NaniteData != nullptr;
	}

	FORCEINLINE uint32 GetNaniteResourceID() const
	{
		Nanite::FResources& Resource = NaniteData->NaniteResource;
		return Resource.RuntimeResourceID;
	}

	FORCEINLINE uint32 GetNaniteHierarchyOffset() const
	{
		Nanite::FResources& Resource = NaniteData->NaniteResource;
		return Resource.HierarchyOffset;
	}

	FORCEINLINE uint32 GetNaniteHierarchyOffset(int32 GeometryIndex, bool bFlattened = false) const
	{
		Nanite::FResources& Resource = NaniteData->NaniteResource;
		check(GeometryIndex >= 0 && GeometryIndex < Resource.HierarchyRootOffsets.Num());
		uint32 HierarchyOffset = Resource.HierarchyRootOffsets[GeometryIndex];
		if (bFlattened)
		{
			HierarchyOffset += Resource.HierarchyOffset;
		}
		return HierarchyOffset;
	}

	/** ReindexMaterialSections */
	void ReindexMaterialSections();

	/** appends the standard materials to this UObject */
	void InitializeMaterials();


	/** Returns true if there is anything to render */
	bool HasVisibleGeometry() const;

	/** Invalidates this collection signaling a structural change and renders any previously recorded caches unable to play with this collection */
	void InvalidateCollection();

	/** Check to see if Simulation Data requires regeneration */
	bool IsSimulationDataDirty() const;

	/** Attach a Static Mesh exemplar for embedded geometry, if that mesh has not already been attached. Return the exemplar index. */
	int32 AttachEmbeddedGeometryExemplar(const UStaticMesh* Exemplar);

	/** Remove embedded geometry exemplars with indices matching the sorted removal list. */
	void RemoveExemplars(const TArray<int32>& SortedRemovalIndices);

	/** find or add a auto instance mesh and return its index */
	const FGeometryCollectionAutoInstanceMesh& GetAutoInstanceMesh(int32 AutoInstanceMeshIndex) const;

	/**  find or add a auto instance mesh from another one and return its index */
	int32 FindOrAddAutoInstanceMesh(const FGeometryCollectionAutoInstanceMesh& AutoInstanecMesh);

	/** find or add a auto instance mesh from a mesh and alist of material and return its index */
	int32 FindOrAddAutoInstanceMesh(const UStaticMesh& StaticMesh, const TArray<UMaterialInterface*>& Materials);

	/** Produce a deep copy of GeometryCollection member, stripped of data unecessary for gameplay. */
	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GenerateMinimalGeometryCollection() const;

#if WITH_EDITOR
	/** If this flag is set, we only regenerate simulation data when requested via CreateSimulationData() */
	bool bManualDataCreate;
	
	/** Create the simulation data that can be shared among all instances (mass, volume, etc...)*/
	void CreateSimulationData();

	/** Create the Nanite rendering data. */
	static TUniquePtr<FGeometryCollectionNaniteData> CreateNaniteData(FGeometryCollection* Collection);
#endif

	void InitResources();
	void ReleaseResources();

	/** Fills params struct with parameters used for precomputing content. */
	void GetSharedSimulationParams(FSharedSimulationParameters& OutParams) const;

	/** Accessors for the two guids used to identify this collection */
	FGuid GetIdGuid() const;
	FGuid GetStateGuid() const;

	/** Pointer to the data used to render this geometry collection with Nanite. */
	TUniquePtr<class FGeometryCollectionNaniteData> NaniteData;

	UPROPERTY(EditAnywhere, Category = "Clustering")
	bool EnableClustering;

	/** Maximum level for cluster breaks. */
	UPROPERTY(EditAnywhere, Category = "Clustering")
	int32 ClusterGroupIndex;

	/** Maximum level for cluster breaks. */
	UPROPERTY(EditAnywhere, Category = "Clustering")
	int32 MaxClusterLevel;

	/** Damage threshold for clusters at different levels. */
	UPROPERTY(EditAnywhere, Category = "Damage", meta = (EditCondition = "!bUseSizeSpecificDamageThreshold"))
	TArray<float> DamageThreshold;

	/** whether to use size specific damage threshold instead of level based ones ( see Size Specific Data array ). */
	UPROPERTY(EditAnywhere, Category = "Damage")
	bool bUseSizeSpecificDamageThreshold;

	/** compatibility check, when true, only cluster compute damage from parameters and propagate to direct children
	 *  when false, each child will compute it's damage threshold allowing for more precise and intuitive destruction behavior
	 */
	UPROPERTY(EditAnywhere, Category = "Compatibility")
	bool PerClusterOnlyDamageThreshold;

	/** Data about how damage propagation shoudl behave. */
	UPROPERTY(EditAnywhere, Category = "Damage")
	FGeometryCollectionDamagePropagationData DamagePropagationData;

	/** */
	UPROPERTY(EditAnywhere, Category = "Clustering")
	EClusterConnectionTypeEnum ClusterConnectionType;

	UPROPERTY(EditAnywhere, Category = "Clustering")
	float ConnectionGraphBoundsFilteringMargin;

#if WITH_EDITORONLY_DATA
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GeometrySource")
	TArray<FGeometryCollectionSource> GeometrySource;
#endif

	UPROPERTY(EditAnywhere, Category = "Materials")
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/** References for embedded geometry generation */
	UPROPERTY(EditAnywhere, Category = "EmbeddedGeometry")
	TArray<FGeometryCollectionEmbeddedExemplar> EmbeddedGeometryExemplar;

	/** Whether to use full precision UVs when rendering this geometry. (Does not apply to Nanite rendering) */
	UPROPERTY(EditAnywhere, Category = "Rendering")
	bool bUseFullPrecisionUVs = false;

	/** list of unique static mesh / materials pairs for auto instancing*/
	UPROPERTY(EditAnywhere, Category = "Rendering")
	TArray<FGeometryCollectionAutoInstanceMesh> AutoInstanceMeshes;

	/** static mesh to use as a proxy for rendering until the geometry collection is broken */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Rendering", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath RootProxy;

	/**
	 * Strip unnecessary data from the Geometry Collection to keep the memory footprint as small as possible.
	 */
	UPROPERTY(EditAnywhere, Category = "Nanite")
	bool bStripOnCook;

	/**
	 * Enable support for Nanite.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Nanite")
	bool EnableNanite;

#if WITH_EDITORONLY_DATA
	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	ECollisionTypeEnum CollisionType_DEPRECATED;

	/*
	*  CollisionType defines how to initialize the rigid collision structures.
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	EImplicitTypeEnum ImplicitType_DEPRECATED;

	/*
	*  Resolution on the smallest axes for the level set. (def: 5)
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	int32 MinLevelSetResolution_DEPRECATED;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	int32 MaxLevelSetResolution_DEPRECATED;

	/*
	*  Resolution on the smallest axes for the level set. (def: 5)
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	int32 MinClusterLevelSetResolution_DEPRECATED;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	int32 MaxClusterLevelSetResolution_DEPRECATED;

	/*
	*  Resolution on the smallest axes for the level set. (def: 10)
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	float CollisionObjectReductionPercentage_DEPRECATED;
#endif
	
	/**
	* Mass As Density, units are in kg/m^3
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collisions")
	bool bMassAsDensity;

	/**
	* Total Mass of Collection. If density, units are in kg/m^3
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collisions")
	float Mass;

	/**
	* Smallest allowable mass (def:0.1)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collisions")
	float MinimumMassClamp;

	/**
	* whether to import collision from the source asset
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collisions")
	bool bImportCollisionFromSource;
	
#if WITH_EDITORONLY_DATA
	/**
	 * Number of particles on the triangulated surface to use for collisions.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	float CollisionParticlesFraction_DEPRECATED;

	/**
	 * Max number of particles.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "This property is deprecated. Use the default SizeSpecificData instead."))
	int32 MaximumCollisionParticles_DEPRECATED;
#endif

	/** Remove particle from simulation and dissolve rendered geometry once sleep threshold has been exceeded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Removal, meta = (DisplayName = "Remove on Sleep"))
	bool bRemoveOnMaxSleep;
	
	/** How long may the particle sleep before initiating removal (in seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Removal, meta = (DisplayName = "Sleep Min Max", EditCondition="bRemoveOnMaxSleep"))
	FVector2D MaximumSleepTime;

	/** How long does the removal process take (in seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Removal, meta = (DisplayName = "Removal Duration", EditCondition="bRemoveOnMaxSleep"))
	FVector2D RemovalDuration;

	/** when on non-sleeping, slow moving pieces will be considered as sleeping, this helps removal of jittery but not really moving objects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Removal, meta = (DisplayName = "Slow-Moving as sleeping", EditCondition="bRemoveOnMaxSleep"))
	bool bSlowMovingAsSleeping;

	/** When slow moving detection is on, this defines the linear velocity thresholds in cm/s to consider the object as sleeping . */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Removal, meta = (DisplayName = "Slow-Moving Velocity Threshold", EditCondition="bRemoveOnMaxSleep && bSlowMovingAsSleeping"))
	float SlowMovingVelocityThreshold;
	
	/*
	* Size Specfic Data reflects the default geometry to bind to rigid bodies smaller
	* than the max size volume. This can also be empty to reflect no collision geometry
	* for the collection. 
	*/
	UPROPERTY(EditAnywhere, Category = "Collisions")
	TArray<FGeometryCollectionSizeSpecificData> SizeSpecificData;

	int GetDefaultSizeSpecificDataIndex() const;
	FGeometryCollectionSizeSpecificData& GetDefaultSizeSpecificData();
	const FGeometryCollectionSizeSpecificData& GetDefaultSizeSpecificData() const;
	static FGeometryCollectionSizeSpecificData GeometryCollectionSizeSpecificDataDefaults();

	/**
	* Enable remove pieces on fracture
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use remove on break feature instead ( Fracture editor tools )."))
	bool EnableRemovePiecesOnFracture_DEPRECATED;

	/**
	* Materials relating to remove on fracture
	*/
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use remove on break feature instead ( Fracture editor tools )."))
	TArray<TObjectPtr<UMaterialInterface>> RemoveOnFractureMaterials_DEPRECATED;

	FORCEINLINE const int32 GetBoneSelectedMaterialIndex() const { return BoneSelectedMaterialIndex; }

	/** Returns the asset path for the automatically populated selected material. */
	static const TCHAR* GetSelectedMaterialPath();

#if WITH_EDITORONLY_DATA
	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category = GeometryCollection)
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;
#endif // WITH_EDITORONLY_DATA

	/*
	* Update the convex geometry on the collection.
	*/
	void UpdateConvexGeometry();


	//
	// Dataflow
	//
	UPROPERTY(EditAnywhere, Category = "Procedural")
	TObjectPtr<UDataflow> Dataflow;

private:
#if WITH_EDITOR
	void CreateSimulationDataImp(bool bCopyFromDDC);
#endif

	/*
	* Used to transfer deprecated properties to the size specific structures during serialization
	* and to add back the default size specific data when deleted.
	*/
	void ValidateSizeSpecificDataDefaults();


private:
	/** Guid created on construction of this collection. It should be used to uniquely identify this collection */
	UPROPERTY()
	FGuid PersistentGuid;

	/** 
	 * Guid that can be invalidated on demand - essentially a 'version' that should be changed when a structural change is made to
	 * the geometry collection. This signals to any caches that attempt to link to a geometry collection whether the collection
	 * is still valid (hasn't structurally changed post-recording)
	 */
	UPROPERTY()
	FGuid StateGuid;

#if WITH_EDITOR
	//Used to determine whether we need to cook content
	FGuid LastBuiltGuid;

	//Used to determine whether we need to regenerate simulation data
	FGuid SimulationDataGuid;
#endif

	// #todo(dmp): rename to be consistent BoneSelectedMaterialID?
	UPROPERTY()
	int32 BoneSelectedMaterialIndex;

	TSharedPtr<FGeometryCollection, ESPMode::ThreadSafe> GeometryCollection;
};
