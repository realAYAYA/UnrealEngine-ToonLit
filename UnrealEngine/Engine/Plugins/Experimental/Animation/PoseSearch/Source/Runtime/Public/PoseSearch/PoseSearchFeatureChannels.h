// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PoseSearch.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "PoseSearchFeatureChannels.generated.h"

UENUM(BlueprintType)
enum class EInputQueryPose : uint8
{
	// use character pose to compose the query
	UseCharacterPose,

	// if available reuse continuing pose from the database to compose the query or else UseCharacterPose
	UseContinuingPose,

	// if available reuse and interpolate continuing pose from the database to compose the query or else UseCharacterPose
	UseInterpolatedContinuingPose,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel_Position
UCLASS(BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel_Position : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTimeOffset = 0.f;

	UPROPERTY()
	int8 SchemaBoneIdx;

	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 ColorPresetIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	// if UseSampleTimeOffsetRootBone is true, this UPoseSearchFeatureChannel_Position will calculate the position of Bone from the pose SampleTimeOffset seconds away from the current time pose root bone
	// if false the calculated position of Bone will be in component space from the pose SampleTimeOffset seconds away
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bUseSampleTimeOffsetRootBone = true;

	// UPoseSearchFeatureChannel interface
	virtual void InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput) const override;
	virtual bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector) const override;
};

//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel_Heading

UENUM(BlueprintType)
enum class EHeadingAxis : uint8
{
	X,
	Y,
	Z,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UCLASS(BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel_Heading : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTimeOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EHeadingAxis HeadingAxis = EHeadingAxis::X;	

	UPROPERTY()
	int8 SchemaBoneIdx;

	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 ColorPresetIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	// if UseSampleTimeOffsetRootBone is true, this UPoseSearchFeatureChannel_Position will calculate the position of Bone from the pose SampleTimeOffset seconds away from the current time pose root bone
	// if false the calculated position of Bone will be in component space from the pose SampleTimeOffset seconds away
	UPROPERTY(EditAnywhere, Category = "Settings")
	bool bUseSampleTimeOffsetRootBone = true;

	// UPoseSearchFeatureChannel interface
	virtual void InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput) const override;
	virtual bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector) const override;

	FVector GetAxis(const FQuat& Rotation) const;
};

//////////////////////////////////////////////////////////////////////////
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPoseSearchBoneFlags : uint32
{
	Velocity = 1 << 0,
	Position = 1 << 1,
	Rotation = 1 << 2,
	Phase = 1 << 3,
};
ENUM_CLASS_FLAGS(EPoseSearchBoneFlags);
constexpr bool EnumHasAnyFlags(int32 Flags, EPoseSearchBoneFlags Contains) { return (Flags & int32(Contains)) != 0; }
inline int32& operator|=(int32& Lhs, EPoseSearchBoneFlags Rhs) { return Lhs |= int32(Rhs); }

USTRUCT()
struct POSESEARCH_API FPoseSearchBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Config)
	FBoneReference Reference;

	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = "/Script/PoseSearch.EPoseSearchBoneFlags"), Category = Config)
	int32 Flags = int32(EPoseSearchBoneFlags::Position);

	UPROPERTY(EditAnywhere, Category = Config)
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = Config, meta=(ExcludeFromHash))
	int32 ColorPresetIndex = 0;
};

// UPoseSearchFeatureChannel_Pose
UCLASS(BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel_Pose : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FPoseSearchBone> SampledBones;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<float> SampleTimes;

	UPROPERTY()
	TArray<int8> SchemaBoneIdx;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	// UObject interface
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	// UPoseSearchFeatureChannel interface
	virtual void InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput) const override;
	virtual void ComputeMeanDeviations(const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations) const override;
	virtual bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector) const override;

#if WITH_EDITOR
	virtual void ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData, const UPoseSearchSchema* Schema) const override;
#endif

protected:
	void AddPoseFeatures(UE::PoseSearch::IAssetIndexer& Indexer, int32 SampleIdx, FPoseSearchFeatureVectorBuilder& FeatureVector, const TArray<TArray<FVector2D>>& Phases) const;
	void CalculatePhases(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput, TArray<TArray<FVector2D>>& OutPhases) const;
};

//////////////////////////////////////////////////////////////////////////
// UPoseSearchFeatureChannel_Trajectory
UENUM()
enum class EPoseSearchFeatureDomain : int32
{
	Time,
	Distance,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EPoseSearchTrajectoryFlags : uint32
{
	Velocity = 1 << 0,
	Position = 1 << 1,
	VelocityDirection = 1 << 2,
	FacingDirection = 1 << 3,
	VelocityXY = 1 << 4,
	PositionXY = 1 << 5,
	VelocityDirectionXY = 1 << 6,
	FacingDirectionXY = 1 << 7,
};
ENUM_CLASS_FLAGS(EPoseSearchTrajectoryFlags);
constexpr bool EnumHasAnyFlags(int32 Flags, EPoseSearchTrajectoryFlags Contains) { return (Flags & int32(Contains)) != 0; }
inline int32& operator|=(int32& Lhs, EPoseSearchTrajectoryFlags Rhs) { return Lhs |= int32(Rhs); }

USTRUCT()
struct POSESEARCH_API FPoseSearchTrajectorySample
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Config)
	float Offset = 0.f; // offset in time or distance depending on UPoseSearchFeatureChannel_Trajectory.Domain

	UPROPERTY(EditAnywhere, meta = (Bitmask, BitmaskEnum = "/Script/PoseSearch.EPoseSearchTrajectoryFlags"), Category = Config)
	int32 Flags = int32(EPoseSearchTrajectoryFlags::Position);

	UPROPERTY(EditAnywhere, Category = Config)
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = Config)
	int32 ColorPresetIndex = 0;
};

UCLASS(BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel_Trajectory : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Settings")
	EPoseSearchFeatureDomain Domain = EPoseSearchFeatureDomain::Time;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FPoseSearchTrajectorySample> Samples;

	// UObject interface
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	// UPoseSearchFeatureChannel interface
	virtual void InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput) const override;
	virtual void ComputeMeanDeviations(const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations) const override;
	virtual bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector) const override;

#if WITH_EDITOR
	virtual void ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData, const UPoseSearchSchema* Schema) const override;
#endif

protected:
	void IndexAssetPrivate(const UE::PoseSearch::IAssetIndexer& Indexer, int32 SampleIdx, FPoseSearchFeatureVectorBuilder& FeatureVector) const;
	float GetSampleTime(const UE::PoseSearch::IAssetIndexer& Indexer, float Offset, float SampleTime, float RootDistance) const;
};