// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraDataInterfaceArrayInt.h"
#include "CEClonerEffectorShared.generated.h"

class UNiagaraDataChannelReader;
class UNiagaraDataChannelWriter;
class AActor;
class UDynamicMesh;
class UNiagaraSystem;
class USplineComponent;

/** Enumerate the different layouts available to dispose cloner instances */
UENUM(BlueprintType)
enum class ECEClonerLayout : uint8
{
	Grid,
	Line,
	Circle,
	Cylinder,
	Sphere,
	Honeycomb,
	SampleMesh,
	SampleSpline
};

/** Enumerate the axis available to use */
UENUM(BlueprintType)
enum class ECEClonerAxis : uint8
{
	X,
	Y,
	Z,
	Custom
};

/** Enumerate the planes available to use */
UENUM(BlueprintType)
enum class ECEClonerPlane : uint8
{
	XY,
	YZ,
	XZ,
	Custom
};

/** Enumerate the mesh render modes available to render a cloner instance */
UENUM(BlueprintType)
enum class ECEClonerMeshRenderMode : uint8
{
	/** Iterate through each attachment mesh available */
	Iterate,
	/** Pick randomly through each attachment mesh available, update cloner seed for variations */
	Random,
	/** Blend based on the total cloner instances and attachment mesh available */
	Blend
};

/** Enumerate the grid constraints available when cloner layout selected is grid */
UENUM(BlueprintType)
enum class ECEClonerGridConstraint : uint8
{
	None,
	Sphere,
	Cylinder,
	Texture
};

/* Enumerates all easing functions that are available to apply on weights
See https://easings.net/ for curves visualizations and web open domain implementations
Used as one enum to send the index to niagara as uint8 and apply easing directly in niagara */
UENUM(BlueprintType)
enum class ECEClonerEasing : uint8
{
	Linear,

	InSine,
	OutSine,
	InOutSine,

	InQuad,
	OutQuad,
	InOutQuad,

	InCubic,
	OutCubic,
	InOutCubic,

	InQuart,
	OutQuart,
	InOutQuart,

	InQuint,
	OutQuint,
	InOutQuint,

	InExpo,
	OutExpo,
	InOutExpo,

	InCirc,
	OutCirc,
	InOutCirc,

	InBack,
	OutBack,
	InOutBack,

	InElastic,
	OutElastic,
	InOutElastic,

	InBounce,
	OutBounce,
	InOutBounce,

	Random
};

/** Enumerate the mesh asset to look for when mesh layout is selected */
UENUM(BlueprintType)
enum class ECEClonerMeshAsset : uint8
{
	StaticMesh,
	SkeletalMesh
};

/** Enumerate the mesh sample dataset to pick from when mesh layout is selected */
UENUM(BlueprintType)
enum class ECEClonerMeshSampleData : uint8
{
	Vertices,
	Triangles,
	Sockets,
	Bones,
	Sections
};

/** Enumerates the effector shapes available */
UENUM(BlueprintType)
enum class ECEClonerEffectorType : uint8
{
	/** Clones inside the sphere radius will be affected by the effector */
	Sphere,
	/** Clones between two planes will be affected by the effector */
	Plane,
	/** Clones inside the box extent will be affected by the effector */
	Box,
	/** All clones will be affected by the effector with the same max weight */
	Unbound
};

/** Enumerates the effector mode available */
UENUM(BlueprintType)
enum class ECEClonerEffectorMode : uint8
{
	/** Control clones offset, rotation, scale manually */
	Default,
	/** Rotates clones towards a target actor */
	Target,
	/** Randomly applies curl noise across the field zone */
	NoiseField
};

UENUM()
enum class ECEClonerAttachmentStatus : uint8
{
	/** Item should be removed, no longer valid */
	Invalid,
	/** Item should be updated, changes detected */
	Outdated,
	/** Item is up to date, no changes needed */
	Updated,
	/** Item is being updated at the moment */
	Updating
};

/** Enumerate all texture channels to sample for constraint */
UENUM()
enum class ECEClonerTextureSampleChannel : uint8
{
	RGBLuminance,
	RGBAverage,
	RGBMax,
	R,
	G,
	B,
	A
};

/** Enumerate all operation compare mode for constraint */
UENUM()
enum class ECEClonerCompareMode : uint8
{
	Greater,
	GreaterEqual,
	Equal,
	NotEqual,
	Less,
	LessEqual
};

/** Enumerates all modes to handle cloner spawn mode */
UENUM()
enum class ECEClonerSpawnLoopMode : uint8
{
	/** Cloner spawns once and then enters idle mode */
	Once,
	/** Cloner spawns multiple times and then enters idle mode */
	Multiple,
	/** Cloner spawns infinitely and never enters idle mode */
	Infinite
};

/** Enumerates all modes for how clones are spawned */
UENUM()
enum class ECEClonerSpawnBehaviorMode : uint8
{
	/** Spawns instantly the number of clones needed for the layout */
	Instant,
	/** Spawns at a specific rate per second during the spawn loop duration */
	Rate,
};

USTRUCT()
struct FCEClonerAttachmentItem
{
	GENERATED_BODY()

	/** Children attached to this actor, order is not important here as they are combined into one */
	UPROPERTY()
	TSet<TWeakObjectPtr<AActor>> ChildrenActors;

	/** Current cloner attached actor represented by this item */
	UPROPERTY()
	TWeakObjectPtr<AActor> ItemActor;

	/** Parent of this item actor or null if below cloner */
	UPROPERTY()
	TWeakObjectPtr<AActor> ParentActor;

	/** Actual baked mesh for the current actor item,
		dynamic because it's easier for merging them together and avoid conversions again */
	UPROPERTY()
	TObjectPtr<UDynamicMesh> BakedMesh;

	/** Actual baked materials for the current actor item */
	UPROPERTY()
	TArray<TWeakObjectPtr<UMaterialInterface>> BakedMaterials;

	/** Status of the baked mesh, does it needs to be updated */
	UPROPERTY()
	ECEClonerAttachmentStatus MeshStatus = ECEClonerAttachmentStatus::Outdated;

	/** Last actor item transform, used to trigger an update if changed */
	UPROPERTY()
	FTransform ActorTransform;

	/** Item is root cloner actor */
	UPROPERTY()
	bool bRootItem = false;

	/** Status of this attachment item */
	UPROPERTY()
	ECEClonerAttachmentStatus Status = ECEClonerAttachmentStatus::Outdated;

	friend uint32 GetTypeHash(const FCEClonerAttachmentItem& InItem)
	{
		return GetTypeHash(InItem.ItemActor);
	}

	bool operator==(const FCEClonerAttachmentItem& InOther) const
	{
		return ItemActor.Get() == InOther.ItemActor.Get();
	}
};

USTRUCT()
struct FCEClonerAttachmentTree
{
	GENERATED_BODY()

	/** All cloner attached actor items */
	UPROPERTY()
	TMap<TWeakObjectPtr<AActor>, FCEClonerAttachmentItem> ItemAttachmentMap;

	/** Actors directly attached to the cloner actor, order is important here */
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> RootActors;

	/** Merged static meshes corresponding to the root actors for niagara,
		result of merging dynamic meshes together */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> MergedBakedMeshes;

	/** Status of the cloner tree */
	UPROPERTY()
	ECEClonerAttachmentStatus Status = ECEClonerAttachmentStatus::Updated;

	void Reset()
	{
		ItemAttachmentMap.Empty();
		RootActors.Empty();
		MergedBakedMeshes.Empty();
		Status = ECEClonerAttachmentStatus::Updated;
	}
};

struct FCEClonerEffectorChannelData
{
	friend class UCEEffectorSubsystem;

	/** General */
	static constexpr const TCHAR* EasingName = TEXT("Easing");
	static constexpr const TCHAR* ModeName = TEXT("Mode");
	static constexpr const TCHAR* TypeName = TEXT("Type");
	static constexpr const TCHAR* MagnitudeName = TEXT("Magnitude");
	static constexpr const TCHAR* InnerExtentName = TEXT("InnerExtent");
	static constexpr const TCHAR* OuterExtentName = TEXT("OuterExtent");
	static constexpr const TCHAR* LocationDeltaName = TEXT("LocationDelta");
	static constexpr const TCHAR* RotationDeltaName = TEXT("RotationDelta");
	static constexpr const TCHAR* ScaleDeltaName = TEXT("ScaleDelta");
	static constexpr const TCHAR* LocationName = TEXT("Location");
	static constexpr const TCHAR* RotationName = TEXT("Rotation");
	static constexpr const TCHAR* ScaleName = TEXT("Scale");
	static constexpr const TCHAR* FrequencyName = TEXT("Frequency");
	static constexpr const TCHAR* PanName = TEXT("Pan");

	/** Forces */
	static constexpr const TCHAR* OrientationForceRateName = TEXT("OrientationForceRate");
	static constexpr const TCHAR* OrientationForceMinName = TEXT("OrientationForceMin");
	static constexpr const TCHAR* OrientationForceMaxName = TEXT("OrientationForceMax");
	static constexpr const TCHAR* VortexForceAmountName = TEXT("VortexForceAmount");
	static constexpr const TCHAR* VortexForceAxisName = TEXT("VortexForceAxis");
	static constexpr const TCHAR* CurlNoiseForceStrengthName = TEXT("CurlNoiseForceStrength");
	static constexpr const TCHAR* CurlNoiseForceFrequencyName = TEXT("CurlNoiseForceFrequency");
	static constexpr const TCHAR* AttractionForceStrengthName = TEXT("AttractionForceStrength");
	static constexpr const TCHAR* AttractionForceFalloffName = TEXT("AttractionForceFalloff");
	static constexpr const TCHAR* GravityForceAccelerationName = TEXT("GravityForceAcceleration");

	int32 GetIdentifier() const
	{
		return Identifier;
	}

	/** General parameters */
	ECEClonerEasing Easing = ECEClonerEasing::Linear;
	ECEClonerEffectorMode Mode = ECEClonerEffectorMode::Default;
	ECEClonerEffectorType Type = ECEClonerEffectorType::Sphere;
	float Magnitude = 0.f;
	FVector InnerExtent = FVector::ZeroVector;
	FVector OuterExtent = FVector::ZeroVector;
	FVector LocationDelta = FVector::ZeroVector;
	FQuat RotationDelta = FQuat::Identity;
	FVector ScaleDelta = FVector::OneVector;
	FVector Location = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector Scale = FVector::OneVector;
	float Frequency = 1.f;
	FVector Pan = FVector::ZeroVector;

	/** Forces parameters */
	float OrientationForceRate = 0.f;
	FVector OrientationForceMin = FVector::ZeroVector;
	FVector OrientationForceMax = FVector::ZeroVector;
	float VortexForceAmount = 0.f;
	FVector VortexForceAxis = FVector::ZeroVector;
	float CurlNoiseForceStrength = 0.f;
	float CurlNoiseForceFrequency = 0.f;
	float AttractionForceStrength = 0.f;
	float AttractionForceFalloff = 0.f;
	FVector GravityForceAcceleration = FVector::ZeroVector;

protected:
	/** Cache effector identifier to detect a change and update cloners DI */
	int32 Identifier = INDEX_NONE;

	void Write(UNiagaraDataChannelWriter* InWriter) const;
	void Read(const UNiagaraDataChannelReader* InReader);
};

USTRUCT()
struct FCEClonerEffectorDataInterfaces
{
	friend class UCEClonerLayoutBase;

	GENERATED_BODY()

	static inline const FName IndexName = TEXT("EffectorIndexArray");

	explicit FCEClonerEffectorDataInterfaces(const UNiagaraSystem* InSystem);
	FCEClonerEffectorDataInterfaces() = default;

	void Clear() const;
	void CopyTo(FCEClonerEffectorDataInterfaces& InOther) const;
	void Resize(int32 InSize) const;
	void Remove(int32 InIndex) const;
	bool IsValid() const;
	int32 Num() const;

	UNiagaraDataInterfaceArrayInt32* GetIndexArray() const;

protected:
	UPROPERTY()
	TMap<FName, TObjectPtr<UNiagaraDataInterface>> DataInterfaces;
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerSampleMeshOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 Count = 3 * 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerMeshAsset Asset = ECEClonerMeshAsset::StaticMesh;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerMeshSampleData SampleData = ECEClonerMeshSampleData::Vertices;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	TWeakObjectPtr<AActor> SampleActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> SceneComponent = nullptr;
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerSampleSplineOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 Count = 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	TWeakObjectPtr<AActor> SplineActor = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USplineComponent> SplineComponent = nullptr;
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerGridConstraintSphere
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Radius = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FVector Center = FVector(0);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerGridConstraintCylinder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Radius = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Height = 800.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FVector Center = FVector(0);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerGridConstraintTexture
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	TWeakObjectPtr<UTexture> Texture;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerTextureSampleChannel Channel = ECEClonerTextureSampleChannel::RGBLuminance;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerPlane Plane = ECEClonerPlane::XY;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerCompareMode CompareMode = ECEClonerCompareMode::Greater;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Threshold = 0.f;
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerGridLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FIntVector Count = FIntVector(3, 3, 3);

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FVector Spacing = FVector(400.f, 400.f, 400.f);

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerGridConstraint Constraint = ECEClonerGridConstraint::None;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Constraint != ECEClonerGridConstraint::None", EditConditionHides))
	bool bInvertConstraint = false;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Sphere", EditConditionHides))
	FCEClonerGridConstraintSphere SphereConstraint;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Cylinder", EditConditionHides))
	FCEClonerGridConstraintCylinder CylinderConstraint;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Constraint == ECEClonerGridConstraint::Texture", EditConditionHides))
	FCEClonerGridConstraintTexture TextureConstraint;
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerLineLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 Count = 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Spacing = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerAxis Axis = ECEClonerAxis::Y;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(ClampMin="0", ClampMax="1", EditCondition="Axis == ECEClonerAxis::Custom", EditConditionHides))
	FVector Direction = FVector::YAxisVector;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FRotator Rotation = FRotator(0.f);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerCircleLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 Count = 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Radius = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float AngleStart = 0.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float AngleRatio = 1.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	bool bOrientMesh = false;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerPlane Plane = ECEClonerPlane::XY;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Plane == ECEClonerPlane::Custom", EditConditionHides))
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01"))
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerCylinderLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 BaseCount = 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 HeightCount = 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Height = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Radius = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float AngleStart = 0.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float AngleRatio = 1.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	bool bOrientMesh = false;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerPlane Plane = ECEClonerPlane::XY;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(EditCondition="Plane == ECEClonerPlane::Custom", EditConditionHides))
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01"))
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerSphereLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 Count = 3 * 3 * 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float Radius = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(ClampMin="0", ClampMax="1"))
	float Ratio = 1.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	bool bOrientMesh = false;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01"))
	FVector Scale = FVector(1.f, 1.f, 1.f);
};

USTRUCT(BlueprintType)
struct CLONEREFFECTOR_API FCEClonerHoneycombLayoutOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	ECEClonerPlane Plane = ECEClonerPlane::XY;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 WidthCount = 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	int32 HeightCount = 3;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float WidthOffset = 0.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float HeightOffset = 0.5f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float WidthSpacing = 400.f;

	UPROPERTY(BlueprintReadWrite, EditInstanceOnly, Category="Cloner")
	float HeightSpacing = 400.f;
};

namespace UE::ClonerEffector
{
	void SetBillboardComponentSprite(const AActor* InActor, const FString& InTexturePath);
	void SetBillboardComponentVisibility(const AActor* InActor, bool bInVisibility);
}