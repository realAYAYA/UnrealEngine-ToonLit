// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Field/FieldSystem.h"
#include "GeometryCollection/RecordedTransformTrack.h"
#include "GeometryCollectionSimulationTypes.h"
#include "Chaos/ClusterCreationParameters.h"
#include "Chaos/CollisionFilterData.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "Chaos/PBDRigidClusteringTypes.h"

class FGeometryCollection;
class FGeometryDynamicCollection;


struct FCollectionLevelSetData
{
	FCollectionLevelSetData()
		: MinLevelSetResolution(5)
		, MaxLevelSetResolution(10)
		, MinClusterLevelSetResolution(25)
		, MaxClusterLevelSetResolution(50)
	{}

	int32 MinLevelSetResolution;
	int32 MaxLevelSetResolution;
	int32 MinClusterLevelSetResolution;
	int32 MaxClusterLevelSetResolution;
};

struct FCollectionCollisionParticleData
{
	FCollectionCollisionParticleData()
		: CollisionParticlesFraction(1.f)
		, MaximumCollisionParticles(60)
	{}

	float CollisionParticlesFraction;
	int32 MaximumCollisionParticles;
};



struct FCollectionCollisionTypeData
{
	FCollectionCollisionTypeData()
		: CollisionType(ECollisionTypeEnum::Chaos_Surface_Volumetric)
		, ImplicitType(EImplicitTypeEnum::Chaos_Implicit_Sphere)
		, LevelSetData()
		, CollisionParticleData()
		, CollisionObjectReductionPercentage(0.f)
		, CollisionMarginFraction(0.f)
	{
	}

	ECollisionTypeEnum CollisionType;
	EImplicitTypeEnum ImplicitType;
	FCollectionLevelSetData LevelSetData;
	FCollectionCollisionParticleData CollisionParticleData;
	float CollisionObjectReductionPercentage;
	float CollisionMarginFraction;
};

struct FSharedSimulationSizeSpecificData
{
	FSharedSimulationSizeSpecificData()
		: MaxSize(0.f)
		, CollisionShapesData({ FCollectionCollisionTypeData() })
		, DamageThreshold(5000.f)
	{
	}

	float MaxSize;
	TArray<FCollectionCollisionTypeData> CollisionShapesData;
	float DamageThreshold;

	bool operator<(const FSharedSimulationSizeSpecificData& Rhs) const { return MaxSize < Rhs.MaxSize; }
};

//
//
//
enum ESimulationInitializationState : uint8 { Unintialized = 0, Activated, Created, Initialized };


/**
*  Simulation Parameters
*/
struct FSharedSimulationParameters
{
	FSharedSimulationParameters()
	: bMassAsDensity(true)
	, Mass(1.0f)
	, MinimumMassClamp(0.1f)								// todo : Expose to users with better initial values
	, MaximumMassClamp(1e5f)								// todo : Expose to users with better initial values
	, MinimumBoundingExtentClamp(0.1f)						// todo : Expose to users with better initial values
	, MaximumBoundingExtentClamp(1e6f)						// todo : Expose to users with better initial values
	, MinimumInertiaTensorDiagonalClamp(UE_SMALL_NUMBER)	// todo : Expose to users with better initial values
	, MaximumInertiaTensorDiagonalClamp(1e20f)				// todo : Expose to users with better initial values
	, MaximumCollisionParticleCount(60)
	, bUseImportedCollisionImplicits(false)
	{
		SizeSpecificData.AddDefaulted();
	}

	FSharedSimulationParameters(ECollisionTypeEnum InCollisionType
		,EImplicitTypeEnum InImplicitType
		,int32 InMinLevelSetResolution
		,int32 InMaxLevelSetResolution
		,int32 InMinClusterLevelSetResolution
		,int32 InMaxClusterLevelSetResolution
		,bool InMassAsDensity
		,float InMass
		, float InMinimumMassClamp
		, float InMaximumMassClamp
		, float InMinimumBoundingExtentClamp
		, float InMaximumBoundingExtentClamp
		, float InMinimumInertiaTensorDiagonalClamp
		, float InMaximumInertiaTensorDiagonalClamp
		, float InCollisionParticlesFraction
		, int32 InMaximumCollisionParticleCount
		, float InCollisionMarginFraction
		, bool InUseImportedCollisionImplicits )
	: bMassAsDensity(InMassAsDensity)
	, Mass(InMass)
	, MinimumMassClamp(InMinimumMassClamp)
	, MaximumMassClamp(InMinimumMassClamp)
	, MinimumBoundingExtentClamp(InMinimumBoundingExtentClamp)
	, MaximumBoundingExtentClamp(InMinimumBoundingExtentClamp)
	, MinimumInertiaTensorDiagonalClamp(InMinimumInertiaTensorDiagonalClamp)
	, MaximumInertiaTensorDiagonalClamp(InMaximumInertiaTensorDiagonalClamp)
	, MaximumCollisionParticleCount(InMaximumCollisionParticleCount)
	, bUseImportedCollisionImplicits(InUseImportedCollisionImplicits)
	{
		SizeSpecificData.AddDefaulted();
		if (ensure(SizeSpecificData.Num() && SizeSpecificData[0].CollisionShapesData.Num()))
		{
			SizeSpecificData[0].CollisionShapesData[0].CollisionType = InCollisionType;
			SizeSpecificData[0].CollisionShapesData[0].ImplicitType = InImplicitType;
			SizeSpecificData[0].CollisionShapesData[0].CollisionMarginFraction = InCollisionMarginFraction;
			SizeSpecificData[0].CollisionShapesData[0].LevelSetData.MinLevelSetResolution = InMinLevelSetResolution;
			SizeSpecificData[0].CollisionShapesData[0].LevelSetData.MaxLevelSetResolution = InMaxLevelSetResolution;
			SizeSpecificData[0].CollisionShapesData[0].LevelSetData.MinClusterLevelSetResolution = InMinClusterLevelSetResolution;
			SizeSpecificData[0].CollisionShapesData[0].LevelSetData.MaxClusterLevelSetResolution = InMaxClusterLevelSetResolution;
			SizeSpecificData[0].CollisionShapesData[0].CollisionParticleData.CollisionParticlesFraction = InCollisionParticlesFraction;
			SizeSpecificData[0].CollisionShapesData[0].CollisionParticleData.MaximumCollisionParticles = InMaximumCollisionParticleCount;
		}
	}

	bool bMassAsDensity;
	float Mass;
	float MinimumMassClamp;
	float MaximumMassClamp;
	float MinimumBoundingExtentClamp;
	float MaximumBoundingExtentClamp;
	float MinimumInertiaTensorDiagonalClamp;
	float MaximumInertiaTensorDiagonalClamp;

	float MinimumVolumeClamp() const { return MinimumBoundingExtentClamp * MinimumBoundingExtentClamp * MinimumBoundingExtentClamp; }
	float MaximumVolumeClamp() const { return MaximumBoundingExtentClamp * MaximumBoundingExtentClamp * MaximumBoundingExtentClamp; }

	TArray<FSharedSimulationSizeSpecificData> SizeSpecificData;
	int32 MaximumCollisionParticleCount;
	bool bUseImportedCollisionImplicits;
};

struct FSimulationParameters
{
	FSimulationParameters()
		: Name("")
		, RestCollection(nullptr)
		, InitialRootIndex(INDEX_NONE)
		, RecordedTrack(nullptr)
		, bOwnsTrack(false)
		, Simulating(false)
		, WorldTransform(FTransform::Identity)
		, EnableClustering(true)
		, ClusterGroupIndex(0)
		, MaxClusterLevel(100)
		, MaxSimulatedLevel(100)
		, bUseSizeSpecificDamageThresholds(false)
		, bUseMaterialDamageModifiers(false)
		, DamageModel(EDamageModelTypeEnum::Chaos_Damage_Model_UserDefined_Damage_Threshold)
		, DamageEvaluationModel(Chaos::EDamageEvaluationModel::StrainFromDamageThreshold)
		, DamageThreshold({500000.f, 50000.f, 5000.f})
		, bUsePerClusterOnlyDamageThreshold(false)
		, ClusterConnectionMethod(Chaos::FClusterCreationParameters::EConnectionMethod::PointImplicit)
		, ConnectionGraphBoundsFilteringMargin(0)
		, CollisionGroup(0)
		, CollisionSampleFraction(1.0)
		, InitialVelocityType(EInitialVelocityTypeEnum::Chaos_Initial_Velocity_None)
		, InitialLinearVelocity(FVector(0))
		, InitialAngularVelocity(FVector(0))
		, CacheType(EGeometryCollectionCacheType::None)
		, CacheBeginTime(0.0f)
		, ReverseCacheBeginTime(0.0f)
		, bClearCache(false)
		, ObjectType(EObjectStateTypeEnum::Chaos_NONE)
		, StartAwake(true)
		, MaterialOverrideMassScaleMultiplier(1.0f)
		, bGenerateBreakingData(false)
		, bGenerateCollisionData(false)
		, bGenerateTrailingData(false)
		, bGenerateCrumblingData(false)
		, bGenerateCrumblingChildrenData(false)
		, bGenerateGlobalBreakingData(false)
		, bGenerateGlobalCollisionData(false)
		, bGenerateGlobalCrumblingData(false)
		, bGenerateGlobalCrumblingChildrenData(false)
		, EnableGravity(true)
		, GravityGroupIndex(0)
		, OneWayInteractionLevel(INDEX_NONE)
		, UseInertiaConditioning(true)
		, UseCCD(false)
		, UseMACD(false)
		, LinearDamping(0.01f)
		, AngularDamping(0)
		, InitialOverlapDepenetrationVelocity(-1.0f)
		, SleepThresholdMultiplier(1.0f)
		, bUseDamagePropagation(false)
		, BreakDamagePropagationFactor(1.0f)
		, ShockDamagePropagationFactor(0.0f)
		, SimulationFilterData()
		, QueryFilterData()
		, UserData(nullptr)
		, bEnableStrainOnCollision(true)
		, bUseStaticMeshCollisionForTraces(false)
		, bOptimizeConvexes(true)
	{}

	FSimulationParameters(const FSimulationParameters& Other)
		: Name(Other.Name)
		, RestCollection(Other.RestCollection)
		, InitialRootIndex(Other.InitialRootIndex)
		, InitializationCommands(Other.InitializationCommands)
		, RecordedTrack(Other.RecordedTrack)
		, bOwnsTrack(false)
		, Simulating(Other.Simulating)
		, WorldTransform(Other.WorldTransform)
		, EnableClustering(Other.EnableClustering)
		, ClusterGroupIndex(Other.ClusterGroupIndex)
		, MaxClusterLevel(Other.MaxClusterLevel)
		, MaxSimulatedLevel(Other.MaxSimulatedLevel)
		, bUseSizeSpecificDamageThresholds(Other.bUseSizeSpecificDamageThresholds)
		, bUseMaterialDamageModifiers(Other.bUseMaterialDamageModifiers)
		, DamageModel(Other.DamageModel)
		, DamageEvaluationModel(Other.DamageEvaluationModel)
		, DamageThreshold(Other.DamageThreshold)
		, bUsePerClusterOnlyDamageThreshold(Other.bUsePerClusterOnlyDamageThreshold)
		, ClusterConnectionMethod(Other.ClusterConnectionMethod)
		, ConnectionGraphBoundsFilteringMargin(Other.ConnectionGraphBoundsFilteringMargin)
		, CollisionGroup(Other.CollisionGroup)
		, CollisionSampleFraction(Other.CollisionSampleFraction)
		, InitialVelocityType(Other.InitialVelocityType)
		, InitialLinearVelocity(Other.InitialLinearVelocity)
		, InitialAngularVelocity(Other.InitialAngularVelocity)
		, CacheType(Other.CacheType)
		, CacheBeginTime(Other.CacheBeginTime)
		, ReverseCacheBeginTime(Other.ReverseCacheBeginTime)
		, bClearCache(Other.bClearCache)
		, ObjectType(Other.ObjectType)
		, StartAwake(Other.StartAwake)
		, PhysicalMaterialHandle(Other.PhysicalMaterialHandle)
		, MaterialOverrideMassScaleMultiplier(Other.MaterialOverrideMassScaleMultiplier)
		, bGenerateBreakingData(Other.bGenerateBreakingData)
		, bGenerateCollisionData(Other.bGenerateCollisionData)
		, bGenerateTrailingData(Other.bGenerateTrailingData)
		, bGenerateCrumblingData(Other.bGenerateCrumblingData)
		, bGenerateCrumblingChildrenData(Other.bGenerateCrumblingChildrenData)
		, bGenerateGlobalBreakingData(Other.bGenerateGlobalBreakingData)
		, bGenerateGlobalCollisionData(Other.bGenerateGlobalCollisionData)
		, bGenerateGlobalCrumblingData(Other.bGenerateGlobalCrumblingData)
		, bGenerateGlobalCrumblingChildrenData(Other.bGenerateGlobalCrumblingChildrenData)
		, Shared(Other.Shared)
		, EnableGravity(Other.EnableGravity)
		, GravityGroupIndex(Other.GravityGroupIndex)
		, OneWayInteractionLevel(Other.OneWayInteractionLevel)
		, UseInertiaConditioning(Other.UseInertiaConditioning)
		, UseCCD(Other.UseCCD)
		, UseMACD(Other.UseMACD)
		, LinearDamping(Other.LinearDamping)
		, AngularDamping(Other.AngularDamping)
		, InitialOverlapDepenetrationVelocity(Other.InitialOverlapDepenetrationVelocity)
		, SleepThresholdMultiplier(Other.SleepThresholdMultiplier)
		, bUseDamagePropagation(Other.bUseDamagePropagation)
		, BreakDamagePropagationFactor(Other.BreakDamagePropagationFactor)
		, ShockDamagePropagationFactor(Other.ShockDamagePropagationFactor)
		, SimulationFilterData(Other.SimulationFilterData)
		, QueryFilterData(Other.QueryFilterData)
		, UserData(Other.UserData)
		, bEnableStrainOnCollision(Other.bEnableStrainOnCollision)
		, bUseStaticMeshCollisionForTraces(Other.bUseStaticMeshCollisionForTraces)
		, bOptimizeConvexes(Other.bOptimizeConvexes)
	{
	}

	~FSimulationParameters()
	{
		if (bOwnsTrack)
		{
			delete const_cast<FRecordedTransformTrack*>(RecordedTrack);
		}
	}

	bool IsCacheRecording() { return CacheType == EGeometryCollectionCacheType::Record || CacheType == EGeometryCollectionCacheType::RecordAndPlay; }
	bool IsCachePlaying() { return CacheType == EGeometryCollectionCacheType::Play || CacheType == EGeometryCollectionCacheType::RecordAndPlay; }

	FString Name;
	const FGeometryCollection* RestCollection;
	int32 InitialRootIndex;
	TArray<FFieldSystemCommand> InitializationCommands;
	const FRecordedTransformTrack* RecordedTrack;
	bool bOwnsTrack;

	bool Simulating;

	FTransform WorldTransform;
	FTransform PrevWorldTransform;

	bool EnableClustering;
	int32 ClusterGroupIndex;
	int32 MaxClusterLevel;
	int32 MaxSimulatedLevel;
	bool bUseSizeSpecificDamageThresholds;
	bool bUseMaterialDamageModifiers;

	/** this is the user expose damage model, used for creation of the particles */
	EDamageModelTypeEnum DamageModel; 

	/** this is the lower level damage model for clustering, used at runm time */
	Chaos::EDamageEvaluationModel DamageEvaluationModel;

	TArray<float> DamageThreshold;
	bool bUsePerClusterOnlyDamageThreshold;
	Chaos::FClusterCreationParameters::EConnectionMethod ClusterConnectionMethod;
	float ConnectionGraphBoundsFilteringMargin;

	int32 CollisionGroup;
	float CollisionSampleFraction;

	EInitialVelocityTypeEnum InitialVelocityType;
	FVector InitialLinearVelocity;
	FVector InitialAngularVelocity;

	EGeometryCollectionCacheType CacheType;
	float CacheBeginTime;
	float ReverseCacheBeginTime;
	bool bClearCache;

	EObjectStateTypeEnum ObjectType;
	bool StartAwake;

	Chaos::FMaterialHandle PhysicalMaterialHandle;

	float MaterialOverrideMassScaleMultiplier;

	bool bGenerateBreakingData;
	bool bGenerateCollisionData;
	bool bGenerateTrailingData;
	bool bGenerateCrumblingData;
	bool bGenerateCrumblingChildrenData;

	bool bGenerateGlobalBreakingData;
	bool bGenerateGlobalCollisionData;
	bool bGenerateGlobalCrumblingData;
	bool bGenerateGlobalCrumblingChildrenData;

	FSharedSimulationParameters Shared;

	bool EnableGravity;
	int32 GravityGroupIndex;
	int32 OneWayInteractionLevel;
	bool UseInertiaConditioning;
	bool UseCCD;
	bool UseMACD;
	float LinearDamping;
	float AngularDamping;
	float InitialOverlapDepenetrationVelocity;
	float SleepThresholdMultiplier;

	bool bUseDamagePropagation;
	float BreakDamagePropagationFactor;
	float ShockDamagePropagationFactor;

	FCollisionFilterData SimulationFilterData;
	FCollisionFilterData QueryFilterData;
	void* UserData;
	bool bEnableStrainOnCollision;

	bool bUseStaticMeshCollisionForTraces;

	bool bOptimizeConvexes = true;
};
