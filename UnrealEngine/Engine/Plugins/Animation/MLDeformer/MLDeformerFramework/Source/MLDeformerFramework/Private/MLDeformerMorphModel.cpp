// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerComponent.h"
#include "MLDeformerMorphModelInputInfo.h"
#include "Components/ExternalMorphSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/MorphTargetVertexInfoBuffers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerMorphModel)

#define LOCTEXT_NAMESPACE "MLDeformerMorphModel"

UMLDeformerMorphModel::UMLDeformerMorphModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MorphTargetSet = MakeShared<FExternalMorphSet>();
	MorphTargetSet->Name = GetClass()->GetFName();
}

void UMLDeformerMorphModel::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerMorphModel::Serialize)

	Super::Serialize(Archive);

	// Check if we have initialized our compressed morph buffers.
	bool bHasMorphData = false;
	if (Archive.IsSaving())
	{
		// Strip editor only data on cook.
		if (Archive.IsCooking())
		{			
			MorphTargetDeltas.Empty();
		}

		bHasMorphData = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.IsMorphCPUDataValid() : false;
	}
	Archive << bHasMorphData;

	// Load or save the compressed morph buffers, if they exist.
	if (bHasMorphData)
	{
		check(MorphTargetSet.IsValid());
		Archive << MorphTargetSet->MorphBuffers;
	}
}

void UMLDeformerMorphModel::PostLoad()
{
	Super::PostLoad();

	// If we have an input info, but it isn't one inherited from the MorphInputInfo, try to create a new one.
	// This is because we introduced a UMLDeformerMorphModelInputInfo later on, and we want to convert old assets to use this new class.
	UMLDeformerInputInfo* CurrentInputInfo = GetInputInfo();
	if (CurrentInputInfo && !CurrentInputInfo->IsA<UMLDeformerMorphModelInputInfo>())
	{
		UMLDeformerMorphModelInputInfo* MorphInputInfo = Cast<UMLDeformerMorphModelInputInfo>(CreateInputInfo());
		MorphInputInfo->CopyMembersFrom(CurrentInputInfo);
		CurrentInputInfo->ConditionalBeginDestroy();
		check(MorphInputInfo); // The input info class should be inherited from the UMLDeformerMorphModelInputInfo class.
		SetInputInfo(MorphInputInfo);
	}

	UpdateStatistics();

#if WITH_EDITOR
	InvalidateMemUsage();
#endif
}

bool UMLDeformerMorphModel::CanDynamicallyUpdateMorphTargets() const
{
	return GetMorphTargetDeltas().Num() == (GetNumBaseMeshVerts() * GetNumMorphTargets());
}

UMLDeformerModelInstance* UMLDeformerMorphModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UMLDeformerMorphModelInstance>(Component);
}

void UMLDeformerMorphModel::SetMorphTargetDeltaFloats(const TArray<float>& Deltas)
{
	FloatArrayToVector3Array(Deltas, MorphTargetDeltas);
}

void UMLDeformerMorphModel::SetMorphTargetDeltas(const TArray<FVector3f>& Deltas)
{
	MorphTargetDeltas = Deltas;
}

int32 UMLDeformerMorphModel::GetMorphTargetDeltaStartIndex(int32 BlendShapeIndex) const
{
	if (MorphTargetDeltas.Num() == 0)
	{
		return INDEX_NONE;
	}

	return GetNumBaseMeshVerts() * BlendShapeIndex;
}

void UMLDeformerMorphModel::BeginDestroy()
{
	if (MorphTargetSet)
	{
		BeginReleaseResource(&MorphTargetSet->MorphBuffers);
		RenderCommandFence.BeginFence();
	}
	Super::BeginDestroy();
}

bool UMLDeformerMorphModel::IsReadyForFinishDestroy()
{
	// Wait for associated render resources to be released.
	return Super::IsReadyForFinishDestroy() && RenderCommandFence.IsFenceComplete();
}

void UMLDeformerMorphModel::SetMorphTargetsErrorOrder(const TArray<int32>& MorphTargetOrder, const TArray<float>& ErrorValues)
{
	MorphTargetErrorOrder = MorphTargetOrder;
	MorphTargetErrors = ErrorValues;
}

void UMLDeformerMorphModel::UpdateStatistics()
{
	NumMorphTargets = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.GetNumMorphs() : 0;
	CompressedMorphDataSizeInBytes = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.GetMorphDataSizeInBytes() : 0;
	UncompressedMorphDataSizeInBytes = GetMorphTargetDeltas().Num() * sizeof(FVector3f);
}

void UMLDeformerMorphModel::SetMorphTargetsMaxWeights(const TArray<float>& MaxWeights)
{
	MaxMorphWeights = MaxWeights;
}

TArrayView<const float> UMLDeformerMorphModel::GetMorphTargetMaxWeights() const
{
	return MaxMorphWeights;
}

TArrayView<const float> UMLDeformerMorphModel::GetMorphTargetErrorValues() const
{
	return MorphTargetErrors;
}

TArrayView<const int32> UMLDeformerMorphModel::GetMorphTargetErrorOrder() const
{ 
	return MorphTargetErrorOrder;
}

TArrayView<const FMLDeformerMorphModelQualityLevel> UMLDeformerMorphModel::GetQualityLevels() const
{
	return QualityLevels;
}

TArray<FMLDeformerMorphModelQualityLevel>& UMLDeformerMorphModel::GetQualityLevelsArray()
{
	return QualityLevels;
}

float UMLDeformerMorphModel::GetMorphTargetError(int32 MorphIndex) const
{ 
	return MorphTargetErrors[MorphIndex];
}

void UMLDeformerMorphModel::SetMorphTargetError(int32 MorphIndex, float Error)
{ 
	MorphTargetErrors[MorphIndex] = Error;
}

int32 UMLDeformerMorphModel::GetNumActiveMorphs(int32 QualityLevel) const
{
	if (QualityLevels.IsEmpty() || MorphTargetErrorOrder.IsEmpty() || MorphTargetErrors.IsEmpty())
	{
		return FMath::Max<int32>(0, NumMorphTargets - 1);	// -1 Because this number includes the means morph target.
	}

	const int32 ClampedQualityLevel = FMath::Clamp<int32>(QualityLevel, 0, QualityLevels.Num() - 1);
	const int32 NumActiveMorphs = FMath::Clamp<int32>(QualityLevels[ClampedQualityLevel].GetMaxActiveMorphs(), 0, NumMorphTargets - 1);	// -1 Because we want to exclude the means morph target.
	return NumActiveMorphs;
}

#if WITH_EDITOR
void UMLDeformerMorphModel::UpdateMemoryUsage()
{
	Super::UpdateMemoryUsage();

	// We strip the deltas when cooking.
	CookedMemUsageInBytes -= MorphTargetDeltas.GetAllocatedSize();

	// Strip the input item mask buffer from the cooked size.
	if (GetInputInfo() && GetInputInfo()->IsA<UMLDeformerMorphModelInputInfo>())
	{
		const UMLDeformerMorphModelInputInfo* MorphInputInfo = Cast<UMLDeformerMorphModelInputInfo>(GetInputInfo());
		check(MorphInputInfo);	// Input Info class is expected to be inherited from the UMLDeformerMorphModelInputInfo class.
		CookedMemUsageInBytes -= MorphInputInfo->GetInputItemMaskBuffer().GetAllocatedSize();
	}

	// Add the compressed morph target data size.
	const uint64 GPUMorphSize = GetCompressedMorphDataSizeInBytes();
	GPUMemUsageInBytes += GPUMorphSize;
	CookedMemUsageInBytes += GPUMorphSize;
}
#endif

#undef LOCTEXT_NAMESPACE
