// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassLODTypes.h"
#include "Engine/DataTable.h"
#include "Misc/MTAccessDetector.h"

#include "MassRepresentationTypes.generated.h"

class UMaterialInterface;
class UStaticMesh;

DECLARE_LOG_CATEGORY_EXTERN(LogMassRepresentation, Log, All);

namespace UE::Mass::ProcessorGroupNames
{
	const FName Representation = FName(TEXT("Representation"));
}

UENUM()
enum class EMassRepresentationType : uint8
{
	HighResSpawnedActor,
	LowResSpawnedActor,
	StaticMeshInstance,
	None,
};

enum class EMassActorEnabledType : uint8
{
	Disabled,
	LowRes,
	HighRes,
};

USTRUCT()
struct FStaticMeshInstanceVisualizationMeshDesc
{
	GENERATED_BODY()
	
	/** The static mesh visual representation */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/**
	 * Material overrides for the static mesh visual representation. 
	 * 
	 * Array indices correspond to material slot indices on the static mesh.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	/** The minimum inclusive LOD significance to start using this static mesh */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	float MinLODSignificance = float(EMassLOD::High);

	/** The maximum exclusive LOD significance to stop using this static mesh */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	float MaxLODSignificance = float(EMassLOD::Max);

	/** Controls whether the ISM can cast shadow or not */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	bool bCastShadows = false;
	
	/** Controls the mobility of the ISM */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TEnumAsByte<EComponentMobility::Type> Mobility = EComponentMobility::Movable;

	bool operator==(const FStaticMeshInstanceVisualizationMeshDesc& Other) const
	{
		return Mesh == Other.Mesh && 
			MaterialOverrides == Other.MaterialOverrides && 
			FMath::IsNearlyEqual(MinLODSignificance, Other.MinLODSignificance, KINDA_SMALL_NUMBER) &&
			FMath::IsNearlyEqual(MaxLODSignificance, Other.MaxLODSignificance, KINDA_SMALL_NUMBER) &&
			bCastShadows == Other.bCastShadows && 
			Mobility == Other.Mobility;
	}
	
	friend FORCEINLINE uint32 GetTypeHash(const FStaticMeshInstanceVisualizationMeshDesc& MeshDesc)
	{
		uint32 Hash = 0x0;
		Hash = PointerHash(MeshDesc.Mesh, Hash);
		Hash = HashCombine(GetTypeHash(MeshDesc.bCastShadows), Hash);
		Hash = HashCombine(GetTypeHash(MeshDesc.Mobility), Hash);
		for (UMaterialInterface* MaterialOverride : MeshDesc.MaterialOverrides)
		{
			if (MaterialOverride)
			{
				Hash = PointerHash(MaterialOverride, Hash);
			}
		}
		return Hash;
	}

	operator uint32() const
	{
		return GetTypeHash(*this);
	}
};

USTRUCT()
struct FStaticMeshInstanceVisualizationDesc : public FTableRowBase
{
	GENERATED_BODY()

	/** 
	 * Mesh descriptions. These will be instanced together using the same transform for each, to 
	 * visualize the given agent.
	 */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	TArray<FStaticMeshInstanceVisualizationMeshDesc> Meshes;

	/** Boolean to enable code to transform the static meshes if not align the mass agent transform */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual")
	bool bUseTransformOffset = false;

	/** Transform to offset the static meshes if not align the mass agent transform */
	UPROPERTY(EditAnywhere, Category = "Mass|Visual", meta=(EditCondition="bUseTransformOffset"))
	FTransform TransformOffset;

	bool operator==(const FStaticMeshInstanceVisualizationDesc& Other) const
	{
		return Meshes == Other.Meshes;
	}
};

class UInstancedStaticMeshComponent;

USTRUCT()
struct MASSREPRESENTATION_API FISMCSharedData
{
	GENERATED_BODY()

	FISMCSharedData() = default;

	FISMCSharedData(UInstancedStaticMeshComponent* InISMC)
		: ISMC(InISMC),
		RefCount(1)
	{

	}

	UInstancedStaticMeshComponent* ISMC = nullptr;

	int32 RefCount = 1;

	/** Buffer holding current frame transforms for the static mesh instances, used to batch update the transforms */
	TArray<int32> UpdateInstanceIds;
	TArray<FTransform> StaticMeshInstanceTransforms;
	TArray<FTransform> StaticMeshInstancePrevTransforms;

	/** Buffer holding current frame custom floats for the static mesh instances, used to batch update the ISMs custom data */
	TArray<float> StaticMeshInstanceCustomFloats;

	// When initially adding to StaticMeshInstanceCustomFloats, can use the size as the write iterator, but on subsequent processors, we need to know where to start writing
	int32 WriteIterator = 0;
};

typedef TMap<uint32, FISMCSharedData> FISMCSharedDataMap;

USTRUCT()
struct MASSREPRESENTATION_API FMassLODSignificanceRange
{
	GENERATED_BODY()
public:

	void AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const TArray<uint32>& ExcludeStaticMeshRefs);

	// Adds the specified struct reinterpreted as custom floats to our custom data. Individual members of the specified struct should always fit into a float.
	// When adding any custom data, the custom data must be added for every instance.
	template<typename InCustomDataType>
	void AddBatchedCustomData(InCustomDataType InCustomData, const TArray<uint32>& ExcludeStaticMeshRefs, int32 NumFloatsToPad = 0)
	{
		check(ISMCSharedDataPtr);
		static_assert((sizeof(InCustomDataType) % sizeof(float)) == 0, "AddBatchedCustomData: InCustomDataType should have a total size multiple of sizeof(float), and have members that fit in a float's boundaries");
		const size_t StructSize = sizeof(InCustomDataType);
		const size_t StructSizeInFloats = StructSize / sizeof(float);
		for (int i = 0; i < StaticMeshRefs.Num(); i++)
		{
			if (ExcludeStaticMeshRefs.Contains(StaticMeshRefs[i]))
			{
				continue;
			}

			FISMCSharedData& SharedData = (*ISMCSharedDataPtr)[StaticMeshRefs[i]];
			const int32 StartIndex = SharedData.StaticMeshInstanceCustomFloats.AddDefaulted(StructSizeInFloats + NumFloatsToPad);
			InCustomDataType* CustomData = reinterpret_cast<InCustomDataType*>(&SharedData.StaticMeshInstanceCustomFloats[StartIndex]);
			*CustomData = InCustomData;
		}
	}

	void AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const TArray<uint32>& ExcludeStaticMeshRefs);

	void WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const int32 FloatsPerInstance, const int32 StartIndex, const TArray<uint32>& ExcludeStaticMeshRefs);

	/** LOD Significance range */
	float MinSignificance;
	float MaxSignificance;

	/** The component handling these instances */
	UPROPERTY()
	TArray<uint32> StaticMeshRefs;

	FISMCSharedDataMap* ISMCSharedDataPtr = nullptr;
};

USTRUCT()
struct MASSREPRESENTATION_API FMassInstancedStaticMeshInfo
{
	GENERATED_BODY()
public:

	FMassInstancedStaticMeshInfo() = default;

	FMassInstancedStaticMeshInfo(const FStaticMeshInstanceVisualizationDesc& InDesc)
		: Desc(InDesc)
	{
	}

	bool operator==(const FStaticMeshInstanceVisualizationDesc& OtherDesc) const
	{
		return Desc == OtherDesc;
	}
	
	const FStaticMeshInstanceVisualizationDesc& GetDesc() const
	{
		return Desc;
	}

	/** Whether or not to transform the static meshes if not align the mass agent transform */
	bool ShouldUseTransformOffset() const { return Desc.bUseTransformOffset; }
	FTransform GetTransformOffset() const { return Desc.TransformOffset; }

	FORCEINLINE FMassLODSignificanceRange* GetLODSignificanceRange(float LODSignificance)
	{
		for (FMassLODSignificanceRange& Range : LODSignificanceRanges)
		{
			if (LODSignificance >= Range.MinSignificance && LODSignificance < Range.MaxSignificance)
			{
				return &Range;
			}
		}
		return nullptr;
	}

	FORCEINLINE void AddBatchedTransform(const int32 InstanceId, const FTransform& Transform, const FTransform& PrevTransform, const float LODSignificance, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedTransform(InstanceId, Transform, PrevTransform, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (PrevRange != Range)
				{
					PrevRange->AddBatchedTransform(InstanceId, Transform, PrevTransform, Range->StaticMeshRefs);
				}
			}
		}
	}

	// Adds the specified struct reinterpreted as custom floats to our custom data. Individual members of the specified struct should always fit into a float.
	// When adding any custom data, the custom data must be added for every instance.
	template<typename InCustomDataType>
	void AddBatchedCustomData(InCustomDataType InCustomData, const float LODSignificance, const float PrevLODSignificance = -1.0f, int32 NumFloatsToPad = 0)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedCustomData(InCustomData, TArray<uint32>(), NumFloatsToPad);
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (PrevRange != Range)
				{
					PrevRange->AddBatchedCustomData(InCustomData, Range->StaticMeshRefs, NumFloatsToPad);
				}
			}
		}
	}

	FORCEINLINE void AddBatchedCustomDataFloats(const TArray<float>& CustomFloats, const float LODSignificance, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->AddBatchedCustomDataFloats(CustomFloats, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (PrevRange != Range)
				{
					PrevRange->AddBatchedCustomDataFloats(CustomFloats, Range->StaticMeshRefs);
				}
			}
		}
	}

	FORCEINLINE void WriteCustomDataFloatsAtStartIndex(int32 StaticMeshIndex, const TArrayView<float>& CustomFloats, const float LODSignificance, const int32 FloatsPerInstance, const int32 FloatStartIndex, const float PrevLODSignificance = -1.0f)
	{
		if (FMassLODSignificanceRange* Range = GetLODSignificanceRange(LODSignificance))
		{
			Range->WriteCustomDataFloatsAtStartIndex(StaticMeshIndex, CustomFloats, FloatsPerInstance, FloatStartIndex, TArray<uint32>());
			if(PrevLODSignificance >= 0.0f)
			{
				FMassLODSignificanceRange* PrevRange = GetLODSignificanceRange(PrevLODSignificance);
				if (PrevRange != Range)
				{
					PrevRange->WriteCustomDataFloatsAtStartIndex(StaticMeshIndex, CustomFloats, FloatsPerInstance, FloatStartIndex, Range->StaticMeshRefs);
				}
			}
		}
	}

protected:

	/** Destroy the visual instance */
	void ClearVisualInstance(FISMCSharedDataMap& ISMCSharedData);

	/** Information about this static mesh which will represent all instances */
	UPROPERTY()
		FStaticMeshInstanceVisualizationDesc Desc;

	/** The component handling these instances */
	UPROPERTY()
		TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstancedStaticMeshComponents;

	UPROPERTY()
		TArray<FMassLODSignificanceRange> LODSignificanceRanges;
	friend class UMassVisualizationComponent;
};

#if ENABLE_MT_DETECTOR

#define MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(ArrayView, AccessDetector) FMassInstancedStaticMeshInfoArrayView(ArrayView, AccessDetector)
struct FMassInstancedStaticMeshInfoArrayViewAccessDetector
{
	FMassInstancedStaticMeshInfoArrayViewAccessDetector(TArrayView<FMassInstancedStaticMeshInfo> InInstancedStaticMeshInfos, const FRWRecursiveAccessDetector& InAccessDetector)
		: InstancedStaticMeshInfos(InInstancedStaticMeshInfos)
		, AccessDetector(&InAccessDetector)
	{
		UE_MT_ACQUIRE_WRITE_ACCESS(*AccessDetector);
	}

	FMassInstancedStaticMeshInfoArrayViewAccessDetector(FMassInstancedStaticMeshInfoArrayViewAccessDetector&& Other)
		: InstancedStaticMeshInfos(Other.InstancedStaticMeshInfos)
		, AccessDetector(Other.AccessDetector)
	{
		Other.AccessDetector = nullptr;
	}
	FMassInstancedStaticMeshInfoArrayViewAccessDetector(const FMassInstancedStaticMeshInfoArrayViewAccessDetector& Other) = delete;
	void operator=(const FMassInstancedStaticMeshInfoArrayViewAccessDetector& Other) = delete;

	~FMassInstancedStaticMeshInfoArrayViewAccessDetector()
	{
		if(AccessDetector)
		{
			UE_MT_RELEASE_WRITE_ACCESS(*AccessDetector);
		}
	}

	FORCEINLINE FMassInstancedStaticMeshInfo& operator[](int32 Index) const
	{
		return InstancedStaticMeshInfos[Index];
	}

private:
	TArrayView<FMassInstancedStaticMeshInfo> InstancedStaticMeshInfos;
	const FRWRecursiveAccessDetector* AccessDetector;
};

typedef FMassInstancedStaticMeshInfoArrayViewAccessDetector FMassInstancedStaticMeshInfoArrayView;

#else // ENABLE_MT_DETECTOR

#define MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(ArrayView, AccessDetector) ArrayView

typedef TArrayView<FMassInstancedStaticMeshInfo> FMassInstancedStaticMeshInfoArrayView;

#endif // ENABLE_MT_DETECTOR
